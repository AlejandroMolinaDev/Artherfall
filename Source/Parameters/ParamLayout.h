#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ParamIDs.h"

namespace aetherfall
{

/** Builds the NormalisableRange of a parameter from its ParamInfo. */
juce::NormalisableRange<float> makeRange (const ParamInfo&);

/** Formats a real (denormalised) value for display, e.g. "4.20 s", "1.2 kHz", "Off". */
juce::String valueToText (const ParamInfo&, float value, int maxLength = 0);

/** Parses user text back into a real value (accepts units, "k" suffix, "off"...). */
float textToValue (const ParamInfo&, const juce::String& text);

/** Creates the complete APVTS layout from the parameter table (ParamList.h). */
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

/** Version hint used for every juce::ParameterID (VST3/AU parameter versioning). */
inline constexpr int parameterVersionHint = 1;

//==============================================================================
/**
    Lock-free access to the parameter values for the audio thread.
    Holds the raw std::atomic<float>* of every parameter (resolved once) and a
    plain float snapshot refreshed at control rate.
*/
class ParameterCache
{
public:
    explicit ParameterCache (juce::AudioProcessorValueTreeState&);

    /** Copies every raw parameter value into the snapshot (audio thread). */
    void refresh() noexcept;

    float raw (PID p) const noexcept               { return values[idx (p)]; }
    const float* data() const noexcept             { return values.data(); }
    float* data() noexcept                         { return values.data(); }

    /** Direct atomic read, bypassing the snapshot. */
    float load (PID p) const noexcept              { return atomics[idx (p)]->load (std::memory_order_relaxed); }

private:
    std::array<std::atomic<float>*, (size_t) numParams> atomics {};
    std::array<float, (size_t) numParams> values {};
};

} // namespace aetherfall
