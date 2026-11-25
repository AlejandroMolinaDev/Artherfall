#pragma once

#include <array>
#include <cstdint>
#include "ModSources.h"
#include "Parameters/ParamIDs.h"

namespace aetherfall
{

/** One row of the modulation matrix (its amount is the parameter modN_amt). */
struct ModSlot
{
    ModSource source = ModSource::None;
    std::int16_t dest = -1;              // PID index, -1 = unassigned
    ModCurve curve = ModCurve::Linear;
    ModSource via = ModSource::None;     // optional multiplier source
};

/** One destination of a macro: the macro value (0..1) times `amount` is added
    to the destination's normalised value. */
struct MacroTarget
{
    std::int16_t dest = -1;
    float amount = 0.0f;                 // -1..1 (normalised range)
};

inline constexpr int macroTargetsPerMacro = 8;
inline constexpr int seqMaxSteps = 32;
inline constexpr int lfoCustomPoints = 16;

/**
    Everything about modulation that is NOT a host parameter. Edited on the
    message thread (state tree), handed to the audio thread through a
    TripleBuffer. Plain data, trivially copyable.
*/
struct ModRouting
{
    std::array<ModSlot, (size_t) numModSlots> slots {};
    std::array<std::array<MacroTarget, (size_t) macroTargetsPerMacro>, (size_t) numMacros> macros {};
    std::array<std::array<float, (size_t) seqMaxSteps>, (size_t) numStepSeqs> seqSteps {};
    std::array<std::array<float, (size_t) lfoCustomPoints>, (size_t) numLfos> lfoCustom {};
};

namespace routing
{
    /** Default macro mappings (spec section 6). */
    inline void setDefaultMacros (ModRouting& r)
    {
        auto set = [&r] (int macro, std::initializer_list<std::pair<PID, float>> targets)
        {
            auto& m = r.macros[(size_t) macro];
            m = {};
            size_t i = 0;
            for (const auto& [pid, amt] : targets)
                if (i < m.size())
                    m[i++] = { (std::int16_t) pid, amt };
        };

        // HAZE: size + decay + diffusion + granular mix
        set (0, { { PID::CoreSize, 0.35f }, { PID::CoreDecay, 0.30f }, { PID::CoreDiffEarly, 0.25f },
                  { PID::CoreDiffLate, 0.30f }, { PID::GranMix, 0.50f } });
        // DRIFT: global modulation amount (LFOs, wobble, pitch spray)
        set (1, { { PID::GlobModDepth, 0.40f }, { PID::CoreModDepth, 0.50f }, { PID::TapeWow, 0.40f },
                  { PID::GranPitchRand, 0.20f }, { PID::LofiWobble, 0.40f }, { PID::MfxDepth, 0.30f } });
        // GRIT: saturation + bitcrush + filter resonance
        set (2, { { PID::SatDrive, 0.55f }, { PID::InDrive, 0.30f }, { PID::CrushBits, -0.45f },
                  { PID::CrushMix, 0.35f }, { PID::FiltRes, 0.40f } });
        // BLOOM: shimmer + drone + swell
        set (3, { { PID::CoreShimmer, 0.55f }, { PID::CoreBloom, 0.60f }, { PID::DroneMix, 0.50f },
                  { PID::ShdlyShimmer, 0.40f } });
    }

    /** Default step sequences: a gentle rising/falling contour and a pulse. */
    inline void setDefaultSequences (ModRouting& r)
    {
        for (int s = 0; s < seqMaxSteps; ++s)
        {
            r.seqSteps[0][(size_t) s] = 0.5f + 0.5f * std::sin ((float) s / seqMaxSteps * 6.2831853f);
            r.seqSteps[1][(size_t) s] = (s % 4 == 0) ? 1.0f : ((s % 2 == 0) ? 0.5f : 0.15f);
        }
    }

    /** Default custom LFO shape: slow swell then a quick fall ("breathing"). */
    inline void setDefaultLfoShapes (ModRouting& r)
    {
        for (auto& shape : r.lfoCustom)
            for (int p = 0; p < lfoCustomPoints; ++p)
            {
                const float t = (float) p / (lfoCustomPoints - 1);
                shape[(size_t) p] = t < 0.75f ? -1.0f + 2.0f * (t / 0.75f) * (t / 0.75f)
                                              : 1.0f - 2.0f * (t - 0.75f) / 0.25f;
            }
    }

    inline ModRouting makeDefault()
    {
        ModRouting r;
        setDefaultMacros (r);
        setDefaultSequences (r);
        setDefaultLfoShapes (r);
        return r;
    }
} // namespace routing

} // namespace aetherfall
