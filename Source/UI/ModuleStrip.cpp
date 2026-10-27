#include "ModuleStrip.h"
#include "ModuleArt.h"

using namespace StripTheme;
using namespace StripTheme::Layout;

namespace
{
    // Top of the controls block of a module, in strip coordinates: sliders, then buttons,
    // stacked from the bottom edge up. The illustration fills the space above.
    int controlsTop(const ModuleSpec& spec)
    {
        const int rows = static_cast<int>(spec.sliders.size());
        const int buttons = spec.buttons.empty() ? 0 : spec.buttonHeight + (rows ? 12 : 0);
        return moduleH - 14 - rows * rowH - buttons;
    }

    // Draws a bold and a light string side by side, centred on `area`.
    void drawTitle(juce::Graphics& g, const StripLookAndFeel& look, const juce::String& title, juce::Rectangle<float> area)
    {
        g.setColour(text);
        g.setFont(StripLookAndFeel::font(look.light, 22.0f));
        g.drawText(title, area, juce::Justification::centred, false);
    }
}

//==============================================================================
ModuleStrip::ModuleStrip(SpectralStripProcessor& p, StripLookAndFeel& lookAndFeel) : processor(p), look(lookAndFeel)
{
    addAndMakeVisible(view);
    view.addAndMakeVisible(content);
    content.setInterceptsMouseClicks(false, true);
    addAndMakeVisible(scrollBar);
    scrollBar.setAutoHide(false);
    scrollBar.addListener(this);

    for (int slot = 0; slot < SpectralEngine::maxModules; ++slot)
        for (const auto& spec : moduleSpecs())
        {
            const auto idFor = [slot](const char* id) { return SpectralStripProcessor::moduleParamID(slot, id); };
            for (const auto& s : spec.sliders)
            {
                auto slider = std::make_unique<LabelledSlider>();
                slider->attach(processor.state, idFor(s.id), s.label, s.unit, s.decimals, s.help, look);
                // The wheel scrolls the strip instead of changing values.
                slider->slider.setScrollWheelEnabled(false);
                if (juce::String(s.id) == "toneTilt") slider->slider.setDoubleClickReturnValue(true, 0);
                content.addChildComponent(slider->slider);
                content.addChildComponent(slider->label);
                sliders[idFor(s.id)] = std::move(slider);
            }
            const auto addToggle = [&](const char* id, const juce::String& label, const juce::String& help, const char* style)
            {
                auto toggle = std::make_unique<AttachedToggle>();
                toggle->attach(processor.state, idFor(id), label, help, style);
                content.addChildComponent(toggle->button);
                toggles[idFor(id)] = std::move(toggle);
            };
            if (spec.powerId != nullptr) addToggle(spec.powerId, "Enabled", {}, "power");
            for (const auto& b : spec.buttons) addToggle(b.id, b.label, b.help, "");
        }

    for (int i = 0; i < SpectralEngine::maxModules; ++i)
    {
        auto& header = headers[static_cast<size_t>(i)];
        header.setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        header.onDown = [this](const juce::MouseEvent& e) { beginDrag(e); };
        header.onDrag = [this](const juce::MouseEvent& e) { continueDrag(e); };
        header.onUp = [this](const juce::MouseEvent& e) { endDrag(e); };
        content.addChildComponent(header);
        auto& remove = removeButtons[static_cast<size_t>(i)];
        remove.setTooltip("Remove module");
        remove.onClick = [this, i]
        {
            auto chain = processor.getChain();
            chain[static_cast<size_t>(i)] = 0;
            processor.setChain(chain);
            refresh();
        };
        content.addChildComponent(remove);
    }
    addSlot.setTooltip("Add an effect to the end of the strip.");
    addSlot.onClick = [this] { showAddMenu(); };
    content.addChildComponent(addSlot);

    vblank = std::make_unique<juce::VBlankAttachment>(this, [this]
    {
        lastVBlank = juce::Time::getMillisecondCounterHiRes() * 0.001;
        animate();
    });
    refresh();
}

ModuleStrip::~ModuleStrip() { scrollBar.removeListener(this); }

int ModuleStrip::moduleCount() const
{
    return static_cast<int>(std::count_if(shownChain.begin(), shownChain.end(), [](int entry) { return entry != 0; }));
}

// Columns include the dashed "+" slot after the last module while there is room.
float ModuleStrip::contentWidth() const
{
    const int count = moduleCount();
    const int columns = count + (count < SpectralEngine::maxModules ? 1 : 0);
    return static_cast<float>(columns * modulePitch - (modulePitch - moduleW));
}

float ModuleStrip::maxScroll() const { return std::max(0.0f, contentWidth() - static_cast<float>(stripW)); }

