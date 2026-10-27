#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "StripLookAndFeel.h"

// A slider with its name label, attached to a parameter. The value is not drawn next to
// the slider: the editor's info bar shows it while the mouse is over the control.
struct LabelledSlider
{
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    void attach(juce::AudioProcessorValueTreeState& state, const juce::String& parameterID, const juce::String& name,
                const juce::String& unit, int decimals, const juce::String& help, const StripLookAndFeel& look,
                juce::Slider::SliderStyle style = juce::Slider::LinearHorizontal)
    {
        slider.setName(name);
        slider.setSliderStyle(style);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setTextValueSuffix(unit);
        slider.setTooltip(help);
        label.setText(name, juce::dontSendNotification);
        label.setFont(look.font(14.0f));
        label.setBorderSize({});
        label.setColour(juce::Label::textColourId, StripTheme::text);
        label.setInterceptsMouseClicks(false, false);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, parameterID, slider);
        // Set after the attachment, which would otherwise format with the parameter's 0.001 step.
        slider.textFromValueFunction = [decimals](double v) { return juce::String(v, decimals); };
        slider.updateText();
    }

    void setVisible(bool visible) { slider.setVisible(visible); label.setVisible(visible); }

    // "Threshold   -48.0 dB   -   help text", for the info bar.
    juce::String describe()
    {
        auto description = label.getText() + "   " + slider.getTextFromValue(slider.getValue());
        if (slider.getTooltip().isNotEmpty()) description << "   " << juce::String(juce::CharPointer_UTF8("\xe2\x80\x94")) << "   " << slider.getTooltip();
        return description;
    }
};

// A toggle button attached to a boolean parameter.
struct AttachedToggle
{
    juce::ToggleButton button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;

    // style: "" (text button), "power" or "filter"; see StripLookAndFeel.
    void attach(juce::AudioProcessorValueTreeState& state, const juce::String& parameterID, const juce::String& text,
                const juce::String& help, const char* style = "")
    {
        button.setButtonText(text);
        button.setTooltip(help);
        button.getProperties().set("style", style);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, parameterID, button);
    }

    juce::String describe()
    {
        const bool power = button.getProperties()["style"].toString() == "power";
        auto description = (power ? juce::String("Module") : button.getButtonText()) + "   " + (button.getToggleState() ? "on" : "off");
        if (button.getTooltip().isNotEmpty()) description << "   " << juce::String(juce::CharPointer_UTF8("\xe2\x80\x94")) << "   " << button.getTooltip();
        return description;
    }
};
