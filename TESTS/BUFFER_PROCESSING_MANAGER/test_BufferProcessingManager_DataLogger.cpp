#include <catch2/catch_test_macros.hpp>

#include "TEST_UTILS/TestUtils.h"
#include "SUBMODULES/RD/TESTS/TEST_UTILS/TestUtils.h"
#include "Processor/BufferProcessingManager.h"

#include "BufferFiller.h"

//========================================================
//===================== DATA LOGGING =====================
//========================================================
// Mirrors canonical RD pattern (see SUBMODULES/RD/TESTS/PROCESSORS/RD_PROCESSOR/test_RD_Processor_DataLogger.cpp):
//   rootDir   = __FILE__/../OUTPUT/<TEST CASE NAME>/TEST_CASE_ROOT_DIR
//   outputName= "DATA_LOG_OUTPUT_DIR"
//   swapper.setDataLogRootDirectory(rootDir);
//   swapper.setDataLogOutputName  (outputName);
//   swapper.startLogging();   // deletes outputDir recursively, sets isLogging=true
//   ... run work ...
//   swapper.stopLogging();

TEST_CASE("BufferProcessingManager processes all-ones buffer in 256-sample blocks with logging",
          "[BufferProcessingManager][DataLogger]")
{
    TestUtils::SetupAndTeardown setup;
    BufferProcessingManager manager;

    auto& swapper = manager.getSwapper();

    juce::File rootDir = juce::File (__FILE__).getParentDirectory()
                                              .getChildFile ("OUTPUT")
                                              .getChildFile ("BufferProcessingManager processes all-ones buffer in 256-sample blocks with logging")
                                              .getChildFile ("TEST_CASE_ROOT_DIR");

    const double sampleRate  = 44100.0;
    const int    blockSize   = 256;
    const int    numChannels = 2;
    const int    numBlocks   = 4;
    const int    numSamples  = blockSize * numBlocks;

    auto runSection = [&] (ActiveProcessor processorIndex, const juce::String& sectionName)
    {
        const juce::String outputName = "DATA_LOG_OUTPUT_DIR_" + sectionName;

        swapper.setDataLogRootDirectory (rootDir);
        swapper.setDataLogOutputName    (outputName);
        swapper.startLogging();

        manager.setActiveProcessor (processorIndex);

        juce::AudioBuffer<float> inputBuffer (numChannels, numSamples);
        BufferFiller::fillWithAllOnes (inputBuffer);

        juce::AudioBuffer<float> outputBuffer (numChannels, numSamples);
        outputBuffer.clear();

        bool success = manager.processBuffers (inputBuffer, outputBuffer, numSamples, numSamples, sampleRate, blockSize);
        REQUIRE(success);

        auto stateLog = swapper.createProcessorDataLogFile();
        REQUIRE(stateLog.existsAsFile());

        auto sectionDir = swapper.getDataLogOutputDirectory();
        REQUIRE(sectionDir == rootDir.getChildFile (outputName));
        REQUIRE(sectionDir.exists());
        REQUIRE(sectionDir.getNumberOfChildFiles (juce::File::findFilesAndDirectories) > 0);

        swapper.stopLogging();
    };

    SECTION("Gain processor — all-ones, 256-block")
    {
        runSection (ActiveProcessor::kGain, "gain");
    }

    SECTION("GrainShifter processor — all-ones, 256-block")
    {
        runSection (ActiveProcessor::kGrainShifter, "grain-shifter");
    }
}
