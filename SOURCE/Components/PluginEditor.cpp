#include "Components/PluginEditor.h"
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

    // Output directory section
    outputLabel.setText("Output Directory:", juce::dontSendNotification);
    outputLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    addAndMakeVisible(outputLabel);

    auto defaultOutputDir = FileToBufferManager::getDefaultOutputDirectory();
    mProcessor.getFileToBufferManager().setOutputDirectory(defaultOutputDir);
    outputPathLabel.setText(defaultOutputDir.getFullPathName(), juce::dontSendNotification);
    outputPathLabel.setColour(juce::Label::backgroundColourId, juce::Colours::darkgrey);
    outputPathLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    outputPathLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(outputPathLabel);

    chooseOutputButton.setButtonText("Choose Output Dir...");
    chooseOutputButton.onClick = [this]() { chooseOutputDirectory(); };
    addAndMakeVisible(chooseOutputButton);

    // Process button
    processButton.setButtonText("Process File");
    processButton.onClick = [this]() { processFile(); };
    processButton.setEnabled(false);
    addAndMakeVisible(processButton);

    // Logging toggle
    loggingToggle.setButtonText("Enable Data Logging");
    loggingToggle.setToggleState(mProcessor.getIsLogging(), juce::dontSendNotification);
    loggingToggle.onClick = [this]()
    {
        if (loggingToggle.getToggleState())
            mProcessor.startLogging();
        else
            mProcessor.stopLogging();
    };
    addAndMakeVisible(loggingToggle);

    // Per-block CSV logging toggle — independent of main logging flag.
    // Cascades through RD_Processor children only (swapper + active processor);
    // PitchManager / Granulator are unaffected.
    blockLoggingToggle.setButtonText("Per-Block CSV Logging");
    blockLoggingToggle.setToggleState(mProcessor.getIsBlockLogging(), juce::dontSendNotification);
    blockLoggingToggle.onClick = [this]()
    {
        mProcessor.setIsBlockLogging(blockLoggingToggle.getToggleState());
    };
    addAndMakeVisible(blockLoggingToggle);

    // Block-size selector — powers of 2, 32..4096.
    blockSizeLabel.setText("Block Size:", juce::dontSendNotification);
    blockSizeLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    addAndMakeVisible(blockSizeLabel);

    {
        const int currentBlockSize = mProcessor.getBufferProcessingManager().getBlockSize();
        int idToSelect = 0;
        for (int v = 32, id = 1; v <= 4096; v <<= 1, ++id)
        {
            blockSizeSelector.addItem(juce::String(v), id);
            if (v == currentBlockSize) idToSelect = id;
        }
        if (idToSelect == 0) idToSelect = 5; // 512 fallback
        blockSizeSelector.setSelectedId(idToSelect, juce::dontSendNotification);
    }
    blockSizeSelector.onChange = [this]()
    {
        const int chosen = blockSizeSelector.getText().getIntValue();
        mProcessor.getBufferProcessingManager().setBlockSize(chosen);
    };
    addAndMakeVisible(blockSizeSelector);

    // GrainShifter parameter controls — name on top, value box, horizontal slider.
    auto setupParamSlider = [this] (juce::Slider& s, juce::Label& nameLbl, juce::Label& valLbl,
                                    const juce::String& name)
    {
        nameLbl.setText(name, juce::dontSendNotification);
        nameLbl.setFont(juce::Font(13.0f, juce::Font::bold));
        nameLbl.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(nameLbl);

        valLbl.setColour(juce::Label::backgroundColourId, juce::Colours::black);
        valLbl.setColour(juce::Label::textColourId,       juce::Colours::white);
        valLbl.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(valLbl);

        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        addAndMakeVisible(s);
    };

    setupParamSlider(pitchWindowSlider,    pitchWindowLabel,    pitchWindowValueLabel,    "Pitch Window Size");
    setupParamSlider(pitchHopSlider,       pitchHopLabel,       pitchHopValueLabel,       "Pitch Hop Size");
    setupParamSlider(pitchThresholdSlider, pitchThresholdLabel, pitchThresholdValueLabel, "Detection Threshold");

    // Window/hop are AudioParameterChoice (powers of 2). SliderAttachment maps slider
    // value to choice index — lock to integer step. textFromValueFunction renders the
    // actual sample-count integer (not the index).
    static const juce::StringArray kWindowChoices { "512", "1024", "2048", "4096", "8192" };
    static const juce::StringArray kHopChoices    { "256", "512", "1024", "2048", "4096" };

    pitchWindowSlider.setRange(0.0, (double) (kWindowChoices.size() - 1), 1.0);
    pitchWindowSlider.textFromValueFunction = [] (double v)
    {
        const int idx = juce::jlimit(0, kWindowChoices.size() - 1, (int) std::round(v));
        return kWindowChoices[idx];
    };
    pitchWindowSlider.onValueChange = [this]()
    {
        pitchWindowValueLabel.setText(pitchWindowSlider.getTextFromValue(pitchWindowSlider.getValue()),
                                      juce::dontSendNotification);
    };

    pitchHopSlider.setRange(0.0, (double) (kHopChoices.size() - 1), 1.0);
    pitchHopSlider.textFromValueFunction = [] (double v)
    {
        const int idx = juce::jlimit(0, kHopChoices.size() - 1, (int) std::round(v));
        return kHopChoices[idx];
    };
    pitchHopSlider.onValueChange = [this]()
    {
        pitchHopValueLabel.setText(pitchHopSlider.getTextFromValue(pitchHopSlider.getValue()),
                                   juce::dontSendNotification);
    };

    pitchThresholdSlider.setRange(0.0, 1.0, 0.01);
    pitchThresholdSlider.onValueChange = [this]()
    {
        pitchThresholdValueLabel.setText(juce::String(pitchThresholdSlider.getValue(), 2),
                                         juce::dontSendNotification);
    };

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
    processorSelector.addItem("Grain Shifter (TD-PSOLA)", 2);
    processorSelector.setSelectedId(2);  // Default to GrainShifter
    processorSelector.onChange = [this]() { processorSelectionChanged(); };
    addAndMakeVisible(processorSelector);

    // Unified parameter control
    parameterLabel.setText("Parameter:", juce::dontSendNotification);
    parameterLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    addAndMakeVisible(parameterLabel);

    parameterSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    parameterSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    parameterSlider.onValueChange = [this]() { updateParameterValueLabel(); };
    addAndMakeVisible(parameterSlider);

    parameterValueLabel.setText("1.00", juce::dontSendNotification);
    parameterValueLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    parameterValueLabel.setJustificationType(juce::Justification::centred);
    parameterValueLabel.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    parameterValueLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(parameterValueLabel);

    // Configure parameter control for default processor (Grain Shifter)
    mProcessor.setActiveProcessor(ActiveProcessor::kGrainShifter);
    configureParameterControlForProcessor(ActiveProcessor::kGrainShifter);

    // Set window size
    setSize(900, 800);

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
    auto bounds = getLocalBounds().reduced(30);
    bounds.removeFromTop(60); // Space for title

    // Input file section
    inputLabel.setBounds(bounds.removeFromTop(28));
    auto inputRow = bounds.removeFromTop(40);
    chooseInputButton.setBounds(inputRow.removeFromRight(180));
    inputRow.removeFromRight(15); // Spacing
    inputPathLabel.setBounds(inputRow);

    bounds.removeFromTop(20); // Spacing

    // Output file section
    outputLabel.setBounds(bounds.removeFromTop(28));
    auto outputRow = bounds.removeFromTop(40);
    chooseOutputButton.setBounds(outputRow.removeFromRight(180));
    outputRow.removeFromRight(15); // Spacing
    outputPathLabel.setBounds(outputRow);

    bounds.removeFromTop(20); // Spacing

    // Processor selection + Process button + block size + status, all in one row.
    processorLabel.setBounds(bounds.removeFromTop(28));
    auto processorRow = bounds.removeFromTop(40);
    processorSelector.setBounds(processorRow.removeFromLeft(220));
    processorRow.removeFromLeft(15);
    processButton    .setBounds(processorRow.removeFromLeft(120));
    processorRow.removeFromLeft(15);
    blockSizeLabel   .setBounds(processorRow.removeFromLeft(80));
    blockSizeSelector.setBounds(processorRow.removeFromLeft(100));
    processorRow.removeFromLeft(15);
    statusLabel      .setBounds(processorRow);

    bounds.removeFromTop(20); // Spacing

    // Parameter control (unified for both processors) — horizontal linear slider.
    parameterLabel.setBounds(bounds.removeFromTop(28));
    auto parameterRow = bounds.removeFromTop(40);
    parameterValueLabel.setBounds(parameterRow.removeFromRight(120));
    parameterRow.removeFromRight(15);
    parameterSlider    .setBounds(parameterRow);

    bounds.removeFromTop(25); // Spacing

    // GrainShifter param row — three columns: name, value, slider stacked.
    auto paramRow = bounds.removeFromTop(80);
    const int paramColW = paramRow.getWidth() / 3;
    auto layoutParamCol = [] (juce::Rectangle<int> col,
                              juce::Label& nameLbl, juce::Label& valLbl, juce::Slider& s)
    {
        col = col.reduced(8, 0);
        nameLbl.setBounds(col.removeFromTop(20));
        valLbl .setBounds(col.removeFromTop(22).reduced(col.getWidth() / 4, 0));
        col.removeFromTop(4);
        s      .setBounds(col.removeFromTop(28));
    };
    layoutParamCol(paramRow.removeFromLeft(paramColW),
                   pitchWindowLabel,    pitchWindowValueLabel,    pitchWindowSlider);
    layoutParamCol(paramRow.removeFromLeft(paramColW),
                   pitchHopLabel,       pitchHopValueLabel,       pitchHopSlider);
    layoutParamCol(paramRow,
                   pitchThresholdLabel, pitchThresholdValueLabel, pitchThresholdSlider);

    bounds.removeFromTop(15); // Spacing

    // Logging toggles — side by side, taller for visible checkbox
    auto loggingRow = bounds.removeFromTop(40).reduced(60, 0);
    loggingToggle     .setBounds(loggingRow.removeFromLeft(loggingRow.getWidth() / 2));
    blockLoggingToggle.setBounds(loggingRow);

}

