#include "WavExporter.h"

bool WavExporter::saveBufferToWav(const juce::AudioBuffer<float>& buffer,
    double sampleRate,
    const juce::File& file,
    int bitsPerSample)
{
    if (file.existsAsFile())
    {
        if (!file.deleteFile())
        {
            // try to remove existing file; if we can't, fail early
            juce::Logger::writeToLog("WavExporter: failed to delete existing file: " + file.getFullPathName());
            return false;
        }
    }

    // Create an output stream which the writer will take ownership of.
    std::unique_ptr<juce::FileOutputStream> stream(file.createOutputStream());
    if (!stream || !stream->openedOk())
        return false;

    juce::WavAudioFormat wavFormat;
    // createWriterFor takes ownership of the stream pointer. We'll hand it the raw pointer from our unique_ptr.release()
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(stream.release(), sampleRate,
            (unsigned int)buffer.getNumChannels(),
            bitsPerSample, {}, 0));
    if (!writer)
        return false;

    // writeFromAudioSampleBuffer will write the samples
    writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
    // writer destructor will delete the stream now
    return true;
}
