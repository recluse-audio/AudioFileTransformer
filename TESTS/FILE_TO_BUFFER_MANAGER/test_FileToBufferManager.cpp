#include "TEST_UTILS/TestUtils.h"
#include "SUBMODULES/RD/TESTS/TEST_UTILS/TestUtils.h"
#include "Processor/PluginProcessor.h"
#include "Processor/FileToBufferManager.h"
#include "Processor/BufferProcessingManager.h"
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <thread>

namespace
{
    juce::File findInputFile()
    {
        std::vector<juce::File> candidates = {
            juce::File::getCurrentWorkingDirectory().getChildFile("TESTS/TEST_FILES/Somewhere_Mono.wav"),
            juce::File("C:\\Users\\rdeve\\Test_Vox\\Somewhere_Mono_48k.wav"),
            juce::File::getCurrentWorkingDirectory().getChildFile("SUBMODULES/RD/TESTS/GOLDEN/GOLDEN_Somewhere_Mono_441k.wav")
        };
        for (const auto& f : candidates)
            if (f.existsAsFile())
                return f;
        return {};
    }

    void waitForCompletion(FileToBufferManager& fbm, int timeoutMs = 30000)
    {
        const auto start = std::chrono::steady_clock::now();
        while (fbm.isProcessing())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            if (std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count() > timeoutMs)
                break;
        }
    }
}

TEST_CASE("FileToBufferManager threaded load->process->write end-to-end",
          "[FileToBufferManager][file]")
{
    TestUtils::SetupAndTeardown setup;

    auto inputFile = findInputFile();
    REQUIRE(inputFile.existsAsFile());

    auto outputDir = juce::File::getCurrentWorkingDirectory()
                       .getChildFile("TESTS/FILE_TO_BUFFER_MANAGER/OUTPUT");
    if (! outputDir.exists())
        outputDir.createDirectory();

    AudioFileTransformerProcessor processor;
    processor.setActiveProcessor(ActiveProcessor::kGain);
    auto* gainNode = processor.getGainNode();
    REQUIRE(gainNode != nullptr);
    gainNode->setGain(0.5f);

    auto& fbm = processor.getFileToBufferManager();
    fbm.setInputFile(inputFile);
    fbm.setOutputDirectory(outputDir);

    std::atomic<float> lastProgress { 0.0f };
    std::atomic<int>   callbackCount { 0 };
    fbm.setProgressCallback([&](float p)
    {
        lastProgress.store(p);
        callbackCount.fetch_add(1);
    });

    SECTION("startProcessing succeeds and produces an output WAV")
    {
        REQUIRE(fbm.startProcessing(processor.getInputBuffer(),
                                    processor.getProcessedBuffer(),
                                    processor.getBufferProcessingManager()));
        waitForCompletion(fbm);

        REQUIRE_FALSE(fbm.isProcessing());
        INFO("FBM error: " << fbm.getError().toStdString());
        REQUIRE(fbm.wasSuccessful());
        REQUIRE(callbackCount.load() > 0);
        REQUIRE(lastProgress.load() == 1.0f);
    }

    SECTION("Missing input file fails validation, no thread spawned")
    {
        fbm.setInputFile(juce::File("C:\\does\\not\\exist.wav"));
        REQUIRE_FALSE(fbm.startProcessing(processor.getInputBuffer(),
                                          processor.getProcessedBuffer(),
                                          processor.getBufferProcessingManager()));
        REQUIRE_FALSE(fbm.isProcessing());
        REQUIRE(fbm.getError().isNotEmpty());
    }
}

TEST_CASE("FileToBufferManager synchronous load + write helpers",
          "[FileToBufferManager][file][sync]")
{
    TestUtils::SetupAndTeardown setup;

    auto inputFile = findInputFile();
    REQUIRE(inputFile.existsAsFile());

    FileToBufferManager fbm;
    fbm.setInputFile(inputFile);

    juce::AudioBuffer<float> dest(2, AudioFileTransformerProcessor::kStorageSamples);
    dest.clear();

    double sampleRate    = 0.0;
    int    samplesRead   = 0;
    REQUIRE(fbm.loadInputToBuffer(dest,
                                   AudioFileTransformerProcessor::kStorageSamples,
                                   sampleRate,
                                   samplesRead));
    REQUIRE(samplesRead > 0);
    REQUIRE(sampleRate > 0.0);

    auto outDir = juce::File::getCurrentWorkingDirectory()
                    .getChildFile("TESTS/FILE_TO_BUFFER_MANAGER/OUTPUT");
    if (! outDir.exists())
        outDir.createDirectory();
    auto outFile = outDir.getChildFile("sync_helper_out.wav");

    REQUIRE(fbm.writeBufferToFile(dest, outFile, sampleRate, samplesRead));
    REQUIRE(outFile.existsAsFile());
    REQUIRE(outFile.getSize() > 0);
}
