#include "ResynthesisWindow.h"
#include <cmath>
#include <algorithm>

using namespace juce;

//
// SimpleWaveDisplay paint implementation
//
void ResynthesisWindow::SimpleWaveDisplay::paint(Graphics& g)
{
    g.fillAll(Colour(0xFF1B1F23));

    if (!buf || buf->getNumSamples() == 0)
    {
        g.setColour(Colour(0xFF2B2F33));
        g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(6.0f), 6.0f);
        return;
    }

    auto bounds = getLocalBounds().reduced(8);
    g.setColour(Colour(0xFF2B2F33));
    g.fillRoundedRectangle(bounds.toFloat(), 6.0f);

    g.setColour(Colour(0xFF4DB6A9));
    const int w = bounds.getWidth();
    const int h = bounds.getHeight();
    const float* data = buf->getReadPointer(0);
    const int numSamples = buf->getNumSamples();
    double step = (double)numSamples / (double)w;

    Path p;
    p.preallocateSpace(w * 3);
    for (int x = 0; x < w; ++x)
    {
        int si = (int)(x * step);
        si = jlimit(0, numSamples - 1, si);
        float v = data[si];
        float y = jmap(v, -1.0f, 1.0f, (float)bounds.getBottom(), (float)bounds.getY());
        if (x == 0) p.startNewSubPath(bounds.getX() + x, y);
        else p.lineTo(bounds.getX() + x, y);
    }

    g.strokePath(p, PathStrokeType(1.6f));
}

//
// ResynthesisWindow implementation
//
ResynthesisWindow::ResynthesisWindow(PluginProcessor& ownerProcessor)
    : DocumentWindow ("Resynthesis",
                      Colours::transparentBlack,
                      DocumentWindow::allButtons),
      owner(ownerProcessor)
{
    setUsingNativeTitleBar(false);
    setResizable(true, true);
    setResizeLimits(560, 320, 2200, 1400);

    formatManager.registerBasicFormats();

    // UI components - pass pointers to addAndMakeVisible
    addAndMakeVisible(&uploadBtn);
    uploadBtn.addListener(this);

    addAndMakeVisible(&fileNameLabel);
    fileNameLabel.setText("No file", dontSendNotification);
    fileNameLabel.setColour(Label::textColourId, Colour(0xff9aa0a6));

    addAndMakeVisible(&analyzeBtn);
    analyzeBtn.addListener(this);

    addAndMakeVisible(&originalWave);
    addAndMakeVisible(&resynthWave);

    addAndMakeVisible(&detectedNoteLabel);
    detectedNoteLabel.setColour(Label::textColourId, Colour(0xffd6dce0));
    detectedNoteLabel.setFont(Font(20.0f, Font::bold));
    addAndMakeVisible(&pitchHzLabel);
    pitchHzLabel.setColour(Label::textColourId, Colour(0xff9aa0a6));

    addAndMakeVisible(&zoomSlider);
    zoomSlider.setRange(1.0, 16.0, 1.0);
    zoomSlider.setValue(1.0);
    zoomSlider.addListener(this);

    auto setupKnob = [this](Slider& s)
    {
        s.setSliderStyle(Slider::RotaryVerticalDrag);
        s.setTextBoxStyle(Slider::TextBoxBelow, false, 60, 20);
        s.setRange(0.0, 1.0, 0.01);
        s.setValue(0.5);
        s.addListener(this);
        // use pointer overload
        addAndMakeVisible(&s);
    };

    setupKnob(harmonicSmoothKnob);
    setupKnob(envelopeSmoothKnob);
    setupKnob(subWeightKnob);
    setupKnob(transientKnob);
    setupKnob(distortionKnob);
    setupKnob(noiseBlendKnob);
    setupKnob(glideKnob);
    setupKnob(accuracyKnob);

    addAndMakeVisible(&generateResynthBtn);
    generateResynthBtn.addListener(this);
    addAndMakeVisible(&playResynthBtn);
    playResynthBtn.addListener(this);
    addAndMakeVisible(&replaceMainBtn);
    replaceMainBtn.addListener(this);
    addAndMakeVisible(&exportWavBtn);
    exportWavBtn.addListener(this);

    // FFT
    fftOrder = 11; // 2048
    fftSize = 1 << fftOrder;
    fft.reset(new dsp::FFT(fftOrder));
    fftWindow.assign(fftSize, 0.0f);
    for (int i = 0; i < fftSize; ++i)
        fftWindow[i] = 0.5f * (1.0f - std::cos(2.0 * MathConstants<double>::pi * i / (fftSize - 1)));

    fftData.assign(fftSize * 2, 0.0f);

    setContentNonOwned(new Component(), true);
    setVisible(false);
    centreWithSize(1000, 680);

    hasLoaded = false;
}

