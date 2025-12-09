#include <JuceHeader.h>
#include "PluginProcessor.h"

// Minimal JUCEApplication that hosts the plugin editor in a window.
class StandaloneHostApplication  : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override       { return "808orade Standalone"; }
    const juce::String getApplicationVersion() override    { return "0.1"; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise (const juce::String&) override
    {
        processor = std::make_unique<PluginProcessor>();

        // create the plugin editor (AudioProcessorEditor*) from processor
        juce::AudioProcessorEditor* editor = processor->createEditor();
        if (editor == nullptr)
        {
            // fallback window with message
            mainWindow.reset (new juce::DocumentWindow ("808orade",
                                                        juce::Colours::black,
                                                        juce::DocumentWindow::allButtons));
            auto* l = new juce::Label();
            l->setText("Failed to create editor", juce::dontSendNotification);
            mainWindow->setContentOwned(l, true);
        }
        else
        {
            mainWindow.reset (new juce::DocumentWindow ("808orade",
                                                        juce::Colours::black,
                                                        juce::DocumentWindow::allButtons));
            mainWindow->setUsingNativeTitleBar (true);
            mainWindow->setContentOwned (editor, true); // DocumentWindow takes ownership
        }

        mainWindow->centreWithSize (1000, 700);
        mainWindow->setVisible (true);
    }

    void shutdown() override
    {
        // destroy window before processor/editor so editor is deleted first
        mainWindow = nullptr;
        processor = nullptr;
    }

    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted (const juce::String&) override {}

private:
    std::unique_ptr<PluginProcessor> processor;
    std::unique_ptr<juce::DocumentWindow> mainWindow;
};

START_JUCE_APPLICATION (StandaloneHostApplication)