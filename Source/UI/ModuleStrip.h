#pragma once
#include "../PluginProcessor.h"
#include "Controls.h"
#include "ModuleSpecs.h"

// The row of effect modules. Each module is a column with a header (remove, title, power),
// an illustration and its controls; after the last module there is a dashed "+" slot.
//
// Modules live in a wide content component that slides inside a clipping view, so the strip
// scrolls continuously: trackpads move it 1:1 (macOS supplies the momentum), mouse wheels
// animate towards a target, and dragging a header near an edge scrolls automatically.
//
// Drawn in the editor's 1040 x 800 coordinates; the editor scales this whole component.
class ModuleStrip : public juce::Component, private juce::ScrollBar::Listener
{
public:
    ModuleStrip(SpectralStripProcessor&, StripLookAndFeel&);
    ~ModuleStrip() override;

    // Lays the modules out again after the chain changed.
    void refresh();
    // UI timer: repaints illustrations whose parameters changed, or that animate.
    void updateArt(float elapsedSeconds);
    // Adds an effect at the end of the chain and scrolls to it.
    void addModule(int type);
    // Info bar text for a control of the strip, or an empty string.
    juce::String describe(juce::Component*) const;

    void resized() override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    // Header area of a module: forwards drags to the strip, which reorders the chain.
    struct Header : juce::Component
    {
        std::function<void(const juce::MouseEvent&)> onDown, onDrag, onUp;
        void mouseDown(const juce::MouseEvent& e) override { onDown(e); }
        void mouseDrag(const juce::MouseEvent& e) override { onDrag(e); }
        void mouseUp(const juce::MouseEvent& e) override   { onUp(e); }
    };
    struct Content : juce::Component
    {
        explicit Content(ModuleStrip& s) : strip(s) {}
        void paint(juce::Graphics& g) override { strip.paintContent(g); }
        ModuleStrip& strip;
    };

    void paintContent(juce::Graphics&);
    void layoutModule(int column, int type, int slot);
    juce::Rectangle<float> artBounds(int column, int type) const;
    void showAddMenu();

    void beginDrag(const juce::MouseEvent&);
    void continueDrag(const juce::MouseEvent&);
    void endDrag(const juce::MouseEvent&);
    void updateDropColumn();

    int moduleCount() const;
    float contentWidth() const;
    float maxScroll() const;
    void applyScroll();
    void animate();
    void scrollBarMoved(juce::ScrollBar*, double) override;

    SpectralStripProcessor& processor;
    StripLookAndFeel& look;
    SpectralStripProcessor::Chain shownChain {};

    juce::Component view;           // clips the content to the visible strip
    Content content { *this };
    juce::ScrollBar scrollBar { false };
    std::array<Header, SpectralEngine::maxModules> headers;          // by chain position
    std::array<IconButton, SpectralEngine::maxModules> removeButtons;
    IconButton addSlot { IconButton::Icon::plus };

    // Controls of every slot and effect type, by parameter ID.
    std::map<juce::String, std::unique_ptr<LabelledSlider>> sliders;
    std::map<juce::String, std::unique_ptr<AttachedToggle>> toggles;

    float scroll = 0, scrollTarget = 0;         // in strip pixels
    double lastFrame = 0, lastVBlank = 0;
    std::unique_ptr<juce::VBlankAttachment> vblank;

    int draggedEntry = 0, dropColumn = -1;
    float dragX = -1;

    std::array<double, SpectralEngine::maxModules> artSignature {};  // by chain position
    std::array<float, SpectralEngine::maxModules> freezePhase {};    // by slot
};
