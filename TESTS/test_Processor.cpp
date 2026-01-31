#include "TEST_UTILS/TestUtils.h"
#include "../../SOURCE/PluginProcessor.h"
#include "../../SUBMODULES/RD/SOURCE/BufferFiller.h"

TEST_CASE("AudioFileTransformerProcessor basic functionality", "[AudioFileTransformer][processor]")
{
    AudioFileTransformerProcessor processor;

    SECTION("Processor has correct properties")
    {
        REQUIRE(processor.getName().isNotEmpty());
        REQUIRE(processor.hasEditor() == true);
        REQUIRE(processor.getTailLengthSeconds() >= 0.0);
    }

    SECTION("Plugin can be prepared for playback")
    {
        const double sampleRate = 44100.0;
        const int samplesPerBlock = 512;

        REQUIRE_NOTHROW(processor.prepareToPlay(sampleRate, samplesPerBlock));
        REQUIRE_NOTHROW(processor.releaseResources());
    }

    SECTION("Plugin can process audio")
    {
        const double sampleRate = 44100.0;
        const int samplesPerBlock = 512;

        processor.prepareToPlay(sampleRate, samplesPerBlock);

        juce::AudioBuffer<float> buffer(2, samplesPerBlock);
        buffer.clear();

        juce::MidiBuffer midiBuffer;

        REQUIRE_NOTHROW(processor.processBlock(buffer, midiBuffer));

        processor.releaseResources();
    }

    SECTION("Processor graph is set up correctly")
    {
        // Verify gain node exists and is accessible
        auto* gainNode = processor.getGainNode();
        REQUIRE(gainNode != nullptr);

        // Verify gain node has correct properties
        REQUIRE(gainNode->getName() == "Gain Processor");
        REQUIRE(gainNode->getTailLengthSeconds() == 0.0);

        // Verify processor can access and modify gain
        gainNode->setGain(0.5f);
        // Process a buffer to ensure graph is properly connected
        processor.prepareToPlay(44100.0, 512);
        juce::AudioBuffer<float> buffer(2, 512);
        BufferFiller::fillWithAllOnes(buffer);
        juce::MidiBuffer midiBuffer;
        REQUIRE_NOTHROW(processor.processBlock(buffer, midiBuffer));
        processor.releaseResources();
    }
}

TEST_CASE("AudioFileTransformerProcessor gain processing", "[AudioFileTransformer][processor][gain]")
{
    AudioFileTransformerProcessor processor;
    const double sampleRate = 44100.0;
    const int samplesPerBlock = 512;
    const int numChannels = 2;

    processor.prepareToPlay(sampleRate, samplesPerBlock);

    SECTION("Gain of 0.0 produces exactly zero output")
    {
        auto* gainNode = processor.getGainNode();
        REQUIRE(gainNode != nullptr);
        gainNode->setGain(0.0f);

        juce::AudioBuffer<float> buffer(numChannels, samplesPerBlock);
        BufferFiller::fillWithAllOnes(buffer);

        juce::MidiBuffer midiBuffer;
        processor.processBlock(buffer, midiBuffer);

        // Every sample should be exactly 0.0 (1.0 * 0.0 = 0.0)
        for (int ch = 0; ch < numChannels; ++ch)
        {
            for (int i = 0; i < samplesPerBlock; ++i)
            {
                REQUIRE(buffer.getSample(ch, i) == 0.0f);
            }
        }
    }

    SECTION("Gain of 0.25 produces exactly 0.25 output")
    {
        auto* gainNode = processor.getGainNode();
        REQUIRE(gainNode != nullptr);
        gainNode->setGain(0.25f);

        juce::AudioBuffer<float> buffer(numChannels, samplesPerBlock);
        BufferFiller::fillWithAllOnes(buffer);

        juce::MidiBuffer midiBuffer;
        processor.processBlock(buffer, midiBuffer);

        // Every sample should be exactly 0.25 (1.0 * 0.25 = 0.25)
        for (int ch = 0; ch < numChannels; ++ch)
        {
            for (int i = 0; i < samplesPerBlock; ++i)
            {
                float sample = buffer.getSample(ch, i);
                REQUIRE(std::abs(sample - 0.25f) < 1e-6f);
            }
        }
    }

    SECTION("Gain of 0.5 produces exactly 0.5 output")
    {
        auto* gainNode = processor.getGainNode();
        REQUIRE(gainNode != nullptr);
        gainNode->setGain(0.5f);

        juce::AudioBuffer<float> buffer(numChannels, samplesPerBlock);
        BufferFiller::fillWithAllOnes(buffer);

        juce::MidiBuffer midiBuffer;
        processor.processBlock(buffer, midiBuffer);

        // Every sample should be exactly 0.5 (1.0 * 0.5 = 0.5)
        for (int ch = 0; ch < numChannels; ++ch)
        {
            for (int i = 0; i < samplesPerBlock; ++i)
            {
                float sample = buffer.getSample(ch, i);
                REQUIRE(std::abs(sample - 0.5f) < 1e-6f);
            }
        }
    }

    SECTION("Gain of 0.75 produces exactly 0.75 output")
    {
        auto* gainNode = processor.getGainNode();
        REQUIRE(gainNode != nullptr);
        gainNode->setGain(0.75f);

        juce::AudioBuffer<float> buffer(numChannels, samplesPerBlock);
        BufferFiller::fillWithAllOnes(buffer);

        juce::MidiBuffer midiBuffer;
        processor.processBlock(buffer, midiBuffer);

        // Every sample should be exactly 0.75 (1.0 * 0.75 = 0.75)
        for (int ch = 0; ch < numChannels; ++ch)
        {
            for (int i = 0; i < samplesPerBlock; ++i)
            {
                float sample = buffer.getSample(ch, i);
                REQUIRE(std::abs(sample - 0.75f) < 1e-6f);
            }
        }
    }

    SECTION("Gain of 1.0 produces exactly 1.0 output (unity gain)")
    {
        auto* gainNode = processor.getGainNode();
        REQUIRE(gainNode != nullptr);
        gainNode->setGain(1.0f);

        juce::AudioBuffer<float> buffer(numChannels, samplesPerBlock);
        BufferFiller::fillWithAllOnes(buffer);

        juce::MidiBuffer midiBuffer;
        processor.processBlock(buffer, midiBuffer);

        // Every sample should be exactly 1.0 (1.0 * 1.0 = 1.0)
        for (int ch = 0; ch < numChannels; ++ch)
        {
            for (int i = 0; i < samplesPerBlock; ++i)
            {
                float sample = buffer.getSample(ch, i);
                REQUIRE(std::abs(sample - 1.0f) < 1e-6f);
            }
        }
    }

    SECTION("Arbitrary gain value applies exactly")
    {
        const float testGain = 0.12345f;
        auto* gainNode = processor.getGainNode();
        REQUIRE(gainNode != nullptr);
        gainNode->setGain(testGain);

        juce::AudioBuffer<float> buffer(numChannels, samplesPerBlock);
        BufferFiller::fillWithAllOnes(buffer);

        juce::MidiBuffer midiBuffer;
        processor.processBlock(buffer, midiBuffer);

        // Every sample should be exactly testGain (1.0 * testGain = testGain)
        for (int ch = 0; ch < numChannels; ++ch)
        {
            for (int i = 0; i < samplesPerBlock; ++i)
            {
                float sample = buffer.getSample(ch, i);
                REQUIRE(std::abs(sample - testGain) < 1e-6f);
            }
        }
    }

    processor.releaseResources();
}
