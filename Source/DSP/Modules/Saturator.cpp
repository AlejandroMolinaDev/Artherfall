#include "Saturator.h"
#include "DSP/Utils/DspMath.h"

namespace aetherfall
{

using namespace dsp;

void Saturator::prepareModule()
{
    oversampler.prepare (maxBlock, 2);
    for (auto& d : dc)
        d.prepare (sampleRate, 10.0);

    rmsCoeff = (float) std::exp (-1.0 / (0.3 * sampleRate));   // 300 ms RMS window
    gainSmoother.prepare (sampleRate, 0.05);
    reset();
}

void Saturator::reset()
{
    oversampler.reset();
    for (auto& d : dc) d.reset();
    for (auto& f : toneLow) f.reset();
    for (auto& f : toneHigh) f.reset();
    msIn = msOut = 0.0f;
    primed = false;
    lastTone = 1000.0f;
}

float Saturator::shape (int mode, float x) noexcept
{
    switch (mode)
    {
        case 0: // Tape: smooth symmetric soft clip (odd harmonics, gentle knee)
            return fastTanh (x);

        case 1: // Tube: asymmetric — positive half compresses harder than the negative one
            return x >= 0.0f ? fastTanh (x) : 0.9f * (std::exp (x * 1.1f) - 1.0f);

        case 2: // Diode: exponential knee, harder than tanh
            return std::copysign (1.0f - std::exp (-2.0f * std::abs (x)), x);

        case 3: // Fold: sine wavefolder — the signal folds back as the drive rises
            return std::sin (x * (float) halfPi);

        case 4: // Fuzz: near-hard clipping, asymmetric
            return x >= 0.0f ? fastTanh (3.0f * x) : 0.7f * fastTanh (2.0f * x);

        default:
            return x;
    }
}

void Saturator::process (juce::AudioBuffer<float>& buffer, const ModContext& ctx)
{
    const int n = std::min (ctx.numSamples, buffer.getNumSamples());
    if (n <= 0)
        return;

    const int mode = ctx.getChoice (PID::SatMode);
    oversampler.setFactorIndex (ctx.getChoice (PID::SatOversample));

    // Drive 0..1 -> 0..+36 dB, bias up to 0.5 of DC offset before the curve.
    const float drive = dbToGain (36.0f * clamp (ctx.get (PID::SatDrive), 0.0f, 1.0f));
    const float bias  = 0.5f * clamp (ctx.get (PID::SatBias), 0.0f, 1.0f);

    if (! primed)
    {
        prevDrive = drive;
        prevBias = bias;
    }

    // Input level for auto gain.
    for (int i = 0; i < n; ++i)
    {
        const float l = buffer.getSample (0, i), r = buffer.getSample (1, i);
        msIn = 0.5f * (l * l + r * r) + rmsCoeff * (msIn - 0.5f * (l * l + r * r));
    }

    // Waveshaping at the oversampled rate, with drive/bias ramped over the chunk.
    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), 2, (size_t) n);
    const float d0 = prevDrive, d1 = drive, b0 = prevBias, b1 = bias;

    oversampler.process (block, [mode, d0, d1, b0, b1] (juce::dsp::AudioBlock<float>& up)
    {
        const auto len = (int) up.getNumSamples();
        const float inv = len > 0 ? 1.0f / (float) len : 0.0f;

        for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
        {
            float* x = up.getChannelPointer (ch);
            for (int i = 0; i < len; ++i)
            {
                const float t = (float) (i + 1) * inv;
                const float g = d0 + (d1 - d0) * t;
                const float b = b0 + (b1 - b0) * t;
                // Subtract shape(b) so silence stays silent (no DC step on bias changes).
                x[i] = shape (mode, x[i] * g + b) - shape (mode, b);
            }
        }
    });

    prevDrive = drive;
    prevBias = bias;

    // Tone: tilt around 800 Hz (+-6 dB shelves in opposite directions).
    const float tone = clamp (ctx.get (PID::SatTone), -1.0f, 1.0f);
    if (std::abs (tone - lastTone) > 1.0e-3f)
    {
        for (int c = 0; c < 2; ++c)
        {
            toneLow[(size_t) c].setLowShelf (800.0f, 0.7f, -6.0f * tone, (float) sampleRate);
            toneHigh[(size_t) c].setHighShelf (800.0f, 0.7f, 6.0f * tone, (float) sampleRate);
        }
        lastTone = tone;
    }

    for (int c = 0; c < 2; ++c)
    {
        float* x = buffer.getWritePointer (c);
        for (int i = 0; i < n; ++i)
            x[i] = toneHigh[(size_t) c].process (toneLow[(size_t) c].process (dc[(size_t) c].process (x[i])));
    }

    // Output level for auto gain.
    for (int i = 0; i < n; ++i)
    {
        const float l = buffer.getSample (0, i), r = buffer.getSample (1, i);
        msOut = 0.5f * (l * l + r * r) + rmsCoeff * (msOut - 0.5f * (l * l + r * r));
    }

    float target = dbToGain (ctx.get (PID::SatOutput));
    if (ctx.getBool (PID::SatAutoGain) && msOut > 1.0e-10f && msIn > 1.0e-10f)
        target *= clamp (std::sqrt (msIn / msOut), dbToGain (-24.0f), dbToGain (12.0f));

    if (! primed)
    {
        gainSmoother.setCurrentAndTarget (target);
        primed = true;
    }
    gainSmoother.setTarget (target);

    for (int i = 0; i < n; ++i)
    {
        const float g = gainSmoother.next();
        buffer.setSample (0, i, buffer.getSample (0, i) * g);
        buffer.setSample (1, i, buffer.getSample (1, i) * g);
    }
}

} // namespace aetherfall
