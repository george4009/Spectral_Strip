#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    using ModuleType = SpectralEngine::ModuleType;
    using ModuleSettings = SpectralEngine::ModuleSettings;

    // A parameter that the host may automate only while its slot holds its effect type.
    // Global parameters use slot -1 and are always automatable.
    template <typename Base>
    struct ChainParameter : Base
    {
        template <typename... Args>
        ChainParameter(const std::atomic<int>* occupiedTypes, int slotIndex, int effectType, Args&&... args)
            : Base(std::forward<Args>(args)...), types(occupiedTypes), slot(slotIndex), kind(effectType) {}

        bool isAutomatable() const override
        {
            return slot < 0 || types[slot].load(std::memory_order_relaxed) == kind;
        }

        const std::atomic<int>* types;
        int slot, kind;
    };

    // How each per-module parameter maps onto SpectralEngine::ModuleSettings.
    // The order here is the order of SpectralStripProcessor::moduleParameters.
    struct BoolField  { const char* id; bool ModuleSettings::* field; };
    struct FloatField { const char* id; float ModuleSettings::* field; };
    constexpr BoolField boolFields[] {
        { "freezeModule", &ModuleSettings::freezeModule }, { "freeze", &ModuleSettings::freeze },
        { "gate", &ModuleSettings::gate }, { "invert", &ModuleSettings::invert }, { "delay", &ModuleSettings::delay },
        { "blur", &ModuleSettings::blur }, { "tremolo", &ModuleSettings::tremolo }, { "reverb", &ModuleSettings::reverb },
    };
    constexpr FloatField floatFields[] {
        { "smear", &ModuleSettings::smear }, { "shimmer", &ModuleSettings::shimmer },
        { "threshold", &ModuleSettings::threshold }, { "gateAttack", &ModuleSettings::gateAttack },
        { "gateRelease", &ModuleSettings::gateRelease }, { "gateTilt", &ModuleSettings::gateTilt },
        { "toneTilt", &ModuleSettings::toneTilt },
        { "delayMs", &ModuleSettings::delayMs }, { "spread", &ModuleSettings::spread }, { "feedback", &ModuleSettings::feedback },
        { "damping", &ModuleSettings::damping }, { "delayMix", &ModuleSettings::delayMix },
        { "blurAmount", &ModuleSettings::blurAmount }, { "blurWidth", &ModuleSettings::blurWidth }, { "blurMix", &ModuleSettings::blurMix },
        { "tremoloRate", &ModuleSettings::tremoloRate }, { "tremoloDepth", &ModuleSettings::tremoloDepth },
        { "tremoloShape", &ModuleSettings::tremoloShape },
        { "reverbDecay", &ModuleSettings::reverbDecay }, { "reverbSize", &ModuleSettings::reverbSize },
        { "reverbShimmer", &ModuleSettings::reverbShimmer }, { "reverbTone", &ModuleSettings::reverbTone },
        { "reverbMix", &ModuleSettings::reverbMix },
    };

    const SpectralStripProcessor::Chain defaultChain { 0x11, 0x22, 0x33 };

    // Smoothstep 0 -> 1, used for the onset and transport crossfades.
    float smoothstep(float t) { return t * t * (3.0f - 2.0f * t); }
}

//==============================================================================
SpectralStripProcessor::SpectralStripProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "SpectralStripState", makeParameters(slotType))
{
    for (size_t i = 0; i < defaultChain.size(); ++i)
    {
        chainEntries[i].store(defaultChain[i], std::memory_order_relaxed);
        slotType[i].store(SpectralEngine::entryType(defaultChain[i]), std::memory_order_relaxed);
    }
    for (int slot = 0; slot < SpectralEngine::maxModules; ++slot)
    {
        auto& pointers = moduleParameters[static_cast<size_t>(slot)];
        for (const auto& f : boolFields)  pointers.push_back(state.getRawParameterValue(moduleParamID(slot, f.id)));
        for (const auto& f : floatFields) pointers.push_back(state.getRawParameterValue(moduleParamID(slot, f.id)));
    }
}

