#include "TEST_UTILS/TestUtils.h"
#include "../../SOURCE/PluginProcessor.h"
#include "../../SUBMODULES/RD/SOURCE/BufferFiller.h"
#include "../../SUBMODULES/RD/SOURCE/PROCESSORS/GAIN/GainProcessor.h"

TEST_CASE("Diagnostic: Processor graph node accessibility", "[diagnostic]")
{
    const double sampleRate = 44100.0;

    SECTION("GainProcessor node is accessible")
    {
        AudioFileTransformerProcessor processor;
        processor.setActiveProcessor(ActiveProcessor::Gain);

        auto* gainNode = processor.getGainNode();
        REQUIRE(gainNode != nullptr);
        REQUIRE(gainNode->getName() == "Gain Processor");

        // Can set and read parameters
        gainNode->setGain(0.75f);
        // Note: GainProcessor doesn't have a getGain() method, but setGain() doesn't throw
        REQUIRE_NOTHROW(gainNode->setGain(0.5f));
    }

    SECTION("GranulatorProcessor node is accessible")
    {
        AudioFileTransformerProcessor processor;
        processor.setActiveProcessor(ActiveProcessor::Granulator);

        auto* granulatorNode = processor.getGranulatorNode();
        REQUIRE(granulatorNode != nullptr);
        REQUIRE(granulatorNode->getName() == "Granulator Processor");
    }
}
