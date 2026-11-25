#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <vector>

#include "DSP/Utils/DspMath.h"
#include "DSP/Utils/Smoothers.h"
#include "DSP/Utils/DCBlocker.h"
#include "DSP/Utils/DelayLine.h"
#include "DSP/Utils/Filters.h"
#include "DSP/Utils/Oversampler.h"
#include "DSP/Utils/Random.h"
#include "Parameters/NoteDivision.h"

using namespace aetherfall;
using namespace aetherfall::dsp;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

TEST_CASE ("LinearSmoother reaches target exactly", "[utils]")
{
    LinearSmoother<float> s;
    s.prepare (48000.0, 0.01);       // 480 samples
    s.setCurrentAndTarget (0.0f);
    s.setTarget (1.0f);

    float v = 0.0f;
    for (int i = 0; i < 479; ++i)
    {
        const float prev = v;
        v = s.next();
        REQUIRE (v >= prev);          // monotonic
    }
    REQUIRE (s.isSmoothing());
    REQUIRE (s.next() == 1.0f);
    REQUIRE_FALSE (s.isSmoothing());

    s.setTarget (0.5f);
    s.skip (10000);
    REQUIRE (s.getCurrent() == 0.5f);
}

TEST_CASE ("OnePoleSmoother converges", "[utils]")
{
    OnePoleSmoother<double> s;
    s.prepare (48000.0, 0.01);
    s.setCurrentAndTarget (0.0);
    s.setTarget (1.0);
    for (int i = 0; i < 48000; ++i)
        s.next();
    REQUIRE_THAT (s.getCurrent(), WithinAbs (1.0, 1.0e-9));
}

TEST_CASE ("ControlRamp interpolates across a chunk", "[utils]")
{
    ControlRamp<float> r;
    r.reset (0.0f);
    r.beginChunk (1.0f, 4);
    REQUIRE_THAT (r.next(), WithinAbs (0.25f, 1.0e-6f));
    REQUIRE_THAT (r.next(), WithinAbs (0.5f, 1.0e-6f));
    REQUIRE_THAT (r.next(), WithinAbs (0.75f, 1.0e-6f));
    REQUIRE_THAT (r.next(), WithinAbs (1.0f, 1.0e-6f));
}

TEST_CASE ("DCBlocker removes DC offset", "[utils]")
{
    DCBlocker<float> dc;
    dc.prepare (48000.0);

    float y = 0.0f;
    for (int i = 0; i < 48000 * 2; ++i)
        y = dc.process (0.5f);

    REQUIRE (std::abs (y) < 1.0e-3f);
}

TEST_CASE ("DelayLine integer and fractional taps", "[utils]")
{
    DelayLine<float> d;
    d.prepare (100);

    for (int i = 0; i < 50; ++i)
        d.write ((float) i);

    REQUIRE (d.tap (0) == 49.0f);
    REQUIRE (d.tap (10) == 39.0f);
    REQUIRE_THAT (d.tapLinear (10.5f), WithinAbs (38.5f, 1.0e-5f));
    REQUIRE_THAT (d.tapCubic (10.25f), WithinAbs (38.75f, 1.0e-4f)); // exact on a ramp

    // Feedback convention: read(D-1) before writing gives a D-sample delay.
    DelayLine<float> fb;
    fb.prepare (64);
    std::vector<float> out;
    for (int n = 0; n < 20; ++n)
    {
        const float y = fb.tap (7 - 1);
        fb.write (n == 0 ? 1.0f : 0.0f);
        out.push_back (y);
    }
    REQUIRE (out[7] == 1.0f);
    for (int n = 0; n < 20; ++n)
        if (n != 7)
            REQUIRE (out[(size_t) n] == 0.0f);
}

TEST_CASE ("DelayLine cubic interpolation of a sine is accurate", "[utils]")
{
    DelayLine<double> d;
    d.prepare (1000);
    const double w = 2.0 * pi * 440.0 / 48000.0;
    for (int n = 0; n < 900; ++n)
        d.write (std::sin (w * n));

    const double delay = 123.37;
    const double expected = std::sin (w * (899 - delay));
    REQUIRE_THAT (d.tapCubic (delay), WithinAbs (expected, 1.0e-4));
}

