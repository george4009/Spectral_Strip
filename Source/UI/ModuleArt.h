#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../SpectralEngine.h"

// Illustrations drawn at the top of each module. They are pictures of what the current
// settings do (echo spacing, tremolo envelope, ...), not measurements of the audio.

// Parameter values of the module being drawn, looked up by base ID (e.g. "delayMs").
struct ModuleArtValues
{
    std::function<float(const char*)> raw;          // value in the parameter's own units
    std::function<float(const char*)> normalised;   // 0...1 along the parameter's range
    float freezePhase = 0;                          // animation phase of a live Freeze
    double seconds = 0;                             // clock for animated drawings
};

// Parameters a module's drawing depends on. The first one is the module's on/off switch.
const std::vector<const char*>& artParameters(int type);
// True while the drawing changes every frame (a live Freeze, an enabled Tremolo).
bool artIsAnimated(int type, const ModuleArtValues&);
void drawModuleArt(juce::Graphics&, int type, juce::Rectangle<float> area, const ModuleArtValues&);