// Parameter order and IDs are part of saved sessions and host automation: new parameters
// must be appended, never inserted.
juce::AudioProcessorValueTreeState::ParameterLayout SpectralStripProcessor::makeParameters(const std::array<std::atomic<int>, SpectralEngine::maxModules>& types)
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto* occupied = types.data();
    const auto toggle = [&](const juce::String& id, const juce::String& name, bool defaultValue, int slot, int kind, int version = 1)
    {
        layout.add(std::make_unique<ChainParameter<juce::AudioParameterBool>>(occupied, slot, kind, juce::ParameterID { id, version }, name, defaultValue));
    };
    const auto number = [&](const juce::String& id, const juce::String& name, float lo, float hi, float defaultValue,
                            int slot, int kind, float skew = 1.0f, int version = 1)
    {
        layout.add(std::make_unique<ChainParameter<juce::AudioParameterFloat>>(occupied, slot, kind, juce::ParameterID { id, version }, name,
                                                                               juce::NormalisableRange<float>(lo, hi, 0.001f, skew), defaultValue));
    };
    constexpr int global = -1;

    // Version 0.1 parameters: the original Freeze (slot 0), Gate (slot 1) and Delay (slot 2),
    // plus the global controls.
    toggle("freezeModule", "Freeze module", true, 0, ModuleType::freeze);
    toggle("freeze", "Capture freeze", false, 0, ModuleType::freeze);
    number("smear", "Temporal smear", 0, 1, 0, 0, ModuleType::freeze);
    toggle("gate", "Gate enabled", false, 1, ModuleType::gate);
    toggle("invert", "Gate inverted", false, 1, ModuleType::gate);
    number("threshold", "Gate threshold", -90, 0, -48, 1, ModuleType::gate);
    toggle("delay", "Delay enabled", false, 2, ModuleType::delay);
    number("delayMs", "Delay time (ms)", 10, 1000, 250, 2, ModuleType::delay, 0.5f);
    number("spread", "Delay dispersion", -1, 1, 0.5f, 2, ModuleType::delay);
    number("feedback", "Delay feedback", 0, 0.92f, 0.3f, 2, ModuleType::delay);
    number("delayMix", "Delay mix", 0, 1, 0.5f, 2, ModuleType::delay);
    number("low", "Selection low (Hz)", 20, 20000, 20, global, 0, 0.25f);
    number("high", "Selection high (Hz)", 20, 20000, 20000, global, 0, 0.25f);
    number("mix", "Global mix", 0, 1, 1, global, 0);
    number("output", "Output (dB)", -24, 12, 0, global, 0);
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { "fftSize", 1 }, "FFT Size",
                                                            juce::StringArray { "512", "1024", "2048", "4096", "8192" }, 2));
    toggle("bandFilter", "Band filter enabled", true, global, 0);
    number("shimmer", "Freeze shimmer", 0, 1, 0, 0, ModuleType::freeze);
    number("gateAttack", "Gate attack (ms)", 1, 500, 10, 1, ModuleType::gate, 0.4f);
    number("gateRelease", "Gate release (ms)", 5, 2000, 100, 1, ModuleType::gate, 0.4f);
    number("gateTilt", "Gate threshold slope (legacy dB/oct)", 0, 6, 3, 1, ModuleType::gate);
    number("damping", "Delay damping", 0, 1, 0, 2, ModuleType::delay);

    // Freeze, Gate and Delay parameters for every other slot (0.3: modular strip).
    for (int slot = 0; slot < SpectralEngine::maxModules; ++slot)
    {
        const auto prefix = [slot](const char* effect) { return juce::String(effect) + " " + juce::String(slot + 1) + " "; };
        const auto addToggle = [&](const char* id, const char* label, bool defaultValue, int kind, const char* effect)
        {
            const auto key = moduleParamID(slot, id);
            if (key != id) toggle(key, prefix(effect) + label, defaultValue, slot, kind);
        };
        const auto addNumber = [&](const char* id, const char* label, float lo, float hi, float defaultValue, int kind, const char* effect, float skew = 1.0f)
        {
            const auto key = moduleParamID(slot, id);
            if (key != id) number(key, prefix(effect) + label, lo, hi, defaultValue, slot, kind, skew);
        };
        addToggle("freezeModule", "enabled", true, ModuleType::freeze, "Freeze");
        addToggle("freeze", "capture", false, ModuleType::freeze, "Freeze");
        addToggle("gate", "gate enabled", true, ModuleType::gate, "Gate");
        addToggle("invert", "inverted", false, ModuleType::gate, "Gate");
        addToggle("delay", "enabled", true, ModuleType::delay, "Delay");
        addNumber("smear", "smear", 0, 1, 0, ModuleType::freeze, "Freeze");
        addNumber("shimmer", "shimmer (legacy)", 0, 1, 0, ModuleType::freeze, "Freeze");
        addNumber("threshold", "threshold", -90, 0, -48, ModuleType::gate, "Gate");
        addNumber("delayMs", "time (ms)", 10, 1000, 250, ModuleType::delay, "Delay", .5f);
        addNumber("spread", "dispersion", -1, 1, .5f, ModuleType::delay, "Delay");
        addNumber("gateAttack", "attack (ms)", 1, 500, 10, ModuleType::gate, "Gate", .4f);
        addNumber("gateRelease", "release (ms)", 5, 2000, 100, ModuleType::gate, "Gate", .4f);
        addNumber("gateTilt", "threshold slope (legacy)", 0, 6, 3, ModuleType::gate, "Gate");
        addNumber("feedback", "feedback", 0, .92f, .3f, ModuleType::delay, "Delay");
        addNumber("damping", "damping", 0, 1, 0, ModuleType::delay, "Delay");
        addNumber("delayMix", "mix", 0, 1, .5f, ModuleType::delay, "Delay");
    }
    for (int slot = 0; slot < SpectralEngine::maxModules; ++slot)
    {
        const auto name = "Blur " + juce::String(slot + 1);
        toggle(moduleParamID(slot, "blur"), name + " enabled", true, slot, ModuleType::blur);
        number(moduleParamID(slot, "blurAmount"), name + " amount", 0, 1, .6f, slot, ModuleType::blur);
        number(moduleParamID(slot, "blurWidth"), name + " width (Hz)", 20, 4000, 400, slot, ModuleType::blur, .35f);
        number(moduleParamID(slot, "blurMix"), name + " mix", 0, 1, 1, slot, ModuleType::blur);
    }
    for (int slot = 0; slot < SpectralEngine::maxModules; ++slot)
    {
        const auto name = "Tremolo " + juce::String(slot + 1);
        toggle(moduleParamID(slot, "tremolo"), name + " enabled", true, slot, ModuleType::tremolo);
        number(moduleParamID(slot, "tremoloRate"), name + " rate (Hz)", .1f, 20, 4, slot, ModuleType::tremolo, .4f);
        number(moduleParamID(slot, "tremoloDepth"), name + " depth", 0, 1, .75f, slot, ModuleType::tremolo);
        number(moduleParamID(slot, "tremoloShape"), name + " shape", 0, 1, 0, slot, ModuleType::tremolo);
    }
    for (int slot = 0; slot < SpectralEngine::maxModules; ++slot)
    {
        const auto name = "Shimmer " + juce::String(slot + 1);
        toggle(moduleParamID(slot, "reverb"), name + " enabled", true, slot, ModuleType::shimmer);
        number(moduleParamID(slot, "reverbDecay"), name + " decay (s)", .5f, 20, 6, slot, ModuleType::shimmer, .5f);
        number(moduleParamID(slot, "reverbSize"), name + " size", 0, 1, .6f, slot, ModuleType::shimmer);
        number(moduleParamID(slot, "reverbShimmer"), name + " shimmer", 0, 1, .45f, slot, ModuleType::shimmer);
        number(moduleParamID(slot, "reverbTone"), name + " tone", 0, 1, .55f, slot, ModuleType::shimmer);
        number(moduleParamID(slot, "reverbMix"), name + " mix", 0, 1, .35f, slot, ModuleType::shimmer);
    }
    for (int slot = 0; slot < SpectralEngine::maxModules; ++slot)
        number(moduleParamID(slot, "toneTilt"), "Gate " + juce::String(slot + 1) + " tone tilt (dB/oct)", -6, 6, 0, slot, ModuleType::gate, 1.0f, 2);
    return layout;
}

