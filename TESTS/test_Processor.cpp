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
        // Verify both gain and granulator nodes exist
        auto* gainNode = processor.getGainNode();
        REQUIRE(gainNode != nullptr);

        auto* granulatorNode = processor.getGranulatorNode();
        REQUIRE(granulatorNode != nullptr);

        // Verify gain node has correct properties
        REQUIRE(gainNode->getName() == "Gain Processor");
        REQUIRE(gainNode->getTailLengthSeconds() == 0.0);

        // Switch to gain processor for testing
        processor.setActiveProcessor(AudioFileTransformerProcessor::ActiveProcessor::Gain);

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
    TestUtils::SetupAndTeardown setup;
    AudioFileTransformerProcessor processor;
    const double sampleRate = 44100.0;
    const int samplesPerBlock = 512;
    const int numChannels = 2;

    // Switch to gain processor for gain testing
    processor.setActiveProcessor(AudioFileTransformerProcessor::ActiveProcessor::Gain);
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

TEST_CASE("AudioFileTransformerProcessor processor swapping", "[AudioFileTransformer][processor][swapping]")
{
    TestUtils::SetupAndTeardown setup;
    AudioFileTransformerProcessor processor;
    const double sampleRate = 44100.0;
    const int samplesPerBlock = 512;

    SECTION("Default processor is Granulator")
    {
        REQUIRE(processor.getActiveProcessor() == AudioFileTransformerProcessor::ActiveProcessor::Granulator);
    }

    SECTION("Can switch to Gain processor")
    {
        processor.setActiveProcessor(AudioFileTransformerProcessor::ActiveProcessor::Gain);
        REQUIRE(processor.getActiveProcessor() == AudioFileTransformerProcessor::ActiveProcessor::Gain);

        // Verify gain processing works after switch
        processor.prepareToPlay(sampleRate, samplesPerBlock);
        auto* gainNode = processor.getGainNode();
        REQUIRE(gainNode != nullptr);
        gainNode->setGain(0.5f);

        juce::AudioBuffer<float> buffer(2, samplesPerBlock);
        BufferFiller::fillWithAllOnes(buffer);
        juce::MidiBuffer midiBuffer;
        processor.processBlock(buffer, midiBuffer);

        // Verify gain was applied (1.0 * 0.5 = 0.5)
        float sample = buffer.getSample(0, 0);
        REQUIRE(std::abs(sample - 0.5f) < 1e-6f);

        processor.releaseResources();
    }

    SECTION("Can switch to Granulator processor")
    {
        processor.setActiveProcessor(AudioFileTransformerProcessor::ActiveProcessor::Granulator);
        REQUIRE(processor.getActiveProcessor() == AudioFileTransformerProcessor::ActiveProcessor::Granulator);

        // Verify granulator exists and is accessible
        processor.prepareToPlay(sampleRate, samplesPerBlock);
        auto* granulatorNode = processor.getGranulatorNode();
        REQUIRE(granulatorNode != nullptr);

        // Process a buffer (won't pitch shift without proper setup, but should not crash)
        juce::AudioBuffer<float> buffer(2, samplesPerBlock);
        BufferFiller::fillWithAllOnes(buffer);
        juce::MidiBuffer midiBuffer;
        REQUIRE_NOTHROW(processor.processBlock(buffer, midiBuffer));

        processor.releaseResources();
    }

    SECTION("Can switch between processors multiple times")
    {
        processor.setActiveProcessor(AudioFileTransformerProcessor::ActiveProcessor::Gain);
        REQUIRE(processor.getActiveProcessor() == AudioFileTransformerProcessor::ActiveProcessor::Gain);

        processor.setActiveProcessor(AudioFileTransformerProcessor::ActiveProcessor::Granulator);
        REQUIRE(processor.getActiveProcessor() == AudioFileTransformerProcessor::ActiveProcessor::Granulator);

        processor.setActiveProcessor(AudioFileTransformerProcessor::ActiveProcessor::Gain);
        REQUIRE(processor.getActiveProcessor() == AudioFileTransformerProcessor::ActiveProcessor::Gain);

        // Verify gain still works after multiple swaps
        processor.prepareToPlay(sampleRate, samplesPerBlock);
        auto* gainNode = processor.getGainNode();
        gainNode->setGain(0.25f);

        juce::AudioBuffer<float> buffer(2, samplesPerBlock);
        BufferFiller::fillWithAllOnes(buffer);
        juce::MidiBuffer midiBuffer;
        processor.processBlock(buffer, midiBuffer);

        float sample = buffer.getSample(0, 0);
        REQUIRE(std::abs(sample - 0.25f) < 1e-6f);

        processor.releaseResources();
    }
}

TEST_CASE("AudioFileTransformerProcessor processBlock() with all-ones passthrough", "[AudioFileTransformer][processor][passthrough]")
{
    TestUtils::SetupAndTeardown setup;
    /**
     * Test both processors with all 1's input.
     * Gain: Immediate passthrough with unity gain.
     * Granulator: Dry passthrough after warmup when pitch is not detected.
     */
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 128;
    constexpr int numChannels = 2;

    AudioFileTransformerProcessor processor;
    juce::MidiBuffer midiBuffer;

    SECTION("Gain processor with unity gain")
    {
        processor.setActiveProcessor(AudioFileTransformerProcessor::ActiveProcessor::Gain);
        processor.prepareToPlay(sampleRate, blockSize);

        auto* gainNode = processor.getGainNode();
        REQUIRE(gainNode != nullptr);
        gainNode->setGain(1.0f);  // Unity gain

        juce::AudioBuffer<float> processBuffer(numChannels, blockSize);

        // Process multiple blocks of all-ones
        // Gain processor has no latency, should pass through immediately
        for(int processBlockCall = 0; processBlockCall < 32; processBlockCall++)
        {
            BufferFiller::fillWithAllOnes(processBuffer);
            processor.processBlock(processBuffer, midiBuffer);

            // With unity gain, should always output 1.0 immediately
            float expectedValue = 1.0f;

            INFO("Gain - Process Block Call: " << processBlockCall);
            CHECK(processBuffer.getSample(0, 0) == expectedValue);
        }

        processor.releaseResources();
    }

    SECTION("Granulator processor with dry passthrough")
    {
        processor.setActiveProcessor(AudioFileTransformerProcessor::ActiveProcessor::Granulator);
        processor.prepareToPlay(sampleRate, blockSize);

        juce::AudioBuffer<float> processBuffer(numChannels, blockSize);

        // Process multiple blocks of all-ones
        // First few blocks will be 0 due to circular buffer warmup (lookahead delay)
        // After warmup, should pass through as 1's
        for(int processBlockCall = 0; processBlockCall < 32; processBlockCall++)
        {
            BufferFiller::fillWithAllOnes(processBuffer);
            processor.processBlock(processBuffer, midiBuffer);

            // We expect 1.0 to come out since that's what we put in
            // But the first 4 block calls will be 0.0 due to reading delayed audio data
            // from the circular buffer (lookahead delay = 512 samples, blockSize = 128, so 512/128 = 4 blocks)
            float expectedValue = 1.0f;
            if(processBlockCall <= 3)
                expectedValue = 0.0f;

            INFO("Granulator - Process Block Call: " << processBlockCall << ", Expected: " << expectedValue);
            CHECK(processBuffer.getSample(0, 0) == expectedValue);
        }

        processor.releaseResources();
    }
}

