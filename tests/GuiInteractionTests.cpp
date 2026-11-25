#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <set>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "GUI/Components/ParamWidgets.h"
#include "GUI/Views/ChainView.h"
#include "GUI/Views/ModView.h"
#include "GUI/Views/PresetBrowser.h"
#include "Presets/PresetManager.h"

// =============================================================================
//  GUI interaction tests: the editor is driven like a user would (headless):
//  every widget of every view / module / modulation page must move its
//  parameter, host automation must move the widget back, and the custom
//  editors (chain slots, gate pattern, sequencer, LFO shape, matrix, macro
//  targets, source chips, header buttons, preset browser) must change the
//  plug-in state.
// =============================================================================

using namespace aetherfall;
using Catch::Matchers::WithinAbs;

namespace
{
    template <typename T>
    void collect (juce::Component& root, std::vector<T*>& out, bool visibleOnly = true)
    {
        for (auto* child : root.getChildren())
        {
            if (visibleOnly && ! child->isVisible())
                continue;
            if (auto* t = dynamic_cast<T*> (child))
                out.push_back (t);
            collect (*child, out, visibleOnly);
        }
    }

    template <typename T>
    std::vector<T*> all (juce::Component& root, bool visibleOnly = true)
    {
        std::vector<T*> v;
        collect (root, v, visibleOnly);
        return v;
    }

    juce::TextButton* buttonWithText (juce::Component& root, const juce::String& text)
    {
        for (auto* b : all<juce::TextButton> (root))
            if (b->getButtonText() == text)
                return b;
        return nullptr;
    }

    juce::MouseEvent mouseAt (juce::Component& c, juce::Point<float> pos, bool dragged = false)
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), pos, juce::ModifierKeys(),
                                 juce::MouseInputSource::defaultPressure, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &c, &c, now, pos, now, 1, dragged);
    }

    float paramValue (AetherfallProcessor& p, PID pid)
    {
        return p.getValueTreeState().getRawParameterValue (juce::String (std::string (info (pid).id)))->load();
    }

    juce::RangedAudioParameter* param (AetherfallProcessor& p, PID pid)
    {
        return p.getValueTreeState().getParameter (juce::String (std::string (info (pid).id)));
    }

    struct Fixture
    {
        Fixture()
        {
            proc.setPlayConfigDetails (2, 2, 48000.0, 256);
            proc.prepareToPlay (48000.0, 256);
            proc.getPresetManager().setUserFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                                     .getChildFile ("af_gui_tests"));
            editor.reset (dynamic_cast<AetherfallEditor*> (proc.createEditor()));
            REQUIRE (editor != nullptr);
            editor->setSize (AetherfallEditor::baseWidth, AetherfallEditor::baseHeight);
        }

        ~Fixture() { editor.reset(); }

        juce::Component& view() { return editor->getContent(); }

        AetherfallProcessor proc;
        std::unique_ptr<AetherfallEditor> editor;
    };

    /** Drives one widget both ways; returns a description of what failed (empty = ok). */
    std::string exercise (AetherfallProcessor& p, ParamWidget& w)
    {
        const auto pid = w.getPid();
        const auto& inf = info (pid);
        auto* prm = param (p, pid);
        const std::string id (inf.id);

        if (auto* k = dynamic_cast<Knob*> (&w))
        {
            auto& s = k->getSlider();
            s.setValue (s.getMaximum(), juce::sendNotificationSync);
            if (std::abs (paramValue (p, pid) - inf.rangeMax()) > 1.0e-3f * std::max (1.0f, std::abs (inf.rangeMax())))
                return id + ": knob max did not reach the parameter";
            s.setValue (s.getMinimum(), juce::sendNotificationSync);
            if (std::abs (paramValue (p, pid) - inf.rangeMin()) > 1.0e-3f * std::max (1.0f, std::abs (inf.rangeMin())))
                return id + ": knob min did not reach the parameter";
            // Host automation -> knob.
            prm->setValueNotifyingHost (prm->convertTo0to1 (inf.defaultValue));
            if (std::abs ((float) s.getValue() - inf.defaultValue) > 1.0e-3f * std::max (1.0f, std::abs (inf.defaultValue)))
                return id + ": automation did not move the knob";
            if (! s.isDoubleClickReturnEnabled() || std::abs ((float) s.getDoubleClickReturnValue() - inf.defaultValue) > 1.0e-4f)
                return id + ": double-click does not return to the default";
            return {};
        }
        if (auto* c = dynamic_cast<ChoiceBox*> (&w))
        {
            auto& box = c->getComboBox();
            const int n = box.getNumItems();
            if (n != inf.numChoices())
                return id + ": combo has " + std::to_string (n) + " items, parameter has " + std::to_string (inf.numChoices());
            box.setSelectedItemIndex (n - 1, juce::sendNotificationSync);
            if ((int) std::lround (paramValue (p, pid)) != n - 1)
                return id + ": combo did not reach the parameter";
            prm->setValueNotifyingHost (prm->convertTo0to1 (0.0f));
            if (box.getSelectedItemIndex() != 0)
                return id + ": automation did not move the combo";
            prm->setValueNotifyingHost (prm->convertTo0to1 (inf.defaultValue));
            return {};
        }
        if (auto* t = dynamic_cast<ToggleSwitch*> (&w))
        {
            auto& b = t->getButton();
            const bool before = b.getToggleState();
            b.setToggleState (! before, juce::sendNotificationSync);
            if ((paramValue (p, pid) >= 0.5f) == before)
                return id + ": toggle did not reach the parameter";
            prm->setValueNotifyingHost (before ? 1.0f : 0.0f);
            if (b.getToggleState() != before)
                return id + ": automation did not move the toggle";
            return {};
        }
        return id + ": unknown widget type";
    }
}

