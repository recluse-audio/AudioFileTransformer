#pragma once

#include "Util/Juce_Header.h"
#include <atomic>
#include <functional>

class BufferProcessingManager;

/**
 * @brief Owns offline file I/O for AudioFileTransformer.
 *
 * Holds input file + output directory paths, the progress callback, and the
 * worker thread that loads a WAV into a caller-owned input storage buffer,
 * runs it through the BufferProcessingManager, and writes the resulting
 * output storage buffer back out to a timestamped WAV.
 */
class FileToBufferManager
{
public:
    FileToBufferManager();
    ~FileToBufferManager();

    //==============================================================================
    // Paths
    void       setInputFile(const juce::File& file)        { mInputFile = file; }
    void       setOutputDirectory(const juce::File& dir)   { mOutputDirectory = dir; }
    juce::File getInputFile() const                        { return mInputFile; }
    juce::File getOutputDirectory() const                  { return mOutputDirectory; }

    static juce::File getDefaultInputFile();
    static juce::File getDefaultOutputDirectory();

    //==============================================================================
    // Progress / state
    void         setProgressCallback(std::function<void(float)> cb) { mProgressCallback = std::move(cb); }
    bool         isProcessing()    const { return mIsProcessing.load(); }
    bool         wasSuccessful()   const { return mSuccess.load(); }
    juce::String getError()        const { return mError; }

    //==============================================================================
    // Synchronous load: input WAV -> destBuffer (uses RD::BufferFiller overload).
    bool loadInputToBuffer(juce::AudioBuffer<float>& destBuffer,
                           int     maxSamples,
                           double& sampleRateOut,
                           int&    samplesReadOut);

    // Synchronous write: srcBuffer -> outputFile.
    bool writeBufferToFile(juce::AudioBuffer<float>& srcBuffer,
                           const juce::File& outputFile,
                           double sampleRate,
                           int    numSamplesToWrite);

    //==============================================================================
    // Threaded orchestration: load -> process -> write.
    // Caller passes references to processor-owned storage buffers and the
    // BufferProcessingManager that drives the DSP. Returns false if validation
    // fails before the thread starts.
    bool startProcessing(juce::AudioBuffer<float>& inputStorage,
                         juce::AudioBuffer<float>& outputStorage,
                         BufferProcessingManager&  bufferProcessingManager);

    void stopProcessing();

private:
    class WorkerThread;

    juce::File mInputFile;
    juce::File mOutputDirectory;
    juce::File mResolvedOutputFile;

    std::function<void(float)> mProgressCallback;

    std::atomic<bool> mIsProcessing { false };
    std::atomic<bool> mSuccess      { false };
    juce::String      mError;

    std::unique_ptr<WorkerThread> mThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FileToBufferManager)
};
