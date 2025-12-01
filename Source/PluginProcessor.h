#pragma once

#include <JuceHeader.h>
#include "808Generator.h"
#include "WavExporter.h"
#include <atomic>
#include <memory>
#include <mutex>

//==============================================================================
// Audio processor for 808orade with preview playback support.
// generate808AndStore(...) stores a generated buffer into an atomically-published
// shared_ptr that the audio thread will read and play when previewing.
// NOTE: std::atomic<std::shared_ptr<T>> is not supported because std::shared_ptr
// is not trivially copyable on MSVC. We use a mutex-protected shared_ptr instead.
class PluginProcessor  : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    // standard overrides
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "808orade"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int index) override { juce::ignoreUnused(index); }
    const juce::String getProgramName (int index) override { juce::ignoreUnused(index); return {}; }
    void changeProgramName (int index, const juce::String& newName) override { juce::ignoreUnused(index, newName); }

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- API used by editor ----
    // Generate an 808 using the provided params and publish the buffer for playback / display.
    // Returns true on success.
    bool generate808AndStore(const GeneratorParams& params);

    // Return a shared_ptr to the current generated buffer. May be nullptr if none generated.
    std::shared_ptr<juce::AudioBuffer<float>> getGeneratedBufferSharedPtr() const noexcept;

    // Start/stop preview playback (threadsafe)
    void startPreview() noexcept;
    void stopPreview() noexcept;
    bool isPreviewing() const noexcept;

    // Access last used params (for display / seed, etc.)
    const GeneratorParams& getLastParams() const noexcept { return lastParams; }

private:
    Generator808 generator;

    // Publication of the generated buffer:
    // Use a mutex-protected shared_ptr instead of std::atomic<std::shared_ptr<...>>
    // to avoid the static_assert failure on MSVC.
    mutable std::mutex generatedBufferMutex;
    std::shared_ptr<juce::AudioBuffer<float>> generatedBufferPtr; // guarded by generatedBufferMutex

    // playback state (audio thread reads/writes)
    std::atomic<int> playPosition { 0 };
    std::atomic<bool> previewing { false };

    GeneratorParams lastParams;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
