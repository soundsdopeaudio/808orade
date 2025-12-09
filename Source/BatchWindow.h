#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h" // need concrete type

/*
 BatchWindow
 - UI for batch generation + export
 - Options: count (25,50,100), use descriptors (ask DescriptorWindow for selections), naming scheme, destination folder
 - Public API: open(), closeWindow()
*/

class BatchWindow : public juce::DocumentWindow,
    private juce::Button::Listener
{
public:
    BatchWindow(PluginProcessor& ownerProcessor);
    ~BatchWindow() override;

    std::function<void()> onCloseCallback;

    void closeButtonPressed() override
    {
        setVisible(false);
        if (onCloseCallback)
            onCloseCallback();
    }

    void open();
    void closeWindow();

private:
    void buildUI();
    void buttonClicked(juce::Button* b) override;

    // Use concrete PluginProcessor reference (remove duplicate owner declarations)
    PluginProcessor& owner;

    // UI
    juce::ToggleButton useDescriptorToggle{ "Use Descriptors" };
    juce::ComboBox countCombo; // 25/50/100
    juce::TextButton chooseFolderBtn{ "Choose Folder" };
    juce::Label folderLabel;
    juce::TextEditor prefixEditor;
    juce::TextButton generateBatchBtn{ "Generate Batch" };
    juce::TextButton exportAllBtn{ "Export All" };

    // last chosen folder
    juce::File destFolder;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BatchWindow)
};

