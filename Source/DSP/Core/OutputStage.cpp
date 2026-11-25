#include "OutputStage.h"
#include "DSP/Utils/DspMath.h"

namespace aetherfall
{

using namespace dsp;

namespace
{
    /** Clipper curve: linear up to T, then a tanh knee that reaches 1.0. */
    inline float softClip (float x) noexcept
    {
        constexpr float T = 0.8f;   // knee starts at about -1.9 dBFS
        const float a = std::abs (x);
        if (a <= T)
            return x;
        return std::copysign (T + (1.0f - T) * fastTanh ((a - T) / (1.0f - T)), x);
    }
}

void OutputStage::prepareModule()
{
    for (auto* s : { &hpfAmt, &lpfAmt, &width, &glueOn, &dryGain, &wetGain, &clipOn,
                     &clipDrive, &outGain, &limiterOn, &ceiling, &bypass })
        s->prepare (sampleRate, 0.03);
    limRelease = (float) std::exp (-1.0 / (0.08 * sampleRate));
    outMeter.prepare (sampleRate);
    reset();
}

void OutputStage::reset()
{
    for (auto& f : tiltLow)  f.reset();
    for (auto& f : tiltHigh) f.reset();
    for (auto& f : hpf) f.reset();
    for (auto& f : lpf) f.reset();
    lastTilt = 1000.0f;
    hpfHz = lpfHz = 0.0f;
    glueEnvDb = -120.0f;
    glueGrDb = 0.0f;
    duckEnv = 0.0f;
    duckGrDb = 0.0f;
    limGain = 1.0f;
    outMeter.reset();
    primed = false;
}

float OutputStage::glueGain (float levelDb, const ModContext& ctx) noexcept
{
    // Soft-knee (6 dB) static curve -> target gain reduction in dB.
    const float thr = ctx.get (PID::OutGlueThresh);
    const float ratio = std::max (1.0f, ctx.get (PID::OutGlueRatio));
    constexpr float knee = 6.0f;
    const float over = levelDb - thr;
    float compressed = 0.0f;
    if (2.0f * over <= -knee)       compressed = 0.0f;
    else if (2.0f * over < knee)    compressed = (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
    else                            compressed = over;
    return -compressed * (1.0f - 1.0f / ratio);
}

void OutputStage::process (juce::AudioBuffer<float>& wet, const float* dryL, const float* dryR,
                           const ModContext& ctx, Meters* meters)
{
    const int n = std::min (ctx.numSamples, wet.getNumSamples());
    if (n <= 0)
        return;

    const float sr = (float) sampleRate;
    const float tilt = clamp (ctx.get (PID::OutTilt), -12.0f, 12.0f);
    const float tHpf = ctx.get (PID::OutHPF), tLpf = ctx.get (PID::OutLPF);
    const bool hpfOnT = tHpf > 20.5f, lpfOnT = tLpf < 19900.0f;
    const float mix = clamp (ctx.get (PID::OutMix), 0.0f, 1.0f);

    auto targets = [&]
    {
        hpfAmt.setTarget (hpfOnT ? 1.0f : 0.0f);
        lpfAmt.setTarget (lpfOnT ? 1.0f : 0.0f);
        width.setTarget (clamp (ctx.get (PID::OutWidth), 0.0f, 2.0f));
        glueOn.setTarget (ctx.getBool (PID::OutGlueOn) ? 1.0f : 0.0f);
        dryGain.setTarget (std::cos (mix * (float) halfPi));
        wetGain.setTarget (std::sin (mix * (float) halfPi));
        clipOn.setTarget (ctx.getBool (PID::OutClipOn) ? 1.0f : 0.0f);
        clipDrive.setTarget (dbToGain (ctx.get (PID::OutClipDrive)));
        outGain.setTarget (dbToGain (ctx.get (PID::OutGain)));
        limiterOn.setTarget (ctx.getBool (PID::OutLimitOn) ? 1.0f : 0.0f);
        ceiling.setTarget (dbToGain (ctx.get (PID::OutCeiling)));
        bypass.setTarget (ctx.getBool (PID::Bypass) ? 1.0f : 0.0f);
    };

    targets();
    if (! primed)
    {
        for (auto* s : { &hpfAmt, &lpfAmt, &width, &glueOn, &dryGain, &wetGain, &clipOn,
                         &clipDrive, &outGain, &limiterOn, &ceiling, &bypass })
            s->setCurrentAndTarget (s->getTarget());
        primed = true;
    }

    if (std::abs (tilt - lastTilt) > 1.0e-3f)
    {
        // Tilt around ~800 Hz: half the gain up on one side, half down on the other.
        for (int c = 0; c < 2; ++c)
        {
            tiltLow[(size_t) c].setLowShelf (800.0f, 0.5f, -0.5f * tilt, sr);
            tiltHigh[(size_t) c].setHighShelf (800.0f, 0.5f, 0.5f * tilt, sr);
        }
        lastTilt = tilt;
    }
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

    // Time constants.
    const float glueAtk = std::exp (-1.0f / (std::max (0.05f, ctx.get (PID::OutGlueAttack)) * 0.001f * sr));
    const float glueRel = std::exp (-1.0f / (std::max (5.0f, ctx.get (PID::OutGlueRelease)) * 0.001f * sr));
    const float glueMakeup = ctx.get (PID::OutGlueMakeup);
    const float duckAtk = std::exp (-1.0f / (std::max (0.5f, ctx.get (PID::OutDuckAttack)) * 0.001f * sr));
    const float duckRel = std::exp (-1.0f / (std::max (5.0f, ctx.get (PID::OutDuckRelease)) * 0.001f * sr));
    const float duckAmt = clamp (ctx.get (PID::OutDuck), 0.0f, 1.0f);
    const float duckThr = ctx.get (PID::OutDuckThresh);

    float* L = wet.getWritePointer (0);
    float* R = wet.getWritePointer (1);
    float maxGlueGr = 0.0f, maxDuckGr = 0.0f, minLim = 1.0f;

    for (int i = 0; i < n; ++i)
    {
        float l = L[i], r = R[i];

        // --- Tilt + filters.
        l = tiltHigh[0].process (tiltLow[0].process (l));
        r = tiltHigh[1].process (tiltLow[1].process (r));
        const float ha = hpfAmt.next(), la = lpfAmt.next();
        { const float h = hpf[0].processHP (l); l += ha * (h - l); }
        { const float h = hpf[1].processHP (r); r += ha * (h - r); }
        { const float lp = lpf[0].processLP (l); l += la * (lp - l); }
        { const float lp = lpf[1].processLP (r); r += la * (lp - r); }

        // --- Width.
        const float w = width.next();
        const float m = 0.5f * (l + r), s = 0.5f * (l - r) * w;
        l = m + s;
        r = m - s;

        // --- Glue compressor: peak level in dB, attack/release on the gain reduction.
        const float go = glueOn.next();
        {
            const float level = std::max (std::abs (l), std::abs (r));
            const float levelDb = gainToDb (level + 1.0e-9f);
            const float target = glueGain (levelDb, ctx);
            glueGrDb = target + (target < glueGrDb ? glueAtk : glueRel) * (glueGrDb - target);
            const float g = dbToGain (glueGrDb + glueMakeup);
            const float gg = 1.0f + go * (g - 1.0f);
            l *= gg;
            r *= gg;
            maxGlueGr = std::min (maxGlueGr, go * glueGrDb);
        }

        // --- Ducker: the dry input pushes the wet down.
        {
            const float d = std::max (std::abs (dryL[i]), std::abs (dryR[i]));
            duckEnv = d + (d > duckEnv ? duckAtk : duckRel) * (duckEnv - d);
            const float over = std::max (0.0f, gainToDb (duckEnv + 1.0e-9f) - duckThr);
            duckGrDb = -duckAmt * std::min (30.0f, over * 1.5f);
            const float g = dbToGain (duckGrDb);
            l *= g;
            r *= g;
            maxDuckGr = std::min (maxDuckGr, duckGrDb);
        }

        // --- Dry / wet.
        const float dg = dryGain.next(), wg = wetGain.next();
        float ol = dg * dryL[i] + wg * l;
        float orr = dg * dryR[i] + wg * r;

        // --- Soft clipper.
        const float co = clipOn.next(), cd = clipDrive.next();
        if (co > 1.0e-4f)
        {
            const float cl = softClip (ol * cd), cr = softClip (orr * cd);
            ol += co * (cl - ol);
            orr += co * (cr - orr);
        }

        // --- Output gain.
        const float og = outGain.next();
        ol *= og;
        orr *= og;

        // --- Safety limiter: instant attack, 80 ms release, hard ceiling.
        const float lo = limiterOn.next(), ceil = ceiling.next();
        if (lo > 1.0e-4f)
        {
            const float peak = std::max (std::abs (ol), std::abs (orr));
            const float needed = peak > ceil ? ceil / peak : 1.0f;
            limGain = needed < limGain ? needed : needed + limRelease * (limGain - needed);
            const float lg = 1.0f + lo * (limGain - 1.0f);
            ol = clamp (ol * lg, -ceil, ceil) * lo + ol * lg * (1.0f - lo);
            orr = clamp (orr * lg, -ceil, ceil) * lo + orr * lg * (1.0f - lo);
            minLim = std::min (minLim, limGain);
        }

        // --- Bypass (host or button): crossfade to the aligned dry signal.
        const float bp = bypass.next();
        L[i] = sanitize (ol + bp * (dryL[i] - ol));
        R[i] = sanitize (orr + bp * (dryR[i] - orr));
    }

    if (meters != nullptr)
    {
        float pl = 0.0f, pr = 0.0f;
        outMeter.process (L, R, n, pl, pr);
        meters->outPeakL.store (pl, std::memory_order_relaxed);
        meters->outPeakR.store (pr, std::memory_order_relaxed);
        meters->outRmsL.store (outMeter.rmsL(), std::memory_order_relaxed);
        meters->outRmsR.store (outMeter.rmsR(), std::memory_order_relaxed);
        meters->correlation.store (outMeter.correlation(), std::memory_order_relaxed);
        meters->glueGainReductionDb.store (maxGlueGr, std::memory_order_relaxed);
        meters->duckGainReductionDb.store (maxDuckGr, std::memory_order_relaxed);
        meters->limiterGainReductionDb.store (gainToDb (minLim), std::memory_order_relaxed);
    }
}

} // namespace aetherfall
