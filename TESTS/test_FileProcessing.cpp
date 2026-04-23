#include "TEST_UTILS/TestUtils.h"
#include "../SUBMODULES/RD/TESTS/TEST_UTILS/TestUtils.h"
#include "Processor/PluginProcessor.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("AudioFileTransformerProcessor file processing", "[AudioFileTransformer][file]")
{
    TestUtils::SetupAndTeardown setup;
    AudioFileTransformerProcessor processor;

    std::vector<juce::File> possibleInputFiles = {
        juce::File::getCurrentWorkingDirectory().getChildFile("TESTS/TEST_FILES/Somewhere_Mono.wav"),
        juce::File("C:\\Users\\rdeve\\Test_Vox\\Somewhere_Mono_48k.wav"),
        juce::File(juce::File::getCurrentWorkingDirectory().getChildFile("SUBMODULES/RD/TESTS/GOLDEN/GOLDEN_Somewhere_Mono_441k.wav"))
    };

    juce::File inputFile;
    for (const auto& file : possibleInputFiles)
    {
        if (file.existsAsFile())
        {
            inputFile = file;
            break;
        }
    }

    REQUIRE(inputFile.existsAsFile());

    auto outputDir = juce::File::getCurrentWorkingDirectory().getChildFile("TESTS/OUTPUT");
    if (!outputDir.exists())
        outputDir.createDirectory();

    processor.setActiveProcessor(ActiveProcessor::kGain);

    SECTION("Compare input and processed buffers - Gain processor")
    {
        auto* gainNode = processor.getGainNode();
        const float gainValue = 0.5f;
        gainNode->setGain(gainValue);

        auto testOutputFile = outputDir.getChildFile("Somewhere_Mono_Buffer_Test_Gain.wav");

        bool success = processor.processFile(inputFile, testOutputFile);
        if (!success)
            INFO("Error: " << processor.getLastError().toStdString());

        REQUIRE(success == true);

        auto& inputBuffer = processor.getInputBuffer();
        auto& processedBuffer = processor.getProcessedBuffer();

        REQUIRE(inputBuffer.getNumSamples() > 0);
        REQUIRE(processedBuffer.getNumSamples() > 0);
        REQUIRE(inputBuffer.getNumChannels() == 2);
        REQUIRE(processedBuffer.getNumChannels() == 2);

        REQUIRE(inputBuffer.getNumSamples() == processedBuffer.getNumSamples());

        for (int ch = 0; ch < inputBuffer.getNumChannels(); ++ch)
        {
            for (int i = 0; i < inputBuffer.getNumSamples(); ++i)
            {
                float expectedValue = inputBuffer.getSample(ch, i) * gainValue;
                float actualValue = processedBuffer.getSample(ch, i);

                float difference = std::abs(expectedValue - actualValue);
                if (difference > 0.0001f)
                {
                    INFO("Channel " << ch << ", Sample " << i << ": Expected " << expectedValue << ", Got " << actualValue);
                    CHECK(difference <= 0.003f);
                }
            }
        }
    }

    SECTION("Compare input and processed buffers - GrainShifter processor")
    {
        processor.setActiveProcessor(ActiveProcessor::kGrainShifter);
        auto* shifterNode = processor.getGrainShifterNode();
        REQUIRE(shifterNode != nullptr);

        auto testOutputFile = outputDir.getChildFile("Somewhere_Mono_Buffer_Test_GrainShifter.wav");

        bool success = processor.processFile(inputFile, testOutputFile);
        if (!success)
            INFO("Error: " << processor.getLastError().toStdString());
        REQUIRE(success == true);

        auto& inputBuffer = processor.getInputBuffer();
        auto& processedBuffer = processor.getProcessedBuffer();

        REQUIRE(inputBuffer.getNumSamples() > 0);
        REQUIRE(processedBuffer.getNumSamples() > 0);
        REQUIRE(inputBuffer.getNumChannels() == 2);
        REQUIRE(processedBuffer.getNumChannels() == 2);

        bool hasNonZeroSamples = false;
        for (int ch = 0; ch < processedBuffer.getNumChannels(); ++ch)
        {
            for (int i = 0; i < processedBuffer.getNumSamples(); ++i)
            {
                if (std::abs(processedBuffer.getSample(ch, i)) > 0.0001f)
                {
                    hasNonZeroSamples = true;
                    break;
                }
            }
            if (hasNonZeroSamples) break;
        }
        REQUIRE(hasNonZeroSamples == true);
    }
}
