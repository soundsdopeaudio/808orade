#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"   // need concrete type here
#include "808Generator.h"
#include "WavExporter.h"

// class declaration uses PluginProcessor& in ctor
class ResynthesisWindow : public juce::DocumentWindow,
    private juce::Button::Listener,
    private juce::Slider::Listener
{
public:
    ResynthesisWindow(PluginProcessor& ownerProcessor);
    ~ResynthesisWindow() override;

    void open();
    void closeWindow();

private:
    PluginProcessor& owner;
    // ...
};
