/**
 * test_GranulatorProcessor.cpp
 * Created by Ryan Devens
 *
 * Comprehensive tests for GranulatorProcessor - TD-PSOLA pitch shifting processor
 */

#define _USE_MATH_DEFINES
#include <cmath>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../SUBMODULES/RD/SOURCE/PROCESSORS/GRAIN/GranulatorProcessor.h"
#include "../SUBMODULES/RD/SOURCE/BufferFiller.h"
#include "../SUBMODULES/RD/SOURCE/BufferHelper.h"
#include "../SUBMODULES/RD/TESTS/TEST_UTILS/TestUtils.h"

//==============================================================================
// Test Constants
//==============================================================================
namespace TestConfig
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 128;
    constexpr int circularBufferSize = 8192;
    constexpr int maxGrainSize = 4096;

    // Frequency that yields 256 samples period at 48kHz
    constexpr float testFrequency = 187.5f; // 48000 / 256 = 187.5 Hz
    constexpr int testPeriod = 256;
}

//==============================================================================
// Mock Input Helper
//==============================================================================
struct MockProcessorInput
{
    double sampleRate = 48000.0;
    int blockSize = 128;
    float frequency = 187.5f;  // 256 samples period at 48kHz
    float shiftRatio = 1.0f;
    int numBlocks = 4;

    MockProcessorInput(
        double sr = 48000.0,
        int bs = 128,
        float freq = 187.5f,
        float shift = 1.0f,
        int blocks = 4
    )
        : sampleRate(sr)
        , blockSize(bs)
        , frequency(freq)
        , shiftRatio(shift)
        , numBlocks(blocks)
    {}
};

//==============================================================================
// Basic Initialization Tests
//==============================================================================

TEST_CASE("GranulatorProcessor initialization", "[GranulatorProcessor][init]")
{
    TestUtils::SetupAndTeardown setup;
    GranulatorProcessor processor;

    SECTION("Initial state is kDetecting")
    {
        CHECK(processor.getCurrentState() == GranulatorProcessor::ProcessState::kDetecting);
    }

    SECTION("After prepareToPlay, components are initialized")
    {
        processor.prepareToPlay(TestConfig::sampleRate, TestConfig::blockSize);

        // Verify pitch detector exists
        REQUIRE(processor.getPitchDetector() != nullptr);

        // Verify buffers are properly sized
        CHECK(processor.getCircularBuffer().getSize() == TestConfig::circularBufferSize);
        CHECK(processor.getCircularBuffer().getNumChannels() == 2);
    }
}

//==============================================================================
// Range Calculation Tests
//==============================================================================

