#pragma once

#include <array>
#include "DSP/Chain/IModule.h"
#include "DSP/Utils/DelayLine.h"
#include "DSP/Utils/Filters.h"
#include "DSP/Utils/Random.h"
#include "DSP/Utils/Smoothers.h"

namespace aetherfall
{

/**
    Lo-Fi Tape Machine (module 18): the whole "worn cassette" chain in one
    block — compression, tape saturation, wow/flutter wobble, high-frequency
    loss, head bump and hiss.
*/
class LoFiTape final : public ModuleBase
{
public:
    ModuleType getType() const override { return ModuleType::LoFi; }
    void reset() override;
    void process (juce::AudioBuffer<float>&, const ModContext&) override;

protected:
    void prepareModule() override;

private:
    std::array<dsp::DelayLine<float>, 2> wobbleLine;
    std::array<dsp::OnePole<float>, 2> hfLoss1, hfLoss2;
    std::array<dsp::Biquad<float>, 2> headBump;
    dsp::Random32 rng { 0x10F1u };

    float compEnv = 0.0f, compAtk = 0.0f, compRel = 0.0f;
    double wobPhase = 0.0, flutPhase = 0.0;
    float lastTone = -1.0f;
    dsp::LinearSmoother<float> sat, comp, wobble, noise;
    bool primed = false;
};

} // namespace aetherfall
