/**
 * @file GrainExport.h
 * @brief Data structures and export functions for TD-PSOLA grain analysis
 *
 * Provides detailed grain information for debugging and analysis of the TD-PSOLA algorithm.
 * Exports synthesis grain data showing how the algorithm maps source to output grains.
 */

#pragma once

#include "Util/Juce_Header.h"
#include <vector>
#include <fstream>

namespace TD_PSOLA
{

/**
 * @brief Represents a synthesis grain in the TD-PSOLA output
 *
 * Contains information about where the grain came from (source) and
 * where it was placed in the output (synthesis position).
 */
struct SynthesisGrain
{
    int grainId;               // Sequential grain index
    int centerSample;          // Synthesis mark position (center of output grain)
    int startSample;           // Start of synthesis window
    int endSample;             // End of synthesis window
    int sourceAnalysisId;      // Which analysis grain this maps to
    int sourceCenter;          // Center of the source analysis grain
    int sourceStart;           // Start of source signal extraction
    int sourceEnd;             // End of source signal extraction
    int sourcePeriod;          // Period at source analysis mark (distance to next mark)
    int synthesisPeriod;       // Period at synthesis mark (distance to next mark)
    float windowAlpha;         // Tukey window alpha parameter
    int durationSamples;       // Total grain length
};

/**
 * @brief Complete grain analysis data for a TD-PSOLA operation
 */
struct GrainData
{
    float fRatio;
    int signalLength;
    int numAnalysisGrains;
    int numSynthesisGrains;
    std::vector<SynthesisGrain> synthesisGrains;
};

/**
 * @brief Export grain data to CSV and summary text files
 *
 * Creates two files:
 * - <basePath>_synthesis_grains.csv: Detailed grain mapping data
 * - <basePath>_grain_summary.txt: Summary statistics
 *
 * @param grainData Grain analysis data to export
 * @param outputPath Base path for output files (e.g., "output.wav" -> "output_synthesis_grains.csv")
 * @return true if export succeeded, false otherwise
 */
inline bool exportGrainsToCSV(const GrainData& grainData, const juce::String& outputPath)
{
    // Remove extension from output path to create base path
    juce::String basePath = outputPath.upToLastOccurrenceOf(".", false, false);

    // Export synthesis grains CSV
    juce::String csvPath = basePath + "_synthesis_grains.csv";
    juce::File csvFile(csvPath);

    std::ofstream csvStream(csvFile.getFullPathName().toStdString());
    if (!csvStream.is_open())
        return false;

    // Write CSV header
    csvStream << "source_analysis_id,source_start,source_center,source_end,";
    csvStream << "grain_id,start_sample,center_sample,end_sample,";
    csvStream << "source_period,synthesis_period,duration_samples,window_alpha\n";

    // Write grain data
    for (const auto& grain : grainData.synthesisGrains)
    {
        csvStream << grain.sourceAnalysisId << ","
                  << grain.sourceStart << ","
                  << grain.sourceCenter << ","
                  << grain.sourceEnd << ","
                  << grain.grainId << ","
                  << grain.startSample << ","
                  << grain.centerSample << ","
                  << grain.endSample << ","
                  << grain.sourcePeriod << ","
                  << grain.synthesisPeriod << ","
                  << grain.durationSamples << ","
                  << grain.windowAlpha << "\n";
    }

    csvStream.close();

    // Export summary text file
    juce::String summaryPath = basePath + "_grain_summary.txt";
    juce::File summaryFile(summaryPath);

    std::ofstream summaryStream(summaryFile.getFullPathName().toStdString());
    if (!summaryStream.is_open())
        return false;

    summaryStream << "TD-PSOLA Grain Analysis Summary\n";
    summaryStream << "==================================================\n\n";
    summaryStream << "Pitch Shift Ratio (f_ratio): " << grainData.fRatio << "\n";
    summaryStream << "Signal Length: " << grainData.signalLength << " samples\n";
    summaryStream << "Number of Analysis Grains: " << grainData.numAnalysisGrains << "\n";
    summaryStream << "Number of Synthesis Grains: " << grainData.numSynthesisGrains << "\n\n";

    summaryStream.close();

    return true;
}

} // namespace TD_PSOLA
