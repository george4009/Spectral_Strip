#pragma once
#include "Theme.h"

// Flat, dark look: thin sliders with a blue fill and a pill thumb, rectangular toggle
// buttons, outlined combo boxes and the embedded Lato typeface.
//
// Toggle buttons pick their drawing from the "style" component property:
//   "power"  - an on/off icon (module headers)
//   "filter" - a square button with a band-pass icon (sidebar)
//   other    - a flat rectangular button showing the button text
class StripLookAndFeel : public juce::LookAndFeel_V4
{
public:
    StripLookAndFeel();

    juce::Typeface::Ptr regular, bold, light, lightItalic;
    juce::Font font(float height) const { return juce::Font(juce::FontOptions(regular).withHeight(height)); }
    static juce::Font font(const juce::Typeface::Ptr& typeface, float height) { return juce::Font(juce::FontOptions(typeface).withHeight(height)); }

    // Popup menus are drawn inside the (scaled) editor; this keeps their text at the same scale.
    float menuScale = 1.0f;

    juce::Typeface::Ptr getTypefaceForFont(const juce::Font&) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height, float position,
                          float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius(juce::Slider&) override { return 8; }
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawComboBox(juce::Graphics&, int width, int height, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override { return font(14.0f); }
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    void drawScrollbar(juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height, bool vertical,
                       int thumbStart, int thumbSize, bool over, bool down) override;
    int getMinimumScrollbarThumbSize(juce::ScrollBar&) override { return 24; }
    void drawPopupMenuBackground(juce::Graphics&, int width, int height) override;
    // A desktop menu stays where it opened; parenting it to the editor makes it follow the
    // plug-in window, at the same scale as the transformed combo box.
    juce::PopupMenu::Options getOptionsForComboBoxPopupMenu(juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override { return font(15.0f * menuScale); }
};

// Small flat button with a drawn icon (a close cross or a plus).
class IconButton : public juce::Button
{
public:
    enum class Icon { close, plus };
    explicit IconButton(Icon i = Icon::close) : juce::Button({}), icon(i) {}
    void paintButton(juce::Graphics&, bool over, bool down) override;
private:
    Icon icon;
};
