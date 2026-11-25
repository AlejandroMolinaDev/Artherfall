#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <vector>
#include "TestHelpers.h"
#include "DSP/Modules/ModuleFactory.h"
#include "DSP/Modules/GranularCloud.h"

using namespace aetherfall;
using namespace aetherfall::test;

namespace
{
    /** Randomises every module-specific parameter within its range. */
    void randomiseModuleParams (ParamSet& ps, ModuleType t, dsp::Random32& rng)
    {
        const auto& mi = moduleInfo (t);
        for (int p = (int) mi.firstParam; p <= (int) mi.lastParam; ++p)
        {
            const auto& info = paramInfos[(size_t) p];
            const float lo = info.rangeMin(), hi = info.rangeMax();
            float v = lo + rng.nextFloat() * (hi - lo);
            if (info.kind != Kind::Float || info.interval >= 1.0f)
                v = std::round (v);
            ps.set (info.pid, v);
        }
    }

    double rms (const std::vector<float>& x, size_t from = 0)
    {
        double acc = 0.0;
        for (size_t i = from; i < x.size(); ++i)
            acc += (double) x[i] * x[i];
        return std::sqrt (acc / (double) std::max<size_t> (1, x.size() - from));
    }
}

TEST_CASE ("Every module survives random parameters and hostile signals", "[modules]")
{
    const int typeIndex = GENERATE (Catch::Generators::range (1, numModuleTypes + 1));
    const auto sig = GENERATE (Signal::Noise, Signal::Impulse, Signal::DC, Signal::Silence, Signal::Sine);
    const auto type = (ModuleType) typeIndex;

    auto module = createModule (type);
    REQUIRE (module->getType() == type);
    module->prepare (48000.0, 64, 2);

    ParamSet ps;
    dsp::Random32 rng ((std::uint32_t) (typeIndex * 7919 + (int) sig));
    dsp::Random32 sigRng (17);
    bool finite = true;
    float peak = 0.0f;

    runModule (*module, ps, 48000.0, 48000 * 3,
               [&] (int n) { return generate (sig, n, sigRng, 0.8f); },
               [&] (int, float l, float r)
               {
                   finite = finite && std::isfinite (l) && std::isfinite (r);
                   peak = std::max ({ peak, std::abs (l), std::abs (r) });
               },
               [&] (int n) { if (n % 12000 == 0) randomiseModuleParams (ps, type, rng); });

    INFO ("module " << std::string (moduleInfo (type).id) << " signal " << (int) sig << " peak " << peak);
    REQUIRE (finite);
    REQUIRE (peak < 20.0f);
}

TEST_CASE ("Saturator: auto gain keeps the level, silence stays silent", "[modules]")
{
    auto sat = createModule (ModuleType::Saturator);
    sat->prepare (48000.0, 64, 2);

    for (int mode = 0; mode < 5; ++mode)
    {
        ParamSet ps;
        ps.set (PID::SatMode, (float) mode).set (PID::SatDrive, 0.7f).set (PID::SatBias, 0.3f)
          .set (PID::SatAutoGain, 1.0f);
        sat->reset();

        std::vector<float> in, out;
        runModule (*sat, ps, 48000.0, 48000 * 2,
                   [&] (int n) { const float x = 0.3f * std::sin (0.0288f * (float) n); in.push_back (x); return x; },
                   [&] (int, float l, float) { out.push_back (l); });

        const double ratioDb = 20.0 * std::log10 (rms (out, 48000) / rms (in, 48000));
        INFO ("mode " << mode << " level change " << ratioDb << " dB");
        REQUIRE (std::abs (ratioDb) < 2.0);

        // Silence in (with bias) -> silence out (no DC, no noise).
        float peak = 0.0f;
        runModule (*sat, ps, 48000.0, 24000, [] (int) { return 0.0f; },
                   [&] (int n, float l, float) { if (n > 12000) peak = std::max (peak, std::abs (l)); });
        REQUIRE (peak < 1.0e-3f);
    }
}

TEST_CASE ("Filter Sweep: low-pass and high-pass attenuate correctly", "[modules]")
{
    auto filt = createModule (ModuleType::Filter);
    filt->prepare (48000.0, 64, 2);

    auto levelFor = [&] (int mode, int type, float freq)
    {
        ParamSet ps;
        ps.set (PID::FiltMode, (float) mode).set (PID::FiltType, (float) type).set (PID::FiltSlope, 1.0f)
          .set (PID::FiltCutoff, 500.0f).set (PID::FiltRes, 0.1f).set (PID::FiltDrive, 0.0f);
        filt->reset();
        std::vector<float> out;
        const float w = 2.0f * 3.14159265f * freq / 48000.0f;
        runModule (*filt, ps, 48000.0, 24000, [&] (int n) { return 0.25f * std::sin (w * (float) n); },
                   [&] (int, float l, float) { out.push_back (l); });
        return 20.0 * std::log10 (rms (out, 12000) / (0.25 / std::sqrt (2.0)));
    };

    for (int type = 0; type < 2; ++type)
    {
        INFO ("type " << type);
        REQUIRE (levelFor (0, type, 100.0f) > -4.0);    // LP passes lows
        REQUIRE (levelFor (0, type, 8000.0f) < -40.0);  // LP kills highs (24 dB/oct)
        REQUIRE (levelFor (1, type, 8000.0f) > -4.0);   // HP passes highs
        REQUIRE (levelFor (1, type, 50.0f) < -25.0);    // HP kills lows
    }
}