juce::String SpectralStripProcessor::moduleParamID(int slot, const char* id)
{
    const juce::String name(id);
    if ((slot == 0 && (name == "freezeModule" || name == "freeze" || name == "smear" || name == "shimmer"))
        || (slot == 1 && (name == "gate" || name == "invert" || name == "threshold" || name == "gateAttack" || name == "gateRelease" || name == "gateTilt"))
        || (slot == 2 && (name == "delay" || name == "delayMs" || name == "spread" || name == "feedback" || name == "damping" || name == "delayMix")))
        return name;
    return "instance" + juce::String(slot + 1) + "_" + name;
}

bool SpectralStripProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    const auto out = layout.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
        && out == layout.getMainInputChannelSet();
}

//==============================================================================
void SpectralStripProcessor::prepareToPlay(double sampleRate, int)
{
    const std::lock_guard<std::mutex> mutation(engineMutation);
    activeChain = copyChain(activeEpoch);
    activeStateRevision = stateRevision.load();
    engine.prepare(sampleRate, static_cast<int>(value("fftSize")));
    engine.allocateFor(activeChain);
    setLatencySamples(activeLatency());
    activeFFTSize.store(engine.getSize());
    reconfigureGain.reset(sampleRate, 0.01);
    reconfigureGain.setCurrentAndTargetValue(1);
    warmupSamples = 0;
    fadeSamples = std::max(1, static_cast<int>(sampleRate * 0.005));
    startupPosition = 0;
    bridgePosition = fadeSamples;
    lastOutput.fill(0);
    bridgeOutput.fill(0);
    wasPlaying = haveExpectedPosition = false;
    prepared = true;
    mix.reset(sampleRate, 0.035);
    outputGain.reset(sampleRate, 0.035);
    mix.setCurrentAndTargetValue(value("mix"));
    outputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(value("output")));
}

