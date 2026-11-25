#pragma once

namespace aetherfall::dsp::interp
{

/** Linear interpolation between y0 (t = 0) and y1 (t = 1). */
template <typename T> inline T linear (T y0, T y1, T t) noexcept { return y0 + (y1 - y0) * t; }

/** 4-point, 3rd-order Hermite (Catmull-Rom) interpolation between y0 and y1.
    ym1 is the sample before y0, y2 the one after y1. Good quality / cost for
    modulated delay lines (low HF loss compared with linear). */
template <typename T> inline T hermite (T ym1, T y0, T y1, T y2, T t) noexcept
{
    const T c0 = y0;
    const T c1 = T (0.5) * (y1 - ym1);
    const T c2 = ym1 - T (2.5) * y0 + T (2) * y1 - T (0.5) * y2;
    const T c3 = T (0.5) * (y2 - ym1) + T (1.5) * (y0 - y1);
    return ((c3 * t + c2) * t + c1) * t + c0;
}

/** 4-point Lagrange (3rd order) interpolation, slightly flatter passband. */
template <typename T> inline T lagrange3 (T ym1, T y0, T y1, T y2, T t) noexcept
{
    const T tm1 = t + T (1), t1 = t - T (1), t2 = t - T (2);
    return -ym1 * t * t1 * t2 / T (6)
           + y0 * tm1 * t1 * t2 / T (2)
           - y1 * tm1 * t * t2 / T (2)
           + y2 * tm1 * t * t1 / T (6);
}

} // namespace aetherfall::dsp::interp
