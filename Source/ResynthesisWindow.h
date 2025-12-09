#pragma once
#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>   // make juce::dsp available
#include "PluginProcessor.h"   // need concrete type here
#include "808Generator.h"
#include "WavExporter.h"

// ResynthesisWindow
// - Upload-only resynthesis UI
// - Calls PluginProcessor::generate808AndStore(...) so the generated result is visible in main window
class ResynthesisWindow : public juce::DocumentWindow,
    private juce::Button::Listener,
    private juce::Slider::Listener
{
public:
    explicit ResynthesisWindow(PluginProcessor& ownerProcessor);
    ~ResynthesisWindow() override;


    void open();

    std::function<void()> onCloseCallback;

    void closeButtonPressed() override;

    void closeWindow();

private:
    PluginProcessor& owner;

    // UI controls
    juce::TextButton uploadBtn{ "Upload Audio" };
    juce::Label fileNameLabel;
    juce::TextButton analyzeBtn{ "Analyze" };

    // simple waveform display (left)
    class SimpleWaveDisplay : public juce::Component
    {
    public:
        SimpleWaveDisplay() = default;
        void setBuffer(const juce::AudioBuffer<float>* b) { buf = b; repaint(); }
        void paint(juce::Graphics& g) override;

    private:
        const juce::AudioBuffer<float>* buf = nullptr;
    };

    SimpleWaveDisplay originalWave;
    SimpleWaveDisplay resynthWave;

    juce::Label detectedNoteLabel;
    juce::Label pitchHzLabel;
    juce::Slider zoomSlider;

    // resynthesis knobs
    juce::Slider harmonicSmoothKnob;
    juce::Slider envelopeSmoothKnob;
    juce::Slider subWeightKnob;
    juce::Slider transientKnob;
    juce::Slider distortionKnob;
    juce::Slider noiseBlendKnob;
    juce::Slider glideKnob;
    juce::Slider accuracyKnob;

    juce::TextButton generateResynthBtn{ "Generate Resynth" };
    juce::TextButton playResynthBtn{ "Play Resynth" };
    juce::TextButton replaceMainBtn{ "Replace Main Window 808" };
    juce::TextButton exportWavBtn{ "Export Resynth (WAV)" };

    // Internals
    juce::AudioFormatManager formatManager;
    juce::AudioBuffer<float> loadedBuffer;
    double loadedSampleRate = 44100.0;
    bool hasLoaded = false;

    std::shared_ptr<juce::AudioBuffer<float>> generatedPtr;

    // FFT / analysis
    int fftOrder = 11;
    int fftSize = 1 << fftOrder;
    std::unique_ptr<juce::dsp::FFT> fft;

    std::vector<float> fftWindow;
    std::vector<float> fftData;

    // helpers
    void buildUI();
    void layoutChildren();

    void analyzeLoadedFile();
    double detectDominantFrequency(); // removed const - method mutates fftData
    void computeRMSAndEnvelope();

    // listeners
    void buttonClicked(juce::Button* b) override;
    void sliderValueChanged(juce::Slider* s) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResynthesisWindow)
};
