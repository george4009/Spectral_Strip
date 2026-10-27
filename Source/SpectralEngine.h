#pragma once
#include <juce_dsp/juce_dsp.h>
#include "ShimmerReverb.h"
#include <array>
#include <atomic>
#include <complex>
#include <vector>

/*
    SpectralEngine
    ==============

    The DSP core of Spectral Strip. One short-time Fourier transform (STFT) is shared by
    every module in the strip:

        input -> window -> FFT -> module 1 -> module 2 -> ... -> band filter -> IFFT -> overlap-add

    - FFT size is selectable from 512 to 8192 samples, with a hop of size / 4 (4x overlap)
      and sqrt-Hann windows on both analysis and synthesis.
    - Output frames are scheduled `size` samples after their input, so the base latency
      equals the FFT size. Each Shimmer module adds `size - hop` more (it works in the
      time domain between two extra transforms).
    - Up to `maxModules` effects run in the order given by the chain. Each chain entry
      names an effect type and a stable "slot", the instance whose parameters and memory
      it uses; moving an entry in the chain therefore keeps its settings.
    - process() is called once per sample from the audio thread. It never allocates or
      locks: buffers are sized in prepare() / allocateFor() on the message thread.
*/
class SpectralEngine
{
public:
    //==============================================================================
    // FFT sizes are 2^minOrder ... 2^maxOrder. A "choice" is an index into that range.
    static constexpr int minOrder = 9, maxOrder = 13, choices = maxOrder - minOrder + 1;
    static constexpr int defaultSize = 2048, defaultHop = defaultSize / 4;
    static constexpr int maxSize = 1 << maxOrder, maxBins = maxSize / 2 + 1;
    static constexpr int displayBins = 96;      // analyser resolution (log-spaced, 20 Hz - 20 kHz)
    static constexpr int maxModules = 20;

    // Effect types stored in chain entries.
    struct ModuleType
    {
        static constexpr int none = 0, freeze = 1, gate = 2, delay = 3, blur = 4, tremolo = 5, shimmer = 6;
        static constexpr int count = 6;
    };

    //==============================================================================
    // Chain entries pack an effect type and a slot (instance) number.
    // Entries below 256 keep the original packing: high nibble = type, low nibble = slot + 1.
    // Slots 16-20 do not fit there, so they use (type << 8) | (slot + 1). 0 means "empty".
    static int entryType(int entry) noexcept { return entry >= 256 ? entry >> 8 : entry >> 4; }
    static int entrySlot(int entry) noexcept { return entry >= 256 ? (entry & 255) - 1 : (entry & 15) - 1; }
    static int makeEntry(int type, int slot) noexcept
    {
        return slot < 15 ? (type << 4) | (slot + 1) : (type << 8) | (slot + 1);
    }

    // Parameters of one module instance. Every instance carries the fields of every type;
    // only those of the type occupying the slot are used.
    struct ModuleSettings
    {
        // Freeze
        bool freezeModule = true, freeze = false;
        float smear = 0;
        float shimmer = 0;          // legacy "freeze shimmer": kept for old sessions, not used by the DSP
        // Gate / Tilt
        bool gate = false, invert = false;
        float threshold = -48, gateAttack = 10, gateRelease = 100;
        float gateTilt = 3;         // threshold slope in dB/octave (legacy, still automatable)
        float toneTilt = 0;         // tone tilt in dB/octave around 1 kHz
        // Delay
        bool delay = false;
        float delayMs = 250, spread = .5f, feedback = .3f, damping = 0, delayMix = .5f;
        // Blur
        bool blur = true;
        float blurAmount = .6f, blurWidth = 400, blurMix = 1;
        // Tremolo
        bool tremolo = true;
        float tremoloRate = 4, tremoloDepth = .75f, tremoloShape = 0;
        // Shimmer reverb
        bool reverb = true;
        float reverbDecay = 6, reverbSize = .6f, reverbShimmer = .45f, reverbTone = .55f, reverbMix = .35f;
    };

    // Everything process() needs for one sample. With useInstances == false every module
    // reads the inherited ModuleSettings (the single-instance layout of early versions,
    // still used by the tests); otherwise each slot reads modules[slot].
    struct Settings : ModuleSettings
    {
        std::array<int, maxModules> chain { 0x11, 0x22, 0x33, 0, 0, 0, 0, 0 };
        std::array<ModuleSettings, maxModules> modules {};
        bool useInstances = false, bandFilter = true;
        float low = 20, high = 20000;
    };

    //==============================================================================
    SpectralEngine()
    {
        for (int index = 0; index < choices; ++index)
        {
            const int n = 1 << (minOrder + index);
            at(transforms, index) = std::make_unique<juce::dsp::FFT>(minOrder + index);
            for (int i = 0; i < n; ++i)
                at(at(windows, index), i) = std::sqrt(0.5f - 0.5f * std::cos(twoPi * static_cast<float>(i) / static_cast<float>(n)));
        }
    }

    int getSize() const noexcept   { return size; }
    int getChoice() const noexcept { return activeChoice; }

    // Total latency of a chain: one FFT frame plus the extra frame of each Shimmer module.
    int getLatency(const std::array<int, maxModules>& chain) const noexcept
    {
        int extra = 0;
        for (const int entry : chain)
            if (entryType(entry) == ModuleType::shimmer)
                extra += size - hop;
        return size + extra;
    }

