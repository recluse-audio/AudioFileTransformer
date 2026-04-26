#pragma once

#include "Util/Juce_Header.h"
#include "Processor/BufferProcessingManager.h"
#include "Processor/FileToBufferManager.h"
#include "PROCESSORS/GAIN/GainProcessor.h"
#include "PROCESSORS/GRAIN/GrainShifterProcessor.h"

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
    // Access to the swapper and its inner processors
    RD_ProcessorSwapper&   getSwapper() { return mBufferProcessingManager.getSwapper(); }
    GainProcessor*         getGainNode();
    GrainShifterProcessor* getGrainShifterNode();

    // Processor swapping (delegated to BufferProcessingManager)
    void setActiveProcessor(ActiveProcessor processor);
    ActiveProcessor getActiveProcessor() const;

    //==============================================================================
    BufferProcessingManager& getBufferProcessingManager() { return mBufferProcessingManager; }
    FileToBufferManager&     getFileToBufferManager()     { return mFileToBufferManager; }

    juce::AudioBuffer<float>& getInputBuffer()     { return mInputBuffer; }
    juce::AudioBuffer<float>& getProcessedBuffer() { return mProcessedBuffer; }

    //==============================================================================
    // Storage buffer sizing: 2ch x 60s at the maximum supported sample rate.
    static constexpr double kMaxSupportedSampleRate = 192000.0;
    static constexpr double kStorageSeconds         = 60.0;
    static constexpr int    kStorageChannels        = 2;
    static constexpr int    kStorageSamples         = static_cast<int>(kMaxSupportedSampleRate * kStorageSeconds);

private:
    //==============================================================================
    BufferProcessingManager mBufferProcessingManager;
    FileToBufferManager     mFileToBufferManager;

    juce::AudioBuffer<float> mInputBuffer     { kStorageChannels, kStorageSamples };
    juce::AudioBuffer<float> mProcessedBuffer { kStorageChannels, kStorageSamples };

    juce::AudioProcessor::BusesProperties _getBusesProperties();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioFileTransformerProcessor)
};
