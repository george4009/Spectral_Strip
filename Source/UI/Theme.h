#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Colours and fixed layout of the editor. Everything is laid out on a 1040 x 800 canvas
// and scaled to the window size with an affine transform.
namespace StripTheme
{
    const juce::Colour background (0xff1d1e1f), panel (0xff2b2c2e), tile (0xff333436),
                       grid (0xff323335), gridMajor (0xff48494b), buttonFill (0xff3c3d3f),
                       buttonHover (0xff47484a), track (0xff3a3b3d), outline (0xff6a6b6d),
                       blue (0xff41729f), blueLight (0xff5b8fc4), text (0xffe6e6e6),
                       dim (0xff8c8d8f), thumb (0xffd6d6d6);

    namespace Layout
    {
        constexpr float canvasWidth = 1040.0f, canvasHeight = 800.0f;
        const juce::Rectangle<float> scope (12, 64, 940, 192);      // analyser
        const juce::Rectangle<float> info (172, 754, 676, 32);      // info bar
        // Module strip: columns of `moduleW`, `modulePitch` apart, scrolling inside `stripW`.
        constexpr int stripX = 12, stripY = 268, stripW = 940;
        constexpr int moduleW = 223, moduleH = 432, modulePitch = 239, headerH = 40;
        constexpr int rowH = 30;                                    // one "label + slider" row
        constexpr int sidebarX = 964;                               // global controls column
    }
}
