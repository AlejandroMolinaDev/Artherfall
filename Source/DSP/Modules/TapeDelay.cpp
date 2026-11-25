#include "TapeDelay.h"
#include "DSP/Utils/DspMath.h"

namespace aetherfall
{

using namespace dsp;

void TapeDelay::prepareModule()
{
    const int maxSamples = (int) std::ceil ((maxDelaySeconds + 0.05) * sampleRate);
    for (auto& c : ch)
    {
        c.line.prepare (maxSamples);
        c.dc.prepare (sampleRate, 8.0);
    }

    glideCoeff = std::exp (-1.0 / (0.25 * sampleRate));   // 250 ms tape glide
    for (auto* s : { &feedback, &wow, &flutter, &sat })
        s->prepare (sampleRate, 0.03);

    reset();
}

void TapeDelay::reset()
{
    for (auto& c : ch)
    {
        c.line.reset();
        c.ageLP.reset();
        c.ageHP.reset();
        c.dc.reset();
    }
    wowPhase = 0.0;
    flutterPhase = 0.25;
    flutterPhase2 = 0.6;
    wowDrift = wowDriftTarget = 0.0;
    driftCounter = 0;
    lastAge = -1.0f;
    primed = false;
}

void TapeDelay::process (juce::AudioBuffer<float>& buffer, const ModContext& ctx)
{
    const int n = std::min (ctx.numSamples, buffer.getNumSamples());
    if (n <= 0)
        return;

    const double sr = sampleRate;
    const double timeSec = clamp (ctx.timeSeconds (PID::TapeTime, PID::TapeSync, PID::TapeDiv), 0.005, maxDelaySeconds);
    const double spread = clamp ((double) ctx.get (PID::TapeSpread), 0.0, 1.0);

    // Right channel runs slightly longer for width.
    const double targetSamples[2] = { timeSec * sr, std::min (timeSec * (1.0 + 0.12 * spread), maxDelaySeconds) * sr };

    const float fb = clamp (ctx.get (PID::TapeFeedback), 0.0f, 1.05f);
    const float age = clamp (ctx.get (PID::TapeAge), 0.0f, 1.0f);

    if (! primed)
    {
        for (int c = 0; c < 2; ++c)
            ch[(size_t) c].delaySamples = targetSamples[c];
        feedback.setCurrentAndTarget (fb);
        wow.setCurrentAndTarget (ctx.get (PID::TapeWow));
        flutter.setCurrentAndTarget (ctx.get (PID::TapeFlutter));
        sat.setCurrentAndTarget (ctx.get (PID::TapeSat));
        primed = true;
    }

    feedback.setTarget (fb);
    wow.setTarget (ctx.get (PID::TapeWow));
    flutter.setTarget (ctx.get (PID::TapeFlutter));
    sat.setTarget (ctx.get (PID::TapeSat));

    // "Age": each pass through the loop loses highs and lows.
    if (std::abs (age - lastAge) > 1.0e-4f)
    {
        for (auto& c : ch)
        {
            c.ageLP.setCutoff (16000.0f * (1.0f - 0.85f * age), (float) sr);
            c.ageHP.setCutoff (30.0f + 220.0f * age, (float) sr);
        }
        lastAge = age;
    }

    // Tail estimate: time for the repeats to fall by 60 dB.
    tail = fb >= 0.999f ? 30.0 : std::min (30.0, timeSec * std::log (0.001) / std::log (std::max (0.01f, fb)) + timeSec);

    const double wowInc = 0.55 / sr;           // ~0.55 Hz wow
    const double flutInc = 7.3 / sr;           // flutter components
    const double flutInc2 = 11.7 / sr;

    float* io[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };

    for (int i = 0; i < n; ++i)
    {
        // Wow drifts randomly (target re-drawn every ~0.25 s) on top of a slow sine.
        if (--driftCounter <= 0)
        {
            wowDriftTarget = rng.nextBipolar();
            driftCounter = (int) (0.25 * sr);
        }
        wowDrift += 0.00005 * (wowDriftTarget - wowDrift);

        wowPhase += wowInc;           if (wowPhase >= 1.0) wowPhase -= 1.0;
        flutterPhase += flutInc;      if (flutterPhase >= 1.0) flutterPhase -= 1.0;
        flutterPhase2 += flutInc2;    if (flutterPhase2 >= 1.0) flutterPhase2 -= 1.0;

        const double wowAmt  = (double) wow.next() * 0.004 * sr;      // up to 4 ms
        const double flutAmt = (double) flutter.next() * 0.0004 * sr; // up to 0.4 ms
        const double mod = wowAmt * (0.6 * std::sin (twoPi * wowPhase) + 0.4 * wowDrift)
                         + flutAmt * (0.7 * std::sin (twoPi * flutterPhase) + 0.3 * std::sin (twoPi * flutterPhase2));

        const float g = feedback.next();
        const float s = sat.next();
        const float drive = 1.0f + 4.0f * s;
        const float invDrive = 1.0f / drive;

        for (int c = 0; c < 2; ++c)
        {
            auto& chan = ch[(size_t) c];
            chan.delaySamples = targetSamples[c] + glideCoeff * (chan.delaySamples - targetSamples[c]);

            // Opposite modulation polarity per channel: subtle stereo wobble.
            const double d = std::max (2.0, chan.delaySamples + (c == 0 ? mod : -0.8 * mod) + wowAmt);
            const float y = chan.line.tapCubic ((float) (d - 1.0));

            // Repeat path: age filters, then the record head saturates.
            float rep = chan.ageHP.processHP (chan.ageLP.processLP (y));
            rep = chan.dc.process (rep);

            const float rec = io[c][i] + g * rep;
            chan.line.write (fastTanh (rec * drive) * invDrive);

            io[c][i] = y;
        }
    }
}

} // namespace aetherfall
