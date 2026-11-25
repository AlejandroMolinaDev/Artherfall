#pragma once

#include <array>
#include "DSP/Chain/IModule.h"
#include "DSP/Utils/DelayLine.h"
#include "DSP/Utils/Filters.h"
#include "DSP/Utils/Smoothers.h"
#include "DSP/Utils/TempoPhase.h"

namespace aetherfall
{

/**
    Stereo Engine (module 14): M/S width, Haas delay, auto-pan, "orbit"
    (rotation of the stereo image) and bass mono (Linkwitz-Riley crossover,
    lows summed to mono so the low end stays centred).
*/
class StereoEngine final : public ModuleBase
{
public:
    ModuleType getType() const override { return ModuleType::Stereo; }
    void reset() override;
    void process (juce::AudioBuffer<float>&, const ModContext&) override;

protected:
    void prepareModule() override;

private:
    void updateCrossover (float hz);

    // LR4 = two cascaded 2nd-order Butterworth sections per band.
    std::array<std::array<dsp::Biquad<float>, 2>, 2> lowpass, highpass;   // [channel][section]
    float crossoverHz = 0.0f;

    dsp::DelayLine<float> haasHi, haasFull;   // one per path (both always run)
    dsp::TempoPhase lfo;

    dsp::LinearSmoother<float> width, haas, panDepth, orbit, bassMonoAmt;
    bool primed = false;
};

} // namespace aetherfall
