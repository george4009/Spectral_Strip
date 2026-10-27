#pragma once
#include "../SpectralEngine.h"
#include <vector>

// What each module shows in the editor: its title, its on/off parameter, its slider rows
// and its buttons. Parameter IDs are base IDs; SpectralStripProcessor::moduleParamID turns
// them into the ID of a given slot.
struct SliderSpec
{
    const char* id;
    const char* label;
    const char* unit;          // appended to the value in the info bar
    int decimals;
    const char* help = "";
};

struct ButtonSpec
{
    const char* id;
    const char* label;
    const char* help = "";
};

struct ModuleSpec
{
    int type;
    const char* title;
    const char* powerId;                // on/off switch in the header, or nullptr
    std::vector<SliderSpec> sliders;
    std::vector<ButtonSpec> buttons;    // a row below the sliders
    int buttonHeight;
};

inline const std::vector<ModuleSpec>& moduleSpecs()
{
    using T = SpectralEngine::ModuleType;
    static const std::vector<ModuleSpec> specs {
        { T::freeze, "freeze", "freezeModule",
          {},   // Smear remains available to saved sessions and automation, but is not shown.
          { { "freeze", "Freeze", "Captures the current spectrum; press it while audio is playing." } }, 34 },
        { T::gate, "gate/tilt", nullptr,
          { { "threshold", "Threshold", " dB", 1 },
            { "toneTilt", "Tilt", " dB/oct", 1, "Tone tilt around 1 kHz: negative darkens, positive brightens, zero is neutral. Works even with Gate off. End-band gain is limited to +/-18 dB." },
            { "gateAttack", "Attack", " ms", 0 },
            { "gateRelease", "Release", " ms", 0 } },
          { { "gate", "Gate", "Per-bin threshold suppression. Tilt works with Gate off." },
            { "invert", "Invert", "Keeps the components below the threshold instead." } }, 24 },
        { T::delay, "delay", "delay",
          { { "delayMs", "Time", " ms", 0, "Delay time; the shortest effective delay is one FFT hop (FFT size / 4 samples)." },
            { "spread", "Dispersion", "", 2, "Spreads delay times between low and high frequencies of the band, evenly in octaves." },
            { "feedback", "Feedback", "", 2 },
            { "damping", "Damping", "", 2, "Each repeat loses high frequencies above 200 Hz, up to 90% at 20 kHz." },
            { "delayMix", "Mix", "", 2 } },
          {}, 0 },
        { T::blur, "blur", "blur",
          { { "blurAmount", "Amount", "", 2 },
            { "blurWidth", "Width", " Hz", 0 },
            { "blurMix", "Mix", "", 2 } },
          {}, 0 },
        { T::tremolo, "tremolo", "tremolo",
          { { "tremoloRate", "Rate", " Hz", 2 },
            { "tremoloDepth", "Depth", "", 2 },
            { "tremoloShape", "Shape", "", 2, "0: smooth sine. 0.5: rounded square. 1: short pulses (12.5% duty). Higher Depth makes the contrast more audible." } },
          {}, 0 },
        { T::shimmer, "shimmer", "reverb",
          { { "reverbDecay", "Decay", " s", 2 },
            { "reverbSize", "Size", "", 2 },
            { "reverbShimmer", "Shimmer", "", 2, "Amount of +12 semitone excitation and feedback in the reverb. Works without Freeze." },
            { "reverbTone", "Tone", "", 2 },
            { "reverbMix", "Mix", "", 2 } },
          {}, 0 },
    };
    return specs;
}

inline const ModuleSpec* findModuleSpec(int type)
{
    for (const auto& spec : moduleSpecs())
        if (spec.type == type) return &spec;
    return nullptr;
}

// Names offered by the "Add module" menus, indexed by type - 1.
inline const juce::StringArray& moduleMenuNames()
{
    static const juce::StringArray names { "Freeze", "Gate / Tilt", "Delay", "Blur", "Tremolo", "Shimmer" };
    return names;
}
