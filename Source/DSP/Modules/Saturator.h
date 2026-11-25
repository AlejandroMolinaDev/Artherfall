#pragma once

#include <array>
#include "DSP/Chain/IModule.h"
#include "DSP/Utils/Oversampler.h"
#include "DSP/Utils/DCBlocker.h"
#include "DSP/Utils/Filters.h"
#include "DSP/Utils/Smoothers.h"

namespace aetherfall
{

/**
    Saturator (module 11): Tape / Tube / Diode / Fold / Fuzz waveshapers with
    1x/2x/4x oversampling, bias (asymmetry -> even harmonics), tone tilt and
    RMS-matched auto gain.
*/
class Saturator final : public ModuleBase
{
public:
    ModuleType getType() const override { return ModuleType::Saturator; }
    void reset() override;
    void process (juce::AudioBuffer<float>&, const ModContext&) override;

    /** The static transfer curves (input already includes drive and bias). */
    static float shape (int mode, float x) noexcept;

protected:
    void prepareModule() override;

private:
    dsp::Oversampler oversampler;
    std::array<dsp::DCBlocker<float>, 2> dc;
    std::array<dsp::Biquad<float>, 2> toneLow, toneHigh;

    float prevDrive = 1.0f, prevBias = 0.0f;
    float lastTone = 1000.0f;
    float msIn = 0.0f, msOut = 0.0f, rmsCoeff = 0.0f;
    dsp::LinearSmoother<float> gainSmoother;
    bool primed = false;
};

} // namespace aetherfall
