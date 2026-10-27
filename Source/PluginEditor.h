#pragma once
#include "PluginProcessor.h"
#include "UI/Controls.h"
#include "UI/ModuleStrip.h"
#include "UI/SpectrumView.h"
#include "UI/StripLookAndFeel.h"

/*
    SpectralStripEditor
    ===================

    Layout (on a 1040 x 800 canvas, scaled to the window with a fixed aspect ratio):

        title
        analyser (SpectrumView)                       | sidebar: Low, High,
        module strip (ModuleStrip)                    |          Filter,
                                                      |          Dry/wet, Output
        [+ Add module]  [ info bar ]           FFT [size]

    The info bar shows the name, value and help of the control under the mouse, or the
    current FFT size and latency.
*/
class SpectralStripEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SpectralStripEditor(SpectralStripProcessor&);
    ~SpectralStripEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::String statusText() const;
    juce::String describe(juce::Component*);

    SpectralStripProcessor& audioProcessor;
    StripLookAndFeel look;
    SpectrumView spectrum;
    ModuleStrip strip;

    // Sidebar
    LabelledSlider low, high, mix, output;
    AttachedToggle bandFilter;
    // Bottom bar
    juce::ComboBox addModule, fftSizeChoice;
    juce::Label fftSizeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> fftSizeAttachment;

    juce::String infoText;
    bool infoIsStatus = true;
    double lastTick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralStripEditor)
};
