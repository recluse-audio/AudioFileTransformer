#pragma once

#include "Util/Juce_Header.h"
#include <atomic>
#include <functional>

// Forward declaration to avoid circular dependency
class AudioFileTransformerProcessor;

/**
 * @brief Manages offline file processing in a separate thread
 *
 * This class handles creating a processor instance for offline file processing,
 * managing the processing thread, and reporting progress/results.
 */
class FileProcessingManager
{
public:
    enum class ActiveProcessor
    {
        Gain,
        Granulator
    };

    struct ProcessingConfig
    {
        juce::File inputFile;
        juce::File outputFile;
        ActiveProcessor activeProcessor;
        float gainValue;
        float shiftRatio;
        std::function<void(float)> progressCallback;
    };

    FileProcessingManager();
    ~FileProcessingManager();

    // Start processing with the given configuration
    bool startProcessing(const ProcessingConfig& config);

    // Stop processing if currently running
    void stopProcessing();

    // Query processing state
    bool isProcessing() const;
    bool wasSuccessful() const;
    juce::String getError() const;

private:
    class ProcessingThread : public juce::Thread
    {
    public:
        ProcessingThread(const ProcessingConfig& config);

        void run() override;

        bool wasSuccessful() const { return mSuccess; }
        juce::String getError() const { return mError; }

    private:
        ProcessingConfig mConfig;
        std::atomic<bool> mSuccess { false };
        juce::String mError;
    };

    std::unique_ptr<ProcessingThread> mThread;
    std::atomic<bool> mIsProcessing { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FileProcessingManager)
};