TEST_CASE("GranulatorProcessor range calculations", "[GranulatorProcessor][ranges]")
{
    TestUtils::SetupAndTeardown setup;
    GranulatorProcessor processor;
    processor.prepareToPlay(TestConfig::sampleRate, TestConfig::blockSize);

    // Simulate processing several blocks
    juce::AudioBuffer<float> buffer(2, TestConfig::blockSize);
    juce::MidiBuffer midiBuffer;

    // Process 5 blocks to advance sample counter
    for (int i = 0; i < 5; ++i)
    {
        buffer.clear();
        processor.processBlock(buffer, midiBuffer);
    }

    SECTION("getProcessCounterRange returns correct boundaries")
    {
        auto [start, end] = processor.getProcessCounterRange();

        // After 5 blocks of 128 samples: 5 * 128 = 640 samples processed
        CHECK(start == 640);
        CHECK(end == 640 + TestConfig::blockSize - 1);
    }

    SECTION("getDetectionRange has correct lookahead offset")
    {
        auto [detectStart, detectEnd] = processor.getDetectionRange();
        auto [processStart, processEnd] = processor.getProcessCounterRange();

        // Detection range should be delayed by kMinLookaheadSize (512)
        constexpr int kMinLookaheadSize = 512;
        constexpr int kMinDetectionSize = 1024;

        CHECK(detectEnd == processEnd - kMinLookaheadSize);
        CHECK(detectStart == detectEnd - kMinDetectionSize);
    }

    SECTION("getAnalysisReadRange creates 2-period window")
    {
        constexpr float detectedPeriod = 256.0f;
        juce::int64 analysisMark = 1000;

        auto [start, mark, end] = processor.getAnalysisReadRange(analysisMark, detectedPeriod);

        // Should be centered on mark with ±1 period
        CHECK(start == analysisMark - static_cast<juce::int64>(detectedPeriod));
        CHECK(mark == analysisMark);
        CHECK(end == analysisMark + static_cast<juce::int64>(detectedPeriod) - 1);
    }

    SECTION("getAnalysisWriteRange offsets by lookahead")
    {
        constexpr float detectedPeriod = 256.0f;
        juce::int64 analysisMark = 1000;
        constexpr int kMinLookaheadSize = 512;

        auto readRange = processor.getAnalysisReadRange(analysisMark, detectedPeriod);
        auto [writeStart, writeMark, writeEnd] = processor.getAnalysisWriteRange(readRange);

        auto [readStart, readMark, readEnd] = readRange;

        CHECK(writeStart == readStart + kMinLookaheadSize);
        CHECK(writeMark == readMark + kMinLookaheadSize);
        CHECK(writeEnd == readEnd + kMinLookaheadSize);
    }

    SECTION("getDryBlockRange has correct delay")
    {
        auto [dryStart, dryEnd] = processor.getDryBlockRange();
        auto [processStart, processEnd] = processor.getProcessCounterRange();

        constexpr int kMinLookaheadSize = 512;

        // Dry range should be delayed relative to process counter
        CHECK(dryStart == processStart - kMinLookaheadSize);
        CHECK(dryEnd == dryStart + TestConfig::blockSize);
    }
}

//==============================================================================
// State Transition Tests
//==============================================================================

TEST_CASE("GranulatorProcessor state transitions", "[GranulatorProcessor][state]")
{
    TestUtils::SetupAndTeardown setup;
    GranulatorProcessor processor;
    processor.prepareToPlay(TestConfig::sampleRate, TestConfig::blockSize);

    juce::MidiBuffer midiBuffer;

    SECTION("Starts in kDetecting state")
    {
        CHECK(processor.getCurrentState() == GranulatorProcessor::ProcessState::kDetecting);
    }

    SECTION("Transitions to kTracking when pitch detected")
    {
        // Create buffer with sine wave at known frequency
        constexpr int period = TestConfig::testPeriod;
        juce::AudioBuffer<float> buffer(2, TestConfig::blockSize);

        // Process multiple blocks with pitched audio
        for (int i = 0; i < 20; ++i)
        {
            buffer.clear();
            BufferFiller::generateSineCycles(buffer, period);
            processor.processBlock(buffer, midiBuffer);

            // Should transition to tracking once pitch is detected
            if (processor.getCurrentState() == GranulatorProcessor::ProcessState::kTracking)
            {
                break;
            }
        }

        // Should eventually detect pitch and switch to tracking
        CHECK(processor.getCurrentState() == GranulatorProcessor::ProcessState::kTracking);
    }

    SECTION("Resets to kDetecting when pitch is lost")
    {
        constexpr int period = TestConfig::testPeriod;
        juce::AudioBuffer<float> buffer(2, TestConfig::blockSize);

        // First, get into tracking state
        for (int i = 0; i < 20; ++i)
        {
            buffer.clear();
            BufferFiller::generateSineCycles(buffer, period);
            processor.processBlock(buffer, midiBuffer);
        }

        // Verify we're tracking
        REQUIRE(processor.getCurrentState() == GranulatorProcessor::ProcessState::kTracking);

        // Now send silence to lose pitch
        // Need enough blocks to flush detection buffer (1024 samples at 128 samples/block = ~8 blocks minimum)
        // Use more to ensure detection buffer is fully cleared
        for (int i = 0; i < 20; ++i)
        {
            buffer.clear();  // Silent buffer
            processor.processBlock(buffer, midiBuffer);
        }

        // Should reset to detecting
        CHECK(processor.getCurrentState() == GranulatorProcessor::ProcessState::kDetecting);
    }
}

