#pragma once

#include <array>
#include "DSP/Chain/IModule.h"
#include "DSP/Utils/DelayLine.h"
#include "DSP/Utils/DCBlocker.h"
#include "DSP/Utils/Filters.h"
#include "DSP/Utils/Random.h"
#include "DSP/Utils/Smoothers.h"

namespace aetherfall
{

/**
    Tape Delay (module 4).

    A stereo delay that behaves like a tape loop:
      - delay-time changes glide (the "tape speed" changes -> pitch bends),
      - wow (slow, drifting) and flutter (fast) modulate the read head,
      - every repeat goes through the record head again: saturation and
        "age" filtering (losing highs and lows) accumulate per repeat,
      - feedback above 100 % self-oscillates but the record-head saturation
        keeps it bounded.
    Sync: 1/64 .. 16 bars (incl. triplets/dotted), clamped to 8 s.
*/
class TapeDelay final : public ModuleBase
{
public:
    ModuleType getType() const override { return ModuleType::TapeDelay; }
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
        dsp::OnePole<float> ageLP, ageHP;
        dsp::DCBlocker<float> dc;
        double delaySamples = 0.0;    // smoothed (tape glide)
    };

    std::array<Channel, 2> ch;
    dsp::Random32 rng { 0x51u };

    double wowPhase = 0.0, flutterPhase = 0.0, flutterPhase2 = 0.0;
    double wowDrift = 0.0, wowDriftTarget = 0.0;
    int driftCounter = 0;
    double glideCoeff = 0.0;

    dsp::LinearSmoother<float> feedback, wow, flutter, sat;
    float lastAge = -1.0f;
    double tail = 0.0;
    bool primed = false;
};

} // namespace aetherfall
