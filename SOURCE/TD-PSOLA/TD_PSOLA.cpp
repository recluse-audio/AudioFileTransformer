/**
 * @file TD_PSOLA.cpp
 * @brief Implementation of TD-PSOLA pitch shifting algorithm
 */

#include "TD_PSOLA.h"
#include "BufferFiller.h"
#include <cmath>
#include <algorithm>

namespace TD_PSOLA
{

TDPSOLA::TDPSOLA()
{
    // Initialize with default FFT size
    mFFT = std::make_unique<juce::dsp::FFT>(12); // 2^12 = 4096
}

TDPSOLA::~TDPSOLA()
{
}

bool TDPSOLA::process(const juce::AudioBuffer<float>& inputBuffer,
                      juce::AudioBuffer<float>& outputBuffer,
                      float fRatio,
                      float sampleRate,
                      const Config& config)
{
    // Validate inputs
    if (fRatio <= 0.0f || sampleRate <= 0.0f)
        return false;

    int numChannels = inputBuffer.getNumChannels();
    int numSamples = inputBuffer.getNumSamples();

    if (numChannels == 0 || numSamples == 0)
        return false;

    // Resize output buffer
    outputBuffer.setSize(numChannels, numSamples, false, true, false);
    outputBuffer.clear();

    // Process each channel independently
    for (int ch = 0; ch < numChannels; ch++)
    {
        // Create single-channel buffer view
        juce::AudioBuffer<float> inputChannel(const_cast<float**>(inputBuffer.getArrayOfReadPointers()) + ch, 1, numSamples);
        juce::AudioBuffer<float> outputChannel(outputBuffer.getArrayOfWritePointers() + ch, 1, numSamples);

        // Step 1: Detect pitch periods
        std::vector<int> periods = detectPitchPeriods(inputChannel, sampleRate, config);

        if (periods.empty())
            return false;

        // Step 2: Place pitch marks
        int hopSize = static_cast<int>(config.analysisWindowMs / 1000.0f * sampleRate);
        std::vector<int> analysisPitchMarks = placePitchMarks(inputChannel, periods, hopSize);

        if (analysisPitchMarks.empty())
            return false;

        // Step 3: Interpolate pitch marks for synthesis
        std::vector<float> synthesisPitchMarks = interpolatePitchMarks(analysisPitchMarks, fRatio);

        // Step 4: Perform PSOLA overlap-add
        psolaOverlapAdd(inputChannel, analysisPitchMarks, synthesisPitchMarks, fRatio, outputChannel);
    }

    return true;
}

bool TDPSOLA::processWithGrainExport(const juce::AudioBuffer<float>& inputBuffer,
                                      juce::AudioBuffer<float>& outputBuffer,
                                      GrainData& grainData,
                                      float fRatio,
                                      float sampleRate,
                                      const Config& config)
{
    // Validate inputs
    if (fRatio <= 0.0f || sampleRate <= 0.0f)
        return false;

    int numChannels = inputBuffer.getNumChannels();
    int numSamples = inputBuffer.getNumSamples();

    if (numChannels == 0 || numSamples == 0)
        return false;

    // Only support mono for grain export
    if (numChannels != 1)
        return false;

    // Resize output buffer
    outputBuffer.setSize(1, numSamples, false, true, false);
    outputBuffer.clear();

    // Create single-channel buffer view
    juce::AudioBuffer<float> inputChannel(const_cast<float**>(inputBuffer.getArrayOfReadPointers()), 1, numSamples);
    juce::AudioBuffer<float> outputChannel(outputBuffer.getArrayOfWritePointers(), 1, numSamples);

    // Step 1: Detect pitch periods
    std::vector<int> periods = detectPitchPeriods(inputChannel, sampleRate, config);

    if (periods.empty())
        return false;

    // Step 2: Place pitch marks
    int hopSize = static_cast<int>(config.analysisWindowMs / 1000.0f * sampleRate);
    std::vector<int> analysisPitchMarks = placePitchMarks(inputChannel, periods, hopSize);

    if (analysisPitchMarks.empty())
        return false;

    // Step 3: Interpolate pitch marks for synthesis
    std::vector<float> synthesisPitchMarks = interpolatePitchMarks(analysisPitchMarks, fRatio);

    // Initialize grain data
    grainData.fRatio = fRatio;
    grainData.signalLength = numSamples;
    grainData.numAnalysisGrains = static_cast<int>(analysisPitchMarks.size());
    grainData.numSynthesisGrains = static_cast<int>(synthesisPitchMarks.size());
    grainData.synthesisGrains.clear();

    // Step 4: Perform PSOLA overlap-add with grain export
    psolaOverlapAddWithGrainExport(inputChannel, analysisPitchMarks, synthesisPitchMarks, fRatio, outputChannel, grainData);

    return true;
}

std::vector<int> TDPSOLA::detectPitchPeriods(const juce::AudioBuffer<float>& channelData,
                                              float sampleRate,
                                              const Config& config)
{
    int numSamples = channelData.getNumSamples();
    int minPeriod = static_cast<int>(sampleRate / config.maxHz);
    int maxPeriod = static_cast<int>(sampleRate / config.minHz);
    int sequenceLength = static_cast<int>(config.analysisWindowMs / 1000.0f * sampleRate);

    // First pass: compute periods
    std::vector<int> periods = computePeriodsPerSequence(channelData, sequenceLength, minPeriod, maxPeriod);

    if (periods.empty())
        return periods;

    // Calculate mean and std for period variation bounds
    float meanPeriod = 0.0f;
    for (int p : periods)
        meanPeriod += p;
    meanPeriod /= periods.size();

    float variance = 0.0f;
    for (int p : periods)
        variance += (p - meanPeriod) * (p - meanPeriod);
    float stdPeriod = std::sqrt(variance / periods.size());

    // Second pass with bounded periods
    int minVariedPeriod = static_cast<int>(meanPeriod - config.inTypeScalar * stdPeriod);
    int maxVariedPeriod = static_cast<int>(meanPeriod + config.inTypeScalar * stdPeriod);

    minPeriod = std::max(minPeriod, minVariedPeriod);
    maxPeriod = std::min(maxPeriod, maxVariedPeriod);

    return computePeriodsPerSequence(channelData, sequenceLength, minPeriod, maxPeriod);
}

std::vector<int> TDPSOLA::computePeriodsPerSequence(const juce::AudioBuffer<float>& signal,
                                                     int sequenceLength,
                                                     int minPeriod,
                                                     int maxPeriod)
{
    std::vector<int> periods;
    int numSamples = signal.getNumSamples();
    const float* signalData = signal.getReadPointer(0);

    // Ensure FFT size accommodates sequence length
    int fftOrder = static_cast<int>(std::ceil(std::log2(sequenceLength)));
    int fftSize = 1 << fftOrder;

    if (!mFFT || mFFT->getSize() != fftSize)
        mFFT = std::make_unique<juce::dsp::FFT>(fftOrder);

    mFFTBuffer.setSize(1, fftSize * 2, false, true, false); // Complex data needs 2x space

    int offset = 0;
    while (offset < numSamples)
    {
        int remainingSamples = numSamples - offset;
        int currentSequence = std::min(sequenceLength, remainingSamples);

        // Prepare FFT buffer
        mFFTBuffer.clear();
        float* fftData = mFFTBuffer.getWritePointer(0);

        // Copy signal data to FFT buffer (interleaved real/imag format)
        for (int i = 0; i < currentSequence; i++)
        {
            fftData[i * 2] = signalData[offset + i]; // Real part
            fftData[i * 2 + 1] = 0.0f;                // Imaginary part
        }

        // Forward FFT - use real-only for real input signal
        mFFT->performRealOnlyForwardTransform(fftData, false); // false = don't use complex format

        // Remove DC component
        fftData[0] = 0.0f;

        // Compute power spectrum: fourier * conj(fourier) = |fourier|^2
        // After real-only forward transform, data is in frequency domain
        // We compute magnitude squared for autocorrelation
        for (int i = 0; i < fftSize; i++)
        {
            float real = fftData[i * 2];
            float imag = fftData[i * 2 + 1];
            float powerSpec = real * real + imag * imag;
            fftData[i * 2] = powerSpec;
            fftData[i * 2 + 1] = 0.0f;
        }

        // Inverse FFT to get autocorrelation
        mFFT->performRealOnlyInverseTransform(fftData);

        // Find peak in autocorrelation (within period range)
        int peakIndex = minPeriod;
        float peakValue = fftData[minPeriod * 2];

        for (int i = minPeriod + 1; i < std::min(maxPeriod, fftSize / 2); i++)
        {
            float value = fftData[i * 2];
            if (value > peakValue)
            {
                peakValue = value;
                peakIndex = i;
            }
        }

        periods.push_back(peakIndex);
        offset += sequenceLength;
    }

    return periods;
}

std::vector<int> TDPSOLA::placePitchMarks(const juce::AudioBuffer<float>& channelData,
                                           const std::vector<int>& periods,
                                           int hopSize)
{
    std::vector<int> pitchMarks;
    int numSamples = channelData.getNumSamples();
    const float* signal = channelData.getReadPointer(0);

    if (periods.empty())
        return pitchMarks;

    // Find first peak in first period
    int firstPeriod = periods[0];
    int searchRange = static_cast<int>(firstPeriod * 1.1f);
    searchRange = std::min(searchRange, numSamples);

    int firstPeak = 0;
    float maxVal = signal[0];
    for (int i = 1; i < searchRange; i++)
    {
        if (std::abs(signal[i]) > std::abs(maxVal))
        {
            maxVal = signal[i];
            firstPeak = i;
        }
    }
    pitchMarks.push_back(firstPeak);

    // Find subsequent peaks
    const float maxChangeRatio = 1.02f;
    const float minChangeRatio = 0.98f;

    while (true)
    {
        int prevPeak = pitchMarks.back();
        int periodIdx = prevPeak / hopSize;

        if (periodIdx >= static_cast<int>(periods.size()))
            break;

        int period = periods[periodIdx];

        // Calculate expected range for next peak
        int minExpected = prevPeak + static_cast<int>(period * minChangeRatio);
        int maxExpected = prevPeak + static_cast<int>(period * maxChangeRatio);

        if (maxExpected >= numSamples)
            break;

        // Find peak in expected range
        int peakInRange = minExpected;
        float maxInRange = signal[minExpected];

        for (int i = minExpected + 1; i <= maxExpected; i++)
        {
            if (std::abs(signal[i]) > std::abs(maxInRange))
            {
                maxInRange = signal[i];
                peakInRange = i;
            }
        }

        pitchMarks.push_back(peakInRange);
    }

    return pitchMarks;
}

std::vector<float> TDPSOLA::interpolatePitchMarks(const std::vector<int>& pitchMarks,
                                                   float fRatio)
{
    std::vector<float> newPitchMarks;

    if (pitchMarks.empty())
        return newPitchMarks;

    // Generate linearly spaced reference indices
    int numNewMarks = static_cast<int>(pitchMarks.size() * fRatio);
    int numOriginalMarks = static_cast<int>(pitchMarks.size());

    for (int i = 0; i < numNewMarks; i++)
    {
        // Linear interpolation index
        float refIndex = (i * (numOriginalMarks - 1)) / static_cast<float>(numNewMarks - 1 > 0 ? numNewMarks - 1 : 1);

        int leftIdx = static_cast<int>(std::floor(refIndex));
        int rightIdx = static_cast<int>(std::ceil(refIndex));

        // Clamp indices
        leftIdx = std::clamp(leftIdx, 0, numOriginalMarks - 1);
        rightIdx = std::clamp(rightIdx, 0, numOriginalMarks - 1);

        // Linear interpolation
        float weight = refIndex - leftIdx;
        float interpolatedMark = pitchMarks[leftIdx] * (1.0f - weight) + pitchMarks[rightIdx] * weight;

        newPitchMarks.push_back(interpolatedMark);
    }

    return newPitchMarks;
}

void TDPSOLA::psolaOverlapAdd(const juce::AudioBuffer<float>& inputChannel,
                               const std::vector<int>& analysisPitchMarks,
                               const std::vector<float>& synthesisPitchMarks,
                               float fRatio,
                               juce::AudioBuffer<float>& outputChannel)
{
    int numSamples = inputChannel.getNumSamples();
    const float* inputData = inputChannel.getReadPointer(0);
    float* outputData = outputChannel.getWritePointer(0);

    // Clear output
    outputChannel.clear();

    // Window alpha parameter (from Python implementation)
    float alpha = (fRatio >= 1.0f) ? 0.8f : 0.6f;

    // Process each synthesis pitch mark
    for (size_t j = 0; j < synthesisPitchMarks.size(); j++)
    {
        // Find corresponding analysis pitch mark
        float synthMark = synthesisPitchMarks[j];
        size_t closestIdx = 0;
        float minDist = std::abs(analysisPitchMarks[0] - synthMark);

        for (size_t i = 1; i < analysisPitchMarks.size(); i++)
        {
            float dist = std::abs(analysisPitchMarks[i] - synthMark);
            if (dist < minDist)
            {
                minDist = dist;
                closestIdx = i;
            }
        }

        int analysisMark = analysisPitchMarks[closestIdx];

        // Calculate distances to previous and next marks
        int samplesToPrev = (closestIdx == 0) ? analysisMark : (analysisMark - analysisPitchMarks[closestIdx - 1]);
        int samplesToNext = (closestIdx == analysisPitchMarks.size() - 1) ?
                            (numSamples - 1 - analysisMark) :
                            (analysisPitchMarks[closestIdx + 1] - analysisMark);

        // Edge case truncation
        if (analysisMark - samplesToPrev < 0)
            samplesToPrev = analysisMark;
        if (analysisMark + samplesToNext > numSamples - 1)
            samplesToNext = numSamples - 1 - analysisMark;

        // Define synthesis window range
        int newWindowStart = std::max(0, static_cast<int>(synthMark) - samplesToPrev);
        int newWindowEnd = std::min(numSamples, static_cast<int>(synthMark) + samplesToNext);

        if (newWindowStart >= numSamples)
            break;

        int windowLength = newWindowEnd - newWindowStart;

        // Generate Tukey window
        mWindowBuffer.setSize(1, windowLength, false, true, false);
        BufferFiller::generateTukey(mWindowBuffer, alpha);

        // Extract and apply window to original signal segment
        int origWindowStart = std::max(0, analysisMark - samplesToPrev);
        int origWindowEnd = std::min(numSamples, analysisMark + samplesToNext);

        // Ensure we don't go out of bounds
        if (origWindowEnd - origWindowStart > windowLength)
            origWindowEnd = origWindowStart + windowLength;

        const float* windowData = mWindowBuffer.getReadPointer(0);

        // Copy and window the segment to output position
        for (int i = 0; i < windowLength && (origWindowStart + i) < origWindowEnd; i++)
        {
            int outputIdx = newWindowStart + i;
            int inputIdx = origWindowStart + i;

            if (outputIdx < numSamples && inputIdx < numSamples)
                outputData[outputIdx] += windowData[i] * inputData[inputIdx];
        }
    }
}

void TDPSOLA::psolaOverlapAddWithGrainExport(const juce::AudioBuffer<float>& inputChannel,
                                              const std::vector<int>& analysisPitchMarks,
                                              const std::vector<float>& synthesisPitchMarks,
                                              float fRatio,
                                              juce::AudioBuffer<float>& outputChannel,
                                              GrainData& grainData)
{
    int numSamples = inputChannel.getNumSamples();
    const float* inputData = inputChannel.getReadPointer(0);
    float* outputData = outputChannel.getWritePointer(0);

    // Clear output
    outputChannel.clear();

    // Window alpha parameter (from Python implementation)
    float alpha = (fRatio >= 1.0f) ? 0.8f : 0.6f;

    // Process each synthesis pitch mark
    for (size_t j = 0; j < synthesisPitchMarks.size(); j++)
    {
        // Find corresponding analysis pitch mark
        float synthMark = synthesisPitchMarks[j];
        size_t closestIdx = 0;
        float minDist = std::abs(analysisPitchMarks[0] - synthMark);

        for (size_t i = 1; i < analysisPitchMarks.size(); i++)
        {
            float dist = std::abs(analysisPitchMarks[i] - synthMark);
            if (dist < minDist)
            {
                minDist = dist;
                closestIdx = i;
            }
        }

        int analysisMark = analysisPitchMarks[closestIdx];

        // Calculate distances to previous and next marks
        int samplesToPrev = (closestIdx == 0) ? analysisMark : (analysisMark - analysisPitchMarks[closestIdx - 1]);
        int samplesToNext = (closestIdx == analysisPitchMarks.size() - 1) ?
                            (numSamples - 1 - analysisMark) :
                            (analysisPitchMarks[closestIdx + 1] - analysisMark);

        // Edge case truncation
        if (analysisMark - samplesToPrev < 0)
            samplesToPrev = analysisMark;
        if (analysisMark + samplesToNext > numSamples - 1)
            samplesToNext = numSamples - 1 - analysisMark;

        // Define synthesis window range
        int newWindowStart = std::max(0, static_cast<int>(synthMark) - samplesToPrev);
        int newWindowEnd = std::min(numSamples, static_cast<int>(synthMark) + samplesToNext);

        if (newWindowStart >= numSamples)
            break;

        int windowLength = newWindowEnd - newWindowStart;

        // Generate Tukey window
        mWindowBuffer.setSize(1, windowLength, false, true, false);
        BufferFiller::generateTukey(mWindowBuffer, alpha);

        // Extract and apply window to original signal segment
        int origWindowStart = std::max(0, analysisMark - samplesToPrev);
        int origWindowEnd = std::min(numSamples, analysisMark + samplesToNext);

        // Ensure we don't go out of bounds
        if (origWindowEnd - origWindowStart > windowLength)
            origWindowEnd = origWindowStart + windowLength;

        const float* windowData = mWindowBuffer.getReadPointer(0);

        // Copy and window the segment to output position
        for (int i = 0; i < windowLength && (origWindowStart + i) < origWindowEnd; i++)
        {
            int outputIdx = newWindowStart + i;
            int inputIdx = origWindowStart + i;

            if (outputIdx < numSamples && inputIdx < numSamples)
                outputData[outputIdx] += windowData[i] * inputData[inputIdx];
        }

        // Record grain data
        SynthesisGrain grain;
        grain.grainId = static_cast<int>(j);
        grain.centerSample = static_cast<int>(synthMark);
        grain.startSample = newWindowStart;
        grain.endSample = newWindowEnd;
        grain.sourceAnalysisId = static_cast<int>(closestIdx);
        grain.sourceCenter = analysisMark;
        grain.sourceStart = origWindowStart;
        grain.sourceEnd = origWindowEnd;

        // Calculate periods
        grain.sourcePeriod = (closestIdx < analysisPitchMarks.size() - 1) ?
                             (analysisPitchMarks[closestIdx + 1] - analysisPitchMarks[closestIdx]) :
                             samplesToNext;
        grain.synthesisPeriod = (j < synthesisPitchMarks.size() - 1) ?
                                 static_cast<int>(synthesisPitchMarks[j + 1] - synthesisPitchMarks[j]) :
                                 samplesToNext;

        grain.windowAlpha = alpha;
        grain.durationSamples = windowLength;

        grainData.synthesisGrains.push_back(grain);
    }
}

} // namespace TD_PSOLA
