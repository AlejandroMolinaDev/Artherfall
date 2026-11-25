#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <vector>
#include "TestHelpers.h"
#include "DSP/Core/InputStage.h"
#include "DSP/Core/OutputStage.h"
#include "PluginProcessor.h"

using namespace aetherfall;
using namespace aetherfall::test;

namespace
{
    /** Runs the output stage with separate wet and dry generators. */
    void runOutput (OutputStage& os, ParamSet& ps, int numSamples,
                    const std::function<float (int, int)>& wetGen,   // (n, channel)
                    const std::function<float (int, int)>& dryGen,
                    const std::function<void (int, float, float)>& onOut,
                    Meters* meters = nullptr)
    {
        juce::AudioBuffer<float> wet (2, 64);
        std::array<float, 64> dl {}, dr {};
        ModContext ctx;
        ctx.values = ps.v.data();
        ctx.sampleRate = 48000.0;
        for (int start = 0; start < numSamples; start += 64)
        {
            const int len = std::min (64, numSamples - start);
            ctx.numSamples = len;
            for (int i = 0; i < len; ++i)
            {
                wet.setSample (0, i, wetGen (start + i, 0));
                wet.setSample (1, i, wetGen (start + i, 1));
                dl[(size_t) i] = dryGen (start + i, 0);
                dr[(size_t) i] = dryGen (start + i, 1);
            }
            juce::AudioBuffer<float> view (wet.getArrayOfWritePointers(), 2, 0, len);
            os.process (view, dl.data(), dr.data(), ctx, meters);
            for (int i = 0; i < len; ++i)
                onOut (start + i, view.getSample (0, i), view.getSample (1, i));
        }
    }

    float sine (int n, float hz, float amp = 0.5f)
    {
        return amp * std::sin (2.0f * 3.14159265f * hz * (float) n / 48000.0f);
    }

    double rmsRange (const std::vector<float>& x, size_t a, size_t b)
    {
        double acc = 0.0;
        for (size_t i = a; i < b; ++i) acc += (double) x[i] * x[i];
        return std::sqrt (acc / (double) (b - a));
    }
}

TEST_CASE ("Ducker pushes the wet down while the dry input plays", "[stages]")
{
    OutputStage os;
    os.prepare (48000.0, 64, 2);

    ParamSet ps;
    ps.set (PID::OutMix, 1.0f)                 // wet only at the output
      .set (PID::OutDuck, 1.0f).set (PID::OutDuckThresh, -30.0f)
      .set (PID::OutDuckAttack, 5.0f).set (PID::OutDuckRelease, 200.0f)
      .set (PID::OutClipOn, 0.0f).set (PID::OutLimitOn, 0.0f);

    dsp::Random32 rng (2);
    std::vector<float> out;
    // Dry: loud noise for 1 s, then silence. Wet: constant sine.
    runOutput (os, ps, 96000,
               [] (int n, int) { return sine (n, 300.0f, 0.3f); },
               [&] (int n, int) { return n < 48000 ? 0.5f * rng.nextBipolar() : 0.0f; },
               [&] (int, float l, float) { out.push_back (l); });

    const double during = rmsRange (out, 24000, 48000);
    const double after  = rmsRange (out, 84000, 96000);
    const double depthDb = 20.0 * std::log10 (after / during);
    INFO ("ducking depth " << depthDb << " dB");
    REQUIRE (depthDb > 6.0);                                          // clearly audible
    REQUIRE (std::abs (20.0 * std::log10 (after / (0.3 / std::sqrt (2.0)))) < 0.5);   // fully recovers
}

TEST_CASE ("Safety limiter never exceeds the ceiling", "[stages]")
{
    OutputStage os;
    os.prepare (48000.0, 64, 2);

    ParamSet ps;
    ps.set (PID::OutMix, 1.0f).set (PID::OutLimitOn, 1.0f).set (PID::OutCeiling, -1.0f)
      .set (PID::OutGain, 12.0f).set (PID::OutClipOn, 0.0f);

    dsp::Random32 rng (9);
    const float ceiling = dsp::dbToGain (-1.0f);
    float peak = 0.0f;
    Meters meters;
    runOutput (os, ps, 48000,
               [&] (int, int) { return 2.0f * rng.nextBipolar(); },
               [] (int, int) { return 0.0f; },
               [&] (int, float l, float r) { peak = std::max ({ peak, std::abs (l), std::abs (r) }); },
               &meters);
    INFO ("peak " << peak);
    REQUIRE (peak <= ceiling + 1.0e-6f);
    REQUIRE (meters.limiterGainReductionDb.load() < -3.0f);
}

TEST_CASE ("Glue compressor reduces loud material", "[stages]")
{
    OutputStage os;
    os.prepare (48000.0, 64, 2);
    ParamSet ps;
    ps.set (PID::OutMix, 1.0f).set (PID::OutGlueOn, 1.0f).set (PID::OutGlueThresh, -30.0f)
      .set (PID::OutGlueRatio, 4.0f).set (PID::OutClipOn, 0.0f).set (PID::OutLimitOn, 0.0f);

    std::vector<float> out;
    Meters meters;
    runOutput (os, ps, 48000, [] (int n, int) { return sine (n, 200.0f, 0.5f); }, [] (int, int) { return 0.0f; },
               [&] (int, float l, float) { out.push_back (l); }, &meters);
    const double db = 20.0 * std::log10 (rmsRange (out, 24000, 48000) / (0.5 / std::sqrt (2.0)));
    INFO ("level change " << db << " dB, GR meter " << meters.glueGainReductionDb.load());
    REQUIRE (db < -10.0);
    REQUIRE (meters.glueGainReductionDb.load() < -10.0f);
}