// Drops stale STFT and Delay data at the start of a new transport segment. No allocation.
void SpectralStripProcessor::restartStream()
{
    engine.selectSize(engine.getChoice());
    startupPosition = 0;
    bridgeOutput = lastOutput;
    bridgePosition = 0;
}

void SpectralStripProcessor::reset()
{
    const std::unique_lock<std::mutex> mutation(engineMutation, std::try_to_lock);
    if (!mutation.owns_lock()) { stateRevision.fetch_add(1); return; }   // the audio thread will reset
    if (prepared) restartStream();
    wasPlaying = haveExpectedPosition = false;
}

// Play, seeks and loop jumps start a new segment: Freeze and Delay memories would
// otherwise replay audio from the previous position.
void SpectralStripProcessor::trackTransport(int blockSamples)
{
    if (auto* playHead = getPlayHead())
        if (const auto position = playHead->getPosition())
        {
            const bool playing = position->getIsPlaying();
            const auto time = position->getTimeInSamples();
            const bool seek = playing && wasPlaying && time && haveExpectedPosition
                           && std::abs(*time - expectedPosition) > 2;
            if (playing && (!wasPlaying || seek)) restartStream();
            wasPlaying = playing;
            haveExpectedPosition = playing && time.hasValue();
            if (time) expectedPosition = *time + blockSamples;
            return;
        }
    wasPlaying = haveExpectedPosition = false;
}

void SpectralStripProcessor::readSettings(SpectralEngine::Settings& s) const
{
    s.useInstances = true;
    s.chain = activeChain;
    for (size_t slot = 0; slot < s.modules.size(); ++slot)
    {
        auto& module = s.modules[slot];
        const auto& pointers = moduleParameters[slot];
        size_t index = 0;
        for (const auto& f : boolFields)  module.*(f.field) = pointers[index++]->load() > .5f;
        for (const auto& f : floatFields) module.*(f.field) = pointers[index++]->load();
    }
    s.bandFilter = value("bandFilter") > .5f;
    s.low = value("low");
    s.high = value("high");
}

