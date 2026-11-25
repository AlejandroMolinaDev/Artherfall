#include "TranceGate.h"
#include "DSP/Utils/DspMath.h"
#include "Parameters/ChoiceLists.h"

namespace aetherfall
{

using namespace dsp;

void TranceGate::prepareModule() { reset(); }

void TranceGate::reset()
{
    position = 0.0;
    gain = 1.0f;
}

void TranceGate::process (juce::AudioBuffer<float>& buffer, const ModContext& ctx)
{
    const int n = std::min (ctx.numSamples, buffer.getNumSamples());
    if (n <= 0)
        return;

    const int divIndex = clamp (ctx.getChoice (PID::GateDiv), 0, (int) choices::gateDivBeats.size() - 1);
    const double stepBeats = choices::gateDivBeats[(size_t) divIndex];
    const int steps = clamp ((int) std::lround (ctx.get (PID::GateSteps)), 1, 16);
    const float swing = clamp (ctx.get (PID::GateSwing), 0.0f, 0.75f);
    const float smooth = clamp (ctx.get (PID::GateSmooth), 0.0f, 1.0f);
    const float depth = clamp (ctx.get (PID::GateDepth), 0.0f, 1.0f);

    const auto& t = ctx.transport;
    const double stepsPerSample = (t.bpm / 60.0) / stepBeats / sampleRate;
    const double stepSeconds = stepBeats * 60.0 / t.bpm;

    // Lock to the song position while the host plays.
    if (t.isPlaying && t.hasPpq)
        position = t.ppqPosition / stepBeats;

    // Smoothing: 1 ms (hard gate) .. 45 % of a step (soft pulse).
    const double smoothSec = 0.001 + (double) smooth * 0.45 * stepSeconds;
    const float coeff = (float) std::exp (-1.0 / (smoothSec * sampleRate));

    float* x[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };

    for (int i = 0; i < n; ++i)
    {
        // Swing moves the boundary between the two steps of each pair later.
        const double pairPos = std::fmod (position, 2.0);
        const double boundary = 1.0 + 0.5 * (double) swing;
        const long long pairIndex = (long long) std::floor (position / 2.0);
        const int stepInPair = pairPos < boundary ? 0 : 1;
        const int step = (int) (((pairIndex * 2 + stepInPair) % steps + steps) % steps);

        const bool open = ((ctx.gatePattern >> step) & 1u) != 0;
        const float target = open ? 1.0f : 1.0f - depth;
        gain = target + coeff * (gain - target);

        x[0][i] *= gain;
        x[1][i] *= gain;
        position += stepsPerSample;
    }
}

} // namespace aetherfall
