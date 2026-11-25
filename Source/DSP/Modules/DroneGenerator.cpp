#include "DroneGenerator.h"
#include "DSP/Utils/DspMath.h"
#include "DSP/Utils/MusicTheory.h"

namespace aetherfall
{

using namespace dsp;

namespace
{
    // Voicings (semitones above the root), index-compatible with choices::droneVoicing.
    struct Voicing { int count; int notes[DroneGenerator::maxVoicingNotes]; };
    constexpr Voicing voicings[] = {
        { 1, { 0 } },               // Unison
        { 2, { 0, 7 } },            // Fifth
        { 2, { 0, 12 } },           // Octave
        { 3, { 0, 3, 7 } },         // Minor
        { 3, { 0, 2, 7 } },         // Sus2
        { 5, { 0, 3, 7, 10, 14 } }, // Minor 9
    };

    /** PolyBLEP residual: removes the aliasing of the saw's discontinuity. */
    inline double polyBlep (double t, double dt) noexcept
    {
        if (t < dt)       { t /= dt; return t + t - t * t - 1.0; }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
        return 0.0;
    }
}

void DroneGenerator::prepareModule()
{
    tracker.prepare (sampleRate);
    envAtk = (float) std::exp (-1.0 / (0.02 * sampleRate));
    envRel = (float) std::exp (-1.0 / (0.6 * sampleRate));
    for (auto* s : { &wave, &noise, &follow, &detune })
        s->prepare (sampleRate, 0.05);
    reset();
}

void DroneGenerator::reset()
{
    tracker.reset();
    for (size_t i = 0; i < oscPhase.size(); ++i)
        oscPhase[i] = 0.173 * (double) i;   // spread start phases
    for (auto& f : filter) f.reset();
    pinkState = { 0.0f, 0.0f };
    hasNote = false;
    confidentFrames = 0;
    env = 0.0f;
    onsetGain = 0.0f;
    primed = false;
}

void DroneGenerator::process (juce::AudioBuffer<float>& buffer, const ModContext& ctx)
{
    const int n = std::min (ctx.numSamples, buffer.getNumSamples());
    if (n <= 0)
        return;

    float* x[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };

    // --- Pitch tracking + envelope on the (mono) input.
    for (int i = 0; i < n; ++i)
    {
        const float m = 0.5f * (x[0][i] + x[1][i]);
        tracker.push (m);
        const float lvl = std::abs (m);
        env = lvl + (lvl > env ? envAtk : envRel) * (env - lvl);
    }

    const bool hold = ctx.getBool (PID::DroneHold);
    const float hz = tracker.getFrequency();

    if (! hold && hz > 0.0f && tracker.getConfidence() > 0.75f)
    {
        // Require a few consecutive confident chunks before moving (avoids
        // jumping on noisy attacks).
        if (++confidentFrames >= 3)
        {
            double note = hzToNote ((double) hz) + 12.0 * (ctx.getChoice (PID::DroneOctave) - 2);
            if (ctx.getBool (PID::DroneQuantize))
                note = music::quantizeToScale (note, ctx.getChoice (PID::GlobKey), ctx.getChoice (PID::GlobScale));
            else
                note = std::round (note);

            targetNote = clamp (note, 12.0, 108.0);
            if (! hasNote)
            {
                currentNote = targetNote;
                hasNote = true;
            }
        }
    }
    else
    {
        confidentFrames = 0;
    }

    const auto& voicing = voicings[(size_t) clamp (ctx.getChoice (PID::DroneVoicing), 0, 5)];

    if (! primed)
    {
        wave.setCurrentAndTarget (ctx.get (PID::DroneWave));
        noise.setCurrentAndTarget (ctx.get (PID::DroneNoise));
        follow.setCurrentAndTarget (ctx.get (PID::DroneFollow));
        detune.setCurrentAndTarget (ctx.get (PID::DroneDetune));
        logCutoff = std::log (std::max (20.0f, ctx.get (PID::DroneCutoff)));
        primed = true;
    }
    wave.setTarget (clamp (ctx.get (PID::DroneWave), 0.0f, 1.0f));
    noise.setTarget (clamp (ctx.get (PID::DroneNoise), 0.0f, 1.0f));
    follow.setTarget (clamp (ctx.get (PID::DroneFollow), 0.0f, 1.0f));
    detune.setTarget (clamp (ctx.get (PID::DroneDetune), 0.0f, 100.0f));

    const double glideCoeff = std::exp (-1.0 / (std::max (0.005, ctx.get (PID::DroneGlide) * 0.001) * sampleRate));
    const double cutTarget = std::log (clamp ((double) ctx.get (PID::DroneCutoff), 20.0, 18000.0));
    const double cutCoeff = std::exp (-1.0 / (0.01 * sampleRate));
    const float res = clamp (ctx.get (PID::DroneRes), 0.0f, 0.98f);

    // Fade in when the first note is captured.
    const float onsetTarget = hasNote ? 1.0f : 0.0f;
    const float onsetCoeff = (float) std::exp (-1.0 / (0.3 * sampleRate));
    const int numOsc = voicing.count * oscPerNote;
    const float level = 0.35f / std::sqrt ((float) numOsc);

    // Control rate: glide the note over the chunk and compute every oscillator
    // increment once (no exp/pow per sample).
    currentNote = targetNote + std::pow (glideCoeff, (double) n) * (currentNote - targetNote);
    const float dtCents = detune.getTarget();
    std::array<double, maxVoicingNotes * oscPerNote> inc {};
    for (int v = 0; v < voicing.count; ++v)
        for (int k = 0; k < oscPerNote; ++k)
        {
            const double cents = (k == 0 ? -1.0 : 1.0) * dtCents * 0.5;
            inc[(size_t) (v * oscPerNote + k)] = std::min (0.45, noteToHz (currentNote + voicing.notes[v] + cents / 100.0) / sampleRate);
        }
    detune.skip (n);

    for (int i = 0; i < n; ++i)
    {
        logCutoff = cutTarget + cutCoeff * (logCutoff - cutTarget);
        onsetGain = onsetTarget + onsetCoeff * (onsetGain - onsetTarget);

        const float wv = wave.next(), nz = noise.next(), fl = follow.next();

        float outL = 0.0f, outR = 0.0f;
        for (int o = 0; o < numOsc; ++o)
        {
            double& ph = oscPhase[(size_t) o];
            const double dt = inc[(size_t) o];
            ph += dt;
            if (ph >= 1.0) ph -= 1.0;

            const float sine = sinCycles (ph);
            const float saw = (float) (2.0 * ph - 1.0 - polyBlep (ph, dt));
            const float s = sine + wv * (saw - sine);
            if ((o & 1) == 0) outL += s; else outR += s;
        }

        // Noise layer (pink-ish, one-pole each side).
        pinkState[0] = 0.97f * pinkState[0] + 0.03f * rng.nextBipolar() * 6.0f;
        pinkState[1] = 0.97f * pinkState[1] + 0.03f * rng.nextBipolar() * 6.0f;

        const float amp = level * onsetGain * (1.0f + fl * (std::min (1.0f, env * 6.0f) - 1.0f));

        float sides[2] = { outL + nz * pinkState[0] * 2.0f, outR + nz * pinkState[1] * 2.0f };
        for (int c = 0; c < 2; ++c)
        {
            if ((i & 15) == 0)
                filter[(size_t) c].setParams ((float) std::exp (logCutoff), res, (float) sampleRate);
            x[c][i] = amp * filter[(size_t) c].processLP (sides[c]);
        }
    }
}

} // namespace aetherfall
