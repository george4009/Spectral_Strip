#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "SpectralEngine.h"
#include <cstdint>
#include <mutex>

/*
    SpectralStripProcessor
    ======================

    The plug-in: parameters, state, the module chain and the audio callback around
    SpectralEngine.

    Threading model
    - The chain (which effect sits in which position) is edited on the message thread and
      published to the audio thread through a sequence counter, without locks.
    - Allocation (Delay history, Shimmer lines) happens on the message thread under
      `engineMutation`. The audio thread only *tries* that lock: if it is busy, the block
      is output silent instead of waiting.
    - Changing the FFT size, the chain or the whole state fades the output out, switches
      at silence, waits for the new latency to fill and fades back in.

    Parameters
    - Global: low, high, bandFilter, mix, output, fftSize.
    - Per module instance ("slot" 0...19): every effect's parameters, named
      "instance<N>_<id>". For compatibility with the first versions, the Freeze, Gate and
      Delay parameters of slots 1, 2 and 3 keep their original unprefixed IDs (see
      moduleParamID). A module's parameters are automatable only while its slot holds it.
*/
class SpectralStripProcessor : public juce::AudioProcessor
{
public:
    using Chain = std::array<int, SpectralEngine::maxModules>;

    SpectralStripProcessor();

    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void reset() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Spectral Strip"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    // A frozen spectrum can sustain forever; report a generous but finite tail.
    double getTailLengthSeconds() const override { return 120.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // Chain editing (message thread). Entries use SpectralEngine::makeEntry; 0 is empty.
    Chain getChain() const noexcept;
    void setChain(const Chain&, bool notifyHost = true);
    bool addInstance(int type);     // appends an effect in the first free slot

    // Parameter ID of parameter `id` (e.g. "threshold") for module slot `slot`.
    static juce::String moduleParamID(int slot, const char* id);

private:
    // Effect type occupying each slot, or 0 when the slot is empty. Module parameters
    // read this to decide whether the host may automate them. Declared before `state`
    // so that it outlives every parameter.
    std::array<std::atomic<int>, SpectralEngine::maxModules> slotType {};

public:
    juce::AudioProcessorValueTreeState state;
    SpectralEngine engine;
    std::atomic<int> activeFFTSize { SpectralEngine::defaultSize };   // for the editor

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters(const std::array<std::atomic<int>, SpectralEngine::maxModules>& slotType);
    float value(const char* id) const { return state.getRawParameterValue(id)->load(); }
    void render(juce::AudioBuffer<float>&, bool bypass);
    void readSettings(SpectralEngine::Settings&) const;
    void restartStream();
    void trackTransport(int blockSamples);
    int activeLatency() const noexcept { return engine.getLatency(activeChain); }

    // Serialises allocation and chain publication. The audio thread only tries it.
    std::mutex engineMutation;

    // Lock-free chain publication: `chainEpoch` is odd while the entries are being
    // written and even when they form a consistent snapshot.
    std::array<std::atomic<int>, SpectralEngine::maxModules> chainEntries {};
    std::atomic<std::uint32_t> chainEpoch { 0 };
    Chain copyChain(std::uint32_t& epoch) const noexcept;
    void publishChain(const Chain&) noexcept;

    // Audio-thread copy of what the engine currently runs.
    Chain activeChain { 0x11, 0x22, 0x33 };
    std::uint32_t activeEpoch = 0;
    // Bumped by setStateInformation and by a reset() that could not take the lock.
    std::atomic<unsigned> stateRevision { 0 };
    unsigned activeStateRevision = 0;

    // Raw parameter pointers per slot, in the order of the engine field table.
    std::array<std::vector<std::atomic<float>*>, SpectralEngine::maxModules> moduleParameters;

    juce::SmoothedValue<float> mix, outputGain, reconfigureGain;
    int warmupSamples = 0;          // silent samples left while a new latency fills
    // A 5 ms onset fade on (re)start, and a crossfade from the last output on transport jumps.
    int fadeSamples = 240, startupPosition = 0, bridgePosition = 240;
    std::array<float, 2> lastOutput {}, bridgeOutput {};
    bool prepared = false, wasPlaying = false, haveExpectedPosition = false;
    juce::int64 expectedPosition = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralStripProcessor)
};
