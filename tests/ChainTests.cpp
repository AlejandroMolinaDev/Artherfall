#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <vector>
#include "TestHelpers.h"
#include "DSP/Chain/FXChain.h"
#include "DSP/Chain/ChainOrder.h"
#include "PluginProcessor.h"

using namespace aetherfall;
using namespace aetherfall::test;

namespace
{
    chain::Order makeOrder (std::initializer_list<ModuleType> types)
    {
        auto o = chain::emptyOrder();
        size_t i = 0;
        for (auto t : types)
            o[i++] = t;
        return o;
    }

    /** Runs the chain on a mono input; returns the left output. */
    std::vector<float> runChain (FXChain& fx, ParamSet& ps, int numSamples,
                                 const std::function<float (int)>& input,
                                 const std::function<void (int)>& onChunk = {})
    {
        std::vector<float> out;
        juce::AudioBuffer<float> buf (2, 64);
        ModContext ctx;
        ctx.values = ps.v.data();
        ctx.sampleRate = 48000.0;

        for (int start = 0; start < numSamples; start += 64)
        {
            const int len = std::min (64, numSamples - start);
            if (onChunk) onChunk (start);
            juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), 2, 0, len);
            for (int i = 0; i < len; ++i)
            {
                const float x = input (start + i);
                view.setSample (0, i, x);
                view.setSample (1, i, x);
            }
            ctx.numSamples = len;
            fx.process (view, ctx);
            for (int i = 0; i < len; ++i)
                out.push_back (view.getSample (0, i));
        }
        return out;
    }

    float sine (int n) { return 0.5f * std::sin (2.0f * 3.14159265f * 220.0f * (float) n / 48000.0f); }
}

TEST_CASE ("Chain order: pack/unpack/string round trips and sanitising", "[chain]")
{
    const auto o = makeOrder ({ ModuleType::Granular, ModuleType::None, ModuleType::Filter, ModuleType::Granular });
    const auto clean = chain::sanitize (o);
    REQUIRE (clean[0] == ModuleType::Granular);
    REQUIRE (clean[2] == ModuleType::Filter);
    REQUIRE (clean[3] == ModuleType::None);   // duplicate removed

    REQUIRE (chain::unpack (chain::pack (clean)) == clean);
    REQUIRE (chain::toString (clean) == "granular,,filter");
    REQUIRE (chain::fromString ("granular,,filter") == clean);
    REQUIRE (chain::fromString ("bogus,filter")[1] == ModuleType::Filter);
    REQUIRE (chain::fromString ("")[0] == ModuleType::None);
}

TEST_CASE ("Empty chain is transparent", "[chain]")
{
    FXChain fx;
    fx.prepare (48000.0, 64);
    ParamSet ps;
    const auto out = runChain (fx, ps, 4800, sine);
    for (int n = 0; n < 4800; ++n)
        REQUIRE (out[(size_t) n] == sine (n));
}

TEST_CASE ("Re-ordering the chain in real time does not click", "[chain]")
{
    FXChain fx;
    fx.prepare (48000.0, 64);

    ParamSet ps;
    ps.set (PID::SatDrive, 1.0f).set (PID::SatMix, 1.0f).set (PID::SatMode, 3.0f)   // folder: very different
      .set (PID::FiltCutoff, 400.0f).set (PID::FiltMix, 1.0f);

    const auto a = chain::pack (makeOrder ({ ModuleType::Saturator, ModuleType::Filter }));
    const auto b = chain::pack (makeOrder ({ ModuleType::Filter, ModuleType::Saturator }));
    fx.setRequestedOrder (a);
    fx.reset();

    float maxStep = 0.0f, steadyStep = 0.0f;
    const auto out = runChain (fx, ps, 48000 * 2, sine,
                               [&] (int n) { if (n > 0 && n % 4800 == 0) fx.setRequestedOrder ((n / 4800) % 2 ? b : a); });

    // Each order has its own steady-state "roughness" (the folder after the
    // filter has sharper edges than before it), so compare the transitions
    // (first 1200 samples after each change) with the steady state of BOTH
    // orders (last half of every segment).
    for (size_t i = 1; i < out.size(); ++i)
    {
        const float step = std::abs (out[i] - out[i - 1]);
        const size_t posInSegment = i % 4800;
        if (posInSegment >= 2400)
            steadyStep = std::max (steadyStep, step);
        else if (i >= 4800 && posInSegment < 1200)
            maxStep = std::max (maxStep, step);
    }

    INFO ("steady max step " << steadyStep << ", max step while re-ordering " << maxStep);
    REQUIRE (maxStep < steadyStep * 1.25f + 0.01f);
    REQUIRE (fx.getActiveOrder() == chain::unpack (b));   // 19 changes -> ends on b
}

