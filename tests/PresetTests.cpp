#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <set>
#include "PluginProcessor.h"
#include "Presets/PresetManager.h"

using namespace aetherfall;
using Catch::Matchers::WithinAbs;

namespace
{
    float value (AetherfallProcessor& p, PID pid)
    {
        return p.getValueTreeState().getRawParameterValue (juce::String (std::string (info (pid).id)))->load();
    }

    juce::File tempPresetFolder()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("aetherfall_preset_tests");
        dir.deleteRecursively();
        dir.createDirectory();
        return dir;
    }
}

TEST_CASE ("Factory bank: at least 60 presets, every category covered, unique names", "[presets]")
{
    const auto& bank = factoryPresets();
    REQUIRE (bank.size() >= 60);

    std::set<std::string> names;
    std::array<int, numFactoryCategories> perCategory {};
    for (const auto& p : bank)
    {
        REQUIRE (names.insert (p.name).second);
        REQUIRE ((int) p.category < numFactoryCategories);
        ++perCategory[(size_t) p.category];
        for (const auto& [pid, v] : p.values)
        {
            INFO (p.name << " / " << std::string (info (pid).id) << " = " << v);
            REQUIRE (v >= info (pid).rangeMin());
            REQUIRE (v <= info (pid).rangeMax());
        }
    }
    for (int c : perCategory)
        REQUIRE (c >= 5);
}

TEST_CASE ("Every factory preset loads and processes audio cleanly", "[presets]")
{
    AetherfallProcessor p;
    p.setPlayConfigDetails (2, 2, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);
    auto& pm = p.getPresetManager();
    pm.setUserFolder (tempPresetFolder());

    juce::Random rng (11);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;

    const auto count = (int) factoryPresets().size();
    for (int i = 0; i < count; ++i)
    {
        REQUIRE (pm.load (i));
        INFO ("preset " << factoryPresets()[(size_t) i].name);
        REQUIRE (p.getPresetName() == juce::String (factoryPresets()[(size_t) i].name));

        bool finite = true;
        float peak = 0.0f;
        for (int b = 0; b < 60; ++b)   // ~0.64 s
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int s = 0; s < 512; ++s)
                    buf.setSample (ch, s, 0.3f * (rng.nextFloat() * 2.0f - 1.0f));
            p.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int s = 0; s < 512; ++s)
                {
                    finite = finite && std::isfinite (buf.getSample (ch, s));
                    peak = std::max (peak, std::abs (buf.getSample (ch, s)));
                }
        }
        REQUIRE (finite);
        REQUIRE (peak <= 1.0f);   // safety limiter
        REQUIRE (peak > 1.0e-4f);
    }
}

TEST_CASE ("Factory preset content is applied (values, chain, modulation, gate)", "[presets]")
{
    AetherfallProcessor p;
    auto& pm = p.getPresetManager();
    pm.setUserFolder (tempPresetFolder());

    const auto& bank = factoryPresets();
    int pulse = -1, breath = -1;
    for (size_t i = 0; i < bank.size(); ++i)
    {
        if (juce::String (bank[i].name) == "Pulse Field") pulse = (int) i;
        if (juce::String (bank[i].name) == "Frozen Breath") breath = (int) i;
    }
    REQUIRE (pulse >= 0);
    REQUIRE (breath >= 0);

    REQUIRE (pm.load (pulse));
    REQUIRE (p.getChainOrder()[0] == ModuleType::Gate);
    REQUIRE (p.getChainOrder()[1] == ModuleType::ShimmerDelay);
    REQUIRE (p.getGatePattern() == 0x5555u);
    REQUIRE_THAT (value (p, PID::GateSwing), WithinAbs (0.3f, 1.0e-4f));

    REQUIRE (pm.load (breath));
    REQUIRE (p.getChainOrder()[0] == ModuleType::None);            // chain cleared
    REQUIRE_THAT (value (p, PID::GateSwing), WithinAbs (0.0f, 1.0e-6f));   // back to default
    const auto r = p.getModRouting();
    REQUIRE (r.slots[0].source == ModSource::Lfo1);
    REQUIRE (r.slots[0].dest == (std::int16_t) PID::CoreDampHF);
    REQUIRE_THAT (value (p, PID::Mod1Amount), WithinAbs (0.3f, 1.0e-4f));
    REQUIRE (r.macros[0][0].dest == (std::int16_t) PID::CoreSize);  // default macros restored

    // Loading a preset is one undo step.
    REQUIRE (p.undo());
    REQUIRE (p.getChainOrder()[0] == ModuleType::Gate);
}

TEST_CASE ("User presets: save, rescan, load, favourites, delete", "[presets]")
{
    const auto folder = tempPresetFolder();
    AetherfallProcessor p;
    auto& pm = p.getPresetManager();
    pm.setUserFolder (folder);

    auto* decay = p.getValueTreeState().getParameter ("core_decay");
    decay->setValueNotifyingHost (decay->convertTo0to1 (33.0f));
    auto order = chain::emptyOrder();
    order[0] = ModuleType::Comb;
    p.setChainOrder (order);

    REQUIRE (pm.saveUserPreset ("My Space", PresetCategory::Frozen));
    REQUIRE (folder.getChildFile ("My Space.afpreset").existsAsFile());

    // Change everything, then load it back.
    pm.load (0);
    REQUIRE (p.getChainOrder()[0] != ModuleType::Comb);

    const auto mine = pm.filter ("my space", -3);
    REQUIRE (mine.size() == 1);
    REQUIRE (pm.load (mine[0]));
    REQUIRE_THAT (value (p, PID::CoreDecay), WithinAbs (33.0f, 0.05f));
    REQUIRE (p.getChainOrder()[0] == ModuleType::Comb);
    REQUIRE (p.getPresetCategory() == PresetCategory::Frozen);

    // Favourites persist in the folder.
    const auto& entry = pm.getEntries()[(size_t) mine[0]];
    pm.toggleFavourite (entry);
    REQUIRE (pm.isFavourite (entry));
    PresetManager other (p);
    other.setUserFolder (folder);
    REQUIRE (other.filter ({}, -2).size() == 1);

    // Next / previous wrap around the list.
    const int before = pm.getCurrentIndex();
    REQUIRE (pm.loadNext());
    REQUIRE (pm.getCurrentIndex() == (before + 1) % (int) pm.getEntries().size());
    REQUIRE (pm.loadPrevious());
    REQUIRE (pm.getCurrentIndex() == before);

    REQUIRE (pm.deleteUserPreset (mine[0]));
    REQUIRE (pm.filter ("my space", -3).empty());
    folder.deleteRecursively();
}

TEST_CASE ("Preset name is saved with the project", "[presets]")
{
    juce::MemoryBlock state;
    {
        AetherfallProcessor p;
        p.getPresetManager().setUserFolder (tempPresetFolder());
        REQUIRE (p.getPresetManager().load (5));
        p.getStateInformation (state);
    }
    AetherfallProcessor q;
    q.setStateInformation (state.getData(), (int) state.getSize());
    REQUIRE (q.getPresetName() == juce::String (factoryPresets()[5].name));
}
