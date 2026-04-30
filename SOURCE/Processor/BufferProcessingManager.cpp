#include "Processor/BufferProcessingManager.h"
#include "PROCESSORS/BASE/RD_Processor.h"

BufferProcessingManager::BufferProcessingManager()
{
    _refreshActiveLoggerChild();
}

BufferProcessingManager::~BufferProcessingManager()
{
    if (auto* active = dynamic_cast<RD_Processor*> (mSwapper.getActiveProcessor()))
        mSwapper.removeChild (active);
}

void BufferProcessingManager::_refreshActiveLoggerChild()
{
    for (int i = 0; i < mSwapper.getNumProcessors(); ++i)
    {
        if (auto* p = dynamic_cast<RD_Processor*> (mSwapper.getProcessorByIndex (static_cast<ActiveProcessor> (i))))
            mSwapper.removeChild (p);
    }

    if (auto* active = dynamic_cast<RD_Processor*> (mSwapper.getActiveProcessor()))
    {
        active->setOutputDirectoryName (active->getName());
        mSwapper.addChild (active);
    }
}

//==============================================================================
void BufferProcessingManager::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mSwapper.prepareToPlay(sampleRate, samplesPerBlock);
}

void BufferProcessingManager::releaseResources()
{
    mSwapper.releaseResources();
}

void BufferProcessingManager::processSingleBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiBuffer)
{
    mSwapper.processBlock(buffer, midiBuffer);
}

//==============================================================================
void BufferProcessingManager::setActiveProcessor(ActiveProcessor processor)
{
    mSwapper.setActiveProcessor(processor);
    _refreshActiveLoggerChild();
}

ActiveProcessor BufferProcessingManager::getActiveProcessor() const
{
    return mSwapper.getActiveProcessorIndex();
}

//==============================================================================
bool BufferProcessingManager::processBuffers(const juce::AudioBuffer<float>& inputStorage,
                                              juce::AudioBuffer<float>&       outputStorage,
                                              int    inputSampleCount,
                                              int    outputSampleCount,
                                              double sampleRate,
                                              int    blockSize,
                                              std::function<void(float)> progressCallback)
{
    lastError.clear();

    if (inputStorage.getNumChannels() == 0 || inputStorage.getNumSamples() == 0)
    {
        lastError = "Input storage buffer is empty";
        return false;
    }

    if (outputStorage.getNumChannels() == 0 || outputStorage.getNumSamples() == 0)
    {
        lastError = "Output storage buffer is not sized";
        return false;
    }

    if (inputSampleCount < 0 || outputSampleCount <= 0)
    {
        lastError = "Invalid sample counts";
        return false;
    }

    if (inputSampleCount  > inputStorage.getNumSamples()
     || outputSampleCount > outputStorage.getNumSamples())
    {
        lastError = "Sample counts exceed storage capacity";
        return false;
    }

    mSwapper.prepareToPlay(sampleRate, blockSize);

    const int numChannels = juce::jmin(inputStorage.getNumChannels(), outputStorage.getNumChannels());
    juce::AudioBuffer<float> processBuffer(numChannels, blockSize);
    juce::MidiBuffer midiBuffer;

    int samplesProcessed = 0;

    while (samplesProcessed < outputSampleCount)
    {
        const int samplesThisBlock = juce::jmin(blockSize, outputSampleCount - samplesProcessed);

        processBuffer.clear();

        const int inputAvailable = juce::jmax(0, inputSampleCount - samplesProcessed);
        const int inputToCopy    = juce::jmin(samplesThisBlock, inputAvailable);

        if (inputToCopy > 0)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                processBuffer.copyFrom(ch, 0, inputStorage, ch, samplesProcessed, inputToCopy);
        }

        mSwapper.processBlock(processBuffer, midiBuffer);

        for (int ch = 0; ch < numChannels; ++ch)
            outputStorage.copyFrom(ch, samplesProcessed, processBuffer, ch, 0, samplesThisBlock);

        samplesProcessed += samplesThisBlock;

        if (progressCallback)
            progressCallback(static_cast<float>(samplesProcessed) / static_cast<float>(outputSampleCount));
    }

    mSwapper.releaseResources();
    return true;
}
