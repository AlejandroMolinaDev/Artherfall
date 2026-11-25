#include "ShimmerDelay.h"
#include "DSP/Utils/DspMath.h"

namespace aetherfall
{

using namespace dsp;

void ShimmerDelay::prepareModule()
{
    for (auto& c : ch)
    {
        c.line.prepare ((int) std::ceil ((maxDelaySeconds + 0.05) * sampleRate));
        c.shifter.prepare (sampleRate, 0.07);
        c.dc.prepare (sampleRate, 8.0);
    }
    feedback.prepare (sampleRate, 0.03);
    shimmer.prepare (sampleRate, 0.03);
    glide = std::exp (-1.0 / (0.12 * sampleRate));
    reset();
}

void ShimmerDelay::reset()
{
    for (auto& c : ch)
    {
        c.line.reset();
        c.shifter.reset();
        c.lp.reset();
        c.hp.reset();
        c.dc.reset();
        c.fbOut = 0.0f;
    }
    lastLP = lastHP = -1.0f;
    lastPitch = 1000.0f;
    primed = false;
}

void ShimmerDelay::process (juce::AudioBuffer<float>& buffer, const ModContext& ctx)
{
    const int n = std::min (ctx.numSamples, buffer.getNumSamples());
    if (n <= 0)
        return;

    const double secs = clamp (ctx.timeSeconds (PID::ShdlyTime, PID::ShdlySync, PID::ShdlyDiv), 0.01, maxDelaySeconds);
    const double target = secs * sampleRate;
    const bool pingPong = ctx.getBool (PID::ShdlyPingPong);
    const float fbTarget = clamp (ctx.get (PID::ShdlyFeedback), 0.0f, 0.98f);

    if (! primed)
    {
        for (auto& c : ch) c.delay = target;
        feedback.setCurrentAndTarget (fbTarget);
        shimmer.setCurrentAndTarget (ctx.get (PID::ShdlyShimmer));
        primed = true;
    }
    feedback.setTarget (fbTarget);
    shimmer.setTarget (clamp (ctx.get (PID::ShdlyShimmer), 0.0f, 1.0f));

    const float lpHz = ctx.get (PID::ShdlyLPF), hpHz = ctx.get (PID::ShdlyHPF);
    if (std::abs (lpHz - lastLP) > 1.0f || std::abs (hpHz - lastHP) > 0.5f)
    {
        for (auto& c : ch)
        {
            c.lp.setCutoff (lpHz, (float) sampleRate);
            c.hp.setCutoff (hpHz, (float) sampleRate);
        }
        lastLP = lpHz;
        lastHP = hpHz;
    }

    const float pitch = std::round (ctx.get (PID::ShdlyPitch));
    if (std::abs (pitch - lastPitch) > 0.01f)
    {
        for (auto& c : ch) c.shifter.setSemitones (pitch);
        lastPitch = pitch;
    }

    tail = std::min (30.0, secs * std::log (0.001) / std::log (std::max (0.05f, fbTarget)) + secs);

    float* x[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };

    for (int i = 0; i < n; ++i)
    {
        const float fb = feedback.next();
        const float sh = shimmer.next();
        float y[2];

        for (int c = 0; c < 2; ++c)
        {
            auto& chan = ch[(size_t) c];
            chan.delay = target + glide * (chan.delay - target);
            y[c] = chan.line.tapCubic ((float) std::max (1.0, chan.delay - 1.0));

            // Loop: filters -> pitch shift (crossfaded by "shimmer").
            float f = chan.hp.processHP (chan.lp.processLP (y[c]));
            const float shifted = chan.shifter.process (f);
            f += sh * (shifted - f);
            chan.fbOut = chan.dc.process (f);
        }

        if (pingPong)
        {
            // Mono input enters the left line; each line feeds the other one.
            const float in = 0.5f * (x[0][i] + x[1][i]);
            ch[0].line.write (fastTanh (in + fb * ch[1].fbOut));
            ch[1].line.write (fastTanh (fb * ch[0].fbOut));
        }
        else
        {
            for (int c = 0; c < 2; ++c)
                ch[(size_t) c].line.write (fastTanh (x[c][i] + fb * ch[(size_t) c].fbOut));
        }

        x[0][i] = y[0];
        x[1][i] = y[1];
    }
}

} // namespace aetherfall
