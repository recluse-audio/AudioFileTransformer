#include "TEST_UTILS/TestUtils.h"
#include "../../SOURCE/Audio/AudioFileProcessor.h"
#include <catch2/matchers/catch_matchers_floating_point.hpp>

TEST_CASE("AudioFileProcessor construction", "[AudioFileProcessor]")
{
    SECTION("Can create AudioFileProcessor instance")
    {
        AudioFileProcessor processor;
        REQUIRE(processor.getLastError().isEmpty());
    }
}

TEST_CASE("AudioFileProcessor::processFile - basic functionality", "[AudioFileProcessor]")
{
    AudioFileProcessor processor;

    SECTION("Process valid WAV file to output")
    {
        // Input: existing test file
        auto inputFile = juce::File::getCurrentWorkingDirectory()
            .getChildFile("TESTS/TEST_FILES/Somewhere_Mono_48k.wav");

        // Output: temp file
        auto outputFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("test_output.wav");

        // Ensure output doesn't exist before test
        outputFile.deleteFile();

        // Process file
        float lastProgress = 0.0f;
        bool result = processor.processFile(
            inputFile,
            outputFile,
            [&lastProgress](float progress) {
                lastProgress = progress;
            }
        );

        REQUIRE(result == true);
        REQUIRE(processor.getLastError().isEmpty() == true);
        REQUIRE(outputFile.existsAsFile() == true);
        REQUIRE(lastProgress > 0.0f);

        // Cleanup
        outputFile.deleteFile();
    }

    SECTION("Processing non-existent input file fails")
    {
        auto inputFile = juce::File("non_existent_file.wav");
        auto outputFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("test_output.wav");

        bool result = processor.processFile(inputFile, outputFile, nullptr);

        REQUIRE(result == false);
        REQUIRE(processor.getLastError().isNotEmpty() == true);
        REQUIRE(outputFile.existsAsFile() == false);
    }

    SECTION("Processing to invalid output path fails")
    {
        auto inputFile = juce::File::getCurrentWorkingDirectory()
            .getChildFile("TESTS/TEST_FILES/Somewhere_Mono_48k.wav");
        auto outputFile = juce::File("C:/invalid/path/that/does/not/exist/output.wav");

        bool result = processor.processFile(inputFile, outputFile, nullptr);

        REQUIRE(result == false);
        REQUIRE(processor.getLastError().isNotEmpty() == true);
    }
}

TEST_CASE("AudioFileProcessor::processFile - audio verification", "[AudioFileProcessor]")
{
    AudioFileProcessor processor;

    SECTION("Output file matches input file (bit-perfect copy)")
    {
        auto inputFile = juce::File::getCurrentWorkingDirectory()
            .getChildFile("TESTS/TEST_FILES/Somewhere_Mono_48k.wav");
        auto outputFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("test_output_verify.wav");

        outputFile.deleteFile();

        // Process file
        bool result = processor.processFile(inputFile, outputFile, nullptr);
        REQUIRE(result == true);

        // Read both files and compare
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        auto inputReader = std::unique_ptr<juce::AudioFormatReader>(
            formatManager.createReaderFor(inputFile));
        auto outputReader = std::unique_ptr<juce::AudioFormatReader>(
            formatManager.createReaderFor(outputFile));

        REQUIRE(inputReader != nullptr);
        REQUIRE(outputReader != nullptr);

        // Compare metadata
        REQUIRE(outputReader->sampleRate == inputReader->sampleRate);
        REQUIRE(outputReader->numChannels == inputReader->numChannels);
        REQUIRE(outputReader->lengthInSamples == inputReader->lengthInSamples);
        REQUIRE(outputReader->bitsPerSample == inputReader->bitsPerSample);

        // Read and compare audio data
        juce::AudioBuffer<float> inputBuffer(
            (int)inputReader->numChannels,
            (int)inputReader->lengthInSamples
        );
        juce::AudioBuffer<float> outputBuffer(
            (int)outputReader->numChannels,
            (int)outputReader->lengthInSamples
        );

        inputReader->read(&inputBuffer, 0, (int)inputReader->lengthInSamples, 0, true, true);
        outputReader->read(&outputBuffer, 0, (int)outputReader->lengthInSamples, 0, true, true);

        // Compare samples (allowing for small floating-point differences)
        for (int channel = 0; channel < inputBuffer.getNumChannels(); ++channel)
        {
            const float* inputSamples = inputBuffer.getReadPointer(channel);
            const float* outputSamples = outputBuffer.getReadPointer(channel);

            for (int i = 0; i < inputBuffer.getNumSamples(); ++i)
            {
                REQUIRE_THAT(outputSamples[i],
                    Catch::Matchers::WithinAbs(inputSamples[i], 0.0001f));
            }
        }

        // Cleanup
        outputFile.deleteFile();
    }

    SECTION("Progress callback is called during processing")
    {
        auto inputFile = juce::File::getCurrentWorkingDirectory()
            .getChildFile("TESTS/TEST_FILES/Somewhere_Mono_48k.wav");
        auto outputFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("test_output_progress.wav");

        outputFile.deleteFile();

        int callbackCount = 0;
        float maxProgress = 0.0f;

        bool result = processor.processFile(
            inputFile,
            outputFile,
            [&callbackCount, &maxProgress](float progress) {
                ++callbackCount;
                if (progress > maxProgress)
                    maxProgress = progress;
            }
        );

        REQUIRE(result == true);
        REQUIRE(callbackCount > 0);
        REQUIRE(maxProgress > 0.0f);
        REQUIRE(maxProgress <= 1.0f);

        // Cleanup
        outputFile.deleteFile();
    }
}

TEST_CASE("AudioFileProcessor::getLastError", "[AudioFileProcessor]")
{
    AudioFileProcessor processor;

    SECTION("Error is empty after successful processing")
    {
        auto inputFile = juce::File::getCurrentWorkingDirectory()
            .getChildFile("TESTS/TEST_FILES/Somewhere_Mono_48k.wav");
        auto outputFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("test_output_error.wav");

        outputFile.deleteFile();

        processor.processFile(inputFile, outputFile, nullptr);

        REQUIRE(processor.getLastError().isEmpty() == true);

        // Cleanup
        outputFile.deleteFile();
    }

    SECTION("Error is set after failed processing")
    {
        auto inputFile = juce::File("non_existent.wav");
        auto outputFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("test_output.wav");

        processor.processFile(inputFile, outputFile, nullptr);

        REQUIRE(processor.getLastError().isNotEmpty() == true);
    }
}