    // Position of a frequency inside the LOW-HIGH band: 0 at LOW, 1 at HIGH, linear in octaves.
    static float bandPosition(float frequency, float lo, float hi) noexcept
    {
        lo = std::max(lo, 1.0f);
        const float span = std::log(std::max(hi, lo) / lo);
        if (span < 1.0e-4f) return 0.5f;
        return juce::jlimit(0.0f, 1.0f, std::log(std::max(frequency, lo) / lo) / span);
    }

    // Analyser levels in dB for the editor (left channel, after the modules). Written by the
    // audio thread once per hop, read by the UI timer.
    std::array<std::atomic<float>, displayBins> spectrum {};

    //==============================================================================
    // Message thread. Releases per-slot memory and precomputes per-bin tables for every FFT size.
    void prepare(double sampleRate, int choice = 2)
    {
        sr = sampleRate;
        // Delay history is sized for 2 s at the smallest hop. Slots allocate it only when a
        // Delay occupies them (see allocateFor).
        historyCapacity = 0;
        for (int index = 0; index < choices; ++index)
        {
            const int n = 1 << (minOrder + index);
            historyCapacity = std::max(historyCapacity, static_cast<size_t>((static_cast<int>(std::ceil(2.0 * sr / (n / 4))) + 3) * (n / 2 + 1)));
        }
        for (auto& channel : channels)
            for (auto& module : channel.modules)
            {
                std::vector<std::complex<float>>().swap(module.history);
                module.reverb.release();
            }
        for (int index = 0; index < choices; ++index)
        {
            const int n = 1 << (minOrder + index), quarter = n / 4;
            auto& table = at(tables, index);
            for (int k = 0; k <= n / 2; ++k)
            {
                const float frequency = static_cast<float>(k * sr / n);
                at(table.frequency, k) = frequency;
                at(table.expected, k) = twoPi * static_cast<float>(k * quarter) / static_cast<float>(n);
                at(table.octaves, k) = std::log2(std::max(frequency, 20.0f) / 1000.0f);
                at(table.darkening, k) = juce::jlimit(0.0f, 1.0f, std::log2(std::max(frequency, 200.0f) / 200.0f) / std::log2(100.0f));
                at(table.display, k) = frequency >= 20 ? juce::jlimit(0, displayBins - 1, static_cast<int>(std::log(frequency / 20.0f) / std::log(1000.0f) * (displayBins - 1))) : -1;
            }
        }
        selectSize(choice);
    }

    // Message thread, before a chain becomes active. Allocates Delay history and Shimmer
    // delay lines for the slots that need them. Slots that are already prepared are left
    // untouched, because the audio thread may still be reading them.
    void allocateFor(const std::array<int, maxModules>& chain)
    {
        if (historyCapacity == 0) return;
        for (int ch = 0; ch < 2; ++ch)
            for (const int entry : chain)
            {
                const int slot = entrySlot(entry), type = entryType(entry);
                if (slot < 0 || slot >= maxModules) continue;
                auto& module = at(at(channels, ch).modules, slot);
                if (type == ModuleType::delay && module.history.size() < historyCapacity)
                    module.history.resize(historyCapacity);
                if (type == ModuleType::shimmer && !module.reverb.ready())
                    module.reverb.prepare(sr, ch);
            }
    }

    // Audio thread, at a muted block boundary. Switches FFT size and clears all running state.
    // Storage and FFT plans already exist; Delay history is invalidated logically (validFrames)
    // instead of clearing several megabytes inside the callback.
    void selectSize(int choice) noexcept
    {
        activeChoice = juce::jlimit(0, choices - 1, choice);
        size = 1 << (minOrder + activeChoice);
        hop = size / 4;
        bins = size / 2 + 1;
        historyFrames = static_cast<int>(std::ceil(2.0 * sr / hop)) + 3;
        for (auto& c : channels)
        {
            c.input.fill(0); c.output.fill(0); c.fftData.fill(0); c.dryHistory.fill(0);
            c.filterGain.fill(1);
            for (int i = 0; i < maxModules; ++i)
                at(c.modules, i).reset(i);
        }
        for (auto& module : channels[1].modules) module.noise.state ^= 0x7f4a7c15u;
        position = dryPosition = hopCount = frameIndex = validFrames = samplesSinceReset = 0;
        for (auto& v : spectrum) v.store(-90.0f, std::memory_order_relaxed);
    }

    //==============================================================================
    // Audio thread, once per sample. `in`, `wet` and `dry` hold `count` (1 or 2) channels.
    // `dry` is the input delayed by the chain latency, for latency-compensated mixing.
    void process(const float* in, float* wet, float* dry, int count, const Settings& s)
    {
        const int latency = getLatency(s.chain);
        const auto dryRead = static_cast<size_t>((dryPosition - latency + maxDrySize) % maxDrySize);
        for (int ch = 0; ch < count; ++ch)
        {
            auto& c = at(channels, ch);
            dry[ch] = c.dryHistory[dryRead];
            at(c.dryHistory, dryPosition) = in[ch];
            at(c.input, position) = in[ch];
            wet[ch] = at(c.output, position);
            at(c.output, position) = 0;
        }
        samplesSinceReset = std::min(maxDrySize, samplesSinceReset + 1);
        dryPosition = (dryPosition + 1) % maxDrySize;
        position = (position + 1) % size;
        if (++hopCount == hop)
        {
            hopCount = 0;
            prepareFrame(s);
            for (int ch = 0; ch < count; ++ch)
                transform(at(channels, ch), s, ch == 0);
            frameIndex = (frameIndex + 1) % historyFrames;
            validFrames = std::min(historyFrames, validFrames + 1);
        }
    }

private:
    static constexpr float twoPi = juce::MathConstants<float>::twoPi;