void ModuleStrip::resized()
{
    view.setBounds(0, 0, stripW, moduleH);
    scrollBar.setBounds(0, moduleH + 6, stripW, 9);
}

//==============================================================================
void ModuleStrip::refresh()
{
    shownChain = processor.getChain();
    content.setBounds(0, 0, std::max(stripW, juce::roundToInt(contentWidth())), moduleH);
    scrollTarget = juce::jlimit(0.0f, maxScroll(), scrollTarget);
    scroll = juce::jlimit(0.0f, maxScroll(), scroll);
    applyScroll();

    for (auto& [id, s] : sliders) s->setVisible(false);
    for (auto& [id, t] : toggles) t->button.setVisible(false);
    const int count = moduleCount();
    for (int i = 0; i < SpectralEngine::maxModules; ++i)
    {
        const int entry = shownChain[static_cast<size_t>(i)];
        headers[static_cast<size_t>(i)].setVisible(entry != 0);
        removeButtons[static_cast<size_t>(i)].setVisible(entry != 0);
        if (entry) layoutModule(i, SpectralEngine::entryType(entry), SpectralEngine::entrySlot(entry));
    }
    addSlot.setVisible(count < SpectralEngine::maxModules);
    addSlot.setBounds(count * modulePitch + moduleW / 2 - 15, moduleH / 2 - 15, 30, 30);
    content.repaint();
}

void ModuleStrip::layoutModule(int column, int type, int slot)
{
    const auto* spec = findModuleSpec(type);
    if (spec == nullptr) return;
    const int x = column * modulePitch, left = x + 12, innerWidth = moduleW - 24;
    headers[static_cast<size_t>(column)].setBounds(x + 52, 0, moduleW - 104, headerH);
    removeButtons[static_cast<size_t>(column)].setBounds(x + 6, 10, 20, 20);
    const auto idFor = [slot](const char* id) { return SpectralStripProcessor::moduleParamID(slot, id); };

    if (spec->powerId != nullptr)
    {
        auto& power = toggles.at(idFor(spec->powerId))->button;
        power.setBounds(x + moduleW - 28, 10, 20, 20);
        power.setVisible(true);
    }
    int y = controlsTop(*spec);
    for (const auto& s : spec->sliders)
    {
        auto& row = *sliders.at(idFor(s.id));
        row.label.setBounds(left, y, 78, rowH);
        row.slider.setBounds(left + 76, y, innerWidth - 76, rowH);
        row.setVisible(true);
        y += rowH;
    }
    const int buttons = static_cast<int>(spec->buttons.size());
    if (buttons > 0)
    {
        const int buttonWidth = (innerWidth - 6 * (buttons - 1)) / buttons;
        const int buttonY = moduleH - 14 - spec->buttonHeight;
        for (int b = 0; b < buttons; ++b)
        {
            auto& button = toggles.at(idFor(spec->buttons[static_cast<size_t>(b)].id))->button;
            button.setBounds(left + b * (buttonWidth + 6), buttonY, buttonWidth, spec->buttonHeight);
            button.setVisible(true);
        }
    }
}

juce::Rectangle<float> ModuleStrip::artBounds(int column, int type) const
{
    const auto* spec = findModuleSpec(type);
    const int top = headerH + 4, bottom = spec != nullptr ? controlsTop(*spec) - 14 : moduleH - 14;
    return { static_cast<float>(column * modulePitch + 12), static_cast<float>(top), static_cast<float>(moduleW - 24), static_cast<float>(bottom - top) };
}

