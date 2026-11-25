#pragma once

#include <cmath>
#include "Parameters/ParamIDs.h"
#include "Parameters/NoteDivision.h"

namespace aetherfall
{

/** Host transport snapshot for the current processing chunk. */
struct TransportState
{
    double bpm = 120.0;
    double ppqPosition = 0.0;   // quarter notes at the start of the chunk
    bool   isPlaying = false;
    bool   hasPpq = false;      // host provided a musical position
    int    timeSigNumerator = 4;
    int    timeSigDenominator = 4;
};

/** Values extracted by the Input Analyzer (phase F5). Zero until then. */
struct AnalysisFrame
{
    float rms = 0.0f;              // linear RMS of the input
    float peak = 0.0f;             // linear peak of the input
    float transient = 0.0f;        // 0..1 transient strength (decaying)
    float pitchHz = 0.0f;          // detected fundamental (0 = none)
    float pitchConfidence = 0.0f;  // 0..1
    float centroidHz = 0.0f;       // spectral centroid (brightness)
};

/**
    Everything a module needs to know for one processing chunk.

    `values` holds the EFFECTIVE value of every parameter (host value plus
    modulation), in real units, indexed by PID. It is refreshed at control rate
    (every <= 64 samples) by the processor; modules smooth internally.
*/
struct ModContext
{
    const float* values = nullptr;
    double sampleRate = 48000.0;
    int numSamples = 0;               // samples in this chunk
    TransportState transport;
    AnalysisFrame analysis;

    float get (PID p) const noexcept      { return values[idx (p)]; }
    bool  getBool (PID p) const noexcept  { return values[idx (p)] >= 0.5f; }
    int   getChoice (PID p) const noexcept { return (int) std::lround (values[idx (p)]); }

    /** Length of a tempo division (index into choices::noteDiv) in seconds. */
    double divisionSeconds (int divIndex) const noexcept
    {
        return notediv::toSeconds (divIndex, transport.bpm,
                                   transport.timeSigNumerator, transport.timeSigDenominator);
    }

    /** Resolves a "time in ms / sync / division" triple to seconds. */
    double timeSeconds (PID msParam, PID syncParam, PID divParam) const noexcept
    {
        return getBool (syncParam) ? divisionSeconds (getChoice (divParam))
                                   : (double) get (msParam) * 0.001;
    }

    /** Resolves a "rate in Hz / sync / division" triple to Hz. */
    double rateHz (PID hzParam, PID syncParam, PID divParam) const noexcept
    {
        if (getBool (syncParam))
        {
            const double secs = divisionSeconds (getChoice (divParam));
            return secs > 0.0 ? 1.0 / secs : 1.0;
        }
        return (double) get (hzParam);
    }
};

} // namespace aetherfall