    // std::array access with an int index, to keep the DSP code free of size_t casts.
    template <typename T, size_t N> static T& at(std::array<T, N>& a, int i) noexcept { return a[static_cast<size_t>(i)]; }
    template <typename T, size_t N> static const T& at(const std::array<T, N>& a, int i) noexcept { return a[static_cast<size_t>(i)]; }

    // xorshift32: deterministic, allocation-free noise in [-1, 1].
    struct Random
    {
        uint32_t state = 1;
        float next() noexcept
        {
            state ^= state << 13; state ^= state >> 17; state ^= state << 5;
            return static_cast<float>(state) * (2.0f / 4294967295.0f) - 1.0f;
        }
    };

    using Bins = std::array<float, maxBins>;

    // Running state of one module instance (one per slot and channel).
    struct ModuleState
    {
        // Freeze / Smear
        Bins previousPhase {}, previousMagnitude {}, smoothedMagnitude {}, smearEnvelope {};
        Bins smearPhase {}, smearStep {}, smearDiffusion {};
        Bins heldPhase {}, heldStep {}, heldMagnitude {};
        float smearAmount = 0, freezeBlend = 0;
        double previousFramePower = 0;
        bool smearTonal = false, frozen = false;
        // Gate / Tilt
        Bins gateGain {};
        float gateBlend = 0, toneTilt = 0;
        // Delay: a ring of `historyFrames` spectra, allocated only for Delay slots.
        std::vector<std::complex<float>> history;
        float delayBlend = 0;
        // Blur
        Bins blurPhase {};
        bool blurInitialised = false;
        float blurBlend = 0, blurAmount = 0, blurWidth = 400;
        // Tremolo
        double tremoloPhase = 0;
        float tremoloDepth = 0, tremoloRate = 4, tremoloShape = 0;
        // Shimmer: the reverb runs in the time domain, between an extra IFFT and FFT.
        ShimmerReverb reverb;
        std::array<float, maxSize> bridgeInput {}, bridgeOutput {}, bridgeDry {};
        int bridgePosition = 0;

        Random noise, smearNoise;

        void reset(int slot) noexcept
        {
            previousPhase.fill(0); heldPhase.fill(0); heldStep.fill(0);
            smearPhase.fill(0); smearStep.fill(0); previousMagnitude.fill(0);
            heldMagnitude.fill(0); smoothedMagnitude.fill(0); smearEnvelope.fill(0); smearDiffusion.fill(0); gateGain.fill(0);
            toneTilt = smearAmount = 0; previousFramePower = 0; smearTonal = false;
            reverb.reset(); bridgeInput.fill(0); bridgeOutput.fill(0); bridgeDry.fill(0); bridgePosition = 0;
            blurPhase.fill(0); blurInitialised = false;
            blurBlend = blurAmount = 0; blurWidth = 400;
            tremoloPhase = 0; tremoloDepth = 0; tremoloRate = 4; tremoloShape = 0;
            frozen = false; freezeBlend = gateBlend = delayBlend = 0;
            noise.state = (0x9e3779b9u + static_cast<uint32_t>(slot) * 0x85ebca6bu) | 1u;
            smearNoise = noise;
        }
    };

    // Long enough to compensate the dry signal for the largest possible chain latency.
    static constexpr int maxDrySize = maxSize * (maxModules + 1);

    struct Channel
    {
        std::array<float, maxSize> input {}, output {};
        std::array<float, maxDrySize> dryHistory {};
        // Scratch for transforms: JUCE's real-only FFT works in place on 2 * size floats.
        std::array<float, maxSize * 2> fftData {}, timeData {}, bridgeFFT {};
        Bins filterGain {};
        std::array<std::complex<float>, maxBins> working {}, original {};
        Bins blurPower {}, blurTemporary {}, frameMagnitude {};
        std::array<double, maxBins + 1> blurPrefix {};
        std::array<ModuleState, maxModules> modules;
    };

    // Per-bin values that depend only on FFT size and sample rate.
    struct BinTable
    {
        Bins frequency {};
        Bins expected {};       // phase advance of the bin's centre frequency over one hop
        Bins octaves {};        // log2(frequency / 1 kHz), at least 20 Hz
        Bins darkening {};      // 0 below 200 Hz, 1 at 20 kHz: Delay damping weight
        std::array<int, maxBins> display {};   // analyser column, or -1 below 20 Hz
    };

    // Values for one hop that every module and channel shares.
    struct FrameControls
    {
        float transition = 0;           // one-pole step for ~35 ms parameter smoothing
        float smearTracking = 0;        // ~50 ms phase-advance tracking for Smear
        float smearJitterScale = 0;
        std::array<float, maxModules> smoothing {}, attack {}, release {};
        std::array<bool, maxModules> hold {};
    };

