#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"   // <<< concrete processor header

class DescriptorWindow : public juce::DocumentWindow,
    private juce::Button::Listener,
    private juce::TextEditor::Listener
{
public:
    // NOTE: now accepts PluginProcessor& so we can call plugin-specific API
    DescriptorWindow(PluginProcessor& ownerProcessor);
    ~DescriptorWindow() override;

    // ... rest unchanged
};
