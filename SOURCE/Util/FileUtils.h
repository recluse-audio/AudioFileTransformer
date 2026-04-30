#pragma once

#include "Juce_Header.h"
#include <functional>

namespace FileUtils
{
    /**
     * Reads a WAV file into a pre-sized destination buffer.
     *
     * Replacement for the removed RD BufferFiller::loadFromWavFile 7-arg overload.
     *
     * @param wavFile          Source file.
     * @param destBuffer       Pre-allocated buffer (must have >= dest channels you want filled).
     * @param maxSamples       Cap on samples to read (clamped against file length and dest length).
     * @param sampleRateOut    Set to the file's sample rate on success.
     * @param numChannelsOut   Set to the file's channel count on success.
     * @param samplesReadOut   Set to actual samples read on success (0 on failure).
     * @param progressCallback Optional 0.0 -> 1.0 progress reporter, fired across chunked reads.
     * @return true on success, false if file missing/unreadable.
     *
     * Mono source -> stereo dest: ch0 is duplicated into ch1.
     * Tail beyond samplesReadOut is left untouched (caller must clear if desired).
     */
    bool loadWavIntoBuffer(const juce::File& wavFile,
                           juce::AudioBuffer<float>& destBuffer,
                           int maxSamples,
                           double& sampleRateOut,
                           int& numChannelsOut,
                           int& samplesReadOut,
                           std::function<void(float)> progressCallback = nullptr);

    /**
     * Checks if a file has a supported audio file extension.
     * Currently supports: .wav, .mp3
     *
     * @param file The file to check
     * @return true if the file extension is supported, false otherwise
     */
    bool isSupportedAudioFile(const juce::File& file);

    /**
     * Validates that an input file exists, is readable, and has a supported format.
     *
     * @param file The input file to validate
     * @param errorMsg Output parameter for error message if validation fails
     * @return true if validation passes, false otherwise
     */
    bool validateInputFile(const juce::File& file, juce::String& errorMsg);

    /**
     * Validates that an output path is valid and writable.
     * Checks that parent directory exists and file has supported extension.
     *
     * @param file The output file path to validate
     * @param errorMsg Output parameter for error message if validation fails
     * @return true if validation passes, false otherwise
     */
    bool validateOutputPath(const juce::File& file, juce::String& errorMsg);

} // namespace FileUtils
