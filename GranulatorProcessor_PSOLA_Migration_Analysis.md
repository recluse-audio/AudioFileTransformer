# GranulatorProcessor vs TD-PSOLA Guide: Comparison & Migration Strategy

## Executive Summary

Your `GranulatorProcessor` implements a working grain-based pitch shifting system that is **already realtime-safe**! The active code path has no allocations. The TD-PSOLA guide patterns would add useful pitch mark storage for debugging and future enhancements.

### [VERIFIED] Current Implementation is Realtime-Safe
**Verified**: All active code paths (processBlock → doDetection/doCorrection → chooseStablePitchMark → Granulator) have **zero allocations**.

### [NOTE] Dead Code Found (Not a Bug)
**Line 250 in `GranulatorProcessor.cpp`**: `refineMarkByCorrelation()` contains `std::vector<float> ref(P);` allocation, but **this function is never called**. Can be removed or fixed if you plan to use it later.

### [GOOD] Patterns Already Present
- Fixed-size grain array (`std::array<Grain, kNumGrains>`)
- Pre-allocated grain buffers in `prepare()`
- Pre-allocated `CircularBuffer` for audio history
- Pre-allocated detection buffer
- No allocations in active code path

---

## Verified Active Code Path (Realtime Safety Audit)

**Audio Thread Call Chain:**
```
processBlock()
  ├─> mCircularBuffer->pushBuffer(buffer)           [OK] No allocation
  ├─> doDetection(buffer)
  │     ├─> mCircularBuffer->readRange()            [OK] No allocation
  │     └─> mPitchDetector->process()               [OK] No allocation (verified in PitchDetector.cpp)
  ├─> doCorrection(buffer, detected_period)
  │     ├─> chooseStablePitchMark()                 [OK] No allocation (just peak finding)
  │     │     └─> mCircularBuffer->findPeakInRange() [OK] No allocation
  │     └─> mGranulator->processTracking()          [OK] No allocation
  │           ├─> makeGrain()                        [OK] Uses pre-allocated grain buffers
  │           └─> processActiveGrains()              [OK] No allocation
  └─> processDry(buffer)
        └─> mCircularBuffer->readRange()            [OK] No allocation
```

**Result:** ZERO ALLOCATIONS IN ACTIVE CODE PATH - Your implementation is already realtime-safe!

**Dead Code (Never Called):**
- `refineMarkByCorrelation()` - Contains allocation but never called

---

## Current Architecture Analysis

### Pitch Mark Detection & Tracking

**Current Approach:**
```cpp
// Single predicted mark (no history)
juce::int64 mPredictedNextAnalysisMark = -1;

// Find peaks on-demand
juce::int64 chooseStablePitchMark(endDetectionSample, detectedPeriod)
{
    if (mPredictedNextAnalysisMark is valid)
    {
        // Search ±25% radius around prediction
        return findPeakInRange(predicted ± radius);
    }
    else
    {
        // Wide search (full period) - jittery until tracking starts
        return findPeakInRange(last period);
    }
}
```

**Issues:**
- [ISSUE] No persistent mark storage - marks found once are lost
- [ISSUE] Repeated peak searches in same regions
- [ISSUE] Only one predicted mark (no history)
- [ISSUE] Marks aren't stored for debugging/analysis

### Grain Management

**Current Approach:**
```cpp
std::array<Grain, kNumGrains> mGrains;  // Fixed-size (good!)

// Each Grain has:
- juce::AudioBuffer<float> mBuffer;      // Pre-allocated (good!)
- juce::AudioBuffer<float> mWindowBuffer; // Pre-allocated (good!)
- std::tuple<int64, int64, int64> mAnalysisRange;
- std::tuple<int64, int64, int64> mSynthRange;
```

**Assessment:**
- [OK] Realtime-safe grain array
- [OK] Pre-allocated buffers
- [OK] Fixed capacity (no dynamic growth)

### Dead Code with Allocation (Not Currently Used)

**[NOTE] UNUSED FUNCTION (Line 250):**
```cpp
juce::int64 GranulatorProcessor::refineMarkByCorrelation(juce::int64 predictedMark, float detectedPeriod)
{
    const int P = (int)std::llround(detectedPeriod);
    const int radius = std::max(1, P / 4);

    // [WARNING] ALLOCATES, but this function is NEVER CALLED (dead code)
    std::vector<float> ref(P);

    // ... correlation logic ...
}
```

