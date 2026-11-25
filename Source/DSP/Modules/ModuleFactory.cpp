#include "ModuleFactory.h"

#include "GranularCloud.h"
#include "TapeDelay.h"
#include "FilterSweep.h"
#include "Saturator.h"
#include "StereoEngine.h"
#include "Bitcrush.h"
#include "RingFreq.h"
#include "ModFX.h"
#include "ShimmerDelay.h"
#include "MultiTap.h"
#include "Comb.h"
#include "TranceGate.h"
#include "ReverseSwell.h"
#include "TextureLayer.h"
#include "LoFiTape.h"
#include "ResonatorBank.h"
#include "DroneGenerator.h"
#include "SpectralFreeze.h"

namespace aetherfall
{

namespace
{
    /** Placeholder for modules of phase F4: outputs its input unchanged. */
    class PassthroughModule final : public ModuleBase
    {
    public:
        explicit PassthroughModule (ModuleType t) : type (t) {}
        void reset() override {}
        void process (juce::AudioBuffer<float>&, const ModContext&) override {}
        ModuleType getType() const override { return type; }

    protected:
        void prepareModule() override {}

    private:
        ModuleType type;
    };
}

std::unique_ptr<IModule> createModule (ModuleType type)
{
    switch (type)
    {
        case ModuleType::Granular:   return std::make_unique<GranularCloud>();
        case ModuleType::TapeDelay:  return std::make_unique<TapeDelay>();
        case ModuleType::Filter:     return std::make_unique<FilterSweep>();
        case ModuleType::Saturator:  return std::make_unique<Saturator>();
        case ModuleType::Stereo:     return std::make_unique<StereoEngine>();
        case ModuleType::Bitcrush:   return std::make_unique<Bitcrush>();
        case ModuleType::RingFreq:   return std::make_unique<RingFreq>();
        case ModuleType::ModFX:      return std::make_unique<ModFX>();
        case ModuleType::ShimmerDelay: return std::make_unique<ShimmerDelay>();
        case ModuleType::MultiTap:   return std::make_unique<MultiTap>();
        case ModuleType::Comb:       return std::make_unique<Comb>();
        case ModuleType::Gate:       return std::make_unique<TranceGate>();
        case ModuleType::ReverseSwell: return std::make_unique<ReverseSwell>();
        case ModuleType::Texture:    return std::make_unique<TextureLayer>();
        case ModuleType::LoFi:       return std::make_unique<LoFiTape>();
        case ModuleType::Resonator:  return std::make_unique<ResonatorBank>();
        case ModuleType::Drone:      return std::make_unique<DroneGenerator>();
        case ModuleType::Spectral:   return std::make_unique<SpectralFreeze>();

        case ModuleType::None:
        case ModuleType::Count:
            break;
    }
    return std::make_unique<PassthroughModule> (type);
}

bool isModuleImplemented (ModuleType type) noexcept
{
    constexpr ModuleType implemented[] = {
        ModuleType::Granular, ModuleType::TapeDelay, ModuleType::Filter,
        ModuleType::Saturator, ModuleType::Stereo, ModuleType::Bitcrush, ModuleType::RingFreq,
        ModuleType::ModFX, ModuleType::ShimmerDelay, ModuleType::MultiTap, ModuleType::Comb,
        ModuleType::Gate, ModuleType::ReverseSwell, ModuleType::Texture, ModuleType::LoFi,
        ModuleType::Resonator, ModuleType::Drone, ModuleType::Spectral
    };

    for (auto t : implemented)
        if (t == type)
            return true;
    return false;
}

} // namespace aetherfall
