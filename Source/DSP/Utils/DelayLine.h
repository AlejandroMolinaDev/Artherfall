#pragma once

#include <vector>
#include <cmath>
#include "DspMath.h"
#include "Interp.h"

namespace aetherfall::dsp
{

/**
    Circular delay line with power-of-two size (index wrap = bit mask).

    Conventions:
      - write(x) stores the newest sample.
      - tap(d) reads the sample written d samples BEFORE the newest one
        (tap(0) == newest). So "write then tap(D)" is a D-sample delay.
      - In feedback loops read first, then write:  y = tap(D - 1); write(x + g*y);
        which yields a delay of exactly D samples.

    Memory is allocated in prepare() only (never on the audio thread).
*/
template <typename T>
class DelayLine
{
public:
    void prepare (int maxDelaySamples)
    {
        const int size = nextPowerOfTwo (std::max (4, maxDelaySamples + 4));
        buffer.assign ((size_t) size, T (0));
        mask = size - 1;
        writePos = 0;
        maxDelay = maxDelaySamples;
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), T (0));
        writePos = 0;
    }

    int getMaxDelay() const noexcept { return maxDelay; }

    void write (T x) noexcept
    {
        buffer[(size_t) writePos] = x;
        writePos = (writePos + 1) & mask;
    }

    /** Integer tap (0 = newest sample). */
    T tap (int d) const noexcept
    {
        return buffer[(size_t) ((writePos - 1 - d) & mask)];
    }

    /** Fractional tap, linear interpolation. */
    T tapLinear (T d) const noexcept
    {
        d = clamp (d, T (0), (T) maxDelay);
        const int i = (int) d;
        const T f = d - (T) i;
        return interp::linear (tap (i), tap (i + 1), f);
    }

    /** Fractional tap, 4-point Hermite interpolation. Needs d >= 1 so that the
        sample "after" the read point is already written. */
    T tapCubic (T d) const noexcept
    {
        d = clamp (d, T (1), (T) maxDelay - T (2));
        const int i = (int) d;
        const T f = d - (T) i;
        return interp::hermite (tap (i - 1), tap (i), tap (i + 1), tap (i + 2), f);
    }

private:
    std::vector<T> buffer;
    int mask = 0, writePos = 0, maxDelay = 0;
};

} // namespace aetherfall::dsp
