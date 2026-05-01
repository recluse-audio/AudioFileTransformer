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

        // New layout: single accumulating input_samples.csv / output_samples.csv at
        // the swapper's output dir root, appended once per processBlock chunk.
        auto inCsv  = sectionDir.getChildFile ("input_samples.csv");
        auto outCsv = sectionDir.getChildFile ("output_samples.csv");
        REQUIRE(inCsv .existsAsFile());
        REQUIRE(outCsv.existsAsFile());

        const int rowsPerBlock = 2 + numChannels;
        const int expectedRows = rowsPerBlock * numBlocks;
        auto countLines = [] (const juce::File& f)
        {
            return juce::StringArray::fromLines (f.loadFileAsString().trimEnd()).size();
        };
        REQUIRE(countLines (inCsv ) == expectedRows);
        REQUIRE(countLines (outCsv) == expectedRows);

        // No stale per-block subdir/XML layout.
        juce::Array<juce::File> blockSubdirs;
        sectionDir.findChildFiles (blockSubdirs, juce::File::findDirectories, false, "process_block_*");
        REQUIRE(blockSubdirs.isEmpty());

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
