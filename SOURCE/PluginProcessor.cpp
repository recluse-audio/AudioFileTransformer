#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioFileTransformerProcessor::AudioFileTransformerProcessor()
    : AudioProcessor(_getBusesProperties())
{
    _setupProcessorGraph();
}

AudioFileTransformerProcessor::~AudioFileTransformerProcessor()
{
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
void AudioFileTransformerProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Enable the graph's buses
    processorGraph.enableAllBuses();

    // Configure the graph's channel layout
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::stereo());
    layout.outputBuses.add(juce::AudioChannelSet::stereo());
    processorGraph.setBusesLayout(layout);

    processorGraph.setPlayConfigDetails(getTotalNumInputChannels(),
                                        getTotalNumOutputChannels(),
                                        sampleRate,
                                        samplesPerBlock);
    processorGraph.prepareToPlay(sampleRate, samplesPerBlock);
}

void AudioFileTransformerProcessor::releaseResources()
{
    processorGraph.releaseResources();
}

bool AudioFileTransformerProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Only support stereo for now
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void AudioFileTransformerProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Process through the graph
    processorGraph.processBlock(buffer, midiMessages);
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
    // No state to save - parameters are in individual nodes
}

void AudioFileTransformerProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::ignoreUnused(data, sizeInBytes);
    // No state to restore - parameters are in individual nodes
}

GainProcessor* AudioFileTransformerProcessor::getGainNode()
{
    // Iterate through all nodes to find the GainProcessor
    for (auto* node : processorGraph.getNodes())
    {
        if (auto* gainProc = dynamic_cast<GainProcessor*>(node->getProcessor()))
            return gainProc;
    }
    return nullptr;
}

juce::AudioProcessor::BusesProperties AudioFileTransformerProcessor::_getBusesProperties()
{
    return BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true);
}

void AudioFileTransformerProcessor::_setupProcessorGraph()
{
    // Clear any existing nodes
    processorGraph.clear();

    // Initialize the graph with input and output buses
    juce::AudioProcessor::BusesProperties graphBuses;
    graphBuses = graphBuses
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true);
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

    // Create and add GainProcessor node
    auto gainProcessor = std::make_unique<GainProcessor>();
    gainProcessor->setGain(0.01f);  // Set gain to 0.1f
    gainNodeID = processorGraph.addNode(std::move(gainProcessor))->nodeID;

    // Connect: Audio Input -> Gain -> Audio Output
    // Connect left channel (channel 0)
    processorGraph.addConnection({
        { audioInputNodeID, 0 },
        { gainNodeID, 0 }
    });
    processorGraph.addConnection({
        { gainNodeID, 0 },
        { audioOutputNodeID, 0 }
    });

    // Connect right channel (channel 1)
    processorGraph.addConnection({
        { audioInputNodeID, 1 },
        { gainNodeID, 1 }
    });
    processorGraph.addConnection({
        { gainNodeID, 1 },
        { audioOutputNodeID, 1 }
    });
}

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioFileTransformerProcessor();
}
