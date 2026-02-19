#include "PluginEditor.h"
#include "Util/Version.h"

AudioFileTransformerEditor::AudioFileTransformerEditor(AudioFileTransformerProcessor& p)
    : AudioProcessorEditor(&p)
    , mProcessor(p)
{
    // Input file section
    inputLabel.setText("Input File:", juce::dontSendNotification);
    inputLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    addAndMakeVisible(inputLabel);

    inputPathLabel.setText("No file selected", juce::dontSendNotification);
    inputPathLabel.setColour(juce::Label::backgroundColourId, juce::Colours::darkgrey);
    inputPathLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    inputPathLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(inputPathLabel);

    chooseInputButton.setButtonText("Choose Input...");
    chooseInputButton.onClick = [this]() { chooseInputFile(); };
    addAndMakeVisible(chooseInputButton);

    // Output file section
    outputLabel.setText("Output File:", juce::dontSendNotification);
    outputLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    addAndMakeVisible(outputLabel);

    // Set default output path in processor
    auto defaultOutputFile = AudioFileTransformerProcessor::getDefaultOutputFile();
    mProcessor.setOutputFile(defaultOutputFile);
    outputPathLabel.setText(defaultOutputFile.getFullPathName(), juce::dontSendNotification);
    outputPathLabel.setColour(juce::Label::backgroundColourId, juce::Colours::darkgrey);
    outputPathLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    outputPathLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(outputPathLabel);

    chooseOutputButton.setButtonText("Choose Output...");
    chooseOutputButton.onClick = [this]() { chooseOutputFile(); };
    addAndMakeVisible(chooseOutputButton);

    // Process button
    processButton.setButtonText("Process File");
    processButton.onClick = [this]() { processFile(); };
    processButton.setEnabled(false);
    addAndMakeVisible(processButton);

    // Status label
    statusLabel.setText("Ready", juce::dontSendNotification);
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    addAndMakeVisible(statusLabel);

    // Processor selection
    processorLabel.setText("Active Processor:", juce::dontSendNotification);
    processorLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    addAndMakeVisible(processorLabel);

    processorSelector.addItem("Gain Processor", 1);
    processorSelector.addItem("Granulator Processor (Pitch Shift)", 2);
    processorSelector.addItem("TDPSOLA Processor (Pitch Shift)", 3);
    processorSelector.setSelectedId(3);  // Default to TDPSOLA
    processorSelector.onChange = [this]() { processorSelectionChanged(); };
    addAndMakeVisible(processorSelector);

    // Unified parameter control
    parameterLabel.setText("Parameter:", juce::dontSendNotification);
    parameterLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    addAndMakeVisible(parameterLabel);

    parameterSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    parameterSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    parameterSlider.onValueChange = [this]() { updateParameterValueLabel(); };
    addAndMakeVisible(parameterSlider);

    parameterValueLabel.setText("1.00", juce::dontSendNotification);
    parameterValueLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    parameterValueLabel.setJustificationType(juce::Justification::centred);
    parameterValueLabel.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    parameterValueLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(parameterValueLabel);

    // Configure parameter control for default processor (TDPSOLA)
    configureParameterControlForProcessor(ActiveProcessor::TDPSOLA);

    // Set window size
    setSize(600, 450);

    // Set default input file
    setDefaultInputFile();

    // Start timer for UI updates
    startTimerHz(10);
}

AudioFileTransformerEditor::~AudioFileTransformerEditor()
{
    stopTimer();
}

void AudioFileTransformerEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff303030));

    // Title
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(20.0f, juce::Font::bold));
    g.drawText("Audio File Transformer", 10, 10, getWidth() - 20, 30, juce::Justification::centred);

    // Version
    g.setFont(juce::Font(10.0f));
    g.setColour(juce::Colours::grey);
    g.drawText("v" BUILD_VERSION_STRING, 10, getHeight() - 20, 100, 15, juce::Justification::centredLeft);
}

