#include <catch2/catch_test_macros.hpp>

#include "TEST_UTILS/TestUtils.h"
#include "BufferWriter.h"
#include "BufferFiller.h"

#include <atomic>

TEST_CASE("BufferWriter write overload writes subset of buffer with progress",
          "[BufferWriter][overload]")
{
    TestUtils::SetupAndTeardown setup;

    auto outDir = juce::File::getCurrentWorkingDirectory()
                    .getChildFile("TESTS/RD/OUTPUT");
    if (! outDir.exists())
        outDir.createDirectory();

    const double sampleRate = 44100.0;
    const int    bufferSamples = 8192;
    juce::AudioBuffer<float> buffer(2, bufferSamples);
    BufferFiller::fillWithAllOnes(buffer);

    SECTION("Writes exactly numSamplesToWrite, progress reaches 1.0")
    {
        const int numToWrite = 3000;
        auto outFile = outDir.getChildFile("subset_write.wav");

        std::atomic<float> last { 0.0f };
        std::atomic<int>   count { 0 };
        auto result = BufferWriter::writeToWav(buffer, outFile, sampleRate, numToWrite, 24,
                                               [&](float p) { last.store(p); count.fetch_add(1); });
        REQUIRE(result == BufferWriter::Result::kSuccess);
        REQUIRE(outFile.existsAsFile());
        REQUIRE(count.load() > 0);
        REQUIRE(last.load() == 1.0f);

        // Reload and check sample count
        juce::AudioBuffer<float> roundtrip(2, bufferSamples);
        roundtrip.clear();
        double srOut = 0.0;
        int    chsOut = 0;
        int    samplesOut = 0;
        REQUIRE(BufferFiller::loadFromWavFile(outFile, roundtrip, bufferSamples,
                                               srOut, chsOut, samplesOut));
        REQUIRE(samplesOut == numToWrite);
        REQUIRE(srOut == sampleRate);
    }

    SECTION("numSamplesToWrite > buffer length is clamped to buffer length")
    {
        auto outFile = outDir.getChildFile("clamped_write.wav");
        auto result = BufferWriter::writeToWav(buffer, outFile, sampleRate, bufferSamples * 100, 24, nullptr);
        REQUIRE(result == BufferWriter::Result::kSuccess);

        juce::AudioBuffer<float> roundtrip(2, bufferSamples * 2);
        roundtrip.clear();
        double srOut = 0.0;
        int    chsOut = 0;
        int    samplesOut = 0;
        REQUIRE(BufferFiller::loadFromWavFile(outFile, roundtrip, bufferSamples * 2,
                                               srOut, chsOut, samplesOut));
        REQUIRE(samplesOut == bufferSamples);
    }
}
