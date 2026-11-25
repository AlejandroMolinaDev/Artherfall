#pragma once

#include <array>
#include <cmath>
#include "DSP/Chain/ModContext.h"
#include "DSP/Utils/DspMath.h"
#include "DSP/Utils/Random.h"
#include "Parameters/NoteDivision.h"
#include "ModRouting.h"

// -----------------------------------------------------------------------------
// Control-rate modulators. Each is advanced once per processing chunk (<= 64
// samples) and returns one value for that chunk.
// -----------------------------------------------------------------------------
namespace aetherfall::mod
{

/** Low-frequency oscillator (spec section 5): 8 shapes, free or synced,
    phase offset, retrigger, polarity and output smoothing. */
class Lfo
{
public:
    struct Settings
    {
        int shape = 0;            // choices::lfoShape
        double rateHz = 1.0;
        bool sync = false;
        int division = 16;
        double phaseOffset = 0.0;
        int trigger = 0;          // 0 free, 1 transport, 2 transient, 3 MIDI
        bool unipolar = false;
        float smooth = 0.0f;
    };

    void reset (double initialPhase, std::uint32_t seed) noexcept
    {
        phase = initialPhase;
        cycle = 0;
        rng.setSeed (seed);
        held = rng.nextBipolar();
        prevRandom = held;
        nextRandom = rng.nextBipolar();
        smoothed = 0.0f;
        wasPlaying = false;
        primed = false;
    }

    /** Advances by n samples and returns the value at the start of the chunk. */
    float process (const Settings& s, const TransportState& t, double sampleRate, int n,
                   bool transientOnset, bool midiNoteOn, const std::array<float, (size_t) lfoCustomPoints>& custom) noexcept
    {
        const double beats = notediv::toQuarterBeats (s.division, t.timeSigNumerator, t.timeSigDenominator);
        const double cyclesPerSecond = s.sync ? (beats > 0.0 ? (t.bpm / 60.0) / beats : 1.0) : std::max (0.0, s.rateHz);

        // Retrigger sources.
        const bool transportStart = t.isPlaying && ! wasPlaying;
        wasPlaying = t.isPlaying;
        if ((s.trigger == 1 && transportStart) || (s.trigger == 2 && transientOnset) || (s.trigger == 3 && midiNoteOn))
            phase = 0.0;

        long long newCycle = cycle;
        if (s.sync && t.isPlaying && t.hasPpq && beats > 0.0 && s.trigger != 2 && s.trigger != 3)
        {
            // Locked to the song position: lands on the beat after jumps/loops.
            const double pos = t.ppqPosition / beats;
            newCycle = (long long) std::floor (pos);
            phase = pos - std::floor (pos);
        }
        else if (phase >= 1.0)
        {
            newCycle += (long long) std::floor (phase);
            phase -= std::floor (phase);
        }

        if (newCycle != cycle)
        {
            cycle = newCycle;
            held = rng.nextBipolar();
            prevRandom = nextRandom;
            nextRandom = rng.nextBipolar();
        }

        double p = phase + s.phaseOffset;
        p -= std::floor (p);

        float v = 0.0f;
        switch (s.shape)
        {
            case 0: v = (float) std::sin (dsp::twoPi * p); break;                       // Sine
            case 1: v = (float) (1.0 - 4.0 * std::abs (p - 0.5)); break;                // Triangle
            case 2: v = (float) (2.0 * p - 1.0); break;                                 // Saw up
            case 3: v = (float) (1.0 - 2.0 * p); break;                                 // Saw down
            case 4: v = p < 0.5 ? 1.0f : -1.0f; break;                                  // Square
            case 5: v = held; break;                                                    // S&H
            case 6:                                                                     // Smooth random
            {
                const float k = (float) (0.5 - 0.5 * std::cos (dsp::pi * p));
                v = prevRandom + k * (nextRandom - prevRandom);
                break;
            }
            case 7:                                                                     // Custom
            {
                const double x = p * lfoCustomPoints;
                const int i0 = (int) x % lfoCustomPoints;
                const int i1 = (i0 + 1) % lfoCustomPoints;
                const float f = (float) (x - std::floor (x));
                v = custom[(size_t) i0] + f * (custom[(size_t) i1] - custom[(size_t) i0]);
                break;
            }
            default: break;
        }

        if (s.unipolar)
            v = 0.5f * (v + 1.0f);

        // Output smoothing (0 .. 0.5 s), useful for S&H / square.
        const double tau = (double) s.smooth * s.smooth * 0.5;
        if (! primed || tau <= 1.0e-4)
            smoothed = v;
        else
            smoothed = v + (float) std::exp (-(double) n / (tau * sampleRate)) * (smoothed - v);
        primed = true;

        phase += cyclesPerSecond * (double) n / sampleRate;
        return smoothed;
    }

