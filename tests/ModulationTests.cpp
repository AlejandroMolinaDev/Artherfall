#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "DSP/Modulation/ModulationEngine.h"
#include "DSP/Modulation/ModulatorSources.h"
#include "DSP/Analysis/InputAnalyzer.h"
#include "Parameters/ModRoutingState.h"
#include "Parameters/ParamLayout.h"
#include "PluginProcessor.h"

using namespace aetherfall;
using namespace aetherfall::test;
using Catch::Matchers::WithinAbs;

TEST_CASE ("Synced LFO is locked to the song position", "[mod]")
{
    mod::Lfo lfo;
    lfo.reset (0.0, 1);
    mod::Lfo::Settings s;
    s.shape = 0;          // sine
    s.sync = true;
    s.division = 11;      // 1/4 -> one cycle per beat

    TransportState t;
    t.bpm = 120.0;
    t.isPlaying = true;
    t.hasPpq = true;

    std::array<float, (size_t) lfoCustomPoints> custom {};
    t.ppqPosition = 10.25;   // a quarter of the way through a beat -> sin(pi/2) = 1
    REQUIRE_THAT (lfo.process (s, t, 48000.0, 64, false, false, custom), WithinAbs (1.0, 1.0e-4));
    t.ppqPosition = 3.75;    // three quarters -> -1
    REQUIRE_THAT (lfo.process (s, t, 48000.0, 64, false, false, custom), WithinAbs (-1.0, 1.0e-4));

    s.unipolar = true;
    t.ppqPosition = 0.0;     // sin(0) = 0 -> unipolar 0.5
    REQUIRE_THAT (lfo.process (s, t, 48000.0, 64, false, false, custom), WithinAbs (0.5, 1.0e-4));
}

TEST_CASE ("Free LFO runs at its rate and every shape stays in range", "[mod]")
{
    std::array<float, (size_t) lfoCustomPoints> custom {};
    for (auto& c : custom) c = 0.3f;
    TransportState t;

    for (int shape = 0; shape < 8; ++shape)
    {
        mod::Lfo lfo;
        lfo.reset (0.0, 7);
        mod::Lfo::Settings s;
        s.shape = shape;
        s.rateHz = 3.0;
        float lo = 10.0f, hi = -10.0f;
        for (int c = 0; c < 750 * 2; ++c)   // 2 s of chunks
        {
            const float v = lfo.process (s, t, 48000.0, 64, false, false, custom);
            lo = std::min (lo, v);
            hi = std::max (hi, v);
        }
        INFO ("shape " << shape);
        REQUIRE (lo >= -1.0001f);
        REQUIRE (hi <= 1.0001f);
    }

    // 3 Hz for 1 s = 3 cycles.
    mod::Lfo lfo;
    lfo.reset (0.0, 1);
    mod::Lfo::Settings s;
    s.rateHz = 3.0;
    for (int c = 0; c < 750; ++c)
        lfo.process (s, t, 48000.0, 64, false, false, custom);
    // Exactly a whole number of cycles later (the phase wraps lazily on the next call).
    const double ph = lfo.getPhase();
    REQUIRE_THAT (ph - std::round (ph), WithinAbs (0.0, 1.0e-6));
}

TEST_CASE ("Chaos and step sequencer stay bounded and follow their steps", "[mod]")
{
    mod::Chaos chaos;
    for (int mode = 0; mode < 2; ++mode)
    {
        chaos.reset (3);
        for (int c = 0; c < 20000; ++c)
        {
            const float v = chaos.process (mode, 5.0, 0.2f, 48000.0, 64);
            REQUIRE (std::isfinite (v));
            REQUIRE (std::abs (v) <= 1.0f);
        }
    }

    mod::StepSeq seq;
    seq.reset();
    std::array<float, (size_t) seqMaxSteps> steps {};
    for (int i = 0; i < seqMaxSteps; ++i) steps[(size_t) i] = (float) i / 31.0f;

    TransportState t;
    t.bpm = 120.0; t.isPlaying = true; t.hasPpq = true;
    t.ppqPosition = 0.25 * 5 + 0.1;   // 1/16 steps -> step 5
    REQUIRE_THAT (seq.process (steps, 16, 5, 0.0f, false, t, 48000.0, 64), WithinAbs (5.0f / 31.0f, 1.0e-6));
    t.ppqPosition = 0.25 * 17;        // step 17 with 16 steps -> wraps to 1
    REQUIRE_THAT (seq.process (steps, 16, 5, 0.0f, false, t, 48000.0, 64), WithinAbs (1.0f / 31.0f, 1.0e-6));
}

