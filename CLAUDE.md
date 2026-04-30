# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

AudioFileTransformer is a JUCE-based audio plugin project that processes audio files through a modular audio processor graph. The plugin is built using C++20 and CMake, with support for VST3 and Standalone formats.

## Build System

### Common Build Commands

All build scripts are located in `HELPER_SCRIPTS/` and automatically regenerate CMake file lists before building.

```bash
# Clean rebuild all targets (Debug by default)
python HELPER_SCRIPTS/rebuild_all.py

# Build tests and run them
python HELPER_SCRIPTS/build_tests.py
cd BUILD
ctest

# Build VST3 plugin
python HELPER_SCRIPTS/build_vst3.py

# Build AU plugin (macOS)
python HELPER_SCRIPTS/build_au.py

# Build standalone application
python HELPER_SCRIPTS/build_app.py        # or build_standalone.py

# Full build (all formats)
python HELPER_SCRIPTS/build_complete.py

# Installer + signing + release
python HELPER_SCRIPTS/build_installer.py
python HELPER_SCRIPTS/sign_builds.py
python HELPER_SCRIPTS/sign_installers.py
python HELPER_SCRIPTS/release_workflow.py
python HELPER_SCRIPTS/build_and_release_workflow.py

# Bump version
python HELPER_SCRIPTS/update_version.py VERSION.txt SOURCE/Util/Version.h
```

**Note**: On Windows, builds default to Debug configuration. For Release builds, pass `--config Release` to the build scripts.

### Test Execution

Tests use the Catch2 framework. After building tests:

```bash
cd BUILD
ctest                          # Run all tests
ctest -R ProcessorTest         # Run specific test
./Tests                        # Run tests directly (more verbose output)
```

### Adding New Files

When adding new source or test files:

1. Add files to appropriate directory (`SOURCE/` or `TESTS/`)
2. Run `python HELPER_SCRIPTS/regenSource.py` to update `CMAKE/SOURCES.cmake` and `CMAKE/TESTS.cmake`
3. Rebuild

**Important**: The build scripts automatically regenerate CMake file lists before building, so you typically don't need to run `regenSource.py` manually.

## Architecture

### Processor Swapping

Active processor managed by `RD_ProcessorSwapper` (RD submodule), wrapped by `BufferProcessingManager`:

- `AudioFileTransformerProcessor` (SOURCE/Processor/PluginProcessor.h) = main plugin class
- Owns `BufferProcessingManager` which owns `RD_ProcessorSwapper`
- `ActiveProcessor` = alias for `RD_ProcessorSwapper::ProcessorIndex`
- Current processors: `GainProcessor`, `GrainShifterProcessor` (RD submodule)
- Swap at runtime via `setActiveProcessor(ActiveProcessor)` — delegates to `BufferProcessingManager`
- Realtime path: `processBlock` → `mBufferProcessingManager.processSingleBlock()`
- Offline path: `BufferProcessingManager::processBuffers()` chunks input into block-sized segments

**Adding new processor**: register inside `RD_ProcessorSwapper`, extend `ProcessorIndex`, add accessor on `PluginProcessor` mirroring `getGainNode()` / `getGrainShifterNode()`.

### File Processing Architecture

Offline file processing via `FileToBufferManager` (SOURCE/Processor/FileToBufferManager.h/cpp):

- Owns input file path, output dir path, progress callback (`std::function<void(float)>`), worker thread
- Caller passes its `BufferProcessingManager` + storage buffers in — no separate processor instance
- Sync helpers: `loadInputToBuffer(destBuffer, maxSamples, sampleRateOut, samplesReadOut)`, `writeBufferToFile(srcBuffer, outputFile, sampleRate, numSamplesToWrite)`
- Async orchestration: `startProcessing(inputStorage, outputStorage, BufferProcessingManager&)` runs load → process → write on worker thread
- State query: `isProcessing()`, `wasSuccessful()`, `getError()`
- Path defaults: `getDefaultInputFile()`, `getDefaultOutputDirectory()`

**Thread safety**: Caller must not invoke realtime `processBlock` against the same `BufferProcessingManager` while `startProcessing` thread runs.

### RD Submodule Integration

The `SUBMODULES/RD` directory contains reusable audio processing utilities:

- **Processor Swap Infrastructure**: `RD_ProcessorSwapper` (PROCESSORS/) holds multiple processors, exposes one as active
- **Audio Processors**:
  - `GainProcessor` (PROCESSORS/GAIN/)
  - `GrainShifterProcessor` (PROCESSORS/GRAIN/) — granular pitch/time shifter
- **Buffer Utilities**: `BufferHelper`, `BufferMath`, `BufferRange`, `BufferWriter`, `BufferFiller` (Tukey window gen)
- **Audio I/O**: `AudioFileProcessor`, `AudioFileHelpers`
- **DSP**: `CircularBuffer`, `Interpolator`, `Window`, `PitchDetector` (YIN)

These RD components are included in the main plugin's source list and directly accessible.

### TD-PSOLA Module

`SOURCE/TD_PSOLA/TD_PSOLA.h` — offline pitch shifter. Autocorrelation pitch detect → max-based pitch marks → Tukey-windowed overlap-add. Stereo (per-channel). Not realtime, no circular buffer. Best on voiced/musical signals 75–1700 Hz, ±700 cents. Detail in `SOURCE/TD_PSOLA/README.md`.

### File Structure