**Status:**
- [OK] Not a bug - function is never called in active code path
- [NOTE] If you plan to use this function later, pre-allocate the buffer

**Fix (if you plan to use it later):**
Pre-allocate correlation buffer in `prepareToPlay()`:
```cpp
class GranulatorProcessor
{
private:
    juce::AudioBuffer<float> mCorrelationBuffer;  // Pre-allocated
};

void prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Worst case: 2 periods at 50Hz = ~1764 samples at 44.1kHz
    const int maxPeriod = static_cast<int>(sampleRate / 50.0) * 2;
    mCorrelationBuffer.setSize(1, maxPeriod);
}

juce::int64 refineMarkByCorrelation(juce::int64 predictedMark, float detectedPeriod)
{
    const int P = (int)std::llround(detectedPeriod);
    jassert(P <= mCorrelationBuffer.getNumSamples()); // Safety check

    // [OK] Use pre-allocated buffer (just write to it, no allocation)
    for (int i = 0; i < P; ++i)
        mCorrelationBuffer.setSample(0, i, readMonoSample(predictedMark - P + i));

    // ... rest of correlation logic ...
}
```

---

## TD-PSOLA Guide Patterns

### Pattern 1: PitchMarkBuffer (Current Block Marks)

**Purpose:** Store pitch marks found within the current process block (realtime-safe).

**Implementation:**
```cpp
class PitchMarkBuffer
{
public:
    static constexpr int MAX_MARKS_PER_BUFFER = 16;

    void clear() { mCount = 0; }
    void addMark(int position)
    {
        if (mCount < MAX_MARKS_PER_BUFFER)
            mPositions[mCount++] = position;
    }

    int getCount() const { return mCount; }
    int operator[](int idx) const { return mPositions[idx]; }

private:
    std::array<int, MAX_MARKS_PER_BUFFER> mPositions;
    int mCount = 0;
};
```

**Benefits:**
- Zero allocations (just resets counter)
- Fixed capacity known at compile time
- Bounds-checked with assertions

### Pattern 2: PitchMarkHistory (Historical Marks)

**Purpose:** Store pitch mark history for grain extraction and debugging.

**Synchronization Note:**
Marks are stored as absolute sample positions (e.g., sample 10000, 10100, 10200...). When reading audio at these positions, `CircularBuffer::getWrappedIndex()` converts them to buffer indices via modulo operation. This works correctly because:
- CircularBuffer is sized large enough for the marks you'll actually use
- Current implementation: `circularBufferSize = pitchDetectBufferNumSamples * 2` (~4096 samples at 44.1kHz)
- Marks older than circularBufferSize will wrap to newer audio positions (by design)

**Implementation:**
```cpp
class PitchMarkHistory
{
public:
    static constexpr int HISTORY_CAPACITY = 512;

    void prepareToPlay(double sampleRate, int maxSamplesPerBlock)
    {
        mMarkRingBuffer.resize(HISTORY_CAPACITY);  // ONE-TIME allocation
        clear();
    }

    void addMark(juce::int64 absoluteSamplePosition)
    {
        mMarkRingBuffer[mWritePos] = absoluteSamplePosition;
        mWritePos = (mWritePos + 1) % HISTORY_CAPACITY;
        if (mCount < HISTORY_CAPACITY)
            mCount++;
    }

    juce::int64 getMark(int indexFromNewest) const
    {
        jassert(indexFromNewest < mCount);
        int pos = (mWritePos - 1 - indexFromNewest + HISTORY_CAPACITY) % HISTORY_CAPACITY;
        return mMarkRingBuffer[pos];
    }

    int getCount() const { return mCount; }
    void clear() { mWritePos = 0; mCount = 0; }

private:
    std::vector<juce::int64> mMarkRingBuffer;  // Allocated ONCE in prepareToPlay
    int mWritePos = 0;
    int mCount = 0;
};
```

**Benefits:**
- Fixed capacity with ring buffer (no growth)
- Stores mark history for analysis
- Access recent marks without searching
- Works with existing CircularBuffer wrapping via `getWrappedIndex()`

---

## Migration Strategy

### Step 1: Fix Critical Allocation Bug

**File:** `GranulatorProcessor.h`
```cpp
class GranulatorProcessor
{
private:
    // Add pre-allocated correlation buffer
    juce::AudioBuffer<float> mCorrelationBuffer;
};
```

