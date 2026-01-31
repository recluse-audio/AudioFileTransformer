#pragma once

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "../../SOURCE/Util/Juce_Header.h"

/**
 * Test utilities for JUCE plugin testing.
 */

namespace TestUtils {

/**
 * Creates a simple test audio buffer with a sine wave.
 *
 * @param numChannels Number of audio channels
 * @param numSamples Number of samples
 * @param frequency Frequency of the sine wave in Hz
 * @param sampleRate Sample rate in Hz
 * @return Audio buffer containing the sine wave
 */
juce::AudioBuffer<float> createSineBuffer(
    int numChannels,
    int numSamples,
    float frequency,
    double sampleRate);

/**
 * Checks if an audio buffer is silent (all samples near zero).
 *
 * @param buffer Audio buffer to check
 * @param threshold Maximum absolute value to consider silent
 * @return true if buffer is silent, false otherwise
 */
bool isSilent(const juce::AudioBuffer<float>& buffer, float threshold = 0.0001f);

/**
 * Calculates RMS level of an audio buffer.
 *
 * @param buffer Audio buffer
 * @param channel Channel to analyze
 * @return RMS level
 */
float calculateRMS(const juce::AudioBuffer<float>& buffer, int channel = 0);

} // namespace TestUtils
