#pragma once

#include <array>
#include "DSP/Chain/IModule.h"
#include "DSP/Utils/Filters.h"
#include "DSP/Utils/Smoothers.h"

namespace aetherfall
{

/**
    Input Stage (spec section 3): conditions the signal that feeds the FX chain
    and the ATMOS CORE (the dry signal of the final dry/wet is untouched).
      gain -> high-pass (20 Hz = off) -> low-pass (20 kHz = off)
           -> bass mono (Linkwitz-Riley crossover, 0 = off) -> pre-drive
    Filters switched "off" are crossfaded out, so they add no phase shift.
*/
class InputStage final : public ModuleBase
{
public:
    void reset() override;
    void process (juce::AudioBuffer<float>&, const ModContext&) override;

protected:
    void prepareModule() override;

private:
    std::array<dsp::SVF<float>, 2> hpf, lpf;
    std::array<std::array<dsp::Biquad<float>, 2>, 2> xLow, xHigh;   // [channel][section]
    float hpfHz = 0.0f, lpfHz = 0.0f, xoverHz = 0.0f;
    dsp::LinearSmoother<float> gain, hpfAmt, lpfAmt, monoAmt, driveAmt;
    bool primed = false;
};

} // namespace aetherfall
