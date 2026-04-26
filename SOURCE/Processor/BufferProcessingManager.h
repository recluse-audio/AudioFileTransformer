#pragma once

#include "Util/Juce_Header.h"
#include "PROCESSORS/RD_ProcessorSwapper.h"

using ActiveProcessor = RD_ProcessorSwapper::ProcessorIndex;

/**
 * Manages buffer processing via an RD_ProcessorSwapper.
 * Handles large audio buffers by parsing them into process blocks,
 * processing through the swapper, and writing results.
 */
class BufferProcessingManager
{
public:
    BufferProcessingManager();
    ~BufferProcessingManager();

    //==============================================================================
    void setActiveProcessor(ActiveProcessor processor);
    ActiveProcessor getActiveProcessor() const;

    RD_ProcessorSwapper&       getSwapper()       { return mSwapper; }
    const RD_ProcessorSwapper& getSwapper() const { return mSwapper; }

    //==============================================================================
    bool processBuffers(const juce::AudioBuffer<float>& inputStorage,
                        juce::AudioBuffer<float>&       outputStorage,
                        int    inputSampleCount,
                        int    outputSampleCount,
                        double sampleRate,
                        int    blockSize = 512,
                        std::function<void(float)> progressCallback = nullptr);

    void processSingleBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiBuffer);

    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void releaseResources();

    juce::String getLastError() const { return lastError; }

private:
    RD_ProcessorSwapper mSwapper;
    juce::String lastError = "-";

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BufferProcessingManager)
};
