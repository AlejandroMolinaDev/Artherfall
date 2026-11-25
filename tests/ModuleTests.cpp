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

namespace
{
    std::vector<float> runImpulse (ModuleType type, ParamSet& ps, int numSamples)
    {
        auto m = createModule (type);
        m->prepare (48000.0, 64, 2);
        std::vector<float> out;
        runModule (*m, ps, 48000.0, numSamples, [] (int n) { return n == 0 ? 1.0f : 0.0f; },
                   [&] (int, float l, float r) { out.push_back (0.5f * (l + r)); });
        return out;
    }

    /** Estimated frequency from zero crossings after a warm-up. */
    double zeroCrossingFrequency (const std::vector<float>& x, size_t from, double sr)
    {
        int crossings = 0;
        for (size_t i = from + 1; i < x.size(); ++i)
            if ((x[i - 1] < 0.0f) != (x[i] < 0.0f))
                ++crossings;
        return crossings / 2.0 / ((double) (x.size() - from) / sr);
    }
}

TEST_CASE ("Bitcrush: transparent at 16 bit / full rate, crunchy at low settings", "[modules]")
{
    auto m = createModule (ModuleType::Bitcrush);
    m->prepare (48000.0, 64, 2);

    auto errorFor = [&] (float bits, float rate)
    {
        ParamSet ps;
        ps.set (PID::CrushBits, bits).set (PID::CrushRate, rate).set (PID::CrushSmooth, 0.0f).set (PID::CrushJitter, 0.0f);
        m->reset();
        std::vector<float> in, out;
        runModule (*m, ps, 48000.0, 9600,
                   [&] (int n) { const float x = 0.5f * std::sin (0.0288f * (float) n); in.push_back (x); return x; },
                   [&] (int, float l, float) { out.push_back (l); });
        double err = 0.0;
        for (size_t i = 4800; i < in.size(); ++i)
            err = std::max (err, (double) std::abs (in[i] - out[i]));
        return err;
    };

    REQUIRE (errorFor (16.0f, 48000.0f) < 1.0e-4);
    REQUIRE (errorFor (4.0f, 48000.0f) > 0.02);
    REQUIRE (errorFor (16.0f, 2000.0f) > 0.02);
}

TEST_CASE ("Frequency shifter moves a sine by the shift amount", "[modules]")
{
    auto m = createModule (ModuleType::RingFreq);
    m->prepare (48000.0, 64, 2);
    ParamSet ps;
    ps.set (PID::RingMode, 1.0f).set (PID::RingShift, 200.0f).set (PID::RingFeedback, 0.0f).set (PID::RingStereo, 0.0f);

    std::vector<float> out;
    runModule (*m, ps, 48000.0, 48000, [] (int n) { return 0.5f * std::sin (2.0f * 3.14159265f * 1000.0f * (float) n / 48000.0f); },
               [&] (int, float l, float) { out.push_back (l); });

    const double f = zeroCrossingFrequency (out, 4800, 48000.0);
    INFO ("measured " << f << " Hz");
    REQUIRE (std::abs (f - 1200.0) < 15.0);
}

TEST_CASE ("Shimmer Delay: first echo on time", "[modules]")
{
    ParamSet ps;
    ps.set (PID::ShdlySync, 0.0f).set (PID::ShdlyTime, 250.0f).set (PID::ShdlyPingPong, 0.0f)
      .set (PID::ShdlyLPF, 20000.0f).set (PID::ShdlyHPF, 20.0f);
    const auto out = runImpulse (ModuleType::ShimmerDelay, ps, 24000);

    size_t peak = 0;
    for (size_t i = 0; i < out.size(); ++i)
        if (std::abs (out[i]) > std::abs (out[peak])) peak = i;
    INFO ("peak at " << peak);
    REQUIRE (std::abs ((int) peak - 12000) <= 3);
}

