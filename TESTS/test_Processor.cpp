#include "TEST_UTILS/TestUtils.h"
#include "../../SOURCE/PluginProcessor.h"

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

    SECTION("Processor can store input and output file paths")
    {
        // Placeholder test for file I/O functionality
        // This will be implemented when file processing is added
        REQUIRE_NOTHROW(processor.getName());
    }
}
