#pragma once

#include "PluginProcessor.h"
#include "Audio/AudioFileProcessor.h"

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

    // File chooser
    std::unique_ptr<juce::FileChooser> fileChooser;

    // Audio processor
    AudioFileProcessor audioProcessor;

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

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioFileTransformerEditor)
};