TEST_CASE ("Multi-Tap: taps land on the expected grid", "[modules]")
{
    ParamSet ps;
    ps.set (PID::MtapSync, 0.0f).set (PID::MtapTime, 400.0f).set (PID::MtapTaps, 4.0f).set (PID::MtapShape, 0.0f)
      .set (PID::MtapDiffusion, 0.0f).set (PID::MtapFeedback, 0.0f).set (PID::MtapTone, 20000.0f).set (PID::MtapWidth, 0.0f);
    const auto out = runImpulse (ModuleType::MultiTap, ps, 24000);

    for (int k = 1; k <= 4; ++k)
    {
        const int expected = k * 4800;   // 100 ms grid
        float local = 0.0f;
        for (int i = expected - 300; i < expected + 300; ++i)
            local = std::max (local, std::abs (out[(size_t) i]));
        INFO ("tap " << k);
        REQUIRE (local > 0.1f);
    }
    // Nothing between the taps.
    float between = 0.0f;
    for (int i = 6800; i < 8800; ++i)
        between = std::max (between, std::abs (out[(size_t) i]));
    REQUIRE (between < 0.02f);
}

TEST_CASE ("Comb rings at its tuned period", "[modules]")
{
    ParamSet ps;
    ps.set (PID::CombFreq, 200.0f).set (PID::CombFeedback, 0.9f).set (PID::CombDamp, 0.0f)
      .set (PID::CombSmear, 0.0f).set (PID::CombModDepth, 0.0f).set (PID::CombSpread, 0.0f);
    const auto out = runImpulse (ModuleType::Comb, ps, 4800);

    // Echoes every 240 samples (48000 / 200).
    for (int k = 1; k <= 4; ++k)
    {
        const int at = k * 240;
        float local = 0.0f;
        for (int i = at - 3; i <= at + 3; ++i)
            local = std::max (local, std::abs (out[(size_t) i]));
        INFO ("echo " << k);
        REQUIRE (local > 0.05f);
    }
}

TEST_CASE ("ModFX: every mode produces a signal", "[modules]")
{
    auto m = createModule (ModuleType::ModFX);
    m->prepare (48000.0, 64, 2);
    for (int mode = 0; mode < 3; ++mode)
    {
        ParamSet ps;
        ps.set (PID::MfxMode, (float) mode);
        m->reset();
        std::vector<float> out;
        runModule (*m, ps, 48000.0, 24000, [] (int n) { return 0.5f * std::sin (0.0288f * (float) n); },
                   [&] (int, float l, float) { out.push_back (l); });
        INFO ("mode " << mode);
        REQUIRE (rms (out, 12000) > 0.1);
    }
}

TEST_CASE ("Trance Gate follows the pattern in time with the host", "[modules]")
{
    auto gate = createModule (ModuleType::Gate);
    gate->prepare (48000.0, 64, 2);

    ParamSet ps;
    ps.set (PID::GateDiv, 2.0f).set (PID::GateSteps, 16.0f).set (PID::GateSwing, 0.0f)
      .set (PID::GateSmooth, 0.0f).set (PID::GateDepth, 1.0f);

    juce::AudioBuffer<float> buf (2, 64);
    ModContext ctx;
    ctx.values = ps.v.data();
    ctx.sampleRate = 48000.0;
    ctx.numSamples = 64;
    ctx.transport.bpm = 120.0;
    ctx.transport.isPlaying = true;
    ctx.transport.hasPpq = true;
    ctx.gatePattern = 0x5555u;   // steps 1, 3, 5... open

    // 1/16 at 120 BPM = 6000 samples per step.
    std::vector<float> out;
    for (int start = 0; start < 6000 * 4; start += 64)
    {
        ctx.transport.ppqPosition = (double) start / 48000.0 * 2.0;
        for (int i = 0; i < 64; ++i) { buf.setSample (0, i, 1.0f); buf.setSample (1, i, 1.0f); }
        gate->process (buf, ctx);
        for (int i = 0; i < 64; ++i) out.push_back (buf.getSample (0, i));
    }

    REQUIRE (out[3000] > 0.99f);    // step 1 open
    REQUIRE (out[9000] < 0.01f);    // step 2 closed
    REQUIRE (out[15000] > 0.99f);   // step 3 open
    REQUIRE (out[21000] < 0.01f);   // step 4 closed
}

