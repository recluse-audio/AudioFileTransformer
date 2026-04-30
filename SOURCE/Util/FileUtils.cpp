#include "FileUtils.h"

namespace FileUtils
{

bool isSupportedAudioFile(const juce::File& file)
{
    auto extension = file.getFileExtension().toLowerCase();
    return extension == ".wav" || extension == ".mp3";
}

bool validateInputFile(const juce::File& file, juce::String& errorMsg)
{
    errorMsg.clear();

    // Check if file exists
    if (!file.existsAsFile())
    {
        errorMsg = "File not found: " + file.getFullPathName();
        return false;
    }

    // Check if it's actually a file (not a directory)
    if (file.isDirectory())
    {
        errorMsg = "Path is a directory, not a file: " + file.getFullPathName();
        return false;
    }

    // Check if file has supported extension
    if (!isSupportedAudioFile(file))
    {
        errorMsg = "File format not supported. Only .wav and .mp3 files are supported: "
                   + file.getFullPathName();
        return false;
    }

    // Check if file is readable
    auto fileStream = file.createInputStream();
    if (fileStream == nullptr)
    {
        errorMsg = "Cannot read file (permission denied?): " + file.getFullPathName();
        return false;
    }

    return true;
}

bool validateOutputPath(const juce::File& file, juce::String& errorMsg)
{
    errorMsg.clear();

    // Check if file path is empty
    if (file.getFullPathName().isEmpty())
    {
        errorMsg = "Output file path is empty";
        return false;
    }

    // Check if parent directory exists
    auto parentDir = file.getParentDirectory();
    if (!parentDir.exists())
    {
        errorMsg = "Parent directory does not exist: " + parentDir.getFullPathName();
        return false;
    }

    // Check if parent is actually a directory
    if (!parentDir.isDirectory())
    {
        errorMsg = "Parent path is not a directory: " + parentDir.getFullPathName();
        return false;
    }

    // Check if file has supported extension
    if (!isSupportedAudioFile(file))
    {
        errorMsg = "Output file extension not supported. Only .wav and .mp3 are supported: "
                   + file.getFileExtension();
        return false;
    }

    // Check if parent directory is writable by attempting to create a temp file
    auto tempFile = parentDir.getChildFile(".write_test_" + juce::String(juce::Random::getSystemRandom().nextInt()));
    if (!tempFile.create())
    {
        errorMsg = "Cannot write to directory (permission denied?): " + parentDir.getFullPathName();
        return false;
    }
    tempFile.deleteFile();

    return true;
}

bool loadWavIntoBuffer(const juce::File& wavFile,
                       juce::AudioBuffer<float>& destBuffer,
                       int maxSamples,
                       double& sampleRateOut,
                       int& numChannelsOut,
                       int& samplesReadOut,
                       std::function<void(float)> progressCallback)
{
    sampleRateOut   = 0.0;
    numChannelsOut  = 0;
    samplesReadOut  = 0;

    if (! wavFile.existsAsFile())
        return false;

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (wavFile));
    if (reader == nullptr)
        return false;

    sampleRateOut  = reader->sampleRate;
    numChannelsOut = static_cast<int> (reader->numChannels);

    const int fileSamples = static_cast<int> (juce::jmin<juce::int64> (
        reader->lengthInSamples,
        static_cast<juce::int64> (std::numeric_limits<int>::max())));

    const int destSamples   = destBuffer.getNumSamples();
    const int totalToRead   = juce::jmin (maxSamples, fileSamples, destSamples);
    if (totalToRead <= 0)
        return true;

    const int destChannels  = destBuffer.getNumChannels();
    const int chunkSize     = 4096;
    int       samplesDone   = 0;

    while (samplesDone < totalToRead)
    {
        const int thisChunk = juce::jmin (chunkSize, totalToRead - samplesDone);
        const bool ok = reader->read (&destBuffer, samplesDone, thisChunk, samplesDone, true, destChannels > 1);
        if (! ok)
            return false;
        samplesDone += thisChunk;

        if (progressCallback)
            progressCallback (static_cast<float> (samplesDone) / static_cast<float> (totalToRead));
    }

    if (numChannelsOut == 1 && destChannels > 1)
    {
        for (int ch = 1; ch < destChannels; ++ch)
            destBuffer.copyFrom (ch, 0, destBuffer, 0, 0, totalToRead);
    }

    samplesReadOut = totalToRead;
    return true;
}

} // namespace FileUtils