**File:** `GranulatorProcessor.cpp`
```cpp
void GranulatorProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // ... existing code ...

    // Pre-allocate correlation buffer (worst case: 2 periods at 50Hz)
    const int maxPeriod = static_cast<int>(sampleRate / 50.0) * 2;
    mCorrelationBuffer.setSize(1, maxPeriod);
}

juce::int64 GranulatorProcessor::refineMarkByCorrelation(juce::int64 predictedMark, float detectedPeriod)
{
    const int P = (int)std::llround(detectedPeriod);
    const int radius = std::max(1, P / 4);

    jassert(P <= mCorrelationBuffer.getNumSamples()); // Safety check

    // [OK] Use pre-allocated buffer instead of std::vector
    for (int i = 0; i < P; ++i)
        mCorrelationBuffer.setSample(0, i, readMonoSample(predictedMark - P + i));

    double bestScore = -1.0;
    juce::int64 bestMark = predictedMark;

    for (int off = -radius; off <= radius; ++off)
    {
        const juce::int64 cand = predictedMark + off;

        double num = 0.0, denA = 0.0, denB = 0.0;
        for (int i = 0; i < P; ++i)
        {
            const float a = mCorrelationBuffer.getSample(0, i);  // Read from pre-allocated buffer
            const float b = readMonoSample(cand - P + i);
            num  += (double)a * (double)b;
            denA += (double)a * (double)a;
            denB += (double)b * (double)b;
        }

        const double score = num / (std::sqrt(denA * denB) + 1e-12);
        if (score > bestScore)
        {
            bestScore = score;
            bestMark = cand;
        }
    }

    return bestMark;
}
```

### Step 2: Add Pitch Mark Storage Classes

**New File:** `SUBMODULES/RD/SOURCE/PitchMarkBuffer.h`
```cpp
#pragma once
#include "Util/Juce_Header.h"
#include <array>

class PitchMarkBuffer
{
public:
    static constexpr int MAX_MARKS_PER_BUFFER = 16;

    PitchMarkBuffer()
    {
        clear();
    }

    void clear()
    {
        mCount = 0;
    }

    void addMark(juce::int64 absolutePosition)
    {
        jassert(mCount < MAX_MARKS_PER_BUFFER);
        if (mCount < MAX_MARKS_PER_BUFFER)
        {
            mPositions[mCount++] = absolutePosition;
        }
    }

    int getCount() const
    {
        return mCount;
    }

    juce::int64 operator[](int idx) const
    {
        jassert(idx < mCount);
        return mPositions[idx];
    }

private:
    std::array<juce::int64, MAX_MARKS_PER_BUFFER> mPositions;
    int mCount = 0;
};
```

**New File:** `SUBMODULES/RD/SOURCE/PitchMarkHistory.h`
```cpp
#pragma once
#include "Util/Juce_Header.h"
#include <vector>

class PitchMarkHistory
{
public:
    static constexpr int HISTORY_CAPACITY = 512;

    void prepareToPlay(double sampleRate, int maxSamplesPerBlock)
    {
        mMarkRingBuffer.resize(HISTORY_CAPACITY);  // ONE-TIME allocation
        clear();
    }

    void addMark(juce::int64 absoluteSamplePosition)
    {
        mMarkRingBuffer[mWritePos] = absoluteSamplePosition;
        mWritePos = (mWritePos + 1) % HISTORY_CAPACITY;

        if (mCount < HISTORY_CAPACITY)
        {
            mCount++;
        }
    }

    juce::int64 getMark(int indexFromNewest) const
    {
        jassert(indexFromNewest < mCount);
        int pos = (mWritePos - 1 - indexFromNewest + HISTORY_CAPACITY) % HISTORY_CAPACITY;
        return mMarkRingBuffer[pos];
    }

    int getCount() const
    {
        return mCount;
    }

    void clear()
    {
        mWritePos = 0;
        mCount = 0;
    }

private:
    std::vector<juce::int64> mMarkRingBuffer;  // Allocated ONCE in prepareToPlay
    int mWritePos = 0;
    int mCount = 0;
};
```

### Step 3: Integrate Pitch Mark Storage

**File:** `GranulatorProcessor.h`
```cpp
#include "../../PitchMarkBuffer.h"
#include "../../PitchMarkHistory.h"

class GranulatorProcessor : public juce::AudioProcessor
{
private:
    // Add mark storage
    PitchMarkBuffer mCurrentBlockMarks;
    PitchMarkHistory mAnalysisMarkHistory;

    // Keep existing predicted mark for comparison/migration
    juce::int64 mPredictedNextAnalysisMark = -1;
};
```

