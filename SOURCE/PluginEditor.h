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

    // Unified parameter control (changes based on active processor)
    juce::Label parameterLabel;
    juce::Slider parameterSlider;
    juce::Label parameterValueLabel;

    // File chooser
    std::unique_ptr<juce::FileChooser> fileChooser;

    // Processing state
    std::atomic<float> currentProgress { 0.0f };

    // Button callbacks
    void chooseInputFile();
    void chooseOutputFile();
    void processFile();
    void processorSelectionChanged();

    // Helper methods
    void setDefaultInputFile();
    void updateProcessButtonState();
    void updateParameterValueLabel();
    void configureParameterControlForProcessor(AudioFileTransformerProcessor::ActiveProcessor processor);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioFileTransformerEditor)
};
