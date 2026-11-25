#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "PluginInfo.h"
#include "GUI/Views/BasicViews.h"
#include "GUI/Views/ChainView.h"
#include "GUI/Views/ModView.h"
#include "GUI/Views/PresetBrowser.h"
#include "Presets/PresetManager.h"

namespace aetherfall
{

namespace
{
    const juce::Identifier uiNode { "UI" }, scaleProp { "scale" }, tabProp { "tab" };
}

//==============================================================================
/** Tab button that also opens its view while a modulation source is dragged
    over it (so a source can be dropped on a knob of any view). */
class TabButton final : public juce::TextButton, public juce::DragAndDropTarget
{
public:
    std::function<void()> onDragHover;
    bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.toString().startsWith ("modsrc:"); }
    void itemDragEnter (const SourceDetails&) override { if (onDragHover) onDragHover(); }
    void itemDropped (const SourceDetails&) override {}
};

/** Top bar: logo, view tabs, undo/redo, A/B, randomize, bypass. */
class AetherfallEditor::Header final : public juce::Component, private juce::Timer
{
public:
    Header (AetherfallEditor& e, juce::AudioProcessorValueTreeState& apvts) : editor (e)
    {
        static const char* names[] = { "MAIN", "CHAIN", "CORE", "MOD", "OUTPUT" };
        for (size_t i = 0; i < tabs.size(); ++i)
        {
            auto& b = tabs[i];
            b.setButtonText (names[i]);
            b.onClick = [this, i] { editor.showTab ((Tab) i); };
            b.onDragHover = [this, i] { editor.showTab ((Tab) i); };
            addAndMakeVisible (b);
        }

        undoButton.setButtonText (juce::CharPointer_UTF8 ("\xe2\x86\xb6"));
        redoButton.setButtonText (juce::CharPointer_UTF8 ("\xe2\x86\xb7"));
        undoButton.setTooltip ("Undo (Ctrl/Cmd+Z)");
        redoButton.setTooltip ("Redo (Ctrl/Cmd+Shift+Z)");
        undoButton.onClick = [this] { editor.getProcessor().undo(); };
        redoButton.onClick = [this] { editor.getProcessor().redo(); };

        abButton.setTooltip ("A/B: switch between two complete settings (right-click: copy to the other)");
        abButton.onClick = [this]
        {
            if (juce::ModifierKeys::currentModifiers.isPopupMenu())
                editor.getProcessor().copyCurrentToOtherAB();
            else
                editor.getProcessor().toggleAB();
            update();
        };

        randomButton.setButtonText ("RAND");
        randomButton.setTooltip ("Smart randomize a section");
        randomButton.onClick = [this] { showRandomMenu(); };

        for (auto* b : { &undoButton, &redoButton, &abButton, &randomButton })
            addAndMakeVisible (*b);

        prevButton.setButtonText (juce::CharPointer_UTF8 ("\xe2\x97\x80"));
        nextButton.setButtonText (juce::CharPointer_UTF8 ("\xe2\x96\xb6"));
        prevButton.setTooltip ("Previous preset");
        nextButton.setTooltip ("Next preset");
        presetButton.setTooltip ("Browse presets");
        prevButton.onClick = [this] { editor.getProcessor().getPresetManager().loadPrevious(); update(); };
        nextButton.onClick = [this] { editor.getProcessor().getPresetManager().loadNext(); update(); };
        presetButton.onClick = [this] { editor.showPresetBrowser (! editor.isPresetBrowserVisible()); };
        for (auto* b : { &prevButton, &nextButton, &presetButton })
            addAndMakeVisible (*b);

        bypass = std::make_unique<ToggleSwitch> (apvts, PID::Bypass, "Bypass");
        addAndMakeVisible (*bypass);
        update();
        startTimerHz (5);
    }

