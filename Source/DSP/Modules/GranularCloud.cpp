#include "GranularCloud.h"
#include "DSP/Utils/DspMath.h"

namespace aetherfall
{

using namespace dsp;

void GranularCloud::prepareModule()
{
    const int size = nextPowerOfTwo ((int) std::ceil (bufferSeconds * sampleRate) + 8);
    buffer.assign ((size_t) size, 0.0f);
    mask = size - 1;

    // Window tables (+1 guard point for interpolation).
    for (auto& w : windows)
        w.assign ((size_t) windowSize + 1, 0.0f);

    for (int i = 0; i <= windowSize; ++i)
    {
        const double x = (double) i / windowSize;
        windows[0][(size_t) i] = (float) (0.5 - 0.5 * std::cos (twoPi * x));               // Hann

        constexpr double alpha = 0.5;                                                      // Tukey
        double tk = 1.0;
        if (x < alpha / 2)            tk = 0.5 * (1.0 + std::cos (twoPi / alpha * (x - alpha / 2)));
        else if (x > 1.0 - alpha / 2) tk = 0.5 * (1.0 + std::cos (twoPi / alpha * (x - 1.0 + alpha / 2)));
        windows[1][(size_t) i] = (float) tk;

        windows[2][(size_t) i] = (float) (1.0 - std::abs (2.0 * x - 1.0));                 // Triangle
    }

    norm.prepare (sampleRate, 0.1);
    feedback.prepare (sampleRate, 0.05);
    reset();
}

void GranularCloud::reset()
{
    std::fill (buffer.begin(), buffer.end(), 0.0f);
    writePos = 0;
    for (auto& g : grains)
        g.active = false;
    samplesToNext = 0.0;
    primed = false;
}

int GranularCloud::getActiveGrainCount() const noexcept
{
    int count = 0;
    for (const auto& g : grains)
        count += g.active ? 1 : 0;
    return count;
}

float GranularCloud::window (int type, float phase) const noexcept
{
    const float x = clamp (phase, 0.0f, 1.0f) * (float) windowSize;
    const int i = std::min ((int) x, windowSize - 1);
    const float f = x - (float) i;
    const auto& w = windows[(size_t) type];
    return w[(size_t) i] + f * (w[(size_t) i + 1] - w[(size_t) i]);
}

void GranularCloud::spawnGrain (const ModContext& ctx)
{
    // Eco quality: at most 32 simultaneous grains.
    const int voiceLimit = ctx.getChoice (PID::GlobQuality) == 0 ? maxGrains / 2 : maxGrains;
    Grain* g = nullptr;
    for (int i = 0; i < voiceLimit; ++i)
        if (! grains[(size_t) i].active) { g = &grains[(size_t) i]; break; }

    if (g == nullptr)
        return;   // all 64 voices busy

    const double sr = sampleRate;
    const int bufLen = mask + 1;
    const int length = std::max (64, (int) (ctx.get (PID::GranSize) * 0.001 * sr));

    // --- Pitch: base + random component (free, octaves, or octaves + fifths).
    const float range = ctx.get (PID::GranPitchRand);
    float rnd = 0.0f;
    switch (ctx.getChoice (PID::GranRandMode))
    {
        case 1:  // octaves
        {
            const int maxOct = (int) std::floor (range / 12.0f);
            rnd = 12.0f * (float) (rng.nextInt (2 * maxOct + 1) - maxOct);
            break;
        }
        case 2:  // octaves + fifths
        {
            static constexpr float steps[] = { -24.0f, -19.0f, -12.0f, -7.0f, 0.0f, 7.0f, 12.0f, 19.0f, 24.0f };
            float pool[9];
            int count = 0;
            for (float s : steps)
                if (std::abs (s) <= range + 1.0e-3f)
                    pool[count++] = s;
            rnd = pool[rng.nextInt (count)];
            break;
        }
        default:
            rnd = range * rng.nextBipolar();
            break;
    }

    double ratio = semitonesToRatio ((double) (ctx.get (PID::GranPitch) + rnd));
    const bool reverse = rng.nextFloat() < ctx.get (PID::GranReverse);

    // --- Start offset behind the write head.
    double offset = (double) ctx.get (PID::GranPosition) * 6.0 * sr
                  + (double) rng.nextFloat() * ctx.get (PID::GranSpray) * 1.5 * sr;

    if (! reverse)
    {
        // A grain faster than real time must not overtake the write head.
        const double minOffset = (ratio > 1.0 ? (double) length * (ratio - 1.0) : 0.0) + 8.0;
        offset = clamp (offset, minOffset, std::max (minOffset, (double) bufLen - 8.0));
    }
    else
    {
        // Backwards: the grain moves into the past; stay inside the buffer.
        offset = clamp (offset, 8.0, std::max (8.0, (double) bufLen - (double) length * ratio - 8.0));
        ratio = -ratio;
    }

    double pos = (double) writePos - offset;
    while (pos < 0.0)
        pos += (double) bufLen;

    // --- Equal-power pan inside the stereo width.
    const float pan = rng.nextBipolar() * clamp (ctx.get (PID::GranWidth), 0.0f, 1.0f);
    const float theta = (pan + 1.0f) * (float) (pi * 0.25);

    g->active = true;
    g->pos = pos;
    g->inc = ratio;
    g->length = length;
    g->age = 0;
    g->gainL = 1.41421356f * std::cos (theta);
    g->gainR = 1.41421356f * std::sin (theta);
}

void GranularCloud::process (juce::AudioBuffer<float>& io, const ModContext& ctx)
{
    const int n = std::min (ctx.numSamples, io.getNumSamples());
    if (n <= 0)
        return;

    const double sr = sampleRate;
    const bool frozen = ctx.getBool (PID::GranFreeze);
    const double density = clamp ((double) ctx.get (PID::GranDensity), 0.5, 200.0);
    const double grainSec = ctx.get (PID::GranSize) * 0.001;
    windowType = clamp (ctx.getChoice (PID::GranWindow), 0, 2);

    // Expected number of overlapping grains -> uncorrelated sum grows with sqrt.
    const float overlap = (float) std::max (1.0, density * grainSec);
    const float targetNorm = 1.0f / std::sqrt (overlap);
    const float targetFb = clamp (ctx.get (PID::GranFeedback), 0.0f, 0.9f);

    if (! primed)
    {
        norm.setCurrentAndTarget (targetNorm);
        feedback.setCurrentAndTarget (targetFb);
        primed = true;
    }
    norm.setTarget (targetNorm);
    feedback.setTarget (targetFb);

    float* L = io.getWritePointer (0);
    float* R = io.getWritePointer (1);
    const float* buf = buffer.data();

    for (int i = 0; i < n; ++i)
    {
        // --- Scheduler: jittered intervals around 1/density.
        samplesToNext -= 1.0;
        if (samplesToNext <= 0.0)
        {
            spawnGrain (ctx);
            samplesToNext += sr / density * (0.7 + 0.6 * (double) rng.nextFloat());
        }

        // --- Render active grains.
        float outL = 0.0f, outR = 0.0f;
        for (auto& g : grains)
        {
            if (! g.active)
                continue;

            const float w = window (windowType, (float) g.age / (float) g.length);

            const int i0 = (int) g.pos;
            const float f = (float) (g.pos - (double) i0);
            const float a = buf[(size_t) (i0 & mask)];
            const float b = buf[(size_t) ((i0 + 1) & mask)];
            const float s = (a + f * (b - a)) * w;

            outL += s * g.gainL;
            outR += s * g.gainR;

            g.pos += g.inc;
            if (g.pos >= (double) (mask + 1)) g.pos -= (double) (mask + 1);
            else if (g.pos < 0.0)             g.pos += (double) (mask + 1);

            if (++g.age >= g.length)
                g.active = false;
        }

        const float nrm = norm.next();
        outL *= nrm;
        outR *= nrm;

        // --- Write input (+ feedback of the cloud) unless frozen.
        const float fb = feedback.next();
        if (! frozen)
        {
            const float in = 0.5f * (L[i] + R[i]) + fb * 0.5f * (outL + outR);
            buffer[(size_t) writePos] = fastTanh (in);
            writePos = (writePos + 1) & mask;
        }

        L[i] = outL;
        R[i] = outR;
    }
}

} // namespace aetherfall
