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
    juce::ignoreUnused(sampleRate, samplesPerBlock);
}

void PluginProcessor::releaseResources()
{
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo())
        return true;
    return false;
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    const int numOutCh = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // If previewing and we have a generated buffer, stream it to output.
    if (isPreviewing())
    {
        std::shared_ptr<juce::AudioBuffer<float>> bufPtr;
        {
            std::lock_guard<std::mutex> lock(generatedBufferMutex);
            bufPtr = generatedBufferPtr;
        }

        if (bufPtr && bufPtr->getNumSamples() > 0)
        {
            const int genCh = bufPtr->getNumChannels();
            int pos = playPosition.load();

            for (int s = 0; s < numSamples; ++s)
            {
                if (pos >= bufPtr->getNumSamples())
                {
                    // reached end; stop preview
                    previewing.store(false);
                    playPosition.store(0);
                    buffer.clear(s, numSamples - s);
                    return;
                }

                for (int ch = 0; ch < numOutCh; ++ch)
                {
                    const float sample = bufPtr->getSample(juce::jmin(ch, genCh - 1), pos);
                    buffer.setSample(ch, s, sample);
                }
                ++pos;
            }

            playPosition.store(pos);
            return;
        }
    }

    // Default: clear output (silence)
    buffer.clear();
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ignoreUnused(destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ignoreUnused(data, sizeInBytes);
}

// generate and store result in generatedBufferPtr
bool PluginProcessor::generate808AndStore(const GeneratorParams& params)
{
    lastParams = params;
    GeneratorParams p = params;
    if (p.sampleRate <= 0.0) p.sampleRate = 44100.0;

    auto buf = generator.renderToBuffer(p); // returns stereo buffer

    auto newBuf = std::make_shared<juce::AudioBuffer<float>>(buf.getNumChannels(), buf.getNumSamples());
    newBuf->makeCopyOf(buf);

    {
        std::lock_guard<std::mutex> lock(generatedBufferMutex);
        generatedBufferPtr = newBuf;
    }

    return (generatedBufferPtr && generatedBufferPtr->getNumSamples() > 0);
}

// thread-safe getter used by editors
std::shared_ptr<juce::AudioBuffer<float>> PluginProcessor::getGeneratedBufferSharedPtr() const noexcept
{
    std::lock_guard<std::mutex> lock(generatedBufferMutex);
    return generatedBufferPtr;
}

// playback control
void PluginProcessor::startPreview() noexcept
{
    playPosition.store(0);
    previewing.store(true);
}

void PluginProcessor::stopPreview() noexcept
{
    previewing.store(false);
    playPosition.store(0);
}

bool PluginProcessor::isPreviewing() const noexcept
{
    return previewing.load();
}