TEST_CASE ("GUI: every widget of every view moves its parameter and follows automation", "[gui]")
{
    Fixture f;
    std::set<int> covered;
    std::vector<std::string> failures;

    auto testVisibleWidgets = [&]
    {
        for (auto* w : all<ParamWidget> (f.view()))
        {
            covered.insert ((int) w->getPid());
            if (auto err = exercise (f.proc, *w); ! err.empty())
                failures.push_back (err);
        }
    };

    // MAIN, CORE, OUTPUT (and the header bypass, visible on every tab).
    for (auto tab : { AetherfallEditor::Tab::Main, AetherfallEditor::Tab::Core, AetherfallEditor::Tab::Output })
    {
        f.editor->showTab (tab);
        testVisibleWidgets();
    }

    // CHAIN: every module in slot 1, its panel and its tile.
    f.editor->showTab (AetherfallEditor::Tab::Chain);
    auto chainViews = all<ChainView> (f.view());
    REQUIRE (chainViews.size() == 1);
    auto* chain = chainViews[0];
    for (int t = 1; t <= numModuleTypes; ++t)
    {
        chain->setSlotModule (0, (ModuleType) t);
        chain->selectSlot (0);
        REQUIRE (f.proc.getChainOrder()[0] == (ModuleType) t);
        testVisibleWidgets();
    }

    // MOD: every page and every instance.
    f.editor->showTab (AetherfallEditor::Tab::Mod);
    const std::pair<const char*, int> pages[] = { { "LFO", numLfos }, { "ENV", numEnvFollowers }, { "SEQ", numStepSeqs },
                                                   { "CHAOS", numChaos }, { "MACRO", numMacros } };
    for (const auto& [page, count] : pages)
    {
        auto* pageButton = buttonWithText (f.view(), page);
        REQUIRE (pageButton != nullptr);
        pageButton->onClick();
        for (int i = 0; i < count; ++i)
        {
            auto* inst = buttonWithText (f.view(), juce::String (i + 1));
            REQUIRE (inst != nullptr);
            inst->onClick();
            testVisibleWidgets();
        }
    }

    for (const auto& fl : failures)
        UNSCOPED_INFO (fl);
    CHECK (failures.empty());

    // Every parameter is controllable from the UI (mod amounts live in the matrix rows).
    std::vector<std::string> missing;
    for (const auto& inf : paramInfos)
        if (covered.count ((int) inf.pid) == 0 && ! ((int) inf.pid >= (int) PID::Mod1Amount && (int) inf.pid <= (int) PID::Mod32Amount))
            missing.push_back (std::string (inf.id));
    for (const auto& m : missing)
        UNSCOPED_INFO ("no widget for " << m);
    CHECK (missing.empty());
    INFO ("widgets exercised for " << covered.size() << " parameters");
    CHECK (covered.size() >= (size_t) numParams - numModSlots);
}

