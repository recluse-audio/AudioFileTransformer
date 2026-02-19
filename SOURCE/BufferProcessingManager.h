#pragma once

#include "Util/Juce_Header.h"
#include "../SUBMODULES/RD/SOURCE/PROCESSORS/GAIN/GainProcessor.h"
#include "../SUBMODULES/RD/SOURCE/PROCESSORS/GRAIN/GranulatorProcessor.h"
#include "../SUBMODULES/RD/SOURCE/PROCESSORS/TDPSOLA/TDPSOLA_Processor.h"

// Processor type selection
enum class ActiveProcessor
{
    Gain,
    Granulator,
    TDPSOLA
};

/**
 * Manages buffer processing through a processor graph.
 * Handles large audio buffers by parsing them into process blocks,
 * processing through the graph, and writing results.
 * Does NOT inherit from juce::AudioProcessor.
 */
class BufferProcessingManager
{
public:
    BufferProcessingManager();
    ~BufferProcessingManager();

    //==============================================================================
    // Processor management
    void setActiveProcessor(ActiveProcessor processor);
    ActiveProcessor getActiveProcessor() const { return mActiveProcessor; }

    // Access to processor graph nodes (for testing)
    GainProcessor*        getGainNode();
    GranulatorProcessor*  getGranulatorNode();
    TDPSOLA_Processor*    getTDPSOLANode();

    //==============================================================================
    // Buffer processing
    bool processBuffers(const juce::AudioBuffer<float>& inputBuffer, juce::AudioBuffer<float>& outputBuffer,
        double sampleRate, int blockSize = 512, std::function<void(float)> progressCallback = nullptr );

    // Single block processing (for real-time audio)
    void processSingleBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiBuffer);

    // Prepare for real-time processing
    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void releaseResources();

    juce::String getLastError() const { return lastError; }

private:
    //==============================================================================
    // Audio processor graph
    juce::AudioProcessorGraph processorGraph;
    juce::AudioProcessorGraph::NodeID audioInputNodeID;
    juce::AudioProcessorGraph::NodeID audioOutputNodeID;
    juce::AudioProcessorGraph::NodeID gainNodeID;
    juce::AudioProcessorGraph::NodeID granulatorNodeID;
    juce::AudioProcessorGraph::NodeID tdpsolaNodeID;

    // Active processor tracking
    ActiveProcessor mActiveProcessor = ActiveProcessor::TDPSOLA;

    // Error tracking
    juce::String lastError;

    //==============================================================================
    void _setupProcessorGraph();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BufferProcessingManager)
};
