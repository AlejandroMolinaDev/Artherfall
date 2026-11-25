#pragma once

#include <array>
#include <cmath>

#include "DSP/Utils/DelayLine.h"
#include "DSP/Utils/DspMath.h"
#include "DSP/Utils/Filters.h"
#include "Diffuser.h"
#include "PitchShifter.h"

namespace aetherfall::dsp
{

/**
    16-line Feedback Delay Network — the tank of ATMOS CORE.

    Per sample, for every line i:
        y_i = read(line_i, length_i + modulation_i)        (cubic interpolation)
        y_i = shimmer crossfade (pitch-shifted read of the same line)
        y_i = late all-pass diffuser_i (y_i)               (echo density grows)
        y_i = HF shelf + LF shelf (frequency-dependent decay)
        y_i = g_i * y_i                                     (decay, per length)
    then the 16 outputs are mixed by a normalised Hadamard matrix (orthogonal,
    i.e. energy preserving) and written back, together with the input.

    Why this works:
      * an orthogonal feedback matrix is lossless, so the decay is set ONLY by
        the per-line gains g_i = 10^(-3 * L_i / (RT60 * fs)): every line loses
        60 dB after RT60 seconds regardless of its length -> smooth decay;
      * the Hadamard matrix mixes every line into every other one, so echo
        density explodes after a few round trips;
      * slowly modulated read positions break up metallic resonances (and add
        the lush chorus character);
      * the shimmer reads the same delay buffer through two granular heads
        (pitch shifter) and blends that into the loop: the pitched energy is
        still scaled by g_i < 1, so the shimmer can never run away.

    All state is double precision (long feedback loops accumulate error).
*/
class FDN16
{
public:
    static constexpr int numLines = 16;
    static constexpr int numShimmerLines = 8;   // lines 0..7 carry shimmer

    /** Targets for one control-rate chunk (all values reached by linear ramps). */
    struct Params
    {
        std::array<double, numLines> lengthSamples {};  // main delay per line
        std::array<double, numLines> gain {};           // per-line decay gain
        std::array<double, numLines> hfShelfGain {};    // HF band gain relative to mid
        std::array<double, numLines> lfShelfGain {};    // LF band gain relative to mid
        double hfCutoffHz = 8000.0;
        double lfCutoffHz = 100.0;
        double modDepthSamples = 0.0;
        double modRateHz = 0.3;
        double lateDiffusion = 0.5;   // all-pass gain inside the loop
        double lateScale = 1.0;       // length scale of the in-loop all-passes
        double shimmer = 0.0;         // 0..1 crossfade towards the shifted reads
        double shimmerBlend = 0.0;    // 0 = interval A, 1 = interval B
        double ratioA = 2.0, ratioB = 1.5;
        double saturation = 0.0;      // in-loop soft saturation (Lo-Fi colour)
        double inputGain = 1.0;       // 0 when frozen
        bool linearInterpolation = false;   // Eco quality: cheaper reads
    };

    void prepare (double sampleRate, double maxLengthSamples)
    {
        sr = sampleRate;
        shimmerWindow = 0.07 * sr;

        // Room for: longest line + modulation + shimmer window + margin.
        const int maxDelay = (int) std::ceil (maxLengthSamples + 0.02 * sr + shimmerWindow) + 16;
        for (auto& l : lines)
            l.prepare (maxDelay);

        for (int i = 0; i < numLines; ++i)
            lateAP[(size_t) i].prepare ((int) std::ceil (lateBaseSec[(size_t) i] * maxLateScale * sr) + 4);

        reset();
    }

    void reset()
    {
        for (auto& l : lines) l.reset();
        for (auto& a : lateAP) a.reset();
        for (auto& f : hfLP) f.reset();
        for (auto& f : lfLP) f.reset();

        for (int i = 0; i < numLines; ++i)
        {
            const double ph = (double) i * 0.6180339887 * twoPi;   // golden-ratio spread
            modRe[(size_t) i] = std::cos (ph);
            modIm[(size_t) i] = std::sin (ph);
        }

        for (int i = 0; i < numShimmerLines; ++i)
        {
            headsA[(size_t) i].setWindow (shimmerWindow);
            headsB[(size_t) i].setWindow (shimmerWindow);
            headsA[(size_t) i].reset ((double) i / numShimmerLines);
            headsB[(size_t) i].reset ((double) i / numShimmerLines + 0.25);
        }

        initialised = false;
    }

