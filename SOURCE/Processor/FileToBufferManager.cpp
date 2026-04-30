#include "Processor/FileToBufferManager.h"
#include "Processor/BufferProcessingManager.h"
#include "Util/FileUtils.h"
#include "BufferWriter.h"
#include "PROCESSORS/GAIN/GainProcessor.h"
#include "PROCESSORS/GRAIN/GrainShifterProcessor.h"

//==============================================================================
class FileToBufferManager::WorkerThread : public juce::Thread
{
public:
    WorkerThread(FileToBufferManager&     owner,
                 juce::AudioBuffer<float>& inputStorage,
                 juce::AudioBuffer<float>& outputStorage,
                 BufferProcessingManager& bpm)
        : juce::Thread("FileToBufferManager")
        , mOwner(owner)
        , mInputStorage(inputStorage)
        , mOutputStorage(outputStorage)
        , mBPM(bpm)
    {
    }

    void run() override
    {
        struct ScopedFlag
        {
            std::atomic<bool>& flag;
            ~ScopedFlag() { flag.store(false); }
        } scoped { mOwner.mIsProcessing };

        // Phase 1: load (0.0 -> 0.33)
        double sampleRate    = 0.0;
        int    samplesRead   = 0;
        const int maxSamples = mInputStorage.getNumSamples();

        auto loadProgress = [this](float p)
        {
            if (mOwner.mProgressCallback)
                mOwner.mProgressCallback(p * 0.33f);
        };

        if (! FileUtils::loadWavIntoBuffer(mOwner.mInputFile,
                                            mInputStorage,
                                            maxSamples,
                                            sampleRate,
                                            mNumChannelsRead,
                                            samplesRead,
                                            loadProgress))
        {
            mOwner.mError = "Failed to load WAV: " + mOwner.mInputFile.getFullPathName();
            mOwner.mSuccess.store(false);
            return;
        }

        if (samplesRead <= 0)
        {
            mOwner.mError = "No samples read from WAV file";
            mOwner.mSuccess.store(false);
            return;
        }

        // Phase 2: process (0.33 -> 0.66)
        int    latencySamples = 0;
        double tailSeconds    = 0.0;
        if (auto* active = mBPM.getSwapper().getActiveProcessor())
        {
            latencySamples = active->getLatencySamples();
            tailSeconds    = active->getTailLengthSeconds();
        }
        const int tailSamples = static_cast<int>(tailSeconds * sampleRate);
        const int outputSampleCount = juce::jmin(mOutputStorage.getNumSamples(),
                                                  samplesRead + latencySamples + tailSamples);

        mOutputStorage.clear();

        auto processProgress = [this](float p)
        {
            if (mOwner.mProgressCallback)
                mOwner.mProgressCallback(0.33f + p * 0.33f);
        };

        if (! mBPM.processBuffers(mInputStorage,
                                   mOutputStorage,
                                   samplesRead,
                                   outputSampleCount,
                                   sampleRate,
                                   512,
                                   processProgress))
        {
            mOwner.mError = "Buffer processing failed: " + mBPM.getLastError();
            mOwner.mSuccess.store(false);
            return;
        }

        // Phase 3: write (0.66 -> 1.0)
        auto writeProgress = [this](float p)
        {
            if (mOwner.mProgressCallback)
                mOwner.mProgressCallback(0.66f + p * 0.34f);
        };

        auto result = BufferWriter::writeToWav(mOutputStorage,
                                               mOwner.mResolvedOutputFile,
                                               sampleRate,
                                               outputSampleCount,
                                               24,
                                               writeProgress);

        if (result != BufferWriter::Result::kSuccess)
        {
            mOwner.mError = "Failed to write WAV: " + mOwner.mResolvedOutputFile.getFullPathName();
            mOwner.mSuccess.store(false);
            return;
        }

        mOwner.mSuccess.store(true);
    }

private:
    FileToBufferManager&      mOwner;
    juce::AudioBuffer<float>& mInputStorage;
    juce::AudioBuffer<float>& mOutputStorage;
    BufferProcessingManager&  mBPM;
    int                       mNumChannelsRead { 0 };
};

//==============================================================================
FileToBufferManager::FileToBufferManager() {}
FileToBufferManager::~FileToBufferManager()
{
    stopProcessing();
}

