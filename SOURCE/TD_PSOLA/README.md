# TD-PSOLA Implementation

## Overview

This is a C++ JUCE implementation of the **Time-Domain Pitch Synchronous Overlap-Add (TD-PSOLA)** algorithm for pitch shifting audio. It's based on the Python reference implementation from `c:\REPOS\PLUGIN_PROJECTS\RD_PSOLA\td-psola`.

## Key Features

- **Non-realtime processing**: Designed for offline buffer processing, no threading or circular buffering
- **Vanilla TD-PSOLA**: Implements the basic algorithm with autocorrelation pitch detection and peak-based pitch marking
- **Stereo support**: Processes each channel independently
- **Tukey windowing**: Uses tapered cosine windows for smooth overlap-add

## Architecture

### Core Algorithm Steps

1. **Pitch Detection** - Uses frequency-domain autocorrelation to detect pitch periods across the signal
2. **Pitch Marking** - Places pitch marks at signal peaks based on detected periods (max-based method)
3. **Mark Interpolation** - Generates new synthesis pitch marks based on the pitch shift ratio
4. **Overlap-Add** - Extracts windowed segments from the original signal and overlaps them at new positions

### Files

- **TD_PSOLA.h/cpp** - Main TD-PSOLA processor class
- **BufferFiller.h** (modified in RD submodule) - Added `generateTukey()` function for window generation

## Usage

```cpp
#include "TD-PSOLA/TD_PSOLA.h"

TD_PSOLA::TDPSOLA psola;

// Input audio buffer (mono or stereo)
juce::AudioBuffer<float> inputBuffer;
// ... load audio into inputBuffer ...

// Output buffer
juce::AudioBuffer<float> outputBuffer;

// Process with pitch shift
float fRatio = 1.5f;  // 1.5 = up a perfect fifth
float sampleRate = 44100.0f;

bool success = psola.process(inputBuffer, outputBuffer, fRatio, sampleRate);

if (success) {
    // outputBuffer now contains pitch-shifted audio
}
```

### Pitch Shift Ratios

- `2.0` = up one octave
- `1.5` = up a perfect fifth
- `1.0` = no change
- `0.89` = down ~200 cents
- `0.5` = down one octave

### Configuration

You can customize the pitch detection parameters:

```cpp
TD_PSOLA::TDPSOLA::Config config;
config.maxHz = 1700.0f;          // Maximum frequency (Hz)
config.minHz = 75.0f;            // Minimum frequency (Hz)
config.analysisWindowMs = 40.0f; // Analysis window size (ms)
config.inTypeScalar = 2.2f;      // Std dev scaling for period variation

psola.process(inputBuffer, outputBuffer, fRatio, sampleRate, config);
```

## Implementation Details

### Autocorrelation Pitch Detection

Uses FFT-based autocorrelation to detect pitch periods:
- Transforms signal to frequency domain
- Computes power spectrum
- Inverse FFT gives autocorrelation
- Finds peak within expected period range

### Pitch Marking

Max-based method:
- Finds first peak in the first period
- For each subsequent period, searches for max within expected range
- Allows small variation (±2%) from detected period

### Overlap-Add Windowing

- Uses Tukey windows (tapered cosine)
- Alpha parameter adjusted based on pitch direction:
  - `0.8` for upward pitch shifts
  - `0.6` for downward pitch shifts
- Window size determined by distance between pitch marks

## Tests

Tests are located in `TESTS/test_TD_PSOLA.cpp`:

1. **Basic Instantiation** - Verifies object creation
2. **Process Sine Wave** - Tests various pitch shift ratios on synthetic sine waves
3. **Stereo Processing** - Verifies multi-channel support
4. **Invalid Inputs** - Tests error handling
5. **Tukey Window Generation** - Validates window generation
6. **Pitch Detection on Sine Waves** - Verifies pitch detector accuracy

Run tests:
```bash
cd BUILD
ctest -R TD_PSOLA
```

## Limitations

- **Designed for voice/music**: Frequency range (75-1700 Hz) is optimized for vocal/musical content
- **Stable pitch assumption**: Works best on signals with relatively constant pitch
- **Voiced signals**: Expects periodic signals; may produce artifacts on unvoiced/noisy content
- **Shift quality**: Small shifts (up to ±700 cents) work best; larger shifts degrade quality
- **Processing strategy**: For best results, process in short segments (notes, words, phrases)

## Differences from Python Implementation

This implementation focuses on the **vanilla TD-PSOLA** algorithm:

- ✅ Autocorrelation pitch detection
- ✅ Max-based pitch marking
- ✅ Tukey window overlap-add
- ❌ Transient preservation (not implemented)
- ❌ pYIN pitch detection (uses autocorrelation instead)
- ❌ Energy-based pitch marking (uses max-based instead)

The transient-preserving variant from the Python code could be added in the future if needed.

## Integration with Existing Code

The TD-PSOLA implementation uses existing components from the RD submodule:

- **BufferFiller** - For window generation and test signal creation
- **JUCE DSP** - FFT for autocorrelation
- **AudioBuffer<float>** - Consistent with codebase buffer handling

It does **not** use:
- The existing `PitchDetector` class (YIN algorithm) - TD-PSOLA has its own autocorrelation-based detection
- `GranulatorProcessor` - Different approach than granular synthesis
- Real-time processing infrastructure - Designed for offline use

## Future Enhancements

Potential additions:
- Transient detection and preservation
- pYIN-based pitch detection for varying pitch
- Energy-based pitch marking for improved accuracy
- Time-stretching (modify playback speed without changing pitch)
- Real-time processing support with circular buffering

## References

1. F. Charpentier and M. Stella. "Diphone synthesis using an overlap-add technique for speech waveforms concatenation." In *Int. Conf. Acoustics, Speech, and Signal Processing (ICASSP)*. Vol. 11. IEEE, 1986.
2. Python implementation: `c:\REPOS\PLUGIN_PROJECTS\RD_PSOLA\td-psola`