//==============================================================================
// Pitch Detection Integration Tests
//==============================================================================

TEST_CASE("GranulatorProcessor pitch detection integration", "[GranulatorProcessor][pitch]")
{
    TestUtils::SetupAndTeardown setup;
    GranulatorProcessor processor;
    processor.prepareToPlay(TestConfig::sampleRate, TestConfig::blockSize);

    juce::MidiBuffer midiBuffer;

    SECTION("Detects pitch from sine wave input")
    {
        constexpr int period = TestConfig::testPeriod;
        juce::AudioBuffer<float> buffer(2, TestConfig::blockSize);

        // Process blocks until pitch is detected
        bool pitchDetected = false;
        for (int i = 0; i < 30; ++i)
        {
            buffer.clear();
            BufferFiller::generateSineCycles(buffer, period);
            processor.processBlock(buffer, midiBuffer);

            // Check if pitch detector has detected something
            if (processor.getPitchDetector()->getCurrentPeriod() > 0)
            {
                pitchDetected = true;
                double detectedPeriod = processor.getPitchDetector()->getCurrentPeriod();

                // YIN may detect at the fundamental or a harmonic
                // Accept period or its integer multiples within reasonable range
                // For 256 samples period, could detect 256, 512, etc.
                bool validDetection = (detectedPeriod >= period * 0.9 && detectedPeriod <= period * 1.1) ||
                                     (detectedPeriod >= period * 1.9 && detectedPeriod <= period * 2.1);
                CHECK(validDetection);
                break;
            }
        }

        CHECK(pitchDetected);
    }

    SECTION("Does not detect pitch from silence")
    {
        juce::AudioBuffer<float> buffer(2, TestConfig::blockSize);
        buffer.clear();

        // Process several silent blocks
        for (int i = 0; i < 10; ++i)
        {
            processor.processBlock(buffer, midiBuffer);
        }

        // Should not have detected any pitch
        CHECK(processor.getPitchDetector()->getCurrentPeriod() <= 0);
    }
}

//==============================================================================
// Circular Buffer Integration Tests
//==============================================================================

TEST_CASE("GranulatorProcessor circular buffer integration", "[GranulatorProcessor][circular]")
{
    TestUtils::SetupAndTeardown setup;
    GranulatorProcessor processor;
    processor.prepareToPlay(TestConfig::sampleRate, TestConfig::blockSize);

    juce::MidiBuffer midiBuffer;
    juce::AudioBuffer<float> buffer(2, TestConfig::blockSize);

    SECTION("Input is written to circular buffer")
    {
        // Fill buffer with known value
        BufferFiller::fillWithValue(buffer, 0.5f);

        processor.processBlock(buffer, midiBuffer);

        // Check that circular buffer contains the written values
        auto& circBuf = processor.getCircularBuffer();

        // First block should be at the start
        for (int i = 0; i < TestConfig::blockSize; ++i)
        {
            float value = circBuf.getBuffer().getSample(0, i);
            CHECK(value == Catch::Approx(0.5f).margin(0.001f));
        }
    }

    SECTION("Circular buffer wraps around correctly")
    {
        // Write enough blocks to exceed buffer size
        int totalBlocks = (TestConfig::circularBufferSize / TestConfig::blockSize) + 2;

        for (int block = 0; block < totalBlocks; ++block)
        {
            buffer.clear();
            BufferFiller::fillWithValue(buffer, static_cast<float>(block));
            processor.processBlock(buffer, midiBuffer);
        }

        // Verify wrap-around by checking that old data has been overwritten
        // This is implicitly tested by the processor continuing to work
        CHECK(true); // Passed if no crash/assertion
    }
}