    std::array<Channel, 2> channels;
    std::array<std::array<float, maxSize>, choices> windows {};
    std::array<std::unique_ptr<juce::dsp::FFT>, choices> transforms;
    std::array<BinTable, choices> tables {};
    // Shared by both channels and every module during one hop.
    Bins bandMask {}, filterTarget {}, dispersion {};
    int size = defaultSize, hop = defaultHop, bins = defaultSize / 2 + 1, activeChoice = 2;
    int validFrames = 0, samplesSinceReset = 0;
    double sr = 44100;
    int position = 0, dryPosition = 0, hopCount = 0, frameIndex = 0, historyFrames = 0;
    size_t historyCapacity = 0;

    //==============================================================================
    // Bit-identical to std::remainder(x, 2*pi), which is much slower on the audio thread.
    // Near zero the float subtraction is exact. Below 2^20 the quotient cannot round
    // across a half-integer, and n * 2*pi and x - n * 2*pi are exact in double.
    static float wrapped(float x)
    {
        constexpr float half = twoPi * 0.5f;
        if (std::abs(x) <= half) return x;
        if (x > half && x < 3 * half) return x - twoPi;
        if (x < -half && x > -3 * half) { const float r = x + twoPi; return juce::exactlyEqual(r, 0.0f) ? -0.0f : r; }
        if (!(std::abs(x) < 1048576.0f)) return std::remainder(x, twoPi);
        const double period = twoPi;
        const auto result = static_cast<float>(static_cast<double>(x) - std::nearbyint(static_cast<double>(x) / period) * period);
        return juce::exactlyEqual(result, 0.0f) ? std::copysign(0.0f, x) : result;
    }

    // Copies the complex spectrum into JUCE's interleaved layout for an inverse real FFT.
    void spectrumToFFTData(const Channel& c, std::array<float, maxSize * 2>& data) const noexcept
    {
        std::fill_n(data.begin(), 2 * size, 0.0f);
        for (int k = 0; k < bins; ++k)
        {
            at(data, 2 * k) = at(c.working, k).real();
            at(data, 2 * k + 1) = (k == 0 || k == bins - 1) ? 0.0f : at(c.working, k).imag();
        }
    }
    static std::complex<float> binOf(const std::array<float, maxSize * 2>& data, int k) noexcept
    {
        return { at(data, 2 * k), at(data, 2 * k + 1) };
    }

    //==============================================================================
    // Band mask (where the effects act), band filter target and Delay dispersion, per bin.
    void prepareFrame(const Settings& s)
    {
        const auto& table = at(tables, activeChoice);
        const float lo = std::min(s.low, s.high), hi = std::max(s.low, s.high);
        // Soft edges, at least 30 Hz or one bin wide.
        const float feather = std::max(static_cast<float>(sr / size), 30.0f);
        const bool closed = s.high <= 20.0f || s.low >= 20000.0f || s.low >= s.high;
        bool hasDelay = false;
        for (const int entry : s.chain)
            hasDelay = hasDelay || entryType(entry) == ModuleType::delay;
        for (int k = 0; k < bins; ++k)
        {
            const float frequency = at(table.frequency, k);
            at(bandMask, k) = (lo <= 20 ? 1.0f : juce::jlimit(0.0f, 1.0f, (frequency - lo) / feather))
                            * (hi >= 20000 ? 1.0f : juce::jlimit(0.0f, 1.0f, (hi - frequency) / feather));
            // The endpoints are fully open, so the default band remains transparent.
            const float highPass = s.low <= 20.0f ? 1.0f : juce::jlimit(0.0f, 1.0f, (frequency - s.low) / feather);
            const float lowPass = s.high >= 20000.0f ? 1.0f : juce::jlimit(0.0f, 1.0f, (s.high - frequency) / feather);
            // Crossing the controls closes the band; it never reopens it.
            at(filterTarget, k) = s.bandFilter ? (closed ? 0.0f : highPass * lowPass) : 1.0f;
            if (hasDelay) at(dispersion, k) = bandPosition(frequency, lo, hi);
        }
    }

    const ModuleSettings& settingsFor(const Settings& s, int slot) const noexcept
    {
        return s.useInstances ? at(s.modules, slot) : static_cast<const ModuleSettings&>(s);
    }

    // Smoothed per-module controls for this hop. Every slot of the channel is updated,
    // including slots that are not in the chain, so their state is ready when inserted.
    FrameControls updateControls(Channel& c, const Settings& s)
    {
        FrameControls f;
        f.transition = 1.0f - std::exp(-static_cast<float>(hop / (sr * .035)));
        f.smearTracking = 1 - std::exp(-static_cast<float>(hop / (sr * .05)));
        f.smearJitterScale = .7f * std::sqrt(static_cast<float>(hop / (sr * .01)));
        const auto follow = [this](float ms) { return 1.0f - std::exp(-static_cast<float>(hop / (sr * std::max(.1f, ms) * .001))); };

        // A Freeze may capture only once its input holds a full frame of real signal,
        // which is later for modules placed after Shimmer reverbs.
        std::array<int, maxModules> readyAfter {};
        readyAfter.fill(size);
        int precedingLatency = 0;
        for (const int entry : s.chain)
        {
            const int slot = entrySlot(entry);
            if (slot >= 0 && slot < maxModules) at(readyAfter, slot) = size + precedingLatency;
            if (entryType(entry) == ModuleType::shimmer) precedingLatency += size - hop;
        }

        const float transition = f.transition;
        for (int i = 0; i < maxModules; ++i)
        {
            auto& module = at(c.modules, i);
            const auto& params = settingsFor(s, i);
            const bool hold = params.freezeModule && params.freeze && samplesSinceReset >= at(readyAfter, i);
            at(f.hold, i) = hold;
            module.freezeBlend += transition * ((hold ? 1.0f : 0.0f) - module.freezeBlend);
            module.gateBlend += transition * ((params.gate ? 1.0f : 0.0f) - module.gateBlend);
            module.toneTilt += transition * (juce::jlimit(-6.0f, 6.0f, params.toneTilt) - module.toneTilt);
            module.delayBlend += transition * ((params.delay ? params.delayMix : 0.0f) - module.delayBlend);
            module.smearAmount += transition * (juce::jlimit(0.0f, 1.0f, params.smear) - module.smearAmount);
            // Smear time grows with the square of the control, up to 1.5 s per stage.
            const float seconds = params.freezeModule ? module.smearAmount * module.smearAmount * 1.5f : 0;
            at(f.smoothing, i) = seconds > .0001f ? std::exp(-static_cast<float>(hop / sr) / seconds) : 0;
            at(f.attack, i) = follow(params.gateAttack);
            at(f.release, i) = follow(params.gateRelease);
        }
        return f;
    }

