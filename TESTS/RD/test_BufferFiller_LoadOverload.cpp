#include <catch2/catch_test_macros.hpp>

#include "TEST_UTILS/TestUtils.h"
#include "BufferFiller.h"
#include "BufferWriter.h"

#include <atomic>

namespace
{
    juce::File goldenWav()
    {
        std::vector<juce::File> candidates = {
            juce::File::getCurrentWorkingDirectory().getChildFile("SUBMODULES/RD/TESTS/GOLDEN/GOLDEN_Somewhere_Mono_441k.wav"),
            juce::File::getCurrentWorkingDirectory().getChildFile("TESTS/TEST_FILES/Somewhere_Mono.wav")
        };
        for (const auto& f : candidates)
            if (f.existsAsFile())
                return f;
        return {};
    }
}

TEST_CASE("BufferFiller load overload fills pre-sized buffer with progress",
          "[BufferFiller][overload]")
{
    TestUtils::SetupAndTeardown setup;

    auto wav = goldenWav();
    REQUIRE(wav.existsAsFile());

    SECTION("File shorter than maxSamples: samplesRead == file length, tail zeroed")
    {
        const int destSamples = 48000 * 90; // 90s at 48k -- larger than test file
        juce::AudioBuffer<float> dest(2, destSamples);
        dest.clear();

        double sampleRate     = 0.0;
        int    numChannelsRead = 0;
        int    samplesRead    = 0;
        std::atomic<int> callbacks { 0 };

        bool ok = BufferFiller::loadFromWavFile(wav, dest, destSamples,
                                                sampleRate, numChannelsRead, samplesRead,
                                                [&](float){ callbacks.fetch_add(1); });
        REQUIRE(ok);
        REQUIRE(samplesRead > 0);
        REQUIRE(samplesRead < destSamples);
        REQUIRE(sampleRate > 0.0);
        REQUIRE(numChannelsRead >= 1);
        REQUIRE(callbacks.load() > 0);

        // Tail past samplesRead must remain zero
        for (int ch = 0; ch < dest.getNumChannels(); ++ch)
            REQUIRE(dest.getSample(ch, destSamples - 1) == 0.0f);
    }

    SECTION("maxSamples cap truncates the read")
    {
        const int cap = 1024;
        juce::AudioBuffer<float> dest(2, cap * 2);
        dest.clear();

        double sampleRate     = 0.0;
        int    numChannelsRead = 0;
        int    samplesRead    = 0;
        bool ok = BufferFiller::loadFromWavFile(wav, dest, cap,
                                                sampleRate, numChannelsRead, samplesRead);
        REQUIRE(ok);
        REQUIRE(samplesRead == cap);

        // Beyond the cap must be silent
        REQUIRE(dest.getSample(0, cap + 10) == 0.0f);
    }

    SECTION("Mono source -> stereo dest duplicates ch0 into ch1")
    {
        juce::AudioBuffer<float> dest(2, 8192);
        dest.clear();

        double sampleRate     = 0.0;
        int    numChannelsRead = 0;
        int    samplesRead    = 0;
        bool ok = BufferFiller::loadFromWavFile(wav, dest, 8192,
                                                sampleRate, numChannelsRead, samplesRead);
        REQUIRE(ok);

        if (numChannelsRead == 1)
        {
            for (int i = 0; i < samplesRead; ++i)
                REQUIRE(dest.getSample(0, i) == dest.getSample(1, i));
        }
    }

    SECTION("Missing file returns false")
    {
        juce::AudioBuffer<float> dest(2, 1024);
        double sampleRate     = 0.0;
        int    numChannelsRead = 0;
        int    samplesRead    = 0;
        bool ok = BufferFiller::loadFromWavFile(juce::File("C:\\nope\\missing.wav"),
                                                dest, 1024,
                                                sampleRate, numChannelsRead, samplesRead);
        REQUIRE_FALSE(ok);
        REQUIRE(samplesRead == 0);
    }
}
