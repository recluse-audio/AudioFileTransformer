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

    // Set default output path
    currentOutputFile = juce::File("C:\\REPOS\\PLUGIN_PROJECTS\\AudioFileTransformer\\RENDERED_AUDIO\\output.wav");

    // Ensure RENDERED_AUDIO directory exists
    auto outputDir = currentOutputFile.getParentDirectory();
    if (!outputDir.exists())
        outputDir.createDirectory();

    outputPathLabel.setText(currentOutputFile.getFullPathName(), juce::dontSendNotification);
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
    processorSelector.setSelectedId(2);  // Default to Granulator
    processorSelector.onChange = [this]() { processorSelectionChanged(); };
    addAndMakeVisible(processorSelector);

    // Gain control (kept but not used)
    gainLabel.setText("Gain (0.0 - 1.0):", juce::dontSendNotification);
    gainLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    // addAndMakeVisible(gainLabel);  // Hidden since we're using granulator now

    gainTextEditor.setText("0.01");
    gainTextEditor.setJustification(juce::Justification::centred);
    gainTextEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::darkgrey);
    gainTextEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    gainTextEditor.setColour(juce::TextEditor::outlineColourId, juce::Colours::grey);
    gainTextEditor.onReturnKey = [this]() { updateGainFromTextEditor(); };
    gainTextEditor.onFocusLost = [this]() { updateGainFromTextEditor(); };
    // addAndMakeVisible(gainTextEditor);  // Hidden since we're using granulator now

    // Shift ratio control
    shiftRatioLabel.setText("Pitch Shift Ratio (0.5 = octave down, 1.0 = no shift, 1.5 = fifth up):", juce::dontSendNotification);
    shiftRatioLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    addAndMakeVisible(shiftRatioLabel);

    shiftRatioSlider.setRange(0.5, 1.5, 0.01);
    shiftRatioSlider.setValue(1.0);  // Default: no shift
    shiftRatioSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    shiftRatioSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    shiftRatioSlider.onValueChange = [this]() { updateShiftRatioValueLabel(); };
    addAndMakeVisible(shiftRatioSlider);

    shiftRatioValueLabel.setText("1.00", juce::dontSendNotification);
    shiftRatioValueLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    shiftRatioValueLabel.setJustificationType(juce::Justification::centred);
    shiftRatioValueLabel.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    shiftRatioValueLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(shiftRatioValueLabel);

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

    // Stop processing thread if running
    if (processingThread != nullptr)
    {
        processingThread->stopThread(5000);
        processingThread.reset();
    }
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

    // Shift ratio control
    shiftRatioLabel.setBounds(bounds.removeFromTop(25));
    auto shiftRatioRow = bounds.removeFromTop(80);

    // Center the knob and value label
    auto knobArea = shiftRatioRow.removeFromLeft(100);
    shiftRatioSlider.setBounds(knobArea.removeFromTop(80));

    shiftRatioRow.removeFromLeft(20); // Spacing
    auto valueLabelArea = shiftRatioRow.removeFromLeft(80);
    shiftRatioValueLabel.setBounds(valueLabelArea.withTrimmedTop(25));

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
    if (isProcessing.load())
    {
        float progress = currentProgress.load();
        int percent = static_cast<int>(progress * 100.0f);
        statusLabel.setText("Processing... " + juce::String(percent) + "%", juce::dontSendNotification);

        // Check if processing thread finished
        if (processingThread != nullptr && !processingThread->isThreadRunning())
        {
            bool success = processingThread->wasSuccessful();
            juce::String error = processingThread->getError();

            processingThread.reset();
            isProcessing.store(false);
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
            currentInputFile = file;
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
            currentOutputFile = file;
            outputPathLabel.setText(file.getFullPathName(), juce::dontSendNotification);
            updateProcessButtonState();
        }
    });
}

