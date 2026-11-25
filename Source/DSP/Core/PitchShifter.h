#pragma once

#include <cmath>
#include "DSP/Utils/DelayLine.h"
#include "DSP/Utils/DspMath.h"

namespace aetherfall::dsp
{

/**
    Two-head granular ("rotating tape head") pitch-shift reader.

    A delay line is read by two heads whose delay sweeps through a window of
    W samples at a rate of (1 - ratio) samples per sample:
      - ratio > 1 (pitch up):   the delay shrinks, the head runs faster than
                                real time -> higher pitch,
      - ratio < 1 (pitch down): the delay grows, the head runs slower.
    When a head reaches the end of the window it jumps back; it is silent at
    that instant because each head is weighted by a sin^2 window and the two
    heads are half a window apart (sin^2 + cos^2 = 1 -> constant amplitude).

    This class only produces delay OFFSETS + WEIGHTS, so it can read any
    existing delay line — e.g. the FDN lines of ATMOS CORE (shimmer inside the
    feedback loop needs no extra buffer) or its own buffer (PitchShifter).
*/
template <typename T>
class GranularHeads
{
public:
    void setWindow (T windowSamples) noexcept { window = std::max (T (16), windowSamples); }
    void setRatio (T ratio) noexcept           { slope = T (1) - ratio; }
    void reset (T initialPhase = T (0)) noexcept { phase = initialPhase - std::floor (initialPhase); }

    T getWindow() const noexcept { return window; }

    struct Taps { T offsetA, weightA, offsetB, weightB; };

    /** Advances one sample and returns the two head offsets (0..W) and weights. */
    Taps next() noexcept
    {
        phase += slope / window;
        phase -= std::floor (phase);   // wrap into [0, 1)

        T phaseB = phase + T (0.5);
        if (phaseB >= T (1)) phaseB -= T (1);

        // sin(pi*phase) via Bhaskara's approximation (max error ~0.2 %), then
        // squared. Head B is half a window away, so its weight is exactly
        // cos^2 = 1 - sin^2: the two weights always sum to 1 (no gain ripple
        // inside feedback loops).
        const T q = phase * (T (1) - phase);
        const T s = T (16) * q / (T (5) - T (4) * q);
        const T wA = s * s;
        return { phase * window, wA, phaseB * window, T (1) - wA };
    }

private:
    T window = T (2048), slope = T (0), phase = T (0);
};

/** Self-contained pitch shifter (own buffer) built on GranularHeads. */
template <typename T>
class PitchShifter
{
public:
    void prepare (double sampleRate, double windowSeconds = 0.06)
    {
        windowSamples = (T) (windowSeconds * sampleRate);
        line.prepare ((int) windowSamples + 8);
        heads.setWindow (windowSamples);
        reset();
    }

    void reset() noexcept
    {
        line.reset();
        heads.reset();
    }

    void setSemitones (T semis) noexcept { heads.setRatio (semitonesToRatio (semis)); }
    void setRatio (T ratio) noexcept     { heads.setRatio (ratio); }

    /** Latency-ish: average delay of the heads (half a window). */
    T getAverageDelay() const noexcept { return windowSamples * T (0.5); }

    T process (T x) noexcept
    {
        line.write (x);
        const auto t = heads.next();
        return t.weightA * line.tapCubic (t.offsetA + T (1))
             + t.weightB * line.tapCubic (t.offsetB + T (1));
    }

private:
    DelayLine<T> line;
    GranularHeads<T> heads;
    T windowSamples = T (2048);
};

} // namespace aetherfall::dsp