    /** Sets new targets; values ramp linearly over `numSamples`. */
    void beginChunk (const Params& p, int numSamples) noexcept
    {
        const double inv = numSamples > 0 ? 1.0 / numSamples : 1.0;

        if (! initialised)
        {
            cur = p;
            initialised = true;
        }

        for (int i = 0; i < numLines; ++i)
        {
            const auto k = (size_t) i;
            lenStep[k]  = (p.lengthSamples[k] - cur.lengthSamples[k]) * inv;
            gainStep[k] = (p.gain[k]          - cur.gain[k]) * inv;
            hfStep[k]   = (p.hfShelfGain[k]   - cur.hfShelfGain[k]) * inv;
            lfStep[k]   = (p.lfShelfGain[k]   - cur.lfShelfGain[k]) * inv;

            hfLP[k].setCutoff (p.hfCutoffHz, sr);
            lfLP[k].setCutoff (p.lfCutoffHz, sr);

            lateAP[k].setDelay (lateBaseSec[k] * p.lateScale * sr);
            lateAP[k].setGain ((i & 1) ? -p.lateDiffusion : p.lateDiffusion);

            // Per-line modulation rate spread (0.62x .. 1.38x) avoids the lines
            // moving in lockstep (which would sound like a single chorus).
            const double w = twoPi * p.modRateHz * modRateSpread[k] / sr;
            rotC[k] = std::cos (w);
            rotS[k] = std::sin (w);

            // Keep the phasors on the unit circle (rounding drift).
            const double mag = std::sqrt (modRe[k] * modRe[k] + modIm[k] * modIm[k]);
            if (mag > 0.0) { modRe[k] /= mag; modIm[k] /= mag; }
        }

        modDepthStep  = (p.modDepthSamples - cur.modDepthSamples) * inv;
        shimmerStep   = (p.shimmer - cur.shimmer) * inv;
        blendStep     = (p.shimmerBlend - cur.shimmerBlend) * inv;
        satStep       = (p.saturation - cur.saturation) * inv;
        inGainStep    = (p.inputGain - cur.inputGain) * inv;

        linear = p.linearInterpolation;
        for (auto& h : headsA) h.setRatio (p.ratioA);
        for (auto& h : headsB) h.setRatio (p.ratioB);
    }

    /** Processes one stereo sample through the tank. */
    void process (double inL, double inR, double& outL, double& outR) noexcept
    {
        std::array<double, numLines> v;

        cur.modDepthSamples += modDepthStep;
        cur.shimmer         += shimmerStep;
        cur.shimmerBlend    += blendStep;
        cur.saturation      += satStep;
        cur.inputGain       += inGainStep;

        const double shim  = cur.shimmer;
        const double blend = clamp (cur.shimmerBlend, 0.0, 1.0);
        const double sat   = cur.saturation;

        for (int i = 0; i < numLines; ++i)
        {
            const auto k = (size_t) i;

            cur.lengthSamples[k] += lenStep[k];
            cur.gain[k]          += gainStep[k];
            cur.hfShelfGain[k]   += hfStep[k];
            cur.lfShelfGain[k]   += lfStep[k];

            // Rotate the modulation phasor (sin/cos oscillator without trig calls).
            const double re = modRe[k] * rotC[k] - modIm[k] * rotS[k];
            const double im = modRe[k] * rotS[k] + modIm[k] * rotC[k];
            modRe[k] = re;
            modIm[k] = im;

            // Read-before-write: a delay of D samples is tap(D - 1).
            const double d = cur.lengthSamples[k] + cur.modDepthSamples * (1.0 + im);
            double y = linear ? lines[k].tapLinear (d - 1.0) : lines[k].tapCubic (d - 1.0);

            if (i < numShimmerLines && shim > 1.0e-4)
            {
                double shifted = 0.0;

                if (blend < 0.999)
                {
                    const auto t = headsA[k].next();
                    shifted += (1.0 - blend) * (t.weightA * lines[k].tapCubic (d - 1.0 + t.offsetA)
                                              + t.weightB * lines[k].tapCubic (d - 1.0 + t.offsetB));
                }
                if (blend > 0.001)
                {
                    const auto t = headsB[k].next();
                    shifted += blend * (t.weightA * lines[k].tapCubic (d - 1.0 + t.offsetA)
                                      + t.weightB * lines[k].tapCubic (d - 1.0 + t.offsetB));
                }

                y += shim * (shifted - y);
            }

            y = lateAP[k].process (y);

            // Frequency-dependent decay: two first-order shelves.
            const double lp = hfLP[k].processLP (y);
            y = lp + (y - lp) * cur.hfShelfGain[k];
            const double lo = lfLP[k].processLP (y);
            y += lo * (cur.lfShelfGain[k] - 1.0);

            if (sat > 1.0e-4)
                y += sat * (softSat (y * 1.5) * (1.0 / 1.5) - y);

            y *= cur.gain[k];

            // Safety: never let a NaN in, and gently bound runaway energy.
            if (! std::isfinite (y))
                y = 0.0;
            else if (std::abs (y) > 2.0)
                y = std::copysign (2.0 + fastTanh (std::abs (y) - 2.0), y);

            v[k] = y;
        }

        // Output taps: two orthogonal +-1 patterns (Hadamard rows) -> L/R decorrelated.
        double l = 0.0, r = 0.0;
        for (int i = 0; i < numLines; ++i)
        {
            l += tapSignL[(size_t) i] * v[(size_t) i];
            r += tapSignR[(size_t) i] * v[(size_t) i];
        }
        outL = l * 0.25;
        outR = r * 0.25;

        hadamard16 (v);

        // Inject the input: L into even lines, R into odd lines, with signs.
        const double gl = inL * cur.inputGain * inputScale;
        const double gr = inR * cur.inputGain * inputScale;
        for (int i = 0; i < numLines; ++i)
        {
            const double in = (i & 1) ? gr : gl;
            lines[(size_t) i].write (flushDenormal (v[(size_t) i] + inSign[(size_t) i] * in));
        }
    }

