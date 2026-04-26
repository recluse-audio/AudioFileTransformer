#include "Processor/PluginProcessor.h"
#include "Components/PluginEditor.h"

//==============================================================================
AudioFileTransformerProcessor::AudioFileTransformerProcessor()
    : AudioProcessor(_getBusesProperties())
{
    mInputBuffer.clear();
    mProcessedBuffer.clear();
}

AudioFileTransformerProcessor::~AudioFileTransformerProcessor()
{
    mFileToBufferManager.stopProcessing();
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
    mBufferProcessingManager.prepareToPlay(sampleRate, samplesPerBlock);

    if (auto* active = mBufferProcessingManager.getSwapper().getActiveProcessor())
        setLatencySamples(active->getLatencySamples());
}

void AudioFileTransformerProcessor::releaseResources()
{
    mBufferProcessingManager.releaseResources();
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

void AudioFileTransformerProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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
juce::AudioProcessor::BusesProperties AudioFileTransformerProcessor::_getBusesProperties()
{
    return BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioFileTransformerProcessor();
}
