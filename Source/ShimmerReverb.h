#pragma once
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

/*
    ShimmerReverb
    =============

    One channel of the Shimmer module: an eight-line feedback delay network (FDN) with
    Hadamard scattering, modulated line lengths, input diffusers and an octave-up pitch
    shifter both on the input ("excitation") and inside the feedback loop ("shimmer").

    Storage is allocated in prepare() on the message thread; process() is allocation-free.
    reset() invalidates the delay history logically instead of clearing the buffers.
*/
class ShimmerReverb
{
public:
    void prepare(double rate, int channel)
    {
        sr = static_cast<float>(rate);
        side = static_cast<float>(channel);
        for (auto& d : lines) d.prepare(static_cast<int>(sr * .30f) + 8);
        for (auto& d : diffusers) d.prepare(static_cast<int>(sr * .04f) + 8);
        for (auto& v : voices) v.prepare(static_cast<int>(sr * .10f) + 8);
        smooth = 1 - std::exp(-1 / (sr * .05f));
        pitchFilter = 1 - std::exp(-twoPi * std::min(8000.0f, sr * .18f) / sr);
        dcPole = std::exp(-twoPi * 60 / sr);
        for (int j = 0; j < 8; ++j) lfoStep[index(j)] = twoPi * (.071f + .019f * static_cast<float>(j)) / sr;
        for (int j = 0; j < 4; ++j) diffuserDelay[index(j)] = sr * (.0041f + static_cast<float>(j) * .0023f) * (1 + side * .031f);
        for (int voice = 0; voice < 2; ++voice)
        {
            // Independent grain lengths keep the two octave voices from sharing
            // the same cancellation frequencies.
            auto& v = voices[index(voice)];
            v.span = sr * (voice == 0 ? .0437f : .0613f) * (1 + side * .013f);
            v.rate = 1 / v.span;
        }
        lfoDepth = sr * .00025f;
        reset();
    }

    void release()
    {
        for (auto& d : lines) d.release();
        for (auto& d : diffusers) d.release();
        for (auto& v : voices) v.release();
    }

    bool ready() const { return lines[0].ready(); }

    void reset()
    {
        for (auto& d : lines) d.reset();
        for (auto& d : diffusers) d.reset();
        for (auto& v : voices) v.reset();
        low.fill(0);
        for (int j = 0; j < 8; ++j) phase[index(j)] = .7f * static_cast<float>(j) + .31f * side;
        controlCounter = 0;
        decay = targetDecay; size = targetSize; shimmer = targetShimmer; tone = targetTone; mix = targetMix;
    }

    // decay in seconds; size, shimmer amount, tone and mix in 0...1. Smoothed over ~50 ms.
    void set(float newDecay, float newSize, float newShimmer, float newTone, float newMix)
    {
        targetDecay = newDecay; targetSize = newSize; targetShimmer = newShimmer; targetTone = newTone; targetMix = newMix;
    }

    float process(float input)
    {
        decay += smooth * (targetDecay - decay);
        size += smooth * (targetSize - size);
        shimmer += smooth * (targetShimmer - shimmer);
        tone += smooth * (targetTone - tone);
        mix += smooth * (targetMix - mix);
        updateControls();

        // Four allpass diffusers smear the input before it enters the tank.
        float diffuse = input;
        for (int j = 0; j < 4; ++j)
        {
            auto& d = diffusers[index(j)];
            const float delayed = d.read(diffuserDelay[index(j)]);
            const float next = diffuse + .65f * delayed;
            d.push(next);
            diffuse = delayed - .65f * next;
        }

        // Read the eight modulated lines through their damping lowpass.
        std::array<float, 8> values {};
        std::array<float, 2> send {};
        float wet = 0, meanFeedback = 0;
        for (int j = 0; j < 8; ++j)
        {
            const auto k = index(j);
            phase[k] += lfoStep[k];
            if (phase[k] > twoPi) phase[k] -= twoPi;
            const float value = lines[k].read(lengths[k] + lfoDepth * std::sin(phase[k]));
            low[k] += damping * (value - low[k]);
            values[k] = low[k] * feedback[k];
            send[0] += values[k] * norm8;
            send[1] += values[k] * (j % 2 ? -norm8 : norm8);
            meanFeedback += feedback[k] * .125f;
            wet += value * (j % 2 ? -.25f : .25f);
        }

        // Octave up: the loop voice shifts the tank's feedback, the input voice excites
        // the tank with the octave directly, so it stays audible without shortening the decay.
        std::array<float, 2> shifted {}, excitation {};
        for (int voice = 0; voice < 2; ++voice)
        {
            const auto k = index(voice);
            voices[k].advance();
            shifted[k] = voices[k].loop.process(send[k], voices[k], pitchFilter, dcPole);
            excitation[k] = voices[k].input.process(diffuse, voices[k], pitchFilter, dcPole);
        }

        // Normalised Hadamard scattering (fast Walsh-Hadamard transform).
        for (int step = 1; step < 8; step *= 2)
            for (int start = 0; start < 8; start += step * 2)
                for (int j = 0; j < step; ++j)
                {
                    const auto a = index(start + j), b = index(start + j + step);
                    const float x = values[a], y = values[b];
                    values[a] = x + y;
                    values[b] = x - y;
                }

        // Budget the pitch return against the requested decay loss instead of removing a
        // large share of the tank per pass. The two return directions are orthogonal.
        const float pitchAmount = std::min(.12f, (1 - meanFeedback) * .2f) * shimmer;
        for (int j = 0; j < 8; ++j)
        {
            const float octaveReturn = (shifted[0] + (j % 2 ? -shifted[1] : shifted[1])) * norm8;
            const float feedbackValue = (1 - pitchAmount) * values[index(j)] * norm8 + pitchAmount * octaveReturn;
            const float next = diffuse * .25f + shimmer * .35f * excitation[index(j % 2)] + feedbackValue;
            // Smooth saturation only on excessive internal feedback, not on the dry path.
            const float bounded = std::abs(next) <= 1.5f ? next : std::copysign(1.5f + .5f * std::tanh((std::abs(next) - 1.5f) * 2), next);
            lines[index(j)].push(bounded);
        }
        return input + mix * (wet - input);
    }

private:
    static constexpr float twoPi = 6.2831853f;
    static constexpr float norm8 = .35355339f;     // 1 / sqrt(8)
    static constexpr size_t index(int i) noexcept { return static_cast<size_t>(i); }

