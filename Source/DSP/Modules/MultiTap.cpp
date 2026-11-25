#include "MultiTap.h"
#include "DSP/Utils/DspMath.h"

namespace aetherfall
{

using namespace dsp;

void MultiTap::prepareModule()
{
    line.prepare ((int) std::ceil ((maxDelaySeconds + 0.05) * sampleRate));

    for (int k = 0; k < maxTaps; ++k)
    {
        // Later taps get longer all-passes (more smear).
        const double base = 0.004 + 0.003 * k;
        diffusers[(size_t) k].prepare (sampleRate, { base, base * 1.618 }, 1.0);
        diffuserDelay[(size_t) k] = base * 2.618 * sampleRate;
        tapGain[(size_t) k].prepare (sampleRate, 0.03);
    }

    feedback.prepare (sampleRate, 0.03);
    width.prepare (sampleRate, 0.03);
    dc.prepare (sampleRate, 8.0);
    glide = std::exp (-1.0 / (0.1 * sampleRate));
    reset();
}

void MultiTap::reset()
{
    line.reset();
    for (auto& d : diffusers) d.reset();
    for (auto& t : tone) t.reset();
    fbTone.reset();
    dc.reset();
    lastFeedbackSample = 0.0f;
    lastTone = -1.0f;
    primed = false;
}

void MultiTap::process (juce::AudioBuffer<float>& buffer, const ModContext& ctx)
{
    const int n = std::min (ctx.numSamples, buffer.getNumSamples());
    if (n <= 0)
        return;

    const double span = clamp (ctx.timeSeconds (PID::MtapTime, PID::MtapSync, PID::MtapDiv), 0.02, maxDelaySeconds) * sampleRate;
    const int taps = clamp ((int) std::lround (ctx.get (PID::MtapTaps)), 2, maxTaps);
    const float shape = clamp (ctx.get (PID::MtapShape), -1.0f, 1.0f);
    const float diffusion = clamp (ctx.get (PID::MtapDiffusion), 0.0f, 1.0f);
    const float fbTarget = clamp (ctx.get (PID::MtapFeedback), 0.0f, 0.95f);

    // Tap k at span * u^e, u = (k+1)/taps; e > 1 bunches the taps early,
    // e < 1 bunches them late.
    const float e = std::exp2 (1.5f * shape);
    std::array<double, maxTaps> target {};
    for (int k = 0; k < maxTaps; ++k)
    {
        const double u = (double) std::min (k + 1, taps) / (double) taps;
        // The diffuser adds its own delay: read earlier so taps stay on the grid.
        target[(size_t) k] = std::max (2.0, span * std::pow (u, (double) e) - diffuserDelay[(size_t) k]);
        // Taps fade slightly with index; unused taps are muted.
        tapGain[(size_t) k].setTarget (k < taps ? 1.0f - 0.45f * (float) k / (float) maxTaps : 0.0f);
        diffusers[(size_t) k].setGain (0.72f * diffusion * (0.25f + 0.75f * (float) k / (float) (maxTaps - 1)));
    }

    if (! primed)
    {
        tapDelay = target;
        for (int k = 0; k < maxTaps; ++k)
            tapGain[(size_t) k].setCurrentAndTarget (tapGain[(size_t) k].getTarget());
        feedback.setCurrentAndTarget (fbTarget);
        width.setCurrentAndTarget (ctx.get (PID::MtapWidth));
        primed = true;
    }
    feedback.setTarget (fbTarget);
    width.setTarget (clamp (ctx.get (PID::MtapWidth), 0.0f, 1.0f));

    const float toneHz = ctx.get (PID::MtapTone);
    if (std::abs (toneHz - lastTone) > 1.0f)
    {
        for (auto& t : tone) t.setCutoff (toneHz, (float) sampleRate);
        fbTone.setCutoff (toneHz, (float) sampleRate);
        lastTone = toneHz;
    }

    tail = std::min (30.0, span / sampleRate * (1.0 + std::log (0.001) / std::log (std::max (0.05f, fbTarget))));

    const float norm = 1.0f / std::sqrt ((float) taps) * 1.3f;
    float* x[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };

    for (int i = 0; i < n; ++i)
    {
        const float in = 0.5f * (x[0][i] + x[1][i]);
        const float fb = feedback.next();
        const float w = width.next();

        float outL = 0.0f, outR = 0.0f, lastTap = 0.0f;

        for (int k = 0; k < maxTaps; ++k)
        {
            auto& d = tapDelay[(size_t) k];
            d = target[(size_t) k] + glide * (d - target[(size_t) k]);

            const float g = tapGain[(size_t) k].next();
            if (g <= 1.0e-5f && k >= taps)
                continue;

            float s = diffusers[(size_t) k].process (line.tapLinear ((float) d - 1.0f)) * g;
            if (k == taps - 1)
                lastTap = s;

            // Alternate sides; "Width" = how far each tap is panned.
            const float pan = ((k & 1) ? 1.0f : -1.0f) * w * (0.5f + 0.5f * (float) k / (float) maxTaps);
            outL += s * (1.0f - pan) * 0.5f * 1.41421356f;
            outR += s * (1.0f + pan) * 0.5f * 1.41421356f;
        }

        const float fbSig = dc.process (fbTone.processLP (lastTap));
        line.write (fastTanh (in + fb * lastFeedbackSample));
        lastFeedbackSample = fbSig;

        x[0][i] = tone[0].processLP (outL * norm);
        x[1][i] = tone[1].processLP (outR * norm);
    }
}

} // namespace aetherfall