    double getPhase() const noexcept { return phase; }

private:
    double phase = 0.0;
    long long cycle = 0;
    dsp::Random32 rng;
    float held = 0.0f, prevRandom = 0.0f, nextRandom = 0.0f, smoothed = 0.0f;
    bool wasPlaying = false, primed = false;
};

/** Envelope follower on the input or on the wet signal (unipolar 0..1). */
class EnvFollower
{
public:
    void reset() noexcept { env = 0.0f; }

    float process (const float* l, const float* r, int n, double sampleRate,
                   float attackMs, float releaseMs, float gainDb) noexcept
    {
        const float atk = (float) std::exp (-1.0 / (std::max (0.05f, attackMs) * 0.001 * sampleRate));
        const float rel = (float) std::exp (-1.0 / (std::max (1.0f, releaseMs) * 0.001 * sampleRate));
        if (l != nullptr && r != nullptr)
            for (int i = 0; i < n; ++i)
            {
                const float x = std::max (std::abs (l[i]), std::abs (r[i]));
                env = x + (x > env ? atk : rel) * (env - x);
            }
        return dsp::clamp (env * dsp::dbToGain (gainDb) * 2.0f, 0.0f, 1.0f);
    }

private:
    float env = 0.0f;
};

/** 2..32-step sequencer, tempo synced, with glide. */
class StepSeq
{
public:
    void reset() noexcept { position = 0.0; value = 0.0f; primed = false; }

    float process (const std::array<float, (size_t) seqMaxSteps>& steps, int numSteps, int division,
                   float glide, bool bipolar, const TransportState& t, double sampleRate, int n) noexcept
    {
        const double stepBeats = notediv::toQuarterBeats (division, t.timeSigNumerator, t.timeSigDenominator);
        if (t.isPlaying && t.hasPpq && stepBeats > 0.0)
            position = t.ppqPosition / stepBeats;

        numSteps = dsp::clamp (numSteps, 1, seqMaxSteps);
        const long long idx = ((long long) std::floor (position) % numSteps + numSteps) % numSteps;
        const float target = steps[(size_t) idx];

        const double stepSeconds = stepBeats * 60.0 / std::max (20.0, t.bpm);
        const double tau = (double) glide * stepSeconds * 0.5;
        if (! primed || tau <= 1.0e-4)
            value = target;
        else
            value = target + (float) std::exp (-(double) n / (tau * sampleRate)) * (value - target);
        primed = true;

        position += (double) n / sampleRate / std::max (1.0e-4, stepSeconds);
        return bipolar ? value * 2.0f - 1.0f : value;
    }

private:
    double position = 0.0;
    float value = 0.0f;
    bool primed = false;
};

/** Slow chaotic movement: Lorenz attractor or a reflected random walk. */
class Chaos
{
public:
    void reset (std::uint32_t seed) noexcept
    {
        x = 0.1; y = 0.0; z = 0.0;
        walk = 0.0f;
        smoothed = 0.0f;
        rng.setSeed (seed);
    }

    float process (int mode, double rateHz, float smooth, double sampleRate, int n) noexcept
    {
        const double secs = (double) n / sampleRate;
        float v = 0.0f;

        if (mode == 0)
        {
            // Lorenz system (sigma 10, rho 28, beta 8/3), time scaled by the rate.
            const double dtTotal = secs * rateHz * 2.0;
            const int sub = std::max (1, (int) std::ceil (dtTotal / 0.005));
            const double dt = dtTotal / sub;
            for (int k = 0; k < sub; ++k)
            {
                const double dx = 10.0 * (y - x);
                const double dy = x * (28.0 - z) - y;
                const double dz = x * y - (8.0 / 3.0) * z;
                x += dx * dt; y += dy * dt; z += dz * dt;
            }
            v = dsp::clamp ((float) (x / 20.0), -1.0f, 1.0f);
        }
        else
        {
            // Drunk walk with reflection at +-1.
            walk += rng.nextBipolar() * (float) (rateHz * secs * 2.0);
            if (walk > 1.0f)  walk = 2.0f - walk;
            if (walk < -1.0f) walk = -2.0f - walk;
            v = walk;
        }

        const double tau = 0.02 + (double) smooth * 1.0;
        smoothed = v + (float) std::exp (-secs / tau) * (smoothed - v);
        return smoothed;
    }

private:
    double x = 0.1, y = 0.0, z = 0.0;
    float walk = 0.0f, smoothed = 0.0f;
    dsp::Random32 rng;
};

} // namespace aetherfall::mod