void AudioFileTransformerEditor::processFile()
{
    if (isProcessing.load() || !currentInputFile.existsAsFile() || currentOutputFile.getFullPathName().isEmpty())
        return;

    isProcessing.store(true);
    processButton.setEnabled(false);
    statusLabel.setText("Processing... 0%", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightblue);

    // Get current processor settings
    auto activeProc = mProcessor.getActiveProcessor();
    float currentGain = gainTextEditor.getText().getFloatValue();
    float currentShiftRatio = static_cast<float>(shiftRatioSlider.getValue());

    // Create and start processing thread with a separate processor instance
    processingThread = std::make_unique<ProcessingThread>(
        activeProc,
        currentGain,
        currentShiftRatio,
        currentInputFile,
        currentOutputFile,
        [this](float progress) {
            currentProgress.store(progress);
        }
    );

    processingThread->startThread();
}

void AudioFileTransformerEditor::setDefaultInputFile()
{
    // Set default input file
    auto defaultInputFile = juce::File("C:\\Users\\rdeve\\Test_Vox\\Somewhere_Mono_48k.wav");

    if (defaultInputFile.existsAsFile())
    {
        currentInputFile = defaultInputFile;
        inputPathLabel.setText(defaultInputFile.getFullPathName(), juce::dontSendNotification);
        updateProcessButtonState();
    }
    else
    {
        // Fallback: try the test file
        auto testFile = juce::File::getCurrentWorkingDirectory()
            .getChildFile("TESTS/TEST_FILES/Somewhere_Mono_48k.wav");

        if (testFile.existsAsFile())
        {
            currentInputFile = testFile;
            inputPathLabel.setText(testFile.getFullPathName(), juce::dontSendNotification);
            updateProcessButtonState();
        }
    }
}

void AudioFileTransformerEditor::updateProcessButtonState()
{
    bool canProcess = currentInputFile.existsAsFile()
                   && currentOutputFile.getFullPathName().isNotEmpty()
                   && !isProcessing.load();
    processButton.setEnabled(canProcess);
}

void AudioFileTransformerEditor::updateGainFromTextEditor()
{
    auto text = gainTextEditor.getText();
    float gainValue = text.getFloatValue();

    // Clamp to valid range (0.0 - 1.0)
    gainValue = juce::jlimit(0.0f, 1.0f, gainValue);

    // Update the text editor with the clamped value
    gainTextEditor.setText(juce::String(gainValue, 3), juce::dontSendNotification);

    // Update the gain processor
    auto* gainNode = mProcessor.getGainNode();
    if (gainNode != nullptr)
    {
        gainNode->setGain(gainValue);
    }
}

void AudioFileTransformerEditor::updateShiftRatioValueLabel()
{
    float shiftRatio = static_cast<float>(shiftRatioSlider.getValue());
    shiftRatioValueLabel.setText(juce::String(shiftRatio, 2), juce::dontSendNotification);

    // Update the granulator processor in real-time
    auto* granulatorNode = mProcessor.getGranulatorNode();
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

void AudioFileTransformerEditor::processorSelectionChanged()
{
    int selectedId = processorSelector.getSelectedId();

    if (selectedId == 1) // Gain Processor
    {
        mProcessor.setActiveProcessor(AudioFileTransformerProcessor::ActiveProcessor::Gain);

        // Show gain controls, hide shift ratio controls
        gainLabel.setVisible(true);
        gainTextEditor.setVisible(true);
        shiftRatioLabel.setVisible(false);
        shiftRatioSlider.setVisible(false);
        shiftRatioValueLabel.setVisible(false);
    }
    else if (selectedId == 2) // Granulator Processor
    {
        mProcessor.setActiveProcessor(AudioFileTransformerProcessor::ActiveProcessor::Granulator);

        // Hide gain controls, show shift ratio controls
        gainLabel.setVisible(false);
        gainTextEditor.setVisible(false);
        shiftRatioLabel.setVisible(true);
        shiftRatioSlider.setVisible(true);
        shiftRatioValueLabel.setVisible(true);
    }
}
