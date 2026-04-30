#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "TEST_UTILS/TestUtils.h"
#include "SUBMODULES/RD/TESTS/TEST_UTILS/TestUtils.h"
#include "Processor/PluginProcessor.h"
#include "BufferWriter.h"
#include "BufferFiller.h"

//========================================================
//===================== DATA LOGGING =====================
//========================================================
// Protocol mirrors RD/TESTS DataLogger convention:
//   1. Build timestamped outputDir under
//      TESTS/PLUGIN_PROCESSOR/OUTPUT/<TEST CASE NAME>/<timestamp>.
//      Treat outputDir as the parent directory for the logger.
//   2. Per SECTION, configure the logger:
//        processor.setParentDirectory(outputDir);
//        processor.setOutputDirectoryName("<section name>");
//        processor.createOutputDirectory();
//      so each section's logs are isolated under outputDir/<section name>/.
//   3. Log processor state via createProcessorDataLogFile and REQUIRE existence.

TEST_CASE("AudioFileTransformerProcessor createProcessorDataLogFile writes processor name and APVTS XML",
          "[AudioFileTransformerProcessor][DataLogger]")
{
    TestUtils::SetupAndTeardown setup;
    AudioFileTransformerProcessor processor;

    auto timestamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d_%H-%M-%S");
    juce::File parent_directory = juce::File::getCurrentWorkingDirectory()
                               .getChildFile ("TESTS/PLUGIN_PROCESSOR/OUTPUT/AudioFileTransformerProcessor createProcessorDataLogFile writes processor name and APVTS XML")
                               .getChildFile (timestamp);

    processor.setParentDirectory (parent_directory);
    processor.setOutputDirectoryName ("initial");
    processor.createOutputDirectory();

    auto logFile = processor.createProcessorDataLogFile();

    REQUIRE(logFile.existsAsFile());
    REQUIRE(logFile.getFileExtension() == ".xml");

    auto apvtsXml = juce::XmlDocument::parse (logFile);
    REQUIRE(apvtsXml != nullptr);
    REQUIRE(apvtsXml->getStringAttribute ("processorName") == processor.getName());

    auto* gainParam = apvtsXml->getChildByAttribute ("id", "gain");
    REQUIRE(gainParam != nullptr);
    REQUIRE(gainParam->getDoubleAttribute ("value") == Catch::Approx (1.0).margin (1e-5));
}

TEST_CASE("AudioFileTransformerProcessor file transformation writes output WAV inside processor data log directory",
          "[AudioFileTransformerProcessor][DataLogger][file]")
{
    TestUtils::SetupAndTeardown setup;

    AudioFileTransformerProcessor processor;

    auto timestamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d_%H-%M-%S");
    juce::File parent_directory = juce::File::getCurrentWorkingDirectory()
                               .getChildFile ("TESTS/PLUGIN_PROCESSOR/OUTPUT/AudioFileTransformerProcessor file transformation writes output WAV inside processor data log directory")
                               .getChildFile (timestamp);

    processor.setParentDirectory (parent_directory);
    processor.setOutputDirectoryName ("transform_run");
    processor.createOutputDirectory();

    constexpr double sampleRate = 44100.0;
    constexpr int    numSamples = 4096;
    juce::AudioBuffer<float> sineBuffer (2, numSamples);
    BufferFiller::generateSineCycles (sineBuffer, 100);

    auto inputDir = parent_directory.getChildFile ("input");
    inputDir.createDirectory();
    auto inputFile = inputDir.getChildFile ("sine_input.wav");
    REQUIRE(BufferWriter::writeToWav (sineBuffer, inputFile, sampleRate, 24)
            == BufferWriter::Result::kSuccess);
    REQUIRE(inputFile.existsAsFile());

    auto processorOutputDir = processor.getOutputDirectory();
    REQUIRE(processorOutputDir.isDirectory());
    REQUIRE(processorOutputDir.getParentDirectory() == parent_directory);

    processor.setActiveProcessor (ActiveProcessor::kGain);
    auto* gainNode = processor.getGainNode();
    REQUIRE(gainNode != nullptr);
    gainNode->setGain (0.5f);

    processor.getFileToBufferManager().setInputFile (inputFile);

    INFO("transform error: " << processor.getLastTransformError().toStdString());
    REQUIRE(processor.doFileTransform());

    juce::Array<juce::File> wavs;
    processorOutputDir.findChildFiles (wavs, juce::File::findFiles, true, "*.wav");
    REQUIRE_FALSE(wavs.isEmpty());

    for (const auto& wav : wavs)
    {
        REQUIRE(wav.existsAsFile());
        REQUIRE(wav.getSize() > 0);
        REQUIRE(processorOutputDir.isAChildOf (parent_directory));
        REQUIRE(wav.isAChildOf (processorOutputDir));
        REQUIRE_FALSE(wav.getParentDirectory() == parent_directory);
    }
}

