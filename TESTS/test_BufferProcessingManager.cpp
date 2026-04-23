
#include <catch2/catch_test_macros.hpp>

#include "TEST_UTILS/TestUtils.h"
#include "../SUBMODULES/RD/TESTS/TEST_UTILS/TestUtils.h"
#include "Processor/BufferProcessingManager.h"

#include "../SUBMODULES/RD/SOURCE/BufferFiller.h"


TEST_CASE("BufferProcessingManager initialization", "[BufferProcessingManager][buffer]")
{
    TestUtils::SetupAndTeardown setup;
    juce::AudioBuffer<float> inputBuffer (2, 1024);
    juce::AudioBuffer<float> outputBuffer (2, 1024);
    inputBuffer.clear();
    outputBuffer.clear();

    BufferFiller::fillWithAllOnes(inputBuffer);
    int numSamples  = inputBuffer.getNumSamples();
    int numChannels = inputBuffer.getNumChannels();
    
    for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float sample = inputBuffer.getSample(ch, sampleIndex);
            CHECK(sample == 0.f);
        }
    }

}