#include "TEST_UTILS/TestUtils.h"
#include "../SUBMODULES/RD/TESTS/TEST_UTILS/TestUtils.h"
#include "../SOURCE/PluginProcessor.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("AudioFileTransformerProcessor file processing", "[AudioFileTransformer][file]")
{
    TestUtils::SetupAndTeardown setup;
    AudioFileTransformerProcessor processor;

    // Input file paths to try
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

    // Create output directory if it doesn't exist
    auto outputDir = juce::File::getCurrentWorkingDirectory().getChildFile("TESTS/OUTPUT");
    if (!outputDir.exists())
        outputDir.createDirectory();

    auto outputFile = outputDir.getChildFile("Somewhere_Mono_Processed.wav");


    // Switch to gain processor for this test
    processor.setActiveProcessor(ActiveProcessor::Gain);

    SECTION("Process mono audio file through processor graph")
    {
        // Set gain to unity (1.0) for no transformation
        auto* gainNode = processor.getGainNode();
        REQUIRE(gainNode != nullptr);
        gainNode->setGain(1.0f);

        // Track progress
        bool progressCalled = false;
        float lastProgress = 0.0f;

        // Process the file
        bool success = processor.processFile( inputFile, outputFile,
            [&progressCalled, &lastProgress](float progress) {
                progressCalled = true;
                lastProgress = progress;
                INFO("Processing progress: " << (progress * 100.0f) << "%");
            }
        );

        if (!success)
        {
            INFO("Error: " << processor.getLastError().toStdString());
        }

        REQUIRE(success == true);
        REQUIRE(progressCalled == true);
        REQUIRE(lastProgress >= 0.99f); // Should reach ~100%
        REQUIRE(outputFile.existsAsFile());
    }

    SECTION("Verify output file properties match input")
    {
        // Set gain to unity
        auto* gainNode = processor.getGainNode();
        gainNode->setGain(1.0f);

        // Process the file
        bool success = processor.processFile(inputFile, outputFile);
        REQUIRE(success == true);

        // Read input file properties
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        auto inputReader = std::unique_ptr<juce::AudioFormatReader>(
            formatManager.createReaderFor(inputFile));
        REQUIRE(inputReader != nullptr);

        auto outputReader = std::unique_ptr<juce::AudioFormatReader>(
            formatManager.createReaderFor(outputFile));
        REQUIRE(outputReader != nullptr);

        // Verify properties
        INFO("Input channels: " << inputReader->numChannels);
        INFO("Output channels: " << outputReader->numChannels);
        INFO("Input sample rate: " << inputReader->sampleRate);
        INFO("Output sample rate: " << outputReader->sampleRate);
        INFO("Input length: " << inputReader->lengthInSamples);
        INFO("Output length: " << outputReader->lengthInSamples);

        // Sample rate should match
        REQUIRE(outputReader->sampleRate == inputReader->sampleRate);

        // Length should match (or be very close)
        REQUIRE(std::abs((int)outputReader->lengthInSamples - (int)inputReader->lengthInSamples) < 100);

        // Output should be stereo (mono converted to stereo)
        if (inputReader->numChannels == 1)
        {
            REQUIRE(outputReader->numChannels == 2);
        }
        else
        {
            REQUIRE(outputReader->numChannels == inputReader->numChannels);
        }
    }

    SECTION("Process with gain of 0.5")
    {
        auto* gainNode = processor.getGainNode();
        gainNode->setGain(0.5f);

        auto testOutputFile = outputDir.getChildFile("Somewhere_Mono_Processed_Half_Gain.wav");

        bool success = processor.processFile(inputFile, testOutputFile);
        if (!success)
            INFO("Error: " << processor.getLastError().toStdString());

        REQUIRE(success == true);
        REQUIRE(testOutputFile.existsAsFile());

        // Verify the output is quieter
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        auto inputReader = std::unique_ptr<juce::AudioFormatReader>(formatManager.createReaderFor(inputFile));
        auto outputReader = std::unique_ptr<juce::AudioFormatReader>(formatManager.createReaderFor(testOutputFile));
        REQUIRE(inputReader != nullptr);
        REQUIRE(outputReader != nullptr);

        // Read a small chunk from both files
        const int samplesToCheck = 1024;
        juce::AudioBuffer<float> inputBuffer(inputReader->numChannels, samplesToCheck);
        juce::AudioBuffer<float> outputBuffer(outputReader->numChannels, samplesToCheck);
        inputReader->read(&inputBuffer, 0, samplesToCheck, 1000, true, true);
        outputReader->read(&outputBuffer, 0, samplesToCheck, 1000, true, true);

        // Calculate RMS of input
        float inputRMS = inputBuffer.getRMSLevel(0, 0, samplesToCheck);

        // Calculate RMS of output (use first channel)
        float outputRMS = outputBuffer.getRMSLevel(0, 0, samplesToCheck);

        INFO("Input RMS: " << inputRMS);
        INFO("Output RMS: " << outputRMS);

        // Output should be approximately half the amplitude
        if (inputRMS > 0.01f) // Only check if there's actual signal
        {
            float ratio = outputRMS / inputRMS;
            INFO("RMS Ratio: " << ratio);
            REQUIRE(ratio > 0.4f);
            REQUIRE(ratio < 0.6f);
        }
    }

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

        // Access the internal buffers
        auto& inputBuffer = processor.getInputBuffer();
        auto& processedBuffer = processor.getProcessedBuffer();

        // Verify buffers are populated
        REQUIRE(inputBuffer.getNumSamples() > 0);
        REQUIRE(processedBuffer.getNumSamples() > 0);
        REQUIRE(inputBuffer.getNumChannels() == 2); // Should be stereo
        REQUIRE(processedBuffer.getNumChannels() == 2);

        // Verify buffers have same size
        REQUIRE(inputBuffer.getNumSamples() == processedBuffer.getNumSamples());

        // Verify sample-by-sample that processed = input * gain
        for (int ch = 0; ch < inputBuffer.getNumChannels(); ++ch)
        {
            for (int i = 0; i < inputBuffer.getNumSamples(); ++i)
            {
                float expectedValue = inputBuffer.getSample(ch, i) * gainValue;
                float actualValue = processedBuffer.getSample(ch, i);

                // Allow small floating point error
                float difference = std::abs(expectedValue - actualValue);
                if (difference > 0.0001f)
                {
                    INFO("Channel " << ch << ", Sample " << i << ": Expected " << expectedValue << ", Got " << actualValue);
                    REQUIRE(difference <= 0.002f);
                }
            }
        }
    }

    SECTION("Compare input and processed buffers - Granulator processor")
    {
        // Switch to granulator processor
        processor.setActiveProcessor(ActiveProcessor::Granulator);
        auto* granulatorNode = processor.getGranulatorNode();
        REQUIRE(granulatorNode != nullptr);

        auto testOutputFile = outputDir.getChildFile("Somewhere_Mono_Buffer_Test_Granulator.wav");

        bool success = processor.processFile(inputFile, testOutputFile);
        if (!success)
            INFO("Error: " << processor.getLastError().toStdString());
        REQUIRE(success == true);

        // Access the internal buffers
        auto& inputBuffer = processor.getInputBuffer();
        auto& processedBuffer = processor.getProcessedBuffer();

        // Verify buffers are populated
        REQUIRE(inputBuffer.getNumSamples() > 0);
        REQUIRE(processedBuffer.getNumSamples() > 0);
        REQUIRE(inputBuffer.getNumChannels() == 2);
        REQUIRE(processedBuffer.getNumChannels() == 2);

        int latency = processor.getLatencySamples();
        // Verify buffers have same size
        REQUIRE(inputBuffer.getNumSamples() + latency == processedBuffer.getNumSamples());

        // Verify output is not silent
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

    SECTION("Output length differs between Gain and Granulator processors due to latency")
    {
        auto* gainNode = processor.getGainNode();
        gainNode->setGain(1.0f);

        auto gainOutputFile = outputDir.getChildFile("Latency_Test_Gain.wav");
        bool success = processor.processFile(inputFile, gainOutputFile);
        REQUIRE(success == true);

        // Get processed buffer length for Gain
        auto& gainProcessedBuffer = processor.getProcessedBuffer();
        int gainOutputLength = gainProcessedBuffer.getNumSamples();

        // Process with Granulator processor
        processor.setActiveProcessor(ActiveProcessor::Granulator);
        auto* granulatorNode = processor.getGranulatorNode();
        REQUIRE(granulatorNode != nullptr);

        auto granulatorOutputFile = outputDir.getChildFile("Latency_Test_Granulator.wav");
        success = processor.processFile(inputFile, granulatorOutputFile);
        REQUIRE(success == true);

        // Get processed buffer length for Granulator
        auto& granulatorProcessedBuffer = processor.getProcessedBuffer();
        int granulatorOutputLength = granulatorProcessedBuffer.getNumSamples();

        // Verify Granulator output is 512 samples longer (latency from minLookaheadSize)
        INFO("Gain output length: " << gainOutputLength);
        INFO("Granulator output length: " << granulatorOutputLength);
        INFO("Difference: " << (granulatorOutputLength - gainOutputLength));

        REQUIRE(granulatorOutputLength == gainOutputLength + 512);
    }
}
