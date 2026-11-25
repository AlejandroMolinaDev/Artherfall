#pragma once

#include <array>
#include <vector>
#include <juce_graphics/juce_graphics.h>
#include "Parameters/ParamIDs.h"
#include "DSP/Modulation/ModSources.h"

namespace aetherfall
{

/** Preset categories (spec section 8). Index-stable: stored in preset files. */
enum class PresetCategory : int
{
    OutroSkies = 0, IntroHaze, VocalAtmos, SynthWalls, GranularDreams, DarkRude, Frozen, RhythmicSpace, Subtle,
    User, Count
};

inline constexpr int numFactoryCategories = 9;

const char* categoryName (PresetCategory) noexcept;
PresetCategory categoryFromName (const juce::String&) noexcept;
juce::Colour categoryAccent (PresetCategory) noexcept;

/**
    A factory preset described as data: only the values that differ from the
    parameter defaults, the chain order, modulation routes and (optionally)
    the Trance Gate pattern. Everything else is the default state.
*/
struct FactoryPreset
{
    struct Mod
    {
        ModSource source;
        PID dest;
        float amount;
        ModCurve curve = ModCurve::Linear;
    };

    const char* name;
    PresetCategory category;
    std::vector<std::pair<PID, float>> values;
    std::vector<ModuleType> chain;
    std::vector<Mod> mods;
    std::uint32_t gatePattern = 0;   // 0 = keep the default pattern
};

/** All factory presets (built once, message thread). */
const std::vector<FactoryPreset>& factoryPresets();

} // namespace aetherfall
