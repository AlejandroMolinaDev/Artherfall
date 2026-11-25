#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <vector>
#include "TestHelpers.h"
#include "DSP/Core/AtmosCore.h"
#include "DSP/Core/PitchShifter.h"

using namespace aetherfall;
using namespace aetherfall::test;

namespace
{
    /** Energy (mean square) of the left output in consecutive windows. */
    std::vector<double> energyEnvelope (AtmosCore& core, ParamSet& ps, double sr, double seconds,
                                        double windowSec, Signal sig = Signal::Impulse)
    {
        dsp::Random32 rng (7);
        const int total = (int) (seconds * sr);
        const int win = (int) (windowSec * sr);
        std::vector<double> env ((size_t) (total / win + 1), 0.0);

        runModule (core, ps, sr, total,
                   [&] (int n) { return generate (sig, n, rng); },
                   [&] (int n, float l, float) { env[(size_t) (n / win)] += (double) l * l / win; });
        return env;
    }

    /** Estimates RT60 from the -5..-35 dB slope of an energy envelope. */
    double estimateRT60 (const std::vector<double>& env, double windowSec)
    {
        size_t peak = 0;
        for (size_t i = 0; i < env.size(); ++i)
            if (env[i] > env[peak]) peak = i;

        const double ref = env[peak];
        double t5 = -1.0, t35 = -1.0;
        for (size_t i = peak; i < env.size(); ++i)
        {
            const double db = 10.0 * std::log10 (std::max (env[i], 1.0e-30) / ref);
            if (t5 < 0 && db <= -5.0)  t5 = (double) i * windowSec;
            if (t35 < 0 && db <= -35.0) { t35 = (double) i * windowSec; break; }
        }
        if (t5 < 0 || t35 < 0)
            return -1.0;
        return (t35 - t5) * 2.0;   // 30 dB span -> 60 dB
    }
}

TEST_CASE ("ATMOS CORE decay time follows the Decay parameter", "[core]")
{
    const double sr = 48000.0;

    for (float decay : { 1.5f, 4.0f })
    {
        AtmosCore core;
        core.prepare (sr, 64, 2);

        ParamSet ps;
        ps.set (PID::CoreDecay, decay).set (PID::CoreMix, 1.0f).set (PID::CoreModDepth, 0.0f)
          .set (PID::CoreBloom, 0.0f).set (PID::CorePreDelay, 0.0f)
          .set (PID::CoreDampHF, 20000.0f).set (PID::CoreDampLF, 20.0f);

        const auto env = energyEnvelope (core, ps, sr, decay * 1.6, 0.02);
        const double rt = estimateRT60 (env, 0.02);
        INFO ("decay " << decay << " measured RT60 " << rt);
        REQUIRE (rt > decay * 0.6);
        REQUIRE (rt < decay * 1.4);
    }
}

TEST_CASE ("ATMOS CORE stays finite and bounded under stress", "[core]")
{
    const double sr = 48000.0;
    const int color = GENERATE (0, 1, 2, 3, 4);
    const auto sig = GENERATE (Signal::Noise, Signal::Impulse, Signal::DC, Signal::Silence, Signal::Sine);

    AtmosCore core;
    core.prepare (sr, 64, 2);

    ParamSet ps;
    ps.set (PID::CoreColor, (float) color).set (PID::CoreMix, 1.0f)
      .set (PID::CoreSize, 1.0f).set (PID::CoreDecay, 60.0f)
      .set (PID::CoreShimmer, 1.0f).set (PID::CoreShimBlend, 0.5f)
      .set (PID::CoreDiffEarly, 1.0f).set (PID::CoreDiffLate, 1.0f)
      .set (PID::CoreModDepth, 1.0f).set (PID::CoreModRate, 5.0f)
      .set (PID::CoreBloom, 1.0f);

    dsp::Random32 rng (99);
    float peak = 0.0f;
    bool finite = true;

    runModule (core, ps, sr, (int) (sr * 6),
               [&] (int n) { return generate (sig, n, rng, 0.9f); },
               [&] (int, float l, float r)
               {
                   finite = finite && std::isfinite (l) && std::isfinite (r);
                   peak = std::max ({ peak, std::abs (l), std::abs (r) });
               },
               [&] (int n)
               {
                   // Sweep size while running: the delay lengths must glide safely.
                   ps.set (PID::CoreSize, 0.1f + 0.9f * (float) ((n / 4800) % 2));
               });

    INFO ("color " << color << " signal " << (int) sig << " peak " << peak);
    REQUIRE (finite);
    REQUIRE (peak < 12.0f);
}

TEST_CASE ("ATMOS CORE freeze holds the tail without blowing up", "[core]")
{
    const double sr = 48000.0;
    AtmosCore core;
    core.prepare (sr, 64, 2);

    ParamSet ps;
    ps.set (PID::CoreMix, 1.0f).set (PID::CoreDecay, 3.0f).set (PID::CoreBloom, 0.0f);

    dsp::Random32 rng (3);
    const int feed = (int) (sr * 1.0), total = (int) (sr * 12.0);
    double eAfterFreeze = 0.0, eEnd = 0.0;
    bool finite = true;

    runModule (core, ps, sr, total,
               [&] (int n) { return n < feed ? 0.3f * rng.nextBipolar() : 0.0f; },
               [&] (int n, float l, float r)
               {
                   finite = finite && std::isfinite (l) && std::isfinite (r);
                   if (n >= (int) (sr * 1.5) && n < (int) (sr * 2.5)) eAfterFreeze += (double) l * l;
                   if (n >= total - (int) sr) eEnd += (double) l * l;
               },
               [&] (int n) { ps.set (PID::CoreFreeze, n >= feed ? 1.0f : 0.0f); });

    REQUIRE (finite);
    const double ratioDb = 10.0 * std::log10 (eEnd / eAfterFreeze);
    INFO ("energy change over 9.5 s of freeze: " << ratioDb << " dB");
    REQUIRE (ratioDb > -6.0);   // the tail is held...
    REQUIRE (ratioDb < 3.0);    // ...and does not grow
}

