#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ChoiceLists.h"
#include "ParamList.h"

// -----------------------------------------------------------------------------
// Parameter identifiers and the constexpr description table generated from the
// X-macro list in ParamList.h.
// -----------------------------------------------------------------------------
namespace aetherfall
{

enum class Kind : std::uint8_t { Float, Bool, Choice };

/** Display unit, used for value <-> text conversion. */
enum class Unit : std::uint8_t
{
    None,
    Int,        // integer count
    Pct,        // 0..1 shown as %
    BiPct,      // -1..1 shown as signed %
    dB,
    Hz,
    HzOffLow,   // Hz, minimum value displays "Off"
    HzOffHigh,  // Hz, maximum value displays "Off"
    ms,
    s,
    st,         // semitones
    ct,         // cents
    Ratio,      // compressor ratio "x:1"
    PerSec,     // events per second
    Bits,
    Phase       // 0..1 shown in degrees
};

namespace ParamFlag
{
    inline constexpr std::uint32_t NoAuto  = 1u << 0;
    inline constexpr std::uint32_t NoMod   = 1u << 1;
    inline constexpr std::uint32_t SymSkew = 1u << 2;
}

/** Parameter index. Values are positions in paramInfos[] (NOT persisted:
    the host and the presets only ever see the string IDs). */
enum class PID : int
{
   #define AF_X_ENUM(E, id, nm, kind, mn, mx, df, sk, iv, un, cs, fl) E,
    AF_PARAMS (AF_X_ENUM)
   #undef AF_X_ENUM
    Count
};

inline constexpr int numParams = static_cast<int> (PID::Count);

constexpr std::size_t idx (PID p) noexcept { return static_cast<std::size_t> (p); }

struct ParamInfo
{
    PID pid;
    std::string_view id;
    std::string_view name;
    Kind kind;
    float minValue, maxValue, defaultValue;
    float skew;      // centre value, or symmetric skew factor (SymSkew)
    float interval;
    Unit unit;
    ChoiceSet choiceSet;
    std::uint32_t flags;

    constexpr bool isAutomatable() const noexcept { return (flags & ParamFlag::NoAuto) == 0; }

    /** Continuous parameters can be modulation destinations unless flagged. */
    constexpr bool isModulatable() const noexcept
    {
        return kind == Kind::Float && (flags & ParamFlag::NoMod) == 0;
    }

    constexpr int numChoices() const noexcept { return (int) choices::get (choiceSet).size; }

