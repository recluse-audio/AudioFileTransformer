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
    // Validate processor
    if (mConfig.processor == nullptr)
    {
        mError = "No processor provided";
        mSuccess.store(false);
        return;
    }

    // Use the processor directly - it should already be configured
    // Cast to AudioFileTransformerProcessor to access processFile method
    auto* mainProcessor = dynamic_cast<AudioFileTransformerProcessor*>(mConfig.processor);
    if (mainProcessor == nullptr)
    {
        mError = "Processor must be an AudioFileTransformerProcessor";
        mSuccess.store(false);
        return;
    }

    // Process the file using the provided, pre-configured processor
    bool success = mainProcessor->processFile(
        mConfig.inputFile,
        mConfig.outputFile,
        mConfig.progressCallback
    );

    // Store results
    mSuccess.store(success);
    mError = mainProcessor->getLastError();
}
