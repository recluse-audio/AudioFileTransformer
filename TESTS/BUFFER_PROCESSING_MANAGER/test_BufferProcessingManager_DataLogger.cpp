#include <catch2/catch_test_macros.hpp>

#include "TEST_UTILS/TestUtils.h"
#include "SUBMODULES/RD/TESTS/TEST_UTILS/TestUtils.h"
#include "Processor/BufferProcessingManager.h"

#include "BufferFiller.h"

//========================================================
//===================== DATA LOGGING =====================
//========================================================
// Protocol mirrors RD/TESTS DataLogger convention:
//   1. Build timestamped outputDir under
//      TESTS/BUFFER_PROCESSING_MANAGER/OUTPUT/<TEST CASE NAME>/<timestamp>.
//   2. Call swapper.createOutputDirectory(outputDir) once per test case.
//   3. Per SECTION, create sectionDir under outputDir, then setOutputFile(sectionDir).
//   4. setIsLogging(true) on swapper enables its pre/post-process CSV writes.
//   5. Run processBuffers; RD_Processor::processBlock writes pre/post CSVs per block.
//   6. Log final state via createProcessorDataLogFile.

TEST_CASE("BufferProcessingManager processes all-ones buffer in 256-sample blocks with logging",
          "[BufferProcessingManager][DataLogger]")
{
    TestUtils::SetupAndTeardown setup;
    BufferProcessingManager manager;

    auto& swapper = manager.getSwapper();

    auto timestamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d_%H-%M-%S");
    juce::File outputDir = juce::File::getCurrentWorkingDirectory()
                               .getChildFile ("TESTS/BUFFER_PROCESSING_MANAGER/OUTPUT/BufferProcessingManager processes all-ones buffer in 256-sample blocks with logging")
                               .getChildFile (timestamp);

    const double sampleRate = 44100.0;
    const int    blockSize  = 256;
    const int    numChannels = 2;
    const int    numBlocks   = 4;
    const int    numSamples  = blockSize * numBlocks;

    auto runSection = [&] (ActiveProcessor processorIndex, const juce::String& sectionName)
    {
        swapper.setParentDirectory (outputDir);
        swapper.setOutputDirectoryName (sectionName);
        swapper.createOutputDirectory();
        auto sectionDir = swapper.getOutputDirectory();
        swapper.setIsLogging (true);

        manager.setActiveProcessor (processorIndex);

        juce::AudioBuffer<float> inputBuffer (numChannels, numSamples);
        BufferFiller::fillWithAllOnes (inputBuffer);

        juce::AudioBuffer<float> outputBuffer (numChannels, numSamples);
        outputBuffer.clear();

        bool success = manager.processBuffers (inputBuffer, outputBuffer, numSamples, numSamples, sampleRate, blockSize);
        REQUIRE(success);

        auto stateLog = swapper.createProcessorDataLogFile();
        REQUIRE(stateLog.existsAsFile());

        REQUIRE(sectionDir.exists());
        REQUIRE(sectionDir.getNumberOfChildFiles (juce::File::findFilesAndDirectories) > 0);

        swapper.setIsLogging (false);
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