TEST_CASE ("GUI: chain slots - pick, move, re-bind and gate pattern editor", "[gui]")
{
    Fixture f;
    f.editor->showTab (AetherfallEditor::Tab::Chain);
    auto* chain = all<ChainView> (f.view())[0];
    auto tiles = all<SlotTile> (f.view());
    REQUIRE (tiles.size() == (size_t) numChainSlots);

    // Picking through the slot combo box.
    tiles[0]->typeBox.setSelectedId ((int) ModuleType::Granular + 1, juce::sendNotificationSync);
    tiles = all<SlotTile> (f.view());
    tiles[1]->typeBox.setSelectedId ((int) ModuleType::Filter + 1, juce::sendNotificationSync);
    REQUIRE (f.proc.getChainOrder()[0] == ModuleType::Granular);
    REQUIRE (f.proc.getChainOrder()[1] == ModuleType::Filter);

    // Picking a module that is already used moves it.
    tiles = all<SlotTile> (f.view());
    tiles[4]->typeBox.setSelectedId ((int) ModuleType::Granular + 1, juce::sendNotificationSync);
    REQUIRE (f.proc.getChainOrder()[0] == ModuleType::None);
    REQUIRE (f.proc.getChainOrder()[4] == ModuleType::Granular);

    // Drag slot 5 onto slot 1 (drop handler).
    tiles = all<SlotTile> (f.view());
    juce::DragAndDropTarget::SourceDetails drag ("slot:4", tiles[4], {});
    REQUIRE (tiles[0]->isInterestedInDragSource (drag));
    tiles[0]->itemDropped (drag);
    REQUIRE (f.proc.getChainOrder()[0] == ModuleType::Granular);
    REQUIRE (f.proc.getChainOrder()[2] == ModuleType::Filter);

    // After the move, the controls of each tile drive the module now in it.
    tiles = all<SlotTile> (f.view());
    for (size_t s = 0; s < tiles.size(); ++s)
    {
        const auto type = f.proc.getChainOrder()[s];
        REQUIRE (tiles[s]->getModule() == type);
        for (auto* w : all<ParamWidget> (*tiles[s]))
        {
            const auto& mi = moduleInfo (type);
            const auto pid = w->getPid();
            CHECK ((pid == mi.on || pid == mi.mix || pid == mi.solo || pid == mi.parallel));
            CHECK (exercise (f.proc, *w).empty());
        }
    }

    // Trance Gate pattern editor: a click on step 3 toggles bit 2.
    chain->setSlotModule (3, ModuleType::Gate);
    chain->selectSlot (3);
    auto editors = all<GatePatternEditor> (f.view());
    REQUIRE (editors.size() == 1);
    auto* ge = editors[0];
    const auto before = f.proc.getGatePattern();
    const float stepW = (float) ge->getWidth() / 16.0f;
    ge->mouseDown (mouseAt (*ge, { stepW * 2.5f, (float) ge->getHeight() * 0.5f }));
    CHECK (f.proc.getGatePattern() == (before ^ (1u << 2)));
    CHECK (f.proc.canUndo());
}

