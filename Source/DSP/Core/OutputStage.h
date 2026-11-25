#pragma once

#include <array>
#include "DSP/Chain/IModule.h"
#include "DSP/Utils/Filters.h"
#include "DSP/Utils/Smoothers.h"
#include "DSP/Utils/Meters.h"

namespace aetherfall
{

/**
    Output Stage (spec section 3 / phase F6).

    Wet path:  tilt EQ -> HPF / LPF -> width (M/S) -> glue compressor
               -> ducker (wet ducked by the dry input envelope)
    Then:      dry/wet (equal power) -> soft clipper -> output gain
               -> safety limiter (no lookahead, never exceeds the ceiling)
               -> bypass crossfade to the dry signal.

    `dry` must be the dry signal aligned with the wet one (i.e. already
    delayed by the plug-in latency).
*/
class OutputStage final : public ModuleBase
{
public:
    void reset() override;
    void process (juce::AudioBuffer<float>&, const ModContext&) override {}

    /** Processes the wet buffer in place and mixes it with `dryL/dryR`. */
    void process (juce::AudioBuffer<float>& wet, const float* dryL, const float* dryR,
                  const ModContext& ctx, Meters* meters);

protected:
    void prepareModule() override;

private:
    float glueGain (float levelDb, const ModContext& ctx) noexcept;

    std::array<dsp::Biquad<float>, 2> tiltLow, tiltHigh;
    std::array<dsp::SVF<float>, 2> hpf, lpf;
    float lastTilt = 1000.0f, hpfHz = 0.0f, lpfHz = 0.0f;

    // Glue compressor (stereo-linked).
    float glueEnvDb = -120.0f, glueGrDb = 0.0f;
    // Ducker.
    float duckEnv = 0.0f, duckGrDb = 0.0f;
    // Limiter.
    float limGain = 1.0f, limRelease = 0.0f;

    dsp::LinearSmoother<float> hpfAmt, lpfAmt, width, glueOn, dryGain, wetGain,
                               clipOn, clipDrive, outGain, limiterOn, ceiling, bypass;
    LevelMeter outMeter;
    bool primed = false;
};

} // namespace aetherfall
