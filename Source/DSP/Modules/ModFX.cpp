#include "ModFX.h"
#include "DSP/Utils/DspMath.h"

namespace aetherfall
{

using namespace dsp;

void ModFX::prepareModule()
{
    for (auto& l : lines)
        l.prepare ((int) std::ceil (0.05 * sampleRate) + 8);   // 50 ms covers chorus + depth
    for (auto* s : { &depth, &feedback, &center, &width })
        s->prepare (sampleRate, 0.03);
    reset();
}

void ModFX::reset()
{
    for (auto& c : stages) for (auto& s : c) s.reset();
    for (auto& l : lines) l.reset();
    fbState = { 0.0f, 0.0f };
    lfo.reset (0.0);
    lastMode = -1;
    primed = false;
}

void ModFX::process (juce::AudioBuffer<float>& buffer, const ModContext& ctx)
{
    const int n = std::min (ctx.numSamples, buffer.getNumSamples());
    if (n <= 0)
        return;

    const int mode = ctx.getChoice (PID::MfxMode);
    const int numStages = clamp ((int) std::lround (ctx.get (PID::MfxStages)), 2, maxStages);
    const int numVoices = clamp (numStages / 2, 1, ctx.getChoice (PID::GlobQuality) == 0 ? 3 : maxVoices);

    if (mode != lastMode)
    {
        // Switching algorithm: clear the feedback paths to avoid a burst.
        fbState = { 0.0f, 0.0f };
        for (auto& c : stages) for (auto& s : c) s.reset();
        lastMode = mode;
    }

    if (! primed)
    {
        depth.setCurrentAndTarget (ctx.get (PID::MfxDepth));
        feedback.setCurrentAndTarget (ctx.get (PID::MfxFeedback));
        center.setCurrentAndTarget (ctx.get (PID::MfxCenter));
        width.setCurrentAndTarget (ctx.get (PID::MfxWidth));
        primed = true;
    }
    depth.setTarget (clamp (ctx.get (PID::MfxDepth), 0.0f, 1.0f));
    feedback.setTarget (clamp (ctx.get (PID::MfxFeedback), -0.95f, 0.95f));
    center.setTarget (clamp (ctx.get (PID::MfxCenter), 0.0f, 1.0f));
    width.setTarget (clamp (ctx.get (PID::MfxWidth), 0.0f, 1.0f));

    lfo.beginChunk (ctx, ctx.getBool (PID::MfxSync), ctx.getChoice (PID::MfxDiv), ctx.get (PID::MfxRate));

    const float sr = (float) sampleRate;
    float* x[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };

    for (int i = 0; i < n; ++i)
    {
        const double ph = lfo.next();
        const float d = depth.next(), fb = feedback.next(), ce = center.next(), w = width.next();
        const double phR = ph + 0.5 * (double) w;

        for (int c = 0; c < 2; ++c)
        {
            const double p = c == 0 ? ph : phR;
            const float lfoVal = sinCycles (p);
            const float in = x[c][i];
            float out = 0.0f;

            if (mode == 0)   // ---------------------------------------------- Phaser
            {
                // Centre 200 Hz..4 kHz, swept +- 2 octaves at full depth.
                // All-pass coefficient refreshed every 4 samples (tan is costly).
                if ((i & 3) == 0)
                {
                    const float fc = 200.0f * std::exp2 (4.32f * ce + 2.0f * d * lfoVal);
                    const float t = std::tan ((float) pi * clamp (fc, 20.0f, sr * 0.45f) / sr);
                    phaserCoeff[(size_t) c] = (t - 1.0f) / (t + 1.0f);
                }
                const float a = phaserCoeff[(size_t) c];

                float s = in + fb * fbState[(size_t) c];
                for (int k = 0; k < numStages; ++k)
                {
                    auto& ap = stages[(size_t) c][(size_t) k];
                    ap.setCoefficient (a);
                    s = ap.process (s);
                }
                fbState[(size_t) c] = flushDenormal (fastTanh (s));
                out = s;
            }
            else if (mode == 1)   // ------------------------------------------ Flanger
            {
                const float baseMs = 0.5f + 7.5f * ce;
                const float delayMs = baseMs * (1.0f + 0.9f * d * lfoVal);
                const float delay = std::max (1.0f, delayMs * 0.001f * sr);
                const float y = lines[(size_t) c].tapCubic (delay - 1.0f);
                lines[(size_t) c].write (fastTanh (in + fb * y));
                out = y;
            }
            else   // ------------------------------------------------------------- Chorus
            {
                lines[(size_t) c].write (in + 0.5f * fb * fbState[(size_t) c]);
                const float baseMs = 8.0f + 22.0f * ce;
                float sum = 0.0f;
                for (int v = 0; v < numVoices; ++v)
                {
                    const double vp = p + (double) v / numVoices;
                    const float lv = sinCycles (vp);
                    const float delayMs = baseMs + 6.0f * d * lv * (0.7f + 0.3f * (float) (v & 1));
                    // Odd voices lean to the opposite side for an ensemble spread.
                    const float pan = ((v & 1) == (c & 1)) ? 1.0f : 0.55f;
                    sum += pan * lines[(size_t) c].tapCubic (std::max (1.0f, delayMs * 0.001f * sr));
                }
                out = sum / std::sqrt ((float) numVoices);
                fbState[(size_t) c] = flushDenormal (out);
            }

            x[c][i] = out;
        }
    }
}

} // namespace aetherfall
