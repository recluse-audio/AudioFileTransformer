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
// Mirrors canonical RD pattern (see SUBMODULES/RD/TESTS/PROCESSORS/RD_PROCESSOR/test_RD_Processor_DataLogger.cpp):
//   rootDir   = __FILE__/../OUTPUT/<TEST CASE NAME>/TEST_CASE_ROOT_DIR
//   outputName= "DATA_LOG_OUTPUT_DIR"
//   processor.setDataLogRootDirectory(rootDir);
//   processor.setDataLogOutputName  (outputName);
//   processor.startLogging();   // deletes outputDir recursively, sets isLogging=true
//   ... run work ...
//   processor.stopLogging();

TEST_CASE("AudioFileTransformerProcessor createProcessorDataLogFile writes processor name and APVTS XML",
          "[AudioFileTransformerProcessor][DataLogger]")
{
    TestUtils::SetupAndTeardown setup;
    AudioFileTransformerProcessor processor;

    juce::File rootDir = juce::File (__FILE__).getParentDirectory()
                                              .getChildFile ("OUTPUT")
                                              .getChildFile ("AudioFileTransformerProcessor createProcessorDataLogFile writes processor name and APVTS XML")
                                              .getChildFile ("TEST_CASE_ROOT_DIR");

    const juce::String outputName = "DATA_LOG_OUTPUT_DIR";

    processor.setDataLogRootDirectory (rootDir);
    processor.setDataLogOutputName    (outputName);
    processor.startLogging();

    auto logFile = processor.createProcessorDataLogFile();

    REQUIRE(logFile.existsAsFile());
    REQUIRE(logFile.getFileExtension() == ".xml");

    auto apvtsXml = juce::XmlDocument::parse (logFile);
    REQUIRE(apvtsXml != nullptr);
    REQUIRE(apvtsXml->getStringAttribute ("processorName") == processor.getName());

    auto* gainParam = apvtsXml->getChildByAttribute ("id", "gain");
    REQUIRE(gainParam != nullptr);
    REQUIRE(gainParam->getDoubleAttribute ("value") == Catch::Approx (1.0).margin (1e-5));

    processor.stopLogging();
}

TEST_CASE("AudioFileTransformerProcessor file transformation writes output WAV inside processor data log directory",
          "[AudioFileTransformerProcessor][DataLogger][file]")
{
    TestUtils::SetupAndTeardown setup;

    AudioFileTransformerProcessor processor;

    juce::File rootDir = juce::File (__FILE__).getParentDirectory()
                                              .getChildFile ("OUTPUT")
                                              .getChildFile ("AudioFileTransformerProcessor file transformation writes output WAV inside processor data log directory")
                                              .getChildFile ("TEST_CASE_ROOT_DIR");

    const juce::String outputName = "DATA_LOG_OUTPUT_DIR";

    processor.setDataLogRootDirectory (rootDir);
    processor.setDataLogOutputName    (outputName);
    processor.startLogging();

    constexpr double sampleRate = 44100.0;
    constexpr int    numSamples = 4096;
    juce::AudioBuffer<float> sineBuffer (2, numSamples);
    BufferFiller::generateSineCycles (sineBuffer, 100);

    auto inputDir = rootDir.getChildFile ("input");
    inputDir.createDirectory();
    auto inputFile = inputDir.getChildFile ("sine_input.wav");
    REQUIRE(BufferWriter::writeToWav (sineBuffer, inputFile, sampleRate, 24)
            == BufferWriter::Result::kSuccess);
    REQUIRE(inputFile.existsAsFile());

    auto processorOutputDir = processor.getDataLogOutputDirectory();
    REQUIRE(processorOutputDir == rootDir.getChildFile (outputName));

    processor.setActiveProcessor (ActiveProcessor::kGain);
    auto* gainNode = processor.getGainNode();
    REQUIRE(gainNode != nullptr);
    gainNode->setGain (0.5f);

    processor.getFileToBufferManager().setInputFile (inputFile);

    const bool transformOk = processor.doFileTransform();
    INFO("transform error: " << processor.getLastTransformError().toStdString());
    REQUIRE(transformOk);

    juce::Array<juce::File> wavs;
    processorOutputDir.findChildFiles (wavs, juce::File::findFiles, true, "*.wav");
    REQUIRE_FALSE(wavs.isEmpty());

    for (const auto& wav : wavs)
    {
        REQUIRE(wav.existsAsFile());
        REQUIRE(wav.getSize() > 0);
        REQUIRE(processorOutputDir.isAChildOf (rootDir));
        REQUIRE(wav.isAChildOf (processorOutputDir));
    }

    processor.stopLogging();
}

