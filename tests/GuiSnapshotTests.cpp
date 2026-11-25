#include <catch2/catch_test_macros.hpp>

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets/PresetManager.h"

using namespace aetherfall;

// Renders every view of the editor to PNG files (headless). Hidden test:
//   AETHERFALL_SNAPSHOT_DIR=/some/dir AetherfallTests "[.gui]"
TEST_CASE ("Editor snapshots", "[.gui]")
{
    AetherfallProcessor p;
    p.setPlayConfigDetails (2, 2, 48000.0, 256);
    p.prepareToPlay (48000.0, 256);

    // Load a factory preset (sets chain, colours the accent by category).
    p.getPresetManager().setUserFolder (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("af_snap_presets"));
    for (size_t i = 0; i < factoryPresets().size(); ++i)
        if (juce::String (factoryPresets()[i].name) == "Bad Trip Intro")
            p.getPresetManager().load ((int) i);
    p.assignModulation (ModSource::Lfo1, PID::CoreSize, 0.4f);

    // Some audio so meters and the visualiser have something to show.
    juce::AudioBuffer<float> buf (2, 256);
    juce::MidiBuffer midi;
    juce::Random rng (3);
    for (int b = 0; b < 200; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 256; ++i)
                buf.setSample (ch, i, 0.3f * (rng.nextFloat() * 2.0f - 1.0f));
        p.processBlock (buf, midi);
    }

    const auto dir = juce::File (juce::SystemStats::getEnvironmentVariable ("AETHERFALL_SNAPSHOT_DIR",
                                                                         juce::File::getCurrentWorkingDirectory().getFullPathName()));
    dir.createDirectory();

    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto* editor = dynamic_cast<AetherfallEditor*> (ed.get());
    REQUIRE (editor != nullptr);

    const char* names[] = { "main", "chain", "core", "mod", "output" };
    for (float scale : { 1.0f, 1.5f })
    {
        editor->setSize (juce::roundToInt (AetherfallEditor::baseWidth * scale), juce::roundToInt (AetherfallEditor::baseHeight * scale));
        for (int t = 0; t < 5; ++t)
        {
            editor->showTab ((AetherfallEditor::Tab) t);
            const auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
            juce::PNGImageFormat png;
            const auto file = dir.getChildFile (juce::String ("gui_") + names[t] + (scale > 1.0f ? "_150" : "") + ".png");
            file.deleteFile();
            juce::FileOutputStream out (file);
            REQUIRE (out.openedOk());
            REQUIRE (png.writeImageToStream (img, out));
        }
    }

    editor->setSize (AetherfallEditor::baseWidth, AetherfallEditor::baseHeight);
    editor->showPresetBrowser (true);
    const auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    const auto file = dir.getChildFile ("gui_presets.png");
    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat png;
    REQUIRE (png.writeImageToStream (img, out));
}

TEST_CASE ("Editor can be opened, resized and closed repeatedly", "[gui]")
{
    AetherfallProcessor p;
    p.setPlayConfigDetails (2, 2, 48000.0, 256);
    p.prepareToPlay (48000.0, 256);

    for (int i = 0; i < 3; ++i)
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto* editor = dynamic_cast<AetherfallEditor*> (ed.get());
        REQUIRE (editor != nullptr);
        for (int t = 0; t < 5; ++t)
            editor->showTab ((AetherfallEditor::Tab) t);
        editor->setSize (1500, 960);
        editor->setSize (750, 480);
    }

    // The last scale used is remembered in the state.
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    REQUIRE (ed->getWidth() == 750);
}
