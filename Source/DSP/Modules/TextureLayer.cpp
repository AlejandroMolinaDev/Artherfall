#include "TextureLayer.h"
#include "DSP/Utils/DspMath.h"

namespace aetherfall
{

using namespace dsp;

void TextureLayer::prepareModule()
{
    envAtk = (float) std::exp (-1.0 / (0.01 * sampleRate));
    envRel = (float) std::exp (-1.0 / (0.3 * sampleRate));
    for (auto* s : { &level, &follow, &width })
        s->prepare (sampleRate, 0.05);
    for (auto& v : voices)
        v.clickLP.setCutoff (3500.0f, (float) sampleRate);
    reset();
}

void TextureLayer::reset()
{
    for (auto& v : voices)
    {
        v.pink = {};
        v.band.reset();
        v.clickLP.reset();
        v.click = v.drop = 0.0f;
    }
    for (auto& f : toneLP) f.reset();
    for (auto& f : toneHP) f.reset();
    humPhase = 0.0;
    windDrift = windTarget = gust = gustTarget = 0.0;
    driftCounter = 0;
    env = 0.0f;
    lastTone = -1.0f;
    lastType = -1;
    primed = false;
}

float TextureLayer::generate (Voice& v, int type, float density) noexcept
{
    const float white = rng.nextBipolar();
    const float sr = (float) sampleRate;

    switch (type)
    {
        case 0: // Vinyl: crackle + surface noise
        {
            // Crackle events: rate 2..200 per second.
            const float rate = 2.0f + 198.0f * density * density;
            if (rng.nextFloat() < rate / sr)
            {
                v.click = 0.3f + 0.7f * rng.nextFloat();
                v.clickSign = rng.nextFloat() < 0.5f ? -1.0f : 1.0f;
            }
            const float c = v.clickLP.processLP (v.click * v.clickSign * (0.5f + 0.5f * white));
            v.click *= 0.93f;   // ~0.3 ms clicks
            return 1.6f * c + 0.08f * v.pink.process (white);
        }
        case 1: // Tape hiss
            return 0.6f * (white - 0.7f * v.pink.process (white));

        case 2: // Wind: drifting resonant band-pass, gusty amplitude
        {
            const float fc = 300.0f + 900.0f * (float) (0.5 + 0.5 * windDrift);
            v.band.setParams (fc, 0.82f, sr);
            const float g = (float) (0.35 + 0.65 * gust);
            return 2.2f * g * v.band.processBP (v.pink.process (white));
        }
        case 3: // Rain: droplets over a soft bed
        {
            const float rate = 30.0f + 1500.0f * density;
            if (rng.nextFloat() < rate / sr)
                v.drop = 0.2f + 0.8f * rng.nextFloat();
            const float d = v.drop * white;
            v.drop *= 0.985f;   // ~1.5 ms droplets
            return 0.9f * (d - v.clickLP.processLP (d)) + 0.12f * v.pink.process (white);
        }
        case 4: // Hum: 50 Hz + harmonics + buzz (shared phase, added on the common voice)
        default:
            return 0.0f;
    }
}

void TextureLayer::process (juce::AudioBuffer<float>& buffer, const ModContext& ctx)
{
    const int n = std::min (ctx.numSamples, buffer.getNumSamples());
    if (n <= 0)
        return;

    const int type = clamp (ctx.getChoice (PID::TexType), 0, 4);
    const float density = clamp (ctx.get (PID::TexDensity), 0.0f, 1.0f);
    const float tone = clamp (ctx.get (PID::TexTone), 0.0f, 1.0f);

    if (type != lastType)
    {
        for (auto& v : voices) v.band.reset();
        lastType = type;
    }

    if (std::abs (tone - lastTone) > 1.0e-3f)
    {
        // Tone: low-pass 1.5 kHz .. 18 kHz, high-pass 20 .. 200 Hz.
        for (int c = 0; c < 2; ++c)
        {
            toneLP[(size_t) c].setCutoff (1500.0f * std::pow (12.0f, tone), (float) sampleRate);
            toneHP[(size_t) c].setCutoff (20.0f + 180.0f * tone, (float) sampleRate);
        }
        lastTone = tone;
    }

    if (! primed)
    {
        level.setCurrentAndTarget (dbToGain (ctx.get (PID::TexLevel)));
        follow.setCurrentAndTarget (ctx.get (PID::TexFollow));
        width.setCurrentAndTarget (ctx.get (PID::TexWidth));
        primed = true;
    }
    level.setTarget (dbToGain (ctx.get (PID::TexLevel)));
    follow.setTarget (clamp (ctx.get (PID::TexFollow), 0.0f, 1.0f));
    width.setTarget (clamp (ctx.get (PID::TexWidth), 0.0f, 1.0f));

    float* x[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };
    const double humInc = 50.0 / sampleRate;

    for (int i = 0; i < n; ++i)
    {
        // Slow random drift for wind (centre and gusts), re-targeted every 0.5 s.
        if (--driftCounter <= 0)
        {
            windTarget = rng.nextBipolar();
            gustTarget = rng.nextFloat() * (0.4 + 0.6 * density);
            driftCounter = (int) (0.5 * sampleRate);
        }
        windDrift += 0.00003 * (windTarget - windDrift);
        gust += 0.00005 * (gustTarget - gust);

        // Envelope follower on the input.
        const float lvl = std::max (std::abs (x[0][i]), std::abs (x[1][i]));
        env = lvl + (lvl > env ? envAtk : envRel) * (env - lvl);

        const float f = follow.next();
        const float envGain = 1.0f + f * (std::min (1.0f, env * 4.0f) - 1.0f);
        const float g = level.next() * envGain;
        const float w = width.next();

        float common = generate (voices[2], type, density);
        if (type == 4)
        {
            humPhase += humInc;
            if (humPhase >= 1.0) humPhase -= 1.0;
            const double ph = twoPi * humPhase;
            const float buzz = 0.15f * (float) std::sin (ph * 7.0) * (float) (std::sin (ph) > 0.95 ? 1.0 : 0.0);
            common = 0.5f * (float) (std::sin (ph) + 0.5 * std::sin (2.0 * ph) + (0.2 + 0.4 * tone) * std::sin (3.0 * ph)
                                     + 0.15 * std::sin (5.0 * ph)) + buzz;
        }

        for (int c = 0; c < 2; ++c)
        {
            const float own = type == 4 ? common : generate (voices[(size_t) c], type, density);
            float s = common + w * (own - common);
            s = toneHP[(size_t) c].processHP (toneLP[(size_t) c].processLP (s));
            x[c][i] += g * s;
        }
    }
}

} // namespace aetherfall