TEST_CASE ("GUI: modulation view - matrix rows, macro targets, sequencer and LFO editors, source chips", "[gui]")
{
    Fixture f;
    f.editor->showTab (AetherfallEditor::Tab::Mod);
    auto mods = all<ModView> (f.view());
    REQUIRE (mods.size() == 1);

    // --- Matrix row 3: source LFO 2 -> Filter cutoff, curve S, via Macro 1, amount, clear.
    auto rows = all<MatrixRow> (f.view(), false);
    REQUIRE (rows.size() == (size_t) numModSlots);
    auto* row = rows[2];
    auto combos = all<juce::ComboBox> (*row, false);
    auto sliders = all<juce::Slider> (*row, false);
    REQUIRE (combos.size() == 4);
    REQUIRE (sliders.size() == 1);
    combos[0]->setSelectedId ((int) ModSource::Lfo2 + 1, juce::sendNotificationSync);
    combos[1]->setSelectedId ((int) PID::FiltCutoff + 2, juce::sendNotificationSync);
    combos[2]->setSelectedId ((int) ModCurve::SCurve + 1, juce::sendNotificationSync);
    combos[3]->setSelectedId ((int) ModSource::Macro1 + 1, juce::sendNotificationSync);
    sliders[0]->setValue (0.4, juce::sendNotificationSync);
    auto r = f.proc.getModRouting();
    CHECK (r.slots[2].source == ModSource::Lfo2);
    CHECK (r.slots[2].dest == (std::int16_t) PID::FiltCutoff);
    CHECK (r.slots[2].curve == ModCurve::SCurve);
    CHECK (r.slots[2].via == ModSource::Macro1);
    CHECK_THAT (paramValue (f.proc, PID::Mod3Amount), WithinAbs (0.4f, 1.0e-3f));

    auto* clear = all<juce::TextButton> (*row, false)[0];
    clear->onClick();
    CHECK (f.proc.getModRouting().slots[2].source == ModSource::None);

    // --- Source chip dropped on a knob (CORE view) assigns a free slot.
    f.editor->showTab (AetherfallEditor::Tab::Core);
    Knob* sizeKnob = nullptr;
    for (auto* k : all<Knob> (f.view()))
        if (k->getPid() == PID::CoreSize) sizeKnob = k;
    REQUIRE (sizeKnob != nullptr);
    juce::DragAndDropTarget::SourceDetails chipDrag ("modsrc:chaos1", nullptr, {});
    REQUIRE (sizeKnob->isInterestedInDragSource (chipDrag));
    sizeKnob->itemDropped (chipDrag);
    r = f.proc.getModRouting();
    bool found = false;
    for (const auto& s : r.slots)
        found = found || (s.source == ModSource::Chaos1 && s.dest == (std::int16_t) PID::CoreSize);
    CHECK (found);

    // Choice parameters refuse modulation drops.
    for (auto* w : all<ParamWidget> (f.view()))
        if (auto* k = dynamic_cast<Knob*> (w))
            CHECK (k->isInterestedInDragSource (chipDrag) == info (k->getPid()).isModulatable());

    // Dragging a source over a header tab opens that view.
    for (auto* tb : all<juce::TextButton> (f.view()))
        if (tb->getButtonText() == "MOD")
            if (auto* target = dynamic_cast<juce::DragAndDropTarget*> (tb))
            {
                target->itemDragEnter (chipDrag);
                CHECK (f.editor->getCurrentTab() == AetherfallEditor::Tab::Mod);
            }

    // --- Macro targets: HAZE target 8 -> Tape Age, amount -0.5 typed in (no drag).
    buttonWithText (f.view(), "MACRO")->onClick();
    buttonWithText (f.view(), "1")->onClick();
    std::vector<juce::Component*> macroRows;
    for (auto* c : all<juce::Component> (f.view()))
        if (all<juce::ComboBox> (*c, false).size() == 1 && all<juce::Slider> (*c, false).size() == 1
            && dynamic_cast<ParamWidget*> (c) == nullptr && c->getChildren().size() == 2)
            macroRows.push_back (c);
    REQUIRE (macroRows.size() == (size_t) macroTargetsPerMacro);
    auto* mrow = macroRows[7];
    all<juce::ComboBox> (*mrow, false)[0]->setSelectedId ((int) PID::TapeAge + 2, juce::sendNotificationSync);
    all<juce::Slider> (*mrow, false)[0]->setValue (-0.5, juce::sendNotificationSync);
    r = f.proc.getModRouting();
    CHECK (r.macros[0][7].dest == (std::int16_t) PID::TapeAge);
    CHECK_THAT (r.macros[0][7].amount, WithinAbs (-0.5f, 1.0e-3f));

    // --- Step sequencer editor: drawing sets the step value.
    buttonWithText (f.view(), "SEQ")->onClick();
    buttonWithText (f.view(), "1")->onClick();
    juce::Component* seqView = nullptr;
    for (auto* c : all<juce::Component> (f.view()))
        if (juce::String (typeid (*c).name()).contains ("SeqStepsView")) seqView = c;
    REQUIRE (seqView != nullptr);
    const float cellW = (float) (seqView->getWidth() - 2) / (float) seqMaxSteps;
    const juce::Point<float> pt (1.0f + cellW * 5.5f, 1.0f + (float) (seqView->getHeight() - 2) * 0.25f);   // 75 %
    seqView->mouseDown (mouseAt (*seqView, pt));
    seqView->mouseUp (mouseAt (*seqView, pt));
    CHECK_THAT (f.proc.getModRouting().seqSteps[0][5], WithinAbs (0.75f, 0.03f));

    // --- LFO custom shape editor.
    buttonWithText (f.view(), "LFO")->onClick();
    buttonWithText (f.view(), "2")->onClick();
    juce::Component* lfoView = nullptr;
    for (auto* c : all<juce::Component> (f.view()))
        if (juce::String (typeid (*c).name()).contains ("LfoShapeView")) lfoView = c;
    REQUIRE (lfoView != nullptr);
    const float pw = (float) (lfoView->getWidth() - 2) / (float) lfoCustomPoints;
    const juce::Point<float> lp (1.0f + pw * 3.5f, 1.0f + (float) (lfoView->getHeight() - 2) * 0.5f);   // centre = 0
    lfoView->mouseDown (mouseAt (*lfoView, lp));
    lfoView->mouseUp (mouseAt (*lfoView, lp));
    CHECK_THAT (f.proc.getModRouting().lfoCustom[1][3], WithinAbs (0.0f, 0.05f));
}

