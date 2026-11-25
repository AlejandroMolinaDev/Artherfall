#pragma once

#include "DSP/Chain/IModule.h"

namespace aetherfall
{

/**
    Gate / Trance Gate (module 15).
    A 16-step on/off pattern (stored in the plug-in state, see
    AetherfallProcessor::setGatePattern) clocked by the host tempo, with swing,
    smoothing (attack/release of each step) and depth. While the host plays
    the steps are locked to the song position; when stopped the gate keeps
    running at the current tempo.
*/
class TranceGate final : public ModuleBase
{
public:
    ModuleType getType() const override { return ModuleType::Gate; }
    void reset() override;
    void process (juce::AudioBuffer<float>&, const ModContext&) override;

    /** Default pattern: x.xx.xx.x.xx.xx. (bit 0 = step 1). */
    static constexpr std::uint32_t defaultPattern = 0b0110110101101101u;

protected:
    void prepareModule() override;

private:
    double position = 0.0;   // in steps (free-running clock)
    float gain = 1.0f;
};

} // namespace aetherfall
