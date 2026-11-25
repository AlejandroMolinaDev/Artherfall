#pragma once

#include <cmath>
#include "DspMath.h"

// -----------------------------------------------------------------------------
// Basic filters shared by many modules. All are allocation-free and process
// one sample at a time so they can live inside feedback loops.
// -----------------------------------------------------------------------------
namespace aetherfall::dsp
{

/** One-pole low-pass (and complementary high-pass), matched-z design. */
template <typename T>
class OnePole
{
public:
    void setCutoff (T hz, T sampleRate) noexcept
    {
        hz = clamp (hz, T (1), sampleRate * T (0.49));
        a = T (1) - std::exp (T (-twoPi) * hz / sampleRate);
    }

    /** Sets the coefficient directly (0 = frozen, 1 = no filtering). */
    void setCoefficient (T coeff) noexcept { a = clamp (coeff, T (0), T (1)); }

    void reset (T v = T (0)) noexcept { z = v; }

    T processLP (T x) noexcept
    {
        z += a * (x - z);
        z = flushDenormal (z);
        return z;
    }

    T processHP (T x) noexcept { return x - processLP (x); }

    T getState() const noexcept { return z; }

private:
    T a = T (1), z {};
};

/**
    Topology-preserving-transform state variable filter (Zavalishin / Simper).
    Stable under fast modulation, gives LP/BP/HP/Notch simultaneously.
    resonance in [0, 1): 0 = Q 0.5 (no peak), -> 1 = self-oscillation edge.
*/
template <typename T>
class SVF
{
public:
    struct Outputs { T low, band, high; };

    void setParams (T cutoffHz, T resonance, T sampleRate) noexcept
    {
        cutoffHz = clamp (cutoffHz, T (5), sampleRate * T (0.48));
        g = std::tan (T (pi) * cutoffHz / sampleRate);
        k = T (2) - T (2) * clamp (resonance, T (0), T (0.995));  // k = 1/Q
        updateCoefficients();
    }

    /** Sets Q directly (Q = 0.707 is Butterworth). */
    void setParamsQ (T cutoffHz, T q, T sampleRate) noexcept
    {
        cutoffHz = clamp (cutoffHz, T (5), sampleRate * T (0.48));
        g = std::tan (T (pi) * cutoffHz / sampleRate);
        k = T (1) / std::max (q, T (0.05));
        updateCoefficients();
    }

    void reset() noexcept { ic1 = ic2 = T (0); }

    Outputs process (T x) noexcept
    {
        const T v3 = x - ic2;
        const T v1 = a1 * ic1 + a2 * v3;
        const T v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = flushDenormal (T (2) * v1 - ic1);
        ic2 = flushDenormal (T (2) * v2 - ic2);
        return { v2, v1, x - k * v1 - v2 };
    }

    T processLP (T x) noexcept    { return process (x).low; }
    T processHP (T x) noexcept    { return process (x).high; }
    T processBP (T x) noexcept    { return process (x).band; }
    T processNotch (T x) noexcept { const auto o = process (x); return o.low + o.high; }

