#pragma once

#include <cmath>
#include <algorithm>
#include "DspMath.h"

// -----------------------------------------------------------------------------
// Parameter smoothers. Parameters arrive at control rate (one value per
// processing chunk); these turn them into zipper-free per-sample signals.
// -----------------------------------------------------------------------------
namespace aetherfall::dsp
{

/** Linear ramp towards a target over a fixed time. Ideal for gains and mixes:
    reaches the target exactly, constant cost. */
template <typename T>
class LinearSmoother
{
public:
    void prepare (double sampleRate, double rampSeconds) noexcept
    {
        rampLength = std::max (1, (int) std::lround (sampleRate * rampSeconds));
        setCurrentAndTarget (target);
    }

    void setCurrentAndTarget (T v) noexcept
    {
        current = target = v;
        countdown = 0;
        step = T (0);
    }

    void setTarget (T v) noexcept
    {
        if (exactlyEqual (v, target))
            return;

        target = v;
        countdown = rampLength;
        step = (target - current) / (T) rampLength;
    }

    T next() noexcept
    {
        if (countdown > 0)
        {
            current += step;
            if (--countdown == 0)
                current = target;
        }
        return current;
    }

    /** Advances n samples at once (when the per-sample values are not needed). */
    void skip (int n) noexcept
    {
        if (countdown <= 0)
            return;

        if (n >= countdown)
        {
            current = target;
            countdown = 0;
        }
        else
        {
            current += step * (T) n;
            countdown -= n;
        }
    }

    bool isSmoothing() const noexcept { return countdown > 0; }
    T getCurrent() const noexcept     { return current; }
    T getTarget() const noexcept      { return target; }

private:
    T current {}, target {}, step {};
    int countdown = 0, rampLength = 1;
};

/** Exponential (one-pole) smoother. Good for frequencies/times where an
    exponential approach sounds natural. `seconds` is the time constant. */
template <typename T>
class OnePoleSmoother
{
public:
    void prepare (double sampleRate, double seconds) noexcept
    {
        coeff = timeConstantCoeff ((T) seconds, (T) sampleRate);
    }

    void setCurrentAndTarget (T v) noexcept { current = target = v; }
    void setTarget (T v) noexcept           { target = v; }

    T next() noexcept
    {
        current = target + coeff * (current - target);
        return current;
    }

    T getCurrent() const noexcept { return current; }
    T getTarget() const noexcept  { return target; }

private:
    T current {}, target {}, coeff {};
};

/** Smooths a control-rate value across one chunk: the value ramps linearly
    from the previous chunk value to the new one over `chunkLength` samples.
    Cheap and exactly time-aligned with the control rate. */
template <typename T>
class ControlRamp
{
public:
    void reset (T v) noexcept { current = target = v; step = T (0); }

    void beginChunk (T newTarget, int chunkLength) noexcept
    {
        current = target;
        target = newTarget;
        step = chunkLength > 0 ? (target - current) / (T) chunkLength : T (0);
        if (chunkLength <= 0)
            current = target;
    }

    T next() noexcept
    {
        current += step;
        return current;
    }

    T getTarget() const noexcept { return target; }

private:
    T current {}, target {}, step {};
};

} // namespace aetherfall::dsp