    /** Base lengths (seconds) of the 16 lines at Size = 1. */
    static constexpr std::array<double, numLines> baseLengthSec {
        0.0503, 0.0571, 0.0637, 0.0719, 0.0811, 0.0907, 0.1013, 0.1129,
        0.1259, 0.1409, 0.1567, 0.1747, 0.1949, 0.2179, 0.2437, 0.2741
    };

    /** In-loop all-pass lengths (seconds at scale 1), mutually irregular. */
    static constexpr std::array<double, numLines> lateBaseSec {
        0.00431, 0.00557, 0.00673, 0.00797, 0.00911, 0.01039, 0.00463, 0.00601,
        0.00719, 0.00853, 0.00977, 0.01103, 0.00503, 0.00641, 0.00751, 0.00887
    };

    /** Largest scale accepted by Params::lateScale. */
    static constexpr double maxLateScale = 1.7;

    double getShimmerWindow() const noexcept { return shimmerWindow; }

private:
    /** In-place fast Walsh-Hadamard transform, normalised (orthogonal). */
    static void hadamard16 (std::array<double, numLines>& x) noexcept
    {
        for (int len = 1; len < numLines; len <<= 1)
            for (int i = 0; i < numLines; i += len << 1)
                for (int j = i; j < i + len; ++j)
                {
                    const double a = x[(size_t) j], b = x[(size_t) (j + len)];
                    x[(size_t) j] = a + b;
                    x[(size_t) (j + len)] = a - b;
                }

        for (auto& s : x)
            s *= 0.25;   // 1 / sqrt(16)
    }

    static constexpr std::array<double, numLines> modRateSpread {
        0.62, 1.31, 0.87, 1.14, 0.71, 1.38, 0.95, 1.07,
        0.79, 1.22, 0.66, 1.01, 1.27, 0.83, 1.18, 0.91
    };

    static constexpr std::array<double, numLines> tapSignL {
        1, -1,  1, -1, -1,  1, -1,  1,  1, -1,  1, -1, -1,  1, -1,  1 };   // Hadamard row 5
    static constexpr std::array<double, numLines> tapSignR {
        1,  1, -1, -1,  1,  1, -1, -1, -1, -1,  1,  1, -1, -1,  1,  1 };   // Hadamard row 10
    static constexpr std::array<double, numLines> inSign {
        1,  1, -1,  1,  1, -1,  1, -1, -1,  1,  1,  1, -1, -1,  1, -1 };

    static constexpr double inputScale = 0.3535533906; // 1 / sqrt(8): 8 lines per channel

    std::array<DelayLine<double>, numLines> lines;
    std::array<AllpassDiffuser<double>, numLines> lateAP;
    std::array<OnePole<double>, numLines> hfLP, lfLP;
    std::array<GranularHeads<double>, numShimmerLines> headsA, headsB;

    std::array<double, numLines> modRe {}, modIm {}, rotC {}, rotS {};
    std::array<double, numLines> lenStep {}, gainStep {}, hfStep {}, lfStep {};
    double modDepthStep = 0.0, shimmerStep = 0.0, blendStep = 0.0, satStep = 0.0, inGainStep = 0.0;

    Params cur;
    bool initialised = false, linear = false;
    double sr = 48000.0;
    double shimmerWindow = 3360.0;
};

} // namespace aetherfall::dsp
