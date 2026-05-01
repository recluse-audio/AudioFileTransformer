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

- `AudioFileTransformerProcessor` (SOURCE/Processor/PluginProcessor.h) inherits `RD_Processor` (RD submodule), not `juce::AudioProcessor` directly.
- Owns `BufferProcessingManager` which owns `RD_ProcessorSwapper`. Also owns `FileToBufferManager` and storage buffers (`mInputBuffer`, `mProcessedBuffer`, sized 2ch × 60s @ 192 kHz).
- `ActiveProcessor` = alias for `RD_ProcessorSwapper::ProcessorIndex`
- Current processors: `GainProcessor`, `GrainShifterProcessor` (RD submodule)
- Swap at runtime via `setActiveProcessor(ActiveProcessor)` — delegates to `BufferProcessingManager::setActiveProcessor`, which also re-points the DataLogger child registration (see below).
- Realtime path is silent: `doProcessBlock` clears the buffer (offline-only plugin).
- Offline path: `BufferProcessingManager::processBuffers()` chunks input into block-sized segments via `mSwapper.processBlock`.

**`RD_Processor` template-method contract**: `processBlock` and `prepareToPlay` are `final` on the base class. Subclasses (incl. `AudioFileTransformerProcessor`) must override `doProcessBlock` and `doPrepareToPlay` — never `processBlock`/`prepareToPlay`. The base wraps the child call with DataLogger lifecycle hooks.

**Adding new processor**: subclass `RD_Processor`, override `doProcessBlock`/`doPrepareToPlay`, register inside `RD_ProcessorSwapper`, extend `ProcessorIndex`, add accessor on `PluginProcessor` mirroring `getGainNode()` / `getGrainShifterNode()`.

### DataLogger Hierarchy

`RD_Processor` and `RD_ProcessorSwapper` both inherit `DataLogger`. The plugin builds a parent/child logger chain so all data logs nest under one root:

```
AudioFileTransformerProcessor (root)
  └─ RD_ProcessorSwapper          (added as child in PluginProcessor ctor)
       └─ active RD_Processor     (added/removed by BufferProcessingManager::_refreshActiveLoggerChild)
```

- `BufferProcessingManager::setActiveProcessor` calls `_refreshActiveLoggerChild`, which removes any prior active processor from the swapper's child registry and adds the new active one (with `setDataLogOutputName(processor.getName())`). Only the currently active processor is a logger child of the swapper at any time.
- `transformFile` calls `logData()` once before processing whenever `getIsLogging()` is true, to sync child parent dirs before per-block CSVs land. Calls `logData()` once again after writing.
- Lifecycle log layout under each processor's output dir: `prepare_to_play/{prepare_to_play.csv, processor_state.xml}` subdir on prepare; `input_samples.csv` and `output_samples.csv` at the root, **appended once per `processBlock`** (`2 + numChannels` rows per call, accumulating over the whole transform). `processBlock` does NOT write `processor_state.xml` — call `createProcessorDataLogFile()` externally for a one-off APVTS snapshot. Tests that expected per-block subdirs (`process_block_start_<idx>/`, `process_block_end_<idx>/`) or per-block XMLs are stale. Full detail in `SUBMODULES/RD/.claude/CLAUDE.md`.
- DataLogger root for the plugin defaults to `~/Documents/Recluse Audio/Data Logs/` (set inside DataLogger). Override per-test via `setDataLogRootDirectory`.
- **Cascading logging state.** `DataLogger::setIsLogging` recursively walks `mChildren`, so calling `setIsLogging` / `startLogging` / `stopLogging` on the top-level processor fans the flag out to the swapper and any active child currently in the registry. Per-block CSVs come from inside `RD_Processor::processBlock`, which checks `getIsLogging()` on the firing processor — and the offline path drives `swapper.processBlock` (not the top-level processor's), so the swapper + active child must end up with `isLogging=true`. The plugin ctor calls `setIsLogging(true)` once after wiring the swapper as a child; cascade does the rest. **Caveat: `addChild` does not inherit the parent's `isLogging`.** When `BufferProcessingManager::_refreshActiveLoggerChild` swaps in a new active child after logging has already been started, it explicitly mirrors `mSwapper.getIsLogging()` onto that new child so mid-run swaps keep producing per-block CSVs.

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
- Test naming convention: `test_<ComponentName>.cpp` for behavior, `test_<ComponentName>_DataLogger.cpp` for DataLogger output verification (separate file, tagged `[DataLogger]`)
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

1. Subclass `RD_Processor` (NOT `juce::AudioProcessor` directly). Override `doProcessBlock` / `doPrepareToPlay`. Ref: `GainProcessor`, `GrainShifterProcessor`.
2. Register in `RD_ProcessorSwapper` (RD submodule) — extend `ProcessorIndex`
3. Expose accessor on `AudioFileTransformerProcessor` (mirror `getGainNode()`)
4. Wire into `BufferProcessingManager` if special prepare/release needed
5. Tests: `TESTS/PLUGIN_PROCESSOR/test_Processor.cpp` + per-component DataLogger test (`test_<Component>_DataLogger.cpp`) following the protocol in `SUBMODULES/RD/.claude/CLAUDE.md`

**Switching Pattern**: `RD_ProcessorSwapper` owns all processors; only the active one's `processBlock` runs. No graph reconnection — swap is just an index change.

### Offline File Processing

Plugin is offline-only. `doProcessBlock` clears the realtime buffer; all real work goes through the file transform path.

Synchronous API on `AudioFileTransformerProcessor`:

1. `transformFile(inputFile, outputFile, progressCallback)` — validates paths, loads WAV into `mInputBuffer`, runs `BufferProcessingManager::processBuffers`, writes 24-bit WAV via `BufferWriter::writeToWav`. Returns `bool`. Progress callback gets 0.0–1.0 across load/process/write thirds.
2. `doFileTransform(progressCallback)` — composes `outputFile = getDataLogOutputDirectory() / "<timestamp>.wav"` from the processor's DataLogger output dir, pulls input from `mFileToBufferManager.getInputFile()`, then calls `transformFile`.
3. Errors: `getLastTransformError()`.

Storage buffers (`mInputBuffer`, `mProcessedBuffer`) are owned by the processor and reused — no separate processor instance for offline work. Caller must not invoke realtime `processBlock` against the same `BufferProcessingManager` while a transform is in flight (currently moot since realtime path is silent).

`FileToBufferManager` still exposes async `startProcessing` / `isProcessing` / `wasSuccessful` / `getError` for direct use, but the plugin's top-level transform API is sync via `transformFile` / `doFileTransform`.

### Processor Graph Testing Pattern

Tests should:
- Access processor nodes via public methods (e.g., `getGainNode()`)
- Create test signals using `TestUtils::createSineBuffer()`
- Process through `processor.processBlock()`
- Verify output using `TestUtils::calculateRMS()` or `TestUtils::isSilent()`

See `TESTS/PLUGIN_PROCESSOR/test_Processor.cpp` for the gain-verification pattern.

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
