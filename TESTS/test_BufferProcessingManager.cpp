
#include <catch2/catch_test_macros.hpp>

#include "TEST_UTILS/TestUtils.h"
#include "../SUBMODULES/RD/TESTS/TEST_UTILS/TestUtils.h"
#include "Processor/BufferProcessingManager.h"

#include "../SUBMODULES/RD/SOURCE/BufferFiller.h"


TEST_CASE("BufferProcessingManager initialization", "[BufferProcessingManager][buffer]")
{
    //======================== BOILERPLATE =============
    TestUtils::SetupAndTeardown setup;

    //======================== BUFFER SETUP =============
    // Make sure the inputBuffer is all 1s and the outputBuffer all 0s
    //
    const int numChannels = 2;
    const int numSamples = 1024;
    juce::AudioBuffer<float> inputBuffer (numChannels, numSamples);
    juce::AudioBuffer<float> outputBuffer (numChannels, numSamples);
    inputBuffer.clear();
    outputBuffer.clear();

    BufferFiller::fillWithAllOnes(inputBuffer);

    for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float inputSample = inputBuffer.getSample(ch, sampleIndex);
            float outputSample = outputBuffer.getSample(ch, sampleIndex);

            CHECK(inputSample == 1.f);
            CHECK(outputSample == 0.f);
        }
    }
    //===================== END BUFFER SETUP =======================

    //============= SETUP BufferProcessingManager =================
    BufferProcessingManager bpManager;
    bpManager.setActiveProcessor(ActiveProcessor::kGain);
    CHECK(bpManager.getActiveProcessor() == ActiveProcessor::kGain);
    
    bpManager.setActiveProcessor(ActiveProcessor::kGrainShifter);
    CHECK(bpManager.getActiveProcessor() == ActiveProcessor::kGain);
}