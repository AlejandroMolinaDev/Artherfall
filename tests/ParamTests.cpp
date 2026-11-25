#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <set>
#include <string>

#include "Parameters/ParamIDs.h"
#include "Parameters/ParamLayout.h"
#include "PluginProcessor.h"

using namespace aetherfall;

TEST_CASE ("Parameter table: enum order matches table", "[params]")
{
    for (int i = 0; i < numParams; ++i)
        REQUIRE ((int) paramInfos[(size_t) i].pid == i);
}

TEST_CASE ("Parameter table: unique ids, short names, sane defaults", "[params]")
{
    std::set<std::string> ids, names;

    for (const auto& p : paramInfos)
    {
        const std::string id (p.id), name (p.name);
        INFO ("param " << id);

        REQUIRE (! id.empty());
        REQUIRE (ids.insert (id).second);       // unique string id
        REQUIRE (names.insert (name).second);   // unique display name
        REQUIRE (name.size() <= 16);            // FL Studio friendly

        // ids: lowercase ascii, digits, underscore
        for (char c : id)
            REQUIRE (((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'));

        if (p.kind == Kind::Choice)
        {
            REQUIRE (p.numChoices() >= 2);
            REQUIRE (p.defaultValue >= 0.0f);
            REQUIRE ((int) p.defaultValue < p.numChoices());
        }
        else
        {
            REQUIRE (p.minValue < p.maxValue);
            REQUIRE (p.defaultValue >= p.minValue);
            REQUIRE (p.defaultValue <= p.maxValue);
        }

        if (p.kind == Kind::Float && (p.flags & ParamFlag::SymSkew) == 0 && p.skew != 0.0f)
        {
            REQUIRE (p.skew > p.minValue);   // centre must lie inside the range
            REQUIRE (p.skew < p.maxValue);
        }
    }
}

TEST_CASE ("Module table is consistent", "[params]")
{
    for (int t = 1; t <= numModuleTypes; ++t)
    {
        const auto& m = moduleInfo ((ModuleType) t);
        INFO ("module " << std::string (m.id));
        REQUIRE ((int) m.type == t);
        REQUIRE ((int) m.firstParam <= (int) m.lastParam);
        REQUIRE (info (m.on).kind == Kind::Bool);
        REQUIRE (info (m.mix).kind == Kind::Float);
        REQUIRE (! info (m.solo).isAutomatable());
        REQUIRE (! info (m.parallel).isAutomatable());
        // module params come right after the 4 common ones
        REQUIRE ((int) m.firstParam == (int) m.parallel + 1);
    }
}

TEST_CASE ("Repeated parameter blocks are addressable", "[params]")
{
    REQUIRE (lfoParam (7, 0) == PID::Lfo8Shape);
    REQUIRE (lfoParam (3, 1) == PID::Lfo4Rate);
    REQUIRE (envParam (3, 3) == PID::Env4Gain);
    REQUIRE (seqParam (1, 0) == PID::Seq2Div);
    REQUIRE (chaosParam (1, 2) == PID::Chaos2Smooth);
    REQUIRE (modAmountParam (31) == PID::Mod32Amount);
    REQUIRE (macroParam (3) == PID::MacroBloom);
}

TEST_CASE ("Value <-> text conversion round trips", "[params]")
{
    using Catch::Matchers::WithinAbs;
    using Catch::Matchers::WithinRel;

    REQUIRE (valueToText (info (PID::CoreDecay), 4.2f) == "4.20 s");
    REQUIRE (valueToText (info (PID::InHPF), 20.0f) == "Off");
    REQUIRE (valueToText (info (PID::OutLPF), 20000.0f) == "Off");
    REQUIRE (valueToText (info (PID::OutMix), 0.35f) == "35 %");
    REQUIRE (valueToText (info (PID::CoreColor), 4.0f) == "Void");
    REQUIRE (valueToText (info (PID::CoreDampHF), 7000.0f) == "7.00 kHz");

    REQUIRE_THAT (textToValue (info (PID::CoreDampHF), "7.5 kHz"), WithinRel (7500.0f, 1.0e-4f));
    REQUIRE_THAT (textToValue (info (PID::OutMix), "50 %"), WithinAbs (0.5f, 1.0e-6f));
    REQUIRE_THAT (textToValue (info (PID::CorePreDelay), "0.25 s"), WithinAbs (250.0f, 1.0e-3f));
    REQUIRE (textToValue (info (PID::InHPF), "off") == 20.0f);
    REQUIRE (textToValue (info (PID::CoreColor), "Dark") == 1.0f);
}

TEST_CASE ("APVTS exposes every parameter with the table defaults", "[params]")
{
    AetherfallProcessor p;
    auto& apvts = p.getValueTreeState();

    REQUIRE (p.getParameters().size() == numParams);

    for (const auto& info : paramInfos)
    {
        const juce::String id (std::string (info.id));
        INFO ("param " << id);
        auto* raw = apvts.getRawParameterValue (id);
        REQUIRE (raw != nullptr);
        REQUIRE_THAT (raw->load(), Catch::Matchers::WithinAbs (info.defaultValue, 1.0e-4)
                                 || Catch::Matchers::WithinRel (info.defaultValue, 1.0e-6f));
    }

    REQUIRE (p.getBypassParameter() != nullptr);
}
