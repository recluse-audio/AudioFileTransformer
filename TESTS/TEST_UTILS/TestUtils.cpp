#include "TestUtils.h"
#include <cmath>

namespace TestUtils {

juce::AudioBuffer<float> createSineBuffer(
    int numChannels,
    int numSamples,
    float frequency,
    double sampleRate)
{
    juce::AudioBuffer<float> buffer(numChannels, numSamples);

    const float angleDelta = frequency * 2.0f * juce::MathConstants<float>::pi / static_cast<float>(sampleRate);
    float angle = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float value = std::sin(angle);

        for (int channel = 0; channel < numChannels; ++channel)
        {
            buffer.setSample(channel, sample, value);
        }

        angle += angleDelta;
    }

    return buffer;
}

bool isSilent(const juce::AudioBuffer<float>& buffer, float threshold)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const float* data = buffer.getReadPointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            if (std::abs(data[sample]) > threshold)
                return false;
        }
    }

    return true;
}

float calculateRMS(const juce::AudioBuffer<float>& buffer, int channel)
{
    if (channel >= buffer.getNumChannels())
        return 0.0f;

    const float* data = buffer.getReadPointer(channel);
    const int numSamples = buffer.getNumSamples();

    float sum = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        sum += data[i] * data[i];
    }

    return std::sqrt(sum / static_cast<float>(numSamples));
}

} // namespace TestUtils