**File:** `GranulatorProcessor.cpp`
```cpp
void GranulatorProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // ... existing code ...

    // Initialize mark storage
    mCurrentBlockMarks.clear();
    mAnalysisMarkHistory.prepareToPlay(sampleRate, samplesPerBlock);
}

void GranulatorProcessor::doCorrection(juce::AudioBuffer<float>& processBuffer, float detectedPeriod)
{
    const float shiftedPeriod = detectedPeriod / mShiftRatio;
    const juce::int64 endProcessSample   = mSamplesProcessed + mBlockSize - 1;
    const juce::int64 endDetectionSample = endProcessSample - MagicNumbers::minLookaheadSize;

    // Find pitch mark using existing logic
    const juce::int64 markedIndex = chooseStablePitchMark(endDetectionSample, detectedPeriod);

    // [NEW] Store the found mark in current block buffer
    mCurrentBlockMarks.addMark(markedIndex);

    // [NEW] Add to long-term history (absolute position, wraps via getWrappedIndex when used)
    mAnalysisMarkHistory.addMark(markedIndex);

    // Update prediction for next time
    mPredictedNextAnalysisMark = markedIndex + (juce::int64)std::llround(detectedPeriod);

    // ... rest of existing code ...
}

// Example: Reading from history later
void debugAnalysisFunction()
{
    // Marks are stored as absolute positions
    // CircularBuffer handles wrapping when reading
    for (int i = 0; i < mAnalysisMarkHistory.getCount(); ++i)
    {
        juce::int64 absoluteMark = mAnalysisMarkHistory.getMark(i);
        // Read audio at this mark (wrapping handled internally)
        // float sample = readMonoSample(absoluteMark);
    }
}

void GranulatorProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // ... existing code ...

    // [NEW] Clear per-block marks at start of each block
    mCurrentBlockMarks.clear();

    // ... rest of existing code ...
}
```

### Step 4: Add Debugging Accessors (Optional)

```cpp
// In GranulatorProcessor.h
public:
    // Realtime-safe debug accessors - just return const references
    const PitchMarkBuffer& getCurrentBlockMarks() const { return mCurrentBlockMarks; }
    const PitchMarkHistory& getMarkHistory() const { return mAnalysisMarkHistory; }

    // Access marks directly without copying:
    // for (int i = 0; i < processor.getMarkHistory().getCount(); ++i)
    //     int64 mark = processor.getMarkHistory().getMark(i);
```

---

## Benefits of Adding Mark Storage (Optional Enhancement)

### Current State (Already Good!)
- **Zero allocations on audio thread** - already production-ready
- **Deterministic performance** - consistent processing time
- **Realtime-safe** - meets all JUCE best practices

### Additional Benefits from Mark Storage (Steps 2-3)
- **Mark history for debugging** - can analyze pitch tracking over time
- **Better testing** - can verify mark placement in unit tests
- **Future algorithm improvements** - foundation for more sophisticated mark selection
- **Potential accuracy improvements** - access to multiple recent marks for better decisions

### Development Benefits
- **Clear separation of concerns** - mark detection vs. mark storage
- **Easier to debug** - can inspect mark history
- **Better visibility** - understand what the pitch tracker is doing

---

## Testing Strategy

### 1. Verify No Allocations
```cpp
#if JUCE_DEBUG
void GranulatorProcessor::processBlock(...)
{
    // Verify no dynamic growth
    jassert(mCurrentBlockMarks.getCount() <= PitchMarkBuffer::MAX_MARKS_PER_BUFFER);
    jassert(mAnalysisMarkHistory.getCount() <= PitchMarkHistory::HISTORY_CAPACITY);

    // ... existing code ...
}
#endif
```

### 2. Stress Test with Small Buffers
Set buffer size to 64 samples to:
- Expose allocation issues faster
- Test boundary conditions
- Verify mark tracking across many blocks

### 3. Verify Mark Consistency
```cpp
// In test file
TEST_CASE("Pitch marks are stored correctly")
{
    GranulatorProcessor processor;
    processor.prepareToPlay(44100, 512);

    // Process test signal
    auto buffer = TestUtils::createSineBuffer(512, 2, 44100, 440.0f);
    processor.processBlock(buffer, MidiBuffer());

    // Verify marks were stored
    const auto& marks = processor.getCurrentBlockMarks();
    REQUIRE(marks.getCount() > 0);

    // Verify marks are in expected range
    for (int i = 0; i < marks.getCount(); ++i)
    {
        REQUIRE(marks[i] >= 0);
        REQUIRE(marks[i] < processor.getSampleRate());
    }
}
```