    void setActive (Tab t)
    {
        for (size_t i = 0; i < tabs.size(); ++i)
            tabs[i].setToggleState ((int) i == (int) t, juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        g.setColour (palette::panel);
        g.fillRect (r);
        g.setColour (palette::outline);
        g.drawHorizontalLine (r.getBottom() - 1, 0.0f, (float) r.getWidth());
        g.drawHorizontalLine (48, 0.0f, (float) r.getWidth());

        auto* lf = dynamic_cast<AetherLookAndFeel*> (&getLookAndFeel());
        const auto accent = lf != nullptr ? lf->getAccent() : palette::accentDefault;
        auto logo = r.removeFromTop (48).removeFromLeft (220).reduced (18, 0);
        juce::ColourGradient grad (palette::text, (float) logo.getX(), 0.0f, accent, (float) logo.getRight(), 0.0f, false);
        g.setGradientFill (grad);
        g.setFont (AetherLookAndFeel::font (21.0f, true));
        g.drawText ("AETHERFALL", logo, juce::Justification::centredLeft);
    }

    void resized() override
    {
        // Row 1: logo | preset bar | undo, redo, A/B, RAND, bypass.
        auto row1 = getLocalBounds().removeFromTop (48).reduced (0, 9);
        row1.removeFromLeft (230);
        bypass->setBounds (row1.removeFromRight (108).reduced (6, 0));
        randomButton.setBounds (row1.removeFromRight (58).reduced (2, 0));
        abButton.setBounds (row1.removeFromRight (40).reduced (2, 0));
        redoButton.setBounds (row1.removeFromRight (34).reduced (2, 0));
        undoButton.setBounds (row1.removeFromRight (34).reduced (2, 0));
        row1.removeFromRight (16);
        prevButton.setBounds (row1.removeFromLeft (30).reduced (2, 0));
        nextButton.setBounds (row1.removeFromRight (30).reduced (2, 0));
        presetButton.setBounds (row1.reduced (2, 0));

        // Row 2: view tabs, centred.
        auto row2 = getLocalBounds().withTrimmedTop (48).reduced (0, 5);
        const int tabW = 116;
        row2 = row2.withSizeKeepingCentre (tabW * (int) tabs.size(), row2.getHeight());
        for (auto& b : tabs)
            b.setBounds (row2.removeFromLeft (tabW).reduced (3, 0));
    }

private:
    void timerCallback() override { update(); }

    void update()
    {
        auto& p = editor.getProcessor();
        undoButton.setEnabled (p.canUndo());
        redoButton.setEnabled (p.canRedo());
        abButton.setButtonText (p.getABIndex() == 0 ? "A" : "B");
        abButton.setToggleState (p.getABIndex() == 1, juce::dontSendNotification);
        presetButton.setButtonText (p.getPresetName() + "   |   " + categoryName (p.getPresetCategory()));
    }

    void showRandomMenu()
    {
        using RS = AetherfallProcessor::RandomSection;
        juce::PopupMenu menu;
        const std::pair<const char*, RS> sections[] = { { "Core", RS::Core }, { "Chain", RS::Chain },
                                                        { "Modulation", RS::Modulation }, { "Output", RS::Output } };
        const std::pair<const char*, float> amounts[] = { { "Subtle", 0.2f }, { "Medium", 0.5f }, { "Wild", 1.0f } };
        int id = 1;
        for (const auto& [name, sec] : sections)
        {
            juce::PopupMenu sub;
            for (const auto& [label, amt] : amounts)
                sub.addItem (id++, label);
            menu.addSubMenu (name, sub);
        }

        juce::Component::SafePointer<Header> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&randomButton), [safe] (int result)
        {
            if (safe == nullptr || result <= 0)
                return;
            const RS secs[] = { RS::Core, RS::Chain, RS::Modulation, RS::Output };
            const float amts[] = { 0.2f, 0.5f, 1.0f };
            const int s = (result - 1) / 3, a = (result - 1) % 3;
            safe->editor.getProcessor().randomize (secs[s], amts[a], (std::uint32_t) juce::Time::getMillisecondCounter());
        });
    }

    AetherfallEditor& editor;
    std::array<TabButton, 5> tabs;
    juce::TextButton undoButton, redoButton, abButton, randomButton, prevButton, nextButton, presetButton;
    std::unique_ptr<ToggleSwitch> bypass;
};

