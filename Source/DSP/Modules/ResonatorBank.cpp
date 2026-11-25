#include "ResonatorBank.h"
#include "DSP/Utils/DspMath.h"

namespace aetherfall
{

using namespace dsp;

const std::array<std::array<int, ResonatorBank::maxVoices>, 9>& ResonatorBank::chordTable() noexcept
{
    static const std::array<std::array<int, maxVoices>, 9> table {{
        { 0, 3, 7, 12, 15, 19, 24, 27 },     // Minor
        { 0, 3, 7, 10, 12, 15, 19, 22 },     // Minor 7
        { 0, 3, 7, 10, 14, 19, 22, 26 },     // Minor 9
        { 0, 4, 7, 12, 16, 19, 24, 28 },     // Major
        { 0, 4, 7, 11, 12, 16, 19, 23 },     // Major 7
        { 0, 2, 7, 12, 14, 19, 24, 26 },     // Sus2
        { 0, 5, 7, 12, 17, 19, 24, 29 },     // Sus4
        { 0, 7, 12, 19, 24, 31, 36, 43 },    // Fifths
        { -12, 0, 12, 24, 36, 0, 12, 24 },   // Octaves (doubled voices detune apart)
    }};
    return table;
}

void ResonatorBank::prepareModule()
{
    const int maxPeriod = (int) std::ceil (sampleRate / 25.0) + 8;   // lowest note ~25 Hz
    for (int v = 0; v < maxVoices; ++v)
    {
        auto& voice = voices[(size_t) v];
        for (auto& l : voice.line) l.prepare (maxPeriod);
        voice.gain.prepare (sampleRate, 0.05);
        // Fixed per-voice detune pattern (cents), scaled by "Detune".
        voice.staticDetune = (float) ((v % 2 == 0 ? 1 : -1) * (3 + 2 * v));
        voice.driftPhase = 0.137 * v;
    }
    for (auto& d : dc) d.prepare (sampleRate, 10.0);
    for (auto* s : { &decay, &bright, &spread, &drive })
        s->prepare (sampleRate, 0.05);
    glide = std::exp (-1.0 / (0.03 * sampleRate));
    reset();
}

void ResonatorBank::reset()
{
    for (auto& v : voices)
    {
        for (auto& l : v.line) l.reset();
        for (auto& d : v.damp) d.reset();
    }
    for (auto& d : dc) d.reset();
    primed = false;
}

void ResonatorBank::process (juce::AudioBuffer<float>& buffer, const ModContext& ctx)
{
    const int n = std::min (ctx.numSamples, buffer.getNumSamples());
    if (n <= 0)
        return;

    const int chord = clamp (ctx.getChoice (PID::ResoChord), 0, 8);
    const int octave = clamp (ctx.getChoice (PID::ResoOctave), 0, 4) - 2;
    const int numVoices = clamp ((int) std::lround (ctx.get (PID::ResoVoices)), 4, maxVoices);
    const float detuneAmt = clamp (ctx.get (PID::ResoDetune), 0.0f, 1.0f);

    // Root: global key at octave 3 (C3 = MIDI 48), shifted by "Reso Octave".
    const int key = clamp (ctx.getChoice (PID::GlobKey), 0, 11);
    const double rootNote = 48.0 + key + 12.0 * octave;
    const auto& intervals = chordTable()[(size_t) chord];

    std::array<double, maxVoices> targetLog {};
    for (int v = 0; v < maxVoices; ++v)
    {
        const double note = rootNote + intervals[(size_t) v];
        const double hz = clamp (noteToHz (note), 25.0, 8000.0);
        targetLog[(size_t) v] = std::log (hz);
        voices[(size_t) v].gain.setTarget (v < numVoices ? 1.0f : 0.0f);
    }

    if (! primed)
    {
        for (int v = 0; v < maxVoices; ++v)
        {
            voices[(size_t) v].logFreq = targetLog[(size_t) v];
            voices[(size_t) v].gain.setCurrentAndTarget (v < numVoices ? 1.0f : 0.0f);
        }
        decay.setCurrentAndTarget (ctx.get (PID::ResoDecay));
        bright.setCurrentAndTarget (ctx.get (PID::ResoBright));
        spread.setCurrentAndTarget (ctx.get (PID::ResoSpread));
        drive.setCurrentAndTarget (ctx.get (PID::ResoDrive));
        primed = true;
    }
    decay.setTarget (clamp (ctx.get (PID::ResoDecay), 0.05f, 10.0f));
    bright.setTarget (clamp (ctx.get (PID::ResoBright), 0.0f, 1.0f));
    spread.setTarget (clamp (ctx.get (PID::ResoSpread), 0.0f, 1.0f));
    drive.setTarget (clamp (ctx.get (PID::ResoDrive), 0.0f, 1.0f));
    tail = decay.getTarget() + 0.2;

    // --- Control rate: per-voice period, loop gain and pan (the per-sample loop
    //     below only reads delay lines). Glide and drift move slowly, so one
    //     update per chunk is inaudible and saves pow/exp per sample.
    const double chunkGlide = std::pow (glide, (double) n);
    const double driftInc = 0.07 / sampleRate * n;
    const float t60 = decay.getTarget();
    std::array<float, maxVoices> period {}, fbGain {}, gainL {}, gainR {};
    for (int v = 0; v < maxVoices; ++v)
    {
        auto& voice = voices[(size_t) v];
        voice.logFreq = targetLog[(size_t) v] + chunkGlide * (voice.logFreq - targetLog[(size_t) v]);
        voice.driftPhase += driftInc * (1.0 + 0.3 * v);
        voice.driftPhase -= std::floor (voice.driftPhase);

        const double cents = detuneAmt * (voice.staticDetune + 6.0 * std::sin (twoPi * voice.driftPhase));
        const double hz = std::exp (voice.logFreq) * std::exp2 (cents / 1200.0);
        period[(size_t) v] = (float) std::max (2.0, sampleRate / hz - 1.0);
        // Loop gain for a 60 dB decay in t60 seconds: g = 10^(-3 / (f * t60)).
        fbGain[(size_t) v] = (float) std::pow (10.0, -3.0 / (hz * std::max (0.05f, t60)));

        const float sp = spread.getTarget();
        const float pan = ((v & 1) ? 1.0f : -1.0f) * sp * (0.4f + 0.6f * (float) v / maxVoices);
        const float theta = (pan + 1.0f) * (float) (pi * 0.25);
        gainL[(size_t) v] = std::cos (theta) * 1.41421356f;
        gainR[(size_t) v] = std::sin (theta) * 1.41421356f;
    }
    decay.skip (n);
    spread.skip (n);

    const float norm = 1.6f / std::sqrt ((float) numVoices);
    float* x[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };

    for (int i = 0; i < n; ++i)
    {
        const float br = bright.next();
        const float dr = drive.next();

        // Excitation: (optionally saturated) mono input.
        const float pre = 1.0f + 8.0f * dr;
        const float exc = fastTanh (0.5f * (x[0][i] + x[1][i]) * pre) / std::sqrt (pre);

        float outL = 0.0f, outR = 0.0f;
        const float dampCoeff = 0.25f + 0.75f * br;   // one-pole: 1 = open

        for (int v = 0; v < maxVoices; ++v)
        {
            auto& voice = voices[(size_t) v];
            const float g = voice.gain.next();
            if (g <= 1.0e-5f && v >= numVoices)
                continue;

            const float fb = fbGain[(size_t) v];
            const float drive0 = exc * (1.0f - fb) * 4.0f;
            for (int c = 0; c < 2; ++c)
            {
                auto& line = voice.line[(size_t) c];
                auto& damp = voice.damp[(size_t) c];
                float y = line.tapCubic (period[(size_t) v]);
                damp.setCoefficient (dampCoeff);
                y = damp.processLP (y);
                const float w = drive0 + fb * y;
                line.write (std::abs (w) > 2.0f ? std::copysign (2.0f + fastTanh (std::abs (w) - 2.0f), w) : w);
                (c == 0 ? outL : outR) += g * y * (c == 0 ? gainL[(size_t) v] : gainR[(size_t) v]);
            }
        }

        x[0][i] = dc[0].process (outL * norm);
        x[1][i] = dc[1].process (outR * norm);
    }
}

} // namespace aetherfall