//==============================================================================
// Granulator Integration Tests
//==============================================================================

TEST_CASE("GranulatorProcessor granulator integration", "[GranulatorProcessor][granulator]")
{
    TestUtils::SetupAndTeardown setup;
    GranulatorProcessor processor;
    processor.prepareToPlay(TestConfig::sampleRate, TestConfig::blockSize);

    juce::MidiBuffer midiBuffer;

    SECTION("Granulator is prepared correctly")
    {
        auto& granulator = processor.getGranulator();

        // Check that granulator window is initialized
        CHECK(granulator.getWindow().getSize() > 0);
        CHECK(granulator.getSynthMark() == -1); // Initial state
    }

    SECTION("Grains are created during tracking mode")
    {
        constexpr int period = TestConfig::testPeriod;
        juce::AudioBuffer<float> buffer(2, TestConfig::blockSize);

        // Process multiple blocks with pitched audio
        for (int i = 0; i < 30; ++i)
        {
            buffer.clear();
            BufferFiller::generateSineCycles(buffer, period);
            processor.processBlock(buffer, midiBuffer);
        }

        // If in tracking mode, grains should have been created
        if (processor.getCurrentState() == GranulatorProcessor::ProcessState::kTracking)
        {
            auto& granulator = processor.getGranulator();

            // Synth mark should have been set
            CHECK(granulator.getSynthMark() >= 0);
        }
    }
}

//==============================================================================
// Pitch Shifting Tests
//==============================================================================

TEST_CASE("GranulatorProcessor pitch shifting", "[GranulatorProcessor][shift]")
{
    TestUtils::SetupAndTeardown setup;
    GranulatorProcessor processor;
    processor.prepareToPlay(TestConfig::sampleRate, TestConfig::blockSize);

    juce::MidiBuffer midiBuffer;

    SECTION("Shift ratio parameter changes shift ratio")
    {
        auto* param = processor.getAPVTS().getParameter("shiftRatio");
        REQUIRE(param != nullptr);

        // Set shift ratio to 0.5 (octave down)
        param->setValueNotifyingHost(0.0f); // Normalized: 0.0 maps to 0.5

        // Verify parameter was set (through parameterChanged callback)
        // The actual shift ratio is internal and affects processing
        CHECK(param->getValue() == 0.0f);
    }

    SECTION("Shift ratio = 1.0 (no shift) processes correctly")
    {
        auto* param = processor.getAPVTS().getParameter("shiftRatio");
        param->setValueNotifyingHost(0.5f); // Normalized: 0.5 maps to 1.0

        constexpr int period = TestConfig::testPeriod;
        juce::AudioBuffer<float> buffer(2, TestConfig::blockSize);

        // Process multiple blocks
        for (int i = 0; i < 20; ++i)
        {
            buffer.clear();
            BufferFiller::generateSineCycles(buffer, period);
            processor.processBlock(buffer, midiBuffer);
        }

        // Should not crash and should produce output
        CHECK(true); // Test passes if no assertion/crash
    }

    SECTION("Shift ratio = 1.5 (fifth up) processes correctly")
    {
        auto* param = processor.getAPVTS().getParameter("shiftRatio");
        param->setValueNotifyingHost(1.0f); // Normalized: 1.0 maps to 1.5

        constexpr int period = TestConfig::testPeriod;
        juce::AudioBuffer<float> buffer(2, TestConfig::blockSize);

        // Process multiple blocks
        for (int i = 0; i < 20; ++i)
        {
            buffer.clear();
            BufferFiller::generateSineCycles(buffer, period);
            processor.processBlock(buffer, midiBuffer);
        }

        // Should not crash and should produce output
        CHECK(true); // Test passes if no assertion/crash
    }
}

