#include "ParamLayout.h"

namespace aetherfall
{

juce::NormalisableRange<float> makeRange (const ParamInfo& p)
{
    if (p.kind == Kind::Choice)
        return { 0.0f, p.rangeMax(), 1.0f };

    if (p.kind == Kind::Bool)
        return { 0.0f, 1.0f, 1.0f };

    if ((p.flags & ParamFlag::SymSkew) != 0 && p.skew > 0.0f)
        return { p.minValue, p.maxValue, p.interval, p.skew, true };

    juce::NormalisableRange<float> r (p.minValue, p.maxValue, p.interval);

    if (p.skew > p.minValue && p.skew < p.maxValue)
        r.setSkewForCentre (p.skew);

    return r;
}

//==============================================================================
namespace
{
    juce::String trimNumber (double v, int decimals)
    {
        return juce::String (v, decimals);
    }

    juce::String formatHz (float hz)
    {
        if (std::abs (hz) >= 1000.0f)
            return trimNumber (hz / 1000.0, std::abs (hz) >= 10000.0f ? 1 : 2) + " kHz";

        return trimNumber (hz, std::abs (hz) >= 100.0f ? 0 : (std::abs (hz) >= 10.0f ? 1 : 2)) + " Hz";
    }

    juce::String formatSigned (double v, int decimals, const char* suffix)
    {
        auto s = trimNumber (v, decimals);
        if (v > 0.0)
            s = "+" + s;
        return s + suffix;
    }
}

juce::String valueToText (const ParamInfo& p, float v, int maxLength)
{
    juce::String s;

    if (p.kind == Kind::Bool)
    {
        s = v >= 0.5f ? "On" : "Off";
    }
    else if (p.kind == Kind::Choice)
    {
        const auto list = choices::get (p.choiceSet);
        const int i = juce::jlimit (0, (int) list.size - 1, juce::roundToInt (v));
        s = list.size > 0 ? juce::String (std::string (list.data[(size_t) i])) : juce::String (i);
    }
    else
    {
        switch (p.unit)
        {
            case Unit::None:   s = trimNumber (v, 2); break;
            case Unit::Int:    s = juce::String (juce::roundToInt (v)); break;
            case Unit::Pct:    s = trimNumber (v * 100.0, 0) + " %"; break;
            case Unit::BiPct:  s = formatSigned (v * 100.0, 0, " %"); break;
            case Unit::dB:     s = formatSigned (v, std::abs (v) < 10.0f ? 1 : 0, " dB"); break;
            case Unit::Hz:     s = formatHz (v); break;
            case Unit::HzOffLow:  s = (v <= p.minValue + 1.0e-3f) ? juce::String ("Off") : formatHz (v); break;
            case Unit::HzOffHigh: s = (v >= p.maxValue - 1.0e-3f) ? juce::String ("Off") : formatHz (v); break;
            case Unit::ms:     s = v >= 1000.0f ? trimNumber (v / 1000.0, 2) + " s"
                                                : trimNumber (v, v < 10.0f ? 1 : 0) + " ms"; break;
            case Unit::s:      s = v >= 10.0f ? trimNumber (v, 1) + " s" : trimNumber (v, 2) + " s"; break;
            case Unit::st:     s = formatSigned (v, p.interval >= 1.0f ? 0 : 1, " st"); break;
            case Unit::ct:     s = trimNumber (v, 1) + " ct"; break;
            case Unit::Ratio:  s = trimNumber (v, 1) + ":1"; break;
            case Unit::PerSec: s = trimNumber (v, v < 10.0f ? 1 : 0) + " /s"; break;
            case Unit::Bits:   s = trimNumber (v, 1) + " bit"; break;
            case Unit::Phase:  s = trimNumber (v * 360.0, 0) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0")); break;
        }
    }

    if (maxLength > 0 && s.length() > maxLength)
        s = s.substring (0, maxLength);

    return s;
}

float textToValue (const ParamInfo& p, const juce::String& textIn)
{
    const auto text = textIn.trim();
    const auto lower = text.toLowerCase();

    if (p.kind == Kind::Bool)
        return (lower == "on" || lower == "1" || lower == "true" || lower == "yes") ? 1.0f : 0.0f;

    if (p.kind == Kind::Choice)
    {
        const auto list = choices::get (p.choiceSet);
        for (size_t i = 0; i < list.size; ++i)
            if (lower == juce::String (std::string (list.data[i])).toLowerCase())
                return (float) i;
        return (float) juce::jlimit (0, (int) list.size - 1, text.getIntValue());
    }

    if (lower == "off")
    {
        if (p.unit == Unit::HzOffLow)  return p.minValue;
        if (p.unit == Unit::HzOffHigh) return p.maxValue;
    }

    double v = text.retainCharacters ("0123456789.-+eE").getDoubleValue();

    switch (p.unit)
    {
        case Unit::Pct:
        case Unit::BiPct:   v /= 100.0; break;
        case Unit::Phase:   v /= 360.0; break;
        case Unit::Hz:
        case Unit::HzOffLow:
        case Unit::HzOffHigh:
            if (lower.contains ("k")) v *= 1000.0;
            break;
        case Unit::ms:
            if (lower.endsWith (" s") || (lower.endsWith ("s") && ! lower.endsWith ("ms"))) v *= 1000.0;
            break;
        case Unit::s:
            if (lower.endsWith ("ms")) v /= 1000.0;
            break;
        case Unit::None: case Unit::Int: case Unit::dB: case Unit::st: case Unit::ct:
        case Unit::Ratio: case Unit::PerSec: case Unit::Bits:
            break;
    }

    return juce::jlimit (p.minValue, p.maxValue, (float) v);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto& p : paramInfos)
    {
        const juce::ParameterID pid { juce::String (std::string (p.id)), parameterVersionHint };
        const juce::String name (std::string (p.name));
        const ParamInfo* info = &p; // paramInfos is static storage: safe to capture

        auto toText   = [info] (float v, int maxLen) { return valueToText (*info, v, maxLen); };
        auto fromText = [info] (const juce::String& t) { return textToValue (*info, t); };

        switch (p.kind)
        {
            case Kind::Float:
            {
                auto attrs = juce::AudioParameterFloatAttributes()
                                 .withStringFromValueFunction (toText)
                                 .withValueFromStringFunction (fromText)
                                 .withAutomatable (p.isAutomatable());
                layout.add (std::make_unique<juce::AudioParameterFloat> (pid, name, makeRange (p),
                                                                         p.defaultValue, attrs));
                break;
            }

            case Kind::Bool:
            {
                auto attrs = juce::AudioParameterBoolAttributes()
                                 .withAutomatable (p.isAutomatable());
                layout.add (std::make_unique<juce::AudioParameterBool> (pid, name, p.defaultValue >= 0.5f, attrs));
                break;
            }

            case Kind::Choice:
            {
                juce::StringArray options;
                const auto list = choices::get (p.choiceSet);
                for (size_t i = 0; i < list.size; ++i)
                    options.add (juce::String (std::string (list.data[i])));

                auto attrs = juce::AudioParameterChoiceAttributes()
                                 .withAutomatable (p.isAutomatable());
                layout.add (std::make_unique<juce::AudioParameterChoice> (pid, name, options,
                                                                          juce::roundToInt (p.defaultValue), attrs));
                break;
            }
        }
    }

    return layout;
}

//==============================================================================
ParameterCache::ParameterCache (juce::AudioProcessorValueTreeState& apvts)
{
    for (const auto& p : paramInfos)
    {
        auto* a = apvts.getRawParameterValue (juce::String (std::string (p.id)));
        jassert (a != nullptr);
        atomics[idx (p.pid)] = a;
        values[idx (p.pid)] = a != nullptr ? a->load() : p.defaultValue;
    }
}

void ParameterCache::refresh() noexcept
{
    for (size_t i = 0; i < atomics.size(); ++i)
        values[i] = atomics[i]->load (std::memory_order_relaxed);
}

} // namespace aetherfall
