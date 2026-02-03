#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Util/FileUtils.h"

//==============================================================================
AudioFileTransformerProcessor::AudioFileTransformerProcessor()
    : AudioProcessor(_getBusesProperties())
{
    // Register audio formats
    formatManager.registerBasicFormats();

    _setupProcessorGraph();
}

AudioFileTransformerProcessor::~AudioFileTransformerProcessor()
{
    mFileProcessingManager.stopProcessing();
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

GranulatorProcessor* AudioFileTransformerProcessor::getGranulatorNode()
{
    // Iterate through all nodes to find the GranulatorProcessor
    for (auto* node : processorGraph.getNodes())
    {
        if (auto* granProc = dynamic_cast<GranulatorProcessor*>(node->getProcessor()))
            return granProc;
    }
    return nullptr;
}

void AudioFileTransformerProcessor::setActiveProcessor(ActiveProcessor processor)
{
    // Always disconnect and reconnect to ensure graph is properly configured
    // Disconnect all connections between input/output and processors
    processorGraph.disconnectNode(gainNodeID);
    processorGraph.disconnectNode(granulatorNodeID);

    // Determine which processor to connect
    juce::AudioProcessorGraph::NodeID activeNodeID;
    if (processor == ActiveProcessor::Gain)
    {
        activeNodeID = gainNodeID;
        mActiveProcessor = ActiveProcessor::Gain;
    }
    else // ActiveProcessor::Granulator
    {
        activeNodeID = granulatorNodeID;
        mActiveProcessor = ActiveProcessor::Granulator;
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
// File Processing Methods
//==============================================================================

bool AudioFileTransformerProcessor::processFile(const juce::File& inputFile, const juce::File& outputFile, std::function<void(float)> progressCallback)
{
    lastError.clear();
    mInputBuffer.clear();
    mProcessedBuffer.clear();

    // Validate input file
    juce::String validationError;
    if (!FileUtils::validateInputFile(inputFile, validationError))
    {
        lastError = "Input file validation failed: " + validationError;
        return false;
    }

    // Validate output path
    if (!FileUtils::validateOutputPath(outputFile, validationError))
    {
        lastError = "Output path validation failed: " + validationError;
        return false;
    }

    // Report initial progress
    if (progressCallback)
        progressCallback(0.0f);

    // Read input file
    double sampleRate = 0.0;
    unsigned int numChannels = 0;
    unsigned int bitsPerSample = 0;

    if (!readAudioFile(inputFile, mInputBuffer, sampleRate, numChannels, bitsPerSample))
    {
        return false;
    }

    // Report progress after read
    if (progressCallback)
        progressCallback(0.3f);

    // Ensure buffer has correct channel count (stereo)
    if (mInputBuffer.getNumChannels() == 1)
    {
        // Convert mono to stereo by duplicating the channel
        juce::AudioBuffer<float> stereoBuffer(2, mInputBuffer.getNumSamples());
        stereoBuffer.copyFrom(0, 0, mInputBuffer, 0, 0, mInputBuffer.getNumSamples());
        stereoBuffer.copyFrom(1, 0, mInputBuffer, 0, 0, mInputBuffer.getNumSamples());
        mInputBuffer = stereoBuffer;
        numChannels = 2;
    }
    else if (mInputBuffer.getNumChannels() != 2)
    {
        lastError = "Only mono and stereo files are supported";
        return false;
    }

    // Prepare the processor graph for processing
    prepareToPlay(sampleRate, 512);

    // Process the audio buffer through the graph in chunks
    const int blockSize = 512;
    const int totalSamples = mInputBuffer.getNumSamples();
    juce::MidiBuffer midiBuffer;

    mProcessedBuffer.setSize(mInputBuffer.getNumChannels(), mInputBuffer.getNumSamples());

    for (int startSample = 0; startSample < totalSamples; startSample += blockSize)
    {
        int samplesToProcess = juce::jmin(blockSize, totalSamples - startSample);

        // Get write pointers for this block
        float* leftChannel = mProcessedBuffer.getWritePointer(0, startSample);
        float* rightChannel = mProcessedBuffer.getWritePointer(1, startSample);
        float* channels[2] = { leftChannel, rightChannel };

        // Create a buffer for this block
        juce::AudioBuffer<float> blockBuffer(channels, 2, samplesToProcess);

        // Process through the graph
        processBlock(blockBuffer, midiBuffer);

        // Report progress during processing
        if (progressCallback)
        {
            float progress = 0.3f + (0.4f * (float)startSample / (float)totalSamples);
            progressCallback(progress);
        }
    }

    releaseResources();

    // Report progress after processing
    if (progressCallback)
        progressCallback(0.7f);

    // Write output file
    if (!writeAudioFile(outputFile, mProcessedBuffer, sampleRate, numChannels, bitsPerSample))
    {
        return false;
    }

    // Report completion
    if (progressCallback)
        progressCallback(1.0f);

    return true;
}

juce::String AudioFileTransformerProcessor::getLastError() const
{
    return lastError;
}

void AudioFileTransformerProcessor::setInputFile(const juce::File& file)
{
    mInputFile = file;
}

void AudioFileTransformerProcessor::setOutputFile(const juce::File& file)
{
    mOutputFile = file;
}

juce::File AudioFileTransformerProcessor::getDefaultInputFile()
{
    // Try primary default location first
    auto primaryDefault = juce::File("C:\\Users\\rdeve\\Test_Vox\\Somewhere_Mono_48k.wav");
    if (primaryDefault.existsAsFile())
        return primaryDefault;

    // Fallback: try the test file in the project
    auto fallbackDefault = juce::File::getCurrentWorkingDirectory()
        .getChildFile("TESTS/TEST_FILES/Somewhere_Mono_48k.wav");

    if (fallbackDefault.existsAsFile())
        return fallbackDefault;

    // If neither exists, return the primary path anyway
    return primaryDefault;
}

juce::File AudioFileTransformerProcessor::getDefaultOutputFile()
{
    // Use desktop location for cross-platform compatibility
    auto desktopDir = juce::File::getSpecialLocation(juce::File::userDesktopDirectory);
    auto outputDir = desktopDir.getChildFile("AudioFileTransformations");

    // Ensure AudioFileTransformations directory exists on desktop
    if (!outputDir.exists())
        outputDir.createDirectory();

    // Create unique filename with timestamp: output_2026-02-01_14-30-45.wav
    auto currentTime = juce::Time::getCurrentTime();
    auto timestamp = currentTime.formatted("%Y-%m-%d_%H-%M-%S");
    auto filename = "output_" + timestamp + ".wav";

    auto defaultOutputFile = outputDir.getChildFile(filename);
    return defaultOutputFile;
}

bool AudioFileTransformerProcessor::startFileProcessing(std::function<void(float)> progressCallback)
{
    // Validate files
    if (!mInputFile.existsAsFile() || mOutputFile.getFullPathName().isEmpty())
        return false;

    // Configure and start file processing
    // The processor (this) is already configured with the active processor and parameters
    FileProcessingManager::ProcessingConfig config;
    config.inputFile = mInputFile;
    config.outputFile = mOutputFile;
    config.processor = this;
    config.progressCallback = progressCallback;

    return mFileProcessingManager.startProcessing(config);
}

void AudioFileTransformerProcessor::stopFileProcessing()
{
    mFileProcessingManager.stopProcessing();
}

bool AudioFileTransformerProcessor::isFileProcessing() const
{
    return mFileProcessingManager.isProcessing();
}

bool AudioFileTransformerProcessor::wasFileProcessingSuccessful() const
{
    return mFileProcessingManager.wasSuccessful();
}

juce::String AudioFileTransformerProcessor::getFileProcessingError() const
{
    return mFileProcessingManager.getError();
}

bool AudioFileTransformerProcessor::readAudioFile(const juce::File& file, juce::AudioBuffer<float>& buffer, 
                                                double& sampleRate,  unsigned int& numChannels, unsigned int& bitsPerSample)
{
    // Create reader for the input file
    auto reader = std::unique_ptr<juce::AudioFormatReader>(formatManager.createReaderFor(file));

    if (reader == nullptr)
    {
        lastError = "Failed to create reader for file: " + file.getFullPathName();
        return false;
    }

    // Get audio properties
    sampleRate = reader->sampleRate;
    numChannels = reader->numChannels;
    bitsPerSample = reader->bitsPerSample;

    // Allocate buffer for entire file
    buffer.setSize((int)reader->numChannels, (int)reader->lengthInSamples);

    // Read all samples into buffer
    if (!reader->read(&buffer, 0, (int)reader->lengthInSamples, 0, true, true))
    {
        lastError = "Failed to read audio data from file: " + file.getFullPathName();
        return false;
    }

    return true;
}

bool AudioFileTransformerProcessor::writeAudioFile(const juce::File& file, const juce::AudioBuffer<float>& buffer,
                                                    double sampleRate, unsigned int numChannels, unsigned int bitsPerSample)
{
    // Delete existing file if present
    if (file.existsAsFile())
    {
        if (!file.deleteFile())
        {
            lastError = "Failed to delete existing output file: " + file.getFullPathName();
            return false;
        }
    }

    // Create WAV format writer
    juce::WavAudioFormat wavFormat;
    auto fileStream = std::make_unique<juce::FileOutputStream>(file);

    if (!fileStream->openedOk())
    {
        lastError = "Failed to create output file stream: " + file.getFullPathName();
        return false;
    }

    // Create writer with same format as input
    auto writer = std::unique_ptr<juce::AudioFormatWriter>(
        wavFormat.createWriterFor(
            fileStream.release(),
            sampleRate,
            numChannels,
            bitsPerSample > 0 ? bitsPerSample : 24, // Default to 24-bit if not specified
            {},
            0
        )
    );

    if (writer == nullptr)
    {
        lastError = "Failed to create audio writer for output file: " + file.getFullPathName();
        return false;
    }

    // Write buffer to file
    if (!writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()))
    {
        lastError = "Failed to write audio data to file: " + file.getFullPathName();
        return false;
    }

    // Writer is automatically flushed and closed when it goes out of scope
    return true;
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

    // Create GainProcessor node
    auto gainProcessor = std::make_unique<GainProcessor>();
    gainProcessor->setGain(0.01f);  // Set gain to 0.01f
    gainNodeID = processorGraph.addNode(std::move(gainProcessor))->nodeID;

    // Create and add GranulatorProcessor node
    auto granulatorProcessor = std::make_unique<GranulatorProcessor>();
    granulatorNodeID = processorGraph.addNode(std::move(granulatorProcessor))->nodeID;

    // Set the active processor (connects the appropriate processor to input/output)
    setActiveProcessor(mActiveProcessor);
}

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioFileTransformerProcessor();
}
