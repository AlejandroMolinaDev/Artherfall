#pragma once

#include <array>
#include <cmath>
#include <functional>
#include <juce_audio_basics/juce_audio_basics.h>

#include "DSP/Chain/IModule.h"
#include "DSP/Utils/Random.h"
#include "Parameters/ParamIDs.h"

namespace aetherfall::test
{

/** Parameter values initialised to the table defaults. */
struct ParamSet
{
    std::array<float, (size_t) numParams> v {};

    ParamSet()
    {
        for (const auto& p : paramInfos)
            v[idx (p.pid)] = p.defaultValue;
    }

    ParamSet& set (PID p, float value) { v[idx (p)] = value; return *this; }
    float get (PID p) const { return v[idx (p)]; }
};

/** Signal generators for stress tests. */
enum class Signal { Silence, Noise, Impulse, DC, Sine };

inline float generate (Signal s, int n, dsp::Random32& rng, float amp = 0.5f)
{
    switch (s)
    {
        case Signal::Silence: return 0.0f;
        case Signal::Noise:   return amp * rng.nextBipolar();
        case Signal::Impulse: return n == 0 ? 1.0f : 0.0f;
        case Signal::DC:      return amp;
        case Signal::Sine:    return amp * std::sin (2.0f * 3.14159265f * 220.0f * (float) n / 48000.0f);
    }
    return 0.0f;
}

/**
    Runs a module for `numSamples` samples in 64-sample chunks.
    `input(n)` returns the (mono) input sample, `onOutput(n, l, r)` receives
    the output. `params` may be modified by `onChunk(n)` between chunks.
*/
inline void runModule (IModule& module, ParamSet& params, double sampleRate, int numSamples,
                       const std::function<float (int)>& input,
                       const std::function<void (int, float, float)>& onOutput,
                       const std::function<void (int)>& onChunk = {})
{
    constexpr int chunk = 64;
    juce::AudioBuffer<float> buf (2, chunk);

    ModContext ctx;
    ctx.values = params.v.data();
    ctx.sampleRate = sampleRate;

    for (int start = 0; start < numSamples; start += chunk)
    {
        const int len = std::min (chunk, numSamples - start);
        if (onChunk)
            onChunk (start);

        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), 2, 0, len);
        for (int i = 0; i < len; ++i)
        {
            const float x = input (start + i);
            view.setSample (0, i, x);
            view.setSample (1, i, x);
        }

        ctx.numSamples = len;
        module.process (view, ctx);

        for (int i = 0; i < len; ++i)
            onOutput (start + i, view.getSample (0, i), view.getSample (1, i));
    }
}

} // namespace aetherfall::test