//==============================================================================
juce::File FileToBufferManager::getDefaultInputFile()
{
    return juce::File("C:\\REPOS\\PLUGIN_PROJECTS\\AudioFileTransformer\\AUDIO_FILES\\Somewhere_Mono.wav");
}

juce::File FileToBufferManager::getDefaultOutputDirectory()
{
    return juce::File("C:\\REPOS\\PLUGIN_PROJECTS\\AudioFileTransformer\\OUTPUT");
}

//==============================================================================
bool FileToBufferManager::loadInputToBuffer(juce::AudioBuffer<float>& destBuffer,
                                             int     maxSamples,
                                             double& sampleRateOut,
                                             int&    samplesReadOut)
{
    int numChannelsRead = 0;
    auto progress = [this](float p)
    {
        if (mProgressCallback)
            mProgressCallback(p);
    };

    bool ok = FileUtils::loadWavIntoBuffer(mInputFile,
                                            destBuffer,
                                            maxSamples,
                                            sampleRateOut,
                                            numChannelsRead,
                                            samplesReadOut,
                                            progress);
    if (! ok)
        mError = "Failed to load WAV: " + mInputFile.getFullPathName();

    return ok;
}

bool FileToBufferManager::writeBufferToFile(juce::AudioBuffer<float>& srcBuffer,
                                             const juce::File& outputFile,
                                             double sampleRate,
                                             int    numSamplesToWrite)
{
    auto progress = [this](float p)
    {
        if (mProgressCallback)
            mProgressCallback(p);
    };

    auto result = BufferWriter::writeToWav(srcBuffer, outputFile, sampleRate, numSamplesToWrite, 24, progress);
    if (result != BufferWriter::Result::kSuccess)
    {
        mError = "Failed to write WAV: " + outputFile.getFullPathName();
        return false;
    }
    return true;
}

//==============================================================================
bool FileToBufferManager::startProcessing(juce::AudioBuffer<float>& inputStorage,
                                           juce::AudioBuffer<float>& outputStorage,
                                           BufferProcessingManager&  bufferProcessingManager)
{
    if (mIsProcessing.load())
        return false;

    mError.clear();
    mSuccess.store(false);

    juce::String validationError;
    if (! FileUtils::validateInputFile(mInputFile, validationError))
    {
        mError = "Input file validation failed: " + validationError;
        return false;
    }
    if (mOutputDirectory.getFullPathName().isEmpty())
    {
        mError = "Output directory not set";
        return false;
    }

    auto timestamp = juce::Time::getCurrentTime().formatted("%Y-%m-%d_%H-%M-%S");
    auto runDir    = mOutputDirectory.getChildFile(timestamp);
    auto createResult = runDir.createDirectory();
    if (createResult.failed())
    {
        mError = "Failed to create output directory: " + runDir.getFullPathName();
        return false;
    }

    mResolvedOutputFile = runDir.getChildFile(timestamp + ".wav");

    if (! FileUtils::validateOutputPath(mResolvedOutputFile, validationError))
    {
        mError = "Output path validation failed: " + validationError;
        return false;
    }

    // Write Transformation_Data.md sidecar
    auto* activeNode = bufferProcessingManager.getSwapper().getActiveProcessor();
    juce::String processorName = activeNode ? activeNode->getName() : juce::String("None");
    juce::String parameterXml  = "<NoParameters/>";
    if (auto* gain = dynamic_cast<GainProcessor*>(activeNode))
        parameterXml = gain->getAPVTS().copyState().toXmlString();
    else if (auto* shifter = dynamic_cast<GrainShifterProcessor*>(activeNode))
        parameterXml = shifter->getAPVTS().copyState().toXmlString();

    juce::String md;
    md << "# Transformation Data\n\n"
       << "- **DateTime:** " << timestamp << "\n"
       << "- **Processor:** " << processorName << "\n"
       << "- **Input File:** " << mInputFile.getFullPathName() << "\n\n"
       << "## Parameter State\n\n"
       << "```xml\n" << parameterXml << "\n```\n";
    runDir.getChildFile("Transformation_Data.md").replaceWithText(md);

    mThread = std::make_unique<WorkerThread>(*this, inputStorage, outputStorage, bufferProcessingManager);
    mIsProcessing.store(true);
    mThread->startThread();
    return true;
}

void FileToBufferManager::stopProcessing()
{
    if (mThread != nullptr)
    {
        mThread->stopThread(5000);
        mThread.reset();
        mIsProcessing.store(false);
    }
}
