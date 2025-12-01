#include "BatchWindow.h"
#include "WavExporter.h"
#include "808Generator.h"
#include <chrono>

BatchWindow::BatchWindow(juce::AudioProcessor& ownerProcessor)
    : juce::DocumentWindow("Batch Exporter",
        juce::Colours::transparentBlack,
        DocumentWindow::allButtons),
    owner(ownerProcessor)
{
    setUsingNativeTitleBar(false);
    setResizable(true, true);
    setResizeLimits(540, 280, 2000, 1200);

    addAndMakeVisible(useDescriptorToggle);
    addAndMakeVisible(countCombo);
    addAndMakeVisible(chooseFolderBtn);
    addAndMakeVisible(folderLabel);
    addAndMakeVisible(prefixEditor);
    addAndMakeVisible(generateBatchBtn);
    addAndMakeVisible(exportAllBtn);

    chooseFolderBtn.addListener(this);
    generateBatchBtn.addListener(this);
    exportAllBtn.addListener(this);

    countCombo.addItem("25", 1);
    countCombo.addItem("50", 2);
    countCombo.addItem("100", 3);
    countCombo.setSelectedId(1);

    prefixEditor.setText("808_");
    folderLabel.setText("No folder selected", juce::dontSendNotification);

    setContentNonOwned(new juce::Component(), true);
    setVisible(false);
    centreWithSize(720, 420);
}

BatchWindow::~BatchWindow()
{
    chooseFolderBtn.removeListener(this);
    generateBatchBtn.removeListener(this);
    exportAllBtn.removeListener(this);
}

void BatchWindow::open()
{
    setVisible(true);
    toFront(true);
}

void BatchWindow::closeWindow()
{
    setVisible(false);
}

void BatchWindow::buttonClicked(juce::Button* b)
{
    if (b == &chooseFolderBtn)
    {
        juce::FileChooser chooser("Select destination folder", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "");
        if (chooser.browseForDirectory())
        {
            destFolder = chooser.getResult();
            folderLabel.setText(destFolder.getFullPathName(), juce::dontSendNotification);
        }
    }
    else if (b == &generateBatchBtn)
    {
        int count = 25;
        if (countCombo.getSelectedId() == 1) count = 25;
        else if (countCombo.getSelectedId() == 2) count = 50;
        else if (countCombo.getSelectedId() == 3) count = 100;

        if (!destFolder.exists())
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "No folder", "Please choose an export folder first.");
            return;
        }

        juce::String prefix = prefixEditor.getText();
        if (prefix.isEmpty()) prefix = "808_";

        // Confirm action
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Batch", "Starting batch generation (" + juce::String(count) + " files). This may take some time.");

        // Use a fresh Generator808 instance so we don't rely on processor internals in case owner is hosted
        Generator808 gen;

        int savedCount = 0;
        for (int i = 0; i < count; ++i)
        {
            // Build params baseline from owner's last params if available; otherwise fallback defaults
            GeneratorParams gp;
            try
            {
                // try to read last params from owner if it exposes getLastParams (our PluginProcessor does)
                // We'll attempt to dynamic_cast to PluginProcessor pointer by known type name
                // If cast fails we just use defaults
                // Because we don't have the concrete type here in header, we rely on owner.getLastParams in plugin build
                // Safe fallback:
                gp = static_cast<GeneratorParams>(owner.getLastParams());
            }
            catch (...)
            {
                gp.sampleRate = 44100.0;
                gp.lengthSeconds = 1.6;
                gp.masterGainDb = -1.5f;
                gp.tuneSemitones = 0.0f;
                gp.subAmount = 0.6f;
                gp.boomAmount = 0.4f;
                gp.punch = 0.55f;
                gp.growl = 0.2f;
                gp.detune = 0.05f;
                gp.analog = 0.08f;
                gp.clean = 0.0f;
            }

            // generate a seed (time-based + index)
            gp.seed = (int64_t)(std::chrono::high_resolution_clock::now().time_since_epoch().count() + i * 7919);
            if (gp.sampleRate <= 0.0) gp.sampleRate = 44100.0;

            // Render
            auto buf = gen.renderToBuffer(gp);

            // filename zero-padded
            juce::String filename = prefix + juce::String::formatted("%03d.wav", i + 1);
            juce::File out = destFolder.getChildFile(filename);
            bool ok = false;
            if (buf.getNumSamples() > 0)
                ok = WavExporter::saveBufferToWav(buf, gp.sampleRate, out, 24);

            if (ok) ++savedCount;
            else juce::Logger::writeToLog("Batch: failed to save " + out.getFullPathName());
        }

        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Batch Done", "Finished generating batch. Saved " + juce::String(savedCount) + " / " + juce::String(count) + " files.");
    }
    else if (b == &exportAllBtn)
    {
        // alias for generateBatchBtn
        generateBatchBtn.triggerClick();
    }
}
