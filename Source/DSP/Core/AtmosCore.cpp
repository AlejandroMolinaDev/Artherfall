#include "AtmosCore.h"
#include "Parameters/ChoiceLists.h"

namespace aetherfall
{

using namespace dsp;

namespace
{
    // Early diffuser lengths (s) per channel — different L/R for decorrelation.
    constexpr std::array<double, 4> earlyLenL { 0.00477, 0.00359, 0.01273, 0.00931 };
    constexpr std::array<double, 4> earlyLenR { 0.00513, 0.00381, 0.01187, 0.00993 };
    constexpr double earlyMaxScale = 1.4;

    // Bloom "smear" diffusers: long all-passes that make the input wash in.
    constexpr std::array<double, 4> bloomLenL { 0.0231, 0.0373, 0.0529, 0.0713 };
    constexpr std::array<double, 4> bloomLenR { 0.0247, 0.0389, 0.0557, 0.0691 };

    constexpr double voidSizeMult = 1.6;   // must match shapeFor (Void)

    double chunkCoeff (double tauSeconds, int numSamples, double sr) noexcept
    {
        return std::exp (-(double) numSamples / (tauSeconds * sr));
    }

    double attackCoeff (double seconds, double sr) noexcept
    {
        return std::exp (-1.0 / (std::max (1.0e-4, seconds) * sr));
    }
}

//==============================================================================
AtmosCore::AtmosCore() = default;

AtmosCore::ColorShape AtmosCore::shapeFor (int colorIndex) noexcept
{
    //                 size  decay hfMul  hfCap    hfRat lfMin  lfRat mod  diff  sat   wDark wVin wLofi wVoid makeup
    switch (colorIndex)
    {
        case 1:  return { 1.00, 1.10, 0.45, 20000.0, 0.22, 20.0,  0.60, 1.00, 0.00, 0.00, 1, 0, 0, 0, 3.0 }; // Dark
        case 2:  return { 0.90, 1.00, 1.00,  9000.0, 0.35, 60.0,  0.50, 1.60, 0.05, 0.00, 0, 1, 0, 0, 0.0 }; // Vintage 80s
        case 3:  return { 0.85, 0.90, 1.00,  5000.0, 0.30, 250.0, 0.40, 1.30, 0.00, 0.35, 0, 0, 1, 0, 2.0 }; // Lo-Fi
        case 4:  return { voidSizeMult, 1.50, 0.50, 20000.0, 0.25, 20.0, 0.70, 1.30, 0.12, 0.00, 0, 0, 0, 1, 2.0 }; // Void
        default: return { 1.00, 1.00, 1.00, 20000.0, 0.35, 20.0,  0.50, 1.00, 0.00, 0.00, 0, 0, 0, 0, 0.0 }; // Clean
    }
}

double AtmosCore::estimateTailSeconds (float decaySeconds, int colorIndex, float preDelayMs, bool freeze)
{
    if (freeze)
        return std::numeric_limits<double>::infinity();

    return (double) decaySeconds * shapeFor (colorIndex).decayMult * 1.2
           + (double) preDelayMs * 0.001 + 0.5;
}

//==============================================================================
void AtmosCore::prepareModule()
{
    const double sr = sampleRate;

    const double maxLen = FDN16::baseLengthSec.back() * voidSizeMult * sr;
    fdn.prepare (sr, maxLen);

    for (auto& d : preDelay)
        d.prepare ((int) std::ceil (maxPreDelaySeconds * sr) + 4);

    earlyDiff[0].prepare (sr, earlyLenL, earlyMaxScale);
    earlyDiff[1].prepare (sr, earlyLenR, earlyMaxScale);
    bloomDiff[0].prepare (sr, bloomLenL, 1.0);
    bloomDiff[1].prepare (sr, bloomLenR, 1.0);
    for (auto& b : bloomDiff)
        b.setGain (0.6);

    for (auto& dc : dcOut)
        dc.prepare (sr, 6.0);

    for (int c = 0; c < 2; ++c)
    {
        const auto k = (size_t) c;
        darkLP[k].setLowPass (3800.0, 0.6, sr);
        darkLowShelf[k].setLowShelf (220.0, 0.7, 2.5, sr);
        vintageLP1[k].setLowPass (std::min (10500.0, sr * 0.45), 0.5412, sr);   // 4th-order Butterworth
        vintageLP2[k].setLowPass (std::min (10500.0, sr * 0.45), 1.3066, sr);
        lofiLP1[k].setLowPass (4500.0, 0.5412, sr);
        lofiLP2[k].setLowPass (4500.0, 1.3066, sr);
        voidLowShelf[k].setLowShelf (150.0, 0.7, 3.0, sr);
        voidHighShelf[k].setHighShelf (3500.0, 0.7, -4.0, sr);
    }

    envFastAtk = attackCoeff (0.005, sr);
    envRel     = attackCoeff (0.25, sr);
    envSlowAtk = attackCoeff (0.25, sr);

    reset();
}

void AtmosCore::reset()
{
    fdn.reset();
    for (auto& d : preDelay)  d.reset();
    for (auto& d : earlyDiff) d.reset();
    for (auto& d : bloomDiff) d.reset();
    for (auto& d : dcOut)     d.reset();

    for (auto* arr : { &darkLP, &darkLowShelf, &vintageLP1, &vintageLP2,
                       &lofiLP1, &lofiLP2, &voidLowShelf, &voidHighShelf })
        for (auto& f : *arr)
            f.reset();

    lofiHold = { 0.0, 0.0 };
    lofiPhase = 0.0;
    envFast = envSlow = 0.0;

    for (auto* s : { &sSize, &sDecay, &sPreDelay, &sDiffEarly, &sDiffLate, &sModDepth, &sModRate,
                     &sDampHF, &sDampLF, &sShimmer, &sBlend, &sBloom, &sWidth, &sFreeze })
        s->primed = false;
    for (auto& s : sShape)
        s.primed = false;

    rampsPrimed = false;
}

//==============================================================================
void AtmosCore::updateChunkParameters (const ModContext& ctx, int n)
{
    const double sr = sampleRate;
    const double cFast = chunkCoeff (0.05, n, sr);
    const double cMed  = chunkCoeff (0.12, n, sr);
    const double cSlow = chunkCoeff (0.35, n, sr);

    // --- Colour mode: every field is smoothed so switching modes glides.
    const auto target = shapeFor (ctx.getChoice (PID::CoreColor));
    const std::array<double, numShapeFields> tv {
        target.sizeMult, target.decayMult, target.hfMult, target.hfCap, target.hfRatio,
        target.lfMin, target.lfRatio, target.modMult, target.diffBoost, target.saturation,
        target.wDark, target.wVintage, target.wLofi, target.wVoid, target.makeupDb };
    std::array<double, numShapeFields> sv {};
    for (size_t k = 0; k < sv.size(); ++k)
        sv[k] = sShape[k].next (tv[k], cMed);

    const ColorShape cs { sv[0], sv[1], sv[2], sv[3], sv[4], sv[5], sv[6], sv[7], sv[8], sv[9],
                          sv[10], sv[11], sv[12], sv[13], sv[14] };
    colorWeights = { cs.wDark, cs.wVintage, cs.wLofi, cs.wVoid };

    // --- Core parameters (log-domain smoothing for times and frequencies).
    const double size     = sSize.next (ctx.get (PID::CoreSize), cSlow);
    const double decay    = std::exp (sDecay.next (std::log (std::max (0.05f, ctx.get (PID::CoreDecay))), cFast));
    const double freeze   = sFreeze.next (ctx.getBool (PID::CoreFreeze) ? 1.0 : 0.0, cFast);
    const double diffE    = sDiffEarly.next (ctx.get (PID::CoreDiffEarly), cFast);
    const double diffL    = sDiffLate.next (ctx.get (PID::CoreDiffLate), cFast);
    const double modDepth = sModDepth.next (ctx.get (PID::CoreModDepth), cFast);
    const double modRate  = std::exp (sModRate.next (std::log (std::max (0.01f, ctx.get (PID::CoreModRate))), cFast));
    const double dampHF   = std::exp (sDampHF.next (std::log (std::max (20.0f, ctx.get (PID::CoreDampHF))), cFast));
    const double dampLF   = std::exp (sDampLF.next (std::log (std::max (5.0f, ctx.get (PID::CoreDampLF))), cFast));
    const double shimmer  = sShimmer.next (ctx.get (PID::CoreShimmer), cFast);
    const double blend    = sBlend.next (ctx.get (PID::CoreShimBlend), cFast);
    const double bloom    = sBloom.next (ctx.get (PID::CoreBloom), cFast);
    const double width    = sWidth.next (ctx.get (PID::CoreWidth), cFast);

    double preDelaySec = ctx.getBool (PID::CorePreSync) ? ctx.divisionSeconds (ctx.getChoice (PID::CorePreDiv))
                                                        : (double) ctx.get (PID::CorePreDelay) * 0.001;
    preDelaySec = sPreDelay.next (clamp (preDelaySec, 0.0, maxPreDelaySeconds), cMed);

    // --- Tank parameters.
    FDN16::Params p;
    const double sizeScale = (0.08 + 0.92 * std::pow (clamp (size, 0.0, 1.0), 1.2)) * cs.sizeMult;
    const double rt60 = clamp (decay * cs.decayMult, 0.1, 120.0);

    const double modDepthSec = std::min (std::pow (clamp (modDepth, 0.0, 1.0), 1.5) * 0.0045 * cs.modMult, 0.0095);
    p.modDepthSamples = modDepthSec * sr;
    p.modRateHz = modRate;

    p.lateScale     = clamp ((0.35 + 0.65 * size) * std::min (cs.sizeMult, 1.3), 0.1, FDN16::maxLateScale);
    p.lateDiffusion = clamp (0.7 * diffL + cs.diffBoost, 0.0, 0.82);

    const double shim = clamp (shimmer, 0.0, 1.0) * 0.85 * (1.0 - freeze);
    p.shimmer = shim;
    p.shimmerBlend = blend;
    p.ratioA = semitonesToRatio ((double) choices::shimIntervalSemis[(size_t) clamp (ctx.getChoice (PID::CoreShimA), 0, 5)]);
    p.ratioB = semitonesToRatio ((double) choices::shimIntervalSemis[(size_t) clamp (ctx.getChoice (PID::CoreShimB), 0, 5)]);

    p.hfCutoffHz = std::min (dampHF * cs.hfMult, cs.hfCap);
    p.lfCutoffHz = std::max (dampLF, cs.lfMin);
    p.saturation = cs.saturation;
    p.inputGain  = 1.0 - freeze;

    const double hfExp = 1.0 / std::max (0.05, cs.hfRatio) - 1.0;
    const double lfExp = 1.0 / std::max (0.05, cs.lfRatio) - 1.0;
    constexpr double frozenGain = 0.999995;   // ~70,000 s RT60: "infinite" but bounded

    for (int i = 0; i < FDN16::numLines; ++i)
    {
        const auto k = (size_t) i;
        const double len = FDN16::baseLengthSec[k] * sizeScale * sr;
        const double loop = len + FDN16::lateBaseSec[k] * p.lateScale * sr + p.modDepthSamples
                            + (i < FDN16::numShimmerLines ? shim * fdn.getShimmerWindow() * 0.5 : 0.0);

        // Attenuation per pass so every line decays 60 dB in rt60 seconds.
        const double dbPerPass = -60.0 * loop / (rt60 * sr);
        const double g  = std::pow (10.0, dbPerPass / 20.0);
        const double hf = std::pow (10.0, dbPerPass * hfExp / 20.0);
        const double lf = std::pow (10.0, dbPerPass * lfExp / 20.0);

        p.lengthSamples[k] = len;
        p.gain[k]        = g  + freeze * (frozenGain - g);
        p.hfShelfGain[k] = hf + freeze * (1.0 - hf);
        p.lfShelfGain[k] = lf + freeze * (1.0 - lf);
    }

    fdn.beginChunk (p, n);

    // --- Input diffusion.
    const double earlyGain  = clamp (0.75 * diffE + cs.diffBoost, 0.0, 0.85);
    const double earlyScale = clamp ((0.3 + 0.7 * size) * std::min (cs.sizeMult, 1.35), 0.05, earlyMaxScale);
    for (auto& d : earlyDiff)
    {
        d.setGain (earlyGain);
        d.setScale (earlyScale);
    }

    // --- Bloom: the slow envelope's attack grows with the amount (0.02 .. 6 s).
    envSlowAtk = attackCoeff (0.02 + 6.0 * bloom * bloom, sr);

    // --- Output level. The steady-state energy of the tank grows with RT60 and
    //     falls with the line lengths (fewer recirculations per second), so the
    //     raw level spans ~25 dB across the Decay/Size ranges. Compensate most of
    //     it (exponents measured with the "[.measure]" test) so that the wet level
    //     sits around -3 dB re. input at Decay 3 s / Size 0.6, while still letting
    //     long tails feel a little bigger.
    constexpr double refSizeScale = 0.578;   // sizeScale at Size = 0.6
    outputScale = 3.2 * std::pow (clamp (rt60, 0.2, 60.0) / 3.0, -0.38)
                      * std::pow (sizeScale / refSizeScale, 0.5)
                      * std::pow (10.0, cs.makeupDb / 20.0);

    // --- Per-sample ramps.
    const double mix = clamp ((double) ctx.get (PID::CoreMix), 0.0, 1.0);
    const double dryG = std::cos (mix * halfPi);
    const double wetG = std::sin (mix * halfPi);
    const double pdSamples = preDelaySec * sr;

    if (! rampsPrimed)
    {
        preDelayRamp.reset (pdSamples);
        dryRamp.reset (dryG);
        wetRamp.reset (wetG);
        widthRamp.reset (width);
        bloomRamp.reset (bloom);
        rampsPrimed = true;
    }

    preDelayRamp.beginChunk (pdSamples, n);
    dryRamp.beginChunk (dryG, n);
    wetRamp.beginChunk (wetG, n);
    widthRamp.beginChunk (width, n);
    bloomRamp.beginChunk (bloom, n);

    tailSeconds = freeze > 0.5 ? 1.0e9 : rt60 * 1.2 + preDelaySec;
}

//==============================================================================
void AtmosCore::processColor (double& l, double& r) noexcept
{
    const double wDark = colorWeights[0], wVin = colorWeights[1];
    const double wLofi = colorWeights[2], wVoid = colorWeights[3];

    // Lo-Fi sample & hold clock (~12 kHz with a little jitter).
    lofiPhase += (12000.0 + 400.0 * rng.nextBipolar()) / sampleRate;
    const bool lofiTick = lofiPhase >= 1.0;
    if (lofiTick)
        lofiPhase -= 1.0;

    constexpr double vinStep = 1.0 / 2048.0;   // 12-bit
    constexpr double lofiStep = 1.0 / 256.0;   // 9-bit

    for (int c = 0; c < 2; ++c)
    {
        const auto k = (size_t) c;
        const double x = c == 0 ? l : r;

        // Run every filter all the time so crossfading colours never clicks.
        const double dark = darkLowShelf[k].process (darkLP[k].process (x));

        double vin = vintageLP2[k].process (vintageLP1[k].process (x));
        vin = std::round (vin / vinStep + 0.5 * (rng.nextFloat() - rng.nextFloat())) * vinStep;

        const double lofiIn = lofiLP2[k].process (lofiLP1[k].process (x));
        if (lofiTick)
            lofiHold[k] = std::round (lofiIn / lofiStep) * lofiStep;
        const double lofi = lofiHold[k];

        const double vd = voidHighShelf[k].process (voidLowShelf[k].process (x));

        double y = x;
        y += wDark * (dark - x);
        y += wVin  * (vin - x);
        y += wLofi * (lofi - x);
        y += wVoid * (vd - x);

        (c == 0 ? l : r) = y;
    }
}

//==============================================================================
void AtmosCore::process (juce::AudioBuffer<float>& buffer, const ModContext& ctx)
{
    const int n = std::min (ctx.numSamples, buffer.getNumSamples());
    if (n <= 0)
        return;

    updateChunkParameters (ctx, n);

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    for (int i = 0; i < n; ++i)
    {
        const double inL = L[i], inR = R[i];

        // Pre-delay (write then tap(D) = D-sample delay).
        preDelay[0].write (inL);
        preDelay[1].write (inR);
        const double pd = preDelayRamp.next();
        double xL = preDelay[0].tapLinear (pd);
        double xR = preDelay[1].tapLinear (pd);

        // Bloom swell: ratio of a slow to a fast envelope. On each new onset the
        // fast one jumps up while the slow one lags, so the tank input fades in
        // over the bloom time; sustained material converges to unity gain.
        const double level = std::max (std::abs (xL), std::abs (xR));
        envFast = level + (level > envFast ? envFastAtk : envRel) * (envFast - level);
        envSlow = level + (level > envSlow ? envSlowAtk : envRel) * (envSlow - level);
        const double swell = envFast > 1.0e-9 ? clamp (envSlow / envFast, 0.0, 1.0) : 1.0;

        const double bloom = bloomRamp.next();
        const double g = 1.0 + bloom * (swell - 1.0);
        xL *= g;
        xR *= g;

        // Early diffusion, then the bloom smear (long all-passes) blended in.
        xL = earlyDiff[0].process (xL);
        xR = earlyDiff[1].process (xR);
        const double sL = bloomDiff[0].process (xL);
        const double sR = bloomDiff[1].process (xR);
        xL += 0.75 * bloom * (sL - xL);
        xR += 0.75 * bloom * (sR - xR);

        // Tank.
        double wL = 0.0, wR = 0.0;
        fdn.process (xL, xR, wL, wR);
        wL *= outputScale;
        wR *= outputScale;

        processColor (wL, wR);

        // Width (M/S).
        const double w = widthRamp.next();
        const double m = 0.5 * (wL + wR);
        const double s = 0.5 * (wL - wR) * w;
        wL = dcOut[0].process (m + s);
        wR = dcOut[1].process (m - s);

        const double dryG = dryRamp.next();
        const double wetG = wetRamp.next();
        L[i] = (float) (dryG * inL + wetG * wL);
        R[i] = (float) (dryG * inR + wetG * wR);
    }
}

} // namespace aetherfall