TEST_CASE ("Input analyzer: transients, level and brightness", "[mod]")
{
    InputAnalyzer an;
    an.prepare (48000.0);

    // Silence then a click: onset detected in the click chunk.
    std::array<float, 64> buf {};
    an.process (buf.data(), buf.data(), 64);
    REQUIRE_FALSE (an.hadOnset());
    for (int c = 0; c < 20; ++c) an.process (buf.data(), buf.data(), 64);
    buf[10] = 0.8f; buf[11] = -0.6f;
    an.process (buf.data(), buf.data(), 64);
    REQUIRE (an.hadOnset());
    REQUIRE (an.getFrame().transient > 0.1f);

    // Steady 1 kHz sine: brightness estimate close to 1 kHz, level ~ -9 dB.
    an.reset();
    std::array<float, 64> sine {};
    for (int c = 0; c < 1500; ++c)
    {
        for (int i = 0; i < 64; ++i)
            sine[(size_t) i] = 0.5f * std::sin (2.0f * 3.14159265f * 1000.0f * (float) (c * 64 + i) / 48000.0f);
        an.process (sine.data(), sine.data(), 64);
    }
    REQUIRE_THAT (an.getFrame().centroidHz, WithinAbs (1000.0, 30.0));
    REQUIRE_THAT (an.getFrame().rms, WithinAbs (0.3536, 0.01));
    REQUIRE (an.rmsNormalised() > 0.8f);
}

TEST_CASE ("Modulation engine: macros and matrix move destinations within range", "[mod]")
{
    ModulationEngine eng;
    eng.prepare (48000.0);

    ModRouting r = routing::makeDefault();
    r.slots[0] = { ModSource::Lfo1, (std::int16_t) PID::FiltCutoff, ModCurve::Linear, ModSource::None };
    eng.setRouting (r);

    ParamSet base;
    base.set (PID::Mod1Amount, 0.5f).set (PID::Lfo1Shape, 4.0f).set (PID::Lfo1Rate, 5.0f);   // square LFO
    ParamSet eff = base;

    ModulationEngine::ChunkInput in;
    in.numSamples = 64;
    std::array<float, 64> silence {};
    in.inL = in.inR = silence.data();

    const auto& cutInfo = info (PID::FiltCutoff);
    const auto range = makeRange (cutInfo);
    const float baseNorm = range.convertTo0to1 (base.get (PID::FiltCutoff));

    float lo = 1.0e9f, hi = -1.0e9f;
    for (int c = 0; c < 750; ++c)
    {
        eng.process (base.v.data(), eff.v.data(), in);
        lo = std::min (lo, eff.get (PID::FiltCutoff));
        hi = std::max (hi, eff.get (PID::FiltCutoff));
        REQUIRE (eff.get (PID::FiltCutoff) >= cutInfo.minValue);
        REQUIRE (eff.get (PID::FiltCutoff) <= cutInfo.maxValue);
    }
    // Square LFO at amount 0.5 (mod depth 1): +-0.5 of the knob travel (clamped).
    REQUIRE_THAT (hi, WithinAbs (range.convertFrom0to1 (std::min (1.0f, baseNorm + 0.5f)), 1.0f));
    REQUIRE_THAT (lo, WithinAbs (range.convertFrom0to1 (std::max (0.0f, baseNorm - 0.5f)), 1.0f));

    // HAZE macro at 100 %: core size moves +0.35 of its travel (default mapping).
    base.set (PID::MacroHaze, 1.0f);
    eng.process (base.v.data(), eff.v.data(), in);
    const auto sizeRange = makeRange (info (PID::CoreSize));
    REQUIRE_THAT (sizeRange.convertTo0to1 (eff.get (PID::CoreSize)),
                  WithinAbs (std::min (1.0f, sizeRange.convertTo0to1 (base.get (PID::CoreSize)) + 0.35f), 1.0e-4));

    // Global mod depth 0 silences the LFO slot (macros still apply).
    base.set (PID::MacroHaze, 0.0f).set (PID::GlobModDepth, 0.0f);
    eng.process (base.v.data(), eff.v.data(), in);
    REQUIRE_THAT (eff.get (PID::FiltCutoff), WithinAbs (base.get (PID::FiltCutoff), 1.0e-2));
}