    //==============================================================================
    // FREEZE / SMEAR. Smear averages magnitudes over time with two cascaded one-pole
    // envelopes and resynthesises with continuous phase; Freeze holds a snapshot whose
    // phases keep advancing at the measured rate, so the held sound keeps sustaining.
    void applyFreeze(Channel& c, ModuleState& module, int slot, const FrameControls& f, bool maskEachModule)
    {
        const auto& table = at(tables, activeChoice);
        // Classify the frame: tonal frames keep coherent phase, noisy ones get diffusion.
        double power = 0;
        float peak = 0;
        for (int k = 0; k < bins; ++k)
        {
            const float m = std::abs(at(c.working, k));
            at(c.frameMagnitude, k) = m;
            power += m * m;
            peak = std::max(peak, m);
        }
        // Do not reclassify the decaying end of a note as broadband noise.
        if (power > 1e-16 && power >= module.previousFramePower * .98)
            module.smearTonal = peak * peak > power * .15;
        module.previousFramePower = power;
        const bool tonalFrame = module.smearTonal;

        const float memory = at(f.smoothing, slot);
        const bool hold = at(f.hold, slot);
        for (int k = 0; k < bins; ++k)
        {
            const float expected = at(table.expected, k);
            auto& value = at(c.working, k);
            const auto before = value;
            const float magnitude = at(c.frameMagnitude, k), inputPhase = std::arg(value);
            auto& smoothed = at(module.smoothedMagnitude, k);
            const float previousSmoothed = smoothed;
            auto& envelope = at(module.smearEnvelope, k);
            envelope = memory * envelope + (1 - memory) * magnitude;
            // Two cascaded envelopes spread an attack over time instead of
            // immediately injecting it into an already ringing spectrum.
            smoothed = memory * smoothed + (1 - memory) * envelope;
            // Follow the frequency only while the analysis window is stable. The synthesis
            // phase is not reset on each new hit: that would re-align the stored energy into
            // a fresh transient and create metallic pulses.
            auto& previousMagnitude = at(module.previousMagnitude, k);
            auto& step = at(module.smearStep, k);
            const bool reliable = magnitude > std::max(1e-8f, envelope * .05f);
            if (reliable && previousMagnitude > 1e-8f
                && magnitude >= previousMagnitude * .98f
                && magnitude <= previousMagnitude * 1.02f)
            {
                const float measured = expected + wrapped(inputPhase - at(module.previousPhase, k) - expected);
                step += f.smearTracking * wrapped(measured - step);
            }
            else if (previousMagnitude <= 1e-8f && previousSmoothed <= 1e-8f)
                step = expected;
            auto& diffusion = at(module.smearDiffusion, k);
            if (tonalFrame) diffusion = 0;
            else if (reliable)
            {
                float localSum = 0, localPeak = 0;
                for (int n = std::max(0, k - 2); n <= std::min(bins - 1, k + 2); ++n)
                {
                    const float m = at(c.frameMagnitude, n);
                    localSum += m;
                    localPeak = std::max(localPeak, m);
                }
                // Broad, noisy spectra need diffusion to avoid an FFT-periodic ringing tail.
                // Concentrated tonal peaks keep their coherent phase.
                const float diffuse = localPeak > 1e-8f ? juce::jlimit(0.0f, 1.0f, localSum / localPeak - 3.0f) : 0.0f;
                diffusion += f.transition * (diffuse - diffusion);
            }
            const float jitter = module.smearNoise.next() * diffusion * module.smearAmount * f.smearJitterScale;
            auto& smearPhase = at(module.smearPhase, k);
            if (memory <= 0 || previousSmoothed <= 1e-8f)
                smearPhase = inputPhase;
            else
                smearPhase = wrapped(smearPhase + step + jitter);
            previousMagnitude = magnitude;
            // Capture on the first frozen frame, then keep advancing the held phases.
            if (hold && !module.frozen)
            {
                at(module.heldMagnitude, k) = smoothed;
                at(module.heldPhase, k) = memory > 0 ? smearPhase : inputPhase;
                at(module.heldStep, k) = memory > 0 ? step : expected + wrapped(inputPhase - at(module.previousPhase, k) - expected);
            }
            else
            {
                at(module.heldPhase, k) = wrapped(at(module.heldPhase, k) + at(module.heldStep, k));
            }
            value = std::polar(smoothed, memory > 0 ? smearPhase : inputPhase);
            if (!juce::exactlyEqual(module.freezeBlend, 0.0f))
                value += module.freezeBlend * (std::polar(at(module.heldMagnitude, k), at(module.heldPhase, k)) - value);
            at(module.previousPhase, k) = inputPhase;
            if (maskEachModule) value = before + at(bandMask, k) * (value - before);
        }
    }