void SpectralStripProcessor::render(juce::AudioBuffer<float>& buffer, bool bypass)
{
    juce::ScopedNoDenormals noDenormals;
    if (buffer.getNumSamples() == 0) return;
    // Never wait for UI allocation or a concurrent prepare/reset.
    const std::unique_lock<std::mutex> mutation(engineMutation, std::try_to_lock);
    if (!mutation.owns_lock() || !prepared) { buffer.clear(); return; }
    trackTransport(buffer.getNumSamples());

    // A pending FFT size, chain or state change: fade out, switch at silence, refill, fade in.
    const int requested = juce::jlimit(0, SpectralEngine::choices - 1, static_cast<int>(value("fftSize")));
    const auto requestedEpoch = chainEpoch.load(std::memory_order_acquire);
    const auto requestedRevision = stateRevision.load();
    const auto upToDate = [&] { return requested == engine.getChoice() && requestedEpoch == activeEpoch && requestedRevision == activeStateRevision; };
    if (!upToDate())
    {
        if (reconfigureGain.getCurrentValue() <= 0.0f)
        {
            engine.selectSize(requested);
            activeChain = copyChain(activeEpoch);
            activeStateRevision = requestedRevision;
            setLatencySamples(activeLatency());
            activeFFTSize.store(engine.getSize());
            warmupSamples = getLatencySamples();
        }
        reconfigureGain.setTargetValue(0);
    }
    else if (warmupSamples == 0) reconfigureGain.setTargetValue(1);

    const int count = juce::jmin(2, getTotalNumInputChannels());
    for (int ch = count; ch < buffer.getNumChannels(); ++ch) buffer.clear(ch, 0, buffer.getNumSamples());
    SpectralEngine::Settings s;
    readSettings(s);
    mix.setTargetValue(bypass ? 0.0f : value("mix"));
    outputGain.setTargetValue(bypass ? 1.0f : juce::Decibels::decibelsToGain(value("output")));

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float in[2] {}, wet[2] {}, dry[2] {};
        // Fade the INPUT so the ramp travels through the FFT and the compensated dry.
        // A fade of the output during its initial latency would end before audio arrives.
        const float onsetGain = smoothstep(static_cast<float>(startupPosition) / static_cast<float>(fadeSamples));
        startupPosition = std::min(fadeSamples, startupPosition + 1);
        for (int ch = 0; ch < count; ++ch) in[ch] = buffer.getSample(ch, i) * onsetGain;
        engine.process(in, wet, dry, count, s);

        float transitionGain = reconfigureGain.getNextValue();
        if (warmupSamples > 0)
        {
            transitionGain = 0;
            if (--warmupSamples == 0 && upToDate()) reconfigureGain.setTargetValue(1);
        }
        const float blend = mix.getNextValue(), gain = outputGain.getNextValue() * transitionGain;
        // After a transport jump, crossfade from the last output to avoid a click.
        const float bridgeGain = 1.0f - smoothstep(static_cast<float>(bridgePosition) / static_cast<float>(fadeSamples));
        bridgePosition = std::min(fadeSamples, bridgePosition + 1);
        for (int ch = 0; ch < count; ++ch)
        {
            const auto channel = static_cast<size_t>(ch);
            const float sample = (dry[ch] + blend * (wet[ch] - dry[ch])) * gain + bridgeOutput[channel] * bridgeGain;
            buffer.setSample(ch, i, sample);
            lastOutput[channel] = sample;
        }
    }
}

void SpectralStripProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)         { render(buffer, false); }
void SpectralStripProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { render(buffer, true); }
juce::AudioProcessorEditor* SpectralStripProcessor::createEditor() { return new SpectralStripEditor(*this); }

//==============================================================================
void SpectralStripProcessor::getStateInformation(juce::MemoryBlock& data)
{
    auto saved = state.copyState();
    const auto chain = getChain();
    juce::StringArray entries;
    for (const int entry : chain) entries.add(juce::String(entry));
    saved.setProperty("moduleChain", entries.joinIntoString(","), nullptr);
    saved.removeProperty("instanceChain", nullptr);
    saved.removeProperty("chainCode", nullptr);
    auto xml = saved.createXml();
    copyXmlToBinary(*xml, data);
}

