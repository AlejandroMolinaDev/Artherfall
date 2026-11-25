#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace aetherfall
{

/** Every modulation source. Persisted by string id (see sourceIds). */
enum class ModSource : std::uint8_t
{
    None = 0,
    Lfo1, Lfo2, Lfo3, Lfo4, Lfo5, Lfo6, Lfo7, Lfo8,
    Env1, Env2, Env3, Env4,
    Seq1, Seq2,
    Chaos1, Chaos2,
    Macro1, Macro2, Macro3, Macro4,
    AnalyzerRms, AnalyzerTransient, AnalyzerPitch, AnalyzerCentroid,
    MidiNote, MidiVelocity, MidiModWheel, MidiPitchBend,
    Count
};

inline constexpr int numModSources = (int) ModSource::Count;

inline constexpr std::array<std::string_view, (size_t) ModSource::Count> sourceIds {
    "none",
    "lfo1", "lfo2", "lfo3", "lfo4", "lfo5", "lfo6", "lfo7", "lfo8",
    "env1", "env2", "env3", "env4",
    "seq1", "seq2",
    "chaos1", "chaos2",
    "macro1", "macro2", "macro3", "macro4",
    "an_rms", "an_transient", "an_pitch", "an_centroid",
    "midi_note", "midi_vel", "midi_modwheel", "midi_pitchbend"
};

inline constexpr std::array<std::string_view, (size_t) ModSource::Count> sourceNames {
    "-",
    "LFO 1", "LFO 2", "LFO 3", "LFO 4", "LFO 5", "LFO 6", "LFO 7", "LFO 8",
    "Env 1", "Env 2", "Env 3", "Env 4",
    "Seq 1", "Seq 2",
    "Chaos 1", "Chaos 2",
    "HAZE", "DRIFT", "GRIT", "BLOOM",
    "In RMS", "In Transient", "In Pitch", "In Brightness",
    "MIDI Note", "MIDI Velocity", "Mod Wheel", "Pitch Bend"
};

inline ModSource sourceFromId (std::string_view id) noexcept
{
    for (size_t i = 0; i < sourceIds.size(); ++i)
        if (sourceIds[i] == id)
            return (ModSource) i;
    return ModSource::None;
}

/** "Modulator" sources are scaled by the global Mod Depth (DRIFT); macros and
    MIDI controllers are direct performance controls and are not. */
constexpr bool isScaledByModDepth (ModSource s) noexcept
{
    return (s >= ModSource::Lfo1 && s <= ModSource::Chaos2)
        || (s >= ModSource::AnalyzerRms && s <= ModSource::AnalyzerCentroid);
}

/** Response curve applied to the source value of a matrix slot. */
enum class ModCurve : std::uint8_t { Linear = 0, Exponential, Logarithmic, SCurve, Count };

inline constexpr std::array<std::string_view, 4> curveNames { "Linear", "Exp", "Log", "S-Curve" };

/** Applies a curve to a value in [-1, 1] (sign preserved). */
inline float applyCurve (ModCurve c, float v) noexcept
{
    const float a = std::abs (v);
    float y = a;
    switch (c)
    {
        case ModCurve::Exponential: y = a * a; break;
        case ModCurve::Logarithmic: y = std::sqrt (a); break;
        case ModCurve::SCurve:      y = a * a * (3.0f - 2.0f * a); break;
        case ModCurve::Linear:
        case ModCurve::Count:       break;
    }
    return v < 0.0f ? -y : y;
}

} // namespace aetherfall
