#pragma once

#include <juce_dsp/juce_dsp.h>
#include <memory>

namespace aetherfall::dsp
{

/**
    Switchable 1x / 2x / 4x oversampling for the distortion modules.

    Both oversampling engines are created in prepare(), so switching factor on
    the audio thread never allocates. Uses JUCE's polyphase IIR half-band
    filters (minimum phase, ~no latency) — the distortion modules mix their
    own dry signal, and linear-phase FIR latency would need extra compensation
    for only a marginal benefit in this context (see DECISIONS.md).
*/
class Oversampler
{
public:
    void prepare (int maxBlockSize, int numChannels)
    {
        channels = numChannels;
        maxBlock = maxBlockSize;

        using OS = juce::dsp::Oversampling<float>;
        os2 = std::make_unique<OS> ((size_t) numChannels, 1, OS::filterHalfBandPolyphaseIIR, true, false);
        os4 = std::make_unique<OS> ((size_t) numChannels, 2, OS::filterHalfBandPolyphaseIIR, true, false);
        os2->initProcessing ((size_t) maxBlockSize);
        os4->initProcessing ((size_t) maxBlockSize);
        reset();
    }

    void reset()
    {
        if (os2 != nullptr) os2->reset();
        if (os4 != nullptr) os4->reset();
    }

    /** 0 = 1x, 1 = 2x, 2 = 4x. Resets the filters of the newly selected engine. */
    void setFactorIndex (int index)
    {
        index = juce::jlimit (0, 2, index);
        if (index == factorIndex)
            return;

        factorIndex = index;
        if (auto* os = current())
            os->reset();
    }

    int getFactorIndex() const noexcept { return factorIndex; }
    int getFactor() const noexcept      { return 1 << factorIndex; }

    /** Runs `fn(block)` on an oversampled copy of `block` and downsamples the
        result back in place. `fn` receives a juce::dsp::AudioBlock<float>&
        whose sample rate is getFactor() times the host rate. */
    template <typename Fn>
    void process (juce::dsp::AudioBlock<float>& block, Fn&& fn)
    {
        jassert ((int) block.getNumSamples() <= maxBlock);

        if (auto* os = current())
        {
            auto up = os->processSamplesUp (block);
            fn (up);
            os->processSamplesDown (block);
        }
        else
        {
            fn (block);
        }
    }

    float getLatencyInSamples() const noexcept
    {
        if (auto* os = const_cast<Oversampler*> (this)->current())
            return os->getLatencyInSamples();
        return 0.0f;
    }

private:
    juce::dsp::Oversampling<float>* current() noexcept
    {
        switch (factorIndex)
        {
            case 1:  return os2.get();
            case 2:  return os4.get();
            default: return nullptr;
        }
    }

    std::unique_ptr<juce::dsp::Oversampling<float>> os2, os4;
    int factorIndex = 0, channels = 2, maxBlock = 0;
};

} // namespace aetherfall::dsp
