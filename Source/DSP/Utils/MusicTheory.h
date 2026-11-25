#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace aetherfall::music
{

/** Scale masks (bit n = semitone n above the key is in the scale).
    Index-compatible with choices::scale. */
inline constexpr std::array<std::uint16_t, 8> scaleMasks {
    0b111111111111,   // Chromatic
    0b010110101101,   // Minor       0 2 3 5 7 8 10
    0b101010110101,   // Major       0 2 4 5 7 9 11
    0b011010101101,   // Dorian      0 2 3 5 7 9 10
    0b010110101011,   // Phrygian    0 1 3 5 7 8 10
    0b100110101101,   // Harm Minor  0 2 3 5 7 8 11
    0b010010101001,   // Min Pent    0 3 5 7 10
    0b001010010101,   // Maj Pent    0 2 4 7 9
};

/** Snaps a (fractional) MIDI note to the nearest note of key + scale. */
inline int quantizeToScale (double note, int key, int scaleIndex) noexcept
{
    const auto mask = scaleMasks[(size_t) (scaleIndex < 0 || scaleIndex > 7 ? 0 : scaleIndex)];
    const int centre = (int) std::lround (note);
    int best = centre;
    double bestDist = 1.0e9;

    for (int candidate = centre - 6; candidate <= centre + 6; ++candidate)
    {
        const int degree = ((candidate - key) % 12 + 12) % 12;
        if ((mask >> degree) & 1u)
        {
            const double d = std::abs ((double) candidate - note);
            if (d < bestDist)
            {
                bestDist = d;
                best = candidate;
            }
        }
    }
    return best;
}

} // namespace aetherfall::music
