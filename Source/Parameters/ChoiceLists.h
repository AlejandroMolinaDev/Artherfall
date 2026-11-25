#pragma once

#include <array>
#include <cstddef>
#include <string_view>

// -----------------------------------------------------------------------------
// Option lists of every choice parameter. The order of the entries is part of
// the saved state (choices are stored as indices): only ever APPEND options.
// -----------------------------------------------------------------------------
namespace aetherfall
{

enum class ChoiceSet : int
{
    None = 0,
    NoteDiv,
    ShimInterval,
    ColorMode,
    GranWindow,
    GranRandMode,
    FFTSize,
    DroneOctave,
    DroneVoicing,
    ResoChord,
    Octave5,
    FilterMode,
    FilterSlope,
    FilterType,
    ModFXMode,
    SatMode,
    Oversampling,
    RingMode,
    GateDiv,
    RevMode,
    TexType,
    LfoShape,
    LfoTrigger,
    Polarity,
    EnvSource,
    ChaosMode,
    Key,
    Scale,
    Quality,
    Count
};

namespace choices
{
    using sv = std::string_view;

    // Tempo divisions shared by delays, LFOs, sequencers... (see NoteDivision.h)
    inline constexpr std::array<sv, 21> noteDiv {
        "1/64", "1/32T", "1/32", "1/32D", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D",
        "1/4T", "1/4", "1/4D", "1/2T", "1/2", "1/2D", "1 Bar", "2 Bars", "4 Bars", "8 Bars", "16 Bars"
    };

    // Shimmer intervals in semitones (ATMOS CORE).
    inline constexpr std::array<sv, 6> shimInterval { "+12", "+7", "+5", "-12", "+19", "+24" };
    inline constexpr std::array<int, 6> shimIntervalSemis { 12, 7, 5, -12, 19, 24 };

    inline constexpr std::array<sv, 5> colorMode   { "Clean", "Dark", "Vintage 80s", "Lo-Fi", "Void" };
    inline constexpr std::array<sv, 3> granWindow  { "Hann", "Tukey", "Triangle" };
    inline constexpr std::array<sv, 3> granRand    { "Free", "Octaves", "Oct + 5th" };
    inline constexpr std::array<sv, 2> fftSize     { "2048", "4096" };
    inline constexpr std::array<sv, 4> droneOctave { "-2 Oct", "-1 Oct", "0", "+1 Oct" };
    inline constexpr std::array<sv, 6> droneVoicing { "Unison", "Fifth", "Octave", "Minor", "Sus2", "Minor 9" };
    inline constexpr std::array<sv, 9> resoChord   { "Minor", "Minor 7", "Minor 9", "Major", "Major 7",
                                                     "Sus2", "Sus4", "Fifths", "Octaves" };
    inline constexpr std::array<sv, 5> octave5     { "-2", "-1", "0", "+1", "+2" };
    inline constexpr std::array<sv, 5> filterMode  { "LP", "HP", "BP", "Notch", "Formant" };
    inline constexpr std::array<sv, 2> filterSlope { "12 dB", "24 dB" };
    inline constexpr std::array<sv, 2> filterType  { "SVF", "Ladder" };
    inline constexpr std::array<sv, 3> modFxMode   { "Phaser", "Flanger", "Chorus" };
    inline constexpr std::array<sv, 5> satMode     { "Tape", "Tube", "Diode", "Fold", "Fuzz" };
    inline constexpr std::array<sv, 3> oversampling { "1x", "2x", "4x" };
    inline constexpr std::array<sv, 2> ringMode    { "Ring", "Freq Shift" };
    inline constexpr std::array<sv, 6> gateDiv     { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" };
    inline constexpr std::array<double, 6> gateDivBeats { 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 };
    inline constexpr std::array<sv, 2> revMode     { "Reverse", "Swell" };
    inline constexpr std::array<sv, 5> texType     { "Vinyl", "Tape Hiss", "Wind", "Rain", "Hum" };
    inline constexpr std::array<sv, 8> lfoShape    { "Sine", "Triangle", "Saw Up", "Saw Down", "Square",
                                                     "S&H", "Smooth Rnd", "Custom" };
    inline constexpr std::array<sv, 4> lfoTrigger  { "Free", "Transport", "Transient", "MIDI Note" };
    inline constexpr std::array<sv, 2> polarity    { "Bipolar", "Unipolar" };
    inline constexpr std::array<sv, 2> envSource   { "Input", "Wet" };
    inline constexpr std::array<sv, 2> chaosMode   { "Lorenz", "Drunk Walk" };
    inline constexpr std::array<sv, 12> key        { "C", "C#", "D", "D#", "E", "F",
                                                     "F#", "G", "G#", "A", "A#", "B" };
    inline constexpr std::array<sv, 8> scale       { "Chromatic", "Minor", "Major", "Dorian", "Phrygian",
                                                     "Harm Minor", "Min Pent", "Maj Pent" };
    inline constexpr std::array<sv, 3> quality     { "Eco", "Normal", "High" };

    /** Generic view over any of the lists above. */
    struct ListView
    {
        const sv* data = nullptr;
        std::size_t size = 0;
    };

    template <std::size_t N>
    constexpr ListView view (const std::array<sv, N>& a) noexcept { return { a.data(), N }; }

    constexpr ListView get (ChoiceSet set) noexcept
    {
        switch (set)
        {
            case ChoiceSet::NoteDiv:      return view (noteDiv);
            case ChoiceSet::ShimInterval: return view (shimInterval);
            case ChoiceSet::ColorMode:    return view (colorMode);
            case ChoiceSet::GranWindow:   return view (granWindow);
            case ChoiceSet::GranRandMode: return view (granRand);
            case ChoiceSet::FFTSize:      return view (fftSize);
            case ChoiceSet::DroneOctave:  return view (droneOctave);
            case ChoiceSet::DroneVoicing: return view (droneVoicing);
            case ChoiceSet::ResoChord:    return view (resoChord);
            case ChoiceSet::Octave5:      return view (octave5);
            case ChoiceSet::FilterMode:   return view (filterMode);
            case ChoiceSet::FilterSlope:  return view (filterSlope);
            case ChoiceSet::FilterType:   return view (filterType);
            case ChoiceSet::ModFXMode:    return view (modFxMode);
            case ChoiceSet::SatMode:      return view (satMode);
            case ChoiceSet::Oversampling: return view (oversampling);
            case ChoiceSet::RingMode:     return view (ringMode);
            case ChoiceSet::GateDiv:      return view (gateDiv);
            case ChoiceSet::RevMode:      return view (revMode);
            case ChoiceSet::TexType:      return view (texType);
            case ChoiceSet::LfoShape:     return view (lfoShape);
            case ChoiceSet::LfoTrigger:   return view (lfoTrigger);
            case ChoiceSet::Polarity:     return view (polarity);
            case ChoiceSet::EnvSource:    return view (envSource);
            case ChoiceSet::ChaosMode:    return view (chaosMode);
            case ChoiceSet::Key:          return view (key);
            case ChoiceSet::Scale:        return view (scale);
            case ChoiceSet::Quality:      return view (quality);
            case ChoiceSet::None:
            case ChoiceSet::Count:        break;
        }
        return {};
    }
} // namespace choices

} // namespace aetherfall
