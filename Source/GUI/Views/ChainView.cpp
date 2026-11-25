#include "ChainView.h"
#include "PluginProcessor.h"
#include "GUI/LookAndFeel/AetherLookAndFeel.h"

namespace aetherfall
{

//==============================================================================
SlotTile::SlotTile (ChainView& o, int i) : owner (o), index (i)
{
    typeBox.addItem ("Empty", 1);
    for (int t = 1; t <= numModuleTypes; ++t)
        typeBox.addItem (juce::String (std::string (moduleInfo ((ModuleType) t).name)), t + 1);
    typeBox.setTooltip ("Module in slot " + juce::String (index + 1) + " (drag the slot header to reorder)");
    typeBox.onChange = [this]
    {
        const auto t = (ModuleType) std::max (0, typeBox.getSelectedId() - 1);
        if (t != module)
            owner.setSlotModule (index, t);
    };
    addAndMakeVisible (typeBox);
}

void SlotTile::setModule (juce::AudioProcessorValueTreeState& apvts, ModuleType t)
{
    module = t;
    typeBox.setSelectedId ((int) t + 1, juce::dontSendNotification);
    onSwitch.reset(); mixKnob.reset(); soloSwitch.reset(); parSwitch.reset();

    if (t != ModuleType::None)
    {
        const auto& mi = moduleInfo (t);
        onSwitch   = createParamWidget (apvts, mi.on, Knob::Size::Small, "On");
        mixKnob    = createParamWidget (apvts, mi.mix, Knob::Size::Small, "Mix");
        soloSwitch = createParamWidget (apvts, mi.solo, Knob::Size::Small, "S");
        parSwitch  = createParamWidget (apvts, mi.parallel, Knob::Size::Small, "P");
        for (auto* w : { onSwitch.get(), mixKnob.get(), soloSwitch.get(), parSwitch.get() })
            addAndMakeVisible (*w);
        if (auto* ts = dynamic_cast<ToggleSwitch*> (soloSwitch.get()))
            ts->getButton().setTooltip ("Solo: only soloed modules are heard");
        if (auto* tp = dynamic_cast<ToggleSwitch*> (parSwitch.get()))
            tp->getButton().setTooltip ("Parallel: runs alongside the previous slot");
    }
    resized();
    repaint();
}

void SlotTile::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (module == ModuleType::None ? palette::panel : palette::panelRaised);
    g.fillRoundedRectangle (r, 8.0f);

    auto* lf = dynamic_cast<AetherLookAndFeel*> (&getLookAndFeel());
    const auto accent = lf != nullptr ? lf->getAccent() : palette::accentDefault;
    g.setColour (dragOver ? palette::warm : (selected ? accent : palette::outline));
    g.drawRoundedRectangle (r, 8.0f, selected || dragOver ? 2.0f : 1.0f);

    // Header with the slot number (drag handle).
    g.setColour (palette::textDim);
    g.setFont (AetherLookAndFeel::font (11.0f, true));
    g.drawText (juce::String (index + 1), getLocalBounds().removeFromTop (22).reduced (10, 0), juce::Justification::centredLeft);
    g.drawText (juce::CharPointer_UTF8 ("\xe2\x8b\xae\xe2\x8b\xae"), getLocalBounds().removeFromTop (22).reduced (10, 0),
                juce::Justification::centredRight);

    if (module == ModuleType::None)
    {
        g.setColour (palette::textDim.withAlpha (0.5f));
        g.setFont (AetherLookAndFeel::font (12.0f));
        g.drawText ("empty", getLocalBounds().withTrimmedTop (60), juce::Justification::centredTop);
    }
}

void SlotTile::resized()
{
    auto r = getLocalBounds().reduced (6);
    r.removeFromTop (18);
    typeBox.setBounds (r.removeFromTop (24));
    r.removeFromTop (4);
    if (onSwitch == nullptr)
        return;
    onSwitch->setBounds (r.removeFromTop (24));
    auto bottom = r.removeFromBottom (24);
    soloSwitch->setBounds (bottom.removeFromLeft (bottom.getWidth() / 2));
    parSwitch->setBounds (bottom);
    mixKnob->setBounds (r.reduced (4, 2));
}

