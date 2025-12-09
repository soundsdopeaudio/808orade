#include "PluginEditor.h"
#include "DescriptorWindow.h"
#include "BatchWindow.h"
#include "ResynthesisWindow.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (600, 600);
    setResizable(true, true);
	setResizeLimits(400, 400, 800, 800);

    // waveform
    addAndMakeVisible(waveform);

    // generate button
    addAndMakeVisible(generateButton);
    generateButton.addListener(this);

    // export button
    addAndMakeVisible(exportButton);
    exportButton.addListener(this);

    // preview toggle
    addAndMakeVisible(previewToggle);
    previewToggle.addListener(this);

    // note label
    addAndMakeVisible(noteLabel);
    noteLabel.setText("C1", juce::dontSendNotification);
    noteLabel.setJustificationType(juce::Justification::centred);
    noteLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD6DCE0));
    noteLabel.setFont(juce::Font(36.0f, juce::Font::bold));

    // seed
    addAndMakeVisible(seedLabel);
    seedLabel.setText("Seed: -", juce::dontSendNotification);
    seedLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF9AA0A6));

    addAndMakeVisible(copySeedButton);
    copySeedButton.addListener(this);

    // tune knob (RegeneratingSlider) - detect mouseUp to regenerate once user finishes dragging
    tuneSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    tuneSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    tuneSlider.setRange(-24.0, 24.0, 0.01);
    tuneSlider.setValue(0.0);
    addAndMakeVisible(tuneSlider);
    tuneSlider.addListener(this);

    tuneSlider.setMouseUpCallback([this]()
    {
        regenerateFromCurrentUI();
    });

    addAndMakeVisible(tuneLabel);
    tuneLabel.setText("TUNE (st)", juce::dontSendNotification);
    tuneLabel.setJustificationType(juce::Justification::centred);

    // main menu button
    addAndMakeVisible(mainMenuBtn);
    mainMenuBtn.addListener(this);
    mainMenuBtn.setTooltip("Menu");

    // create windows but don't open them yet
    descriptorWindow.reset(new DescriptorWindow(processor));
    batchWindow.reset(new BatchWindow(processor));

    // colors & fonts
    setOpaque(true);
    setLookAndFeel(nullptr);
}

PluginEditor::~PluginEditor()
{
    generateButton.removeListener(this);
    exportButton.removeListener(this);
    previewToggle.removeListener(this);
    copySeedButton.removeListener(this);
    tuneSlider.removeListener(this);
    mainMenuBtn.removeListener(this);
}

void PluginEditor::paint (juce::Graphics& g)
{
    // Background
    g.fillAll (juce::Colour (0xFF121416));

    // subtle divider header moved down so it doesn't overlap the logo
    const int headerH = 80; // increased height
    g.setColour(juce::Colours::grey);
    g.drawLine(0.0f, (float)headerH - 1.0f, (float)getWidth(), (float)headerH - 1.0f, 1.0f);

    // Title fallback (drawn below the logo area)
    g.setColour(juce::Colour(0xFFD6DCE0));
    g.setFont(juce::Font(28.0f, juce::Font::bold));
    g.drawText("808orade", 12, headerH - 44, getWidth() - 24, 44, juce::Justification::left);
}

void PluginEditor::resized()
{
    auto r = getLocalBounds().reduced(18);

    // position mainMenuBtn top-right
    mainMenuBtn.setBounds(getWidth() - 44, 8, 36, 28);

    // reserve space for header/logo area so waveform doesn't overlap it
    const int headerH = 80;
    auto topArea = r.removeFromTop(headerH + 16);
    waveform.setBounds(topArea.reduced(0, 8));

    // Middle area: note display, tune knob, generate button
    auto middle = r.removeFromTop(200);
    auto leftCol = middle.removeFromLeft(middle.getWidth() / 2).reduced(8);
    auto rightCol = middle.reduced(8);

    noteLabel.setBounds(leftCol.removeFromTop(80));
    generateButton.setBounds(leftCol.removeFromTop(48).reduced(0, 8));
    seedLabel.setBounds(leftCol.removeFromTop(24));
    copySeedButton.setBounds(leftCol.removeFromTop(36).withSizeKeepingCentre(120, 24));

    tuneSlider.setBounds(rightCol.removeFromTop(160).reduced(24));
    tuneLabel.setBounds(rightCol.removeFromTop(24));

    // bottom area: preview toggle and export
    auto bottom = r.removeFromBottom(120);
    previewToggle.setBounds(bottom.removeFromLeft(120).reduced(8));
    exportButton.setBounds(bottom.reduced(8).withHeight(48).withWidth(160).withX(bottom.getCentreX()-80));
}

void PluginEditor::updateWaveformFromProcessor()
{
    currentGeneratedBufferPtr = processor.getGeneratedBufferSharedPtr();

    if (currentGeneratedBufferPtr && currentGeneratedBufferPtr->getNumSamples() > 0)
    {
        waveform.setBuffer(currentGeneratedBufferPtr.get());
        seedLabel.setText("Seed: " + juce::String((int64_t)processor.getLastParams().seed), juce::dontSendNotification);
        noteLabel.setText("Tune " + juce::String(processor.getLastParams().tuneSemitones, 2) + " st", juce::dontSendNotification);
    }
    else
    {
        waveform.setBuffer(nullptr);
        seedLabel.setText("Seed: -", juce::dontSendNotification);
        noteLabel.setText("C1", juce::dontSendNotification);
    }
}

