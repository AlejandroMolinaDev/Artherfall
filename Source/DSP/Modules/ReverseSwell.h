#pragma once

#include <array>
#include "DSP/Chain/IModule.h"
#include "DSP/Utils/DelayLine.h"
#include "DSP/Utils/Smoothers.h"

namespace aetherfall
{

/**
    Reverse Swell (module 16).
    Live capture played backwards: two read heads sweep the recent audio in
    reverse (delay growing at 2 samples per sample = reversed playback at the
    original pitch), half a segment apart and crossfaded, so the reversed
    stream never clicks. "Xfade" sharpens or softens the crossfade windows.
    Swell mode adds a rising envelope to every reversed segment (the "suck"
    into the next phrase). Feedback re-captures the output.
*/
class ReverseSwell final : public ModuleBase
{
public:
    ModuleType getType() const override { return ModuleType::ReverseSwell; }
    void reset() override;
    void process (juce::AudioBuffer<float>&, const ModContext&) override;
    double getTailSeconds() const override { return segmentSeconds * 2.0; }

    static constexpr double maxSegmentSeconds = 6.0;

protected:
    void prepareModule() override;

private:
    std::array<dsp::DelayLine<float>, 2> lines;
    double phase = 0.0;            // 0..1 over one head period
    double segmentSamples = 48000.0;
    double segmentSeconds = 1.0;
    std::array<float, 2> lastOut {};
    dsp::LinearSmoother<float> feedback, sharpness;
    bool primed = false;
};

} // namespace aetherfall