    // GATE / TILT. A soft-knee (12 dB) gate per bin with its own attack and release, and an
    // independent tone tilt around 1 kHz (limited to +/-18 dB at the band ends).
    void applyGate(Channel& c, ModuleState& module, const ModuleSettings& params, int slot, const FrameControls& f, bool maskEachModule)
    {
        const auto& table = at(tables, activeChoice);
        for (int k = 0; k < bins; ++k)
        {
            auto& value = at(c.working, k);
            const auto before = value;
            const float db = juce::Decibels::gainToDecibels(std::abs(value) * 2.0f / static_cast<float>(size), -120.0f);
            // The legacy slope lowers the threshold above 1 kHz and raises it below, per octave.
            const float threshold = params.threshold - params.gateTilt * at(table.octaves, k);
            float gain = juce::jlimit(0.0f, 1.0f, (db - threshold + 6.0f) / 12.0f);
            gain = gain * gain * (3 - 2 * gain);
            if (params.invert) gain = 1 - gain;
            auto& gateGain = at(module.gateGain, k);
            gateGain += (gain > gateGain ? at(f.attack, slot) : at(f.release, slot)) * (gain - gateGain);
            value *= 1 + module.gateBlend * (gateGain - 1);
            if (std::abs(module.toneTilt) > 1e-6f)
                value *= juce::Decibels::decibelsToGain(module.toneTilt * juce::jlimit(-3.0f, 3.0f, at(table.octaves, k)));
            if (maskEachModule) value = before + at(bandMask, k) * (value - before);
        }
    }

    // DELAY. Each bin is delayed by whole and fractional hops read from a ring of past
    // spectra; Dispersion spreads the times across the band, Damping darkens each repeat.
    void applyDelay(Channel& c, ModuleState& module, const ModuleSettings& params, bool maskEachModule)
    {
        jassert(module.history.size() >= historyCapacity);
        const auto& table = at(tables, activeChoice);
        const auto frame = [&](int age, int k)
        { return module.history[static_cast<size_t>(((frameIndex - age + historyFrames) % historyFrames) * bins + k)]; };
        for (int k = 0; k < bins; ++k)
        {
            const float expected = at(table.expected, k);
            auto& value = at(c.working, k);
            const auto before = value;
            const float proportion = at(dispersion, k);
            const float seconds = params.delayMs * 0.001f * (1 + params.spread * (2 * proportion - 1));
            const float frames = juce::jlimit(1.0f, static_cast<float>(historyFrames - 2), seconds * static_cast<float>(sr / hop));
            const int d = static_cast<int>(frames);
            const float fraction = frames - static_cast<float>(d);
            const auto a = validFrames >= d ? frame(d, k) : std::complex<float> {};
            const auto b = validFrames >= d + 1 ? frame(d + 1, k) : std::complex<float> {};
            // Interpolate magnitude and unwrapped phase separately: mixing complex values
            // would cancel partials whose phase advances by about half a turn per hop.
            std::complex<float> delayed = a + fraction * (b - a);
            const float magA = std::abs(a), magB = std::abs(b);
            if (magA > 0 && magB > 0)
            {
                const float advance = expected + wrapped(std::arg(a) - std::arg(b) - expected);
                delayed = std::polar(magA + fraction * (magB - magA), std::arg(a) - fraction * advance);
            }
            // Damping: each repeat loses up to 90% at 20 kHz, nothing below 200 Hz.
            const float damp = 1.0f - 0.9f * params.damping * at(table.darkening, k);
            // Bound each feedback bin; there is no nonlinear processing in the neutral path.
            auto stored = value + juce::jlimit(0.0f, 0.92f, params.feedback) * damp * delayed;
            const float storedMag = std::abs(stored);
            if (storedMag > static_cast<float>(size) * 4.0f) stored *= static_cast<float>(size) * 4.0f / storedMag;
            module.history[static_cast<size_t>(frameIndex * bins + k)] = params.delay ? stored : std::complex<float> {};
            value += module.delayBlend * (delayed - value);
            if (maskEachModule) value = before + at(bandMask, k) * (value - before);
        }
    }

