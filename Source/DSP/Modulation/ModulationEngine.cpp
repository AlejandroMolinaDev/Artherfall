#include "ModulationEngine.h"
#include "Parameters/ParamLayout.h"

namespace aetherfall
{

ModulationEngine::ModulationEngine()
{
    routing.write (routing::makeDefault());
}

void ModulationEngine::prepare (double sr)
{
    sampleRate = sr;
    ranges.clear();
    ranges.reserve ((size_t) numParams);
    for (const auto& p : paramInfos)
        ranges.push_back (makeRange (p));
    analyzer.prepare (sr);
    reset();
}

void ModulationEngine::reset()
{
    for (int i = 0; i < numLfos; ++i)
        lfos[(size_t) i].reset (0.0, 0x1234u + (std::uint32_t) i * 977u);
    for (auto& e : envs) e.reset();
    for (auto& s : seqs) s.reset();
    for (int i = 0; i < numChaos; ++i)
        chaos[(size_t) i].reset (0xC4A05u + (std::uint32_t) i * 131u);
    analyzer.reset();

    values.fill (0.0f);
    offsets.fill (0.0f);
    numTouched = 0;
    heldNote = -1;
    velocity = modWheel = bend = 0.0f;
    noteOnFlag = false;
    notesDown.fill (false);
}

void ModulationEngine::noteOn (int note, float vel) noexcept
{
    if (note < 0 || note > 127) return;
    notesDown[(size_t) note] = true;
    heldNote = note;
    velocity = vel;
    noteOnFlag = true;
}

void ModulationEngine::noteOff (int note) noexcept
{
    if (note < 0 || note > 127) return;
    notesDown[(size_t) note] = false;
    // Keep the last note value (sustained drones keep their modulation).
}

void ModulationEngine::controller (int number, float value01) noexcept
{
    if (number == 1)
        modWheel = value01;
}

void ModulationEngine::pitchBend (float b) noexcept { bend = b; }

void ModulationEngine::process (const float* base, float* effective, const ChunkInput& in) noexcept
{
    const auto& r = routing.read();
    const int n = in.numSamples;
    const double sr = in.sampleRate;
    const auto& t = in.transport;

    // Modulator settings come from the previous chunk's effective values.
    const float* prev = effective;

    // --- Analyzer.
    // Pitch tracking (YIN) only runs when something uses it.
    bool needsPitch = base[idx (PID::CombTrack)] >= 0.5f;
    for (const auto& slot : r.slots)
        needsPitch = needsPitch || (slot.dest >= 0 && (slot.source == ModSource::AnalyzerPitch || slot.via == ModSource::AnalyzerPitch));
    if (in.inL != nullptr && in.inR != nullptr)
        analyzer.process (in.inL, in.inR, n, needsPitch);
    const bool onset = analyzer.hadOnset();

    // --- LFOs.
    for (int k = 0; k < numLfos; ++k)
    {
        mod::Lfo::Settings s;
        s.shape       = (int) std::lround (prev[idx (lfoParam (k, 0))]);
        s.rateHz      = prev[idx (lfoParam (k, 1))];
        s.sync        = prev[idx (lfoParam (k, 2))] >= 0.5f;
        s.division    = (int) std::lround (prev[idx (lfoParam (k, 3))]);
        s.phaseOffset = prev[idx (lfoParam (k, 4))];
        s.trigger     = (int) std::lround (prev[idx (lfoParam (k, 5))]);
        s.unipolar    = std::lround (prev[idx (lfoParam (k, 6))]) == 1;
        s.smooth      = prev[idx (lfoParam (k, 7))];

        const auto id = (size_t) ModSource::Lfo1 + (size_t) k;
        values[id] = lfos[(size_t) k].process (s, t, sr, n, onset, noteOnFlag, r.lfoCustom[(size_t) k]);
        bipolar[id] = ! s.unipolar;
    }

    // --- Envelope followers.
    for (int k = 0; k < numEnvFollowers; ++k)
    {
        const bool wet = std::lround (prev[idx (envParam (k, 2))]) == 1;
        const auto id = (size_t) ModSource::Env1 + (size_t) k;
        values[id] = envs[(size_t) k].process (wet ? in.wetL : in.inL, wet ? in.wetR : in.inR, n, sr,
                                               prev[idx (envParam (k, 0))], prev[idx (envParam (k, 1))],
                                               prev[idx (envParam (k, 3))]);
        bipolar[id] = false;
    }

    // --- Step sequencers.
    for (int k = 0; k < numStepSeqs; ++k)
    {
        const bool bip = std::lround (prev[idx (seqParam (k, 3))]) == 0;
        const auto id = (size_t) ModSource::Seq1 + (size_t) k;
        values[id] = seqs[(size_t) k].process (r.seqSteps[(size_t) k], (int) std::lround (prev[idx (seqParam (k, 1))]),
                                               (int) std::lround (prev[idx (seqParam (k, 0))]),
                                               prev[idx (seqParam (k, 2))], bip, t, sr, n);
        bipolar[id] = bip;
    }

    // --- Chaos.
    for (int k = 0; k < numChaos; ++k)
    {
        const auto id = (size_t) ModSource::Chaos1 + (size_t) k;
        values[id] = chaos[(size_t) k].process ((int) std::lround (prev[idx (chaosParam (k, 0))]),
                                                prev[idx (chaosParam (k, 1))], prev[idx (chaosParam (k, 2))], sr, n);
        bipolar[id] = true;
    }

    // --- Macros (host values, never modulated themselves).
    for (int k = 0; k < numMacros; ++k)
    {
        const auto id = (size_t) ModSource::Macro1 + (size_t) k;
        values[id] = base[idx (macroParam (k))];
        bipolar[id] = false;
    }

    // --- Analyzer and MIDI sources.
    values[(size_t) ModSource::AnalyzerRms]       = analyzer.rmsNormalised();
    values[(size_t) ModSource::AnalyzerTransient] = analyzer.getFrame().transient;
    values[(size_t) ModSource::AnalyzerPitch]     = analyzer.pitchNormalised();
    values[(size_t) ModSource::AnalyzerCentroid]  = analyzer.centroidNormalised();
    values[(size_t) ModSource::MidiNote]          = heldNote >= 0 ? (float) heldNote / 127.0f : 0.0f;
    values[(size_t) ModSource::MidiVelocity]      = velocity;
    values[(size_t) ModSource::MidiModWheel]      = modWheel;
    values[(size_t) ModSource::MidiPitchBend]     = bend;
    bipolar[(size_t) ModSource::MidiPitchBend]    = true;
    noteOnFlag = false;

    for (size_t s = 0; s < values.size(); ++s)
        sourceValues[s].store (values[s], std::memory_order_relaxed);

    // --- Accumulate offsets (normalised domain).
    for (int i = 0; i < numTouched; ++i)
    {
        offsets[(size_t) touched[(size_t) i]] = 0.0f;
        guiOffsets[(size_t) touched[(size_t) i]].store (0.0f, std::memory_order_relaxed);
    }
    numTouched = 0;

    auto addOffset = [this] (int dest, float amount)
    {
        if (dest < 0 || dest >= numParams || ! paramInfos[(size_t) dest].isModulatable())
            return;
        if (dsp::exactlyEqual (offsets[(size_t) dest], 0.0f) && numTouched < (int) touched.size())
        {
            bool already = false;
            for (int i = 0; i < numTouched && ! already; ++i)
                already = touched[(size_t) i] == dest;
            if (! already)
                touched[(size_t) numTouched++] = (std::int16_t) dest;
        }
        offsets[(size_t) dest] += amount;
    };

    for (int m = 0; m < numMacros; ++m)
    {
        const float mv = values[(size_t) ModSource::Macro1 + (size_t) m];
        if (mv <= 0.0f)
            continue;
        for (const auto& target : r.macros[(size_t) m])
            if (target.dest >= 0 && ! dsp::exactlyEqual (target.amount, 0.0f))
                addOffset (target.dest, mv * target.amount);
    }

    // Effective global mod depth (may itself be driven by DRIFT).
    const auto depthIdx = idx (PID::GlobModDepth);
    const auto& depthRange = ranges[depthIdx];
    const float depth = depthRange.convertFrom0to1 (dsp::clamp (depthRange.convertTo0to1 (base[depthIdx]) + offsets[depthIdx], 0.0f, 1.0f));

    for (int sIdx = 0; sIdx < numModSlots; ++sIdx)
    {
        const auto& slot = r.slots[(size_t) sIdx];
        if (slot.source == ModSource::None || slot.dest < 0 || slot.dest == (std::int16_t) depthIdx)
            continue;

        const float amount = base[idx (modAmountParam (sIdx))];
        if (dsp::exactlyEqual (amount, 0.0f))
            continue;

        float v = applyCurve (slot.curve, src (slot.source));
        if (slot.via != ModSource::None)
        {
            const float via = src (slot.via);
            v *= bipolar[(size_t) slot.via] ? 0.5f * (via + 1.0f) : via;
        }
        if (isScaledByModDepth (slot.source))
            v *= depth;

        addOffset (slot.dest, amount * v);
    }

    // --- Effective values.
    std::copy (base, base + numParams, effective);
    for (int i = 0; i < numTouched; ++i)
    {
        const auto d = (size_t) touched[(size_t) i];
        const auto& range = ranges[d];
        const float baseNorm = range.convertTo0to1 (base[d]);
        const float norm = dsp::clamp (baseNorm + offsets[d], 0.0f, 1.0f);
        effective[d] = range.convertFrom0to1 (norm);
        guiOffsets[d].store (norm - baseNorm, std::memory_order_relaxed);
    }
}

} // namespace aetherfall