TEST_CASE ("Output tilt, filters and width", "[stages]")
{
    auto levelAt = [] (ParamSet ps, float hz)
    {
        OutputStage os;
        os.prepare (48000.0, 64, 2);
        ps.set (PID::OutMix, 1.0f).set (PID::OutClipOn, 0.0f).set (PID::OutLimitOn, 0.0f);
        std::vector<float> out;
        runOutput (os, ps, 24000, [hz] (int n, int) { return sine (n, hz, 0.25f); }, [] (int, int) { return 0.0f; },
                   [&] (int, float l, float) { out.push_back (l); });
        return 20.0 * std::log10 (rmsRange (out, 12000, 24000) / (0.25 / std::sqrt (2.0)));
    };

    ParamSet bright;
    bright.set (PID::OutTilt, 6.0f);
    REQUIRE (levelAt (bright, 8000.0f) - levelAt (bright, 100.0f) > 4.0);

    ParamSet hp;
    hp.set (PID::OutHPF, 500.0f);
    REQUIRE (levelAt (hp, 60.0f) < -20.0);
    REQUIRE (levelAt (hp, 5000.0f) > -1.0);

    ParamSet lp;
    lp.set (PID::OutLPF, 2000.0f);
    REQUIRE (levelAt (lp, 12000.0f) < -20.0);

    // Width 0 -> mono.
    OutputStage os;
    os.prepare (48000.0, 64, 2);
    ParamSet ps;
    ps.set (PID::OutMix, 1.0f).set (PID::OutWidth, 0.0f);
    float maxDiff = 0.0f;
    runOutput (os, ps, 9600, [] (int n, int c) { return c == 0 ? sine (n, 300.0f) : sine (n, 470.0f); },
               [] (int, int) { return 0.0f; },
               [&] (int n, float l, float r) { if (n > 4800) maxDiff = std::max (maxDiff, std::abs (l - r)); });
    REQUIRE (maxDiff < 1.0e-5f);
}

TEST_CASE ("Input stage: high-pass, bass mono and gain", "[stages]")
{
    auto run = [] (ParamSet ps, const std::function<float (int, int)>& gen, std::vector<float>& outL, std::vector<float>& outR)
    {
        InputStage is;
        is.prepare (48000.0, 64, 2);
        juce::AudioBuffer<float> buf (2, 64);
        ModContext ctx;
        ctx.values = ps.v.data();
        ctx.sampleRate = 48000.0;
        ctx.numSamples = 64;
        for (int start = 0; start < 48000; start += 64)
        {
            for (int i = 0; i < 64; ++i) { buf.setSample (0, i, gen (start + i, 0)); buf.setSample (1, i, gen (start + i, 1)); }
            is.process (buf, ctx);
            for (int i = 0; i < 64; ++i) { outL.push_back (buf.getSample (0, i)); outR.push_back (buf.getSample (1, i)); }
        }
    };

    {
        ParamSet ps;
        ps.set (PID::InHPF, 500.0f);
        std::vector<float> l, r;
        run (ps, [] (int n, int) { return sine (n, 60.0f); }, l, r);
        REQUIRE (20.0 * std::log10 (rmsRange (l, 24000, 48000) / (0.5 / std::sqrt (2.0))) < -20.0);
    }
    {
        // Anti-phase 50 Hz (pure side signal) disappears with bass mono at 150 Hz.
        ParamSet ps;
        ps.set (PID::InBassMono, 150.0f);
        std::vector<float> l, r;
        run (ps, [] (int n, int c) { return c == 0 ? sine (n, 50.0f) : -sine (n, 50.0f); }, l, r);
        REQUIRE (rmsRange (l, 24000, 48000) < 0.05);
    }
    {
        ParamSet ps;
        ps.set (PID::InGain, -6.0206f);
        std::vector<float> l, r;
        run (ps, [] (int n, int) { return sine (n, 1000.0f); }, l, r);
        REQUIRE_THAT (rmsRange (l, 24000, 48000), Catch::Matchers::WithinRel (0.25 / std::sqrt (2.0), 0.01));
    }
}

TEST_CASE ("Processor output never exceeds the ceiling with the limiter on", "[stages][processor]")
{
    AetherfallProcessor p;
    p.setPlayConfigDetails (2, 2, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);
    auto setParam = [&] (PID pid, float v)
    {
        auto* param = p.getValueTreeState().getParameter (juce::String (std::string (info (pid).id)));
        param->setValueNotifyingHost (param->convertTo0to1 (v));
    };
    setParam (PID::OutGain, 12.0f);
    setParam (PID::OutMix, 0.7f);

    dsp::Random32 rng (5);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    float peak = 0.0f;
    for (int b = 0; b < 200; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                buf.setSample (ch, i, 0.9f * rng.nextBipolar());
        p.processBlock (buf, midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                peak = std::max (peak, std::abs (buf.getSample (ch, i)));
    }
    REQUIRE (peak <= dsp::dbToGain (-0.3f) + 1.0e-5f);
    REQUIRE (p.getMeters().outPeakL.load() > 0.0f);
}
