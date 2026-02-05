#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Util/FileUtils.h"

//==============================================================================
AudioFileTransformerProcessor::AudioFileTransformerProcessor()
    : AudioProcessor(_getBusesProperties())
{
    // Register audio formats
    formatManager.registerBasicFormats();

    // TEMPORARY: Create direct processor instance for debugging
    // mTestGranulator = std::make_unique<GranulatorProcessor>();
    mTestGain = std::make_unique<GainProcessor>();
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
    // TEMPORARY: Prepare direct processor instance
    // if (mTestGranulator)
    //     mTestGranulator->prepareToPlay(sampleRate, samplesPerBlock);
    if (mTestGain)
    {
        mTestGain->prepareToPlay(sampleRate, samplesPerBlock);
        mTestGain->setGain(2.0f); // 2x boost - should be LOUD and obvious
    }

    // Update the main processor's reported latency after child processors are prepared
    if (getActiveProcessor() == ActiveProcessor::Gain)
    {
        auto* gainNode = getGainNode();
        if (gainNode)
            setLatencySamples(gainNode->getLatencySamples());
    }
    else // ActiveProcessor::Granulator
    {
        auto* granulatorNode = getGranulatorNode();
        if (granulatorNode)
            setLatencySamples(granulatorNode->getLatencySamples());
    }
}

void AudioFileTransformerProcessor::releaseResources()
{
    // TEMPORARY: Release direct processor instance
    // if (mTestGranulator)
    //     mTestGranulator->releaseResources();
    if (mTestGain)
        mTestGain->releaseResources();
}

bool AudioFileTransformerProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Support mono or stereo
    auto inputChannels = layouts.getMainInputChannelSet();
    auto outputChannels = layouts.getMainOutputChannelSet();

    // Input can be mono or stereo
    if (inputChannels != juce::AudioChannelSet::mono() &&
        inputChannels != juce::AudioChannelSet::stereo())
        return false;

    // Output can be mono or stereo
    if (outputChannels != juce::AudioChannelSet::mono() &&
        outputChannels != juce::AudioChannelSet::stereo())
        return false;

    // Input and output must match
    if (inputChannels != outputChannels)
        return false;

    return true;
}

void AudioFileTransformerProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    // This plugin is designed for offline file processing only.
    // Real-time audio processing is not supported - clear buffer to output silence.
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
    // No state to save - parameters are in individual nodes
}

void AudioFileTransformerProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::ignoreUnused(data, sizeInBytes);
    // No state to restore - parameters are in individual nodes
}

GainProcessor* AudioFileTransformerProcessor::getGainNode()
{
    return mBufferProcessingManager.getGainNode();
}

GranulatorProcessor* AudioFileTransformerProcessor::getGranulatorNode()
{
    return mBufferProcessingManager.getGranulatorNode();
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


    // Read input file
    double sampleRate = 0.0;
    unsigned int numChannels = 0;
    unsigned int bitsPerSample = 0;

    if (!readAudioFile(inputFile, mInputBuffer, sampleRate, numChannels, bitsPerSample))
    {
        return false;
    }


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

    // Calculate output size: input + latency + tail
    const int blockSize = 512;
    const int inputSamples = mInputBuffer.getNumSamples();

    int latencySamples = 0;
    double tailLengthSeconds = 0.0;

    if (getActiveProcessor() == ActiveProcessor::Gain)
    {
        auto* gainNode = getGainNode();
        if (gainNode)
        {
            latencySamples = gainNode->getLatencySamples();
            tailLengthSeconds = gainNode->getTailLengthSeconds();
        }
    }
    else if (getActiveProcessor() == ActiveProcessor::Granulator)
    {
        auto* granulatorNode = getGranulatorNode();
        if (granulatorNode)
        {
            latencySamples = granulatorNode->getLatencySamples();
            tailLengthSeconds = granulatorNode->getTailLengthSeconds();
        }
    }

    const int tailSamples = (tailLengthSeconds > 0) ? static_cast<int>(tailLengthSeconds * sampleRate) : 0;
    const int totalOutputSamples = inputSamples + latencySamples + tailSamples;

    // Size output buffer to accommodate input + latency + tail
    mProcessedBuffer.setSize(mInputBuffer.getNumChannels(), totalOutputSamples);
    mProcessedBuffer.clear();

    // Process buffers using BufferProcessingManager (non-realtime, off audio thread)
    if (!mBufferProcessingManager.processBuffers(mInputBuffer, mProcessedBuffer, sampleRate, blockSize, progressCallback))
    {
        lastError = "Buffer processing failed: " + mBufferProcessingManager.getLastError();
        return false;
    }

    if (!writeAudioFile(outputFile, mProcessedBuffer, sampleRate, numChannels, bitsPerSample))
    {
        return false;
    }

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

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioFileTransformerProcessor();
}
