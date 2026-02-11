/**
 * Tests for GranulatorProcessor with grain export similar to TD_PSOLA tests
 */

#include <catch2/catch_test_macros.hpp>
#include "../SOURCE/Util/Juce_Header.h"
#include "../SUBMODULES/RD/SOURCE/PROCESSORS/GRAIN/GranulatorProcessor.h"
#include "../SUBMODULES/RD/SOURCE/PROCESSORS/GRAIN/Granulator.h"
#include "../SUBMODULES/RD/SOURCE/PROCESSORS/GRAIN/Grain.h"
#include "../SUBMODULES/RD/SOURCE/BufferFiller.h"
#include "../SUBMODULES/RD/SOURCE/BufferWriter.h"
#include "../SUBMODULES/RD/SOURCE/AudioFileHelpers.h"
#include "TEST_UTILS/TestUtils.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>

//==============================================================================
// Grain data structures for export (similar to TD_PSOLA::GrainData)
//==============================================================================

struct GranulatorGrainSnapshot
{
    int grainId;               // Sequential grain index (assigned by tracker)
    int grainSlot;             // Which of the 4 grain slots this used (0-3)

    // Analysis range (where audio was read from)
    juce::int64 sourceStart;
    juce::int64 sourceCenter;
    juce::int64 sourceEnd;

    // Synthesis range (where audio was written to)
    juce::int64 synthStart;
    juce::int64 synthCenter;
    juce::int64 synthEnd;

    int grainSize;             // Total grain length in samples
    float detectedPeriod;      // Period at time of grain creation
};

struct GranulatorGrainHistory
{
    float shiftRatio;
    int signalLength;
    std::vector<GranulatorGrainSnapshot> grains;
};

//==============================================================================
// Grain tracking helper
//==============================================================================

class GrainTracker
{
public:
    void captureGrains(GranulatorProcessor& processor, float detectedPeriod)
    {
        auto& granulator = processor.getGranulator();
        auto& grains = granulator.getGrains();

        for (int i = 0; i < kNumGrains; ++i)
        {
            const auto& grain = grains[i];
            if (!grain.isActive)
                continue;

            // Check if we've already tracked this grain
            // Use synthesis center as unique identifier
            juce::int64 synthCenter = std::get<1>(grain.mSynthRange);

            bool alreadyTracked = false;
            for (const auto& tracked : mTrackedSynthCenters)
            {
                if (tracked == synthCenter)
                {
                    alreadyTracked = true;
                    break;
                }
            }

            if (alreadyTracked)
                continue;

            // New grain - capture its data
            GranulatorGrainSnapshot snapshot;
            snapshot.grainId = mNextGrainId++;
            snapshot.grainSlot = i;

            auto analysisRange = grain.mAnalysisRange;
            snapshot.sourceStart = std::get<0>(analysisRange);
            snapshot.sourceCenter = std::get<1>(analysisRange);
            snapshot.sourceEnd = std::get<2>(analysisRange);

            auto synthRange = grain.mSynthRange;
            snapshot.synthStart = std::get<0>(synthRange);
            snapshot.synthCenter = std::get<1>(synthRange);
            snapshot.synthEnd = std::get<2>(synthRange);

            snapshot.grainSize = grain.mGrainSize;
            snapshot.detectedPeriod = detectedPeriod;

            mGrains.push_back(snapshot);
            mTrackedSynthCenters.push_back(synthCenter);
        }
    }

    const std::vector<GranulatorGrainSnapshot>& getGrains() const { return mGrains; }

    void reset()
    {
        mGrains.clear();
        mTrackedSynthCenters.clear();
        mNextGrainId = 0;
    }

private:
    std::vector<GranulatorGrainSnapshot> mGrains;
    std::vector<juce::int64> mTrackedSynthCenters;
    int mNextGrainId = 0;
};

//==============================================================================
// Export functions
//==============================================================================

bool exportGrainHistoryToCSV(const GranulatorGrainHistory& history, const juce::String& outputPath)
{
    // Remove extension from output path to create base path
    juce::String basePath = outputPath.upToLastOccurrenceOf(".", false, false);

    // Export grains CSV
    juce::String csvPath = basePath + "_synthesis_grains.csv";
    juce::File csvFile(csvPath);

    std::ofstream csvStream(csvFile.getFullPathName().toStdString());
    if (!csvStream.is_open())
        return false;

    // Write CSV header (matching TD_PSOLA format where possible)
    csvStream << "grain_id,grain_slot,source_start,source_center,source_end,";
    csvStream << "synth_start,synth_center,synth_end,grain_size,detected_period\n";

    // Write grain data
    for (const auto& grain : history.grains)
    {
        csvStream << grain.grainId << ","
                  << grain.grainSlot << ","
                  << grain.sourceStart << ","
                  << grain.sourceCenter << ","
                  << grain.sourceEnd << ","
                  << grain.synthStart << ","
                  << grain.synthCenter << ","
                  << grain.synthEnd << ","
                  << grain.grainSize << ","
                  << grain.detectedPeriod << "\n";
    }

    csvStream.close();

    // Export summary text file
    juce::String summaryPath = basePath + "_grain_summary.txt";
    juce::File summaryFile(summaryPath);

    std::ofstream summaryStream(summaryFile.getFullPathName().toStdString());
    if (!summaryStream.is_open())
        return false;

    summaryStream << "GranulatorProcessor Grain Analysis Summary\n";
    summaryStream << "==================================================\n\n";
    summaryStream << "Pitch Shift Ratio: " << history.shiftRatio << "\n";
    summaryStream << "Signal Length: " << history.signalLength << " samples\n";
    summaryStream << "Number of Grains Captured: " << history.grains.size() << "\n\n";

    // Calculate average grain size
    if (!history.grains.empty())
    {
        double avgGrainSize = 0.0;
        double avgDetectedPeriod = 0.0;
        for (const auto& grain : history.grains)
        {
            avgGrainSize += grain.grainSize;
            avgDetectedPeriod += grain.detectedPeriod;
        }
        avgGrainSize /= history.grains.size();
        avgDetectedPeriod /= history.grains.size();

        summaryStream << "Average Grain Size: " << avgGrainSize << " samples\n";
        summaryStream << "Average Detected Period: " << avgDetectedPeriod << " samples\n\n";
    }

    summaryStream << "Note: GranulatorProcessor uses 4 active grain slots that recycle.\n";
    summaryStream << "Grains are captured when newly created, not all active grains each block.\n";

    summaryStream.close();

    return true;
}

