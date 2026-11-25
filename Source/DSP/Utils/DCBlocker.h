#pragma once

#include "DspMath.h"

namespace aetherfall::dsp
{

/** First-order DC blocking high-pass:  y[n] = x[n] - x[n-1] + R * y[n-1].
    R is derived from the cutoff (default ~8 Hz), inaudible but removes the DC
    that saturation / feedback / asymmetric shaping can accumulate. */
template <typename T>
class DCBlocker
{
public:
    void prepare (double sampleRate, double cutoffHz = 8.0) noexcept
    {
        R = (T) std::exp (-twoPi * cutoffHz / sampleRate);
        reset();
    }

    void reset() noexcept { x1 = y1 = T (0); }

    T process (T x) noexcept
    {
        const T y = x - x1 + R * y1;
        x1 = x;
        y1 = flushDenormal (y);
        return y1;
    }

private:
    T R = T (0.999), x1 {}, y1 {};
};

} // namespace aetherfall::dsp