//==============================================================================
void ModuleStrip::paintContent(juce::Graphics& g)
{
    const int count = moduleCount();
    const auto clip = g.getClipBounds();
    for (int i = 0; i < SpectralEngine::maxModules; ++i)
    {
        const int entry = shownChain[static_cast<size_t>(i)], type = SpectralEngine::entryType(entry), slot = SpectralEngine::entrySlot(entry);
        const auto area = juce::Rectangle<int>(i * modulePitch, 0, moduleW, moduleH).toFloat();
        if (!clip.intersects(area.getSmallestIntegerContainer())) continue;
        const auto* spec = entry ? findModuleSpec(type) : nullptr;
        if (spec == nullptr)
        {
            if (i == count && count < SpectralEngine::maxModules)
            {
                juce::Path outlinePath, dashed;
                outlinePath.addRoundedRectangle(area.reduced(0.5f), 8.0f);
                const float dashes[] = { 2.0f, 3.0f };
                juce::PathStrokeType(1.0f).createDashedStroke(dashed, outlinePath, dashes, 2);
                g.setColour(juce::Colour(0xff6f7072));
                g.fillPath(dashed);
                g.setColour(dim);
                g.setFont(look.font(13.0f));
                g.drawText("Add a module", area.withTrimmedTop(area.getHeight() * 0.5f + 22).withHeight(20), juce::Justification::centred, false);
            }
        }
        else
        {
            g.setColour(panel);
            g.fillRoundedRectangle(area, 8.0f);
            drawTitle(g, look, spec->title, { area.getX(), 6.0f, area.getWidth(), 30.0f });
            // Position in the chain, next to the remove button.
            g.setFont(look.font(11.5f));
            g.setColour(dim);
            g.drawText(juce::String(i + 1), juce::Rectangle<float>(area.getX() + 30, 13.0f, 20, 16), juce::Justification::centredLeft, false);

            const auto artArea = artBounds(i, type);
            g.setColour(tile);
            g.fillRoundedRectangle(artArea, 4.0f);
            juce::Graphics::ScopedSaveState save(g);
            g.reduceClipRegion(artArea.toNearestInt());
            ModuleArtValues values;
            values.raw = [this, slot](const char* id)
            {
                auto* v = processor.state.getRawParameterValue(SpectralStripProcessor::moduleParamID(slot, id));
                return v != nullptr ? v->load() : 0.0f;
            };
            values.normalised = [this, slot](const char* id)
            {
                auto* parameter = processor.state.getParameter(SpectralStripProcessor::moduleParamID(slot, id));
                return parameter != nullptr ? parameter->getValue() : 0.0f;
            };
            values.freezePhase = freezePhase[static_cast<size_t>(slot)];
            values.seconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
            drawModuleArt(g, type, artArea.reduced(14.0f, 12.0f), values);
        }
        if (draggedEntry && i == dropColumn)
        {
            g.setColour(blueLight);
            g.drawRoundedRectangle(area.reduced(0.75f), 8.0f, 1.5f);
        }
    }
}

void ModuleStrip::updateArt(float elapsedSeconds)
{
    if (shownChain != processor.getChain()) refresh();
    const int firstColumn = static_cast<int>(scroll / modulePitch);
    const int lastColumn = static_cast<int>((scroll + static_cast<float>(stripW)) / modulePitch);
    for (int i = std::max(0, firstColumn); i <= std::min(SpectralEngine::maxModules - 1, lastColumn); ++i)
    {
        const int entry = shownChain[static_cast<size_t>(i)], type = SpectralEngine::entryType(entry), slot = SpectralEngine::entrySlot(entry);
        if (!entry || slot < 0) continue;
        ModuleArtValues values;
        values.raw = [this, slot](const char* id) { return processor.state.getRawParameterValue(SpectralStripProcessor::moduleParamID(slot, id))->load(); };
        // A cheap fingerprint of the parameters the drawing depends on.
        double signature = type;
        for (const char* id : artParameters(type))
            signature = signature * 1.618 + static_cast<double>(processor.state.getParameter(SpectralStripProcessor::moduleParamID(slot, id))->getValue());
        const bool animated = artIsAnimated(type, values);
        if (animated && type == SpectralEngine::ModuleType::freeze) freezePhase[static_cast<size_t>(slot)] += 1.2f * elapsedSeconds;
        auto& shown = artSignature[static_cast<size_t>(i)];
        if (animated || !juce::exactlyEqual(signature, shown))
            content.repaint(artBounds(i, type).getSmallestIntegerContainer().expanded(1));
        shown = signature;
    }
    // Keep scrolling at timer rate if the display link is not running (e.g. a hidden window).
    if (juce::Time::getMillisecondCounterHiRes() * 0.001 - lastVBlank > 0.1) animate();
}

//==============================================================================
void ModuleStrip::addModule(int type)
{
    processor.addInstance(type);
    refresh();
    scrollTarget = maxScroll();
}

// The "+" of the dashed slot opens the list of effects right below itself.
void ModuleStrip::showAddMenu()
{
    juce::PopupMenu menu;
    const bool room = moduleCount() < SpectralEngine::maxModules;
    for (int type = 1; type <= SpectralEngine::ModuleType::count; ++type)
        menu.addItem(type, moduleMenuNames()[type - 1], room);
    const float scale = getTransform().mat00 > 0.0f ? getTransform().mat00 : 1.0f;
    look.menuScale = scale;
    menu.setLookAndFeel(&look);
    auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>();
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&addSlot).withParentComponent(editor)
                           .withMinimumWidth(juce::roundToInt(150.0f * scale)).withStandardItemHeight(juce::roundToInt(26.0f * scale))
                           .withPreferredPopupDirection(juce::PopupMenu::Options::PopupDirection::downwards),
                       [safe = juce::Component::SafePointer<ModuleStrip>(this)](int type)
                       {
                           if (safe != nullptr && type > 0) safe->addModule(type);
                       });
}

