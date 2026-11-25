#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include "IModule.h"
#include "ChainOrder.h"
#include "DSP/Utils/Smoothers.h"

namespace aetherfall
{

/**
    The 8-slot FX chain (spec section 3).

    - Owns one instance of every module type (created in prepare()), so moving
      a module to another slot keeps its state and never allocates.
    - Per module: on/off (smoothed), dry/wet mix (smoothed), solo, parallel.
      Solo: if any module in the chain is soloed, only soloed modules are
      active (the others pass the signal through).
      Parallel: a slot flagged "parallel" receives the same input as the
      previous serial slot; the group outputs are averaged (a disabled
      parallel branch drops out of the average smoothly).
    - Re-ordering: the chain output fades to the chain input (~10 ms), the new
      order is applied at that point, and the output fades back in. Both ends
      of the crossfade are continuous signals, so there is no click.
*/
class FXChain
{
public:
    FXChain();
    ~FXChain();

    void prepare (double sampleRate, int maxBlock);
    void reset();

    /** Requested order, packed (see ChainOrder.h). Audio thread, per chunk. */
    void setRequestedOrder (std::uint64_t packedOrder) noexcept { requested = packedOrder; }

    /** Processes one chunk in place (stereo). */
    void process (juce::AudioBuffer<float>& io, const ModContext& ctx);

    /** Order currently being processed (may lag the requested one during a fade). */
    chain::Order getActiveOrder() const noexcept { return active; }
    bool isTransitioning() const noexcept { return fadeState != Fade::None; }

    int getLatencySamples() const;
    double getTailSeconds() const;

    IModule* getModule (ModuleType t) noexcept { return modules[(size_t) t].get(); }

private:
    struct ModuleState
    {
        dsp::LinearSmoother<float> enable;   // 0 = bypassed, 1 = active
        dsp::LinearSmoother<float> mix;
        bool processing = false;             // was processed in the previous chunk
    };

    enum class Fade { None, Out, In };

    void applyPendingOrder();

    std::array<std::unique_ptr<IModule>, (size_t) ModuleType::Count> modules;
    std::array<ModuleState, (size_t) ModuleType::Count> states;

    chain::Order active = chain::emptyOrder();
    std::uint64_t activePacked = 0, requested = 0;

    Fade fadeState = Fade::None;
    dsp::LinearSmoother<float> fade;

    juce::AudioBuffer<float> chainInput, groupInput, groupSum, branch;
    std::vector<float> groupWeight;

    double sampleRate = 48000.0;
    int maxBlockSize = 64;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FXChain)
};

} // namespace aetherfall