//==============================================================================
AetherfallEditor::AetherfallEditor (AetherfallProcessor& p)
    : AudioProcessorEditor (p), processor (p)
{
    setLookAndFeel (&lookAndFeel);
    auto& apvts = processor.getValueTreeState();

    header = std::make_unique<Header> (*this, apvts);
    content.addAndMakeVisible (*header);

    views[(size_t) Tab::Main]   = std::make_unique<MainView> (processor, apvts);
    views[(size_t) Tab::Chain]  = std::make_unique<ChainView> (processor, apvts);
    views[(size_t) Tab::Core]   = std::make_unique<CoreView> (apvts);
    views[(size_t) Tab::Mod]    = std::make_unique<ModView> (processor, apvts);
    views[(size_t) Tab::Output] = std::make_unique<OutputView> (processor, apvts);
    for (auto& v : views)
        content.addChildComponent (*v);

    addAndMakeVisible (content);
    content.setBounds (0, 0, baseWidth, baseHeight);
    header->setBounds (0, 0, baseWidth, headerHeight);
    for (auto& v : views)
        v->setBounds (0, headerHeight, baseWidth, baseHeight - headerHeight);

    const auto ui = processor.getValueTreeState().state.getChildWithName (uiNode);
    showTab ((Tab) juce::jlimit (0, 4, (int) ui.getProperty (tabProp, 0)));

    setWantsKeyboardFocus (true);
    setResizable (true, true);
    setResizeLimits (baseWidth * 3 / 4, baseHeight * 3 / 4, baseWidth * 2, baseHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);

    const float scale = loadScale();
    setSize (juce::roundToInt (baseWidth * scale), juce::roundToInt (baseHeight * scale));

    timerCallback();
    startTimerHz (4);
}

void AetherfallEditor::timerCallback()
{
    // Accent colour follows the category of the loaded preset.
    const int cat = (int) processor.getPresetCategory();
    if (cat != shownCategory)
    {
        shownCategory = cat;
        lookAndFeel.setAccent (cat == (int) PresetCategory::User ? palette::accentDefault
                                                                  : categoryAccent ((PresetCategory) cat));
        content.repaint();
    }
}

void AetherfallEditor::showPresetBrowser (bool show)
{
    if (! show)
    {
        presetBrowser.reset();
        return;
    }
    auto browser = std::make_unique<PresetBrowser> (processor);
    browser->onClose = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<AetherfallEditor> (this)]
                                                                 { if (safe != nullptr) safe->showPresetBrowser (false); }); };
    content.addAndMakeVisible (*browser);
    browser->setBounds (0, headerHeight, baseWidth, baseHeight - headerHeight);
    presetBrowser = std::move (browser);
}

AetherfallEditor::~AetherfallEditor()
{
    stopTimer();
    presetBrowser.reset();
    setLookAndFeel (nullptr);
}

void AetherfallEditor::showTab (Tab t)
{
    if (presetBrowser != nullptr)
        showPresetBrowser (false);
    currentTab = t;
    for (size_t i = 0; i < views.size(); ++i)
        views[i]->setVisible ((int) i == (int) t);
    header->setActive (t);

    auto ui = processor.getValueTreeState().state.getOrCreateChildWithName (uiNode, nullptr);
    ui.setProperty (tabProp, (int) t, nullptr);
}

float AetherfallEditor::loadScale() const
{
    const auto ui = processor.getValueTreeState().state.getChildWithName (uiNode);
    return juce::jlimit (0.75f, 2.0f, (float) ui.getProperty (scaleProp, 1.0f));
}

void AetherfallEditor::saveScale (float scale)
{
    auto ui = processor.getValueTreeState().state.getOrCreateChildWithName (uiNode, nullptr);
    ui.setProperty (scaleProp, scale, nullptr);
}

bool AetherfallEditor::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    if (mods.isCommandDown() && key.getKeyCode() == 'Z')
        return mods.isShiftDown() ? processor.redo() || true : processor.undo() || true;
    if (mods.isCommandDown() && key.getKeyCode() == 'Y')
        return processor.redo() || true;
    return false;
}

void AetherfallEditor::paint (juce::Graphics& g)
{
    g.fillAll (palette::background);
}

void AetherfallEditor::resized()
{
    const float scale = juce::jlimit (0.5f, 2.5f, (float) getWidth() / (float) baseWidth);
    content.setTransform (juce::AffineTransform::scale (scale));
    saveScale (scale);
}

} // namespace aetherfall
