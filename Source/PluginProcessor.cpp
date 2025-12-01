#include "PluginProcessor.h"
#include "PluginEditor.h"

PluginProcessor::PluginProcessor()
{
}

PluginProcessor::~PluginProcessor()
{
}

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // nothing fancy to prepare for generator (generator will be given sampleRate via params)
}

void PluginProcessor::releaseResources()
{
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // simple plugin: support mono/stereo output
    if (layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo())
        return true;
    return false;
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // This plugin generates offline audio for export. During the normal audio callback
    // we will pass audio through (or silence) because playback preview is handled in the editor for now.
    juce::ignoreUnused (midiMessages);

    if (buffer.getNumChannels() > 0)
    {
        buffer.clear();
    }
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You can save presets/params here later. For now nothing.
    juce::ignoreUnused(destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ignoreUnused(data, sizeInBytes);
}

// generate and store result in generatedBuffer
bool PluginProcessor::generate808AndStore(const GeneratorParams& params)
{
    lastParams = params;
    // copy params & ensure sample rate set
    GeneratorParams p = params;
    if (p.sampleRate <= 0.0) p.sampleRate = 44100.0;

    auto buf = generator.renderToBuffer(p); // returns stereo buffer

    // store into generatedBuffer
    generatedBuffer.setSize(buf.getNumChannels(), buf.getNumSamples());
    generatedBuffer.makeCopyOf(buf);

    return (generatedBuffer.getNumSamples() > 0);
}
