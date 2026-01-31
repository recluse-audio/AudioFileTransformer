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

    outputPathLabel.setText("output.wav", juce::dontSendNotification);
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

    // Set window size
    setSize(600, 300);

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

    bounds.removeFromTop(30); // Spacing

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

    // Process in background thread
    std::thread([this]() {
        bool success = audioProcessor.processFile(
            currentInputFile,
            currentOutputFile,
            [this](float progress) {
                currentProgress.store(progress);
            }
        );

        // Update UI on completion
        juce::MessageManager::callAsync([this, success]() {
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
                statusLabel.setText("Error: " + audioProcessor.getLastError(), juce::dontSendNotification);
                statusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
            }
        });
    }).detach();
}

void AudioFileTransformerEditor::setDefaultInputFile()
{
    // Try to find the default test file
    auto testFile = juce::File::getCurrentWorkingDirectory()
        .getChildFile("TESTS/TEST_FILES/Somewhere_Mono_48k.wav");

    if (!testFile.existsAsFile())
    {
        // Try relative to executable
        testFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
            .getParentDirectory()
            .getChildFile("TESTS/TEST_FILES/Somewhere_Mono_48k.wav");
    }

    if (testFile.existsAsFile())
    {
        currentInputFile = testFile;
        inputPathLabel.setText(testFile.getFullPathName(), juce::dontSendNotification);
        updateProcessButtonState();
    }
}

void AudioFileTransformerEditor::updateProcessButtonState()
{
    bool canProcess = currentInputFile.existsAsFile()
                   && currentOutputFile.getFullPathName().isNotEmpty()
                   && !isProcessing.load();
    processButton.setEnabled(canProcess);
}
