#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "PluginInfo.h"
#include "GUI/Views/BasicViews.h"
#include "GUI/Views/ChainView.h"
#include "GUI/Views/ModView.h"

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

        auto* lf = dynamic_cast<AetherLookAndFeel*> (&getLookAndFeel());
        const auto accent = lf != nullptr ? lf->getAccent() : palette::accentDefault;
        auto logo = r.removeFromLeft (200).reduced (18, 0);
        juce::ColourGradient grad (palette::text, (float) logo.getX(), 0.0f, accent, (float) logo.getRight(), 0.0f, false);
        g.setGradientFill (grad);
        g.setFont (AetherLookAndFeel::font (21.0f, true));
        g.drawText ("AETHERFALL", logo, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (0, 12);
        r.removeFromLeft (200);
        for (auto& b : tabs)
            b.setBounds (r.removeFromLeft (84).reduced (3, 0));
        r.removeFromLeft (14);
        undoButton.setBounds (r.removeFromLeft (34).reduced (2, 0));
        redoButton.setBounds (r.removeFromLeft (34).reduced (2, 0));
        r.removeFromLeft (6);
        abButton.setBounds (r.removeFromLeft (44).reduced (2, 0));
        randomButton.setBounds (r.removeFromLeft (56).reduced (2, 0));
        bypass->setBounds (r.removeFromRight (104).reduced (6, 0));
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
    juce::TextButton undoButton, redoButton, abButton, randomButton;
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
}

AetherfallEditor::~AetherfallEditor()
{
    setLookAndFeel (nullptr);
}

void AetherfallEditor::showTab (Tab t)
{
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
