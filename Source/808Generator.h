#pragma once
#include <JuceHeader.h>
#include <random>
#include <map>
#include <string>
#include <vector>

struct GeneratorParams
{
    int64_t seed = 0;
    double sampleRate = 44100.0;
    double lengthSeconds = 1.5;
    float tuneSemitones = 0.0f; // master tuning
    float masterGainDb = 0.0f;
    // keyword influences (0-1)
    float subAmount = 0.0f;
    float boomAmount = 0.0f;
    float shortness = 0.0f;
    float punch = 0.0f;
    float growl = 0.0f;
    float detune = 0.0f;
    float analog = 0.0f;
    float clean = 0.0f;
};

class GeneratorVoiceUtils
{
public:
    static inline float dBToGain(float db) { return std::pow(10.0f, db / 20.0f); }
    static inline float gainToDb(float g) { return 20.0f * std::log10(g); }
};

class Generator808
{
public:
    Generator808() = default;
    ~Generator808() = default;

    // Render method: fills a stereo buffer (mono sub summed into both channels appropriately)
    void render(const GeneratorParams& params, juce::AudioBuffer<float>& outBuffer);

    // Convenience: return wav data in a float buffer
    juce::AudioBuffer<float> renderToBuffer(const GeneratorParams& params);

private:
    std::mt19937_64 rng;
    std::uniform_real_distribution<double> uni{0.0, 1.0};

    double random01() { return uni(rng); }

    double midiNoteToFreq(double midi) { return 440.0 * std::pow(2.0, (midi - 69.0) / 12.0); }

    void fillOsc(double phaseInc, double& phase, float* dest, int numSamples);

    // core generation helpers
    void generateWaveform(const GeneratorParams& p, juce::AudioBuffer<float>& bufMono);
    void applyFilterAndSaturation(juce::AudioBuffer<float>& bufMono, const GeneratorParams& p);
    void applyStereoWidth(juce::AudioBuffer<float>& bufStereo, const GeneratorParams& p);
};
