#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "DescriptorWindow.h"
#include "BatchWindow.h"
#include <memory>

// Forward-declared previously defined WaveformComponent & RegeneratingSlider remain the same as before.
// (If you used the previous PluginEditor.h provided earlier, keep RegeneratingSlider & WaveformComponent definitions.)

//==============================================================================
// The main plugin editor
class PluginEditor  : public juce::AudioProcessorEditor,
                      private juce::Button::Listener,
                      private juce::Slider::Listener
{
public:
    PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // UI controls (kept from previous implementation)
    WaveformComponent waveform;
    juce::TextButton generateButton { "GENERATE 808" };
    juce::TextButton exportButton { "EXPORT" };
    juce::ToggleButton previewToggle { "Preview" };
    juce::Label noteLabel;
    juce::Label seedLabel;
    juce::TextButton copySeedButton { "Copy Seed" };
    RegeneratingSlider tuneSlider;
    juce::Label tuneLabel;

    // new: main menu button (hamburger)
    juce::TextButton mainMenuBtn { "≡" }; // small icon-like button

    PluginProcessor& processor;

    // windows
    std::unique_ptr<DescriptorWindow> descriptorWindow;
    std::unique_ptr<BatchWindow> batchWindow;

    // we keep a shared_ptr to the generated buffer so its memory remains valid
    std::shared_ptr<juce::AudioBuffer<float>> currentGeneratedBufferPtr;

    // lastParams display
    void updateWaveformFromProcessor();

    // regenerate helper (collects UI values -> params -> generate)
    void regenerateFromCurrentUI();

    // handlers
    void buttonClicked(juce::Button* b) override;
    void sliderValueChanged(juce::Slider* s) override;

    // helper to show main menu
    void showMainMenu();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
