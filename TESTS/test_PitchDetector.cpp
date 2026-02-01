/**
 * test_PitchDetector.cpp
 * Created by Ryan Devens
 *
 * Tests for PitchDetector class functionality
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "../SUBMODULES/RD/SOURCE/PITCH/PitchDetector.h"
#include "../SUBMODULES/RD/SOURCE/BufferFiller.h"

//==============================================================================
// Test Access Class - Provides access to private members for testing
//==============================================================================
class PitchDetectorTester
{
public:
	static double getSampleRate(const PitchDetector& pd) { return pd.mSampleRate; }
	static int getHalfBlock(const PitchDetector& pd) { return pd.mHalfBlock; }
	static int getDifferenceBufferNumSamples(const PitchDetector& pd) { return pd.differenceBuffer.getNumSamples(); }
	static int getDifferenceBufferNumChannels(const PitchDetector& pd) { return pd.differenceBuffer.getNumChannels(); }
	static int getCmndBufferNumSamples(const PitchDetector& pd) { return pd.cmndBuffer.getNumSamples(); }
	static int getCmndBufferNumChannels(const PitchDetector& pd) { return pd.cmndBuffer.getNumChannels(); }
};

//==============================================================================
// Test Constants
//==============================================================================
namespace TestConfig
{
	constexpr double sampleRate = 48000.0;
	constexpr int blockSize = 1024;
	constexpr int expectedHalfBlock = blockSize / 2; // 512
}

//==============================================================================
// prepareToPlay() Tests
//==============================================================================

TEST_CASE("PitchDetector prepareToPlay() initializes member objects correctly", "[PitchDetector][prepareToPlay]")
{
	PitchDetector detector;

	SECTION("Before prepareToPlay: default values from constructor")
	{
		// Default sample rate from #define DEFAULT_SAMPLE_RATE 48000
		CHECK(PitchDetectorTester::getSampleRate(detector) == 48000.0);

		// Default mHalfBlock is 0 (set in class definition)
		CHECK(PitchDetectorTester::getHalfBlock(detector) == 0);

		// Default buffer size from constructor is DEFAULT_BUFFER_SIZE (1024)
		CHECK(PitchDetectorTester::getDifferenceBufferNumSamples(detector) == 1024);
		CHECK(PitchDetectorTester::getDifferenceBufferNumChannels(detector) == 1);
		CHECK(PitchDetectorTester::getCmndBufferNumSamples(detector) == 1024);
		CHECK(PitchDetectorTester::getCmndBufferNumChannels(detector) == 1);
	}

	// Call prepareToPlay with test configuration
	detector.prepareToPlay(TestConfig::sampleRate, TestConfig::blockSize);

	SECTION("After prepareToPlay: mSampleRate is updated")
	{
		CHECK(PitchDetectorTester::getSampleRate(detector) == TestConfig::sampleRate);
	}

	SECTION("After prepareToPlay: mHalfBlock is blockSize / 2")
	{
		CHECK(PitchDetectorTester::getHalfBlock(detector) == TestConfig::expectedHalfBlock);
	}

	SECTION("After prepareToPlay: differenceBuffer is resized correctly")
	{
		CHECK(PitchDetectorTester::getDifferenceBufferNumChannels(detector) == 1);
		CHECK(PitchDetectorTester::getDifferenceBufferNumSamples(detector) == TestConfig::expectedHalfBlock);
	}

	SECTION("After prepareToPlay: cmndBuffer is resized correctly")
	{
		CHECK(PitchDetectorTester::getCmndBufferNumChannels(detector) == 1);
		CHECK(PitchDetectorTester::getCmndBufferNumSamples(detector) == TestConfig::expectedHalfBlock);
	}
}

TEST_CASE("PitchDetector prepareToPlay() with different sample rates", "[PitchDetector][prepareToPlay]")
{
	PitchDetector detector;

	SECTION("44100 Hz sample rate")
	{
		detector.prepareToPlay(44100.0, 512);
		CHECK(PitchDetectorTester::getSampleRate(detector) == 44100.0);
		CHECK(PitchDetectorTester::getHalfBlock(detector) == 256);
	}

	SECTION("96000 Hz sample rate")
	{
		detector.prepareToPlay(96000.0, 2048);
		CHECK(PitchDetectorTester::getSampleRate(detector) == 96000.0);
		CHECK(PitchDetectorTester::getHalfBlock(detector) == 1024);
	}
}

//==============================================================================
// process() Tests
//==============================================================================

TEST_CASE("PitchDetector process() detects sine wave period", "[PitchDetector][process]")
{
	constexpr int bufferSize = 2048;
	constexpr int sinePeriod = 256;
	constexpr int numChannels = 1;
	constexpr double sampleRate = 48000.0;

	// Create and prepare detector
	PitchDetector detector;
	detector.prepareToPlay(sampleRate, bufferSize);

	// Create buffer and fill with sine wave cycles (period = 256 samples)
	juce::AudioBuffer<float> sineBuffer(numChannels, bufferSize);
	sineBuffer.clear();
	BufferFiller::generateSineCycles(sineBuffer, sinePeriod);

	SECTION("Detects period of 256 samples from sine wave")
	{
		float detectedPeriod = detector.process(sineBuffer);

		// YIN algorithm should detect period close to 256 samples
		CHECK(detectedPeriod == Catch::Approx(static_cast<float>(sinePeriod)).margin(1.0f));
	}
}

TEST_CASE("PitchDetector process() with various frequencies", "[PitchDetector][process]")
{
	constexpr int bufferSize = 2048;
	constexpr int numChannels = 1;
	constexpr double sampleRate = 48000.0;

	PitchDetector detector;
	detector.prepareToPlay(sampleRate, bufferSize);

	SECTION("100 Hz sine wave (480 samples period at 48kHz)")
	{
		// 48000 / 100 = 480 samples per period
		constexpr int period = 480;
		juce::AudioBuffer<float> buffer(numChannels, bufferSize);
		buffer.clear();
		BufferFiller::generateSineCycles(buffer, period);

		float detectedPeriod = detector.process(buffer);
		CHECK(detectedPeriod == Catch::Approx(static_cast<float>(period)).margin(2.0f));
	}

	SECTION("200 Hz sine wave (240 samples period at 48kHz)")
	{
		// 48000 / 200 = 240 samples per period
		constexpr int period = 240;
		juce::AudioBuffer<float> buffer(numChannels, bufferSize);
		buffer.clear();
		BufferFiller::generateSineCycles(buffer, period);

		float detectedPeriod = detector.process(buffer);
		CHECK(detectedPeriod == Catch::Approx(static_cast<float>(period)).margin(2.0f));
	}

	SECTION("440 Hz sine wave (109 samples period at 48kHz)")
	{
		// 48000 / 440 ≈ 109 samples per period
		constexpr int period = 109;
		juce::AudioBuffer<float> buffer(numChannels, bufferSize);
		buffer.clear();
		BufferFiller::generateSineCycles(buffer, period);

		float detectedPeriod = detector.process(buffer);
		CHECK(detectedPeriod == Catch::Approx(static_cast<float>(period)).margin(2.0f));
	}
}

TEST_CASE("PitchDetector returns -1 for silent or noise input", "[PitchDetector][process]")
{
	constexpr int bufferSize = 2048;
	constexpr int numChannels = 1;
	constexpr double sampleRate = 48000.0;

	PitchDetector detector;
	detector.prepareToPlay(sampleRate, bufferSize);

	SECTION("Silent buffer returns -1")
	{
		juce::AudioBuffer<float> buffer(numChannels, bufferSize);
		buffer.clear();

		float detectedPeriod = detector.process(buffer);
		CHECK(detectedPeriod == -1.0f);
	}
}

TEST_CASE("PitchDetector getCurrentPeriod() thread-safe getter", "[PitchDetector][thread-safety]")
{
	constexpr int bufferSize = 2048;
	constexpr int sinePeriod = 256;
	constexpr int numChannels = 1;
	constexpr double sampleRate = 48000.0;

	PitchDetector detector;
	detector.prepareToPlay(sampleRate, bufferSize);

	juce::AudioBuffer<float> sineBuffer(numChannels, bufferSize);
	sineBuffer.clear();
	BufferFiller::generateSineCycles(sineBuffer, sinePeriod);

	// Process to detect period
	float detectedPeriod = detector.process(sineBuffer);

	// Verify atomic getter returns same value
	double storedPeriod = detector.getCurrentPeriod();
	CHECK(storedPeriod == Catch::Approx(static_cast<double>(detectedPeriod)).margin(0.1));
}
