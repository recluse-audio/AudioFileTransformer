#include "Processor/BufferProcessingManager.h"

BufferProcessingManager::BufferProcessingManager() {}
BufferProcessingManager::~BufferProcessingManager() {}

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
}

ActiveProcessor BufferProcessingManager::getActiveProcessor() const
{
    return mSwapper.getActiveProcessorIndex();
}

//==============================================================================
bool BufferProcessingManager::processBuffers(const juce::AudioBuffer<float>& inputBuffer,juce::AudioBuffer<float>& outputBuffer,
    double sampleRate,int blockSize,std::function<void(float)> progressCallback)
{
    lastError.clear();

    if (inputBuffer.getNumChannels() == 0 || inputBuffer.getNumSamples() == 0)
    {
        lastError = "Input buffer is empty";
        return false;
    }

    if (outputBuffer.getNumChannels() == 0 || outputBuffer.getNumSamples() == 0)
    {
        lastError = "Output buffer is not sized";
        return false;
    }

    mSwapper.prepareToPlay(sampleRate, blockSize);

    juce::AudioBuffer<float> processBuffer(inputBuffer.getNumChannels(), blockSize);
    juce::MidiBuffer midiBuffer;

    int totalSamples = inputBuffer.getNumSamples();
    int samplesProcessed = 0;

    while (samplesProcessed < totalSamples)
    {
        int samplesToProcess = juce::jmin(blockSize, totalSamples - samplesProcessed);

        processBuffer.clear();

        for (int ch = 0; ch < inputBuffer.getNumChannels(); ++ch)
            processBuffer.copyFrom(ch, 0, inputBuffer, ch, samplesProcessed, samplesToProcess);

        mSwapper.processBlock(processBuffer, midiBuffer);

        for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
            outputBuffer.copyFrom(ch, samplesProcessed, processBuffer, ch, 0, samplesToProcess);

        samplesProcessed += samplesToProcess;

        if (progressCallback)
        {
            float progress = (float)samplesProcessed / (float)totalSamples;
            progressCallback(progress);
        }
    }

    mSwapper.releaseResources();
    return true;
}
