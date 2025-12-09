#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class DescriptorWindow : public juce::DocumentWindow,
    private juce::Button::Listener,
    private juce::TextEditor::Listener
{
public:
    explicit DescriptorWindow(PluginProcessor& ownerProcessor);
    ~DescriptorWindow() override;

    std::function<void()> onCloseCallback;
    void closeButtonPressed() override;

    // API
    std::vector<std::pair<juce::String, float>> getSelectedKeywords() const;
    void applyPrompt(const juce::String& text);
    void open();
    void closeWindow();

    // DocumentWindow override

    // Layout override
    void resized() override;

private:
    PluginProcessor& owner;

    // Content component (owned by DocumentWindow via setContentOwned)
    std::unique_ptr<juce::Component> content;

    // Left column categories
    juce::OwnedArray<juce::TextButton> categoryButtons;

    // Center: keyword grid (toggle buttons)
    juce::OwnedArray<juce::ToggleButton> keywordButtons;
    std::map<juce::String, juce::ToggleButton*> keywordButtonMap; // id -> pointer
    std::map<juce::String, float> keywordIntensity;               // id -> 0..1

    // Right column controls
    juce::TextEditor promptEditor;
    juce::TextButton applyPromptBtn{ "Use Prompt" };
    juce::TextButton savePresetBtn{ "Save Preset" };
    juce::TextButton resetBtn{ "Reset" };
    juce::TextButton generateBtn{ "Generate" };
    juce::TextButton randomizeBtn{ "Randomize" };
    juce::Label intensityLabel;
    juce::Slider intensityKnob; // global intensity
    juce::Component keywordSummaryBox;

    // helper
    void addKeyword(const juce::String& id, const juce::String& labelText);

    // listeners
    void buttonClicked(juce::Button* b) override;
    void textEditorReturnKeyPressed(juce::TextEditor& editor) override;
    void textEditorTextChanged(juce::TextEditor& editor) override {}

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DescriptorWindow)
};
