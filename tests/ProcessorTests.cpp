#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "PluginProcessor.h"
#include "DSP/Utils/Random.h"

using aetherfall::AetherfallProcessor;
using aetherfall::PID;

namespace
{
    void setParam (AetherfallProcessor& p, PID pid, float realValue)
    {
        const auto& info = aetherfall::info (pid);
        auto* param = p.getValueTreeState().getParameter (juce::String (std::string (info.id)));
        REQUIRE (param != nullptr);
        param->setValueNotifyingHost (param->convertTo0to1 (realValue));
    }

    /** Processes `seconds` of noise in blocks of variable size; returns peak. */
    float runNoise (AetherfallProcessor& p, double sr, double seconds, int maxBlock, bool& finite,
                    bool variableBlocks = true)
    {
        aetherfall::dsp::Random32 rng (42);
        juce::AudioBuffer<float> buffer (2, maxBlock);
        juce::MidiBuffer midi;
        float peak = 0.0f;
        finite = true;

        int remaining = (int) (sr * seconds);
        while (remaining > 0)
        {
            int n = variableBlocks ? 1 + rng.nextInt (maxBlock) : maxBlock;
            n = std::min (n, remaining);
            buffer.setSize (2, n, false, false, true);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i)
                    buffer.setSample (ch, i, 0.3f * rng.nextBipolar());

            p.processBlock (buffer, midi);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i)
                {
                    const float y = buffer.getSample (ch, i);
                    finite = finite && std::isfinite (y);
                    peak = std::max (peak, std::abs (y));
                }
            remaining -= n;
        }
        return peak;
    }
}

TEST_CASE ("Processor runs with any block size and stays finite", "[processor]")
{
    AetherfallProcessor p;
    p.setPlayConfigDetails (2, 2, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);

    bool finite = false;
    // Hosts (FL Studio included) may send blocks larger than announced.
    const float peak = runNoise (p, 48000.0, 3.0, 2048, finite);
    REQUIRE (finite);
    REQUIRE (peak < 8.0f);
}

TEST_CASE ("Processor handles mono input (mono -> stereo)", "[processor]")
{
    AetherfallProcessor p;
    p.setPlayConfigDetails (1, 2, 48000.0, 256);
    p.prepareToPlay (48000.0, 256);

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;
    for (int block = 0; block < 50; ++block)
    {
        buffer.clear();
        for (int i = 0; i < 256; ++i)
            buffer.setSample (0, i, std::sin (0.05f * (float) (block * 256 + i)));
        p.processBlock (buffer, midi);
    }

    float diff = 0.0f;
    for (int i = 0; i < 256; ++i)
        diff = std::max (diff, std::abs (buffer.getSample (0, i)) + std::abs (buffer.getSample (1, i)));
    REQUIRE (diff > 0.0f);   // right channel is not silent
}

TEST_CASE ("Processor survives sample-rate and buffer changes", "[processor]")
{
    AetherfallProcessor p;

    for (double sr : { 44100.0, 96000.0, 48000.0, 192000.0, 22050.0 })
        for (int block : { 32, 480, 1024 })
        {
            p.setPlayConfigDetails (2, 2, sr, block);
            p.prepareToPlay (sr, block);
            bool finite = false;
            runNoise (p, sr, 0.3, block, finite, false);
            INFO ("sr " << sr << " block " << block);
            REQUIRE (finite);
            p.releaseResources();
        }
}

TEST_CASE ("Bypass returns the dry signal", "[processor]")
{
    AetherfallProcessor p;
    p.setPlayConfigDetails (2, 2, 48000.0, 256);
    p.prepareToPlay (48000.0, 256);
    setParam (p, PID::Bypass, 1.0f);
    p.reset();

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;
    aetherfall::dsp::Random32 rng (1);
    float maxErr = 0.0f;

    for (int block = 0; block < 40; ++block)
    {
        juce::AudioBuffer<float> in (2, 256);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 256; ++i)
                in.setSample (ch, i, 0.4f * rng.nextBipolar());
        buffer.makeCopyOf (in);
        p.processBlock (buffer, midi);
        if (block > 4)
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 256; ++i)
                    maxErr = std::max (maxErr, std::abs (buffer.getSample (ch, i) - in.getSample (ch, i)));
    }
    REQUIRE (maxErr < 1.0e-5f);
}

TEST_CASE ("Tails keep ringing after the input stops", "[processor]")
{
    AetherfallProcessor p;
    p.setPlayConfigDetails (2, 2, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);
    setParam (p, PID::OutMix, 1.0f);
    setParam (p, PID::CoreDecay, 6.0f);

    bool finite = false;
    runNoise (p, 48000.0, 1.0, 512, finite, false);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    double energy = 0.0;
    for (int block = 0; block < 94; ++block)   // ~1 s of silence
    {
        buffer.clear();
        p.processBlock (buffer, midi);
        if (block > 80)
            for (int i = 0; i < 512; ++i)
                energy += (double) buffer.getSample (0, i) * buffer.getSample (0, i);
    }
    REQUIRE (energy > 1.0e-4);
    REQUIRE (p.getTailLengthSeconds() > 6.0);
}

