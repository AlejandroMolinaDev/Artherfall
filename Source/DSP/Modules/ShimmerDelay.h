#pragma once

#include <array>
#include "DSP/Chain/IModule.h"
#include "DSP/Core/PitchShifter.h"
#include "DSP/Utils/DelayLine.h"
#include "DSP/Utils/DCBlocker.h"
#include "DSP/Utils/Filters.h"
#include "DSP/Utils/Smoothers.h"

namespace aetherfall
{

/**
    Shimmer Delay (module 3).
    Stereo delay whose feedback path is pitch-shifted (default +12): each
    repeat climbs an interval and fades into a shimmering cascade. Filters in
    the loop keep it from turning harsh or muddy; ping-pong bounces the
    repeats between the channels.
*/
class ShimmerDelay final : public ModuleBase
{
public:
    ModuleType getType() const override { return ModuleType::ShimmerDelay; }
    void reset() override;
    void process (juce::AudioBuffer<float>&, const ModContext&) override;
    double getTailSeconds() const override { return tail; }

    static constexpr double maxDelaySeconds = 8.0;

protected:
    void prepareModule() override;

private:
    struct Channel
    {
        dsp::DelayLine<float> line;
        dsp::PitchShifter<float> shifter;
        dsp::OnePole<float> lp, hp;
        dsp::DCBlocker<float> dc;
        float fbOut = 0.0f;
        double delay = 0.0;
    };

    std::array<Channel, 2> ch;
    dsp::LinearSmoother<float> feedback, shimmer;
    double glide = 0.0;
    float lastLP = -1.0f, lastHP = -1.0f, lastPitch = 1000.0f;
    double tail = 0.0;
    bool primed = false;
};

} // namespace aetherfall
