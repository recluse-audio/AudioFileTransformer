#pragma once

#include "Util/Juce_Header.h"
#include "FileProcessingManager.h"
#include "../SUBMODULES/RD/SOURCE/PROCESSORS/GAIN/GainProcessor.h"
#include "../SUBMODULES/RD/SOURCE/PROCESSORS/GRAIN/GranulatorProcessor.h"

class AudioFileTransformerProcessor : public juce::AudioProcessor
{
public:
    AudioFileTransformerProcessor();
    ~AudioFileTransformerProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    // Access to processor graph nodes (for testing)
    GainProcessor* getGainNode();
    GranulatorProcessor* getGranulatorNode();

    // Processor swapping
    enum class ActiveProcessor
    {
        Gain,
        Granulator
    };

    void setActiveProcessor(ActiveProcessor processor);
    ActiveProcessor getActiveProcessor() const { return mActiveProcessor; }

    //==============================================================================
    // File processing methods
    bool processFile(
        const juce::File& inputFile,
        const juce::File& outputFile,
        std::function<void(float)> progressCallback = nullptr
    );
    juce::String getLastError() const;

    // File paths for offline processing
    void setInputFile(const juce::File& file);
    void setOutputFile(const juce::File& file);
    juce::File getInputFile() const { return mInputFile; }
    juce::File getOutputFile() const { return mOutputFile; }

    // Default file paths
    static juce::File getDefaultInputFile();
    static juce::File getDefaultOutputFile();

    // Start/stop offline file processing
    bool startFileProcessing(std::function<void(float)> progressCallback);
    void stopFileProcessing();
    bool isFileProcessing() const;
    bool wasFileProcessingSuccessful() const;
    juce::String getFileProcessingError() const;

    juce::AudioBuffer<float>& getInputBuffer() { return mInputBuffer; }
    juce::AudioBuffer<float>& getProcessedBuffer() { return mProcessedBuffer; }
private:
    //==============================================================================
    // Audio processor graph
    juce::AudioProcessorGraph processorGraph;
    juce::AudioProcessorGraph::NodeID audioInputNodeID;
    juce::AudioProcessorGraph::NodeID audioOutputNodeID;
    juce::AudioProcessorGraph::NodeID gainNodeID;
    juce::AudioProcessorGraph::NodeID granulatorNodeID;

    // Active processor tracking
    ActiveProcessor mActiveProcessor = ActiveProcessor::Granulator;

    // File processing
    juce::AudioFormatManager formatManager;
    juce::String lastError;
    FileProcessingManager mFileProcessingManager;
    juce::File mInputFile;
    juce::File mOutputFile;

    juce::AudioBuffer<float> mInputBuffer; // audio read from file, not processed yet
    juce::AudioBuffer<float> mProcessedBuffer; // results of processing of mInputBuffer
    juce::AudioProcessor::BusesProperties _getBusesProperties();
    void _setupProcessorGraph();

    // File I/O helpers
    bool readAudioFile(const juce::File& file, juce::AudioBuffer<float>& buffer,
                        double& sampleRate, unsigned int& numChannels, unsigned int& bitsPerSample);
    bool writeAudioFile(const juce::File& file, const juce::AudioBuffer<float>& buffer,
                        double sampleRate, unsigned int numChannels, unsigned int bitsPerSample);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioFileTransformerProcessor)
};
