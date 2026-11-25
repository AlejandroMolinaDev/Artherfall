#include "InputStage.h"
#include "DSP/Utils/DspMath.h"

namespace aetherfall
{

using namespace dsp;

void InputStage::prepareModule()
{
    for (auto* s : { &gain, &hpfAmt, &lpfAmt, &monoAmt, &driveAmt })
        s->prepare (sampleRate, 0.03);
    reset();
}

void InputStage::reset()
{
    for (auto& f : hpf) f.reset();
    for (auto& f : lpf) f.reset();
    for (auto& c : xLow)  for (auto& b : c) b.reset();
    for (auto& c : xHigh) for (auto& b : c) b.reset();
    hpfHz = lpfHz = xoverHz = 0.0f;
    primed = false;
}

void InputStage::process (juce::AudioBuffer<float>& buffer, const ModContext& ctx)
{
    const int n = std::min (ctx.numSamples, buffer.getNumSamples());
    if (n <= 0)
        return;

    const float sr = (float) sampleRate;
    const float tHpf = ctx.get (PID::InHPF), tLpf = ctx.get (PID::InLPF), tMono = ctx.get (PID::InBassMono);
    const bool hpfOn = tHpf > 20.5f, lpfOn = tLpf < 19900.0f, monoOn = tMono >= 20.0f;
    const float drive = clamp (ctx.get (PID::InDrive), 0.0f, 1.0f);

    if (! primed)
    {
        gain.setCurrentAndTarget (dbToGain (ctx.get (PID::InGain)));
        hpfAmt.setCurrentAndTarget (hpfOn ? 1.0f : 0.0f);
        lpfAmt.setCurrentAndTarget (lpfOn ? 1.0f : 0.0f);
        monoAmt.setCurrentAndTarget (monoOn ? 1.0f : 0.0f);
        driveAmt.setCurrentAndTarget (drive);
        primed = true;
    }
    gain.setTarget (dbToGain (ctx.get (PID::InGain)));
    hpfAmt.setTarget (hpfOn ? 1.0f : 0.0f);
    lpfAmt.setTarget (lpfOn ? 1.0f : 0.0f);
    monoAmt.setTarget (monoOn ? 1.0f : 0.0f);
    driveAmt.setTarget (drive);

    if (std::abs (tHpf - hpfHz) > 0.05f)
    {
        hpfHz = tHpf;
        for (auto& f : hpf) f.setParamsQ (std::max (20.0f, tHpf), 0.7071f, sr);
    }
    if (std::abs (tLpf - lpfHz) > 1.0f)
    {
        lpfHz = tLpf;
        for (auto& f : lpf) f.setParamsQ (tLpf, 0.7071f, sr);
    }
    const float xo = monoOn ? tMono : 120.0f;
    if (std::abs (xo - xoverHz) > 0.5f)
    {
        xoverHz = xo;
        for (int c = 0; c < 2; ++c)
            for (int s = 0; s < 2; ++s)
            {
                xLow[(size_t) c][(size_t) s].setLowPass (xo, 0.7071f, sr);
                xHigh[(size_t) c][(size_t) s].setHighPass (xo, 0.7071f, sr);
            }
    }

    float* x[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };

    for (int i = 0; i < n; ++i)
    {
        const float g = gain.next(), ha = hpfAmt.next(), la = lpfAmt.next(), ma = monoAmt.next(), da = driveAmt.next();
        float v[2] = { x[0][i] * g, x[1][i] * g };

        for (int c = 0; c < 2; ++c)
        {
            const float h = hpf[(size_t) c].processHP (v[c]);
            v[c] += ha * (h - v[c]);
            const float l = lpf[(size_t) c].processLP (v[c]);
            v[c] += la * (l - v[c]);
        }

        // Bass mono: LR4 split, lows summed to mono (LR4 low + high = all-pass).
        float low[2], high[2];
        for (int c = 0; c < 2; ++c)
        {
            low[c]  = xLow[(size_t) c][1].process (xLow[(size_t) c][0].process (v[c]));
            high[c] = xHigh[(size_t) c][1].process (xHigh[(size_t) c][0].process (v[c]));
        }
        const float lowM = 0.5f * (low[0] + low[1]);
        for (int c = 0; c < 2; ++c)
            v[c] += ma * (lowM + high[c] - v[c]);

        // Pre-drive: soft saturation with level compensation, faded in by amount.
        if (da > 1.0e-4f)
        {
            const float pre = 1.0f + 9.0f * da * da;
            const float comp = 1.0f / std::sqrt (pre);
            for (int c = 0; c < 2; ++c)
            {
                const float sat = fastTanh (v[c] * pre) * comp;
                v[c] += std::min (1.0f, da * 4.0f) * (sat - v[c]);
            }
        }

        x[0][i] = v[0];
        x[1][i] = v[1];
    }
}

} // namespace aetherfall