void SlotTile::mouseDown (const juce::MouseEvent&) { owner.selectSlot (index); }

void SlotTile::mouseDrag (const juce::MouseEvent& e)
{
    if (module != ModuleType::None && e.getDistanceFromDragStart() > 6)
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
            if (! container->isDragAndDropActive())
                container->startDragging ("slot:" + juce::String (index), this);
}

bool SlotTile::isInterestedInDragSource (const SourceDetails& d)
{
    return d.description.toString().startsWith ("slot:");
}

void SlotTile::itemDropped (const SourceDetails& d)
{
    dragOver = false;
    const int from = d.description.toString().fromFirstOccurrenceOf ("slot:", false, false).getIntValue();
    owner.moveSlot (from, index);
    repaint();
}

//==============================================================================
GatePatternEditor::GatePatternEditor (AetherfallProcessor& p) : processor (p)
{
    shown = processor.getGatePattern();
    startTimerHz (5);
}

void GatePatternEditor::timerCallback()
{
    const auto now = processor.getGatePattern();
    if (now != shown) { shown = now; repaint(); }
}

void GatePatternEditor::paint (juce::Graphics& g)
{
    auto* lf = dynamic_cast<AetherLookAndFeel*> (&getLookAndFeel());
    const auto accent = lf != nullptr ? lf->getAccent() : palette::accentDefault;
    const float w = (float) getWidth() / 16.0f;
    for (int s = 0; s < 16; ++s)
    {
        const auto cell = juce::Rectangle<float> ((float) s * w, 0.0f, w, (float) getHeight()).reduced (2.0f);
        const bool on = ((shown >> s) & 1u) != 0;
        g.setColour (on ? accent : palette::track);
        g.fillRoundedRectangle (cell, 3.0f);
        if (s % 4 == 0)
        {
            g.setColour (palette::text.withAlpha (0.25f));
            g.drawRoundedRectangle (cell, 3.0f, 1.0f);
        }
    }
}

void GatePatternEditor::mouseDown (const juce::MouseEvent& e)
{
    const int s = juce::jlimit (0, 15, (int) ((float) e.x / ((float) getWidth() / 16.0f)));
    processor.setGatePattern (shown ^ (1u << s));
    shown = processor.getGatePattern();
    repaint();
}

//==============================================================================
ChainView::ChainView (AetherfallProcessor& p, juce::AudioProcessorValueTreeState& s)
    : processor (p), apvts (s)
{
    for (size_t i = 0; i < tiles.size(); ++i)
    {
        tiles[i] = std::make_unique<SlotTile> (*this, (int) i);
        addAndMakeVisible (*tiles[i]);
    }
    addAndMakeVisible (panel);
    refreshFromProcessor (true);
    startTimerHz (8);
}

ChainView::~ChainView() { stopTimer(); }

void ChainView::timerCallback() { refreshFromProcessor (false); }

void ChainView::refreshFromProcessor (bool force)
{
    const auto order = processor.getChainOrder();
    if (! force && order == shownOrder)
        return;

    for (size_t i = 0; i < tiles.size(); ++i)
        if (force || tiles[i]->getModule() != order[i])
            tiles[i]->setModule (apvts, order[i]);

    shownOrder = order;
    rebuildPanel();
}

void ChainView::selectSlot (int index)
{
    selected = juce::jlimit (0, numChainSlots - 1, index);
    rebuildPanel();
}