TEST_CASE("AudioFileTransformerProcessor doFileTransform with data logging cascades child logs",
          "[AudioFileTransformerProcessor][DataLogger][file]")
{
    TestUtils::SetupAndTeardown setup;

    AudioFileTransformerProcessor processor;

    auto timestamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d_%H-%M-%S");
    juce::File parent_directory = juce::File::getCurrentWorkingDirectory()
                               .getChildFile ("TESTS/PLUGIN_PROCESSOR/OUTPUT/AudioFileTransformerProcessor doFileTransform with data logging cascades child logs")
                               .getChildFile (timestamp);

    processor.setParentDirectory (parent_directory);
    processor.setOutputDirectoryName ("transform_run");
    processor.createOutputDirectory();

    constexpr double sampleRate = 44100.0;
    constexpr int    numSamples = 4096;
    juce::AudioBuffer<float> sineBuffer (2, numSamples);
    BufferFiller::generateSineCycles (sineBuffer, 100);

    auto inputDir = parent_directory.getChildFile ("input");
    inputDir.createDirectory();
    auto inputFile = inputDir.getChildFile ("sine_input.wav");
    REQUIRE(BufferWriter::writeToWav (sineBuffer, inputFile, sampleRate, 24)
            == BufferWriter::Result::kSuccess);

    auto processorOutputDir = processor.getOutputDirectory();

    processor.setActiveProcessor (ActiveProcessor::kGain);
    auto* gainNode = processor.getGainNode();
    REQUIRE(gainNode != nullptr);
    gainNode->setGain (0.5f);

    processor.getFileToBufferManager().setInputFile (inputFile);

    // Enable logging on processor, swapper, and active child — each DataLogger has its own flag.
    // BufferProcessingManager wires the active processor as a DataLogger child of swapper,
    // so doFileTransform's logData() cascade walks processor → swapper → active processor.
    auto& swapper = processor.getBufferProcessingManager().getSwapper();
    processor.setIsLogging (true);
    swapper.setIsLogging (true);
    gainNode->setIsLogging (true);

    REQUIRE(processor.getIsLogging() == true);

    INFO("transform error: " << processor.getLastTransformError().toStdString());
    REQUIRE(processor.doFileTransform());

    // WAV under processor output dir.
    juce::Array<juce::File> wavs;
    processorOutputDir.findChildFiles (wavs, juce::File::findFiles, false, "*.wav");
    REQUIRE_FALSE(wavs.isEmpty());

    // Processor's own DataLogger output (default base impl writes output.txt).
    auto processorLog = processorOutputDir.getChildFile ("output.txt");
    REQUIRE(processorLog.existsAsFile());

    // Swapper child dir nested under processor output.
    auto swapperDir = processorOutputDir.getChildFile (swapper.getName());
    REQUIRE(swapperDir.isDirectory());
    REQUIRE(swapperDir.getChildFile ("output.txt").existsAsFile());

    // Active gain processor nested under swapper.
    auto gainDir = swapperDir.getChildFile (gainNode->getName());
    REQUIRE(gainDir.isDirectory());
    REQUIRE(gainDir.getChildFile ("output.txt").existsAsFile());

    // Active processor wrote pre/post-process CSVs from processBlock cascade.
    juce::Array<juce::File> csvs;
    gainDir.findChildFiles (csvs, juce::File::findFiles, false, "*.csv");
    REQUIRE_FALSE(csvs.isEmpty());

    processor.setIsLogging (false);
    swapper.setIsLogging (false);
    gainNode->setIsLogging (false);
}