void AudioFileTransformerEditor::resized()
{
    auto bounds = getLocalBounds().reduced(20);
    bounds.removeFromTop(50); // Space for title

    // Input file section
    inputLabel.setBounds(bounds.removeFromTop(25));
    auto inputRow = bounds.removeFromTop(30);
    chooseInputButton.setBounds(inputRow.removeFromRight(120));
    inputRow.removeFromRight(10); // Spacing
    inputPathLabel.setBounds(inputRow);

    bounds.removeFromTop(20); // Spacing

    // Output file section
    outputLabel.setBounds(bounds.removeFromTop(25));
    auto outputRow = bounds.removeFromTop(30);
    chooseOutputButton.setBounds(outputRow.removeFromRight(120));
    outputRow.removeFromRight(10); // Spacing
    outputPathLabel.setBounds(outputRow);

    bounds.removeFromTop(20); // Spacing

    // Processor selection
    processorLabel.setBounds(bounds.removeFromTop(25));
    auto processorRow = bounds.removeFromTop(30);
    processorSelector.setBounds(processorRow.removeFromLeft(300));

    bounds.removeFromTop(20); // Spacing

    // Parameter control (unified for both processors)
    parameterLabel.setBounds(bounds.removeFromTop(25));
    auto parameterRow = bounds.removeFromTop(80);

    // Center the knob and value label
    auto knobArea = parameterRow.removeFromLeft(100);
    parameterSlider.setBounds(knobArea.removeFromTop(80));

    parameterRow.removeFromLeft(20); // Spacing
    auto valueLabelArea = parameterRow.removeFromLeft(80);
    parameterValueLabel.setBounds(valueLabelArea.withTrimmedTop(25));

    bounds.removeFromTop(20); // Spacing

    // Process button
    processButton.setBounds(bounds.removeFromTop(40).reduced(100, 0));

    bounds.removeFromTop(20); // Spacing

    // Status label
    statusLabel.setBounds(bounds.removeFromTop(30));
}

void AudioFileTransformerEditor::timerCallback()
{
    // Update progress if processing
    if (mProcessor.isFileProcessing())
    {
        float progress = currentProgress.load();
        int percent = static_cast<int>(progress * 100.0f);
        statusLabel.setText("Processing... " + juce::String(percent) + "%", juce::dontSendNotification);
    }
    else if (currentProgress.load() > 0.0f)
    {
        // Processing just finished
        bool success = mProcessor.wasFileProcessingSuccessful();
        juce::String error = mProcessor.getFileProcessingError();

        currentProgress.store(0.0f);
        processButton.setEnabled(true);

        if (success)
        {
            statusLabel.setText("Processing complete!", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
        }
        else
        {
            statusLabel.setText("Error: " + error, juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
        }
    }
}

void AudioFileTransformerEditor::chooseInputFile()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select an audio file to process",
        juce::File(),
        "*.wav;*.mp3"
    );

    auto flags = juce::FileBrowserComponent::openMode
               | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc) {
        auto file = fc.getResult();
        if (file.existsAsFile())
        {
            mProcessor.setInputFile(file);
            inputPathLabel.setText(file.getFullPathName(), juce::dontSendNotification);
            updateProcessButtonState();
        }
    });
}

void AudioFileTransformerEditor::chooseOutputFile()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select output location",
        juce::File(),
        "*.wav"
    );

    auto flags = juce::FileBrowserComponent::saveMode
               | juce::FileBrowserComponent::canSelectFiles
               | juce::FileBrowserComponent::warnAboutOverwriting;

    fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc) {
        auto file = fc.getResult();
        if (file.getFullPathName().isNotEmpty())
        {
            mProcessor.setOutputFile(file);
            outputPathLabel.setText(file.getFullPathName(), juce::dontSendNotification);
            updateProcessButtonState();
        }
    });
}

void AudioFileTransformerEditor::processFile()
{
    // Simply tell the processor that the process button was clicked
    processButton.setEnabled(false);
    statusLabel.setText("Processing... 0%", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightblue);

    mProcessor.startFileProcessing([this](float progress) {
        currentProgress.store(progress);
    });
}

