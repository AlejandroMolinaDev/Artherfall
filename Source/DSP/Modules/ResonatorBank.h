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
    Resonator Bank (module 7).
    4..8 tuned string-like resonators (feedback combs with damping), each one
    tuned to a note of a chord built on the global Key: anything that excites
    the bank (noise, drums, voices) sings the chord. Decay sets the ring time,
    Bright the damping, Detune a slow inharmonic drift per voice, Spread the
    stereo placement and Drive the excitation saturation.
*/
class ResonatorBank final : public ModuleBase
{
public:
    ModuleType getType() const override { return ModuleType::Resonator; }
    void reset() override;
    void process (juce::AudioBuffer<float>&, const ModContext&) override;
    double getTailSeconds() const override { return tail; }

    static constexpr int maxVoices = 8;

    /** Chord intervals (semitones above the root) for each "Reso Chord" option. */
    static const std::array<std::array<int, maxVoices>, 9>& chordTable() noexcept;

protected:
    void prepareModule() override;

private:
    struct Voice
    {
        std::array<dsp::DelayLine<float>, 2> line;
        std::array<dsp::OnePole<float>, 2> damp;
        double logFreq = 0.0;
        double driftPhase = 0.0;
        float staticDetune = 0.0f;   // cents
        dsp::LinearSmoother<float> gain;
    };

    std::array<Voice, maxVoices> voices;
    std::array<dsp::DCBlocker<float>, 2> dc;
    dsp::LinearSmoother<float> decay, bright, spread, drive;
    double glide = 0.0;
    double tail = 2.0;
    bool primed = false;
};

} // namespace aetherfall
