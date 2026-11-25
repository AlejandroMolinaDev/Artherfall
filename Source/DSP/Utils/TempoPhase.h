#pragma once

#include <cmath>
#include "DSP/Chain/ModContext.h"
#include "Parameters/NoteDivision.h"

namespace aetherfall::dsp
{

/**
    Phase accumulator (0..1) for LFO-like movement that can follow the host.

    - Free mode: runs at `rateHz`.
    - Sync mode: one cycle per tempo division. While the host transport is
      playing and reports a musical position, the phase is LOCKED to the song
      position (so the movement lands on the beat even after jumps/loops);
      when stopped it keeps running at the synced rate from where it was.
*/
class TempoPhase
{
public:
    void reset (double initialPhase = 0.0) noexcept { phase = initialPhase - std::floor (initialPhase); }

    /** Call once per chunk; then call next() once per sample. */
    void beginChunk (const ModContext& ctx, bool sync, int divIndex, double rateHz, double phaseOffset = 0.0) noexcept
    {
        const double sr = ctx.sampleRate > 0.0 ? ctx.sampleRate : 48000.0;

        if (sync)
        {
            const auto& t = ctx.transport;
            const double beats = notediv::toQuarterBeats (divIndex, t.timeSigNumerator, t.timeSigDenominator);
            const double cyclesPerSecond = beats > 0.0 ? (t.bpm / 60.0) / beats : 1.0;
            increment = cyclesPerSecond / sr;

            if (t.isPlaying && t.hasPpq && beats > 0.0)
            {
                const double p = t.ppqPosition / beats + phaseOffset;
                phase = p - std::floor (p);
            }
        }
        else
        {
            increment = std::max (0.0, rateHz) / sr;
        }
    }

    /** Returns the current phase and advances one sample. */
    double next() noexcept
    {
        const double p = phase;
        phase += increment;
        if (phase >= 1.0)
            phase -= std::floor (phase);
        return p;
    }

    double get() const noexcept { return phase; }
    double getIncrement() const noexcept { return increment; }

private:
    double phase = 0.0, increment = 0.0;
};

} // namespace aetherfall::dsp
