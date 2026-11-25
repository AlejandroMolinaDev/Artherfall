#pragma once

#include <array>
#include "DSP/Chain/IModule.h"
#include "DSP/Core/Diffuser.h"
#include "DSP/Utils/DelayLine.h"
#include "DSP/Utils/DCBlocker.h"
#include "DSP/Utils/Filters.h"
#include "DSP/Utils/Smoothers.h"

namespace aetherfall
{

/**
    Multi-Tap Diffuse Delay (module 5).
    Up to 8 taps spread over the delay span ("Shape" bends the spacing: taps
    bunch up early or late). Each tap runs through its own all-pass diffuser
    whose strength grows with the tap index, so the pattern goes from clear
    echoes to a cloud ("de eco a nube"). Taps alternate left/right inside
    "Width"; the last tap feeds back into the line.
*/
class MultiTap final : public ModuleBase
{
public:
    ModuleType getType() const override { return ModuleType::MultiTap; }
    void reset() override;
    void process (juce::AudioBuffer<float>&, const ModContext&) override;
    double getTailSeconds() const override { return tail; }

    static constexpr int maxTaps = 8;
    static constexpr double maxDelaySeconds = 8.0;

protected:
    void prepareModule() override;

private:
    dsp::DelayLine<float> line;
    std::array<dsp::DiffuserChain<float, 2>, maxTaps> diffusers;
    std::array<double, maxTaps> tapDelay {};
    std::array<double, maxTaps> diffuserDelay {};   // delay added by each tap diffuser (compensated)
    std::array<dsp::LinearSmoother<float>, maxTaps> tapGain;
    std::array<dsp::OnePole<float>, 2> tone;
    dsp::OnePole<float> fbTone;
    dsp::DCBlocker<float> dc;
    dsp::LinearSmoother<float> feedback, width;
    float lastFeedbackSample = 0.0f;
    float lastTone = -1.0f;
    double glide = 0.0;
    double tail = 0.0;
    bool primed = false;
};

} // namespace aetherfall
