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
    std::unique_ptr<juce::Label> mVersionLabel;
    std::unique_ptr<juce::Slider> mGainSlider;
    std::unique_ptr<juce::Label> mGainLabel;

    // Parameter attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mGainAttachment;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioFileTransformerEditor)
};