    T getK() const noexcept { return k; }

private:
    void updateCoefficients() noexcept
    {
        a1 = T (1) / (T (1) + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    T g = T (0.1), k = T (1.414), a1 {}, a2 {}, a3 {};
    T ic1 {}, ic2 {};
};

/** RBJ-cookbook biquad, transposed direct form II. Used for static EQ
    (shelves, tilt, peaks) where coefficients change slowly. */
template <typename T>
class Biquad
{
public:
    void reset() noexcept { s1 = s2 = T (0); }

    T process (T x) noexcept
    {
        const T y = b0 * x + s1;
        s1 = flushDenormal (b1 * x - a1 * y + s2);
        s2 = flushDenormal (b2 * x - a2 * y);
        return y;
    }

    void setLowPass (T hz, T q, T fs) noexcept  { design (Type::lowPass, hz, q, T (0), fs); }
    void setHighPass (T hz, T q, T fs) noexcept { design (Type::highPass, hz, q, T (0), fs); }
    void setPeak (T hz, T q, T gainDb, T fs) noexcept      { design (Type::peak, hz, q, gainDb, fs); }
    void setLowShelf (T hz, T q, T gainDb, T fs) noexcept  { design (Type::lowShelf, hz, q, gainDb, fs); }
    void setHighShelf (T hz, T q, T gainDb, T fs) noexcept { design (Type::highShelf, hz, q, gainDb, fs); }

private:
    enum class Type { lowPass, highPass, peak, lowShelf, highShelf };

    void design (Type type, T hz, T q, T gainDb, T fs) noexcept
    {
        hz = clamp (hz, T (5), fs * T (0.49));
        q = std::max (q, T (0.05));
        const T A = std::pow (T (10), gainDb / T (40));
        const T w0 = T (twoPi) * hz / fs;
        const T cw = std::cos (w0), sw = std::sin (w0);
        const T alpha = sw / (T (2) * q);

        T nb0 {}, nb1 {}, nb2 {}, na0 {}, na1 {}, na2 {};

        switch (type)
        {
            case Type::lowPass:
                nb0 = (T (1) - cw) / T (2); nb1 = T (1) - cw; nb2 = nb0;
                na0 = T (1) + alpha; na1 = T (-2) * cw; na2 = T (1) - alpha;
                break;
            case Type::highPass:
                nb0 = (T (1) + cw) / T (2); nb1 = -(T (1) + cw); nb2 = nb0;
                na0 = T (1) + alpha; na1 = T (-2) * cw; na2 = T (1) - alpha;
                break;
            case Type::peak:
                nb0 = T (1) + alpha * A; nb1 = T (-2) * cw; nb2 = T (1) - alpha * A;
                na0 = T (1) + alpha / A; na1 = T (-2) * cw; na2 = T (1) - alpha / A;
                break;
            case Type::lowShelf:
            {
                const T sq = T (2) * std::sqrt (A) * alpha;
                nb0 = A * ((A + 1) - (A - 1) * cw + sq);
                nb1 = T (2) * A * ((A - 1) - (A + 1) * cw);
                nb2 = A * ((A + 1) - (A - 1) * cw - sq);
                na0 = (A + 1) + (A - 1) * cw + sq;
                na1 = T (-2) * ((A - 1) + (A + 1) * cw);
                na2 = (A + 1) + (A - 1) * cw - sq;
                break;
            }
            case Type::highShelf:
            {
                const T sq = T (2) * std::sqrt (A) * alpha;
                nb0 = A * ((A + 1) + (A - 1) * cw + sq);
                nb1 = T (-2) * A * ((A - 1) + (A + 1) * cw);
                nb2 = A * ((A + 1) + (A - 1) * cw - sq);
                na0 = (A + 1) - (A - 1) * cw + sq;
                na1 = T (2) * ((A - 1) - (A + 1) * cw);
                na2 = (A + 1) - (A - 1) * cw - sq;
                break;
            }
        }

        const T inv = T (1) / na0;
        b0 = nb0 * inv; b1 = nb1 * inv; b2 = nb2 * inv;
        a1 = na1 * inv; a2 = na2 * inv;
    }

    T b0 = T (1), b1 {}, b2 {}, a1 {}, a2 {};
    T s1 {}, s2 {};
};

/** First-order allpass  H(z) = (a + z^-1) / (1 + a z^-1). Building block of
    phasers and of dispersion ("smear") inside comb loops. */
template <typename T>
class Allpass1
{
public:
    /** Sets the break frequency (phase = -90 deg). */
    void setFrequency (T hz, T sampleRate) noexcept
    {
        const T t = std::tan (T (pi) * clamp (hz, T (5), sampleRate * T (0.49)) / sampleRate);
        a = (t - T (1)) / (t + T (1));
    }

    void setCoefficient (T coeff) noexcept { a = clamp (coeff, T (-0.999), T (0.999)); }
    void reset() noexcept { z = T (0); }

    T process (T x) noexcept
    {
        const T y = a * x + z;
        z = flushDenormal (x - a * y);
        return y;
    }

private:
    T a {}, z {};
};

} // namespace aetherfall::dsp
