#include "TEST_UTILS/TestUtils.h"
#include "../../SUBMODULES/RD/TESTS/TEST_UTILS/TestUtils.h"
#include "Processor/PluginProcessor.h"
#include "BufferFiller.h"

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

        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = 0; i < samplesPerBlock; ++i)
            {
                REQUIRE(buffer.getSample(ch, i) == 0.0f);
            }
        }

        processor.releaseResources();
    }

    SECTION("Swapper processors are accessible")
    {
        auto* gainNode = processor.getGainNode();
        REQUIRE(gainNode != nullptr);

        auto* shifterNode = processor.getGrainShifterNode();
        REQUIRE(shifterNode != nullptr);

        REQUIRE(gainNode->getName() == "Gain Processor");
        REQUIRE(gainNode->getTailLengthSeconds() == 0.0);
    }
}

TEST_CASE("AudioFileTransformerProcessor processor swapping", "[AudioFileTransformer][processor]")
{
    TestUtils::SetupAndTeardown setup;
    AudioFileTransformerProcessor processor;

    SECTION("Default processor is Gain")
    {
        REQUIRE(processor.getActiveProcessor() == ActiveProcessor::kGain);
    }

    SECTION("Can switch to GrainShifter processor")
    {
        processor.setActiveProcessor(ActiveProcessor::kGrainShifter);
        REQUIRE(processor.getActiveProcessor() == ActiveProcessor::kGrainShifter);

        auto* shifterNode = processor.getGrainShifterNode();
        REQUIRE(shifterNode != nullptr);
    }

    SECTION("Can switch between processors multiple times")
    {
        REQUIRE(processor.getActiveProcessor() == ActiveProcessor::kGain);

        processor.setActiveProcessor(ActiveProcessor::kGrainShifter);
        REQUIRE(processor.getActiveProcessor() == ActiveProcessor::kGrainShifter);

        processor.setActiveProcessor(ActiveProcessor::kGain);
        REQUIRE(processor.getActiveProcessor() == ActiveProcessor::kGain);

        processor.setActiveProcessor(ActiveProcessor::kGrainShifter);
        REQUIRE(processor.getActiveProcessor() == ActiveProcessor::kGrainShifter);
    }
}
