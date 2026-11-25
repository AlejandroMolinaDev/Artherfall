#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "PluginProcessor.h"

using namespace aetherfall;
using Catch::Matchers::WithinAbs;

namespace
{
    juce::RangedAudioParameter* param (AetherfallProcessor& p, PID pid)
    {
        return p.getValueTreeState().getParameter (juce::String (std::string (info (pid).id)));
    }

    /** Simulates a UI knob gesture. */
    void gesture (AetherfallProcessor& p, PID pid, float realValue)
    {
        auto* prm = param (p, pid);
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (realValue));
        prm->endChangeGesture();
    }

    float value (AetherfallProcessor& p, PID pid)
    {
        return p.getValueTreeState().getRawParameterValue (juce::String (std::string (info (pid).id)))->load();
    }
}

TEST_CASE ("Undo / redo of knob gestures and state edits", "[history]")
{
    AetherfallProcessor p;
    const float decay0 = value (p, PID::CoreDecay);

    gesture (p, PID::CoreDecay, 10.0f);
    gesture (p, PID::CoreDecay, 20.0f);
    auto order = chain::emptyOrder();
    order[0] = ModuleType::Granular;
    p.setChainOrder (order);

    REQUIRE (p.canUndo());
    REQUIRE (p.undo());                                  // chain edit
    REQUIRE (p.getChainOrder()[0] == ModuleType::None);
    REQUIRE_THAT (value (p, PID::CoreDecay), WithinAbs (20.0f, 0.05f));
    REQUIRE (p.undo());                                  // second gesture
    REQUIRE_THAT (value (p, PID::CoreDecay), WithinAbs (10.0f, 0.05f));
    REQUIRE (p.undo());                                  // first gesture
    REQUIRE_THAT (value (p, PID::CoreDecay), WithinAbs (decay0, 0.05f));
    REQUIRE_FALSE (p.canUndo());

    REQUIRE (p.redo());
    REQUIRE_THAT (value (p, PID::CoreDecay), WithinAbs (10.0f, 0.05f));
    REQUIRE (p.redo());
    REQUIRE (p.redo());
    REQUIRE (p.getChainOrder()[0] == ModuleType::Granular);
    REQUIRE_FALSE (p.canRedo());

    // Host automation (no gesture) does not create history entries.
    p.undo();
    const bool couldUndo = p.canUndo();
    param (p, PID::CoreSize)->setValueNotifyingHost (0.9f);
    REQUIRE (p.canUndo() == couldUndo);
    REQUIRE (p.canRedo());   // redo list kept: no user edit happened
}

TEST_CASE ("Undo history holds at least 50 steps", "[history]")
{
    AetherfallProcessor p;
    for (int i = 0; i < 60; ++i)
        gesture (p, PID::OutMix, (float) (i % 2 == 0 ? 0.2 : 0.8) + 0.001f * (float) i);

    int steps = 0;
    while (p.undo())
        ++steps;
    REQUIRE (steps >= 50);
}

TEST_CASE ("A/B comparison keeps two independent states", "[history]")
{
    AetherfallProcessor p;
    gesture (p, PID::CoreDecay, 5.0f);   // A
    p.toggleAB();                         // -> B (copy of A)
    REQUIRE (p.getABIndex() == 1);
    REQUIRE_THAT (value (p, PID::CoreDecay), WithinAbs (5.0f, 0.05f));
    gesture (p, PID::CoreDecay, 30.0f);  // edit B
    p.toggleAB();                         // -> A
    REQUIRE_THAT (value (p, PID::CoreDecay), WithinAbs (5.0f, 0.05f));
    p.toggleAB();                         // -> B
    REQUIRE_THAT (value (p, PID::CoreDecay), WithinAbs (30.0f, 0.1f));
}

TEST_CASE ("Randomize changes a section only and is undoable", "[history]")
{
    AetherfallProcessor p;
    const float out0 = value (p, PID::OutGain);
    const float size0 = value (p, PID::CoreSize);
    const float decay0 = value (p, PID::CoreDecay);

    p.randomize (AetherfallProcessor::RandomSection::Core, 1.0f, 42);
    REQUIRE ((std::abs (value (p, PID::CoreSize) - size0) > 1.0e-3f || std::abs (value (p, PID::CoreDecay) - decay0) > 1.0e-3f));
    REQUIRE (value (p, PID::OutGain) == out0);          // other sections untouched
    REQUIRE (value (p, PID::CoreFreeze) == 0.0f);       // never randomised

    REQUIRE (p.undo());
    REQUIRE_THAT (value (p, PID::CoreSize), WithinAbs (size0, 1.0e-4f));
}

TEST_CASE ("Assigning modulation to a parameter uses a free matrix slot", "[history][mod]")
{
    AetherfallProcessor p;
    const int slot = p.assignModulation (ModSource::Lfo3, PID::FiltCutoff, 0.25f);
    REQUIRE (slot == 0);
    REQUIRE (p.assignModulation (ModSource::Chaos1, PID::FiltCutoff) == 1);
    REQUIRE (p.assignModulation (ModSource::Lfo1, PID::CoreColor) == -1);   // choices are not modulatable

    auto r = p.getModRouting();
    REQUIRE (r.slots[0].source == ModSource::Lfo3);
    REQUIRE (r.slots[0].dest == (std::int16_t) PID::FiltCutoff);
    REQUIRE_THAT (value (p, PID::Mod1Amount), WithinAbs (0.25f, 1.0e-4f));

    p.removeModulation (PID::FiltCutoff);
    r = p.getModRouting();
    REQUIRE (r.slots[0].source == ModSource::None);
    REQUIRE (r.slots[1].source == ModSource::None);

    REQUIRE (p.undo());   // removal undone in one step
    REQUIRE (p.getModRouting().slots[1].source == ModSource::Chaos1);
}
