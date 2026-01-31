#include "PluginEditor.h"
#include "Util/Version.h"

//==============================================================================
AudioFileTransformerEditor::AudioFileTransformerEditor(AudioFileTransformerProcessor& p)
    : AudioProcessorEditor(&p)
    , mProcessor(p)
{
    // Version label
    mVersionLabel = std::make_unique<juce::Label>("version", "v" BUILD_VERSION_STRING);
    mVersionLabel->setFont(juce::Font(10.0f));
    addAndMakeVisible(mVersionLabel.get());

    // Gain slider
    mGainSlider = std::make_unique<juce::Slider>(juce::Slider::Rotary, juce::Slider::TextBoxBelow);
    mGainSlider->setTextValueSuffix("");
    addAndMakeVisible(mGainSlider.get());

    // Gain label
    mGainLabel = std::make_unique<juce::Label>("gain_label", "Gain");
    mGainLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(mGainLabel.get());

    // Attach slider to parameter
    mGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        mProcessor.getAPVTS(), "gain", *mGainSlider);

    // Start timer for UI updates (30 Hz)
    startTimerHz(30);

    // Set window size
    setSize(400, 300);
}

AudioFileTransformerEditor::~AudioFileTransformerEditor()
{
    stopTimer();
}

//==============================================================================
void AudioFileTransformerEditor::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(juce::Colour(0xff202020));

    // Title
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(24.0f, juce::Font::bold));
    g.drawText("AudioFileTransformer", getLocalBounds().removeFromTop(60), juce::Justification::centred);
}

void AudioFileTransformerEditor::resized()
{
    auto bounds = getLocalBounds();

    // Version label in bottom left
    mVersionLabel->setBounds(10, getHeight() - 20, 100, 12);

    // Center gain control
    auto centerArea = bounds.reduced(50);
    mGainLabel->setBounds(centerArea.removeFromTop(30));
    mGainSlider->setBounds(centerArea.removeFromTop(100).withSizeKeepingCentre(100, 100));
}

void AudioFileTransformerEditor::timerCallback()
{
    // Update UI elements that need periodic refresh
}
