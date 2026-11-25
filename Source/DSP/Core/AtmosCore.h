#pragma once

#include <array>

#include "DSP/Chain/IModule.h"
#include "DSP/Utils/Smoothers.h"
#include "DSP/Utils/DelayLine.h"
#include "DSP/Utils/DCBlocker.h"
#include "DSP/Utils/Filters.h"
#include "DSP/Utils/Random.h"
#include "Diffuser.h"
#include "FDN.h"

namespace aetherfall
{

/**
    ATMOS CORE — the always-present main engine (spec section 4.1).

        in -> pre-delay -> bloom swell -> early diffusion -> bloom smear
           -> FDN16 tank (modulation, late diffusion, damping, shimmer)
           -> colour stage -> width -> DC blocker -> core mix (dry/wet)

    Parameters are evaluated per control chunk and smoothed at chunk rate;
    everything that would click (delay lengths, gains, mixes) is additionally
    ramped per sample inside the chunk.
*/
class AtmosCore final : public ModuleBase
{
public:
    AtmosCore();

    void reset() override;
    void process (juce::AudioBuffer<float>&, const ModContext&) override;
    double getTailSeconds() const override { return tailSeconds; }

    /** Approximate tail length for given parameter values (message thread). */
    static double estimateTailSeconds (float decaySeconds, int colorIndex, float preDelayMs, bool freeze);

    /** Longest pre-delay in seconds (synced divisions are clamped to it). */
    static constexpr double maxPreDelaySeconds = 2.0;

protected:
    void prepareModule() override;

private:
    /** Target values that depend on the colour mode (smoothed when switching). */
    struct ColorShape
    {
        double sizeMult, decayMult, hfMult, hfCap, hfRatio, lfMin, lfRatio;
        double modMult, diffBoost, saturation;
        double wDark, wVintage, wLofi, wVoid;
        double makeupDb;   // level compensation of the colour
    };
    static constexpr int numShapeFields = 15;
    static ColorShape shapeFor (int colorIndex) noexcept;

    /** One-pole smoothing evaluated once per chunk (cheap, zipper-free at
        control rate because the per-chunk steps are tiny). */
    struct ChunkSmoother
    {
        double value = 0.0;
        bool primed = false;
        double next (double target, double coeff) noexcept
        {
            if (! primed) { value = target; primed = true; }
            else          value = target + coeff * (value - target);
            return value;
        }
    };

    void updateChunkParameters (const ModContext& ctx, int numSamples);
    void processColor (double& l, double& r) noexcept;

    // --- DSP blocks -------------------------------------------------------
    dsp::FDN16 fdn;
    std::array<dsp::DelayLine<double>, 2> preDelay;
    std::array<dsp::DiffuserChain<double, 4>, 2> earlyDiff;
    std::array<dsp::DiffuserChain<double, 4>, 2> bloomDiff;
    std::array<dsp::DCBlocker<double>, 2> dcOut;

    // Colour stage filters (always running; crossfaded by weight).
    std::array<dsp::Biquad<double>, 2> darkLP, darkLowShelf;
    std::array<dsp::Biquad<double>, 2> vintageLP1, vintageLP2;
    std::array<dsp::Biquad<double>, 2> lofiLP1, lofiLP2;
    std::array<dsp::Biquad<double>, 2> voidLowShelf, voidHighShelf;
    std::array<double, 2> lofiHold {};
    double lofiPhase = 0.0;
    dsp::Random32 rng { 0xA7F3u };

    // --- Smoothed control values ----------------------------------------
    ChunkSmoother sSize, sDecay, sPreDelay, sDiffEarly, sDiffLate, sModDepth, sModRate,
                  sDampHF, sDampLF, sShimmer, sBlend, sBloom, sWidth, sFreeze;
    std::array<ChunkSmoother, numShapeFields> sShape;

    // Per-sample ramps inside the chunk.
    dsp::ControlRamp<double> preDelayRamp, dryRamp, wetRamp, widthRamp, bloomRamp;
    bool rampsPrimed = false;
    std::array<double, 4> colorWeights {};   // dark, vintage, lofi, void

    // Bloom swell envelopes.
    double envFast = 0.0, envSlow = 0.0;
    double envFastAtk = 0.0, envRel = 0.0, envSlowAtk = 0.0;

    double outputScale = 1.0;
    double tailSeconds = 5.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AtmosCore)
};

} // namespace aetherfall