    // BLUR. Spreads power across neighbouring bins with three box filters (a close
    // approximation of a Gaussian, O(bins) per pass), keeping the band's total power.
    void applyBlur(Channel& c, ModuleState& module, const ModuleSettings& params, float lo, float hi, float transition)
    {
        module.blurBlend += transition * ((params.blur ? params.blurMix : 0.0f) - module.blurBlend);
        module.blurAmount += transition * (params.blurAmount - module.blurAmount);
        module.blurWidth += transition * (params.blurWidth - module.blurWidth);
        if (module.blurBlend < 1e-7f || module.blurAmount < 1e-7f) return;
        // DC and Nyquist stay real and unmodified. The selected band is a closed domain.
        const int first = juce::jlimit(1, bins - 2, static_cast<int>(std::ceil(lo * static_cast<float>(size) / sr)));
        const int last = juce::jlimit(1, bins - 2, static_cast<int>(std::floor(hi * static_cast<float>(size) / sr)));
        if (last < first) return;
        if (!module.blurInitialised)
        {
            for (int k = 0; k < bins; ++k) at(module.blurPhase, k) = module.noise.next() * juce::MathConstants<float>::pi;
            module.blurInitialised = true;
        }
        double inputPower = 0;
        float peak = 0;
        for (int k = first; k <= last; ++k)
        {
            const float power = std::norm(at(c.working, k));
            at(c.blurPower, k) = power;
            inputPower += power;
            peak = std::max(peak, power);
        }
        const float radius = juce::jlimit(0.0f, static_cast<float>(last - first), module.blurWidth * static_cast<float>(size / sr) / 2.0f);
        const int whole = static_cast<int>(radius);
        const float fractional = radius - static_cast<float>(whole);
        for (int pass = 0; pass < 3; ++pass)
        {
            at(c.blurPrefix, first) = 0;
            for (int k = first; k <= last; ++k) at(c.blurPrefix, k + 1) = at(c.blurPrefix, k) + at(c.blurPower, k);
            for (int k = first; k <= last; ++k)
            {
                const auto average = [&](int r)
                {
                    const int a = std::max(first, k - r), b = std::min(last, k + r);
                    return (at(c.blurPrefix, b + 1) - at(c.blurPrefix, a)) / (b - a + 1);
                };
                at(c.blurTemporary, k) = static_cast<float>((1 - fractional) * average(whole) + fractional * average(whole + 1));
            }
            for (int k = first; k <= last; ++k) at(c.blurPower, k) = at(c.blurTemporary, k);
        }
        double spreadPower = 0;
        for (int k = first; k <= last; ++k) spreadPower += at(c.blurPower, k);
        const float normalise = spreadPower > 1e-20 ? static_cast<float>(inputPower / spreadPower) : 1;
        const auto& expected = at(tables, activeChoice).expected;
        for (int k = first; k <= last; ++k)
        {
            auto& value = at(c.working, k);
            const float power = std::norm(value);
            auto& phase = at(module.blurPhase, k);
            phase = wrapped(phase + at(expected, k));
            // Anchor occupied partials; newly filled bins run with continuous phase.
            if (power > peak * .25f && power > 1e-20f) phase = std::arg(value);
            const float magnitude = std::sqrt(std::max(0.0f, at(c.blurPower, k) * normalise));
            // Amount blends once with the fully diffused spectrum.
            value += module.blurBlend * module.blurAmount * at(bandMask, k) * (std::polar(magnitude, phase) - value);
        }
    }

    // TREMOLO. The frame is taken back to the time domain and modulated sample by sample,
    // so fast LFO rates survive even at the largest FFT size.
    void applyTremolo(Channel& c, ModuleState& module, const ModuleSettings& params, float transition)
    {
        module.tremoloDepth += transition * ((params.tremolo ? params.tremoloDepth : 0.0f) - module.tremoloDepth);
        module.tremoloRate += transition * (params.tremoloRate - module.tremoloRate);
        module.tremoloShape += transition * (params.tremoloShape - module.tremoloShape);
        const double phase = module.tremoloPhase;
        const double step = static_cast<double>(module.tremoloRate) / sr;
        module.tremoloPhase = std::fmod(phase + hop * step, 1.0);
        if (module.tremoloDepth < 1e-7f) return;
        auto& fft = *at(transforms, activeChoice);
        spectrumToFFTData(c, c.timeData);
        fft.performRealOnlyInverseTransform(c.timeData.data());
        // Shape morphs the edge duration and, above halfway, the duty cycle:
        // 0 is exactly a raised-cosine (sine) LFO, 0.5 a rounded square, 1 a 12.5% pulse.
        const float shape = juce::jlimit(0.0f, 1.0f, module.tremoloShape);
        const float narrow = juce::jlimit(0.0f, 1.0f, 2 * shape - 1);
        const float duty = .5f - .375f * narrow * narrow * (3 - 2 * narrow);
        // At least 2 ms per edge at high rates, avoiding hard discontinuities.
        const float edge = std::max(.5f * std::pow(.04f, std::min(2 * shape, 1.0f)), module.tremoloRate * .002f);
        for (int i = 0; i < size; ++i)
        {
            const double cycle = phase + i * step;
            const float distance = static_cast<float>(std::abs(cycle - std::floor(cycle + .5)));
            const float ramp = juce::jlimit(0.0f, 1.0f, (distance - .5f * (duty - edge)) / edge);
            const float envelope = .5f + .5f * std::cos(juce::MathConstants<float>::pi * ramp);
            const float gain = 1 - module.tremoloDepth * (1 - envelope);
            at(c.timeData, i) *= gain;
        }
        fft.performRealOnlyForwardTransform(c.timeData.data(), true);
        for (int k = 0; k < bins; ++k)
        {
            auto& value = at(c.working, k);
            value += at(bandMask, k) * (binOf(c.timeData, k) - value);
        }
    }