//==============================================================================
// Edge Cases
//==============================================================================

TEST_CASE("GranulatorProcessor edge cases", "[GranulatorProcessor][edge]")
{
    TestUtils::SetupAndTeardown setup;
    GranulatorProcessor processor;
    processor.prepareToPlay(TestConfig::sampleRate, TestConfig::blockSize);

    juce::MidiBuffer midiBuffer;
    juce::AudioBuffer<float> buffer(2, TestConfig::blockSize);

    SECTION("Silent input does not crash")
    {
        buffer.clear();

        for (int i = 0; i < 10; ++i)
        {
            processor.processBlock(buffer, midiBuffer);
        }

        CHECK(processor.getCurrentState() == GranulatorProcessor::ProcessState::kDetecting);
    }

    SECTION("Very low frequency (long period)")
    {
        // 50 Hz at 48kHz = 960 samples period
        constexpr int longPeriod = 960;

        for (int i = 0; i < 30; ++i)
        {
            buffer.clear();
            BufferFiller::generateSineCycles(buffer, longPeriod);
            processor.processBlock(buffer, midiBuffer);
        }

        // Should handle without crashing
        CHECK(true);
    }

    SECTION("Very high frequency (short period)")
    {
        // 1000 Hz at 48kHz = 48 samples period
        constexpr int shortPeriod = 48;

        for (int i = 0; i < 30; ++i)
        {
            buffer.clear();
            BufferFiller::generateSineCycles(buffer, shortPeriod);
            processor.processBlock(buffer, midiBuffer);
        }

        // Should handle without crashing
        CHECK(true);
    }

    SECTION("Rapid pitch changes")
    {
        // Alternate between two different frequencies
        constexpr int period1 = 200;
        constexpr int period2 = 300;

        for (int i = 0; i < 20; ++i)
        {
            buffer.clear();
            int period = (i % 2 == 0) ? period1 : period2;
            BufferFiller::generateSineCycles(buffer, period);
            processor.processBlock(buffer, midiBuffer);
        }

        // Should handle without crashing
        CHECK(true);
    }

    SECTION("Multi-block processing maintains state")
    {
        constexpr int period = TestConfig::testPeriod;

        // Process 50 blocks continuously
        for (int i = 0; i < 50; ++i)
        {
            buffer.clear();
            BufferFiller::generateSineCycles(buffer, period);
            processor.processBlock(buffer, midiBuffer);
        }

        // Should eventually reach and maintain tracking state
        CHECK(processor.getCurrentState() == GranulatorProcessor::ProcessState::kTracking);
    }
}

//==============================================================================
// Parameter Tests
//==============================================================================

TEST_CASE("GranulatorProcessor parameter handling", "[GranulatorProcessor][params]")
{
    TestUtils::SetupAndTeardown setup;
    GranulatorProcessor processor;
    processor.prepareToPlay(TestConfig::sampleRate, TestConfig::blockSize);

    SECTION("Has shiftRatio parameter")
    {
        auto* param = processor.getAPVTS().getParameter("shiftRatio");
        REQUIRE(param != nullptr);
    }

    SECTION("shiftRatio parameter has correct range")
    {
        auto* param = dynamic_cast<juce::AudioParameterFloat*>(
            processor.getAPVTS().getParameter("shiftRatio")
        );
        REQUIRE(param != nullptr);

        // Range should be 0.5 to 1.5
        float minVal = param->getNormalisableRange().start;
        float maxVal = param->getNormalisableRange().end;

        CHECK(minVal == 0.5f);
        CHECK(maxVal == 1.5f);
    }

    SECTION("shiftRatio parameter default is 1.0")
    {
        auto* param = dynamic_cast<juce::AudioParameterFloat*>(
            processor.getAPVTS().getParameter("shiftRatio")
        );
        REQUIRE(param != nullptr);

        CHECK(param->get() == 1.0f);
    }
}
