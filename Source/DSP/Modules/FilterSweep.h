#pragma once

#include <array>
#include "DSP/Chain/IModule.h"
#include "DSP/Utils/Filters.h"

namespace aetherfall
{

/**
    Filter Sweep (module 9): multimode LP / HP / BP / Notch / Formant,
    12 or 24 dB/oct, SVF (clean, TPT) or Ladder (4-pole, tanh feedback,
    self-oscillates at full resonance), pre-drive and stereo cutoff offset.
*/
class FilterSweep final : public ModuleBase
{
public:
    ModuleType getType() const override { return ModuleType::Filter; }
    void reset() override;
    void process (juce::AudioBuffer<float>&, const ModContext&) override;

protected:
    void prepareModule() override;

private:
    /** Zero-delay-feedback 4-pole ladder (Zavalishin, cheap tanh nonlinearity). */
    struct Ladder
    {
        std::array<float, 4> s {};
        float G = 0.0f, g = 0.0f;

        void setCutoff (float hz, float sr) noexcept;
        void reset() noexcept { s = {}; }
        /** Returns stage outputs y1..y4 and the (saturated) loop input u. */
        void process (float x, float k, float& u, std::array<float, 4>& y) noexcept;
    };

    struct Channel
    {
        dsp::SVF<float> svf1, svf2;
        Ladder ladder;
        std::array<dsp::SVF<float>, 3> formant;
        float logCutoff = 0.0f;   // smoothed log2(cutoff)
    };

    float processSample (Channel& c, float x, int mode, bool steep, bool ladder, float res, float k) noexcept;
    void updateCoefficients (Channel& c, float cutoffHz, int mode, bool steep, bool ladder, float res, float vowel) noexcept;

    std::array<Channel, 2> ch;
    float smoothCoeff = 0.0f;
    bool primed = false;
};

} // namespace aetherfall