TEST_CASE("AudioFileTransformerProcessor doFileTransform with data logging cascades child logs",
          "[AudioFileTransformerProcessor][DataLogger][file]")
{
    TestUtils::SetupAndTeardown setup;

    AudioFileTransformerProcessor processor;

    juce::File rootDir = juce::File (__FILE__).getParentDirectory()
                                              .getChildFile ("OUTPUT")
                                              .getChildFile ("AudioFileTransformerProcessor doFileTransform with data logging cascades child logs")
                                              .getChildFile ("TEST_CASE_ROOT_DIR");

    const juce::String outputName = "DATA_LOG_OUTPUT_DIR";

    processor.setDataLogRootDirectory (rootDir);
    processor.setDataLogOutputName    (outputName);

    // Enable logging on processor, swapper, and active child — each DataLogger has its own flag.
    // BufferProcessingManager wires the active processor as a DataLogger child of swapper,
    // so doFileTransform's logData() cascade walks processor → swapper → active processor.
    processor.setActiveProcessor (ActiveProcessor::kGain);
    auto* gainNode = processor.getGainNode();
    REQUIRE(gainNode != nullptr);
    gainNode->setGain (0.5f);

    auto& swapper = processor.getBufferProcessingManager().getSwapper();
    processor.startLogging();
    swapper.startLogging();
    gainNode->startLogging();

    REQUIRE(processor.getIsLogging() == true);

    constexpr double sampleRate = 44100.0;
    constexpr int    numSamples = 4096;
    juce::AudioBuffer<float> sineBuffer (2, numSamples);
    BufferFiller::generateSineCycles (sineBuffer, 100);

    auto inputDir = rootDir.getChildFile ("input");
    inputDir.createDirectory();
    auto inputFile = inputDir.getChildFile ("sine_input.wav");
    REQUIRE(BufferWriter::writeToWav (sineBuffer, inputFile, sampleRate, 24)
            == BufferWriter::Result::kSuccess);

    auto processorOutputDir = processor.getDataLogOutputDirectory();

    processor.getFileToBufferManager().setInputFile (inputFile);

    const bool transformOk = processor.doFileTransform();
    INFO("transform error: " << processor.getLastTransformError().toStdString());
    REQUIRE(transformOk);

    // WAV under processor output dir.
    juce::Array<juce::File> wavs;
    processorOutputDir.findChildFiles (wavs, juce::File::findFiles, false, "*.wav");
    REQUIRE_FALSE(wavs.isEmpty());

    // Swapper child dir nested under processor output.
    auto swapperDir = processorOutputDir.getChildFile (swapper.getName());
    REQUIRE(swapperDir.isDirectory());

    // Active gain processor nested under swapper.
    auto gainDir = swapperDir.getChildFile (gainNode->getName());
    REQUIRE(gainDir.isDirectory());

    // Active processor wrote pre/post-process CSVs from processBlock cascade.
    juce::Array<juce::File> csvs;
    gainDir.findChildFiles (csvs, juce::File::findFiles, true, "*.csv");
    REQUIRE_FALSE(csvs.isEmpty());

    processor.stopLogging();
    swapper.stopLogging();
    gainNode->stopLogging();
}
