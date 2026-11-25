#pragma once

#include <array>
#include "DSP/Chain/IModule.h"
#include "DSP/Utils/Filters.h"
#include "DSP/Utils/Random.h"
#include "DSP/Utils/Smoothers.h"

namespace aetherfall
{

/**
    Bitcrush / Decimate (module 12).
    Sample-rate reduction (sample & hold with optional clock jitter) and
    bit-depth reduction (fractional bits allowed, so sweeps are smooth).
    "Smooth" blends in an anti-alias pre-filter and a post-filter that rounds
    off the staircase: 0 = raw and aliasing, 1 = "rude but musical".
*/
class Bitcrush final : public ModuleBase
{
public:
    ModuleType getType() const override { return ModuleType::Bitcrush; }
    void reset() override;
    void process (juce::AudioBuffer<float>&, const ModContext&) override;

protected:
    void prepareModule() override;

private:
    std::array<dsp::SVF<float>, 2> pre, post;
    std::array<float, 2> hold {};
    double phase = 0.0;
    dsp::Random32 rng { 0xB17u };
    dsp::LinearSmoother<float> bits, smooth;
    float lastRate = -1.0f;
    bool primed = false;
};

} // namespace aetherfall
