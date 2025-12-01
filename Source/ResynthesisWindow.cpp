#include "ResynthesisWindow.h"
#include "PluginProcessor.h"
#include <cmath>
#include <algorithm>
#include <chrono>

ResynthesisWindow::ResynthesisWindow(juce::AudioProcessor& ownerProcessor)
    : juce::DocumentWindow("Resynthesis",
        juce::Colours::transparentBlack,
        juce::DocumentWindow::allButtons),
    owner(ownerProcessor)
{
    setUsingNativeTitleBar(false);
    setResizable(true, true);
    setResizeLimits(560, 320, 2200, 1400);

    formatManager.registerBasicFormats();

    addAndMakeVisible(uploadBtn);
    uploadBtn.addListener(this);

    addAndMakeVisible(fileNameLabel);
    fileNameLabel.setText("No file", juce::dontSendNotification);
    fileNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9aa0a6));

    addAndMakeVisible(analyzeBtn);
    analyzeBtn.addListener(this);

    addAndMakeVisible(originalWave);
    addAndMakeVisible(resynthWave);

    addAndMakeVisible(detectedNoteLabel);
    detectedNoteLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd6dce0));
    detectedNoteLabel.setFont(juce::Font(20.0f, juce::Font::bold));
    addAndMakeVisible(pitchHzLabel);
    pitchHzLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9aa0a6));

    addAndMakeVisible(zoomSlider);
    zoomSlider.setRange(1.0, 16.0, 1.0);
    zoomSlider.setValue(1.0);
    zoomSlider.addListener(this);

    // knobs
    auto setupKnob = [this](juce::Slider& s)
        {
            s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
            s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            s.setRange(0.0, 1.0, 0.01);
            s.setValue(0.5);
            s.addListener(this);
            addAndMakeVisible(s);
        };

    setupKnob(harmonicSmoothKnob);
    setupKnob(envelopeSmoothKnob);
    setupKnob(subWeightKnob);
    setupKnob(transientKnob);
    setupKnob(distortionKnob);
    setupKnob(noiseBlendKnob);
    setupKnob(glideKnob);
    setupKnob(accuracyKnob);

    addAndMakeVisible(generateResynthBtn);
    addAndMakeVisible(playResynthBtn);
    addAndMakeVisible(replaceMainBtn);
    addAndMakeVisible(exportWavBtn);

    generateResynthBtn.addListener(this);
    playResynthBtn.addListener(this);
    replaceMainBtn.addListener(this);
    exportWavBtn.addListener(this);

    // FFT setup
    fftOrder = 12; // 4096
    fftSize = 1 << fftOrder;
    fft.reset(new juce::dsp::FFT(fftOrder));
    fftWindow.resize(fftSize);
    for (int i = 0; i < fftSize; ++i)
        fftWindow[i] = 0.5f * (1.0f - std::cos(2.0 * juce::MathConstants<double>::pi * i / (fftSize - 1)));

    fftData.resize(fftSize * 2, 0.0f);

    setContentNonOwned(new juce::Component(), true);
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