void SpectralStripProcessor::setStateInformation(const void* data, int bytes)
{
    auto xml = getXmlFromBinary(data, bytes);
    if (xml == nullptr || !xml->hasTagName(state.state.getType())) return;
    auto restored = juce::ValueTree::fromXml(*xml);
    const auto addParameter = [&](const juce::String& id, const juce::var& value)
    {
        juce::ValueTree parameter("PARAM");
        parameter.setProperty("id", id, nullptr);
        parameter.setProperty("value", value, nullptr);
        restored.appendChild(parameter, nullptr);
    };
    // Values that reproduce how older sessions sounded before each parameter existed.
    const std::pair<const char*, float> legacy[] { { "fftSize", 2 }, { "shimmer", 0 }, { "gateAttack", 35 },
                                                   { "gateRelease", 35 }, { "gateTilt", 0 }, { "damping", 0 } };
    for (const auto& [id, legacyValue] : legacy)
        if (!restored.getChildWithProperty("id", id).isValid())
            addParameter(id, legacyValue);

    Chain chain {};
    if (restored.hasProperty("moduleChain"))
    {
        const auto parts = juce::StringArray::fromTokens(restored.getProperty("moduleChain").toString(), ",", "");
        for (int i = 0; i < juce::jmin(SpectralEngine::maxModules, parts.size()); ++i)
            chain[static_cast<size_t>(i)] = parts[i].getIntValue();
    }
    else if (restored.hasProperty("instanceChain"))
    {
        // Versions up to 0.7.7 packed at most eight entries into one 64-bit value.
        const auto packed = static_cast<juce::int64>(restored.getProperty("instanceChain"));
        for (int i = 0; i < 8; ++i) chain[static_cast<size_t>(i)] = static_cast<int>((packed >> (8 * i)) & 255);
    }
    else
    {
        // Version 0.3: the movable chain held Filter as well; keep the effect order without it.
        const int old = static_cast<int>(restored.getProperty("chainCode", 0x4321));
        int index = 0;
        for (int i = 0; i < 4; ++i)
        {
            const int type = (old >> (4 * i)) & 15;
            if (type >= ModuleType::freeze && type <= ModuleType::delay) chain[static_cast<size_t>(index++)] = (type << 4) | type;
        }
    }
    setChain(chain, false);
    // Old sessions do not carry instance parameters. Fill missing values explicitly,
    // so state recall cannot inherit settings from the previously loaded session.
    for (auto* parameter : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
            if (!restored.getChildWithProperty("id", ranged->paramID).isValid())
                addParameter(ranged->paramID, ranged->convertFrom0to1(ranged->getDefaultValue()));
    state.replaceState(restored);
    // Request a faded audio-thread reset even when topology and FFT size are unchanged.
    stateRevision.fetch_add(1);
}

//==============================================================================
SpectralStripProcessor::Chain SpectralStripProcessor::copyChain(std::uint32_t& epoch) const noexcept
{
    for (;;)
    {
        const auto begin = chainEpoch.load(std::memory_order_acquire);
        if (begin & 1u) continue;               // a writer is publishing
        Chain chain {};
        for (size_t i = 0; i < chain.size(); ++i)
            chain[i] = chainEntries[i].load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        if (begin != chainEpoch.load(std::memory_order_relaxed)) continue;
        epoch = begin;
        return chain;
    }
}

void SpectralStripProcessor::publishChain(const Chain& chain) noexcept
{
    const auto begin = chainEpoch.load(std::memory_order_relaxed);
    chainEpoch.store(begin + 1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    for (size_t i = 0; i < chain.size(); ++i)
        chainEntries[i].store(chain[i], std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    chainEpoch.store(begin + 2, std::memory_order_relaxed);
}

SpectralStripProcessor::Chain SpectralStripProcessor::getChain() const noexcept
{
    std::uint32_t epoch = 0;
    return copyChain(epoch);
}

// Drops invalid and duplicate-slot entries and compacts the chain to the front.
void SpectralStripProcessor::setChain(const Chain& chain, bool notifyHost)
{
    std::unique_lock<std::mutex> mutation(engineMutation);
    Chain sanitized {};
    std::uint32_t usedSlots = 0;
    int position = 0;
    for (const int entry : chain)
    {
        const int slot = SpectralEngine::entrySlot(entry), type = SpectralEngine::entryType(entry);
        if (slot < 0 || slot >= SpectralEngine::maxModules || type < 1 || type > ModuleType::count) continue;
        const auto bit = 1u << static_cast<std::uint32_t>(slot);
        if (usedSlots & bit) continue;
        sanitized[static_cast<size_t>(position++)] = entry;
        usedSlots |= bit;
    }
    if (sanitized == getChain()) return;
    engine.allocateFor(sanitized);
    publishChain(sanitized);
    for (auto& type : slotType) type.store(0, std::memory_order_relaxed);
    for (const int entry : sanitized)
        if (entry)
            slotType[static_cast<size_t>(SpectralEngine::entrySlot(entry))].store(SpectralEngine::entryType(entry), std::memory_order_relaxed);
    mutation.unlock();
    // The list of automatable parameters changed with the chain.
    updateHostDisplay(ChangeDetails {}.withNonParameterStateChanged(notifyHost).withParameterInfoChanged(true));
}

bool SpectralStripProcessor::addInstance(int type)
{
    if (type < 1 || type > ModuleType::count) return false;
    auto chain = getChain();
    std::uint32_t usedSlots = 0;
    int position = 0;
    for (const int entry : chain)
        if (entry)
        {
            usedSlots |= 1u << static_cast<std::uint32_t>(SpectralEngine::entrySlot(entry));
            ++position;
        }
    if (position == SpectralEngine::maxModules) return false;
    for (int slot = 0; slot < SpectralEngine::maxModules; ++slot)
        if (!(usedSlots & (1u << static_cast<std::uint32_t>(slot))))
        {
            chain[static_cast<size_t>(position)] = SpectralEngine::makeEntry(type, slot);
            setChain(chain);
            return true;
        }
    return false;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SpectralStripProcessor(); }