//==============================================================================
// Tests
//==============================================================================

TEST_CASE("GranulatorProcessor - Female_Scale.wav with grain export", "[GranulatorProcessor][grain_export]")
{
    // Create timestamped output directory
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
    juce::String timestamp(ss.str());

    juce::String outputDirName = "GRANULATOR_Female_Scale_1.5_" + timestamp;
    juce::File outputDir = juce::File::getCurrentWorkingDirectory()
                            .getChildFile("../TESTS/OUTPUT")
                            .getChildFile(outputDirName);

    REQUIRE(outputDir.createDirectory());

    // Load input file
    juce::File currentDir = juce::File::getCurrentWorkingDirectory();
    juce::File inputFile = currentDir.getChildFile("../TESTS/TEST_FILES/Female_Scale.wav");
    REQUIRE(inputFile.existsAsFile());

    juce::AudioBuffer<float> inputBuffer;
    bool loadSuccess = BufferFiller::loadFromWavFile(inputFile, inputBuffer);
    REQUIRE(loadSuccess);
    REQUIRE(inputBuffer.getNumSamples() > 0);

    // Get sample rate from file
    double sampleRate = AudioFileHelpers::getFileSampleRate(inputFile);

    const int numInputSamples = inputBuffer.getNumSamples();
    const int numChannels = inputBuffer.getNumChannels();

    // Create GranulatorProcessor
    GranulatorProcessor processor;

    // Set pitch shift ratio to 1.5 (up a perfect fifth)
    float shiftRatio = 1.5f;
    auto& apvts = processor.getAPVTS();
    auto* shiftParam = apvts.getParameter("shift ratio");
    REQUIRE(shiftParam != nullptr);
    shiftParam->setValueNotifyingHost(shiftParam->convertTo0to1(shiftRatio));

    // Prepare processor
    const int blockSize = 512;
    processor.prepareToPlay(sampleRate, blockSize);

    // Create grain tracker
    GrainTracker grainTracker;

    // Process in blocks
    juce::AudioBuffer<float> outputBuffer;
    outputBuffer.setSize(numChannels, numInputSamples);
    outputBuffer.clear();

    juce::MidiBuffer midiBuffer;
    int numBlocksProcessed = 0;

    for (int startSample = 0; startSample < numInputSamples; startSample += blockSize)
    {
        const int samplesThisBlock = std::min(blockSize, numInputSamples - startSample);

        // Create a block buffer
        juce::AudioBuffer<float> blockBuffer(numChannels, samplesThisBlock);

        // Copy input to block
        for (int ch = 0; ch < numChannels; ++ch)
        {
            blockBuffer.copyFrom(ch, 0, inputBuffer, ch, startSample, samplesThisBlock);
        }

        // Process the block
        processor.processBlock(blockBuffer, midiBuffer);

        // Copy processed block to output
        for (int ch = 0; ch < numChannels; ++ch)
        {
            outputBuffer.copyFrom(ch, startSample, blockBuffer, ch, 0, samplesThisBlock);
        }

        // Capture grain data after processing
        float detectedPeriod = processor.getLastDetectedPeriod();
        if (detectedPeriod > 0.0f)
        {
            grainTracker.captureGrains(processor, detectedPeriod);
        }

        numBlocksProcessed++;
    }

    // Write output audio file
    juce::String outputFileName = "Female_Scale_1.5_" + timestamp + ".wav";
    juce::File outputFile = outputDir.getChildFile(outputFileName);
    BufferWriter::Result writeResult = BufferWriter::writeToWav(outputBuffer, outputFile, sampleRate, 24);
    REQUIRE(writeResult == BufferWriter::Result::kSuccess);
    REQUIRE(outputFile.existsAsFile());

    // Create grain history for export
    GranulatorGrainHistory history;
    history.shiftRatio = shiftRatio;
    history.signalLength = numInputSamples;
    history.grains = grainTracker.getGrains();

    // Export grain data
    REQUIRE(exportGrainHistoryToCSV(history, outputFile.getFullPathName()));

    // Verify we captured some grains
    INFO("Captured " << history.grains.size() << " grains");
    CHECK(history.grains.size() > 0);

    // Log output location
    std::cout << "\nGranulatorProcessor test output:" << std::endl;
    std::cout << "  Directory: " << outputDir.getFullPathName() << std::endl;
    std::cout << "  Audio file: " << outputFileName << std::endl;
    std::cout << "  Grains captured: " << history.grains.size() << std::endl;
    std::cout << "  Blocks processed: " << numBlocksProcessed << std::endl;
}
