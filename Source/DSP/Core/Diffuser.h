#pragma once

#include <array>
#include <cstddef>
#include "DSP/Utils/DelayLine.h"
#include "DSP/Utils/DspMath.h"

namespace aetherfall::dsp
{

/**
    Schroeder all-pass diffuser with a (slowly) variable fractional length.

        w[n] = x[n] - g * w[n - M]
        y[n] = g * w[n] + w[n - M]        ->  H(z) = (g + z^-M) / (1 + g z^-M)

    |H| = 1 at every frequency: it does not colour the spectrum, it smears the
    phase, turning an impulse into a dense burst of echoes. Chains of these
    are the classic way to build echo density before / inside a reverb tank.
*/
template <typename T>
class AllpassDiffuser
{
public:
    void prepare (int maxDelaySamples)
    {
        line.prepare (maxDelaySamples + 2);
        reset();
    }

    void reset() noexcept { line.reset(); }

    /** Length in samples (fractional allowed, >= 1). */
    void setDelay (T samples) noexcept { delay = clamp (samples, T (1), (T) line.getMaxDelay()); }
    void setGain (T g) noexcept        { gain = clamp (g, T (-0.95), T (0.95)); }

    T getDelay() const noexcept { return delay; }

    T process (T x) noexcept
    {
        const T delayed = line.tapLinear (delay - T (1)); // read-before-write: D-sample delay
        const T w = x - gain * delayed;
        line.write (flushDenormal (w));
        return gain * w + delayed;
    }

private:
    DelayLine<T> line;
    T delay = T (1), gain = T (0.5);
};

/** N all-pass diffusers in series with per-stage base lengths. */
template <typename T, std::size_t N>
class DiffuserChain
{
public:
    /** baseLengthsSec: length of each stage at scale 1. maxScale bounds the
        lengths reachable later through setScale(). */
    void prepare (double sampleRate, const std::array<double, N>& baseLengthsSec, double maxScale)
    {
        sr = sampleRate;
        base = baseLengthsSec;
        for (std::size_t i = 0; i < N; ++i)
            stages[i].prepare ((int) std::ceil (base[i] * maxScale * sr) + 4);
        setScale (T (1));
        setGain (T (0.6));
    }

    void reset() noexcept
    {
        for (auto& s : stages) s.reset();
    }

    void setScale (T scale) noexcept
    {
        for (std::size_t i = 0; i < N; ++i)
            stages[i].setDelay ((T) (base[i] * sr) * scale);
    }

    /** Same gain magnitude for every stage, alternating sign (more diffuse). */
    void setGain (T g) noexcept
    {
        for (std::size_t i = 0; i < N; ++i)
            stages[i].setGain ((i & 1) ? -g : g);
    }

    T process (T x) noexcept
    {
        for (auto& s : stages)
            x = s.process (x);
        return x;
    }

private:
    std::array<AllpassDiffuser<T>, N> stages;
    std::array<double, N> base {};
    double sr = 48000.0;
};

} // namespace aetherfall::dsp
