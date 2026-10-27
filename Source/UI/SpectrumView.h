#pragma once
#include "../PluginProcessor.h"
#include "StripLookAndFeel.h"

// The analyser: the wet spectrum (left channel, before mix and output) on a log-frequency
// grid, with the shared effect band. The two dots at the top are the LOW / HIGH band
// edges and can be dragged. Drawn in the editor's 1040 x 800 coordinates (Layout::scope).
class SpectrumView : public juce::Component
{
public:
    SpectrumView(SpectralStripProcessor&, const StripLookAndFeel&);

    // Pulls new levels from the engine (UI timer). Repaints only when something moved.
    void update();
    // Info bar text for a point in local coordinates.
    juce::String describeAt(juce::Point<float>) const;

    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    float xForFrequency(float frequency) const;
    float frequencyForX(float x) const;
    int bandEdgeAt(juce::Point<float>) const;          // 0 none, 1 low, 2 high
    juce::RangedAudioParameter* edgeParameter(int edge) const;

    SpectralStripProcessor& processor;
    const StripLookAndFeel& look;
    std::array<float, SpectralEngine::displayBins> display {};
    float shownLow = -1, shownHigh = -1;
    int draggedEdge = 0;
};