    // Fractional delay line with linear interpolation. Samples older than the history
    // written since the last reset read as silence.
    struct Delay
    {
        std::vector<float> data;
        int write = 0, valid = 0;
        void prepare(int n) { data.resize(static_cast<size_t>(n)); reset(); }
        void release() { std::vector<float>().swap(data); reset(); }
        bool ready() const { return !data.empty(); }
        void reset() { write = valid = 0; }
        float read(float delay) const
        {
            delay = std::clamp(delay, 1.0f, static_cast<float>(data.size() - 2));
            const int a = static_cast<int>(delay);
            const float fraction = delay - static_cast<float>(a);
            const auto sample = [&](int age)
            {
                if (age > valid) return 0.0f;
                int position = write - age;
                if (position < 0) position += static_cast<int>(data.size());
                return data[static_cast<size_t>(position)];
            };
            const float first = sample(a);
            return first + (sample(a + 1) - first) * fraction;
        }
        void push(float x)
        {
            data[static_cast<size_t>(write)] = x;
            if (++write == static_cast<int>(data.size())) write = 0;
            valid = std::min(valid + 1, static_cast<int>(data.size()) - 1);
        }
    };

    struct Voice;

    // Two-head granular octave shifter: the signal is band-limited, DC-blocked and read
    // back by two heads half a grain apart, crossfaded with a raised-cosine window.
    struct Shifter
    {
        Delay buffer;
        float lp1 = 0, lp2 = 0, previous = 0, hp = 0;
        void reset() { buffer.reset(); lp1 = lp2 = previous = hp = 0; }
        float process(float input, const Voice& voice, float filter, float pole)
        {
            lp1 += filter * (input - lp1);
            lp2 += filter * (lp1 - lp2);
            hp = lp2 - previous + pole * hp;
            previous = lp2;
            buffer.push(hp);
            float out = 0;
            for (int head = 0; head < 2; ++head)
                out += voice.gain[index(head)] * buffer.read(voice.delay[index(head)]);
            return out;
        }
    };

    // The input and loop shifters of a voice always run with the same grain phase,
    // so the window and read positions are computed once per sample for both.
    struct Voice
    {
        Shifter input, loop;
        float span = 0, rate = 0, phase = 0;
        std::array<float, 2> gain {}, delay {};
        void prepare(int n) { input.buffer.prepare(n); loop.buffer.prepare(n); reset(); }
        void release() { input.buffer.release(); loop.buffer.release(); reset(); }
        void reset() { input.reset(); loop.reset(); phase = 0; }
        void advance()
        {
            phase += rate;
            if (phase >= 1) phase -= 1;
            for (int head = 0; head < 2; ++head)
            {
                float p = phase + static_cast<float>(head) * .5f;
                if (p >= 1) p -= 1;
                gain[index(head)] = .5f - .5f * std::cos(twoPi * p);
                delay[index(head)] = 2 + (1 - p) * span;
            }
        }
    };

    // Damping and line feedback follow tone, size and decay; refreshed every 32 samples.
    void updateControls()
    {
        static constexpr float times[8] = { .0311f, .0371f, .0419f, .0479f, .0533f, .0617f, .0713f, .0797f };
        if (controlCounter-- > 0) return;
        controlCounter = 31;
        damping = 1 - std::exp(-twoPi * (700 + std::pow(tone, 2.0f) * 14500) / sr);
        for (int j = 0; j < 8; ++j)
        {
            const auto k = index(j);
            lengths[k] = sr * times[j] * (.55f + size * 2.0f) * (1 + side * .023f);
            // Gain per pass so that the line decays by 60 dB in `decay` seconds.
            feedback[k] = std::pow(.001f, lengths[k] / (sr * std::max(.5f, decay)));
        }
    }

    std::array<Delay, 8> lines;
    std::array<Delay, 4> diffusers;
    std::array<Voice, 2> voices;
    std::array<float, 8> low {}, phase {}, lengths {}, feedback {};
    float sr = 48000, side = 0;
    float decay = 6, size = .6f, shimmer = .45f, tone = .55f, mix = .35f;
    float targetDecay = 6, targetSize = .6f, targetShimmer = .45f, targetTone = .55f, targetMix = .35f;
    float smooth = 0, damping = 0, pitchFilter = 0, dcPole = 0;
    // Fixed per sample rate and channel, computed once in prepare.
    std::array<float, 8> lfoStep {};
    std::array<float, 4> diffuserDelay {};
    float lfoDepth = 0;
    int controlCounter = 0;
};
