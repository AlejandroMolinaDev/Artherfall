#pragma once

#include <array>
#include <algorithm>

// -----------------------------------------------------------------------------
// Tempo divisions (index-compatible with choices::noteDiv).
// Lengths are expressed in quarter-note beats. Bar-based entries depend on the
// time signature, so they are stored as "bars" and resolved at runtime.
// -----------------------------------------------------------------------------
namespace aetherfall::notediv
{

struct Division
{
    double quarterBeats; // length in quarter notes (when bars == 0)
    double bars;         // length in bars (0 = not bar based)
};

inline constexpr double triplet = 2.0 / 3.0;
inline constexpr double dotted  = 1.5;

inline constexpr std::array<Division, 21> table {{
    { 0.0625,           0 }, // 1/64
    { 0.125 * triplet,  0 }, // 1/32T
    { 0.125,            0 }, // 1/32
    { 0.125 * dotted,   0 }, // 1/32D
    { 0.25 * triplet,   0 }, // 1/16T
    { 0.25,             0 }, // 1/16
    { 0.25 * dotted,    0 }, // 1/16D
    { 0.5 * triplet,    0 }, // 1/8T
    { 0.5,              0 }, // 1/8
    { 0.5 * dotted,     0 }, // 1/8D
    { 1.0 * triplet,    0 }, // 1/4T
    { 1.0,              0 }, // 1/4
    { 1.0 * dotted,     0 }, // 1/4D
    { 2.0 * triplet,    0 }, // 1/2T
    { 2.0,              0 }, // 1/2
    { 2.0 * dotted,     0 }, // 1/2D
    { 0.0,              1 }, // 1 Bar
    { 0.0,              2 }, // 2 Bars
    { 0.0,              4 }, // 4 Bars
    { 0.0,              8 }, // 8 Bars
    { 0.0,             16 }, // 16 Bars
}};

// Handy indices used as defaults.
inline constexpr int idx1_16  = 5;
inline constexpr int idx1_8   = 8;
inline constexpr int idx1_8D  = 9;
inline constexpr int idx1_4   = 11;
inline constexpr int idx1_2   = 14;
inline constexpr int idx1Bar  = 16;
inline constexpr int idx2Bars = 17;

/** Length of a division in quarter-note beats for a given time signature. */
constexpr double toQuarterBeats (int index, int timeSigNumerator = 4, int timeSigDenominator = 4) noexcept
{
    const auto& d = table[(size_t) std::clamp (index, 0, (int) table.size() - 1)];

    if (d.bars > 0.0)
    {
        const double beatsPerBar = (double) std::max (1, timeSigNumerator) * 4.0
                                   / (double) std::max (1, timeSigDenominator);
        return d.bars * beatsPerBar;
    }
    return d.quarterBeats;
}

/** Length of a division in seconds at the given tempo. */
constexpr double toSeconds (int index, double bpm, int timeSigNumerator = 4, int timeSigDenominator = 4) noexcept
{
    const double safeBpm = std::clamp (bpm, 20.0, 999.0);
    return toQuarterBeats (index, timeSigNumerator, timeSigDenominator) * 60.0 / safeBpm;
}

} // namespace aetherfall::notediv