void AudioFileTransformerEditor::timerCallback()
{
    auto& fbm = mProcessor.getFileToBufferManager();
    const bool nowProcessing = fbm.isProcessing();

    if (nowProcessing)
    {
        float progress = currentProgress.load();
        int percent = static_cast<int>(progress * 100.0f);
        statusLabel.setText("Processing... " + juce::String(percent) + "%", juce::dontSendNotification);
    }
    else if (mWasProcessing)
    {
        // Detected processing -> idle transition. Re-enable button regardless of
        // whether progress callbacks ever fired (fast jobs can finish without
        // any non-zero progress reaching the UI thread).
        bool success = fbm.wasSuccessful();
        juce::String error = fbm.getError();

        currentProgress.store(0.0f);
        updateProcessButtonState();

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

    mWasProcessing = nowProcessing;
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
            mProcessor.getFileToBufferManager().setInputFile(file);
            inputPathLabel.setText(file.getFullPathName(), juce::dontSendNotification);
            updateProcessButtonState();
        }
    });
}

void AudioFileTransformerEditor::chooseOutputDirectory()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select output directory",
        mProcessor.getFileToBufferManager().getOutputDirectory()
    );

    auto flags = juce::FileBrowserComponent::openMode
               | juce::FileBrowserComponent::canSelectDirectories;

    fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc) {
        auto dir = fc.getResult();
        if (dir.getFullPathName().isNotEmpty())
        {
            mProcessor.getFileToBufferManager().setOutputDirectory(dir);
            outputPathLabel.setText(dir.getFullPathName(), juce::dontSendNotification);
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

    auto& fbm = mProcessor.getFileToBufferManager();
    fbm.setProgressCallback([this](float progress) {
        currentProgress.store(progress);
    });
    const bool started = fbm.startProcessing(mProcessor.getInputBuffer(),
                                             mProcessor.getProcessedBuffer(),
                                             mProcessor.getBufferProcessingManager());
    if (! started)
    {
        // Pre-thread validation failed: thread never set mIsProcessing=true,
        // so the timer's processing->idle transition will not fire. Restore
        // button state here.
        statusLabel.setText("Error: " + fbm.getError(), juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
        updateProcessButtonState();
    }
}

void AudioFileTransformerEditor::setDefaultInputFile()
{
    auto defaultInputFile = FileToBufferManager::getDefaultInputFile();

    if (defaultInputFile.existsAsFile())
    {
        mProcessor.getFileToBufferManager().setInputFile(defaultInputFile);
        inputPathLabel.setText(defaultInputFile.getFullPathName(), juce::dontSendNotification);
        updateProcessButtonState();
    }
}

void AudioFileTransformerEditor::updateProcessButtonState()
{
    auto& fbm = mProcessor.getFileToBufferManager();
    bool canProcess = fbm.getInputFile().existsAsFile()
                   && fbm.getOutputDirectory().getFullPathName().isNotEmpty()
                   && ! fbm.isProcessing();
    processButton.setEnabled(canProcess);
}

void AudioFileTransformerEditor::updateParameterValueLabel()
{
    float paramValue = static_cast<float>(parameterSlider.getValue());
    auto activeProc = mProcessor.getActiveProcessor();

    if (activeProc == ActiveProcessor::kGain)
    {
        parameterValueLabel.setText(juce::String(paramValue, 3), juce::dontSendNotification);

        auto* gainNode = mProcessor.getGainNode();
        if (gainNode != nullptr)
            gainNode->setGain(paramValue);
    }
    else // ActiveProcessor::kGrainShifter — slider is bound via SliderAttachment; just update label
    {
        parameterValueLabel.setText(juce::String(paramValue, 2), juce::dontSendNotification);
    }
}

void AudioFileTransformerEditor::configureParameterControlForProcessor(ActiveProcessor processor)
{
    // Drop any existing APVTS attachment before reconfiguring the slider range.
    mParamAttachment.reset();
    mPitchWindowAttachment   .reset();
    mPitchHopAttachment      .reset();
    mPitchThresholdAttachment.reset();

    const bool grainActive = (processor == ActiveProcessor::kGrainShifter);

    pitchWindowLabel        .setVisible(grainActive);
    pitchWindowValueLabel   .setVisible(grainActive);
    pitchWindowSlider       .setVisible(grainActive);
    pitchHopLabel           .setVisible(grainActive);
    pitchHopValueLabel      .setVisible(grainActive);
    pitchHopSlider          .setVisible(grainActive);
    pitchThresholdLabel     .setVisible(grainActive);
    pitchThresholdValueLabel.setVisible(grainActive);
    pitchThresholdSlider    .setVisible(grainActive);

    if (processor == ActiveProcessor::kGain)
    {
        parameterLabel.setText("Gain (0.0 = silent, 1.0 = full volume):", juce::dontSendNotification);
        parameterSlider.setRange(0.0, 1.0, 0.001);
        parameterSlider.setValue(0.5, juce::dontSendNotification);
        parameterValueLabel.setText("0.500", juce::dontSendNotification);
    }
    else // ActiveProcessor::kGrainShifter
    {
        parameterLabel.setText("Shift Ratio (0.5 = octave down, 1.0 = no shift, 2.0 = octave up):", juce::dontSendNotification);
        parameterValueLabel.setText("1.00", juce::dontSendNotification);

        auto* shifter = mProcessor.getGrainShifterNode();
        if (shifter != nullptr)
        {
            using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
            mParamAttachment           = std::make_unique<SA>(shifter->getAPVTS(), "shift_ratio",       parameterSlider);
            mPitchWindowAttachment     = std::make_unique<SA>(shifter->getAPVTS(), "pitch_window_size", pitchWindowSlider);
            mPitchHopAttachment        = std::make_unique<SA>(shifter->getAPVTS(), "pitch_hop_size",    pitchHopSlider);
            mPitchThresholdAttachment  = std::make_unique<SA>(shifter->getAPVTS(), "pitch_threshold",   pitchThresholdSlider);

            // Force value labels to refresh from current slider state.
            pitchWindowSlider   .onValueChange();
            pitchHopSlider      .onValueChange();
            pitchThresholdSlider.onValueChange();
        }
    }
}

void AudioFileTransformerEditor::processorSelectionChanged()
{
    int selectedId = processorSelector.getSelectedId();
    ActiveProcessor newProcessor = (selectedId == 1) ? ActiveProcessor::kGain
                                                     : ActiveProcessor::kGrainShifter;

    mProcessor.setActiveProcessor(newProcessor);
    configureParameterControlForProcessor(newProcessor);
    updateParameterValueLabel();
}
