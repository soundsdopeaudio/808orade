#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h" // use concrete plugin processor

class BatchWindow : public juce::DocumentWindow,
    private juce::Button::Listener
{
public:
    BatchWindow(PluginProcessor& ownerProcessor);   // <-- PluginProcessor
    ~BatchWindow() override;

    void open();
    void closeWindow();

private:
    // keep the rest same
    PluginProcessor& owner;
    // ...
};
