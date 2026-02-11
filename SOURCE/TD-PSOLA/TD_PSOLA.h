/**
 * @file TD_PSOLA.h
 * @brief Time-Domain Pitch Synchronous Overlap-Add implementation
 *
 * Non-realtime pitch shifting using TD-PSOLA algorithm.
 * Based on the Python implementation from ant_td_psola.
 *
 * This implementation focuses on offline processing of audio buffers.
 * No threading or circular buffering - designed for simple buffer-in, buffer-out processing.
 */

#pragma once

#include "Util/Juce_Header.h"
#include "GrainExport.h"
#include <vector>

namespace TD_PSOLA
{

/**
 * @brief Main TD-PSOLA processor for pitch shifting
 *
 * Implements the vanilla TD-PSOLA algorithm:
 * 1. Detect pitch periods using autocorrelation
 * 2. Place pitch marks at signal peaks
 * 3. Apply overlap-add with shifted pitch marks
 */
class TDPSOLA
{
public:
    struct Config
    {
        float maxHz = 1700.0f;          // Maximum fundamental frequency (for voice)
        float minHz = 75.0f;            // Minimum fundamental frequency (for voice)
        float analysisWindowMs = 40.0f; // Analysis window size in ms
        float inTypeScalar = 2.2f;      // Standard deviation scaling for period variation
    };

    TDPSOLA();
    ~TDPSOLA();

    /**
     * @brief Process an entire audio buffer with pitch shifting
     *
     * @param inputBuffer Input audio (mono or stereo, will process each channel)
     * @param outputBuffer Output audio (will be resized to match input)
     * @param fRatio Pitch shift ratio (2.0 = up octave, 0.5 = down octave)
     * @param sampleRate Sample rate in Hz
     * @param config Processing configuration
     * @return true if successful, false if failed
     */
    bool process(const juce::AudioBuffer<float>& inputBuffer,
                 juce::AudioBuffer<float>& outputBuffer,
                 float fRatio,
                 float sampleRate,
                 const Config& config = Config());

    /**
     * @brief Process with grain data export
     *
     * @param inputBuffer Input audio (mono only for grain export)
     * @param outputBuffer Output audio
     * @param grainData Output grain analysis data
     * @param fRatio Pitch shift ratio
     * @param sampleRate Sample rate in Hz
     * @param config Processing configuration
     * @return true if successful, false if failed
     */
    bool processWithGrainExport(const juce::AudioBuffer<float>& inputBuffer,
                                 juce::AudioBuffer<float>& outputBuffer,
                                 GrainData& grainData,
                                 float fRatio,
                                 float sampleRate,
                                 const Config& config = Config());

private:
    /**
     * @brief Detect pitch periods using frequency-domain autocorrelation
     *
     * @param channelData Input signal (single channel)
     * @param sampleRate Sample rate in Hz
     * @param config Detection configuration
     * @return Vector of period lengths in samples, one per analysis window
     */
    std::vector<int> detectPitchPeriods(const juce::AudioBuffer<float>& channelData,
                                         float sampleRate,
                                         const Config& config);

    /**
     * @brief Place pitch marks at signal peaks based on detected periods
     *
     * Uses max-based method: finds the maximum value in each period.
     *
     * @param channelData Input signal
     * @param periods Detected pitch periods
     * @param hopSize Hop size used for period detection
     * @return Vector of pitch mark sample indices
     */
    std::vector<int> placePitchMarks(const juce::AudioBuffer<float>& channelData,
                                      const std::vector<int>& periods,
                                      int hopSize);

    /**
     * @brief Interpolate pitch marks to generate synthesis positions
     *
     * @param pitchMarks Original analysis pitch marks
     * @param fRatio Pitch shift ratio
     * @return Vector of synthesis pitch mark positions
     */
    std::vector<float> interpolatePitchMarks(const std::vector<int>& pitchMarks,
                                              float fRatio);

    /**
     * @brief Core PSOLA overlap-add algorithm
     *
     * @param inputChannel Input signal (single channel)
     * @param analysisPitchMarks Original pitch marks
     * @param synthesisPitchMarks New pitch mark positions
     * @param fRatio Pitch shift ratio
     * @param outputChannel Output buffer (must be allocated to same size as input)
     */
    void psolaOverlapAdd(const juce::AudioBuffer<float>& inputChannel,
                         const std::vector<int>& analysisPitchMarks,
                         const std::vector<float>& synthesisPitchMarks,
                         float fRatio,
                         juce::AudioBuffer<float>& outputChannel);

    /**
     * @brief PSOLA overlap-add with grain data export
     *
     * @param inputChannel Input signal (single channel)
     * @param analysisPitchMarks Original pitch marks
     * @param synthesisPitchMarks New pitch mark positions
     * @param fRatio Pitch shift ratio
     * @param outputChannel Output buffer
     * @param grainData Output grain data for export
     */
    void psolaOverlapAddWithGrainExport(const juce::AudioBuffer<float>& inputChannel,
                                         const std::vector<int>& analysisPitchMarks,
                                         const std::vector<float>& synthesisPitchMarks,
                                         float fRatio,
                                         juce::AudioBuffer<float>& outputChannel,
                                         GrainData& grainData);

    /**
     * @brief Compute periods using autocorrelation per analysis window
     */
    std::vector<int> computePeriodsPerSequence(const juce::AudioBuffer<float>& signal,
                                                int sequenceLength,
                                                int minPeriod,
                                                int maxPeriod);

    // FFT for autocorrelation
    std::unique_ptr<juce::dsp::FFT> mFFT;
    juce::AudioBuffer<float> mFFTBuffer;

    // Reusable window buffer
    juce::AudioBuffer<float> mWindowBuffer;
};

} // namespace TD_PSOLA