---

## Migration Checklist (All Optional Enhancements)

### Optional: Add Pitch Mark Storage (Recommended for Debugging)
- [ ] Create `PitchMarkBuffer.h` and `PitchMarkHistory.h` (Step 2)
- [ ] Add mark storage members to `GranulatorProcessor` (Step 3)
- [ ] Initialize mark storage in `prepareToPlay()` (Step 3)
- [ ] Store marks in `doCorrection()` (Step 3)
- [ ] Clear marks at start of `processBlock()` (Step 3)
- [ ] Add `#if JUCE_DEBUG` assertions for mark bounds
- [ ] Update `CMAKE/SOURCES.cmake` with new files
- [ ] Write unit tests for mark storage classes

### Optional: Clean Up Dead Code
- [ ] Remove `refineMarkByCorrelation()` if not planning to use it, OR
- [ ] Fix allocation in `refineMarkByCorrelation()` if you plan to use it (Step 1)

### Testing (Current Implementation)
- [ ] Profile to verify zero allocations on audio thread (should already pass!)
- [ ] Test with 64-sample buffer size
- [ ] Test with varying pitch (vibrato, glides)

---

## Future Enhancements (Optional)

Once mark storage is in place, you can:

1. **Improve Mark Selection:**
   ```cpp
   // In GranulatorProcessor.h - add pre-allocated buffer for stability calculation
   private:
       static constexpr int MAX_STABILITY_WINDOW = 16;
       std::array<float, MAX_STABILITY_WINDOW> mPeriodBuffer;  // Pre-allocated

   // Use multiple recent marks to detect jitter (REALTIME-SAFE)
   float calculateMarkStability(int numRecentMarks)
   {
       if (mAnalysisMarkHistory.getCount() < numRecentMarks)
           return 0.0f;

       jassert(numRecentMarks <= MAX_STABILITY_WINDOW);
       const int count = juce::jmin(numRecentMarks, MAX_STABILITY_WINDOW);

       // [OK] Use pre-allocated buffer, no allocation
       for (int i = 0; i < count - 1; ++i)
       {
           juce::int64 mark1 = mAnalysisMarkHistory.getMark(i);
           juce::int64 mark2 = mAnalysisMarkHistory.getMark(i + 1);
           mPeriodBuffer[i] = static_cast<float>(mark1 - mark2);
       }

       // Calculate variance to detect stability
       return calculateVariance(mPeriodBuffer.data(), count - 1);
   }

   // Helper function to calculate variance (realtime-safe)
   float calculateVariance(const float* values, int count)
   {
       if (count == 0) return 0.0f;

       float sum = 0.0f;
       for (int i = 0; i < count; ++i)
           sum += values[i];

       float mean = sum / count;

       float variance = 0.0f;
       for (int i = 0; i < count; ++i)
       {
           float diff = values[i] - mean;
           variance += diff * diff;
       }

       return variance / count;
   }
   ```

2. **Better Grain Timing:**
   - Use mark history to predict grain placement more accurately
   - Smooth out jitter by analyzing mark variance

3. **Adaptive Search Radius:**
   - Narrow search when marks are stable
   - Widen search when marks show high variance

4. **Debugging Visualization:**
   - Export mark history for plotting
   - Analyze pitch tracking accuracy

---

## Summary

### Good News: Already Realtime-Safe
Your current implementation has **zero allocations in the active audio path**. The code is production-ready from a realtime safety perspective.

### Optional Enhancements
1. **Add pitch mark storage** for debugging and future algorithm improvements
2. **Remove or fix unused `refineMarkByCorrelation()`** (optional cleanup)

### Current State
- **Zero allocations on audio thread** (verified!)
- Good grain management (fixed-size array)
- Pre-allocated grain buffers
- Pre-allocated CircularBuffer and detection buffers
- [NOTE] Unused dead code with allocation (not affecting runtime)
- [LIMITATION] No persistent mark storage (limits debugging/analysis)

### After Adding Mark Storage (Optional)
- Comprehensive mark history for debugging
- Better testing capabilities
- Foundation for algorithm improvements
- Aligned with TD-PSOLA best practices
- Still zero allocations on audio thread