void ResynthesisWindow::buttonClicked(juce::Button* b)
{
    if (b == &uploadBtn)
    {
        juce::FileChooser chooser("Select audio file to upload", juce::File::getSpecialLocation(juce::File::userDesktopDirectory), "*.wav;*.aiff;*.flac;*.mp3");
        if (chooser.browseForFileToOpen())
        {
            juce::File f = chooser.getResult();
            fileNameLabel.setText(f.getFileName(), juce::dontSendNotification);

            std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(f));
            if (reader != nullptr)
            {
                loadedSampleRate = reader->sampleRate;
                loadedBuffer.setSize((int)reader->numChannels, (int)reader->lengthInSamples);
                reader->read(&loadedBuffer, 0, (int)reader->lengthInSamples, 0, true, true);

                // if stereo, convert to mono for analysis convenience by summing channel 0
                if (loadedBuffer.getNumChannels() > 1)
                {
                    juce::AudioBuffer<float> mono(1, loadedBuffer.getNumSamples());
                    mono.clear();
                    for (int ch = 0; ch < loadedBuffer.getNumChannels(); ++ch)
                        mono.addFrom(0, 0, loadedBuffer, ch, 0, loadedBuffer.getNumSamples(), 1.0f / (float)loadedBuffer.getNumChannels());
                    loadedBuffer = mono;
                }

                originalWave.setBuffer(&loadedBuffer);
                hasLoaded = true;

                // auto-analyze
                analyzeLoadedFile();
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Error", "Could not open audio file.");
            }
        }
    }
    else if (b == &analyzeBtn)
    {
        if (hasLoaded) analyzeLoadedFile();
        else juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "No file", "Upload a file first.");
    }
    else if (b == &generateResynthBtn)
    {
        if (!hasLoaded)
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "No file", "Upload and analyze a file first.");
            return;
        }

        // detect dominant frequency (Hz) and convert to semitone relative to default 440 A4
        double domHz = detectDominantFrequency();
        double midi = 69.0 + 12.0 * std::log2(domHz / 440.0);
        // for 808s we may want to bias low — map midi so that the note sits in 28..48 range if possible
        double baseMidi = midi;
        if (baseMidi > 48.0) baseMidi = 48.0;
        if (baseMidi < 28.0) baseMidi = juce::jlimit(28.0, 48.0, baseMidi);

        GeneratorParams gp;
        gp.seed = (int64_t)std::chrono::high_resolution_clock::now().time_since_epoch().count();
        gp.sampleRate = loadedSampleRate > 0.0 ? loadedSampleRate : 44100.0;
        gp.lengthSeconds = 1.6;
        // tuneSemitones is an offset; to set pitch we compute semitone difference to default base used by generator
        // We'll set tuneSemitones to map generator's random base into the desired pitch: simplest: set tuneSemitones = (baseMidi - 36)
        gp.tuneSemitones = (float)(baseMidi - 36.0); // we used 32..42 earlier; 36 is near center
        // map knobs to params
        gp.subAmount = (float)subWeightKnob.getValue();            // 0..1
        gp.boomAmount = (float)harmonicSmoothKnob.getValue();      // reuse to add body
        gp.growl = (float)distortionKnob.getValue();               // growl ~ distortion
        gp.punch = (float)transientKnob.getValue();
        gp.detune = (float)glideKnob.getValue() * 0.4f;
        gp.analog = (float)noiseBlendKnob.getValue() * 0.6f;
        gp.masterGainDb = -1.5f;
        gp.clean = (float)(1.0f - accuracyKnob.getValue());

        // Use PluginProcessor API when available
        auto* proc = dynamic_cast<PluginProcessor*>(&owner);
        bool ok = false;
        if (proc)
            ok = proc->generate808AndStore(gp);

        if (ok)
        {
            // keep a local shared_ptr to the processor published buffer so we can show in resynthWave
            // If owner didn't provide a shared ptr, render locally as fallback
            try
            {
                if (proc)
                {
                    auto ptr = proc->getGeneratedBufferSharedPtr();
                    if (ptr)
                    {
                        generatedPtr = ptr;
                        resynthWave.setBuffer(generatedPtr.get());
                    }
                    else
                    {
                        Generator808 g;
                        auto buf = g.renderToBuffer(gp);
                        generatedPtr = std::make_shared<juce::AudioBuffer<float>>(buf.getNumChannels(), buf.getNumSamples());
                        generatedPtr->makeCopyOf(buf);
                        resynthWave.setBuffer(generatedPtr.get());
                    }
                }
                else
                {
                    Generator808 g;
                    auto buf = g.renderToBuffer(gp);
                    generatedPtr = std::make_shared<juce::AudioBuffer<float>>(buf.getNumChannels(), buf.getNumSamples());
                    generatedPtr->makeCopyOf(buf);
                    resynthWave.setBuffer(generatedPtr.get());
                }
            }
            catch (...)
            {
                // fallback
                Generator808 g;
                auto buf = g.renderToBuffer(gp);
                generatedPtr = std::make_shared<juce::AudioBuffer<float>>(buf.getNumChannels(), buf.getNumSamples());
                generatedPtr->makeCopyOf(buf);
                resynthWave.setBuffer(generatedPtr.get());
            }

            // optionally auto-start preview if the host preview toggle is on
            try
            {
                if (proc && proc->isPreviewing())
                    proc->startPreview();
            }
            catch (...) {}
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Error", "Resynthesis generation failed.");
        }
    }
    else if (b == &playResynthBtn)
    {
        // play generated buffer: attempt to have owner play it (owner.startPreview)
        auto* proc = dynamic_cast<PluginProcessor*>(&owner);
        if (proc)
        {
            proc->startPreview();
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Play", "Preview not available in this host.");
        }
    }
    else if (b == &replaceMainBtn)
    {
        // Generate and store directly into owner's buffer (same as generateResynthBtn does but always ensure storage)
        generateResynthBtn.triggerClick(); // we already store in owner in that path
    }
    else if (b == &exportWavBtn)
    {
        if (!generatedPtr || generatedPtr->getNumSamples() == 0)
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "No resynth", "Generate a resynth first.");
            return;
        }

        juce::FileChooser chooser("Save resynth as WAV", juce::File::getSpecialLocation(juce::File::userDesktopDirectory), "*.wav");
        if (chooser.browseForFileToSave(true))
        {
            juce::File f = chooser.getResult();
            if (!f.hasFileExtension("wav")) f = f.withFileExtension(".wav");

            bool ok = WavExporter::saveBufferToWav(*generatedPtr, loadedSampleRate > 0.0 ? loadedSampleRate : 44100.0, f, 24);
            if (ok)
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Saved", "WAV exported: " + f.getFullPathName());
            else
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Error", "Failed to write WAV.");
        }
    }
}