TEST_CASE ("GUI: header buttons - undo, redo, A/B, presets, bypass, tabs", "[gui]")
{
    Fixture f;
    auto& root = f.view();

    // Tabs.
    for (const auto& [name, tab] : { std::pair<const char*, AetherfallEditor::Tab> { "CHAIN", AetherfallEditor::Tab::Chain },
                                     { "CORE", AetherfallEditor::Tab::Core }, { "OUTPUT", AetherfallEditor::Tab::Output },
                                     { "MOD", AetherfallEditor::Tab::Mod }, { "MAIN", AetherfallEditor::Tab::Main } })
    {
        auto* b = buttonWithText (root, name);
        REQUIRE (b != nullptr);
        b->onClick();
        CHECK (f.editor->getCurrentTab() == tab);
    }

    // A knob gesture, then undo / redo from the header.
    Knob* mix = nullptr;
    for (auto* k : all<Knob> (root))
        if (k->getPid() == PID::OutMix) mix = k;
    REQUIRE (mix != nullptr);
    const float before = paramValue (f.proc, PID::OutMix);
    // A real mouse drag (the drag creates the undo point, like in the plug-in).
    auto& sl = mix->getSlider();
    const juce::Point<float> c0 ((float) sl.getWidth() * 0.5f, (float) sl.getHeight() * 0.5f);
    sl.mouseDown (mouseAt (sl, c0));
    sl.mouseDrag (mouseAt (sl, c0.translated (0.0f, -60.0f), true));
    sl.setValue (0.9, juce::sendNotificationSync);
    sl.mouseUp (mouseAt (sl, c0.translated (0.0f, -60.0f), true));
    CHECK_THAT (paramValue (f.proc, PID::OutMix), WithinAbs (0.9f, 1.0e-3f));

    auto* undo = buttonWithText (root, juce::CharPointer_UTF8 ("\xe2\x86\xb6"));
    auto* redo = buttonWithText (root, juce::CharPointer_UTF8 ("\xe2\x86\xb7"));
    REQUIRE (undo != nullptr);
    REQUIRE (redo != nullptr);
    undo->onClick();
    CHECK_THAT (paramValue (f.proc, PID::OutMix), WithinAbs (before, 1.0e-3f));
    CHECK_THAT ((float) mix->getSlider().getValue(), WithinAbs (before, 1.0e-3f));   // the knob follows
    redo->onClick();
    CHECK_THAT (paramValue (f.proc, PID::OutMix), WithinAbs (0.9f, 1.0e-3f));

    // A/B.
    auto* ab = buttonWithText (root, "A");
    REQUIRE (ab != nullptr);
    ab->onClick();
    CHECK (f.proc.getABIndex() == 1);
    sl.mouseDown (mouseAt (sl, c0));
    sl.setValue (0.1, juce::sendNotificationSync);
    sl.mouseUp (mouseAt (sl, c0));
    ab->onClick();
    CHECK (f.proc.getABIndex() == 0);
    CHECK_THAT (paramValue (f.proc, PID::OutMix), WithinAbs (0.9f, 1.0e-3f));

    // Preset arrows.
    auto* next = buttonWithText (root, juce::CharPointer_UTF8 ("\xe2\x96\xb6"));
    auto* prev = buttonWithText (root, juce::CharPointer_UTF8 ("\xe2\x97\x80"));
    REQUIRE (next != nullptr);
    REQUIRE (prev != nullptr);
    next->onClick();
    const auto first = f.proc.getPresetName();
    next->onClick();
    CHECK (f.proc.getPresetName() != first);
    prev->onClick();
    CHECK (f.proc.getPresetName() == first);

    // Accent colour follows the preset category (editor timer).
    juce::Timer::callPendingTimersSynchronously();
    if (auto* lf = dynamic_cast<AetherLookAndFeel*> (&f.editor->getLookAndFeel()))
        CHECK (lf->getAccent() == categoryAccent (f.proc.getPresetCategory()));

    // Bypass switch.
    for (auto* t : all<ToggleSwitch> (root))
        if (t->getPid() == PID::Bypass)
        {
            t->getButton().setToggleState (true, juce::sendNotificationSync);
            CHECK (paramValue (f.proc, PID::Bypass) >= 0.5f);
            t->getButton().setToggleState (false, juce::sendNotificationSync);
        }
}

