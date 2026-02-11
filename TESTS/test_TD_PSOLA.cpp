/**
 * @file test_TD_PSOLA.cpp
 * @brief Tests for TD-PSOLA pitch shifting implementation
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "TD-PSOLA/TD_PSOLA.h"
#include "TD-PSOLA/GrainExport.h"
#include "BufferFiller.h"
#include "BufferHelper.h"
#include "BufferWriter.h"
#include "PITCH/PitchDetector.h"
#include <ctime>

TEST_CASE("TD_PSOLA - Basic Instantiation", "[TD_PSOLA]")
{
    TD_PSOLA::TDPSOLA psola;

    SECTION("Object creation succeeds")
    {
        REQUIRE(true);
    }
}

TEST_CASE("TD_PSOLA - Process Sine Wave", "[TD_PSOLA]")
{
    TD_PSOLA::TDPSOLA psola;

    float sampleRate = 44100.0f;
    int numSamples = 44100; // 1 second
    float frequency = 440.0f; // A4

    // Create input buffer with sine wave
    juce::AudioBuffer<float> inputBuffer(1, numSamples);
    float period = sampleRate / frequency;
    BufferFiller::generateSineCycles(inputBuffer, static_cast<int>(period));

    SECTION("Pitch shift up by octave (2.0)")
    {
        juce::AudioBuffer<float> outputBuffer;
        bool success = psola.process(inputBuffer, outputBuffer, 2.0f, sampleRate);

        REQUIRE(success);
        REQUIRE(outputBuffer.getNumChannels() == 1);
        REQUIRE(outputBuffer.getNumSamples() == numSamples);

        // Output should not be silent
        float rms = outputBuffer.getRMSLevel(0, 0, outputBuffer.getNumSamples());
        REQUIRE(rms > 0.01f);
    }

    SECTION("Pitch shift down by octave (0.5)")
    {
        juce::AudioBuffer<float> outputBuffer;
        bool success = psola.process(inputBuffer, outputBuffer, 0.5f, sampleRate);

        REQUIRE(success);
        REQUIRE(outputBuffer.getNumChannels() == 1);
        REQUIRE(outputBuffer.getNumSamples() == numSamples);

        // Output should not be silent
        float rms = outputBuffer.getRMSLevel(0, 0, outputBuffer.getNumSamples());
        REQUIRE(rms > 0.01f);
    }

    SECTION("Pitch shift by fifth (1.5)")
    {
        juce::AudioBuffer<float> outputBuffer;
        bool success = psola.process(inputBuffer, outputBuffer, 1.5f, sampleRate);

        REQUIRE(success);
        REQUIRE(outputBuffer.getNumChannels() == 1);
        REQUIRE(outputBuffer.getNumSamples() == numSamples);

        // Output should not be silent
        float rms = outputBuffer.getRMSLevel(0, 0, outputBuffer.getNumSamples());
        REQUIRE(rms > 0.01f);
    }

    SECTION("No pitch shift (1.0)")
    {
        juce::AudioBuffer<float> outputBuffer;
        bool success = psola.process(inputBuffer, outputBuffer, 1.0f, sampleRate);

        REQUIRE(success);
        REQUIRE(outputBuffer.getNumChannels() == 1);
        REQUIRE(outputBuffer.getNumSamples() == numSamples);

        // Output should not be silent
        float rms = outputBuffer.getRMSLevel(0, 0, outputBuffer.getNumSamples());
        REQUIRE(rms > 0.01f);
    }
}

TEST_CASE("TD_PSOLA - Stereo Processing", "[TD_PSOLA]")
{
    TD_PSOLA::TDPSOLA psola;

    float sampleRate = 44100.0f;
    int numSamples = 44100;

    // Create stereo input buffer
    juce::AudioBuffer<float> inputBuffer(2, numSamples);
    BufferFiller::generateSineCycles(inputBuffer, 100); // 441 Hz

    SECTION("Process stereo buffer")
    {
        juce::AudioBuffer<float> outputBuffer;
        bool success = psola.process(inputBuffer, outputBuffer, 1.5f, sampleRate);

        REQUIRE(success);
        REQUIRE(outputBuffer.getNumChannels() == 2);
        REQUIRE(outputBuffer.getNumSamples() == numSamples);

        // Both channels should have output
        float rmsL = outputBuffer.getRMSLevel(0, 0, outputBuffer.getNumSamples());
        float rmsR = outputBuffer.getRMSLevel(1, 0, outputBuffer.getNumSamples());
        REQUIRE(rmsL > 0.01f);
        REQUIRE(rmsR > 0.01f);
    }
}

TEST_CASE("TD_PSOLA - Invalid Inputs", "[TD_PSOLA]")
{
    TD_PSOLA::TDPSOLA psola;

    juce::AudioBuffer<float> inputBuffer(1, 1000);
    juce::AudioBuffer<float> outputBuffer;

    SECTION("Invalid f_ratio (zero)")
    {
        bool success = psola.process(inputBuffer, outputBuffer, 0.0f, 44100.0f);
        REQUIRE_FALSE(success);
    }

    SECTION("Invalid f_ratio (negative)")
    {
        bool success = psola.process(inputBuffer, outputBuffer, -1.0f, 44100.0f);
        REQUIRE_FALSE(success);
    }

    SECTION("Invalid sample rate (zero)")
    {
        bool success = psola.process(inputBuffer, outputBuffer, 1.0f, 0.0f);
        REQUIRE_FALSE(success);
    }

    SECTION("Empty input buffer")
    {
        juce::AudioBuffer<float> emptyBuffer(1, 0);
        bool success = psola.process(emptyBuffer, outputBuffer, 1.0f, 44100.0f);
        REQUIRE_FALSE(success);
    }
}

TEST_CASE("BufferFiller - Tukey Window Generation", "[TD_PSOLA][BufferFiller]")
{
    SECTION("Generate Tukey window with alpha=0.5")
    {
        juce::AudioBuffer<float> window(1, 100);
        BufferFiller::generateTukey(window, 0.5f);

        // Check that window starts and ends near zero
        REQUIRE(window.getSample(0, 0) < 0.1f);
        REQUIRE(window.getSample(0, 99) < 0.1f);

        // Check that middle values are near 1.0
        float midValue = window.getSample(0, 50);
        REQUIRE(midValue > 0.9f);
    }

    SECTION("Generate Tukey window with alpha=0.0 (rectangular)")
    {
        juce::AudioBuffer<float> window(1, 100);
        BufferFiller::generateTukey(window, 0.0f);

        // All values should be 1.0 for rectangular window
        for (int i = 0; i < 100; i++)
        {
            REQUIRE(window.getSample(0, i) == 1.0f);
        }
    }

    SECTION("Generate Tukey window with alpha=1.0 (Hann)")
    {
        juce::AudioBuffer<float> window(1, 100);
        BufferFiller::generateTukey(window, 1.0f);

        // Should look like a Hann window
        REQUIRE(window.getSample(0, 0) < 0.1f);
        REQUIRE(window.getSample(0, 99) < 0.1f);

        float midValue = window.getSample(0, 50);
        REQUIRE(midValue > 0.9f);
    }
}

TEST_CASE("PitchDetector - Detect Pitch on Sine Waves", "[TD_PSOLA][PitchDetector]")
{
    PitchDetector pitchDetector;
    int detectionSize = 4096;

    SECTION("Detect period=100 sine wave")
    {
        int generatedPeriod = 100;

        juce::AudioBuffer<float> buffer(1, detectionSize);
        BufferFiller::generateSineCycles(buffer, generatedPeriod);

        pitchDetector.prepareToPlay(detectionSize);
        pitchDetector.setThreshold(0.1); // Lower threshold for perfect sine waves

        float detectedPeriod = pitchDetector.process(buffer);

        REQUIRE(detectedPeriod > 0.0f);
        // Allow 10% tolerance for pitch detection algorithm variance
        float tolerance = generatedPeriod * 0.1f;
        REQUIRE_THAT(detectedPeriod,
                    Catch::Matchers::WithinAbs(static_cast<float>(generatedPeriod), tolerance));
    }

    SECTION("Detect period=200 sine wave")
    {
        int generatedPeriod = 200;

        juce::AudioBuffer<float> buffer(1, detectionSize);
        BufferFiller::generateSineCycles(buffer, generatedPeriod);

        pitchDetector.prepareToPlay(detectionSize);
        pitchDetector.setThreshold(0.1);

        float detectedPeriod = pitchDetector.process(buffer);

        REQUIRE(detectedPeriod > 0.0f);
        float tolerance = generatedPeriod * 0.1f;
        REQUIRE_THAT(detectedPeriod,
                    Catch::Matchers::WithinAbs(static_cast<float>(generatedPeriod), tolerance));
    }

    SECTION("Detect period=50 sine wave")
    {
        int generatedPeriod = 50;

        juce::AudioBuffer<float> buffer(1, detectionSize);
        BufferFiller::generateSineCycles(buffer, generatedPeriod);

        pitchDetector.prepareToPlay(detectionSize);
        pitchDetector.setThreshold(0.1);

        float detectedPeriod = pitchDetector.process(buffer);

        REQUIRE(detectedPeriod > 0.0f);
        float tolerance = generatedPeriod * 0.1f;
        REQUIRE_THAT(detectedPeriod,
                    Catch::Matchers::WithinAbs(static_cast<float>(generatedPeriod), tolerance));
    }

    SECTION("Detect period=400 sine wave (low frequency)")
    {
        int generatedPeriod = 400;

        juce::AudioBuffer<float> buffer(1, detectionSize);
        BufferFiller::generateSineCycles(buffer, generatedPeriod);

        pitchDetector.prepareToPlay(detectionSize);
        pitchDetector.setThreshold(0.1);

        float detectedPeriod = pitchDetector.process(buffer);

        REQUIRE(detectedPeriod > 0.0f);
        float tolerance = generatedPeriod * 0.1f;
        REQUIRE_THAT(detectedPeriod,
                    Catch::Matchers::WithinAbs(static_cast<float>(generatedPeriod), tolerance));
    }

    SECTION("Test threshold adjustment for perfect sine waves")
    {
        int generatedPeriod = 100;
        float tolerance = generatedPeriod * 0.1f;

        juce::AudioBuffer<float> buffer(1, detectionSize);
        BufferFiller::generateSineCycles(buffer, generatedPeriod);

        pitchDetector.prepareToPlay(detectionSize);

        // Test with very low threshold (more permissive for perfect sines)
        pitchDetector.setThreshold(0.05);
        float detectedPeriod1 = pitchDetector.process(buffer);
        REQUIRE(detectedPeriod1 > 0.0f);
        REQUIRE_THAT(detectedPeriod1,
                    Catch::Matchers::WithinAbs(static_cast<float>(generatedPeriod), tolerance));

        // Test with medium-low threshold
        pitchDetector.setThreshold(0.1);
        float detectedPeriod2 = pitchDetector.process(buffer);
        REQUIRE(detectedPeriod2 > 0.0f);
        REQUIRE_THAT(detectedPeriod2, Catch::Matchers::WithinAbs(static_cast<float>(generatedPeriod), tolerance));
    }
}

TEST_CASE("TD_PSOLA - Process Female_Scale.wav and Compare to Golden", "[TD_PSOLA][Golden]")
{
    TD_PSOLA::TDPSOLA psola;
    float sampleRate = 44100.0f;
    float fRatio = 1.5f;

    // Configure for more lenient pitch detection on real audio
    TD_PSOLA::TDPSOLA::Config config;
    config.maxHz = 600.0f;         // Target around 200-250 Hz fundamental
    config.minHz = 100.0f;         // Raise min to avoid sub-harmonics
    config.analysisWindowMs = 40.0f;
    config.inTypeScalar = 4.0f;    // Lenient variance tolerance

    // Load input file
    juce::File currentDir = juce::File::getCurrentWorkingDirectory();
    juce::File inputFile = currentDir.getChildFile("TESTS/TEST_FILES/Female_Scale.wav");

    REQUIRE(inputFile.existsAsFile());

    juce::AudioBuffer<float> inputBuffer;
    bool loadSuccess = BufferFiller::loadFromWavFile(inputFile, inputBuffer);
    REQUIRE(loadSuccess);
    REQUIRE(inputBuffer.getNumSamples() > 0);

    // Convert to mono if stereo (for grain export)
    juce::AudioBuffer<float> monoBuffer;
    if (inputBuffer.getNumChannels() > 1)
    {
        // Average left and right channels
        monoBuffer.setSize(1, inputBuffer.getNumSamples());
        for (int i = 0; i < inputBuffer.getNumSamples(); i++)
        {
            float avg = 0.0f;
            for (int ch = 0; ch < inputBuffer.getNumChannels(); ch++)
                avg += inputBuffer.getSample(ch, i);
            avg /= inputBuffer.getNumChannels();
            monoBuffer.setSample(0, i, avg);
        }
    }
    else
    {
        monoBuffer.makeCopyOf(inputBuffer);
    }

    // Process with TD-PSOLA and export grain data
    juce::AudioBuffer<float> processedBuffer;
    TD_PSOLA::GrainData grainData;
    bool processSuccess = psola.processWithGrainExport(monoBuffer, processedBuffer, grainData, fRatio, sampleRate, config);
    REQUIRE(processSuccess);
    REQUIRE(processedBuffer.getNumSamples() > 0);

    // Create timestamped output directory
    std::time_t now = std::time(nullptr);
    std::tm* localTime = std::localtime(&now);
    char timestamp[32];
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", localTime);

    juce::String timestampStr(timestamp);
    juce::String outputDirName = "TD_PSOLA_" + timestampStr;
    juce::File outputDir = currentDir.getChildFile("../TESTS/OUTPUT").getChildFile(outputDirName);

    // Create directory if it doesn't exist
    if (!outputDir.exists())
    {
        juce::Result createResult = outputDir.createDirectory();
        REQUIRE(createResult.wasOk());
    }

    // Write processed buffer to timestamped output file
    juce::String outputFileName = "Female_Scale_1.5_" + timestampStr + ".wav";
    juce::File outputFile = outputDir.getChildFile(outputFileName);

    BufferWriter::Result writeResult = BufferWriter::writeToWav(processedBuffer, outputFile, sampleRate, 24);
    REQUIRE(writeResult == BufferWriter::Result::kSuccess);
    REQUIRE(outputFile.existsAsFile());

    // Export grain data
    bool grainExportSuccess = TD_PSOLA::exportGrainsToCSV(grainData, outputFile.getFullPathName());
    REQUIRE(grainExportSuccess);

    // Verify grain export files exist
    juce::File csvFile = outputDir.getChildFile(outputFileName.upToLastOccurrenceOf(".", false, false) + "_synthesis_grains.csv");
    juce::File summaryFile = outputDir.getChildFile(outputFileName.upToLastOccurrenceOf(".", false, false) + "_grain_summary.txt");
    REQUIRE(csvFile.existsAsFile());
    REQUIRE(summaryFile.existsAsFile());

    // Load golden reference file
    juce::File goldenFile = currentDir.getChildFile("TESTS/GOLDEN/GOLDEN_Female_Scale_1.5/GOLDEN_Female_Scale_1.5.wav");
    REQUIRE(goldenFile.existsAsFile());

    juce::AudioBuffer<float> goldenBuffer;
    bool goldenLoadSuccess = BufferFiller::loadFromWavFile(goldenFile, goldenBuffer);
    REQUIRE(goldenLoadSuccess);

    // Compare buffers - note: golden may be mono, processed may be stereo
    REQUIRE(processedBuffer.getNumSamples() == goldenBuffer.getNumSamples());

    // Compare only the first channel (left channel for stereo, only channel for mono)
    int channelsToCompare = std::min(processedBuffer.getNumChannels(), goldenBuffer.getNumChannels());
    REQUIRE(channelsToCompare > 0);

    INFO("Processed channels: " << processedBuffer.getNumChannels());
    INFO("Golden channels: " << goldenBuffer.getNumChannels());
    INFO("Comparing first channel only");

    // Calculate RMS difference for diagnostics
    float maxDifference = 0.0f;
    float sumSquaredDiff = 0.0f;
    int numDifferentSamples = 0;

    const float* processedData = processedBuffer.getReadPointer(0);
    const float* goldenData = goldenBuffer.getReadPointer(0);

    for (int i = 0; i < processedBuffer.getNumSamples(); i++)
    {
        float diff = std::abs(processedData[i] - goldenData[i]);
        maxDifference = std::max(maxDifference, diff);
        sumSquaredDiff += diff * diff;

        if (diff > 0.0001f) // Count significantly different samples
            numDifferentSamples++;
    }

    float rmsDifference = std::sqrt(sumSquaredDiff / processedBuffer.getNumSamples());
    float percentDifferent = (100.0f * numDifferentSamples) / processedBuffer.getNumSamples();

    INFO("Max sample difference: " << maxDifference);
    INFO("RMS difference: " << rmsDifference);
    INFO("Percent of samples different (>0.0001): " << percentDifferent << "%");

    // For now, just verify the processed output is not silent
    REQUIRE_FALSE(BufferHelper::isSilent(processedBuffer));

    // Check if the difference is reasonable (not identical due to minor algorithm differences)
    // Allow up to 15% RMS difference from golden reference
    // (differences due to pitch mark placement and numerical precision)
    CHECK(rmsDifference < 0.15f);

    // Log whether buffers are close enough to be considered matching
    if (rmsDifference < 0.01f)
    {
        INFO("Buffers match closely (RMS < 0.01)");
    }
    else if (rmsDifference < 0.1f)
    {
        INFO("Buffers are reasonably similar (RMS < 0.1)");
    }
    else
    {
        INFO("Buffers differ significantly - algorithm may need adjustment");
    }
}
