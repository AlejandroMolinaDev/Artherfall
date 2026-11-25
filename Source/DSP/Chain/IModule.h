#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "ModContext.h"

namespace aetherfall
{

/**
    Common interface of every effect module that can live in a chain slot
    (section 4.2 of the spec) — and of the fixed stages (core, output).

    Contract:
      - prepare() may allocate; process()/reset() must not allocate or lock.
      - process() works in place on a STEREO buffer (internal processing is
        always 2 channels) whose length is ctx.numSamples (<= maxBlock).
      - The module reads its parameters from ctx (effective, modulated values)
        and is responsible for its own smoothing.
      - The module outputs its fully WET signal; dry/wet, on/off and solo are
        handled by the chain slot that hosts it.
*/
struct IModule
{
    virtual void prepare (double sampleRate, int maxBlock, int numChannels) = 0;
    virtual void reset() = 0;
    virtual void process (juce::AudioBuffer<float>&, const ModContext&) = 0;
    virtual int  getLatencySamples() const { return 0; }
    virtual ~IModule() = default;

    /** Approximate length of the module's own tail (for host tail reporting). */
    virtual double getTailSeconds() const { return 0.0; }

    /** Module identity (ModuleType::None for non-chain stages). */
    virtual ModuleType getType() const { return ModuleType::None; }
};

/** Convenience base that stores the prepare() arguments. */
class ModuleBase : public IModule
{
public:
    void prepare (double sr, int maxBlockSize, int channels) override
    {
        sampleRate = sr;
        maxBlock = maxBlockSize;
        numChannels = channels;
        prepareModule();
    }

protected:
    /** Called from prepare() once sampleRate/maxBlock/numChannels are set. */
    virtual void prepareModule() = 0;

    double sampleRate = 48000.0;
    int maxBlock = 512;
    int numChannels = 2;
};

} // namespace aetherfall
