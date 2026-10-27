#include "SpectrumView.h"

using namespace StripTheme;

namespace
{
    const juce::String dash(juce::CharPointer_UTF8("\xe2\x80\x94"));
    float dbToY(float db, juce::Rectangle<float> plot)
    {
        return plot.getBottom() - plot.getHeight() * juce::jlimit(0.0f, 1.0f, (db + 90.0f) / 90.0f);
    }
}

SpectrumView::SpectrumView(SpectralStripProcessor& p, const StripLookAndFeel& lookAndFeel) : processor(p), look(lookAndFeel)
{
    display.fill(-90);
}

// 20 Hz ... 20 kHz across the width, logarithmic.
float SpectrumView::xForFrequency(float frequency) const
{
    return static_cast<float>(getWidth()) * std::log(frequency / 20.0f) / std::log(1000.0f);
}
float SpectrumView::frequencyForX(float x) const
{
    return 20.0f * std::pow(1000.0f, juce::jlimit(0.0f, 1.0f, x / static_cast<float>(getWidth())));
}

juce::RangedAudioParameter* SpectrumView::edgeParameter(int edge) const
{
    return processor.state.getParameter(edge == 1 ? "low" : "high");
}

void SpectrumView::update()
{
    bool moved = false;
    for (size_t i = 0; i < display.size(); ++i)
    {
        auto& shown = display[i];
        const float next = processor.engine.spectrum[i].load(std::memory_order_relaxed), previous = shown;
        shown += 0.35f * (next - shown);
        moved = moved || std::abs(shown - previous) > 0.01f;
    }
    const float lo = processor.state.getRawParameterValue("low")->load(), hi = processor.state.getRawParameterValue("high")->load();
    moved = moved || !juce::exactlyEqual(lo, shownLow) || !juce::exactlyEqual(hi, shownHigh);
    shownLow = lo;
    shownHigh = hi;
    if (moved) repaint();
}

void SpectrumView::paint(juce::Graphics& g)
{
    const auto plot = getLocalBounds().toFloat();
    juce::Path clip;
    clip.addRoundedRectangle(plot, 8.0f);
    g.setColour(panel);
    g.fillPath(clip);
    g.reduceClipRegion(clip);

    // Grid: every 10 dB, every 1-9 multiple of each decade, stronger at 100 Hz, 1 kHz, 10 kHz.
    g.setColour(grid);
    for (float db = -10; db > -90; db -= 10) g.drawHorizontalLine(juce::roundToInt(dbToY(db, plot)), plot.getX(), plot.getRight());
    for (float decade : { 10.0f, 100.0f, 1000.0f, 10000.0f })
        for (int m = 2; m <= 9; ++m)
        {
            const float f = decade * static_cast<float>(m);
            if (f >= 20.0f && f <= 20000.0f) g.drawVerticalLine(juce::roundToInt(xForFrequency(f)), plot.getY(), plot.getBottom());
        }
    g.setColour(gridMajor);
    for (float f : { 100.0f, 1000.0f, 10000.0f }) g.drawVerticalLine(juce::roundToInt(xForFrequency(f)), plot.getY(), plot.getBottom());

    // Spectrum: a grey gradient fill under a light line.
    juce::Path fill, line;
    fill.startNewSubPath(plot.getX(), plot.getBottom());
    for (size_t i = 0; i < display.size(); ++i)
    {
        const float x = plot.getX() + plot.getWidth() * static_cast<float>(i) / static_cast<float>(display.size() - 1);
        const float y = dbToY(display[i], plot);
        fill.lineTo(x, y);
        if (i == 0) line.startNewSubPath(x, y); else line.lineTo(x, y);
    }
    fill.lineTo(plot.getRight(), plot.getBottom());
    fill.closeSubPath();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffa4a5a7).withAlpha(0.9f), 0, plot.getY(),
                                           juce::Colour(0xff4a4b4d).withAlpha(0.85f), 0, plot.getBottom(), false));
    g.fillPath(fill);
    g.setColour(juce::Colour(0xffe2e2e2));
    g.strokePath(line, juce::PathStrokeType(1.2f));

    // Effect band: dim outside it, blue edges with draggable dots.
    float lo = shownLow, hi = shownHigh;
    if (lo > hi) std::swap(lo, hi);
    const float xl = xForFrequency(lo), xh = xForFrequency(hi);
    g.setColour(background.withAlpha(0.55f));
    g.fillRect(juce::Rectangle<float>(plot.getX(), plot.getY(), xl - plot.getX(), plot.getHeight()));
    g.fillRect(juce::Rectangle<float>(xh, plot.getY(), plot.getRight() - xh, plot.getHeight()));
    for (float edge : { xl, xh })
    {
        g.setColour(blueLight.withAlpha(0.8f));
        g.drawLine(edge, plot.getY(), edge, plot.getBottom(), 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.fillEllipse(juce::Rectangle<float>(20, 20).withCentre({ edge, plot.getY() + 16 }));
        g.setColour(juce::Colours::white);
        g.fillEllipse(juce::Rectangle<float>(9, 9).withCentre({ edge, plot.getY() + 16 }));
    }

    g.setFont(look.font(11.5f));
    g.setColour(text);
    for (float frequency : { 20.0f, 100.0f, 1000.0f, 10000.0f, 20000.0f })
    {
        const auto label = frequency < 1000 ? juce::String(frequency, 0) + " Hz" : juce::String(frequency / 1000, 0) + " kHz";
        const float x = juce::jlimit(plot.getX() + 6.0f, plot.getRight() - 66.0f, xForFrequency(frequency) - 30.0f);
        g.drawText(label, juce::Rectangle<float>(x, plot.getBottom() - 20, 60, 16), juce::Justification::centred, false);
    }
    for (float db : { -20.0f, -40.0f, -60.0f })
        g.drawText(juce::String(static_cast<int>(db)) + " dB", juce::Rectangle<float>(plot.getRight() - 58, dbToY(db, plot) - 15, 50, 14),
                   juce::Justification::centredRight, false);
}