```
SOURCE/
├── Processor/
│   ├── PluginProcessor.h/cpp        # Main plugin class
│   ├── BufferProcessingManager.h/cpp# Wraps RD_ProcessorSwapper, drives realtime + chunked offline
│   └── FileToBufferManager.h/cpp    # Threaded offline file I/O (load → process → write)
├── Components/
│   └── PluginEditor.h/cpp           # GUI editor
├── TD_PSOLA/
│   ├── TD_PSOLA.h/cpp               # Offline PSOLA pitch shifter (autocorr + Tukey OLA)
│   └── GrainExport.h
├── Util/
│   ├── Juce_Header.h                # Central JUCE includes
│   ├── Version.h                    # Auto-generated
│   └── FileUtils.*

TESTS/
├── PLUGIN_PROCESSOR/                # test_Processor.cpp
├── BUFFER_PROCESSING_MANAGER/       # test_BufferProcessingManager*.cpp
├── FILE_TO_BUFFER_MANAGER/          # test_FileToBufferManager.cpp
├── GOLDEN/                          # Golden reference assets (.wav/.csv/.txt) for regression
├── RD/                              # RD submodule tests
├── TD_PSOLA/                        # PSOLA tests
├── UTIL/                            # util tests
├── TEST_FILES/                      # input audio fixtures
└── TEST_UTILS/                      # TestUtils.h/cpp shared helpers

SUBMODULES/RD/SOURCE/                # Reusable audio utilities
├── PROCESSORS/
│   ├── RD_ProcessorSwapper.*        # Multi-processor host w/ active index
│   ├── GAIN/                        # GainProcessor
│   └── GRAIN/                       # GrainShifterProcessor + grain internals
├── Buffer*.h                        # Buffer utilities
└── AudioFile*.*                     # File I/O helpers
```

### Testing Architecture

Tests in `TESTS/` (Catch2), grouped per-component into subdirs (e.g. `TESTS/PLUGIN_PROCESSOR/`, `TESTS/BUFFER_PROCESSING_MANAGER/`, `TESTS/FILE_TO_BUFFER_MANAGER/`, `TESTS/RD/`, `TESTS/TD_PSOLA/`, `TESTS/UTIL/`). Per-test `OUTPUT/` dirs hold artifacts produced by runs.

- `TEST_UTILS/TestUtils.h/cpp` provides audio testing utilities (sine wave generation, RMS calculation, silence detection)
- Tests verify processor graph behavior by accessing nodes via `getGainNode()` and similar methods
- Test naming convention: `test_<ComponentName>.cpp`
- `TESTS/GOLDEN/` holds golden reference assets (`.wav` + `.csv` + `.txt`) for regression compare; tests load these and diff against fresh output
- `TESTS/TEST_FILES/` holds input audio fixtures

## Version Management

Version is stored in `VERSION.txt` (format: `major.minor.patch`).

The build system automatically generates `SOURCE/Util/Version.h` with these macros:
- `PROJECT_VERSION_MAJOR`
- `PROJECT_VERSION_MINOR`
- `PROJECT_VERSION_PATCH`
- `PROJECT_VERSION` (full string)

To update version: edit `VERSION.txt` and rebuild.

## Development Workflow

### Adding a New Audio Processor

1. Subclass `juce::AudioProcessor` (ref: `GainProcessor`, `GrainShifterProcessor`)
2. Register in `RD_ProcessorSwapper` (RD submodule) — extend `ProcessorIndex`
3. Expose accessor on `AudioFileTransformerProcessor` (mirror `getGainNode()`)
4. Wire into `BufferProcessingManager` if special prepare/release needed
5. Tests: `TESTS/test_Processor.cpp` and `TESTS/test_BufferProcessingManager.cpp` patterns

**Switching Pattern**: `RD_ProcessorSwapper` owns all processors; only the active one's `processBlock` runs. No graph reconnection — swap is just an index change.

### Offline File Processing

The plugin supports processing entire audio files offline:

1. Set paths: `setInputFile()`, `setOutputDirectory()`
2. `startFileProcessing(progressCallback)` — async, returns immediately
3. Progress callback receives float 0.0–1.0
4. Poll `isFileProcessing()` / `wasFileProcessingSuccessful()`
5. Errors: `getFileProcessingError()`

Or call `processFile(in, out, cb)` directly for synchronous one-shot.

**Important**: FileProcessingManager creates a separate processor instance for offline work, so file processing and real-time audio don't interfere with each other.

### Processor Graph Testing Pattern

Tests should:
- Access processor nodes via public methods (e.g., `getGainNode()`)
- Create test signals using `TestUtils::createSineBuffer()`
- Process through `processor.processBlock()`
- Verify output using `TestUtils::calculateRMS()` or `TestUtils::isSilent()`

Example from test_Processor.cpp:87-120 shows this pattern for gain verification.

## Key Technical Details

- **C++ Standard**: C++20 (enforced in CMakeLists.txt)
- **JUCE Modules**: Uses `juce_audio_utils`, `juce_audio_processors`, `juce_dsp`
- **Channel Layout**: Currently supports stereo only (see `isBusesLayoutSupported()`)
- **Thread Safety**: Processor graph handles thread safety for audio processing
- **Build Artifacts**: Generated in `BUILD/` directory (not tracked in git)

## Submodule Management

JUCE and RD are git submodules. When cloning:

```bash
git clone --recursive <repo-url>
# or if already cloned:
git submodule update --init --recursive
```