TEST_CASE ("SVF stays stable under heavy modulation", "[utils]")
{
    SVF<float> f;
    Random32 rng (1234);
    float maxAbs = 0.0f;

    for (int n = 0; n < 48000; ++n)
    {
        if ((n & 31) == 0)
            f.setParams (20.0f + rng.nextFloat() * 19000.0f, rng.nextFloat() * 0.99f, 48000.0f);
        const float y = f.processLP (rng.nextBipolar());
        REQUIRE (std::isfinite (y));
        maxAbs = std::max (maxAbs, std::abs (y));
    }
    REQUIRE (maxAbs < 100.0f);
}

TEST_CASE ("Biquad low-pass attenuates high frequencies", "[utils]")
{
    auto rmsAfter = [] (double freq)
    {
        Biquad<double> bq;
        bq.setLowPass (1000.0, 0.707, 48000.0);
        double acc = 0.0;
        for (int n = 0; n < 48000; ++n)
        {
            const double y = bq.process (std::sin (2.0 * pi * freq * n / 48000.0));
            if (n > 4800) acc += y * y;
        }
        return std::sqrt (acc / (48000 - 4801));
    };

    REQUIRE (rmsAfter (100.0) > 0.65);    // passband (~0.707 rms)
    REQUIRE (rmsAfter (10000.0) < 0.01);  // stopband
}

TEST_CASE ("Oversampler round trip keeps the signal finite and close", "[utils]")
{
    Oversampler os;
    os.prepare (256, 2);

    for (int factor = 0; factor < 3; ++factor)
    {
        os.setFactorIndex (factor);
        REQUIRE (os.getFactor() == (1 << factor));

        juce::AudioBuffer<float> buf (2, 256);
        double energyIn = 0.0, energyOut = 0.0;

        for (int block = 0; block < 40; ++block)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 256; ++i)
                {
                    const float x = 0.5f * std::sin (2.0f * (float) pi * 220.0f * (float) (block * 256 + i) / 48000.0f);
                    buf.setSample (ch, i, x);
                    if (block > 4) energyIn += x * x;
                }

            juce::dsp::AudioBlock<float> blk (buf);
            os.process (blk, [] (juce::dsp::AudioBlock<float>&) {});

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 256; ++i)
                {
                    const float y = buf.getSample (ch, i);
                    REQUIRE (std::isfinite (y));
                    if (block > 4) energyOut += y * y;
                }
        }

        REQUIRE_THAT (energyOut / energyIn, WithinRel (1.0, 0.05));
    }
}

TEST_CASE ("Tempo divisions", "[utils]")
{
    REQUIRE_THAT (notediv::toSeconds (notediv::idx1_4, 120.0), WithinAbs (0.5, 1.0e-12));
    REQUIRE_THAT (notediv::toSeconds (notediv::idx1_8D, 120.0), WithinAbs (0.375, 1.0e-12));
    REQUIRE_THAT (notediv::toSeconds (notediv::idx1Bar, 120.0), WithinAbs (2.0, 1.0e-12));
    REQUIRE_THAT (notediv::toSeconds (notediv::idx1Bar, 120.0, 3, 4), WithinAbs (1.5, 1.0e-12));
    REQUIRE_THAT (notediv::toSeconds (7, 120.0), WithinAbs (0.5 * 2.0 / 3.0 * 0.5, 1.0e-12)); // 1/8T
}

TEST_CASE ("Math helpers", "[utils]")
{
    REQUIRE_THAT (dbToGain (-6.0206), WithinRel (0.5, 1.0e-4));
    REQUIRE_THAT (gainToDb (2.0), WithinRel (6.0206, 1.0e-4));
    REQUIRE_THAT (semitonesToRatio (12.0), WithinRel (2.0, 1.0e-12));
    REQUIRE_THAT (noteToHz (69.0), WithinRel (440.0, 1.0e-12));
    REQUIRE_THAT (hzToNote (220.0), WithinAbs (57.0, 1.0e-9));
    REQUIRE (sanitize (std::numeric_limits<float>::quiet_NaN()) == 0.0f);
    REQUIRE (sanitize (std::numeric_limits<float>::infinity()) == 0.0f);
    REQUIRE (std::abs (fastTanh (10.0f)) <= 1.0f);
    for (float x = -4.0f; x <= 4.0f; x += 0.01f)
        REQUIRE_THAT (fastTanh (x), WithinAbs (std::tanh (x), 1.0e-4f));
    REQUIRE (std::abs (softSat (100.0f)) <= 1.0f);
}
