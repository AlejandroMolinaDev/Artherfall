#include "StereoEngine.h"
#include "DSP/Utils/DspMath.h"

namespace aetherfall
{

using namespace dsp;

void StereoEngine::prepareModule()
{
    haasHi.prepare ((int) std::ceil (0.035 * sampleRate) + 4);
    haasFull.prepare ((int) std::ceil (0.035 * sampleRate) + 4);
    for (auto* s : { &width, &haas, &panDepth, &orbit, &bassMonoAmt })
        s->prepare (sampleRate, 0.03);
    crossoverHz = 0.0f;
    reset();
}

void StereoEngine::reset()
{
    for (auto& c : lowpass)  for (auto& b : c) b.reset();
    for (auto& c : highpass) for (auto& b : c) b.reset();
    haasHi.reset();
    haasFull.reset();
    lfo.reset (0.0);
    primed = false;
}

void StereoEngine::updateCrossover (float hz)
{
    crossoverHz = hz;
    const float sr = (float) sampleRate;
    for (int c = 0; c < 2; ++c)
        for (int s = 0; s < 2; ++s)
        {
            lowpass[(size_t) c][(size_t) s].setLowPass (hz, 0.7071f, sr);
            highpass[(size_t) c][(size_t) s].setHighPass (hz, 0.7071f, sr);
        }
}

void StereoEngine::process (juce::AudioBuffer<float>& buffer, const ModContext& ctx)
{
    const int n = std::min (ctx.numSamples, buffer.getNumSamples());
    if (n <= 0)
        return;

    const float bassHz = ctx.get (PID::StereoBassMono);
    const bool bassOn = bassHz >= 20.0f;

    const float tWidth = clamp (ctx.get (PID::StereoWidth), 0.0f, 2.0f);
    const float tHaas  = clamp (ctx.get (PID::StereoHaas), 0.0f, 30.0f) * 0.001f * (float) sampleRate;
    const float tPan   = clamp (ctx.get (PID::StereoAutoPan), 0.0f, 1.0f);
    const float tOrbit = clamp (ctx.get (PID::StereoOrbit), 0.0f, 1.0f);

    if (! primed)
    {
        width.setCurrentAndTarget (tWidth);
        haas.setCurrentAndTarget (tHaas);
        panDepth.setCurrentAndTarget (tPan);
        orbit.setCurrentAndTarget (tOrbit);
        bassMonoAmt.setCurrentAndTarget (bassOn ? 1.0f : 0.0f);
        primed = true;
    }

    width.setTarget (tWidth);
    haas.setTarget (tHaas);
    panDepth.setTarget (tPan);
    orbit.setTarget (tOrbit);
    bassMonoAmt.setTarget (bassOn ? 1.0f : 0.0f);

    if (bassOn && std::abs (bassHz - crossoverHz) > 0.5f)
        updateCrossover (bassHz);
    else if (crossoverHz <= 0.0f)
        updateCrossover (120.0f);

    lfo.beginChunk (ctx, ctx.getBool (PID::StereoSync), ctx.getChoice (PID::StereoDiv),
                    ctx.get (PID::StereoRate));

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    for (int i = 0; i < n; ++i)
    {
        const float l = L[i], r = R[i];

        // Per-sample control values (computed once, shared by both paths).
        const float w  = width.next();
        const float h  = haas.next();
        const float pd = panDepth.next();
        const float ob = orbit.next();
        const float bm = bassMonoAmt.next();
        const double ph = lfo.next();
        const float sinP = sinCycles (ph);
        const float cosP = cosCycles (ph);

        // Width, Haas (right channel delayed up to 30 ms), auto-pan, orbit.
        auto spatialise = [&] (float& a, float& b, DelayLine<float>& haasLine)
        {
            const float m = 0.5f * (a + b);
            const float s = 0.5f * (a - b) * w;
            a = m + s;
            b = m - s;

            haasLine.write (b);
            if (h > 0.01f)
                b = haasLine.tapLinear (h);

            if (pd > 1.0e-4f)   // equal-power balance, unity at the centre
            {
                const float theta = (float) (pi * 0.25) * (1.0f + pd * sinP);
                a *= 1.41421356f * std::cos (theta);
                b *= 1.41421356f * std::sin (theta);
            }

            if (ob > 1.0e-4f)   // rotate the (L, R) vector, up to +-90 degrees
            {
                const float ang = ob * (float) halfPi * cosP;
                const float c = std::cos (ang), s2 = std::sin (ang);
                const float na = a * c - b * s2;
                b = a * s2 + b * c;
                a = na;
            }
        };

        // Path 1: full band (used when Bass Mono is off).
        float fl = l, fr = r;
        spatialise (fl, fr, haasFull);

        // Path 2: LR4 split; highs spatialised, lows summed to mono.
        // (LR4 low + high = all-pass of the input, so the split is transparent.)
        const float lowL = lowpass[0][1].process (lowpass[0][0].process (l));
        const float lowR = lowpass[1][1].process (lowpass[1][0].process (r));
        float hiL = highpass[0][1].process (highpass[0][0].process (l));
        float hiR = highpass[1][1].process (highpass[1][0].process (r));
        spatialise (hiL, hiR, haasHi);
        const float lowM = 0.5f * (lowL + lowR);

        L[i] = fl + bm * (lowM + hiL - fl);
        R[i] = fr + bm * (lowM + hiR - fr);
    }
}

} // namespace aetherfall
