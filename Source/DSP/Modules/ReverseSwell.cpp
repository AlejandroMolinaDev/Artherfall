#include "ReverseSwell.h"
#include "DSP/Utils/DspMath.h"

namespace aetherfall
{

using namespace dsp;

void ReverseSwell::prepareModule()
{
    for (auto& l : lines)
        l.prepare ((int) std::ceil (2.0 * maxSegmentSeconds * sampleRate) + 16);
    feedback.prepare (sampleRate, 0.05);
    sharpness.prepare (sampleRate, 0.05);
    reset();
}

void ReverseSwell::reset()
{
    for (auto& l : lines) l.reset();
    phase = 0.0;
    lastOut = { 0.0f, 0.0f };
    primed = false;
}

void ReverseSwell::process (juce::AudioBuffer<float>& buffer, const ModContext& ctx)
{
    const int n = std::min (ctx.numSamples, buffer.getNumSamples());
    if (n <= 0)
        return;

    segmentSeconds = clamp (ctx.timeSeconds (PID::RevLength, PID::RevSync, PID::RevDiv), 0.05, maxSegmentSeconds);
    // The segment length only changes at the start of a head cycle (no jumps mid-grain).
    const double targetSegment = segmentSeconds * sampleRate;
    const bool swell = ctx.getChoice (PID::RevMode) == 1;

    // Window exponent: Xfade 1 -> sin^2 (smooth, overlapping), 0 -> sin^8 (sharper chunks).
    const float sharp = 1.0f + 3.0f * (1.0f - clamp (ctx.get (PID::RevXfade), 0.0f, 1.0f));

    if (! primed)
    {
        segmentSamples = targetSegment;
        feedback.setCurrentAndTarget (ctx.get (PID::RevFeedback));
        sharpness.setCurrentAndTarget (sharp);
        primed = true;
    }
    feedback.setTarget (clamp (ctx.get (PID::RevFeedback), 0.0f, 0.9f));
    sharpness.setTarget (sharp);

    float* x[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };

    for (int i = 0; i < n; ++i)
    {
        const float fb = feedback.next();
        const float p = sharpness.next();

        for (int c = 0; c < 2; ++c)
            lines[(size_t) c].write (fastTanh (x[c][i] + fb * lastOut[(size_t) c]));

        // Each head: delay goes 0 -> 2*L over L samples (reverse at pitch 1).
        phase += 1.0 / segmentSamples;
        if (phase >= 1.0)
        {
            phase -= 1.0;
            segmentSamples = targetSegment;
        }

        const double phB = phase >= 0.5 ? phase - 0.5 : phase + 0.5;
        const double dA = 2.0 * phase * segmentSamples;
        const double dB = 2.0 * phB * segmentSamples;

        float wA = std::pow ((float) std::sin (pi * phase), 2.0f * p);
        float wB = std::pow ((float) std::sin (pi * phB), 2.0f * p);
        // Normalise with the plain windows (constant level for any sharpness)...
        const float norm = 1.0f / std::max (wA + wB, 1.0e-4f);
        if (swell)
        {
            // ...then add a rising envelope inside each reversed grain.
            wA *= (float) (phase * phase);
            wB *= (float) (phB * phB);
        }

        for (int c = 0; c < 2; ++c)
        {
            auto& l = lines[(size_t) c];
            const float a = l.tapLinear ((float) dA);
            const float b = l.tapLinear ((float) dB);
            float y = (wA * a + wB * b) * norm;
            if (swell)
                y *= 1.6f;   // the envelope removes energy: rough make-up
            lastOut[(size_t) c] = y;
            x[c][i] = y;
        }
    }
}

} // namespace aetherfall
