#pragma once

#include <array>
#include "DSP/Chain/IModule.h"
#include "DSP/Utils/DelayLine.h"
#include "DSP/Utils/DCBlocker.h"
#include "DSP/Utils/Filters.h"
#include "DSP/Utils/Smoothers.h"

namespace aetherfall
{

/**
    Comb / Karplus Smear (module 8).
    A tuned feedback comb per channel (delay = 1 / frequency). Positive
    feedback rings at all harmonics, negative feedback only at odd ones
    (hollow, clarinet-like). Inside the loop: a damping low-pass (Karplus-Strong
    style decay of the highs) and an all-pass "smear" that disperses the
    harmonics into metallic, bell-like textures. The delay is slowly modulated
    and the right channel can be detuned ("Spread"). "Track" follows the pitch
    detected by the Input Analyzer (phase F5) when available.
*/
class Comb final : public ModuleBase
{
public:
    ModuleType getType() const override { return ModuleType::Comb; }
    void reset() override;
    void process (juce::AudioBuffer<float>&, const ModContext&) override;
    double getTailSeconds() const override { return 3.0; }

protected:
    void prepareModule() override;

private:
    struct Channel
    {
        dsp::DelayLine<float> line;
        dsp::OnePole<float> damp;
        std::array<dsp::Allpass1<float>, 4> smear;
        dsp::DCBlocker<float> dc;
        double logFreq = 0.0;
    };

    std::array<Channel, 2> ch;
    double modPhase = 0.0;
    dsp::LinearSmoother<float> feedback, dampAmt, smearAmt, outGain;
    double glide = 0.0;
    bool primed = false;
};

} // namespace aetherfall
