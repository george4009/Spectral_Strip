#include "StripLookAndFeel.h"
#include "EditorFonts.h"

using namespace StripTheme;

StripLookAndFeel::StripLookAndFeel()
{
    regular = juce::Typeface::createSystemTypefaceFor(EditorFonts::regular, EditorFonts::regularSize);
    bold = juce::Typeface::createSystemTypefaceFor(EditorFonts::bold, EditorFonts::boldSize);
    light = juce::Typeface::createSystemTypefaceFor(EditorFonts::light, EditorFonts::lightSize);
    lightItalic = juce::Typeface::createSystemTypefaceFor(EditorFonts::lightItalic, EditorFonts::lightItalicSize);
    setColour(juce::ResizableWindow::backgroundColourId, background);
    setColour(juce::Label::textColourId, text);
    setColour(juce::Slider::textBoxTextColourId, text);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ToggleButton::textColourId, text);
    setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::outlineColourId, outline);
    setColour(juce::ComboBox::textColourId, text);
    setColour(juce::ComboBox::arrowColourId, text);
    setColour(juce::PopupMenu::backgroundColourId, panel);
    setColour(juce::PopupMenu::textColourId, text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, blue);
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xff5a5b5d));
    setColour(juce::TooltipWindow::backgroundColourId, panel);
    setColour(juce::TooltipWindow::textColourId, text);
    setColour(juce::TextEditor::backgroundColourId, panel);
    setColour(juce::TextEditor::textColourId, text);
    setColour(juce::TextEditor::highlightColourId, blue);
    setColour(juce::CaretComponent::caretColourId, text);
}
juce::Typeface::Ptr StripLookAndFeel::getTypefaceForFont(const juce::Font& f)
{
    if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName() && regular != nullptr)
        return f.isBold() ? bold : regular;
    return juce::LookAndFeel_V4::getTypefaceForFont(f);
}
void StripLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
    float position, float, float, juce::Slider::SliderStyle, juce::Slider& slider)
{
    const bool vertical = slider.isVertical();
    const auto range = slider.getRange();
    // Bipolar parameters (tilt, output gain) fill from their zero point, the others from the start.
    const bool bipolar = range.getStart() < 0.0 && range.getEnd() > 0.0;
    const bool on = slider.isEnabled();
    if (!vertical)
    {
        const float cy = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
        const auto rail = juce::Rectangle<float>(static_cast<float>(x), cy-2.0f, static_cast<float>(width), 4.0f);
        g.setColour(StripTheme::track); g.fillRoundedRectangle(rail, 2.0f);
        const float from = bipolar ? slider.getPositionOfValue(0.0) : static_cast<float>(x);
        if (std::abs(position-from) > 0.5f)
        {
            g.setColour(on ? blue : blue.withSaturation(0.0f));
            g.fillRoundedRectangle(juce::Rectangle<float>(std::min(from, position), cy-2.0f, std::abs(position-from), 4.0f), 2.0f);
        }
        const auto knob = juce::Rectangle<float>(14.0f, 8.0f).withCentre({position, cy});
        g.setColour(slider.isMouseOverOrDragging() ? juce::Colours::white : thumb);
        g.fillRoundedRectangle(knob, 4.0f);
    }
    else
    {
        const float cx = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
        const auto rail = juce::Rectangle<float>(cx-3.0f, static_cast<float>(y), 6.0f, static_cast<float>(height));
        g.setColour(StripTheme::track); g.fillRoundedRectangle(rail, 3.0f);
        const float from = bipolar ? slider.getPositionOfValue(0.0) : static_cast<float>(y + height);
        if (std::abs(position-from) > 0.5f)
        {
            g.setColour(blue);
            g.fillRoundedRectangle(juce::Rectangle<float>(cx-3.0f, std::min(from, position), 6.0f, std::abs(position-from)), 3.0f);
        }
        const auto knob = juce::Rectangle<float>(8.0f, 16.0f).withCentre({cx, position});
        g.setColour(slider.isMouseOverOrDragging() ? juce::Colours::white : thumb);
        g.fillRoundedRectangle(knob, 4.0f);
    }
}
void StripLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool over, bool down)
{
    const auto style = button.getProperties()["style"].toString();
    const bool on = button.getToggleState();
    auto bounds = button.getLocalBounds().toFloat();
    if (style == "power")
    {
        const auto c = bounds.getCentre();
        const float r = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.5f;
        juce::Path p;
        p.addCentredArc(c.x, c.y, r, r, 0.0f, juce::MathConstants<float>::pi*0.22f, juce::MathConstants<float>::pi*1.78f, true);
        p.startNewSubPath(c.x, c.y-r-1.0f); p.lineTo(c.x, c.y-r*0.25f);
        g.setColour(on ? (over ? juce::Colours::white : text) : (over ? text : juce::Colour(0xff5f6062)));
        g.strokePath(p, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        return;
    }
    if (style == "filter")
    {
        auto box = bounds.reduced(1.0f);
        g.setColour(on ? blue : (over ? buttonHover : buttonFill)); g.fillRoundedRectangle(box, 3.0f);
        auto icon = box.reduced(box.getWidth()*0.24f, box.getHeight()*0.28f);
        juce::Path p;
        p.startNewSubPath(icon.getX(), icon.getBottom());
        p.quadraticTo(icon.getX()+icon.getWidth()*0.18f, icon.getY(), icon.getX()+icon.getWidth()*0.38f, icon.getY());
        p.lineTo(icon.getX()+icon.getWidth()*0.62f, icon.getY());
        p.quadraticTo(icon.getRight()-icon.getWidth()*0.18f, icon.getY(), icon.getRight(), icon.getBottom());
        g.setColour(on ? juce::Colours::white : text);
        g.strokePath(p, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        return;
    }
    // Flat rectangular push button: grey when off, blue when on.
    g.setColour(on ? (over ? blueLight : blue) : (down ? buttonFill.brighter(0.15f) : over ? buttonHover : buttonFill));
    g.fillRoundedRectangle(bounds.reduced(1.0f), 3.0f);
    g.setColour(on ? juce::Colours::white : text);
    g.setFont(font(bounds.getHeight() > 30 ? 16.0f : 14.0f));
    g.drawText(button.getButtonText(), bounds, juce::Justification::centred, false);
}
void StripLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height)).reduced(0.5f);
    g.setColour(box.isMouseOver(true) ? buttonHover.withAlpha(0.5f) : juce::Colours::transparentBlack);
    g.fillRect(bounds);
    g.setColour(outline); g.drawRect(bounds, 1.0f);
    const float cx = static_cast<float>(width) - 16.0f, cy = static_cast<float>(height) * 0.5f;
    juce::Path chevron; chevron.startNewSubPath(cx-6.0f, cy-3.0f); chevron.lineTo(cx, cy+3.0f); chevron.lineTo(cx+6.0f, cy-3.0f);
    g.setColour(text); g.strokePath(chevron, juce::PathStrokeType(1.3f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
}
void StripLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(4, 0, box.getWidth()-30, box.getHeight());
    label.setFont(getComboBoxFont(box));
}
void StripLookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height,
    bool vertical, int thumbStart, int thumbSize, bool over, bool down)
{
    auto thumbArea = vertical ? juce::Rectangle<int>(x, thumbStart, width, thumbSize) : juce::Rectangle<int>(thumbStart, y, thumbSize, height);
    g.setColour(juce::Colour(0xff262729)); g.fillRoundedRectangle(juce::Rectangle<int>(x, y, width, height).toFloat().reduced(0, 2), 2.0f);
    g.setColour(down ? juce::Colour(0xff8c8d8f) : over ? juce::Colour(0xff727375) : juce::Colour(0xff5a5b5d));
    g.fillRoundedRectangle(thumbArea.toFloat().reduced(1.0f, 1.0f), 3.0f);
}
void StripLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    g.fillAll(panel);
    g.setColour(outline.withAlpha(0.6f)); g.drawRect(0, 0, width, height, 1);
}
juce::PopupMenu::Options StripLookAndFeel::getOptionsForComboBoxPopupMenu(juce::ComboBox& box, juce::Label& label)
{
    const auto transform = box.getTransform();
    menuScale = transform.mat11 > 0.0f ? transform.mat11 : 1.0f;
    auto options = juce::LookAndFeel_V4::getOptionsForComboBoxPopupMenu(box, label);
    if (auto* editor = box.getTopLevelComponent())
        options = options.withParentComponent(editor);
    if (transform.mat00 > 0.0f)
        options = options.withMinimumWidth(juce::roundToInt(static_cast<float>(box.getWidth()) * transform.mat00))
                         .withStandardItemHeight(juce::roundToInt(static_cast<float>(label.getHeight()) * menuScale));
    return options;
}
void IconButton::paintButton(juce::Graphics& g, bool over, bool down)
{
    auto b = getLocalBounds().toFloat();
    if (icon == Icon::plus)
    {
        g.setColour(down ? buttonHover.brighter(0.1f) : over ? buttonHover : buttonFill);
        g.fillRoundedRectangle(b.reduced(1.0f), 3.0f);
    }
    const auto c = b.getCentre();
    const float r = std::min(b.getWidth(), b.getHeight()) * (icon == Icon::plus ? 0.2f : 0.3f);
    juce::Path p;
    if (icon == Icon::close) { p.startNewSubPath(c.x-r, c.y-r); p.lineTo(c.x+r, c.y+r); p.startNewSubPath(c.x+r, c.y-r); p.lineTo(c.x-r, c.y+r); }
    else { p.startNewSubPath(c.x-r, c.y); p.lineTo(c.x+r, c.y); p.startNewSubPath(c.x, c.y-r); p.lineTo(c.x, c.y+r); }
    g.setColour(over ? juce::Colours::white : (icon == Icon::plus ? StripTheme::text : juce::Colour(0xffa0a1a3)));
    g.strokePath(p, juce::PathStrokeType(1.4f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
}
