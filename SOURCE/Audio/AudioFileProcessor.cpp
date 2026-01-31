#include "AudioFileProcessor.h"
#include "../Util/FileUtils.h"

AudioFileProcessor::AudioFileProcessor()
{
    // Register basic audio formats (WAV, AIFF, FLAC, OGG, MP3)
    formatManager.registerBasicFormats();
}

AudioFileProcessor::~AudioFileProcessor()
{
}

bool AudioFileProcessor::processFile(
    const juce::File& inputFile,
    const juce::File& outputFile,
    std::function<void(float)> progressCallback)
{
    lastError.clear();

    // Validate input file
    juce::String validationError;
    if (!FileUtils::validateInputFile(inputFile, validationError))
    {
        lastError = "Input file validation failed: " + validationError;
        return false;
    }

    // Validate output path
    if (!FileUtils::validateOutputPath(outputFile, validationError))
    {
        lastError = "Output path validation failed: " + validationError;
        return false;
    }

    // Report initial progress
    if (progressCallback)
        progressCallback(0.0f);

    // Read input file
    juce::AudioBuffer<float> buffer;
    double sampleRate = 0.0;
    unsigned int numChannels = 0;
    unsigned int bitsPerSample = 0;

    if (!readAudioFile(inputFile, buffer, sampleRate, numChannels, bitsPerSample))
    {
        return false;
    }

    // Report progress after read
    if (progressCallback)
        progressCallback(0.4f);

    // Process the audio buffer (currently just a copy, no transformation)
    processAudioBuffer(buffer);

    // Report progress after processing
    if (progressCallback)
        progressCallback(0.7f);

    // Write output file
    if (!writeAudioFile(outputFile, buffer, sampleRate, numChannels, bitsPerSample))
    {
        return false;
    }

    // Report completion
    if (progressCallback)
        progressCallback(1.0f);

    return true;
}

juce::String AudioFileProcessor::getLastError() const
{
    return lastError;
}

bool AudioFileProcessor::readAudioFile(
    const juce::File& file,
    juce::AudioBuffer<float>& buffer,
    double& sampleRate,
    unsigned int& numChannels,
    unsigned int& bitsPerSample)
{
    // Create reader for the input file
    auto reader = std::unique_ptr<juce::AudioFormatReader>(
        formatManager.createReaderFor(file));

    if (reader == nullptr)
    {
        lastError = "Failed to create reader for file: " + file.getFullPathName();
        return false;
    }

    // Get audio properties
    sampleRate = reader->sampleRate;
    numChannels = reader->numChannels;
    bitsPerSample = reader->bitsPerSample;

    // Allocate buffer for entire file
    buffer.setSize((int)reader->numChannels, (int)reader->lengthInSamples);

    // Read all samples into buffer
    if (!reader->read(&buffer, 0, (int)reader->lengthInSamples, 0, true, true))
    {
        lastError = "Failed to read audio data from file: " + file.getFullPathName();
        return false;
    }

    return true;
}

bool AudioFileProcessor::writeAudioFile(
    const juce::File& file,
    const juce::AudioBuffer<float>& buffer,
    double sampleRate,
    unsigned int numChannels,
    unsigned int bitsPerSample)
{
    // Delete existing file if present
    if (file.existsAsFile())
    {
        if (!file.deleteFile())
        {
            lastError = "Failed to delete existing output file: " + file.getFullPathName();
            return false;
        }
    }

    // Create WAV format writer
    juce::WavAudioFormat wavFormat;
    auto fileStream = std::make_unique<juce::FileOutputStream>(file);

    if (!fileStream->openedOk())
    {
        lastError = "Failed to create output file stream: " + file.getFullPathName();
        return false;
    }

    // Create writer with same format as input
    auto writer = std::unique_ptr<juce::AudioFormatWriter>(
        wavFormat.createWriterFor(
            fileStream.release(),
            sampleRate,
            numChannels,
            bitsPerSample > 0 ? bitsPerSample : 24, // Default to 24-bit if not specified
            {},
            0
        )
    );

    if (writer == nullptr)
    {
        lastError = "Failed to create audio writer for output file: " + file.getFullPathName();
        return false;
    }

    // Write buffer to file
    if (!writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()))
    {
        lastError = "Failed to write audio data to file: " + file.getFullPathName();
        return false;
    }

    // Writer is automatically flushed and closed when it goes out of scope
    return true;
}

void AudioFileProcessor::processAudioBuffer(juce::AudioBuffer<float>& buffer)
{
    // For now, this is a simple copy operation (no transformation)
    // Future implementations will add audio processing here
    // (e.g., gain, filtering, effects, etc.)
    juce::ignoreUnused(buffer);
}