TEST_CASE ("GUI: preset browser - categories, search, load, favourites", "[gui]")
{
    Fixture f;
    f.editor->showPresetBrowser (true);
    auto browsers = all<PresetBrowser> (f.view());
    REQUIRE (browsers.size() == 1);
    auto* browser = browsers[0];
    auto* list = all<juce::ListBox> (*browser)[0];
    auto* model = list->getListBoxModel();
    REQUIRE (model != nullptr);
    CHECK (model->getNumRows() >= 63);

    // Category filter.
    buttonWithText (*browser, "Frozen")->onClick();
    CHECK (model->getNumRows() == 7);

    // Search.
    buttonWithText (*browser, "All")->onClick();
    auto* search = all<juce::TextEditor> (*browser)[0];
    search->setText ("glass", juce::dontSendNotification);
    search->onTextChange();   // TextEditor notifies asynchronously; no message loop in tests
    REQUIRE (model->getNumRows() == 1);

    // Click loads it.
    model->listBoxItemClicked (0, mouseAt (*list, { 30.0f, 10.0f }));
    CHECK (f.proc.getPresetName() == "Glass Particles");
    CHECK (f.proc.getChainOrder()[0] == ModuleType::Granular);

    // Clicking the star toggles the favourite; the favourites filter shows it.
    model->listBoxItemClicked (0, mouseAt (*list, { (float) list->getWidth() - 20.0f, 10.0f }));
    search->setText ({}, juce::dontSendNotification);
    search->onTextChange();
    buttonWithText (*browser, juce::CharPointer_UTF8 ("\xe2\x98\x85 Favourites"))->onClick();
    CHECK (model->getNumRows() == 1);
    model->listBoxItemClicked (0, mouseAt (*list, { (float) list->getWidth() - 20.0f, 10.0f }));   // unfavourite

    // Close.
    buttonWithText (*browser, "Close")->onClick();
    juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("af_gui_tests").deleteRecursively();
}