juce::String ModuleStrip::describe(juce::Component* c) const
{
    for (const auto& [id, s] : sliders)
        if (c == &s->slider) return s->describe();
    for (const auto& [id, t] : toggles)
        if (c == &t->button) return t->describe();
    for (const auto& header : headers)
        if (c == &header) return "Drag to reorder the strip";
    return {};
}

//==============================================================================
// Reordering: drag a header; the drop column is outlined, the chain changes on release.
void ModuleStrip::beginDrag(const juce::MouseEvent& e)
{
    dragX = e.getEventRelativeTo(this).position.x;
    dropColumn = juce::jlimit(0, SpectralEngine::maxModules - 1, static_cast<int>((dragX + scroll) / modulePitch));
    draggedEntry = shownChain[static_cast<size_t>(dropColumn)];
}

void ModuleStrip::continueDrag(const juce::MouseEvent& e)
{
    if (!draggedEntry) return;
    dragX = e.getEventRelativeTo(this).position.x;
    updateDropColumn();
}

void ModuleStrip::updateDropColumn()
{
    const int column = juce::jlimit(0, std::max(0, moduleCount() - 1), static_cast<int>((dragX + scroll) / modulePitch));
    if (column != dropColumn) { dropColumn = column; content.repaint(); }
}

void ModuleStrip::endDrag(const juce::MouseEvent& e)
{
    if (draggedEntry && e.mouseWasDraggedSinceMouseDown())
    {
        auto chain = processor.getChain();
        const auto source = std::find(chain.begin(), chain.end(), draggedEntry);
        if (source != chain.end())
        {
            // Move the entry to the drop column, shifting the ones in between.
            const auto target = chain.begin() + dropColumn;
            if (source < target) std::rotate(source, source + 1, target + 1);
            else std::rotate(target, source, source + 1);
            processor.setChain(chain);
        }
    }
    draggedEntry = 0;
    dropColumn = -1;
    dragX = -1;
    refresh();
}

//==============================================================================
void ModuleStrip::applyScroll()
{
    content.setTransform(juce::AffineTransform::translation(-scroll, 0.0f));
    scrollBar.setRangeLimits(0.0, std::max(static_cast<double>(stripW), static_cast<double>(contentWidth())), juce::dontSendNotification);
    scrollBar.setCurrentRange(scroll, stripW, juce::dontSendNotification);
    scrollBar.setVisible(contentWidth() > static_cast<float>(stripW) + 0.5f);
}

void ModuleStrip::scrollBarMoved(juce::ScrollBar*, double start)
{
    scroll = scrollTarget = juce::jlimit(0.0f, maxScroll(), static_cast<float>(start));
    applyScroll();
}

// Vertical wheels scroll horizontally too.
void ModuleStrip::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const float delta = std::abs(wheel.deltaX) > std::abs(wheel.deltaY) ? wheel.deltaX : wheel.deltaY;
    if (juce::exactlyEqual(delta, 0.0f) || maxScroll() <= 0.0f) return;
    if (wheel.isSmooth)
    {
        // JUCE reports precise (trackpad) deltas as points / 512; convert to strip pixels.
        const float scale = getTransform().mat00 > 0.0f ? getTransform().mat00 : 1.0f;
        scroll = scrollTarget = juce::jlimit(0.0f, maxScroll(), scroll - delta * 512.0f / scale);
        applyScroll();
    }
    else scrollTarget = juce::jlimit(0.0f, maxScroll(), scrollTarget - delta * 2400.0f);
}

void ModuleStrip::animate()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const float dt = lastFrame > 0 ? static_cast<float>(juce::jlimit(0.0, 0.05, now - lastFrame)) : 1.0f / 60.0f;
    lastFrame = now;
    if (draggedEntry && dragX >= 0)
    {
        // Auto-scroll while dragging near an edge, faster the closer to it.
        constexpr float zone = 40.0f;
        const float left = dragX, right = static_cast<float>(stripW) - dragX;
        if (left < zone || right < zone)
        {
            const float speed = left < zone ? -900.0f * (1.0f - std::max(0.0f, left) / zone)
                                            : 900.0f * (1.0f - std::max(0.0f, right) / zone);
            scroll = scrollTarget = juce::jlimit(0.0f, maxScroll(), scroll + speed * dt);
            applyScroll();
            updateDropColumn();
        }
    }
    if (std::abs(scrollTarget - scroll) > 0.05f)
    {
        scroll += (scrollTarget - scroll) * (1.0f - std::exp(-dt * 14.0f));
        if (std::abs(scrollTarget - scroll) <= 0.05f) scroll = scrollTarget;
        applyScroll();
    }
}
