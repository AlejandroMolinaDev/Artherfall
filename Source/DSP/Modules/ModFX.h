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
    Phaser / Flanger / Chorus (module 10).
    - Phaser: 2..12 first-order all-pass stages swept by the LFO, with feedback.
      The notches appear when the slot mixes this with the dry signal (mix 50 %).
    - Flanger: short modulated delay (0.5..8 ms) with +- feedback.
    - Chorus: an ensemble of up to 6 modulated voices (8..30 ms), alternately
      panned for a wide image.
    "Width" offsets the LFO phase of the right channel (up to 180 degrees).
*/
class ModFX final : public ModuleBase
{
public:
    ModuleType getType() const override { return ModuleType::ModFX; }
    void reset() override;
    void process (juce::AudioBuffer<float>&, const ModContext&) override;

protected:
    void prepareModule() override;

private:
    static constexpr int maxStages = 12;
    static constexpr int maxVoices = 6;

    std::array<std::array<dsp::Allpass1<float>, maxStages>, 2> stages;
    std::array<dsp::DelayLine<float>, 2> lines;
    std::array<float, 2> fbState {}, phaserCoeff {};
    dsp::TempoPhase lfo;
    dsp::LinearSmoother<float> depth, feedback, center, width;
    int lastMode = -1;
    bool primed = false;
};

} // namespace aetherfall
