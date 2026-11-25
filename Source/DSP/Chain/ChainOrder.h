#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "Parameters/ParamIDs.h"

// -----------------------------------------------------------------------------
// The order of the FX chain: up to 8 module types, no duplicates.
//  - persisted as text ("granular,tapedelay,filter") in the CHAIN state node,
//  - mirrored to the audio thread as one 64-bit word (one byte per slot), so
//    the audio thread reads it with a single lock-free atomic load.
// -----------------------------------------------------------------------------
namespace aetherfall::chain
{

using Order = std::array<ModuleType, (size_t) numChainSlots>;

constexpr Order emptyOrder() noexcept
{
    Order o {};
    for (auto& t : o) t = ModuleType::None;
    return o;
}

constexpr std::uint64_t pack (const Order& o) noexcept
{
    std::uint64_t v = 0;
    for (size_t i = 0; i < o.size(); ++i)
        v |= (std::uint64_t) (std::uint8_t) o[i] << (8 * i);
    return v;
}

constexpr Order unpack (std::uint64_t v) noexcept
{
    Order o = emptyOrder();
    for (size_t i = 0; i < o.size(); ++i)
    {
        const auto t = (std::uint8_t) ((v >> (8 * i)) & 0xffu);
        o[i] = t <= (std::uint8_t) numModuleTypes ? (ModuleType) t : ModuleType::None;
    }
    return o;
}

/** Removes duplicates and invalid entries, keeps the slot positions of the
    first occurrence of each module (empty slots are allowed anywhere). */
constexpr Order sanitize (const Order& in) noexcept
{
    Order out = emptyOrder();
    bool seen[(size_t) ModuleType::Count] {};
    for (size_t i = 0; i < in.size(); ++i)
    {
        const auto t = in[i];
        const auto ti = (size_t) t;
        if (t == ModuleType::None || ti > (size_t) numModuleTypes || seen[ti])
            continue;
        seen[ti] = true;
        out[i] = t;
    }
    return out;
}

inline ModuleType typeFromId (std::string_view id) noexcept
{
    for (const auto& m : moduleInfos)
        if (m.id == id)
            return m.type;
    return ModuleType::None;
}

/** "granular,,filter" -> {Granular, None, Filter, None...}. Unknown ids -> empty slot. */
inline Order fromString (std::string_view text)
{
    Order o = emptyOrder();
    size_t slot = 0, start = 0;
    while (slot < o.size() && start <= text.size())
    {
        const size_t comma = text.find (',', start);
        const auto token = text.substr (start, comma == std::string_view::npos ? std::string_view::npos : comma - start);
        o[slot++] = typeFromId (token);
        if (comma == std::string_view::npos)
            break;
        start = comma + 1;
    }
    return sanitize (o);
}

inline std::string toString (const Order& o)
{
    // Trailing empty slots are trimmed: "granular,filter".
    size_t last = 0;
    for (size_t i = 0; i < o.size(); ++i)
        if (o[i] != ModuleType::None)
            last = i + 1;

    std::string s;
    for (size_t i = 0; i < last; ++i)
    {
        if (i > 0) s += ',';
        if (o[i] != ModuleType::None)
            s += std::string (moduleInfo (o[i]).id);
    }
    return s;
}

} // namespace aetherfall::chain
