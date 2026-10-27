#include "PluginEditor.h"

using namespace StripTheme;
using namespace StripTheme::Layout;

namespace
{
    #ifdef JucePlugin_VersionString
    const juce::String version(JucePlugin_VersionString);
    #else
    const juce::String version("dev");
    #endif
    const juce::String dot(juce::CharPointer_UTF8(" \xc2\xb7 "));
    const juce::String times(juce::CharPointer_UTF8("\xc3\x97"));
}

SpectralStripEditor::SpectralStripEditor(SpectralStripProcessor& p)
    : AudioProcessorEditor(p), audioProcessor(p), spectrum(p, look), strip(p, look)
{
    setLookAndFeel(&look);
    addAndMakeVisible(spectrum);
    addAndMakeVisible(strip);

    auto& state = audioProcessor.state;
    low.attach(state, "low", "Low", " Hz", 0, "Lower edge of the effect band.", look, juce::Slider::LinearVertical);
    high.attach(state, "high", "High", " Hz", 0, "Upper edge of the effect band.", look, juce::Slider::LinearVertical);
    mix.attach(state, "mix", "Dry/wet", "", 2, {}, look, juce::Slider::LinearVertical);
    output.attach(state, "output", "Output", " dB", 1, {}, look, juce::Slider::LinearVertical);
    for (auto* slider : { &low, &high, &mix, &output })
    {
        slider->label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(slider->slider);
        addAndMakeVisible(slider->label);
    }
    bandFilter.attach(state, "bandFilter", "Filter", "Filters the wet signal. Set Dry/wet to 1.00 for complete cutoff.", "filter");
    addAndMakeVisible(bandFilter.button);

    addModule.setTextWhenNothingSelected("+  Add module");
    addModule.setTooltip("Add an effect to the end of the strip.");
    addModule.addItemList(moduleMenuNames(), 1);
    addModule.onChange = [this]
    {
        if (const int type = addModule.getSelectedId())
        {
            addModule.setSelectedId(0, juce::dontSendNotification);
            strip.addModule(type);
        }
    };
    addAndMakeVisible(addModule);

    fftSizeLabel.setText("FFT", juce::dontSendNotification);
    fftSizeLabel.setFont(look.font(14.0f));
    fftSizeLabel.setJustificationType(juce::Justification::centredRight);
    fftSizeLabel.setColour(juce::Label::textColourId, dim);
    fftSizeChoice.addItemList({ "512", "1024", "2048", "4096", "8192" }, 1);
    fftSizeChoice.setJustificationType(juce::Justification::centred);
    fftSizeChoice.setTooltip("Larger FFT: finer frequency resolution, longer latency. Changing size clears Freeze and Delay memory.");
    fftSizeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, "fftSize", fftSizeChoice);
    addAndMakeVisible(fftSizeLabel);
    addAndMakeVisible(fftSizeChoice);

    setSize(static_cast<int>(Layout::canvasWidth), static_cast<int>(Layout::canvasHeight));
    setResizable(true, true);
    getConstrainer()->setFixedAspectRatio(static_cast<double>(Layout::canvasWidth / Layout::canvasHeight));
    setResizeLimits(780, 600, 1560, 1200);
    infoText = statusText();
    startTimerHz(30);
}

SpectralStripEditor::~SpectralStripEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

//==============================================================================
void SpectralStripEditor::resized()
{
    // Children are placed on the 1040 x 800 canvas, then all scaled by the same transform.
    spectrum.setBounds(scope.toNearestInt());
    strip.setBounds(stripX, stripY, stripW, moduleH + 15);
    const auto placeVertical = [](LabelledSlider& slider, int y, int h)
    {
        slider.slider.setBounds(sidebarX + 16, y, 32, h);
        slider.label.setBounds(sidebarX, y + h + 4, 64, 18);
    };
    placeVertical(low, 80, 118);
    placeVertical(high, 240, 118);
    bandFilter.button.setBounds(sidebarX + 16, 404, 32, 26);
    placeVertical(mix, 448, 118);
    placeVertical(output, 604, 108);
    addModule.setBounds(12, 756, 150, 28);
    fftSizeLabel.setBounds(852, 756, 44, 28);
    fftSizeChoice.setBounds(902, 756, 118, 28);

    const auto scale = juce::AffineTransform::scale(static_cast<float>(getWidth()) / Layout::canvasWidth,
                                                    static_cast<float>(getHeight()) / Layout::canvasHeight);
    // Only our own components: the editor's resize corner must stay unscaled in the window corner.
    const std::initializer_list<juce::Component*> scaled {
        &spectrum, &strip, &low.slider, &low.label, &high.slider, &high.label, &mix.slider, &mix.label,
        &output.slider, &output.label, &bandFilter.button, &addModule, &fftSizeLabel, &fftSizeChoice };
    for (auto* child : scaled)
        child->setTransform(scale);
}

