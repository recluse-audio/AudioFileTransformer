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

} // namespace FileUtils
