#pragma once

#include <array>
#include "DSP/Chain/IModule.h"
#include "DSP/Analysis/PitchTracker.h"
#include "DSP/Utils/Filters.h"
#include "DSP/Utils/Random.h"
#include "DSP/Utils/Smoothers.h"

namespace aetherfall
{

/**
    Drone Generator (module 6) — the "synthesised" heart of AETHERFALL.

    Tracks the pitch of its input (YIN), optionally snaps it to the global
    Key/Scale, and plays a sustained drone that glides to each new note:
    a voicing (unison, fifth, octave, minor, sus2, minor 9) of detuned
    sine->saw oscillators (PolyBLEP), plus filtered noise, through a resonant
    low-pass. The drone keeps sounding after the input stops (it only changes
    note when a new confident pitch arrives); "Follow" makes its level track
    the input envelope, "Hold" freezes the current note.
*/
class DroneGenerator final : public ModuleBase
{
public:
    ModuleType getType() const override { return ModuleType::Drone; }
    void reset() override;
    void process (juce::AudioBuffer<float>&, const ModContext&) override;
    double getTailSeconds() const override { return 1.0; }

    /** Current drone root (MIDI note, fractional while gliding); < 0 = none yet. */
    double getCurrentNote() const noexcept { return hasNote ? currentNote : -1.0; }

    static constexpr int maxVoicingNotes = 5;
    static constexpr int oscPerNote = 2;   // detuned pair, panned L/R

protected:
    void prepareModule() override;

private:
    PitchTracker tracker;
    std::array<double, maxVoicingNotes * oscPerNote> oscPhase {};
    std::array<dsp::SVF<float>, 2> filter;
    std::array<float, 2> pinkState {};
    dsp::Random32 rng { 0xD20Eu };

    double targetNote = 48.0, currentNote = 48.0;
    bool hasNote = false;
    int confidentFrames = 0;
    float env = 0.0f, envAtk = 0.0f, envRel = 0.0f;
    float onsetGain = 0.0f;

    dsp::LinearSmoother<float> wave, noise, follow, detune;
    double logCutoff = 0.0;
    bool primed = false;
};

} // namespace aetherfall
