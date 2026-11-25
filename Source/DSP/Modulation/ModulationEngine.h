#pragma once

#include <array>
#include <atomic>
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>

#include "ModRouting.h"
#include "ModulatorSources.h"
#include "DSP/Analysis/InputAnalyzer.h"
#include "DSP/Utils/TripleBuffer.h"

namespace aetherfall
{

/**
    Evaluates every modulation source once per control chunk and turns the
    host parameter values into the EFFECTIVE values the DSP uses:

        effective = denormalise( normalise(host value)
                                 + sum over macros  (macro * amount)
                                 + sum over matrix  (amount * curve(source) * via * depth) )

    - Destinations are modulated in the normalised (0..1) domain of each
      parameter, so the same amount means "the same knob travel" everywhere.
    - "Mod Depth" (glob_moddepth, the DRIFT target) scales LFO / envelope /
      sequencer / chaos / analyzer sources; macros and MIDI are direct.
    - Modulator settings (LFO rate...) are read from the previous chunk's
      effective values, so modulators can modulate each other (one chunk
      of latency, inaudible at a 64-sample control rate).
*/
class ModulationEngine
{
public:
    struct ChunkInput
    {
        const float* inL = nullptr;    // dry input of this chunk
        const float* inR = nullptr;
        const float* wetL = nullptr;   // wet output of the previous chunk
        const float* wetR = nullptr;
        int numSamples = 0;
        double sampleRate = 48000.0;
        TransportState transport;
    };

    ModulationEngine();

    void prepare (double sampleRate);
    void reset();

    /** Message thread: publishes a new routing (lock-free for the audio thread). */
    void setRouting (const ModRouting& r) noexcept { routing.write (r); }

    /** MIDI events (audio thread, before process()). */
    void noteOn (int note, float velocity) noexcept;
    void noteOff (int note) noexcept;
    void controller (int number, float value01) noexcept;
    void pitchBend (float bipolar) noexcept;

    /** Computes `effective` from `base` (both indexed by PID). */
    void process (const float* base, float* effective, const ChunkInput& in) noexcept;

    const AnalysisFrame& getAnalysis() const noexcept { return analyzer.getFrame(); }

    /** Last value of each source (for the GUI, any thread). */
    float getSourceValue (ModSource s) const noexcept
    {
        return sourceValues[(size_t) s].load (std::memory_order_relaxed);
    }

private:
    float src (ModSource s) const noexcept { return values[(size_t) s]; }

    dsp::TripleBuffer<ModRouting> routing;
    std::vector<juce::NormalisableRange<float>> ranges;

    std::array<mod::Lfo, (size_t) numLfos> lfos;
    std::array<mod::EnvFollower, (size_t) numEnvFollowers> envs;
    std::array<mod::StepSeq, (size_t) numStepSeqs> seqs;
    std::array<mod::Chaos, (size_t) numChaos> chaos;
    InputAnalyzer analyzer;

    std::array<float, (size_t) ModSource::Count> values {};
    std::array<bool, (size_t) ModSource::Count> bipolar {};
    std::array<std::atomic<float>, (size_t) ModSource::Count> sourceValues {};

    std::array<float, (size_t) numParams> offsets {};
    std::array<std::int16_t, (size_t) (numModSlots + numMacros * macroTargetsPerMacro)> touched {};
    int numTouched = 0;

    // MIDI state
    int heldNote = -1;
    float velocity = 0.0f, modWheel = 0.0f, bend = 0.0f;
    bool noteOnFlag = false;
    std::array<bool, 128> notesDown {};

    double sampleRate = 48000.0;
};

} // namespace aetherfall
