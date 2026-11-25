#pragma once

#include <array>
#include "DSP/Chain/IModule.h"
#include "DSP/Utils/Smoothers.h"

namespace aetherfall
{

/**
    Ring Mod / Frequency Shifter (module 13).
    - Ring: multiplies by a sine carrier (sum and difference frequencies:
      metallic, inharmonic). "Stereo" offsets the carrier phase of R.
    - Freq Shift: single-sideband shift via an IIR Hilbert transformer (two
      all-pass chains 90 degrees apart). Every partial moves by the same number
      of Hz, which detunes harmonics against each other. Feedback re-shifts the
      output (barber-pole spirals). "Stereo" turns R into the opposite shift.
*/
class RingFreq final : public ModuleBase
{
public:
    ModuleType getType() const override { return ModuleType::RingFreq; }
    void reset() override;
    void process (juce::AudioBuffer<float>&, const ModContext&) override;

protected:
    void prepareModule() override;

private:
    /** Olli Niemitalo's 8-allpass Hilbert pair (90 deg over ~15 Hz..20 kHz). */
    struct Hilbert
    {
        struct AP { float x1 = 0, x2 = 0, y1 = 0, y2 = 0; };
        std::array<AP, 4> a, b;
        float bDelay = 0.0f;
        void reset() noexcept { a = {}; b = {}; bDelay = 0.0f; }
        void process (float x, float& re, float& im) noexcept;
    };

    std::array<Hilbert, 2> hilbert;
    std::array<double, 2> phase {};
    std::array<float, 2> lastOut {};
    dsp::LinearSmoother<float> feedback, stereo;
    double logFreq = 0.0, shiftHz = 0.0;
    bool primed = false;
};

} // namespace aetherfall
