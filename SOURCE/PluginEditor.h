#pragma once

#include "PluginProcessor.h"

class AudioFileTransformerEditor : public juce::AudioProcessorEditor
                            , public juce::Timer
{
public:
    explicit AudioFileTransformerEditor(AudioFileTransformerProcessor&);
    ~AudioFileTransformerEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

    // Timer callback for updating the UI
    void timerCallback() override;

private:
    AudioFileTransformerProcessor& mProcessor;

    // UI Components
    juce::Label inputLabel;
    juce::Label inputPathLabel;
    juce::TextButton chooseInputButton;

    juce::Label outputLabel;
    juce::Label outputPathLabel;
    juce::TextButton chooseOutputButton;

    juce::TextButton processButton;
    juce::Label statusLabel;

    // Gain control
    juce::Label gainLabel;
    juce::TextEditor gainTextEditor;

    // File chooser
    std::unique_ptr<juce::FileChooser> fileChooser;

    // Processing thread
    class ProcessingThread : public juce::Thread
    {
    public:
        ProcessingThread(float gainValue,
                        const juce::File& input,
                        const juce::File& output,
                        std::function<void(float)> callback)
            : juce::Thread("FileProcessing")
            , gain(gainValue)
            , inputFile(input)
            , outputFile(output)
            , progressCallback(callback)
        {}

        void run() override
        {
            // Create a separate processor instance for offline processing
            // This avoids conflicts with the real-time audio thread
            AudioFileTransformerProcessor offlineProcessor;

            // Set the gain value
            auto* gainNode = offlineProcessor.getGainNode();
            if (gainNode != nullptr)
                gainNode->setGain(gain);

            // Process the file
            success = offlineProcessor.processFile(inputFile, outputFile, progressCallback);
            error = offlineProcessor.getLastError();
        }

        bool wasSuccessful() const { return success; }
        juce::String getError() const { return error; }

    private:
        float gain;
        juce::File inputFile;
        juce::File outputFile;
        std::function<void(float)> progressCallback;
        bool success = false;
        juce::String error;
    };

    std::unique_ptr<ProcessingThread> processingThread;

    // Processing state
    std::atomic<bool> isProcessing { false };
    std::atomic<float> currentProgress { 0.0f };
    juce::File currentInputFile;
    juce::File currentOutputFile;

    // Button callbacks
    void chooseInputFile();
    void chooseOutputFile();
    void processFile();

    // Helper methods
    void setDefaultInputFile();
    void updateProcessButtonState();
    void updateGainFromTextEditor();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioFileTransformerEditor)
};
