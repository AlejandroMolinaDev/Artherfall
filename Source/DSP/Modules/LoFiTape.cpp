#include "LoFiTape.h"
#include "DSP/Utils/DspMath.h"

namespace aetherfall
{

using namespace dsp;

void LoFiTape::prepareModule()
{
    for (auto& l : wobbleLine)
        l.prepare ((int) std::ceil (0.012 * sampleRate) + 8);
    for (auto& b : headBump)
        b.setPeak (85.0f, 0.9f, 2.5f, (float) sampleRate);
    compAtk = (float) std::exp (-1.0 / (0.008 * sampleRate));
    compRel = (float) std::exp (-1.0 / (0.25 * sampleRate));
    for (auto* s : { &sat, &comp, &wobble, &noise })
        s->prepare (sampleRate, 0.05);
    reset();
}

void LoFiTape::reset()
{
    for (auto& l : wobbleLine) l.reset();
    for (auto& f : hfLoss1) f.reset();
    for (auto& f : hfLoss2) f.reset();
    for (auto& b : headBump) b.reset();
    compEnv = 0.0f;
    wobPhase = 0.0;
    flutPhase = 0.3;
    lastTone = -1.0f;
    primed = false;
}

void LoFiTape::process (juce::AudioBuffer<float>& buffer, const ModContext& ctx)
{
    const int n = std::min (ctx.numSamples, buffer.getNumSamples());
    if (n <= 0)
        return;

    const float tone = clamp (ctx.get (PID::LofiTone), 0.0f, 1.0f);
    if (std::abs (tone - lastTone) > 1.0e-3f)
    {
        // Tone = amount of HF loss: 18 kHz (fresh) down to 2.5 kHz (worn), two poles.
        const float fc = 18000.0f * std::pow (2500.0f / 18000.0f, tone);
        for (int c = 0; c < 2; ++c)
        {
            hfLoss1[(size_t) c].setCutoff (fc, (float) sampleRate);
            hfLoss2[(size_t) c].setCutoff (fc * 1.3f, (float) sampleRate);
        }
        lastTone = tone;
    }

    if (! primed)
    {
        sat.setCurrentAndTarget (ctx.get (PID::LofiSat));
        comp.setCurrentAndTarget (ctx.get (PID::LofiComp));
        wobble.setCurrentAndTarget (ctx.get (PID::LofiWobble));
        noise.setCurrentAndTarget (ctx.get (PID::LofiNoise));
        primed = true;
    }
    sat.setTarget (clamp (ctx.get (PID::LofiSat), 0.0f, 1.0f));
    comp.setTarget (clamp (ctx.get (PID::LofiComp), 0.0f, 1.0f));
    wobble.setTarget (clamp (ctx.get (PID::LofiWobble), 0.0f, 1.0f));
    noise.setTarget (clamp (ctx.get (PID::LofiNoise), 0.0f, 1.0f));

    const double wobInc = clamp ((double) ctx.get (PID::LofiRate), 0.05, 10.0) / sampleRate;
    const double flutInc = 9.0 / sampleRate;
    const float baseDelay = (float) (0.006 * sampleRate);
    float* x[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };

    for (int i = 0; i < n; ++i)
    {
        const float s = sat.next(), cp = comp.next(), wb = wobble.next(), nz = noise.next();

        // --- Compressor (shared detector): threshold falls and ratio rises with "Comp".
        const float lvl = std::max (std::abs (x[0][i]), std::abs (x[1][i]));
        compEnv = lvl + (lvl > compEnv ? compAtk : compRel) * (compEnv - lvl);
        const float threshDb = -6.0f - 24.0f * cp;
        const float ratio = 1.0f + 5.0f * cp;
        const float envDb = gainToDb (compEnv + 1.0e-9f);
        const float overDb = std::max (0.0f, envDb - threshDb);
        const float grDb = -overDb * (1.0f - 1.0f / ratio);
        const float compGain = dbToGain (grDb + 12.0f * cp * 0.5f);   // partial make-up

        // --- Wobble: slow wow + fast flutter on a short delay.
        wobPhase += wobInc;   if (wobPhase >= 1.0) wobPhase -= 1.0;
        flutPhase += flutInc; if (flutPhase >= 1.0) flutPhase -= 1.0;
        const float mod = wb * (float) (0.0025 * sampleRate * std::sin (twoPi * wobPhase)
                                        + 0.0002 * sampleRate * std::sin (twoPi * flutPhase));

        const float drive = 1.0f + 6.0f * s;
        const float satNorm = 1.0f / std::pow (drive, 0.6f);
        const float hiss = nz * 0.02f;

        for (int c = 0; c < 2; ++c)
        {
            float v = x[c][i] * compGain;
            v = fastTanh (v * drive) * satNorm;

            wobbleLine[(size_t) c].write (v);
            v = wobbleLine[(size_t) c].tapCubic (std::max (1.5f, baseDelay + mod));

            v = hfLoss2[(size_t) c].processLP (hfLoss1[(size_t) c].processLP (v));
            v = headBump[(size_t) c].process (v);
            v += hiss * rng.nextBipolar();
            x[c][i] = v;
        }
    }
}

} // namespace aetherfall
