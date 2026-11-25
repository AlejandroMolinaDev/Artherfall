#pragma once

#include <cstdint>

namespace aetherfall::dsp
{

/** Tiny, fast and deterministic PRNG (xorshift32). Real-time safe; each DSP
    object owns its own instance so results do not depend on thread timing. */
class Random32
{
public:
    explicit Random32 (std::uint32_t seed = 0x9E3779B9u) noexcept { setSeed (seed); }

    void setSeed (std::uint32_t seed) noexcept { state = seed != 0 ? seed : 0x9E3779B9u; }

    std::uint32_t nextUInt() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    /** Uniform in [0, 1). */
    float nextFloat() noexcept { return (float) (nextUInt() >> 8) * (1.0f / 16777216.0f); }

    /** Uniform in [-1, 1). */
    float nextBipolar() noexcept { return nextFloat() * 2.0f - 1.0f; }

    /** Uniform integer in [0, n). */
    int nextInt (int n) noexcept { return n <= 0 ? 0 : (int) (nextUInt() % (std::uint32_t) n); }

private:
    std::uint32_t state;
};

} // namespace aetherfall::dsp