TEST_CASE ("GUI: views keep running with audio (timers, meters, visualiser)", "[gui]")
{
    Fixture f;
    juce::AudioBuffer<float> buf (2, 256);
    juce::MidiBuffer midi;
    juce::Random rng (2);
    for (int t = 0; t < 5; ++t)
    {
        f.editor->showTab ((AetherfallEditor::Tab) t);
        for (int b = 0; b < 40; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 256; ++i)
                    buf.setSample (ch, i, 0.3f * (rng.nextFloat() * 2.0f - 1.0f));
            f.proc.processBlock (buf, midi);
            juce::Timer::callPendingTimersSynchronously();
        }
        // Paint every view at two scales.
        for (float scale : { 0.75f, 2.0f })
        {
            f.editor->setSize (juce::roundToInt (AetherfallEditor::baseWidth * scale), juce::roundToInt (AetherfallEditor::baseHeight * scale));
            const auto img = f.editor->createComponentSnapshot (f.editor->getLocalBounds(), true, 1.0f);
            CHECK (img.isValid());
        }
    }
}

TEST_CASE ("GUI: keyboard shortcuts for undo / redo", "[gui]")
{
    Fixture f;
    auto* prm = param (f.proc, PID::CoreDecay);
    prm->beginChangeGesture();
    prm->setValueNotifyingHost (0.9f);
    prm->endChangeGesture();
    const float edited = paramValue (f.proc, PID::CoreDecay);

    CHECK (f.editor->keyPressed (juce::KeyPress ('Z', juce::ModifierKeys::commandModifier, 0)));
    CHECK (paramValue (f.proc, PID::CoreDecay) != edited);
    CHECK (f.editor->keyPressed (juce::KeyPress ('Z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)));
    CHECK (paramValue (f.proc, PID::CoreDecay) == edited);
}

// Renders the CHAIN panel of every module (hidden; for visual review).
TEST_CASE ("GUI: module panel snapshots", "[.gui]")
{
    Fixture f;
    const auto dir = juce::File (juce::SystemStats::getEnvironmentVariable ("AETHERFALL_SNAPSHOT_DIR",
                                                                         juce::File::getCurrentWorkingDirectory().getFullPathName()));
    dir.createDirectory();
    f.editor->showTab (AetherfallEditor::Tab::Chain);
    auto* chain = all<ChainView> (f.view())[0];
    for (int t = 1; t <= numModuleTypes; ++t)
    {
        chain->setSlotModule (0, (ModuleType) t);
        chain->selectSlot (0);
        const auto img = f.editor->createComponentSnapshot (f.editor->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        auto file = dir.getChildFile (juce::String::formatted ("module_%02d.png", t));
        file.deleteFile();
        juce::FileOutputStream out (file);
        REQUIRE (png.writeImageToStream (img, out));
    }
}
