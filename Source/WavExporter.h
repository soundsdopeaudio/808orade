#pragma once
#include <JuceHeader.h>

class WavExporter
{
public:
    static bool saveBufferToWav(const juce::AudioBuffer<float>& buffer,
                                double sampleRate,
                                const juce::File& file,
                                int bitsPerSample = 24);
};