    constexpr float rangeMin() const noexcept { return kind == Kind::Choice ? 0.0f : minValue; }
    constexpr float rangeMax() const noexcept
    {
        return kind == Kind::Choice ? (float) (numChoices() - 1) : maxValue;
    }
};

inline constexpr std::array<ParamInfo, numParams> paramInfos {{
   #define AF_X_INFO(E, id, nm, kind, mn, mx, df, sk, iv, un, cs, fl) \
       ParamInfo { PID::E, id, nm, Kind::kind, mn, mx, df, sk, iv, Unit::un, ChoiceSet::cs, fl },
    AF_PARAMS (AF_X_INFO)
   #undef AF_X_INFO
}};

constexpr const ParamInfo& info (PID p) noexcept { return paramInfos[idx (p)]; }

// -----------------------------------------------------------------------------
// Chain modules (section 4.2 of the spec). The numeric values are persisted in
// the chain-order state: never reorder, only append.
// -----------------------------------------------------------------------------
enum class ModuleType : std::uint8_t
{
    None = 0,
    Granular,
    Spectral,
    ShimmerDelay,
    TapeDelay,
    MultiTap,
    Drone,
    Resonator,
    Comb,
    Filter,
    ModFX,
    Saturator,
    Bitcrush,
    RingFreq,
    Stereo,
    Gate,
    ReverseSwell,
    Texture,
    LoFi,
    Count
};

inline constexpr int numModuleTypes = static_cast<int> (ModuleType::Count) - 1; // excluding None
inline constexpr int numChainSlots  = 8;

struct ModuleInfo
{
    ModuleType type;
    std::string_view id;    // persisted in the chain state
    std::string_view name;  // display name
    PID on, mix, solo, parallel;
    PID firstParam;         // first module-specific parameter
    PID lastParam;          // last module-specific parameter (inclusive)
};

inline constexpr std::array<ModuleInfo, (size_t) numModuleTypes> moduleInfos {{
    { ModuleType::Granular,     "granular",  "Granular Cloud",   PID::GranOn,   PID::GranMix,   PID::GranSolo,   PID::GranParallel,   PID::GranSize,      PID::GranWidth },
    { ModuleType::Spectral,     "spectral",  "Spectral Freeze",  PID::SpecOn,   PID::SpecMix,   PID::SpecSolo,   PID::SpecParallel,   PID::SpecFFT,       PID::SpecTilt },
    { ModuleType::ShimmerDelay, "shdelay",   "Shimmer Delay",    PID::ShdlyOn,  PID::ShdlyMix,  PID::ShdlySolo,  PID::ShdlyParallel,  PID::ShdlyTime,     PID::ShdlyHPF },
    { ModuleType::TapeDelay,    "tapedelay", "Tape Delay",       PID::TapeOn,   PID::TapeMix,   PID::TapeSolo,   PID::TapeParallel,   PID::TapeTime,      PID::TapeSpread },
    { ModuleType::MultiTap,     "multitap",  "Multi-Tap Diffuse",PID::MtapOn,   PID::MtapMix,   PID::MtapSolo,   PID::MtapParallel,   PID::MtapTime,      PID::MtapTone },
    { ModuleType::Drone,        "drone",     "Drone Generator",  PID::DroneOn,  PID::DroneMix,  PID::DroneSolo,  PID::DroneParallel,  PID::DroneOctave,   PID::DroneHold },
    { ModuleType::Resonator,    "resonator", "Resonator Bank",   PID::ResoOn,   PID::ResoMix,   PID::ResoSolo,   PID::ResoParallel,   PID::ResoChord,     PID::ResoDrive },
    { ModuleType::Comb,         "comb",      "Comb / Karplus",   PID::CombOn,   PID::CombMix,   PID::CombSolo,   PID::CombParallel,   PID::CombFreq,      PID::CombTrack },
    { ModuleType::Filter,       "filter",    "Filter Sweep",     PID::FiltOn,   PID::FiltMix,   PID::FiltSolo,   PID::FiltParallel,   PID::FiltMode,      PID::FiltStereo },
    { ModuleType::ModFX,        "modfx",     "Phaser/Flanger/Chorus", PID::MfxOn, PID::MfxMix,  PID::MfxSolo,    PID::MfxParallel,    PID::MfxMode,       PID::MfxWidth },
    { ModuleType::Saturator,    "saturator", "Saturator",        PID::SatOn,    PID::SatMix,    PID::SatSolo,    PID::SatParallel,    PID::SatMode,       PID::SatAutoGain },
    { ModuleType::Bitcrush,     "bitcrush",  "Bitcrush",         PID::CrushOn,  PID::CrushMix,  PID::CrushSolo,  PID::CrushParallel,  PID::CrushBits,     PID::CrushJitter },
    { ModuleType::RingFreq,     "ringfreq",  "Ring / Freq Shift",PID::RingOn,   PID::RingMix,   PID::RingSolo,   PID::RingParallel,   PID::RingMode,      PID::RingStereo },
    { ModuleType::Stereo,       "stereo",    "Stereo Engine",    PID::StereoOn, PID::StereoMix, PID::StereoSolo, PID::StereoParallel, PID::StereoWidth,   PID::StereoBassMono },
    { ModuleType::Gate,         "gate",      "Trance Gate",      PID::GateOn,   PID::GateMix,   PID::GateSolo,   PID::GateParallel,   PID::GateDiv,       PID::GateDepth },
    { ModuleType::ReverseSwell, "revswell",  "Reverse Swell",    PID::RevOn,    PID::RevMix,    PID::RevSolo,    PID::RevParallel,    PID::RevMode,       PID::RevXfade },
    { ModuleType::Texture,      "texture",   "Texture Layer",    PID::TexOn,    PID::TexMix,    PID::TexSolo,    PID::TexParallel,    PID::TexType,       PID::TexWidth },
    { ModuleType::LoFi,         "lofi",      "Lo-Fi Tape",       PID::LofiOn,   PID::LofiMix,   PID::LofiSolo,   PID::LofiParallel,   PID::LofiSat,       PID::LofiNoise },
}};

constexpr const ModuleInfo& moduleInfo (ModuleType t) noexcept
{
    return moduleInfos[(size_t) t - 1];
}

// Parameter groups of repeated blocks, so code can address "LFO n" generically.
inline constexpr int numLfos = 8, numEnvFollowers = 4, numStepSeqs = 2, numChaos = 2;
inline constexpr int numMacros = 4, numModSlots = 32;

inline constexpr int lfoParamStride   = 8; // shape, rate, sync, div, phase, trig, polarity, smooth
inline constexpr int envParamStride   = 4; // attack, release, source, gain
inline constexpr int seqParamStride   = 4; // div, steps, glide, polarity
inline constexpr int chaosParamStride = 3; // mode, rate, smooth

constexpr PID offset (PID base, int delta) noexcept { return static_cast<PID> (static_cast<int> (base) + delta); }

constexpr PID lfoParam   (int lfo,   int field) noexcept { return offset (PID::Lfo1Shape,  lfo * lfoParamStride + field); }
constexpr PID envParam   (int env,   int field) noexcept { return offset (PID::Env1Attack, env * envParamStride + field); }
constexpr PID seqParam   (int seq,   int field) noexcept { return offset (PID::Seq1Div,    seq * seqParamStride + field); }
constexpr PID chaosParam (int chaos, int field) noexcept { return offset (PID::Chaos1Mode, chaos * chaosParamStride + field); }
constexpr PID modAmountParam (int slot) noexcept         { return offset (PID::Mod1Amount, slot); }
constexpr PID macroParam (int macro) noexcept            { return offset (PID::MacroHaze, macro); }

} // namespace aetherfall