ResynthesisWindow::~ResynthesisWindow()
{
    uploadBtn.removeListener(this);
    analyzeBtn.removeListener(this);
    generateResynthBtn.removeListener(this);
    playResynthBtn.removeListener(this);
    replaceMainBtn.removeListener(this);
    exportWavBtn.removeListener(this);
}

void ResynthesisWindow::open()
{
    setVisible(true);
    toFront(true);
}

void ResynthesisWindow::closeWindow()
{
    setVisible(false);
}

void ResynthesisWindow::closeButtonPressed()
{
    setVisible(false);
    if (onCloseCallback) onCloseCallback();
}

void ResynthesisWindow::buttonClicked(Button* b)
{
    if (b == &uploadBtn)
    {
        juce::FileChooser chooser("Select audio file to upload", juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
                                  "*.wav;*.aiff;*.flac;*.mp3;*.ogg");
        // launch async - openMode
        chooser.launchAsync(juce::FileBrowserComponent::openMode,
            [this](const juce::FileChooser& fc)
        {
            juce::File f = fc.getResult();
            if (! f.existsAsFile())
                return;

            MessageManager::callAsync([this, f]()
            {
                fileNameLabel.setText(f.getFileName(), dontSendNotification);

                std::unique_ptr<AudioFormatReader> reader (formatManager.createReaderFor(f));
                if (reader != nullptr)
                {
                    loadedSampleRate = reader->sampleRate;
                    loadedBuffer.setSize((int)reader->numChannels, (int)reader->lengthInSamples);
                    reader->read(&loadedBuffer, 0, (int)reader->lengthInSamples, 0, true, true);

                    if (loadedBuffer.getNumChannels() > 1)
                    {
                        AudioBuffer<float> mono(1, loadedBuffer.getNumSamples());
                        mono.clear();
                        for (int ch = 0; ch < loadedBuffer.getNumChannels(); ++ch)
                            mono.addFrom(0, 0, loadedBuffer, ch, 0, loadedBuffer.getNumSamples(), 1.0f / (float)loadedBuffer.getNumChannels());
                        loadedBuffer = mono;
                    }

                    originalWave.setBuffer(&loadedBuffer);
                    hasLoaded = true;

                    analyzeLoadedFile();
                }
                else
                {
                    AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, "Error", "Could not open audio file.");
                }
            });
        });
    }
    else if (b == &analyzeBtn)
    {
        if (hasLoaded) analyzeLoadedFile();
        else AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, "No file", "Upload a file first.");
    }
    else if (b == &generateResynthBtn)
    {
        if (!hasLoaded)
        {
            AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, "No file", "Upload and analyze a file first.");
            return;
        }

        // detect dominant frequency and map to a friendly 808 range
        double domHz = detectDominantFrequency();
        if (domHz <= 0.0) domHz = 40.0;

        double midi = 69.0 + 12.0 * std::log2(domHz / 440.0);
        double baseMidi = midi;
        if (baseMidi > 48.0) baseMidi = 48.0;
        if (baseMidi < 28.0) baseMidi = jlimit(28.0, 48.0, baseMidi);

        GeneratorParams gp;
        gp.seed = (int64_t)std::chrono::high_resolution_clock::now().time_since_epoch().count();
        gp.sampleRate = loadedSampleRate > 0.0 ? loadedSampleRate : 44100.0;
        gp.lengthSeconds = 1.6;
        gp.tuneSemitones = (float)(baseMidi - 36.0); // map generator base to desired midi

        // map knobs to generator params
        gp.subAmount = (float)subWeightKnob.getValue();
        gp.boomAmount = (float)harmonicSmoothKnob.getValue();
        gp.growl = (float)distortionKnob.getValue();
        gp.punch = (float)transientKnob.getValue();
        gp.detune = (float)(glideKnob.getValue() * 0.4);
        gp.analog = (float)(noiseBlendKnob.getValue() * 0.6);
        gp.masterGainDb = -1.5f;
        gp.clean = (float)(1.0f - accuracyKnob.getValue());

        // generate using the PluginProcessor API so main window can display it
        bool ok = owner.generate808AndStore(gp);
        if (ok)
        {
            // get buffer published by owner
            auto p = owner.getGeneratedBufferSharedPtr();
            if (p)
            {
                generatedPtr = p;
                resynthWave.setBuffer(generatedPtr.get());
            }
            else
            {
                // fallback: render locally
                Generator808 g;
                auto buf = g.renderToBuffer(gp);
                generatedPtr = std::make_shared<AudioBuffer<float>>(buf.getNumChannels(), buf.getNumSamples());
                generatedPtr->makeCopyOf(buf);
                resynthWave.setBuffer(generatedPtr.get());
            }

            // start preview if the processor supports it
            try
            {
                owner.startPreview();
            }
            catch (...) {}
        }
        else
        {
            AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, "Error", "Resynthesis generation failed.");
        }
    }
    else if (b == &playResynthBtn)
    {
        // ask the processor to start preview
        owner.startPreview();
    }
    else if (b == &replaceMainBtn)
    {
        // generation already stores into owner; simply notify user (or trigger generate)
        generateResynthBtn.triggerClick();
    }
    else if (b == &exportWavBtn)
    {
        if (!generatedPtr || generatedPtr->getNumSamples() == 0)
        {
            AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, "No resynth", "Generate a resynth first.");
            return;
        }

        // export wav
        juce::FileChooser chooser("Save resynth as WAV", juce::File::getSpecialLocation(juce::File::userDesktopDirectory), "*.wav");
        chooser.launchAsync(juce::FileBrowserComponent::saveMode,
            [this](const juce::FileChooser& fc)
        {
            juce::File f = fc.getResult();
            if (f == juce::File()) return; // cancelled

            juce::File out = f;
            if (! out.hasFileExtension("wav")) out = out.withFileExtension(".wav");
            bool saved = WavExporter::saveBufferToWav(*generatedPtr, loadedSampleRate > 0.0 ? loadedSampleRate : 44100.0, out, 24);
            if (saved)
                AlertWindow::showMessageBoxAsync(AlertWindow::InfoIcon, "Saved", "WAV exported: " + out.getFullPathName());
            else
                AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, "Error", "Failed to write WAV.");
        });
    }
}

