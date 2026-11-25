#include "Bitcrush.h"
#include "DSP/Utils/DspMath.h"

namespace aetherfall
{

using namespace dsp;

void Bitcrush::prepareModule()
{
    bits.prepare (sampleRate, 0.03);
    smooth.prepare (sampleRate, 0.03);
    reset();
}

void Bitcrush::reset()
{
    for (auto& f : pre)  f.reset();
    for (auto& f : post) f.reset();
    hold = { 0.0f, 0.0f };
    phase = 1.0;   // sample immediately
    lastRate = -1.0f;
    primed = false;
}

void Bitcrush::process (juce::AudioBuffer<float>& buffer, const ModContext& ctx)
{
    const int n = std::min (ctx.numSamples, buffer.getNumSamples());
    if (n <= 0)
        return;

    const float rate = clamp (ctx.get (PID::CrushRate), 100.0f, (float) sampleRate);
    const float jitter = clamp (ctx.get (PID::CrushJitter), 0.0f, 1.0f);

    if (! primed)
    {
        bits.setCurrentAndTarget (ctx.get (PID::CrushBits));
        smooth.setCurrentAndTarget (ctx.get (PID::CrushSmooth));
        primed = true;
    }
    bits.setTarget (clamp (ctx.get (PID::CrushBits), 1.0f, 24.0f));
    smooth.setTarget (clamp (ctx.get (PID::CrushSmooth), 0.0f, 1.0f));

    if (std::abs (rate - lastRate) > 1.0f)
    {
        // Both filters sit just below the new Nyquist (rate / 2).
        const float fc = std::min (rate * 0.45f, (float) sampleRate * 0.45f);
        for (int c = 0; c < 2; ++c)
        {
            pre[(size_t) c].setParamsQ (fc, 0.7071f, (float) sampleRate);
            post[(size_t) c].setParamsQ (fc, 0.7071f, (float) sampleRate);
        }
        lastRate = rate;
    }

    const double baseInc = (double) rate / sampleRate;
    float* x[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };

    for (int i = 0; i < n; ++i)
    {
        const float b = bits.next();
        const float sm = smooth.next();
        const float levels = std::exp2 (b - 1.0f);   // fractional bit depths are fine

        // Clock: jitter randomises each period (+-50 % at full jitter).
        phase += baseInc * (1.0 + 0.5 * jitter * (double) rng.nextBipolar());
        const bool tick = phase >= 1.0;
        if (tick)
            phase -= std::floor (phase);

        for (int c = 0; c < 2; ++c)
        {
            const float in = x[c][i];
            const float filtered = pre[(size_t) c].processLP (in);
            if (tick)
            {
                const float s = in + sm * (filtered - in);
                hold[(size_t) c] = std::round (s * levels) / levels;
            }
            const float y = hold[(size_t) c];
            const float yf = post[(size_t) c].processLP (y);
            x[c][i] = y + sm * (yf - y);
        }
    }
}

} // namespace aetherfall
