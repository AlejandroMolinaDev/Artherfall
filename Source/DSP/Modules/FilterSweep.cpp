#include "FilterSweep.h"
#include "DSP/Utils/DspMath.h"

namespace aetherfall
{

using namespace dsp;

namespace
{
    // Formant frequencies (Hz) and levels for the vowels A, E, I, O, U.
    constexpr float vowelF[5][3] = {
        { 800.0f, 1150.0f, 2900.0f },  // A
        { 480.0f, 1720.0f, 2520.0f },  // E
        { 300.0f, 2100.0f, 2900.0f },  // I
        { 450.0f,  800.0f, 2830.0f },  // O
        { 325.0f,  700.0f, 2530.0f },  // U
    };
    constexpr float formantGain[3] = { 1.0f, 0.63f, 0.4f };

    enum Mode { LP = 0, HP, BP, Notch, Formant };
}

//==============================================================================
void FilterSweep::Ladder::setCutoff (float hz, float sr) noexcept
{
    g = std::tan ((float) pi * clamp (hz, 10.0f, sr * 0.45f) / sr);
    G = g / (1.0f + g);
}

void FilterSweep::Ladder::process (float x, float k, float& u, std::array<float, 4>& y) noexcept
{
    // Each one-pole TPT stage: y = G*x + s/(1+g). Cascade -> y4 = G^4*u + S.
    const float inv1g = 1.0f / (1.0f + g);
    const float S1 = s[0] * inv1g, S2 = s[1] * inv1g, S3 = s[2] * inv1g, S4 = s[3] * inv1g;
    const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;
    const float S = G3 * S1 + G2 * S2 + G * S3 + S4;

    // Solve the zero-delay feedback loop, then saturate the loop input: the
    // tanh keeps self-oscillation bounded and adds the classic ladder grit.
    u = fastTanh ((x - k * S) / (1.0f + k * G4));

    float in = u;
    for (size_t i = 0; i < 4; ++i)
    {
        const float v = (in - s[i]) * G;
        const float out = v + s[i];
        s[i] = flushDenormal (out + v);
        y[i] = out;
        in = out;
    }
}

//==============================================================================
void FilterSweep::prepareModule()
{
    smoothCoeff = (float) std::exp (-1.0 / (0.008 * sampleRate));   // 8 ms cutoff glide
    reset();
}

void FilterSweep::reset()
{
    for (auto& c : ch)
    {
        c.svf1.reset();
        c.svf2.reset();
        c.ladder.reset();
        for (auto& f : c.formant) f.reset();
    }
    primed = false;
}

void FilterSweep::updateCoefficients (Channel& c, float hz, int mode, bool steep, bool ladder, float res, float vowel) noexcept
{
    const float sr = (float) sampleRate;

    if (mode == Formant)
    {
        // Vowel morph A-E-I-O-U; the cutoff knob shifts all formants together.
        const float pos = clamp (vowel, 0.0f, 1.0f) * 4.0f;
        const int i0 = std::min (3, (int) pos);
        const float t = pos - (float) i0;
        const float shift = clamp (std::pow (hz / 1000.0f, 0.6f), 0.5f, 2.0f);
        const float q = 6.0f + 14.0f * res;

        for (int f = 0; f < 3; ++f)
        {
            const float fr = lerp (vowelF[i0][f], vowelF[i0 + 1][f], t) * shift;
            c.formant[(size_t) f].setParamsQ (fr, q, sr);
        }
        return;
    }

    if (ladder)
    {
        c.ladder.setCutoff (hz, sr);
        return;
    }

    if (steep)
    {
        c.svf1.setParamsQ (hz, 0.54f, sr);   // first stage: no peak
        c.svf2.setParams (hz, res, sr);      // second stage carries the resonance
    }
    else
    {
        c.svf1.setParams (hz, res, sr);
    }
}

float FilterSweep::processSample (Channel& c, float x, int mode, bool steep, bool ladder, float res, float k) noexcept
{
    if (mode == Formant)
    {
        float y = 0.0f;
        for (int f = 0; f < 3; ++f)
            y += formantGain[f] * c.formant[(size_t) f].processBP (x);
        return y * 2.0f;
    }

    if (ladder)
    {
        float u = 0.0f;
        std::array<float, 4> y {};
        c.ladder.process (x, k, u, y);

        // Resonance thins the passband of a ladder; compensate partially.
        const float comp = 1.0f + 0.5f * k;

        switch (mode)
        {
            case LP:    return comp * (steep ? y[3] : y[1]);
            case HP:    return steep ? (u - 4.0f * y[0] + 6.0f * y[1] - 4.0f * y[2] + y[3])
                                     : (u - 2.0f * y[0] + y[1]);
            case BP:    return comp * (steep ? 4.0f * (y[1] - 2.0f * y[2] + y[3])
                                             : 2.0f * (y[0] - y[1]));
            case Notch: return steep ? (u - 4.0f * y[0] + 6.0f * y[1] - 4.0f * y[2] + 2.0f * y[3])
                                     : (u - 2.0f * y[0] + 2.0f * y[1]);
            default:    return x;
        }
    }

    juce::ignoreUnused (res);

    if (steep)
    {
        switch (mode)
        {
            case LP:    return c.svf2.processLP (c.svf1.processLP (x));
            case HP:    return c.svf2.processHP (c.svf1.processHP (x));
            case BP:    return c.svf2.processBP (c.svf1.processBP (x)) * 2.0f;
            case Notch: return c.svf2.processNotch (c.svf1.processNotch (x));
            default:    return x;
        }
    }

    switch (mode)
    {
        case LP:    return c.svf1.processLP (x);
        case HP:    return c.svf1.processHP (x);
        case BP:    return c.svf1.processBP (x);
        case Notch: return c.svf1.processNotch (x);
        default:    return x;
    }
}

void FilterSweep::process (juce::AudioBuffer<float>& buffer, const ModContext& ctx)
{
    const int n = std::min (ctx.numSamples, buffer.getNumSamples());
    if (n <= 0)
        return;

    const int mode     = ctx.getChoice (PID::FiltMode);
    const bool steep   = ctx.getChoice (PID::FiltSlope) == 1;
    const bool ladder  = ctx.getChoice (PID::FiltType) == 1;
    const float res    = clamp (ctx.get (PID::FiltRes), 0.0f, 1.0f);
    const float drive  = clamp (ctx.get (PID::FiltDrive), 0.0f, 1.0f);
    const float vowel  = ctx.get (PID::FiltVowel);
    const float stereo = clamp (ctx.get (PID::FiltStereo), 0.0f, 1.0f);
    const float cutoff = clamp (ctx.get (PID::FiltCutoff), 20.0f, 20000.0f);

    // Ladder feedback: k = 4 is the self-oscillation threshold.
    const float k = 4.15f * res;

    // Pre-drive: tanh with level compensation so small signals keep their level.
    const float pre = 1.0f + 11.0f * drive * drive;
    const float postComp = 1.0f / std::pow (pre, 0.7f);

    // Stereo: +- half an octave between the channels at full "Stereo".
    const float targetLog[2] = { std::log2 (cutoff) - 0.5f * stereo, std::log2 (cutoff) + 0.5f * stereo };

    if (! primed)
    {
        for (int c = 0; c < 2; ++c)
        {
            ch[(size_t) c].logCutoff = targetLog[c];
            updateCoefficients (ch[(size_t) c], std::exp2 (targetLog[c]), mode, steep, ladder, res, vowel);
        }
        primed = true;
    }

    for (int c = 0; c < 2; ++c)
    {
        auto& chan = ch[(size_t) c];
        float* x = buffer.getWritePointer (c);
        const float tgt = targetLog[c];

        for (int i = 0; i < n; ++i)
        {
            // Glide the cutoff in the log domain; recompute coefficients every
            // 4 samples while it moves (and once per chunk otherwise).
            const float before = chan.logCutoff;
            chan.logCutoff = tgt + smoothCoeff * (chan.logCutoff - tgt);
            if (i == 0 || ((i & 3) == 0 && std::abs (chan.logCutoff - before) > 1.0e-5f))
                updateCoefficients (chan, std::exp2 (chan.logCutoff), mode, steep, ladder, res, vowel);

            float in = x[i];
            if (drive > 1.0e-3f)
                in = fastTanh (in * pre) * postComp;

            float y = processSample (chan, in, mode, steep, ladder, res, k);
            // Keep self-oscillation and extreme resonance bounded.
            y = std::abs (y) > 1.5f ? std::copysign (1.5f + fastTanh (std::abs (y) - 1.5f), y) : y;
            x[i] = sanitize (y);
        }
    }
}

} // namespace aetherfall