void SpectralStripEditor::paint(juce::Graphics& g)
{
    g.fillAll(background);
    g.addTransform(juce::AffineTransform::scale(static_cast<float>(getWidth()) / Layout::canvasWidth,
                                                static_cast<float>(getHeight()) / Layout::canvasHeight));

    // Title: "spectral" bold + "strip" light, a blue divider and an italic subtitle.
    const auto bold = StripLookAndFeel::font(look.bold, 27.0f), light = StripLookAndFeel::font(look.light, 27.0f);
    const auto italic = StripLookAndFeel::font(look.lightItalic, 27.0f);
    const float w1 = juce::GlyphArrangement::getStringWidth(bold, "spectral"), w2 = juce::GlyphArrangement::getStringWidth(light, "strip");
    const float w3 = juce::GlyphArrangement::getStringWidth(italic, "modular");
    float x = Layout::canvasWidth * 0.5f - (w1 + w2 + 26.0f + w3) * 0.5f;
    g.setColour(text);
    g.setFont(bold);
    g.drawText("spectral", juce::Rectangle<float>(x, 14, w1 + 2, 40), juce::Justification::centredLeft, false);
    g.setFont(light);
    g.drawText("strip", juce::Rectangle<float>(x + w1, 14, w2 + 2, 40), juce::Justification::centredLeft, false);
    x += w1 + w2;
    g.setColour(blueLight);
    g.fillRect(juce::Rectangle<float>(x + 12.0f, 22.0f, 2.5f, 24.0f));
    g.setColour(text.withAlpha(0.85f));
    g.setFont(italic);
    g.drawText("modular", juce::Rectangle<float>(x + 26.0f, 14, w3 + 4, 40), juce::Justification::centredLeft, false);

    // Sidebar background with dividers between the band, filter and output groups.
    g.setColour(panel);
    g.fillRoundedRectangle(juce::Rectangle<float>(sidebarX, 64, 64, 680), 8.0f);
    g.setColour(grid);
    for (float y : { 226.0f, 432.0f }) g.drawHorizontalLine(static_cast<int>(y), sidebarX + 10, sidebarX + 54);

    // Info bar with an "i" icon.
    g.setColour(panel);
    g.fillRoundedRectangle(info, 4.0f);
    const auto icon = juce::Rectangle<float>(15, 15).withCentre({ info.getX() + 18, info.getCentreY() });
    g.setColour(dim);
    g.drawEllipse(icon, 1.1f);
    g.fillRect(juce::Rectangle<float>(1.4f, 5.0f).withCentre({ icon.getCentreX(), icon.getCentreY() + 1.5f }));
    g.fillEllipse(juce::Rectangle<float>(1.8f, 1.8f).withCentre({ icon.getCentreX(), icon.getCentreY() - 3.0f }));
    g.setFont(look.font(13.0f));
    g.setColour(infoIsStatus ? dim : text);
    g.drawText(infoText, info.withTrimmedLeft(36).withTrimmedRight(12), juce::Justification::centredLeft, true);
}

//==============================================================================
juce::String SpectralStripEditor::statusText() const
{
    const double latencyMs = 1000.0 * audioProcessor.getLatencySamples() / std::max(1.0, audioProcessor.getSampleRate());
    return "FFT " + juce::String(audioProcessor.activeFFTSize.load()) + dot + "overlap 4" + times
         + dot + "latency " + juce::String(latencyMs, 1) + " ms" + dot + "Spectral Strip v" + version;
}

// Info bar text for the component under the mouse (or the nearest parent that has one).
juce::String SpectralStripEditor::describe(juce::Component* c)
{
    for (; c != nullptr && c != this; c = c->getParentComponent())
    {
        if (c == &spectrum)
        {
            const auto mouse = juce::Desktop::getInstance().getMainMouseSource().getScreenPosition();
            return spectrum.describeAt(spectrum.getLocalPoint(nullptr, mouse));
        }
        if (auto description = strip.describe(c); description.isNotEmpty()) return description;
        for (auto* slider : { &low, &high, &mix, &output })
            if (c == &slider->slider) return slider->describe();
        if (c == &bandFilter.button) return bandFilter.describe();
        if (auto* client = dynamic_cast<juce::SettableTooltipClient*>(c); client != nullptr && client->getTooltip().isNotEmpty())
            return client->getTooltip();
    }
    return {};
}

void SpectralStripEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const float elapsed = lastTick > 0 ? static_cast<float>(std::min(0.1, now - lastTick)) : 0.0f;
    lastTick = now;
    spectrum.update();
    strip.updateArt(elapsed);

    auto* under = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse();
    auto description = (under != nullptr && isParentOf(under)) ? describe(under) : juce::String();
    const bool isStatus = description.isEmpty();
    if (isStatus) description = statusText();
    if (description != infoText || isStatus != infoIsStatus)
    {
        infoText = description;
        infoIsStatus = isStatus;
        const auto scale = juce::AffineTransform::scale(static_cast<float>(getWidth()) / Layout::canvasWidth,
                                                        static_cast<float>(getHeight()) / Layout::canvasHeight);
        repaint(info.transformedBy(scale).getSmallestIntegerContainer());
    }
}
