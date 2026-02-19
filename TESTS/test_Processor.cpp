#include "TEST_UTILS/TestUtils.h"
#include "../../SUBMODULES/RD/TESTS/TEST_UTILS/TestUtils.h"
#include "../../SOURCE/PluginProcessor.h"
#include "../../SUBMODULES/RD/SOURCE/BufferFiller.h"

TEST_CASE("AudioFileTransformerProcessor basic functionality", "[AudioFileTransformer][processor]")
{
    TestUtils::SetupAndTeardown setup;
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

    SECTION("ProcessBlock outputs silence (offline-only plugin)")
    {
        const double sampleRate = 44100.0;
        const int samplesPerBlock = 512;

        processor.prepareToPlay(sampleRate, samplesPerBlock);

        juce::AudioBuffer<float> buffer(2, samplesPerBlock);
        BufferFiller::fillWithAllOnes(buffer);

        juce::MidiBuffer midiBuffer;
        processor.processBlock(buffer, midiBuffer);

        // Should output silence since this is an offline-only processor
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = 0; i < samplesPerBlock; ++i)
            {
                REQUIRE(buffer.getSample(ch, i) == 0.0f);
            }
        }

        processor.releaseResources();
    }

    SECTION("Processor graph nodes are accessible")
    {
        // Verify both gain and granulator nodes exist
        auto* gainNode = processor.getGainNode();
        REQUIRE(gainNode != nullptr);

        auto* granulatorNode = processor.getGranulatorNode();
        REQUIRE(granulatorNode != nullptr);

        // Verify gain node has correct properties
        REQUIRE(gainNode->getName() == "Gain Processor");
        REQUIRE(gainNode->getTailLengthSeconds() == 0.0);
    }
}

TEST_CASE("AudioFileTransformerProcessor processor swapping", "[AudioFileTransformer][processor]")
{
    TestUtils::SetupAndTeardown setup;
    AudioFileTransformerProcessor processor;

    SECTION("Default processor is TDPSOLA")
    {
        REQUIRE(processor.getActiveProcessor() == ActiveProcessor::TDPSOLA);
    }

    SECTION("Can switch to Gain processor")
    {
        processor.setActiveProcessor(ActiveProcessor::Gain);
        REQUIRE(processor.getActiveProcessor() == ActiveProcessor::Gain);

        // Verify gain node is still accessible after switching
        auto* gainNode = processor.getGainNode();
        REQUIRE(gainNode != nullptr);
        gainNode->setGain(0.5f);
    }

    SECTION("Can switch between processors multiple times")
    {
        // Start with default (TDPSOLA)
        REQUIRE(processor.getActiveProcessor() == ActiveProcessor::TDPSOLA);

        // Switch to Gain
        processor.setActiveProcessor(ActiveProcessor::Gain);
        REQUIRE(processor.getActiveProcessor() == ActiveProcessor::Gain);

        // Switch to Granulator
        processor.setActiveProcessor(ActiveProcessor::Granulator);
        REQUIRE(processor.getActiveProcessor() == ActiveProcessor::Granulator);

        // Switch back to Gain
        processor.setActiveProcessor(ActiveProcessor::Gain);
        REQUIRE(processor.getActiveProcessor() == ActiveProcessor::Gain);
    }
}
