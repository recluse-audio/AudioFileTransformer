#include "Processor/PluginProcessor.h"
#include "Components/PluginEditor.h"
#include "Util/FileUtils.h"
#include "BufferWriter.h"

//==============================================================================
AudioFileTransformerProcessor::AudioFileTransformerProcessor()
    : RD_Processor()
{
    mInputBuffer.clear();
    mProcessedBuffer.clear();

    auto& swapper = mBufferProcessingManager.getSwapper();
    swapper.setDataLogOutputName (swapper.getName());
    addChild (&swapper);
    setIsLogging (true);
}

AudioFileTransformerProcessor::~AudioFileTransformerProcessor()
{
    mFileToBufferManager.stopProcessing();
    removeChild (&mBufferProcessingManager.getSwapper());
}

//==============================================================================
const juce::String AudioFileTransformerProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioFileTransformerProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AudioFileTransformerProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AudioFileTransformerProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AudioFileTransformerProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AudioFileTransformerProcessor::getNumPrograms()
{
    return 1;
}

int AudioFileTransformerProcessor::getCurrentProgram()
{
    return 0;
}

void AudioFileTransformerProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String AudioFileTransformerProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void AudioFileTransformerProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void AudioFileTransformerProcessor::doPrepareToPlay(double sampleRate, int samplesPerBlock)
{
    mBufferProcessingManager.prepareToPlay(sampleRate, samplesPerBlock);

    if (auto* active = mBufferProcessingManager.getSwapper().getActiveProcessor())
        setLatencySamples(active->getLatencySamples());
}

void AudioFileTransformerProcessor::releaseResources()
{
    mBufferProcessingManager.releaseResources();
    RD_Processor::releaseResources();
}

bool AudioFileTransformerProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    auto inputChannels = layouts.getMainInputChannelSet();
    auto outputChannels = layouts.getMainOutputChannelSet();

    if (inputChannels != juce::AudioChannelSet::mono() &&
        inputChannels != juce::AudioChannelSet::stereo())
        return false;

    if (outputChannels != juce::AudioChannelSet::mono() &&
        outputChannels != juce::AudioChannelSet::stereo())
        return false;

    if (inputChannels != outputChannels)
        return false;

    return true;
}

void AudioFileTransformerProcessor::doProcessBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    // Offline-only plugin: realtime audio path outputs silence.
    buffer.clear();
}

//==============================================================================
bool AudioFileTransformerProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* AudioFileTransformerProcessor::createEditor()
{
    return new AudioFileTransformerEditor(*this);
}

//==============================================================================
void AudioFileTransformerProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ignoreUnused(destData);
}

void AudioFileTransformerProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::ignoreUnused(data, sizeInBytes);
}

GainProcessor* AudioFileTransformerProcessor::getGainNode()
{
    return dynamic_cast<GainProcessor*>(
        mBufferProcessingManager.getSwapper().getProcessorByIndex(ActiveProcessor::kGain));
}

GrainShifterProcessor* AudioFileTransformerProcessor::getGrainShifterNode()
{
    return dynamic_cast<GrainShifterProcessor*>(
        mBufferProcessingManager.getSwapper().getProcessorByIndex(ActiveProcessor::kGrainShifter));
}

void AudioFileTransformerProcessor::setActiveProcessor(ActiveProcessor processor)
{
    mBufferProcessingManager.setActiveProcessor(processor);
}


ActiveProcessor AudioFileTransformerProcessor::getActiveProcessor() const
{
    return mBufferProcessingManager.getActiveProcessor();
}

//==============================================================================
bool AudioFileTransformerProcessor::transformFile (const juce::File& inputFile,
                                                    const juce::File& outputFile,
                                                    std::function<void(float)> progressCallback)
{
    
    logData();
    mLastTransformError.clear();

    juce::String validation;
    if (! FileUtils::validateInputFile (inputFile, validation))
    {
        mLastTransformError = "Input invalid: " + validation;
        return false;
    }
    if (! FileUtils::validateOutputPath (outputFile, validation))
    {
        mLastTransformError = "Output invalid: " + validation;
        return false;
    }

    auto loadProgress = [progressCallback] (float p)
    {
        if (progressCallback) progressCallback (p * 0.33f);
    };

    double sampleRate    = 0.0;
    int    numChannels   = 0;
    int    samplesRead   = 0;
    const int maxSamples = mInputBuffer.getNumSamples();

    if (! FileUtils::loadWavIntoBuffer (inputFile, mInputBuffer, maxSamples,
                                         sampleRate, numChannels, samplesRead, loadProgress))
    {
        mLastTransformError = "Failed to load WAV: " + inputFile.getFullPathName();
        return false;
    }
    if (samplesRead <= 0)
    {
        mLastTransformError = "No samples read from input";
        return false;
    }

    int    latencySamples = 0;
    double tailSeconds    = 0.0;
    if (auto* active = mBufferProcessingManager.getSwapper().getActiveProcessor())
    {
        latencySamples = active->getLatencySamples();
        tailSeconds    = active->getTailLengthSeconds();
    }
    const int tailSamples       = static_cast<int> (tailSeconds * sampleRate);
    const int outputSampleCount = juce::jmin (mProcessedBuffer.getNumSamples(),
                                              samplesRead + latencySamples + tailSamples);

    mProcessedBuffer.clear();

    // If logging, cascade once before processing so child loggers sync parent
    // dirs from this processor — required so per-block CSVs nest correctly.
    if (getIsLogging())
        logData();

    auto processProgress = [progressCallback] (float p)
    {
        if (progressCallback) progressCallback (0.33f + p * 0.33f);
    };

    if (! mBufferProcessingManager.processBuffers (mInputBuffer,
                                                    mProcessedBuffer,
                                                    samplesRead,
                                                    outputSampleCount,
                                                    sampleRate,
                                                    512,
                                                    processProgress))
    {
        mLastTransformError = "Processing failed: " + mBufferProcessingManager.getLastError();
        return false;
    }

    auto writeProgress = [progressCallback] (float p)
    {
        if (progressCallback) progressCallback (0.66f + p * 0.34f);
    };

    auto result = BufferWriter::writeToWav (mProcessedBuffer,
                                            outputFile,
                                            sampleRate,
                                            outputSampleCount,
                                            24,
                                            writeProgress);
    if (result != BufferWriter::Result::kSuccess)
    {
        mLastTransformError = "Failed to write WAV: " + outputFile.getFullPathName();
        return false;
    }

    if (getIsLogging())
        logData();

    return true;
}

bool AudioFileTransformerProcessor::doFileTransform (std::function<void(float)> progressCallback)
{
    mLastTransformError.clear();

    auto outputDir = getDataLogOutputDirectory();
    auto createResult = outputDir.createDirectory();
    if (createResult.failed())
    {
        mLastTransformError = "Failed to create processor output directory: "
                              + outputDir.getFullPathName()
                              + " (" + createResult.getErrorMessage() + ")";
        return false;
    }

    auto inputFile  = mFileToBufferManager.getInputFile();
    auto timestamp  = juce::Time::getCurrentTime().formatted ("%Y-%m-%d_%H-%M-%S");
    auto outputFile = outputDir.getChildFile (timestamp + ".wav");

    return transformFile (inputFile, outputFile, std::move (progressCallback));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioFileTransformerProcessor();
}
