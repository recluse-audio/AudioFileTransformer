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

# Build standalone application
python HELPER_SCRIPTS/build_app.py
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

### Audio Processing Graph

The plugin uses `juce::AudioProcessorGraph` to create a modular processing chain:

- `AudioFileTransformerProcessor` (SOURCE/PluginProcessor.h) is the main plugin class
- It contains an internal processor graph with nodes connected in series
- Current graph: Audio Input → GainProcessor → Audio Output
- The graph architecture allows easy addition of new processor nodes

**Key Design Pattern**: New audio processors should be added as nodes in the graph (see `_setupProcessorGraph()` in SOURCE/PluginProcessor.cpp:156).

### RD Submodule Integration

The `SUBMODULES/RD` directory contains reusable audio processing utilities:

- **Audio Processors**: `GainProcessor` and other effect processors (in `PROCESSORS/` subdirectories)
- **Buffer Utilities**: `BufferHelper`, `BufferMath`, `BufferRange`, `BufferWriter`, `BufferFiller`
- **Audio I/O**: `AudioFileProcessor`, `AudioFileHelpers`
- **DSP**: `CircularBuffer`, `Interpolator`, `Window`

These RD components are included in the main plugin's source list and directly accessible.

### File Structure

```
SOURCE/
├── PluginProcessor.h/cpp      # Main plugin class, manages processor graph
├── PluginEditor.h/cpp         # GUI editor
├── Audio/                     # Audio file processing
│   └── AudioFileProcessor.*   # Read/write audio files
├── Util/
│   ├── Juce_Header.h         # Central JUCE includes
│   ├── Version.h             # Auto-generated version macros
│   └── FileUtils.*           # File path utilities

SUBMODULES/RD/SOURCE/          # Reusable audio utilities
├── PROCESSORS/                # Audio effect processors
├── Buffer*.h                  # Buffer manipulation utilities
└── AudioFile*.*              # Audio file I/O helpers
```

### Testing Architecture

Tests are in `TESTS/` using Catch2:

- `TEST_UTILS/TestUtils.h/cpp` provides audio testing utilities (sine wave generation, RMS calculation, silence detection)
- Tests verify processor graph behavior by accessing nodes via `getGainNode()` and similar methods
- Test naming convention: `test_<ComponentName>.cpp`

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

1. Create processor class inheriting from `juce::AudioProcessor` (reference: `GainProcessor`)
2. Add processor to graph in `AudioFileTransformerProcessor::_setupProcessorGraph()`
3. Create connections: Input → YourProcessor → Next Node
4. Add test accessor method (like `getGainNode()`) if testing is needed
5. Write tests in `TESTS/test_Processor.cpp` to verify behavior

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
