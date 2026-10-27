// Opens the editor in a window, without a host, with a few modules in the strip.
#include "../Source/PluginProcessor.h"

class PreviewApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Spectral Strip Preview"; }
    const juce::String getApplicationVersion() override { return "preview"; }

    void initialise(const juce::String&) override
    {
        processor = std::make_unique<SpectralStripProcessor>();
        processor->setChain({ 0x66, 0x11, 0x33, 0x55 });
        processor->setRateAndBufferSizeDetails(48000, 512);
        processor->prepareToPlay(48000, 512);
        window = std::make_unique<juce::DocumentWindow>("Spectral Strip - Editor Preview", juce::Colour(0xff1d1e1f), juce::DocumentWindow::allButtons);
        window->setUsingNativeTitleBar(true);
        window->setContentOwned(processor->createEditor(), true);
        window->setResizable(true, false);
        window->centreWithSize(window->getWidth(), window->getHeight());
        window->setVisible(true);
    }

    void shutdown() override
    {
        window.reset();
        processor.reset();
    }

    void systemRequestedQuit() override { quit(); }

private:
    std::unique_ptr<SpectralStripProcessor> processor;
    std::unique_ptr<juce::DocumentWindow> window;
};

START_JUCE_APPLICATION(PreviewApp)
