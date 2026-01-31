#pragma once

#include "../Util/Juce_Header.h"
#include <functional>

/**
 * Handles reading, processing, and writing audio files.
 *
 * Currently performs a simple copy operation from input to output.
 * Future versions will support audio transformations.
 */
class AudioFileProcessor
{
public:
    AudioFileProcessor();
    ~AudioFileProcessor();

    /**
     * Process an audio file from input to output.
     *
     * @param inputFile The input audio file to read
     * @param outputFile The output audio file to write
     * @param progressCallback Optional callback for progress updates (0.0 to 1.0)
     * @return true if processing succeeded, false otherwise
     */
    bool processFile(
        const juce::File& inputFile,
        const juce::File& outputFile,
        std::function<void(float)> progressCallback
    );

    /**
     * Get the last error message from a failed operation.
     *
     * @return Error message string, or empty if no error
     */
    juce::String getLastError() const;

private:
    juce::AudioFormatManager formatManager;
    juce::String lastError;

    /**
     * Read an audio file into a buffer.
     *
     * @param file The file to read
     * @param buffer Output buffer to store audio data
     * @param sampleRate Output sample rate
     * @param numChannels Output number of channels
     * @param bitsPerSample Output bits per sample
     * @return true if read succeeded, false otherwise
     */
    bool readAudioFile(
        const juce::File& file,
        juce::AudioBuffer<float>& buffer,
        double& sampleRate,
        unsigned int& numChannels,
        unsigned int& bitsPerSample
    );

    /**
     * Write an audio buffer to a WAV file.
     *
     * @param file The file to write
     * @param buffer The audio buffer to write
     * @param sampleRate Sample rate
     * @param numChannels Number of channels
     * @param bitsPerSample Bits per sample
     * @return true if write succeeded, false otherwise
     */
    bool writeAudioFile(
        const juce::File& file,
        const juce::AudioBuffer<float>& buffer,
        double sampleRate,
        unsigned int numChannels,
        unsigned int bitsPerSample
    );

    /**
     * Process the audio buffer (currently just a copy).
     *
     * @param buffer The buffer to process (in-place)
     */
    void processAudioBuffer(juce::AudioBuffer<float>& buffer);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioFileProcessor)
};
