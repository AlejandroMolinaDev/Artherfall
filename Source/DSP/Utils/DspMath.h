#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>

// -----------------------------------------------------------------------------
// Small math helpers used across the DSP code. Header-only, allocation-free.
// -----------------------------------------------------------------------------
namespace aetherfall::dsp
{

inline constexpr double pi    = 3.14159265358979323846;
inline constexpr double twoPi = 2.0 * pi;
inline constexpr double halfPi = 0.5 * pi;

/** Exact comparison without -Wfloat-equal noise (intentional equality tests). */
template <typename T> constexpr bool exactlyEqual (T a, T b) noexcept { return std::equal_to<T>{} (a, b); }

template <typename T> constexpr T clamp (T x, T lo, T hi) noexcept { return x < lo ? lo : (x > hi ? hi : x); }
template <typename T> constexpr T lerp (T a, T b, T t) noexcept      { return a + (b - a) * t; }

template <typename T> inline T dbToGain (T db) noexcept
{
    return db <= T (-120) ? T (0) : std::pow (T (10), db * T (0.05));
}

template <typename T> inline T gainToDb (T gain, T floorDb = T (-120)) noexcept
{
    return gain > T (0) ? std::max (floorDb, T (20) * std::log10 (gain)) : floorDb;
}

/** Frequency ratio of an interval in semitones (12-TET). */
template <typename T> inline T semitonesToRatio (T semis) noexcept { return std::exp2 (semis / T (12)); }

/** MIDI note number <-> frequency (A4 = 440 Hz = note 69). */
template <typename T> inline T noteToHz (T note) noexcept  { return T (440) * std::exp2 ((note - T (69)) / T (12)); }
template <typename T> inline T hzToNote (T hz) noexcept    { return T (69) + T (12) * std::log2 (std::max (hz, T (1.0e-3)) / T (440)); }

/** Rational tanh approximation (Pade 7/6). Max error ~1e-5 for |x| < 4,
    output clamped to [-1, 1]. Much cheaper than std::tanh in saturators. */
template <typename T> inline T fastTanh (T x) noexcept
{
    x = clamp (x, T (-5), T (5));
    const T x2 = x * x;
    const T num = x * (T (135135) + x2 * (T (17325) + x2 * (T (378) + x2)));
    const T den = T (135135) + x2 * (T (62370) + x2 * (T (3150) + x2 * T (28)));
    return clamp (num / den, T (-1), T (1));
}

/** Very cheap tanh-like curve (Pade 3/2): ~2 % error around |x| = 1 but
    smooth and bounded. Good enough as a "warm" soft-saturation shape. */
template <typename T> inline T softSat (T x) noexcept
{
    x = clamp (x, T (-3), T (3));
    const T x2 = x * x;
    return x * (T (27) + x2) / (T (27) + T (9) * x2);
}

/** Cubic soft clipper: linear near zero, smoothly saturates to +-1 at |x| = 1.5. */
template <typename T> inline T softClipCubic (T x) noexcept
{
    x = clamp (x, T (-1.5), T (1.5));
    return x - (T (4) / T (27)) * x * x * x;
}

/** Returns 0 for NaN/Inf (feedback-loop guard). */
template <typename T> inline T sanitize (T x) noexcept
{
    return std::isfinite (x) ? x : T (0);
}

/** Flushes values below ~1e-15 to zero (denormal protection inside recursive
    loops, complementary to juce::ScopedNoDenormals). */
template <typename T> inline T flushDenormal (T x) noexcept
{
    return std::abs (x) < T (1.0e-15) ? T (0) : x;
}

/** One-pole coefficient for a time constant in seconds (63 % rise time). */
template <typename T> inline T timeConstantCoeff (T seconds, T sampleRate) noexcept
{
    return seconds <= T (0) ? T (0) : std::exp (T (-1) / (seconds * sampleRate));
}

/** Equal-power crossfade gains for t in [0, 1]. */
template <typename T> inline void equalPowerGains (T t, T& gainA, T& gainB) noexcept
{
    t = clamp (t, T (0), T (1));
    gainA = std::cos (t * T (halfPi));
    gainB = std::sin (t * T (halfPi));
}

/** Rounds up to the next power of two (>= 1). */
inline int nextPowerOfTwo (int v) noexcept
{
    int p = 1;
    while (p < v && p < (1 << 30))
        p <<= 1;
    return p;
}

} // namespace aetherfall::dsp