TEST_CASE ("Modulation routing survives a save / load of the plug-in state", "[mod][processor]")
{
    juce::MemoryBlock state;
    {
        AetherfallProcessor p;
        p.setModSlot (3, { ModSource::Chaos2, (std::int16_t) PID::CoreDecay, ModCurve::SCurve, ModSource::Macro1 });
        p.setMacroTarget (2, 7, { (std::int16_t) PID::TapeAge, -0.25f });
        std::array<float, (size_t) seqMaxSteps> steps {};
        steps[4] = 0.75f;
        p.setSeqSteps (1, steps);
        p.getStateInformation (state);
    }

    AetherfallProcessor q;
    q.setStateInformation (state.getData(), (int) state.getSize());
    const auto r = q.getModRouting();
    REQUIRE (r.slots[3].source == ModSource::Chaos2);
    REQUIRE (r.slots[3].dest == (std::int16_t) PID::CoreDecay);
    REQUIRE (r.slots[3].curve == ModCurve::SCurve);
    REQUIRE (r.slots[3].via == ModSource::Macro1);
    REQUIRE (r.macros[2][7].dest == (std::int16_t) PID::TapeAge);
    REQUIRE_THAT (r.macros[2][7].amount, WithinAbs (-0.25f, 1.0e-4f));
    REQUIRE_THAT (r.seqSteps[1][4], WithinAbs (0.75f, 1.0e-4f));
    // Untouched default macro mapping is still there.
    REQUIRE (r.macros[0][0].dest == (std::int16_t) PID::CoreSize);
}

TEST_CASE ("Processor applies modulation to the audio path", "[mod][processor]")
{
    AetherfallProcessor p;
    p.setPlayConfigDetails (2, 2, 48000.0, 256);
    p.prepareToPlay (48000.0, 256);

    auto setParam = [&] (PID pid, float v)
    {
        auto* param = p.getValueTreeState().getParameter (juce::String (std::string (info (pid).id)));
        param->setValueNotifyingHost (param->convertTo0to1 (v));
    };

    // Square LFO on the output gain: the output level must alternate.
    setParam (PID::CoreMix, 0.0f);
    setParam (PID::OutMix, 1.0f);
    setParam (PID::Lfo1Shape, 4.0f);
    setParam (PID::Lfo1Rate, 2.0f);
    setParam (PID::Mod1Amount, -0.5f);
    p.setModSlot (0, { ModSource::Lfo1, (std::int16_t) PID::OutGain, ModCurve::Linear, ModSource::None });

    juce::AudioBuffer<float> buf (2, 256);
    juce::MidiBuffer midi;
    float lo = 10.0f, hi = 0.0f;
    for (int b = 0; b < 400; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 256; ++i)
                buf.setSample (ch, i, 0.25f);
        p.processBlock (buf, midi);
        if (b > 20)
        {
            lo = std::min (lo, std::abs (buf.getSample (0, 128)));
            hi = std::max (hi, std::abs (buf.getSample (0, 128)));
        }
    }
    INFO ("level range " << lo << " .. " << hi);
    REQUIRE (hi > lo * 4.0f);
}