TEST_CASE ("ATMOS CORE: mix 0 is transparent, reset clears the tail", "[core]")
{
    const double sr = 48000.0;
    AtmosCore core;
    core.prepare (sr, 64, 2);

    ParamSet ps;
    ps.set (PID::CoreMix, 0.0f);

    dsp::Random32 rng (5);
    std::vector<float> in;
    float maxErr = 0.0f;
    runModule (core, ps, sr, 4800,
               [&] (int) { in.push_back (0.5f * rng.nextBipolar()); return in.back(); },
               [&] (int n, float l, float) { maxErr = std::max (maxErr, std::abs (l - in[(size_t) n])); });
    REQUIRE (maxErr < 1.0e-6f);

    // Fill with noise at full wet, reset, then silence must stay silent.
    ps.set (PID::CoreMix, 1.0f);
    runModule (core, ps, sr, 24000, [&] (int) { return 0.5f * rng.nextBipolar(); }, [] (int, float, float) {});
    core.reset();
    float peak = 0.0f;
    runModule (core, ps, sr, 4800, [] (int) { return 0.0f; },
               [&] (int, float l, float r) { peak = std::max ({ peak, std::abs (l), std::abs (r) }); });
    REQUIRE (peak == 0.0f);
}

TEST_CASE ("ATMOS CORE works at every common sample rate", "[core]")
{
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        AtmosCore core;
        core.prepare (sr, 64, 2);
        ParamSet ps;
        ps.set (PID::CoreMix, 1.0f).set (PID::CoreShimmer, 0.5f).set (PID::CoreColor, 4.0f);
        dsp::Random32 rng (11);
        bool finite = true;
        double energy = 0.0;
        runModule (core, ps, sr, (int) sr,
                   [&] (int) { return 0.3f * rng.nextBipolar(); },
                   [&] (int, float l, float r) { finite = finite && std::isfinite (l + r); energy += (double) l * l; });
        INFO ("sr " << sr);
        REQUIRE (finite);
        REQUIRE (energy > 0.0);
    }
}

TEST_CASE ("PitchShifter shifts a sine by an octave", "[core]")
{
    const double sr = 48000.0;
    dsp::PitchShifter<double> ps;
    ps.prepare (sr, 0.05);
    ps.setSemitones (12.0);

    // Count zero crossings of the output (after warm-up) versus the input.
    const double f = 300.0;
    int crossings = 0;
    double prev = 0.0;
    const int total = (int) sr;
    for (int n = 0; n < total; ++n)
    {
        const double y = ps.process (std::sin (2.0 * 3.141592653589793 * f * n / sr));
        if (n > 4800 && ((prev < 0.0) != (y < 0.0)))
            ++crossings;
        prev = y;
    }
    const double measured = crossings / 2.0 / ((total - 4800) / sr);
    INFO ("measured frequency " << measured);
    REQUIRE (measured > 2.0 * f * 0.9);
    REQUIRE (measured < 2.0 * f * 1.1);
}

TEST_CASE ("ATMOS CORE output level survey", "[.measure]")
{
    const double sr = 48000.0;
    for (int color = 0; color < 5; ++color)
        for (float decay : { 0.5f, 2.0f, 4.0f, 10.0f, 30.0f })
            for (float size : { 0.2f, 0.6f, 1.0f })
            {
                AtmosCore core;
                core.prepare (sr, 64, 2);
                ParamSet ps;
                ps.set (PID::CoreMix, 1.0f).set (PID::CoreDecay, decay).set (PID::CoreSize, size)
                  .set (PID::CoreColor, (float) color);
                dsp::Random32 rng (1);
                double ein = 0.0, eout = 0.0;
                const int total = (int) (sr * 8);
                runModule (core, ps, sr, total,
                           [&] (int n) { const float x = 0.2f * rng.nextBipolar(); if (n > total / 2) ein += (double) x * x; return x; },
                           [&] (int n, float l, float) { if (n > total / 2) eout += (double) l * l; });
                WARN ("color " << color << " decay " << decay << " size " << size
                      << "  wet/in = " << 10.0 * std::log10 (eout / ein) << " dB");
            }
}

TEST_CASE ("ATMOS CORE CPU benchmark", "[.bench]")
{
    const double sr = 48000.0;
    for (float shimmer : { 0.0f, 0.6f })
    {
        AtmosCore core;
        core.prepare (sr, 64, 2);
        ParamSet ps;
        ps.set (PID::CoreMix, 1.0f).set (PID::CoreShimmer, shimmer).set (PID::CoreShimBlend, 0.3f);
        dsp::Random32 rng (1);
        const int total = (int) (sr * 20);
        const auto t0 = juce::Time::getHighResolutionTicks();
        float sink = 0.0f;
        runModule (core, ps, sr, total, [&] (int) { return 0.2f * rng.nextBipolar(); },
                   [&] (int, float l, float) { sink += l; });
        const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
        WARN ("shimmer " << shimmer << ": " << 100.0 * secs / 20.0 << " % of one core (sink " << sink << ")");
    }
}