void ResynthesisWindow::sliderValueChanged(Slider* s)
{
    if (s == &zoomSlider)
        originalWave.repaint();
}

void ResynthesisWindow::analyzeLoadedFile()
{
    if (!hasLoaded) return;

    computeRMSAndEnvelope();

    double domHz = detectDominantFrequency();
    if (domHz <= 0.0) domHz = 40.0;

    pitchHzLabel.setText(String(domHz, 2) + " Hz", dontSendNotification);

    double midi = 69.0 + 12.0 * std::log2(domHz / 440.0);
    int midiInt = (int)std::round(midi);
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    int nameIdx = (midiInt + 120) % 12;
    int octave = (midiInt / 12) - 1;
    detectedNoteLabel.setText(String(names[nameIdx]) + String(octave), dontSendNotification);
}

double ResynthesisWindow::detectDominantFrequency()
{
    if (!hasLoaded || loadedBuffer.getNumSamples() < 64) return 0.0;

    const int numSamples = loadedBuffer.getNumSamples();
    int segLen = jmin(fftSize, numSamples);
    int start = jlimit(0, numSamples - segLen, numSamples / 2 - segLen / 2);

    // prepare windowed buffer
    std::fill(fftData.begin(), fftData.end(), 0.0f);
    const float* rd = loadedBuffer.getReadPointer(0);
    for (int i = 0; i < segLen; ++i)
        fftData[i] = rd[start + i] * fftWindow[i];

    // performRealOnlyForwardTransform expects a non-const float*
    fft->performRealOnlyForwardTransform(fftData.data());

    int maxBin = 1;
    float maxVal = 0.0f;
    int nyquist = fftSize / 2;
    for (int b = 1; b < nyquist; ++b)
    {
        float re = fftData[b * 2];
        float im = fftData[b * 2 + 1];
        float mag = std::sqrt(re * re + im * im);
        if (mag > maxVal)
        {
            maxVal = mag;
            maxBin = b;
        }
    }

    double binFreq = (double)loadedSampleRate * (double)maxBin / (double)fftSize;
    return binFreq;
}

void ResynthesisWindow::computeRMSAndEnvelope()
{
    if (!hasLoaded) return;

    const float* d = loadedBuffer.getReadPointer(0);
    int N = loadedBuffer.getNumSamples();

    double sum = 0.0;
    float peak = 0.0f;
    int peakIndex = 0;
    for (int i = 0; i < N; ++i)
    {
        float s = d[i];
        sum += s * s;
        if (std::abs(s) > peak) { peak = std::abs(s); peakIndex = i; }
    }
    double rms = std::sqrt(sum / (double)N);

    double thr = peak * 0.1;
    int attackStart = 0;
    for (int i = 0; i < peakIndex; ++i)
        if (std::abs(d[i]) >= thr) { attackStart = i; break; }
    double attackTime = (peakIndex - attackStart) / loadedSampleRate;

    int releaseIndex = peakIndex;
    for (int i = peakIndex; i < N; ++i)
        if (std::abs(d[i]) <= peak * 0.05f) { releaseIndex = i; break; }
    double releaseTime = (releaseIndex - peakIndex) / loadedSampleRate;

    String info = "RMS: " + String(rms, 4) + "  A: " + String(attackTime, 3) + "s  R: " + String(releaseTime, 3) + "s";
    pitchHzLabel.setText(pitchHzLabel.getText() + "  |  " + info, dontSendNotification);
}
