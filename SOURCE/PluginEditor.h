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

    // Processor selection
    juce::Label processorLabel;
    juce::ComboBox processorSelector;

    // Gain control (kept for reference)
    juce::Label gainLabel;
    juce::TextEditor gainTextEditor;

    // Shift ratio control
    juce::Label shiftRatioLabel;
    juce::Slider shiftRatioSlider;
    juce::Label shiftRatioValueLabel;

    // File chooser
    std::unique_ptr<juce::FileChooser> fileChooser;

    // Processing thread
    class ProcessingThread : public juce::Thread
    {
    public:
        ProcessingThread(AudioFileTransformerProcessor::ActiveProcessor activeProc,
                        float gainValue,
                        float shiftRatioValue,
                        const juce::File& input,
                        const juce::File& output,
                        std::function<void(float)> callback)
            : juce::Thread("FileProcessing")
            , activeProcessor(activeProc)
            , gain(gainValue)
            , shiftRatio(shiftRatioValue)
            , inputFile(input)
            , outputFile(output)
            , progressCallback(callback)
        {}

        void run() override
        {
            // Create a separate processor instance for offline processing
            // This avoids conflicts with the real-time audio thread
            AudioFileTransformerProcessor offlineProcessor;

            // Set the active processor to match the UI selection
            offlineProcessor.setActiveProcessor(activeProcessor);

            // Set parameters based on active processor
            if (activeProcessor == AudioFileTransformerProcessor::ActiveProcessor::Gain)
            {
                auto* gainNode = offlineProcessor.getGainNode();
                if (gainNode != nullptr)
                    gainNode->setGain(gain);
            }
            else // Granulator
            {
                auto* granulatorNode = offlineProcessor.getGranulatorNode();
                if (granulatorNode != nullptr)
                {
                    auto* param = granulatorNode->getAPVTS().getParameter("shiftRatio");
                    if (param != nullptr)
                    {
                        // Convert shift ratio (0.5 to 1.5) to normalized value (0.0 to 1.0)
                        float normalizedValue = (shiftRatio - 0.5f) / 1.0f;
                        param->setValueNotifyingHost(normalizedValue);
                    }
                }
            }

            // Process the file
            success = offlineProcessor.processFile(inputFile, outputFile, progressCallback);
            error = offlineProcessor.getLastError();
        }

        bool wasSuccessful() const { return success; }
        juce::String getError() const { return error; }

    private:
        AudioFileTransformerProcessor::ActiveProcessor activeProcessor;
        float gain;
        float shiftRatio;
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
    void processorSelectionChanged();

    // Helper methods
    void setDefaultInputFile();
    void updateProcessButtonState();
    void updateGainFromTextEditor();
    void updateShiftRatioValueLabel();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioFileTransformerEditor)
};