void ChainView::moveSlot (int from, int to)
{
    if (from == to || from < 0 || to < 0 || from >= numChainSlots || to >= numChainSlots)
        return;

    // Move the module: the others shift to make room (like dragging a card).
    auto order = processor.getChainOrder();
    const auto moving = order[(size_t) from];
    if (from < to)
        for (int i = from; i < to; ++i) order[(size_t) i] = order[(size_t) i + 1];
    else
        for (int i = from; i > to; --i) order[(size_t) i] = order[(size_t) i - 1];
    order[(size_t) to] = moving;

    processor.setChainOrder (order);
    selected = to;
    refreshFromProcessor (true);
}

void ChainView::setSlotModule (int index, ModuleType type)
{
    auto order = processor.getChainOrder();
    // A module lives in one slot only: picking it here moves it from elsewhere.
    for (auto& t : order)
        if (t == type)
            t = ModuleType::None;
    order[(size_t) index] = type;
    processor.setChainOrder (order);
    selected = index;
    refreshFromProcessor (true);
}

void ChainView::rebuildPanel()
{
    for (size_t i = 0; i < tiles.size(); ++i)
        tiles[i]->setSelected ((int) i == selected);

    panelWidgets.clear();
    gateEditor.reset();

    const auto type = shownOrder[(size_t) selected];
    if (type == ModuleType::None)
    {
        panel.setTitle ("Slot " + juce::String (selected + 1) + " - empty (choose a module above)");
        resized();
        return;
    }

    const auto& mi = moduleInfo (type);
    panel.setTitle (juce::String (std::string (mi.name)));
    for (int p = (int) mi.firstParam; p <= (int) mi.lastParam; ++p)
    {
        panelWidgets.push_back (createParamWidget (apvts, (PID) p));
        panel.addAndMakeVisible (*panelWidgets.back());
    }

    if (type == ModuleType::Gate)
    {
        gateEditor = std::make_unique<GatePatternEditor> (processor);
        panel.addAndMakeVisible (*gateEditor);
    }
    resized();
}

void ChainView::paint (juce::Graphics& g)
{
    // Signal-flow arrows between the slots.
    g.setColour (palette::textDim.withAlpha (0.5f));
    for (size_t i = 0; i + 1 < tiles.size(); ++i)
    {
        const auto a = tiles[i]->getBounds(), b = tiles[i + 1]->getBounds();
        const float y = (float) a.getY() + 30.0f;
        juce::Path arrow;
        arrow.addArrow ({ (float) a.getRight() - 1.0f, y, (float) b.getX() + 1.0f, y }, 1.5f, 6.0f, 5.0f);
        g.fillPath (arrow);
    }
}

void ChainView::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto row = r.removeFromTop (190);
    const int gap = 10;
    const int w = (row.getWidth() - gap * (numChainSlots - 1)) / numChainSlots;
    for (auto& t : tiles)
    {
        t->setBounds (row.removeFromLeft (w));
        row.removeFromLeft (gap);
    }

    r.removeFromTop (12);
    panel.setBounds (r);

    auto c = panel.getContentBounds();
    if (gateEditor != nullptr)
    {
        gateEditor->setBounds (c.removeFromBottom (44).reduced (8, 6));
        c.removeFromBottom (6);
    }

    // Knobs first, then choices and toggles in a second row.
    std::vector<juce::Component*> knobs, others;
    for (auto& w2 : panelWidgets)
        (dynamic_cast<Knob*> (w2.get()) != nullptr ? knobs : others).push_back (w2.get());

    const int cols = std::max (6, (int) knobs.size());
    const bool twoRows = ! others.empty();
    auto knobArea = twoRows ? c.removeFromTop (c.getHeight() * 3 / 5) : c;
    layoutGrid (knobs, knobArea.withSizeKeepingCentre (std::min (knobArea.getWidth(), cols * 110), std::min (knobArea.getHeight(), 140)),
                cols, std::min (knobArea.getHeight(), 140));
    if (twoRows)
        layoutGrid (others, c.reduced (8, 10).withHeight (std::min (c.getHeight() - 20, 50)), std::max (4, (int) others.size()), std::min (c.getHeight() - 20, 50));
}

} // namespace aetherfall