TEST_CASE ("Stereo Engine: width 0 is mono, bass mono keeps lows centred and flat", "[modules]")
{
    auto st = createModule (ModuleType::Stereo);
    st->prepare (48000.0, 64, 2);

    // Width 0, everything else neutral -> L == R.
    {
        ParamSet ps;
        ps.set (PID::StereoWidth, 0.0f).set (PID::StereoBassMono, 0.0f);
        juce::AudioBuffer<float> buf (2, 64);
        dsp::Random32 rng (3);
        ModContext ctx;
        ctx.values = ps.v.data();
        ctx.sampleRate = 48000.0;
        ctx.numSamples = 64;
        float maxDiff = 0.0f;
        for (int b = 0; b < 200; ++b)
        {
            for (int i = 0; i < 64; ++i)
            {
                buf.setSample (0, i, rng.nextBipolar());
                buf.setSample (1, i, rng.nextBipolar());
            }
            st->process (buf, ctx);
            if (b > 10)
                for (int i = 0; i < 64; ++i)
                    maxDiff = std::max (maxDiff, std::abs (buf.getSample (0, i) - buf.getSample (1, i)));
        }
        REQUIRE (maxDiff < 1.0e-5f);
    }

    // Bass mono at 150 Hz with a mono sine at the crossover: level preserved (LR4 is flat).
    {
        ParamSet ps;
        ps.set (PID::StereoWidth, 1.0f).set (PID::StereoBassMono, 150.0f);
        st->reset();
        std::vector<float> out;
        const float w = 2.0f * 3.14159265f * 150.0f / 48000.0f;
        runModule (*st, ps, 48000.0, 48000, [&] (int n) { return 0.5f * std::sin (w * (float) n); },
                   [&] (int, float l, float) { out.push_back (l); });
        const double db = 20.0 * std::log10 (rms (out, 24000) / (0.5 / std::sqrt (2.0)));
        INFO ("level at crossover " << db << " dB");
        REQUIRE (std::abs (db) < 0.5);
    }
}

TEST_CASE ("Tape Delay: first echo arrives at the set time", "[modules]")
{
    auto tape = createModule (ModuleType::TapeDelay);
    tape->prepare (48000.0, 64, 2);

    ParamSet ps;
    ps.set (PID::TapeSync, 0.0f).set (PID::TapeTime, 100.0f).set (PID::TapeWow, 0.0f)
      .set (PID::TapeFlutter, 0.0f).set (PID::TapeFeedback, 0.0f).set (PID::TapeSat, 0.0f)
      .set (PID::TapeAge, 0.0f);

    int peakIndex = -1;
    float peak = 0.0f;
    runModule (*tape, ps, 48000.0, 9600, [] (int n) { return n == 0 ? 1.0f : 0.0f; },
               [&] (int n, float l, float) { if (std::abs (l) > peak) { peak = std::abs (l); peakIndex = n; } });

    INFO ("echo at sample " << peakIndex);
    REQUIRE (std::abs (peakIndex - 4800) <= 3);
    REQUIRE (peak > 0.5f);
}

TEST_CASE ("Granular Cloud: makes a cloud, respects the voice limit, freeze scans the buffer", "[modules]")
{
    GranularCloud gran;
    gran.prepare (48000.0, 64, 2);

    ParamSet ps;
    ps.set (PID::GranDensity, 100.0f).set (PID::GranSize, 500.0f).set (PID::GranPosition, 0.05f);

    dsp::Random32 rng (8);
    int maxGrains = 0;
    std::vector<float> out;
    runModule (gran, ps, 48000.0, 48000 * 2, [&] (int) { return 0.4f * rng.nextBipolar(); },
               [&] (int, float l, float) { out.push_back (l); },
               [&] (int) { maxGrains = std::max (maxGrains, gran.getActiveGrainCount()); });

    REQUIRE (maxGrains <= GranularCloud::maxGrains);
    REQUIRE (maxGrains > 10);
    REQUIRE (rms (out, 48000) > 0.05);

    // Freeze, then silence: the cloud keeps playing the frozen buffer.
    ps.set (PID::GranFreeze, 1.0f);
    out.clear();
    runModule (gran, ps, 48000.0, 48000 * 2, [] (int) { return 0.0f; },
               [&] (int, float l, float) { out.push_back (l); });
    REQUIRE (rms (out, 48000) > 0.05);
}
