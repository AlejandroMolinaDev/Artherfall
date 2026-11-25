#include "InputAnalyzer.h"
#include "DSP/Utils/DspMath.h"

namespace aetherfall
{

using namespace dsp;

void InputAnalyzer::prepare (double sampleRate)
{
    sr = sampleRate;
    pitch.prepare (sampleRate);
    auto coeff = [sampleRate] (double s) { return (float) std::exp (-1.0 / (s * sampleRate)); };
    msCoeff = coeff (0.05);
    fastAtk = coeff (0.0005); fastRel = coeff (0.015);
    slowAtk = coeff (0.02);   slowRel = coeff (0.2);
    transientDecay = coeff (0.1);
    centroidCoeff = coeff (0.1);
    reset();
}

void InputAnalyzer::reset()
{
    pitch.reset();
    ms = fast = slow = 0.0f;
    armed = true;
    onset = false;
    prevSample = diffEnergy = sigEnergy = 0.0f;
    lastPitchNorm = 0.0f;
    frame = {};
}

void InputAnalyzer::process (const float* left, const float* right, int n, bool trackPitch) noexcept
{
    onset = false;
    float peak = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float m = 0.5f * (left[i] + right[i]);
        const float a = std::abs (m);
        peak = std::max (peak, a);

        ms = m * m + msCoeff * (ms - m * m);
        fast = a + (a > fast ? fastAtk : fastRel) * (fast - a);
        slow = a + (a > slow ? slowAtk : slowRel) * (slow - a);

        // Transient: the fast envelope jumps well above the slow one.
        const float ratio = fast / (slow + 1.0e-4f);
        if (armed && ratio > 2.0f && fast > 0.003f)
        {
            frame.transient = std::min (1.0f, (ratio - 1.0f) / 3.0f);
            onset = true;
            armed = false;
        }
        else if (ratio < 1.3f)
        {
            armed = true;
        }
        frame.transient *= transientDecay;

        // Brightness: energy of the first difference vs. signal energy.
        const float d = m - prevSample;
        prevSample = m;
        diffEnergy = d * d + centroidCoeff * (diffEnergy - d * d);
        sigEnergy  = m * m + centroidCoeff * (sigEnergy - m * m);

        if (trackPitch)
            pitch.push (m);
    }

    frame.rms = std::sqrt (ms);
    frame.peak = peak;
    frame.pitchHz = pitch.getFrequency();
    frame.pitchConfidence = pitch.getConfidence();

    // For a sinusoid E[dx^2]/E[x^2] = 2(1 - cos w)  ->  w = acos(1 - r/2).
    if (sigEnergy > 1.0e-9f)
    {
        const float r = clamp (diffEnergy / sigEnergy, 0.0f, 4.0f);
        const float w = std::acos (clamp (1.0f - 0.5f * r, -1.0f, 1.0f));
        frame.centroidHz = (float) (w * sr / twoPi);
    }
}

float InputAnalyzer::rmsNormalised() const noexcept
{
    return clamp ((gainToDb (frame.rms) + 60.0f) / 60.0f, 0.0f, 1.0f);
}

float InputAnalyzer::pitchNormalised() const noexcept
{
    // Holds the last confident pitch; MIDI notes 24..96 -> 0..1.
    auto* self = const_cast<InputAnalyzer*> (this);
    if (frame.pitchHz > 0.0f && frame.pitchConfidence > 0.6f)
        self->lastPitchNorm = clamp ((hzToNote (frame.pitchHz) - 24.0f) / 72.0f, 0.0f, 1.0f);
    return lastPitchNorm;
}

float InputAnalyzer::centroidNormalised() const noexcept
{
    if (frame.centroidHz <= 0.0f)
        return 0.0f;
    return clamp (std::log2 (frame.centroidHz / 50.0f) / std::log2 (400.0f), 0.0f, 1.0f);
}

} // namespace aetherfall
