#include "FXChain.h"
#include "DSP/Modules/ModuleFactory.h"

namespace aetherfall
{

FXChain::FXChain() = default;
FXChain::~FXChain() = default;

void FXChain::prepare (double sr, int maxBlock)
{
    sampleRate = sr;
    maxBlockSize = std::max (1, maxBlock);

    for (int t = 1; t <= numModuleTypes; ++t)
    {
        auto& m = modules[(size_t) t];
        if (m == nullptr)
            m = createModule ((ModuleType) t);
        m->prepare (sr, maxBlockSize, 2);

        auto& st = states[(size_t) t];
        st.enable.prepare (sr, 0.015);
        st.mix.prepare (sr, 0.02);
        st.processing = false;
        for (auto& d : st.dryComp)   d.prepare (maxModuleLatency + 8);
        for (auto& d : st.groupComp) d.prepare (maxModuleLatency + 8);
    }

    for (auto* b : { &chainInput, &groupInput, &groupSum, &branch })
        b->setSize (2, maxBlockSize, false, true, false);
    groupWeight.assign ((size_t) maxBlockSize, 0.0f);
    enableBuf.assign ((size_t) maxBlockSize, 0.0f);

    fade.prepare (sr, 0.010);
    reset();
}

void FXChain::reset()
{
    for (int t = 1; t <= numModuleTypes; ++t)
    {
        if (auto& m = modules[(size_t) t])
            m->reset();
        states[(size_t) t].processing = false;
    }

    // Apply any pending order immediately (no fade needed after a reset).
    active = chain::unpack (requested);
    activePacked = requested;
    currentLatency = estimateLatency (active);
    for (int t = 1; t <= numModuleTypes; ++t)
    {
        for (auto& d : states[(size_t) t].dryComp)   d.reset();
        for (auto& d : states[(size_t) t].groupComp) d.reset();
    }
    fadeState = Fade::None;
    fade.setCurrentAndTarget (1.0f);
}

void FXChain::applyPendingOrder()
{
    active = chain::unpack (requested);
    activePacked = requested;

    // Modules that were removed stop processing; modules (re-)added start
    // from a clean state on their first chunk.
    bool inChain[(size_t) ModuleType::Count] {};
    for (auto t : active)
        inChain[(size_t) t] = true;

    for (int t = 1; t <= numModuleTypes; ++t)
        if (! inChain[(size_t) t])
            states[(size_t) t].processing = false;
}

int FXChain::estimateLatency (const chain::Order& order) const
{
    int total = 0;
    for (auto t : order)
        if (t != ModuleType::None && modules[(size_t) t] != nullptr)
            total += modules[(size_t) t]->getLatencySamples();
    return total;
}

double FXChain::getTailSeconds() const
{
    double tail = 0.0;
    for (auto t : active)
        if (t != ModuleType::None && modules[(size_t) t] != nullptr)
            tail += modules[(size_t) t]->getTailSeconds();
    return tail;
}

//==============================================================================
void FXChain::process (juce::AudioBuffer<float>& io, const ModContext& ctx)
{
    const int n = std::min (ctx.numSamples, maxBlockSize);
    if (n <= 0)
        return;

    // New order requested? Fade the chain out first (also from a fade-in).
    if (requested != activePacked && fadeState != Fade::Out)
    {
        fadeState = Fade::Out;
        fade.setTarget (0.0f);
    }

    bool empty = true;
    for (auto t : active)
        empty = empty && (t == ModuleType::None);

    if (empty && fadeState == Fade::None)
        return;

    for (int ch = 0; ch < 2; ++ch)
        chainInput.copyFrom (ch, 0, io, ch, 0, n);

    // Solo: if any enabled module is soloed, the others are bypassed.
    bool anySolo = false;
    for (auto t : active)
        if (t != ModuleType::None)
        {
            const auto& mi = moduleInfo (t);
            anySolo = anySolo || (ctx.getBool (mi.solo) && ctx.getBool (mi.on));
        }

    float* const io0 = io.getWritePointer (0);
    float* const io1 = io.getWritePointer (1);
    float* const gi0 = groupInput.getWritePointer (0);
    float* const gi1 = groupInput.getWritePointer (1);
    float* const gs0 = groupSum.getWritePointer (0);
    float* const gs1 = groupSum.getWritePointer (1);
    float* const br0 = branch.getWritePointer (0);
    float* const br1 = branch.getWritePointer (1);
    float* const gw  = groupWeight.data();

    // --- Pre-scan: group membership and the latency of each group (a group is a
    //     serial slot plus the parallel slots that follow it).
    std::array<int, (size_t) numChainSlots> slotGroup {};
    std::array<int, (size_t) numChainSlots> groupLatency {};
    {
        int g = -1;
        for (size_t s = 0; s < active.size(); ++s)
        {
            const auto type = active[s];
            if (type == ModuleType::None)
                continue;
            if (g < 0 || ! ctx.getBool (moduleInfo (type).parallel))
                ++g;
            slotGroup[s] = g;
            groupLatency[(size_t) g] = std::max (groupLatency[(size_t) g], modules[(size_t) type]->getLatencySamples());
        }
        int total = 0;
        for (int k = 0; k <= g; ++k)
            total += groupLatency[(size_t) k];
        currentLatency = total;
    }

    bool groupOpen = false;

    auto closeGroup = [&]
    {
        // Weighted average of the branches of the group -> running signal.
        for (int i = 0; i < n; ++i)
        {
            const float inv = 1.0f / std::max (gw[i], 1.0e-6f);
            io0[i] = gs0[i] * inv;
            io1[i] = gs1[i] * inv;
        }
    };

    // Delays a branch output so every branch of a group has the same latency.
    auto alignBranch = [&] (ModuleState& st, float* b0, float* b1, int delay)
    {
        if (delay <= 0)
            return;
        for (int i = 0; i < n; ++i)
        {
            st.groupComp[0].write (b0[i]);
            st.groupComp[1].write (b1[i]);
            b0[i] = st.groupComp[0].tap (delay);
            b1[i] = st.groupComp[1].tap (delay);
        }
    };

    for (size_t slot = 0; slot < active.size(); ++slot)
    {
        const auto type = active[slot];
        if (type == ModuleType::None)
            continue;

        const auto& mi = moduleInfo (type);
        auto& st = states[(size_t) type];
        auto* module = modules[(size_t) type].get();

        const bool on = ctx.getBool (mi.on) && (! anySolo || ctx.getBool (mi.solo));
        const bool parallel = groupOpen && ctx.getBool (mi.parallel);
        const int latency = module->getLatencySamples();
        const int gLatency = groupLatency[(size_t) slotGroup[slot]];

        if (! parallel)
        {
            if (groupOpen)
                closeGroup();

            std::copy_n (io0, n, gi0);
            std::copy_n (io1, n, gi1);
            std::fill_n (gs0, n, 0.0f);
            std::fill_n (gs1, n, 0.0f);
            std::fill_n (gw, n, 0.0f);
            groupOpen = true;
        }

        const bool isHead = ! parallel;
        st.enable.setTarget (on ? 1.0f : 0.0f);
        st.mix.setTarget (ctx.get (mi.mix));

        // Modules with latency always run while in the chain, so their delayed
        // dry/wet paths stay continuous when they are switched on and off.
        const bool needsProcessing = latency > 0 || on || st.enable.isSmoothing() || st.enable.getCurrent() > 0.0f;

        if (! needsProcessing)
        {
            st.processing = false;
            st.enable.skip (n);
            st.mix.skip (n);

            if (isHead)   // a bypassed serial slot passes its input through
            {
                std::copy_n (gi0, n, br0);
                std::copy_n (gi1, n, br1);
                alignBranch (st, br0, br1, gLatency);
                for (int i = 0; i < n; ++i)
                {
                    gs0[i] += br0[i];
                    gs1[i] += br1[i];
                    gw[i] += 1.0f;
                }
            }
            continue;
        }

        if (! st.processing)
        {
            // (Re)started: clean state, no ramp from stale smoother values.
            module->reset();
            st.mix.setCurrentAndTarget (ctx.get (mi.mix));
            for (auto& d : st.dryComp)   d.reset();
            for (auto& d : st.groupComp) d.reset();
            st.processing = true;
        }

        std::copy_n (gi0, n, br0);
        std::copy_n (gi1, n, br1);

        juce::AudioBuffer<float> view (branch.getArrayOfWritePointers(), 2, 0, n);
        module->process (view, ctx);

        float* const eb = enableBuf.data();
        for (int i = 0; i < n; ++i)
        {
            const float e = st.enable.next();
            const float m = st.mix.next();
            const float amt = e * m;
            eb[i] = e;

            float d0 = gi0[i], d1 = gi1[i];
            if (latency > 0)
            {
                // Dry reference delayed like the module's wet output.
                st.dryComp[0].write (d0);
                st.dryComp[1].write (d1);
                d0 = st.dryComp[0].tap (latency);
                d1 = st.dryComp[1].tap (latency);
            }

            br0[i] = d0 + amt * (br0[i] - d0);
            br1[i] = d1 + amt * (br1[i] - d1);
        }

        alignBranch (st, br0, br1, gLatency - latency);

        // Parallel branches are weighted by their on/off ramp, so they drop out
        // of the average smoothly when switched off.
        for (int i = 0; i < n; ++i)
        {
            const float w = isHead ? 1.0f : eb[i];
            gs0[i] += w * br0[i];
            gs1[i] += w * br1[i];
            gw[i] += w;
        }
    }

    if (groupOpen)
        closeGroup();
    else
    {
        // Chain became empty while fading: output == input.
        for (int ch = 0; ch < 2; ++ch)
            io.copyFrom (ch, 0, chainInput, ch, 0, n);
    }

    // Re-order crossfade towards / from the chain input.
    if (fadeState != Fade::None)
    {
        const float* ci0 = chainInput.getReadPointer (0);
        const float* ci1 = chainInput.getReadPointer (1);
        for (int i = 0; i < n; ++i)
        {
            const float g = fade.next();
            io0[i] = ci0[i] + g * (io0[i] - ci0[i]);
            io1[i] = ci1[i] + g * (io1[i] - ci1[i]);
        }

        if (fadeState == Fade::Out && ! fade.isSmoothing())
        {
            applyPendingOrder();
            fadeState = Fade::In;
            fade.setTarget (1.0f);
        }
        else if (fadeState == Fade::In && ! fade.isSmoothing())
        {
            fadeState = Fade::None;
        }
    }
}

} // namespace aetherfall