void ResynthesisWindow::sliderValueChanged(juce::Slider* s)
{
    // zoom or knobs - immediate UI feedback only
    if (s == &zoomSlider)
    {
        originalWave.repaint();
    }
}

// --- analysis helpers ---

void ResynthesisWindow::analyzeLoadedFile()
{
    if (!hasLoaded) return;

    // compute simple RMS & envelope and fill a very rough estimate of attack/release
    computeRMSAndEnvelope();

    // compute dominant frequency using FFT on a centered segment
    double domHz = detectDominantFrequency();
    if (domHz <= 0.0) domHz = 32.7; // fallback to C1

    pitchHzLabel.setText(juce::String(domHz, 2) + " Hz", juce::dontSendNotification);

    // compute midi note name roughly
    double midi = 69.0 + 12.0 * std::log2(domHz / 440.0);
    int midiInt = (int)std::round(midi);
    // convert to note name
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    int nameIdx = (midiInt + 120) % 12;
    int octave = (midiInt / 12) - 1;
    detectedNoteLabel.setText(juce::String(names[nameIdx]) + juce::String(octave), juce::dontSendNotification);
}

double ResynthesisWindow::detectDominantFrequency() const
{
    if (!hasLoaded || loadedBuffer.getNumSamples() < 64) return 0.0;

    // take a middle segment up to fftSize samples (mono)
    int segLen = juce::jmin(fftSize, loadedBuffer.getNumSamples());
    int start = juce::jlimit(0, loadedBuffer.getNumSamples() - segLen, loadedBuffer.getNumSamples() / 2 - segLen / 2);

    // prepare real FFT input
    std::vector<float> temp(fftSize, 0.0f);
    auto* rd = loadedBuffer.getReadPointer(0);
    for (int i = 0; i < segLen; ++i)
        temp[i] = rd[start + i] * fftWindow[i];

    // use fftData as complex array (real, imag pairs)
    std::fill(fftData.begin(), fftData.end(), 0.0f);
    for (int i = 0; i < fftSize; ++i)
        fftData[i] = (i < (int)temp.size()) ? temp[i] : 0.0f;

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
    auto* d = loadedBuffer.getReadPointer(0);
    int N = loadedBuffer.getNumSamples();

    // RMS
    double sum = 0.0;
    for (int i = 0; i < N; ++i) sum += d[i] * d[i];
    double rms = std::sqrt(sum / (double)N);

    // quick approximate attack/delay via envelope threshold detection
    float peak = 0.0f;
    int peakIndex = 0;
    for (int i = 0; i < N; ++i)
        if (std::abs(d[i]) > peak) { peak = std::abs(d[i]); peakIndex = i; }

    // find attack start (when envelope crosses 10% of peak)
    double thr = peak * 0.1;
    int attackStart = 0;
    for (int i = 0; i < peakIndex; ++i)
    {
        if (std::abs(d[i]) >= thr) { attackStart = i; break; }
    }
    double attackTime = (peakIndex - attackStart) / loadedSampleRate;
    // release: find when envelope falls below 5% after peak
    int releaseIndex = peakIndex;
    for (int i = peakIndex; i < N; ++i)
    {
        if (std::abs(d[i]) <= peak * 0.05) { releaseIndex = i; break; }
    }
    double releaseTime = (releaseIndex - peakIndex) / loadedSampleRate;

    // update labels (we used pitch label earlier)
    juce::String info = "RMS: " + juce::String(rms, 4) + "  A: " + juce::String(attackTime, 3) + "s  R: " + juce::String(releaseTime, 3) + "s";
    // append to pitch label for a quick multi-line view
    pitchHzLabel.setText(pitchHzLabel.getText() + "  |  " + info, juce::dontSendNotification);
}