int SpectrumView::bandEdgeAt(juce::Point<float> p) const
{
    if (!getLocalBounds().toFloat().expanded(4.0f).contains(p)) return 0;
    const float toLow = std::abs(p.x - xForFrequency(processor.state.getRawParameterValue("low")->load()));
    const float toHigh = std::abs(p.x - xForFrequency(processor.state.getRawParameterValue("high")->load()));
    if (std::min(toLow, toHigh) > 8.0f) return 0;
    return toLow <= toHigh ? 1 : 2;
}

juce::String SpectrumView::describeAt(juce::Point<float> p) const
{
    if (const int edge = draggedEdge ? draggedEdge : bandEdgeAt(p))
    {
        const auto* parameter = edgeParameter(edge);
        return (edge == 1 ? "Low   " : "High   ") + juce::String(parameter->convertFrom0to1(parameter->getValue()), 0)
             + " Hz   " + dash + "   drag to move the edge of the effect band";
    }
    const float f = frequencyForX(p.x);
    return "Analyser   " + (f < 1000.0f ? juce::String(f, 0) + " Hz" : juce::String(f / 1000.0f, 2) + " kHz")
         + "   " + dash + "   wet signal, left channel, before mix and output";
}

void SpectrumView::mouseMove(const juce::MouseEvent& e)
{
    setMouseCursor(bandEdgeAt(e.position) ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
}

void SpectrumView::mouseDown(const juce::MouseEvent& e)
{
    draggedEdge = bandEdgeAt(e.position);
    if (draggedEdge) edgeParameter(draggedEdge)->beginChangeGesture();
}

void SpectrumView::mouseDrag(const juce::MouseEvent& e)
{
    if (!draggedEdge) return;
    auto* parameter = edgeParameter(draggedEdge);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(frequencyForX(e.position.x)));
}

void SpectrumView::mouseUp(const juce::MouseEvent&)
{
    if (draggedEdge) edgeParameter(draggedEdge)->endChangeGesture();
    draggedEdge = 0;
}
