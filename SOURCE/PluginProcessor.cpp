#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioFileTransformerProcessor::AudioFileTransformerProcessor()
    : AudioProcessor(_getBusesProperties())
    , apvts(*this, nullptr, "Parameters", _createParameterLayout())
{
    _initParameterListeners();
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
    juce::ignoreUnused(sampleRate, samplesPerBlock);
    // Initialize your audio processing components here
}

void AudioFileTransformerProcessor::releaseResources()
{
    // Release any resources when playback stops
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
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that don't contain input data
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Your audio processing code here
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
    // Save parameters to XML
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void AudioFileTransformerProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // Restore parameters from XML
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
void AudioFileTransformerProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused(parameterID, newValue);
    // Handle parameter changes here
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout AudioFileTransformerProcessor::_createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Example parameter - add your own here
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "gain",           // Parameter ID
        "Gain",           // Parameter name
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f));           // Default value

    return layout;
}

void AudioFileTransformerProcessor::_initParameterListeners()
{
    // Register parameter listeners
    apvts.addParameterListener("gain", this);
}

juce::AudioProcessor::BusesProperties AudioFileTransformerProcessor::_getBusesProperties()
{
    return BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true);
}

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioFileTransformerProcessor();
}