    // SHIMMER. Overlap-adds the frame into a time-domain bridge, runs the reverb over the
    // finished hop only, and re-analyses its output one frame later (hence the extra latency).
    void applyShimmer(Channel& c, ModuleState& module, const ModuleSettings& params)
    {
        jassert(module.reverb.ready());
        auto& fft = *at(transforms, activeChoice);
        const auto& window = at(windows, activeChoice);
        const int wrap = size - 1;
        spectrumToFFTData(c, c.timeData);
        fft.performRealOnlyInverseTransform(c.timeData.data());
        for (int i = 0; i < size; ++i)
            at(module.bridgeInput, (position + i) & wrap) += at(c.timeData, i) * at(window, i) * .5f;
        module.reverb.set(params.reverbDecay, params.reverbSize, params.reverbShimmer, params.reverbTone, params.reverb ? params.reverbMix : 0.0f);
        if (validFrames == 0) module.reverb.reset();
        // Consume only the finalised hop: never process overlapping samples twice.
        for (int i = 0; i < hop; ++i)
        {
            auto& slot = at(module.bridgeInput, (position + i) & wrap);
            const float input = slot;
            slot = 0;
            at(module.bridgeDry, module.bridgePosition) = input;
            at(module.bridgeOutput, module.bridgePosition) = module.reverb.process(input);
            module.bridgePosition = (module.bridgePosition + 1) & wrap;
        }
        std::fill_n(c.timeData.begin() + size, size, 0.0f);
        std::fill_n(c.bridgeFFT.begin() + size, size, 0.0f);
        for (int i = 0; i < size; ++i)
        {
            const int index = (module.bridgePosition + i) & wrap;
            at(c.timeData, i) = at(module.bridgeOutput, index) * at(window, i);
            at(c.bridgeFFT, i) = at(module.bridgeDry, index) * at(window, i);
        }
        fft.performRealOnlyForwardTransform(c.timeData.data(), true);
        fft.performRealOnlyForwardTransform(c.bridgeFFT.data(), true);
        for (int k = 0; k < bins; ++k)
        {
            const auto dry = binOf(c.bridgeFFT, k), wet = binOf(c.timeData, k);
            at(c.working, k) = dry + at(bandMask, k) * (wet - dry);
        }
    }

    //==============================================================================
    // One hop for one channel: analyse, run the chain, filter, resynthesise.
    void transform(Channel& c, const Settings& s, bool display)
    {
        auto& fft = *at(transforms, activeChoice);
        const auto& window = at(windows, activeChoice);
        const auto& table = at(tables, activeChoice);
        const int wrap = size - 1;
        for (int i = 0; i < size; ++i)
            at(c.fftData, i) = at(c.input, (position + i) & wrap) * at(window, i);
        fft.performRealOnlyForwardTransform(c.fftData.data(), true);

        const auto f = updateControls(c, s);
        const float lo = std::min(s.low, s.high), hi = std::max(s.low, s.high);
        // Sessions from before the modular strip (Freeze, Gate, Delay in this order) applied
        // the band mask once, after all three effects. Keep that for identical recall.
        const bool legacyOrder = s.chain == std::array<int, maxModules> { 0x11, 0x22, 0x33, 0, 0, 0, 0, 0 };
        for (int k = 0; k < bins; ++k)
            at(c.working, k) = at(c.original, k) = binOf(c.fftData, k);

        for (const int entry : s.chain)
        {
            if (!entry) continue;
            const int slot = entrySlot(entry), type = entryType(entry);
            if (slot < 0 || slot >= maxModules || type < 1 || type > ModuleType::count) continue;
            auto& module = at(c.modules, slot);
            const auto& params = settingsFor(s, slot);
            switch (type)
            {
                case ModuleType::freeze:  applyFreeze(c, module, slot, f, !legacyOrder); break;
                case ModuleType::gate:    applyGate(c, module, params, slot, f, !legacyOrder); break;
                case ModuleType::delay:   applyDelay(c, module, params, !legacyOrder); break;
                case ModuleType::blur:    applyBlur(c, module, params, lo, hi, f.transition); break;
                case ModuleType::tremolo: applyTremolo(c, module, params, f.transition); break;
                case ModuleType::shimmer: applyShimmer(c, module, params); break;
                default: break;
            }
        }

        // Band filter (smoothed per bin), spectrum display and resynthesis.
        std::array<float, displayBins> levels;
        levels.fill(-90.0f);
        for (int k = 0; k < bins; ++k)
        {
            auto value = at(c.working, k);
            if (legacyOrder) value = at(c.original, k) + at(bandMask, k) * (value - at(c.original, k));
            const float target = at(filterTarget, k);
            auto& filterGain = at(c.filterGain, k);
            // A fresh stream has no previous response to crossfade from.
            if (validFrames == 0) filterGain = target;
            else filterGain += f.transition * (target - filterGain);
            value *= filterGain;
            at(c.fftData, 2 * k) = value.real();
            at(c.fftData, 2 * k + 1) = (k == 0 || k == size / 2) ? 0.0f : value.imag();
            const int index = at(table.display, k);
            if (display && index >= 0)
                at(levels, index) = std::max(at(levels, index), juce::Decibels::gainToDecibels(std::abs(value) * 2.0f / static_cast<float>(size), -90.0f));
        }
        for (int i = 0; i < maxModules; ++i) at(c.modules, i).frozen = at(f.hold, i);
        fft.performRealOnlyInverseTransform(c.fftData.data());
        for (int i = 0; i < size; ++i)
            at(c.output, (position + i) & wrap) += at(c.fftData, i) * at(window, i) * 0.5f;
        if (display)
            for (int i = 0; i < displayBins; ++i)
                at(spectrum, i).store(at(levels, i), std::memory_order_relaxed);
    }
};
