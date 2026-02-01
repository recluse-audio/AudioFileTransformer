#include "FileProcessingManager.h"
#include "PluginProcessor.h"

FileProcessingManager::FileProcessingManager()
{
}

FileProcessingManager::~FileProcessingManager()
{
    stopProcessing();
}

bool FileProcessingManager::startProcessing(const ProcessingConfig& config)
{
    // Don't start if already processing
    if (mIsProcessing.load())
        return false;

    // Validate input file
    if (!config.inputFile.existsAsFile())
        return false;

    // Validate output path
    if (config.outputFile.getFullPathName().isEmpty())
        return false;

    // Create and start processing thread
    mThread = std::make_unique<ProcessingThread>(config);
    mIsProcessing.store(true);
    mThread->startThread();

    return true;
}

void FileProcessingManager::stopProcessing()
{
    if (mThread != nullptr)
    {
        mThread->stopThread(5000);
        mThread.reset();
        mIsProcessing.store(false);
    }
}

bool FileProcessingManager::isProcessing() const
{
    if (mThread == nullptr)
        return false;

    return mThread->isThreadRunning();
}

bool FileProcessingManager::wasSuccessful() const
{
    if (mThread == nullptr)
        return false;

    return mThread->wasSuccessful();
}

juce::String FileProcessingManager::getError() const
{
    if (mThread == nullptr)
        return "No processing has been performed";

    return mThread->getError();
}

//==============================================================================
// ProcessingThread Implementation
//==============================================================================

FileProcessingManager::ProcessingThread::ProcessingThread(const ProcessingConfig& config)
    : juce::Thread("FileProcessing")
    , mConfig(config)
{
}

void FileProcessingManager::ProcessingThread::run()
{
    // Create a separate processor instance for offline processing
    // This provides complete thread isolation from any real-time audio processing
    AudioFileTransformerProcessor offlineProcessor;

    // Configure the processor to match the requested settings
    auto processorType = (mConfig.activeProcessor == ActiveProcessor::Gain)
                        ? AudioFileTransformerProcessor::ActiveProcessor::Gain
                        : AudioFileTransformerProcessor::ActiveProcessor::Granulator;
    offlineProcessor.setActiveProcessor(processorType);

    // Set parameters based on active processor type
    if (mConfig.activeProcessor == ActiveProcessor::Gain)
    {
        auto* gainNode = offlineProcessor.getGainNode();
        if (gainNode != nullptr)
        {
            gainNode->setGain(mConfig.gainValue);
        }
    }
    else // Granulator
    {
        auto* granulatorNode = offlineProcessor.getGranulatorNode();
        if (granulatorNode != nullptr)
        {
            auto* param = granulatorNode->getAPVTS().getParameter("shiftRatio");
            if (param != nullptr)
            {
                // Convert shift ratio (0.5 to 1.5) to normalized value (0.0 to 1.0)
                float normalizedValue = (mConfig.shiftRatio - 0.5f) / 1.0f;
                param->setValueNotifyingHost(normalizedValue);
            }
        }
    }

    // Process the file
    bool success = offlineProcessor.processFile(
        mConfig.inputFile,
        mConfig.outputFile,
        mConfig.progressCallback
    );

    // Store results
    mSuccess.store(success);
    mError = offlineProcessor.getLastError();
}