void AudioFileTransformerEditor::setDefaultInputFile()
{
    // Get default input file from processor
    auto defaultInputFile = AudioFileTransformerProcessor::getDefaultInputFile();

    if (defaultInputFile.existsAsFile())
    {
        mProcessor.setInputFile(defaultInputFile);
        inputPathLabel.setText(defaultInputFile.getFullPathName(), juce::dontSendNotification);
        updateProcessButtonState();
    }
}

void AudioFileTransformerEditor::updateProcessButtonState()
{
    bool canProcess = mProcessor.getInputFile().existsAsFile()
                   && mProcessor.getOutputFile().getFullPathName().isNotEmpty()
                   && !mProcessor.isFileProcessing();
    processButton.setEnabled(canProcess);
}

void AudioFileTransformerEditor::updateParameterValueLabel()
{
    float paramValue = static_cast<float>(parameterSlider.getValue());
    auto activeProc = mProcessor.getActiveProcessor();

    if (activeProc == ActiveProcessor::Gain)
    {
        parameterValueLabel.setText(juce::String(paramValue, 3), juce::dontSendNotification);

        auto* gainNode = mProcessor.getGainNode();
        if (gainNode != nullptr)
            gainNode->setGain(paramValue);
    }
    else if (activeProc == ActiveProcessor::Granulator)
    {
        parameterValueLabel.setText(juce::String(paramValue, 2), juce::dontSendNotification);

        auto* granulatorNode = mProcessor.getGranulatorNode();
        if (granulatorNode != nullptr)
        {
            auto* param = granulatorNode->getAPVTS().getParameter("shift ratio");
            if (param != nullptr)
            {
                float normalizedValue = (paramValue - 0.5f) / 1.0f;
                param->setValueNotifyingHost(normalizedValue);
            }
        }
    }
    else // ActiveProcessor::TDPSOLA — slider is bound via SliderAttachment; just update label
    {
        parameterValueLabel.setText(juce::String(paramValue, 2), juce::dontSendNotification);
    }
}

void AudioFileTransformerEditor::configureParameterControlForProcessor(ActiveProcessor processor)
{
    // Drop any existing APVTS attachment before reconfiguring the slider range.
    mParamAttachment.reset();

    if (processor == ActiveProcessor::Gain)
    {
        parameterLabel.setText("Gain (0.0 = silent, 1.0 = full volume):", juce::dontSendNotification);
        parameterSlider.setRange(0.0, 1.0, 0.001);
        parameterSlider.setValue(0.5, juce::dontSendNotification);
        parameterValueLabel.setText("0.500", juce::dontSendNotification);
    }
    else if (processor == ActiveProcessor::Granulator)
    {
        parameterLabel.setText("Pitch Shift Ratio (0.5 = octave down, 1.0 = no shift, 1.5 = fifth up):", juce::dontSendNotification);
        parameterSlider.setRange(0.5, 1.5, 0.01);
        parameterSlider.setValue(1.0, juce::dontSendNotification);
        parameterValueLabel.setText("1.00", juce::dontSendNotification);
    }
    else // ActiveProcessor::TDPSOLA
    {
        parameterLabel.setText("Shift Ratio (0.5 = octave down, 1.0 = no shift, 2.0 = octave up):", juce::dontSendNotification);
        parameterValueLabel.setText("1.00", juce::dontSendNotification);

        // Attach the slider directly to the TDPSOLA processor's APVTS.
        auto* tdpsolaNode = mProcessor.getTDPSOLANode();
        if (tdpsolaNode != nullptr)
        {
            mParamAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                tdpsolaNode->getAPVTS(), "shift_ratio", parameterSlider);
        }
    }
}

void AudioFileTransformerEditor::processorSelectionChanged()
{
    int selectedId = processorSelector.getSelectedId();
    ActiveProcessor newProcessor;

    if (selectedId == 1)
        newProcessor = ActiveProcessor::Gain;
    else if (selectedId == 2)
        newProcessor = ActiveProcessor::Granulator;
    else
        newProcessor = ActiveProcessor::TDPSOLA;

    mProcessor.setActiveProcessor(newProcessor);
    configureParameterControlForProcessor(newProcessor);
    updateParameterValueLabel();
}
