#pragma once

#include "Juce_Header.h"

namespace FileUtils
{
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
