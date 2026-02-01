#include "TEST_UTILS/TestUtils.h"
#include "../../SOURCE/PluginProcessor.h"
#include "../../SUBMODULES/RD/SOURCE/BufferFiller.h"
#include "../../SUBMODULES/RD/SOURCE/PROCESSORS/GAIN/GainProcessor.h"

TEST_CASE("Diagnostic: Compare direct GainProcessor vs through graph", "[diagnostic]")
{
    const int samplesPerBlock = 512;
    const int numChannels = 2;
    const double sampleRate = 44100.0;
    const float testGain = 0.5f;

    SECTION("Direct GainProcessor works")
    {
        GainProcessor gainProc;
        gainProc.prepareToPlay(sampleRate, samplesPerBlock);
        gainProc.setGain(testGain);

        juce::AudioBuffer<float> buffer(numChannels, samplesPerBlock);
        BufferFiller::fillWithAllOnes(buffer);

        juce::MidiBuffer midiBuffer;
        gainProc.processBlock(buffer, midiBuffer);

        // Check first sample
        float firstSample = buffer.getSample(0, 0);
        INFO("Direct GainProcessor - First sample: " << firstSample);
        REQUIRE(std::abs(firstSample - testGain) < 1e-6f);

        gainProc.releaseResources();
    }

    SECTION("GainProcessor through AudioFileTransformerProcessor graph")
    {
        AudioFileTransformerProcessor processor;

        // Switch to gain processor for this test
        processor.setActiveProcessor(AudioFileTransformerProcessor::ActiveProcessor::Gain);
        processor.prepareToPlay(sampleRate, samplesPerBlock);

        auto* gainNode = processor.getGainNode();
        REQUIRE(gainNode != nullptr);

        INFO("Setting gain to: " << testGain);
        gainNode->setGain(testGain);

        juce::AudioBuffer<float> buffer(numChannels, samplesPerBlock);
        BufferFiller::fillWithAllOnes(buffer);

        // Print buffer before processing
        float beforeSample = buffer.getSample(0, 0);
        INFO("Before processing - First sample: " << beforeSample);
        REQUIRE(beforeSample == 1.0f);  // Should be 1.0 from fillWithAllOnes

        juce::MidiBuffer midiBuffer;
        processor.processBlock(buffer, midiBuffer);

        // Print buffer after processing
        float afterSample = buffer.getSample(0, 0);
        INFO("After processing - First sample: " << afterSample);
        INFO("Expected: " << testGain);

        // Check if we got any output at all
        bool allZeros = true;
        for (int ch = 0; ch < numChannels && allZeros; ++ch)
        {
            for (int i = 0; i < samplesPerBlock && allZeros; ++i)
            {
                if (buffer.getSample(ch, i) != 0.0f)
                    allZeros = false;
            }
        }
        INFO("All samples are zero: " << (allZeros ? "YES" : "NO"));

        REQUIRE(std::abs(afterSample - testGain) < 1e-6f);

        processor.releaseResources();
    }
}

TEST_CASE("Diagnostic: Check graph preparation order", "[diagnostic]")
{
    const int samplesPerBlock = 512;
    const int numChannels = 2;
    const double sampleRate = 44100.0;
    const float testGain = 0.75f;

    SECTION("Set gain BEFORE prepareToPlay")
    {
        AudioFileTransformerProcessor processor;

        // Switch to gain processor for this test
        processor.setActiveProcessor(AudioFileTransformerProcessor::ActiveProcessor::Gain);

        auto* gainNode = processor.getGainNode();
        REQUIRE(gainNode != nullptr);
        gainNode->setGain(testGain);

        processor.prepareToPlay(sampleRate, samplesPerBlock);

        juce::AudioBuffer<float> buffer(numChannels, samplesPerBlock);
        BufferFiller::fillWithAllOnes(buffer);

        juce::MidiBuffer midiBuffer;
        processor.processBlock(buffer, midiBuffer);

        float afterSample = buffer.getSample(0, 0);
        INFO("Set gain BEFORE prepare - First sample: " << afterSample);
        REQUIRE(std::abs(afterSample - testGain) < 1e-6f);

        processor.releaseResources();
    }

    SECTION("Set gain AFTER prepareToPlay")
    {
        AudioFileTransformerProcessor processor;

        // Switch to gain processor for this test
        processor.setActiveProcessor(AudioFileTransformerProcessor::ActiveProcessor::Gain);
        processor.prepareToPlay(sampleRate, samplesPerBlock);

        auto* gainNode = processor.getGainNode();
        REQUIRE(gainNode != nullptr);
        gainNode->setGain(testGain);

        juce::AudioBuffer<float> buffer(numChannels, samplesPerBlock);
        BufferFiller::fillWithAllOnes(buffer);

        juce::MidiBuffer midiBuffer;
        processor.processBlock(buffer, midiBuffer);

        float afterSample = buffer.getSample(0, 0);
        INFO("Set gain AFTER prepare - First sample: " << afterSample);
        REQUIRE(std::abs(afterSample - testGain) < 1e-6f);

        processor.releaseResources();
    }
}