TEST_CASE ("Reverse Swell plays audio backwards", "[modules]")
{
    ParamSet ps;
    ps.set (PID::RevSync, 0.0f).set (PID::RevLength, 200.0f).set (PID::RevXfade, 0.0f)
      .set (PID::RevFeedback, 0.0f).set (PID::RevMode, 0.0f);

    auto m = createModule (ModuleType::ReverseSwell);
    m->prepare (48000.0, 64, 2);
    std::vector<float> out;
    // Rising ramp: played backwards it falls.
    runModule (*m, ps, 48000.0, 48000, [] (int n) { return (float) n / 48000.0f; },
               [&] (int, float l, float) { out.push_back (l); });

    int falling = 0, total = 0;
    for (size_t i = 24000; i < out.size(); ++i)
    {
        const float d = out[i] - out[i - 1];
        if (std::abs (d) > 1.0e-7f) { ++total; falling += d < 0.0f ? 1 : 0; }
    }
    INFO ("falling fraction " << (double) falling / total);
    REQUIRE ((double) falling / total > 0.6);
}

TEST_CASE ("Texture Layer: adds noise, Follow gates it with the input", "[modules]")
{
    auto m = createModule (ModuleType::Texture);
    m->prepare (48000.0, 64, 2);

    for (int type = 0; type < 5; ++type)
    {
        ParamSet ps;
        ps.set (PID::TexType, (float) type).set (PID::TexLevel, -12.0f).set (PID::TexFollow, 0.0f);
        m->reset();
        std::vector<float> out;
        runModule (*m, ps, 48000.0, 48000, [] (int) { return 0.0f; }, [&] (int, float l, float) { out.push_back (l); });
        INFO ("type " << type);
        REQUIRE (rms (out, 24000) > 0.003);

        ps.set (PID::TexFollow, 1.0f);
        m->reset();
        out.clear();
        runModule (*m, ps, 48000.0, 48000, [] (int) { return 0.0f; }, [&] (int, float l, float) { out.push_back (l); });
        REQUIRE (rms (out, 24000) < 1.0e-4);
    }
}

TEST_CASE ("Lo-Fi Tape loses highs as Tone goes up", "[modules]")
{
    auto m = createModule (ModuleType::LoFi);
    m->prepare (48000.0, 64, 2);
    auto level = [&] (float tone)
    {
        ParamSet ps;
        ps.set (PID::LofiTone, tone).set (PID::LofiNoise, 0.0f).set (PID::LofiWobble, 0.0f)
          .set (PID::LofiSat, 0.0f).set (PID::LofiComp, 0.0f);
        m->reset();
        std::vector<float> out;
        runModule (*m, ps, 48000.0, 24000, [] (int n) { return 0.2f * std::sin (2.0f * 3.14159265f * 8000.0f * (float) n / 48000.0f); },
                   [&] (int, float l, float) { out.push_back (l); });
        return rms (out, 12000);
    };
    REQUIRE (level (1.0f) < level (0.0f) * 0.2);
}

TEST_CASE ("Resonator Bank rings after an impulse", "[modules]")
{
    ParamSet ps;
    ps.set (PID::ResoDecay, 2.0f).set (PID::ResoBright, 0.8f);
    const auto out = runImpulse (ModuleType::Resonator, ps, 48000);
    // With a 2 s ring time the level 0.5..1 s after the hit is only ~20 dB
    // below the first quarter second.
    std::vector<float> early (out.begin(), out.begin() + 12000);
    std::vector<float> late (out.begin() + 24000, out.end());
    INFO ("early " << rms (early) << " late " << rms (late));
    REQUIRE (rms (early) > 1.0e-4);
    REQUIRE (rms (late) > 0.05 * rms (early));
    float peak = 0.0f;
    for (float v : out) peak = std::max (peak, std::abs (v));
    REQUIRE (peak < 4.0f);
}
