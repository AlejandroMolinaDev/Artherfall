#include "Comb.h"
#include "DSP/Utils/DspMath.h"

namespace aetherfall
{

using namespace dsp;

void Comb::prepareModule()
{
    for (auto& c : ch)
    {
        c.line.prepare ((int) std::ceil (sampleRate / 15.0) + 16);   // down to ~15 Hz
        c.dc.prepare (sampleRate, 10.0);
    }
    for (auto* s : { &feedback, &dampAmt, &smearAmt, &outGain })
        s->prepare (sampleRate, 0.03);
    glide = std::exp (-1.0 / (0.02 * sampleRate));
    reset();
}

void Comb::reset()
{
    for (auto& c : ch)
    {
        c.line.reset();
        c.damp.reset();
        for (auto& a : c.smear) a.reset();
        c.dc.reset();
    }
    modPhase = 0.0;
    primed = false;
}

void Comb::process (juce::AudioBuffer<float>& buffer, const ModContext& ctx)
{
    const int n = std::min (ctx.numSamples, buffer.getNumSamples());
    if (n <= 0)
        return;

    double freq = clamp ((double) ctx.get (PID::CombFreq), 20.0, 2000.0);
    if (ctx.getBool (PID::CombTrack) && ctx.analysis.pitchHz > 20.0f && ctx.analysis.pitchConfidence > 0.5f)
        freq = clamp ((double) ctx.analysis.pitchHz, 20.0, 2000.0);

    const double spreadSemis = 0.5 * (double) clamp (ctx.get (PID::CombSpread), 0.0f, 1.0f);
    const double targetLog[2] = { std::log (freq), std::log (freq * std::exp2 (spreadSemis / 12.0)) };

    const float fbT = clamp (ctx.get (PID::CombFeedback), -0.99f, 0.99f);
    // Resonant peaks grow like 1 / (1 - |fb|): normalise so loud rings stay in range.
    const float gainT = 1.0f - 0.9f * std::abs (fbT);

    if (! primed)
    {
        for (int c = 0; c < 2; ++c) ch[(size_t) c].logFreq = targetLog[c];
        feedback.setCurrentAndTarget (fbT);
        dampAmt.setCurrentAndTarget (ctx.get (PID::CombDamp));
        smearAmt.setCurrentAndTarget (ctx.get (PID::CombSmear));
        outGain.setCurrentAndTarget (gainT);
        primed = true;
    }
    feedback.setTarget (fbT);
    dampAmt.setTarget (clamp (ctx.get (PID::CombDamp), 0.0f, 1.0f));
    smearAmt.setTarget (clamp (ctx.get (PID::CombSmear), 0.0f, 1.0f));
    outGain.setTarget (gainT);

    const double modInc = clamp ((double) ctx.get (PID::CombModRate), 0.001, 20.0) / sampleRate;
    const double modDepth = 0.02 * (double) clamp (ctx.get (PID::CombModDepth), 0.0f, 1.0f);   // +-2 % of the period
    float* x[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };

    for (int i = 0; i < n; ++i)
    {
        modPhase += modInc;
        if (modPhase >= 1.0) modPhase -= 1.0;

        const float fb = feedback.next();
        const float dmp = dampAmt.next();
        const float sm = smearAmt.next();
        const float g = outGain.next();

        // Damping: one-pole coefficient from fully open (0) to dark (1).
        const float dampCoeff = 1.0f - 0.92f * dmp;
        const float apCoeff = -0.75f * sm;

        for (int c = 0; c < 2; ++c)
        {
            auto& chan = ch[(size_t) c];
            chan.logFreq = targetLog[c] + glide * (chan.logFreq - targetLog[c]);

            const double mod = 1.0 + modDepth * std::sin (twoPi * (modPhase + 0.25 * c));
            const double period = sampleRate / std::exp (chan.logFreq) * mod;
            float y = chan.line.tapCubic ((float) std::max (2.0, period - 1.0));

            chan.damp.setCoefficient (dampCoeff);
            y = chan.damp.processLP (y);
            if (sm > 1.0e-4f)
                for (auto& ap : chan.smear)
                {
                    ap.setCoefficient (apCoeff);
                    y = ap.process (y);
                }

            const float v = chan.dc.process (x[c][i] + fb * y);
            chan.line.write (std::abs (v) > 2.0f ? std::copysign (2.0f + fastTanh (std::abs (v) - 2.0f), v) : v);
            x[c][i] = v * g;
        }
    }
}

} // namespace aetherfall