TEST_CASE ("Adding, removing and moving smooth modules does not click", "[chain]")
{
    FXChain fx;
    fx.prepare (48000.0, 64);

    ParamSet ps;
    ps.set (PID::SatMode, 0.0f).set (PID::SatDrive, 0.4f).set (PID::SatMix, 1.0f)
      .set (PID::FiltCutoff, 900.0f).set (PID::FiltMix, 1.0f)
      .set (PID::TapeMix, 0.5f).set (PID::TapeSync, 0.0f).set (PID::TapeTime, 120.0f);

    const std::uint64_t orders[] = {
        chain::pack (makeOrder ({ ModuleType::Filter, ModuleType::Saturator })),
        chain::pack (makeOrder ({ ModuleType::Saturator, ModuleType::Filter })),
        chain::pack (makeOrder ({ ModuleType::Filter })),
        chain::pack (makeOrder ({ ModuleType::TapeDelay, ModuleType::Filter, ModuleType::Saturator })),
        chain::pack (makeOrder ({})),
    };

    fx.setRequestedOrder (orders[0]);
    fx.reset();

    const auto out = runChain (fx, ps, 4800 * 15, sine,
                               [&] (int n) { if (n > 0 && n % 4800 == 0) fx.setRequestedOrder (orders[(n / 4800) % 5]); });

    float maxStep = 0.0f;
    for (size_t i = 1; i < out.size(); ++i)
        maxStep = std::max (maxStep, std::abs (out[i] - out[i - 1]));

    // A 220 Hz sine of amplitude 0.5 moves at most ~0.0145 per sample; the
    // processed versions stay in that ballpark. A click would be >> 0.05.
    INFO ("max step " << maxStep);
    REQUIRE (maxStep < 0.04f);
}

TEST_CASE ("Solo bypasses the other modules", "[chain]")
{
    FXChain fx;
    fx.prepare (48000.0, 64);

    ParamSet ps;
    ps.set (PID::SatDrive, 1.0f).set (PID::SatMix, 1.0f)    // would distort heavily
      .set (PID::FiltMix, 0.0f).set (PID::FiltSolo, 1.0f);  // soloed, fully dry

    fx.setRequestedOrder (chain::pack (makeOrder ({ ModuleType::Saturator, ModuleType::Filter })));
    fx.reset();

    const auto out = runChain (fx, ps, 9600, sine);
    float maxErr = 0.0f;
    for (int n = 4800; n < 9600; ++n)
        maxErr = std::max (maxErr, std::abs (out[(size_t) n] - sine (n)));
    REQUIRE (maxErr < 1.0e-4f);
}

TEST_CASE ("Parallel slots share the input and are averaged", "[chain]")
{
    FXChain fx;
    fx.prepare (48000.0, 64);

    // Head: Saturator at mix 0 (dry). Parallel: Filter at mix 0 (dry).
    // Average of two dry branches == input.
    ParamSet ps;
    ps.set (PID::SatMix, 0.0f).set (PID::FiltMix, 0.0f).set (PID::FiltParallel, 1.0f);
    fx.setRequestedOrder (chain::pack (makeOrder ({ ModuleType::Saturator, ModuleType::Filter })));
    fx.reset();

    auto out = runChain (fx, ps, 4800, sine);
    float maxErr = 0.0f;
    for (int n = 2400; n < 4800; ++n)
        maxErr = std::max (maxErr, std::abs (out[(size_t) n] - sine (n)));
    REQUIRE (maxErr < 1.0e-5f);

    // Disabling the parallel branch removes it from the average (still == input).
    ps.set (PID::FiltOn, 0.0f);
    out = runChain (fx, ps, 4800, sine);
    maxErr = 0.0f;
    for (int n = 2400; n < 4800; ++n)
        maxErr = std::max (maxErr, std::abs (out[(size_t) n] - sine (n)));
    REQUIRE (maxErr < 1.0e-5f);
}

TEST_CASE ("Processor: chain order is saved with the state and reaches the audio thread", "[chain][processor]")
{
    const auto order = makeOrder ({ ModuleType::Granular, ModuleType::TapeDelay, ModuleType::Stereo });

    juce::MemoryBlock state;
    {
        AetherfallProcessor p;
        p.setChainOrder (order);
        REQUIRE (p.getChainOrder() == order);
        p.getStateInformation (state);
    }

    AetherfallProcessor q;
    q.setPlayConfigDetails (2, 2, 48000.0, 512);
    q.prepareToPlay (48000.0, 512);
    q.setStateInformation (state.getData(), (int) state.getSize());
    REQUIRE (q.getChainOrder() == order);

    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    for (int i = 0; i < 20; ++i)
    {
        buf.clear();
        q.processBlock (buf, midi);
    }
    REQUIRE (q.getActiveChainOrder() == order);
}
