#include "BufferProcessingManager.h"

BufferProcessingManager::BufferProcessingManager()
{
    _setupProcessorGraph();
}

BufferProcessingManager::~BufferProcessingManager()
{
}

//==============================================================================
void BufferProcessingManager::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Enable the graph's buses
    processorGraph.enableAllBuses();

    // Configure the graph's channel layout
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::stereo());
    layout.outputBuses.add(juce::AudioChannelSet::stereo());
    processorGraph.setBusesLayout(layout);

    processorGraph.setPlayConfigDetails(2, 2, sampleRate, samplesPerBlock);
    processorGraph.prepareToPlay(sampleRate, samplesPerBlock);
}

void BufferProcessingManager::releaseResources()
{
    processorGraph.releaseResources();
}

void BufferProcessingManager::processSingleBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiBuffer)
{
    processorGraph.processBlock(buffer, midiBuffer);
}

//==============================================================================
GainProcessor* BufferProcessingManager::getGainNode()
{
    // Iterate through all nodes to find the GainProcessor
    for (auto* node : processorGraph.getNodes())
    {
        if (auto* gainProc = dynamic_cast<GainProcessor*>(node->getProcessor()))
            return gainProc;
    }
    return nullptr;
}

GranulatorProcessor* BufferProcessingManager::getGranulatorNode()
{
    // Iterate through all nodes to find the GranulatorProcessor
    for (auto* node : processorGraph.getNodes())
    {
        if (auto* granProc = dynamic_cast<GranulatorProcessor*>(node->getProcessor()))
            return granProc;
    }
    return nullptr;
}

TDPSOLA_Processor* BufferProcessingManager::getTDPSOLANode()
{
    for (auto* node : processorGraph.getNodes())
    {
        if (auto* proc = dynamic_cast<TDPSOLA_Processor*>(node->getProcessor()))
            return proc;
    }
    return nullptr;
}

void BufferProcessingManager::setActiveProcessor(ActiveProcessor processor)
{
    // Always disconnect and reconnect to ensure graph is properly configured
    processorGraph.disconnectNode(gainNodeID);
    processorGraph.disconnectNode(granulatorNodeID);
    processorGraph.disconnectNode(tdpsolaNodeID);

    // Determine which processor to connect
    juce::AudioProcessorGraph::NodeID activeNodeID;
    if (processor == ActiveProcessor::Gain)
    {
        activeNodeID = gainNodeID;
        mActiveProcessor = ActiveProcessor::Gain;
    }
    else if (processor == ActiveProcessor::Granulator)
    {
        activeNodeID = granulatorNodeID;
        mActiveProcessor = ActiveProcessor::Granulator;
    }
    else // ActiveProcessor::TDPSOLA
    {
        activeNodeID = tdpsolaNodeID;
        mActiveProcessor = ActiveProcessor::TDPSOLA;
    }

    // Connect: Audio Input -> Active Processor -> Audio Output
    // Connect left channel (channel 0)
    processorGraph.addConnection({
        { audioInputNodeID, 0 },
        { activeNodeID, 0 }
    });
    processorGraph.addConnection({
        { activeNodeID, 0 },
        { audioOutputNodeID, 0 }
    });

    // Connect right channel (channel 1)
    processorGraph.addConnection({
        { audioInputNodeID, 1 },
        { activeNodeID, 1 }
    });
    processorGraph.addConnection({
        { activeNodeID, 1 },
        { audioOutputNodeID, 1 }
    });
}

//==============================================================================
bool BufferProcessingManager::processBuffers(
    const juce::AudioBuffer<float>& inputBuffer,
    juce::AudioBuffer<float>& outputBuffer,
    double sampleRate,
    int blockSize,
    std::function<void(float)> progressCallback)
{
    lastError.clear();

    // Validate buffers
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

    // Enable the graph's buses
    processorGraph.enableAllBuses();

    // Configure the graph's channel layout
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::stereo());
    layout.outputBuses.add(juce::AudioChannelSet::stereo());
    processorGraph.setBusesLayout(layout);

    // Prepare the graph for processing
    processorGraph.setPlayConfigDetails(
        inputBuffer.getNumChannels(),
        outputBuffer.getNumChannels(),
        sampleRate,
        blockSize
    );
    processorGraph.prepareToPlay(sampleRate, blockSize);

    // Create process buffer and MIDI buffer for block-by-block processing
    juce::AudioBuffer<float> processBuffer(inputBuffer.getNumChannels(), blockSize);
    juce::MidiBuffer midiBuffer;

    int totalSamples = inputBuffer.getNumSamples();
    int samplesProcessed = 0;

    // Process in blocks
    while (samplesProcessed < totalSamples)
    {
        int samplesToProcess = juce::jmin(blockSize, totalSamples - samplesProcessed);

        // Clear process buffer
        processBuffer.clear();

        // Copy from input buffer to process buffer
        for (int ch = 0; ch < inputBuffer.getNumChannels(); ++ch)
        {
            processBuffer.copyFrom(ch, 0, inputBuffer, ch, samplesProcessed, samplesToProcess);
        }

        // Process through the graph
        processorGraph.processBlock(processBuffer, midiBuffer);

        // Write to output buffer
        for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
        {
            outputBuffer.copyFrom(ch, samplesProcessed, processBuffer, ch, 0, samplesToProcess);
        }

        samplesProcessed += samplesToProcess;

        // Report progress
        if (progressCallback)
        {
            float progress = (float)samplesProcessed / (float)totalSamples;
            progressCallback(progress);
        }
    }

    // Release resources
    processorGraph.releaseResources();

    return true;
}

//==============================================================================
void BufferProcessingManager::_setupProcessorGraph()
{
    // Clear any existing nodes
    processorGraph.clear();

    // Initialize the graph with input and output buses
    processorGraph.setBusesLayout(juce::AudioProcessor::BusesLayout{
        {juce::AudioChannelSet::stereo()},  // inputs
        {juce::AudioChannelSet::stereo()}   // outputs
    });

    // Create audio input node
    audioInputNodeID = processorGraph.addNode(
        std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(
            juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode
        )
    )->nodeID;

    // Create audio output node
    audioOutputNodeID = processorGraph.addNode(
        std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(
            juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode
        )
    )->nodeID;

    // Create GainProcessor node
    auto gainProcessor = std::make_unique<GainProcessor>();
    gainProcessor->setGain(0.5f);  // Set gain to 0.5f
    gainNodeID = processorGraph.addNode(std::move(gainProcessor))->nodeID;

    // Create and add GranulatorProcessor node
    auto granulatorProcessor = std::make_unique<GranulatorProcessor>();
    granulatorNodeID = processorGraph.addNode(std::move(granulatorProcessor))->nodeID;

    // Create and add TDPSOLA_Processor node
    auto tdpsolaProcessor = std::make_unique<TDPSOLA_Processor>();
    tdpsolaNodeID = processorGraph.addNode(std::move(tdpsolaProcessor))->nodeID;

    // Set the active processor (connects the appropriate processor to input/output)
    setActiveProcessor(mActiveProcessor);
}