TEST_CASE ("State save / restore round trip", "[processor]")
{
    juce::MemoryBlock state;
    {
        AetherfallProcessor p;
        setParam (p, PID::CoreDecay, 12.5f);
        setParam (p, PID::CoreColor, 4.0f);
        setParam (p, PID::OutMix, 0.8f);
        setParam (p, PID::GranFreeze, 1.0f);
        p.getStateInformation (state);
    }

    AetherfallProcessor q;
    q.setStateInformation (state.getData(), (int) state.getSize());
    auto& apvts = q.getValueTreeState();
    REQUIRE_THAT (apvts.getRawParameterValue ("core_decay")->load(), Catch::Matchers::WithinRel (12.5f, 1.0e-3f));
    REQUIRE (apvts.getRawParameterValue ("core_color")->load() == 4.0f);
    REQUIRE_THAT (apvts.getRawParameterValue ("out_mix")->load(), Catch::Matchers::WithinAbs (0.8f, 1.0e-4f));
    REQUIRE (apvts.getRawParameterValue ("gran_freeze")->load() == 1.0f);

    // Garbage must be ignored without crashing.
    const char junk[] = "not a valid state";
    q.setStateInformation (junk, (int) sizeof (junk));
    REQUIRE (apvts.getRawParameterValue ("core_color")->load() == 4.0f);
}

TEST_CASE ("Bus layouts: mono->stereo and stereo->stereo only", "[processor]")
{
    AetherfallProcessor p;
    juce::AudioProcessor::BusesLayout l;

    l.inputBuses.add (juce::AudioChannelSet::stereo());
    l.outputBuses.add (juce::AudioChannelSet::stereo());
    REQUIRE (p.checkBusesLayoutSupported (l));

    l.inputBuses.getReference (0) = juce::AudioChannelSet::mono();
    REQUIRE (p.checkBusesLayoutSupported (l));

    l.outputBuses.getReference (0) = juce::AudioChannelSet::mono();
    REQUIRE_FALSE (p.checkBusesLayoutSupported (l));
}

TEST_CASE ("Processor CPU benchmark with a typical chain", "[.bench]")
{
    AetherfallProcessor p;
    p.setPlayConfigDetails (2, 2, 48000.0, 256);
    p.prepareToPlay (48000.0, 256);

    auto order = aetherfall::chain::emptyOrder();
    order[0] = aetherfall::ModuleType::Granular;
    order[1] = aetherfall::ModuleType::TapeDelay;
    order[2] = aetherfall::ModuleType::Filter;
    order[3] = aetherfall::ModuleType::Saturator;
    order[4] = aetherfall::ModuleType::Stereo;
    p.setChainOrder (order);
    setParam (p, PID::CoreShimmer, 0.4f);

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;
    aetherfall::dsp::Random32 rng (1);
    const int blocks = 48000 * 20 / 256;
    const auto t0 = juce::Time::getHighResolutionTicks();
    for (int b = 0; b < blocks; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 256; ++i)
                buffer.setSample (ch, i, 0.2f * rng.nextBipolar());
        p.processBlock (buffer, midi);
    }
    const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
    WARN ("typical chain + core: " << 100.0 * secs / (blocks * 256 / 48000.0) << " % of one core");
}

TEST_CASE ("Processor CPU benchmark with a heavy chain", "[.bench]")
{
    using aetherfall::ModuleType;
    AetherfallProcessor p;
    p.setPlayConfigDetails (2, 2, 48000.0, 256);
    p.prepareToPlay (48000.0, 256);

    auto order = aetherfall::chain::emptyOrder();
    const ModuleType heavy[] = { ModuleType::Spectral, ModuleType::Granular, ModuleType::Drone, ModuleType::Resonator,
                                 ModuleType::ShimmerDelay, ModuleType::MultiTap, ModuleType::ModFX, ModuleType::Saturator };
    for (size_t i = 0; i < 8; ++i) order[i] = heavy[i];
    p.setChainOrder (order);
    setParam (p, PID::CoreShimmer, 0.4f);
    setParam (p, PID::SatOversample, 2.0f);

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;
    aetherfall::dsp::Random32 rng (1);
    const int blocks = 48000 * 10 / 256;
    const auto t0 = juce::Time::getHighResolutionTicks();
    for (int b = 0; b < blocks; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 256; ++i)
                buffer.setSample (ch, i, 0.2f * rng.nextBipolar());
        p.processBlock (buffer, midi);
    }
    const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
    WARN ("heavy 8-module chain + core: " << 100.0 * secs / (blocks * 256 / 48000.0) << " % of one core");
}

TEST_CASE ("Every quality mode processes a heavy chain cleanly", "[processor]")
{
    using aetherfall::ModuleType;
    for (float quality : { 0.0f, 1.0f, 2.0f })
    {
        AetherfallProcessor p;
        p.setPlayConfigDetails (2, 2, 48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        auto order = aetherfall::chain::emptyOrder();
        const ModuleType mods[] = { ModuleType::Spectral, ModuleType::Granular, ModuleType::Saturator, ModuleType::ModFX };
        for (size_t i = 0; i < 4; ++i) order[i] = mods[i];
        p.setChainOrder (order);
        setParam (p, PID::GlobQuality, quality);
        setParam (p, PID::CoreShimmer, 0.5f);

        bool finite = false;
        const float peak = runNoise (p, 48000.0, 1.0, 512, finite, false);
        INFO ("quality " << quality);
        REQUIRE (finite);
        REQUIRE (peak <= 1.0f);
        REQUIRE (p.getLatencySamples() == 4096);   // latency independent of quality
    }
}