void PluginEditor::regenerateFromCurrentUI()
{
    GeneratorParams gp;
    const auto& last = processor.getLastParams();
    gp = last;
    gp.seed = (int64_t)std::chrono::high_resolution_clock::now().time_since_epoch().count();
    gp.sampleRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 44100.0;
    gp.lengthSeconds = last.lengthSeconds > 0.0 ? last.lengthSeconds : 1.6;
    gp.tuneSemitones = (float)tuneSlider.getValue();
    gp.masterGainDb = last.masterGainDb;

    // If descriptor window exists and user has selected keywords, merge them into params
    if (descriptorWindow)
    {
        auto sel = descriptorWindow->getSelectedKeywords();
        for (auto& kv : sel)
        {
            const juce::String id = kv.first;
            const float intensity = kv.second;
            if (id == "subBtn")
                gp.subAmount = juce::jlimit(0.0f, 1.0f, gp.subAmount + 0.6f * intensity);
            else if (id == "boomyBtn")
                gp.boomAmount = juce::jlimit(0.0f, 1.0f, gp.boomAmount + 0.5f * intensity);
            else if (id == "shortBtn")
                gp.shortness = juce::jlimit(0.0f, 1.0f, gp.shortness + 0.9f * intensity);
            else if (id == "punchyBtn")
                gp.punch = juce::jlimit(0.0f, 1.0f, gp.punch + 0.8f * intensity);
            else if (id == "growlBtn")
                gp.growl = juce::jlimit(0.0f, 1.0f, gp.growl + 0.75f * intensity);
            else if (id == "detunedBtn")
                gp.detune = juce::jlimit(0.0f, 1.0f, gp.detune + 0.8f * intensity);
            else if (id == "analogBtn")
                gp.analog = juce::jlimit(0.0f, 1.0f, gp.analog + 0.6f * intensity);
            else if (id == "cleanBtn")
                gp.clean = juce::jlimit(0.0f, 1.0f, gp.clean + 1.0f * intensity);
            else if (id == "deepBtn")
                gp.subAmount = juce::jlimit(0.0f, 1.0f, gp.subAmount + 0.4f * intensity);
            else if (id == "saturatedBtn")
                gp.masterGainDb += 0.5f * intensity;
        }
    }

    if (std::isnan(gp.subAmount) || gp.subAmount < 0.0f) gp.subAmount = 0.6f;
    if (std::isnan(gp.boomAmount) || gp.boomAmount < 0.0f) gp.boomAmount = 0.4f;
    if (std::isnan(gp.punch) || gp.punch < 0.0f) gp.punch = 0.55f;

    bool ok = processor.generate808AndStore(gp);
    if (ok)
    {
        updateWaveformFromProcessor();
        if (previewToggle.getToggleState())
            processor.startPreview();
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Error", "Generation failed.");
    }
}

void PluginEditor::buttonClicked(juce::Button* b)
{
    if (b == &generateButton)
    {
        regenerateFromCurrentUI();
    }
    else if (b == &exportButton)
    {
        auto bufPtr = processor.getGeneratedBufferSharedPtr();
        if (!bufPtr || bufPtr->getNumSamples() == 0)
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "No audio", "Generate an 808 first.");
            return;
        }

        juce::FileChooser chooser("Save 808 as WAV", juce::File::getSpecialLocation(juce::File::userDesktopDirectory), "*.wav");
        chooser.launchAsync(juce::FileBrowserComponent::saveMode,
            [this, bufPtr](const juce::FileChooser& fc)
        {
            juce::File f = fc.getResult();
            if (f == juce::File()) return; // cancelled
            juce::File out = f;
            if (!out.hasFileExtension("wav")) out = out.withFileExtension(".wav");

            bool saved = WavExporter::saveBufferToWav(*bufPtr, processor.getLastParams().sampleRate > 0.0 ? processor.getLastParams().sampleRate : 44100.0, out, 24);
            if (saved)
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Saved", "WAV exported: " + out.getFullPathName());
            else
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Error", "Failed to write WAV.");
        });
    }
    else if (b == &copySeedButton)
    {
        juce::SystemClipboard::copyTextToClipboard(seedLabel.getText());
    }
    else if (b == &previewToggle)
    {
        if (previewToggle.getToggleState())
            processor.startPreview();
        else
            processor.stopPreview();
    }
    else if (b == &mainMenuBtn)
    {
        showMainMenu();
    }
}

void PluginEditor::sliderValueChanged(juce::Slider* s)
{
    if (s == &tuneSlider)
    {
        noteLabel.setText("Tune " + juce::String(tuneSlider.getValue(), 2) + " st", juce::dontSendNotification);
    }
}

void PluginEditor::showMainMenu()
{
    juce::PopupMenu pm;
    pm.addItem(1, "Descriptor / Tagging...");
    pm.addItem(2, "Resynthesis...");
    pm.addItem(3, "Batch Exporter...");
    pm.addItem(4, "Settings...");

    pm.showMenuAsync(juce::PopupMenu::Options(),
        [this](int result)
    {
        if (result == 1)
        {
            if (descriptorWindow)
                descriptorWindow->open();
        }
        else if (result == 3)
        {
            if (batchWindow)
                batchWindow->open();
        }
        else if (result == 2)
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Resynthesis", "Open the Resynthesis window from the menu (coming soon).");
        }
        else if (result == 4)
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Settings", "Settings window not implemented yet.");
        }
    });
}
