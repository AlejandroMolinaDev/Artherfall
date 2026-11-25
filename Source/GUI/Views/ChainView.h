#pragma once

#include <array>
#include <memory>
#include <vector>
#include "GUI/Components/ParamWidgets.h"
#include "DSP/Chain/ChainOrder.h"

namespace aetherfall
{

class AetherfallProcessor;
class ChainView;

/** One chain slot: module selector, on/off, mix, solo, parallel. Drag the
    header onto another slot to move the module there. */
class SlotTile final : public juce::Component, public juce::DragAndDropTarget
{
public:
    SlotTile (ChainView& owner, int index);
    void setModule (juce::AudioProcessorValueTreeState&, ModuleType);
    ModuleType getModule() const noexcept { return module; }
    void setSelected (bool s) { selected = s; repaint(); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dragOver = true; repaint(); }
    void itemDragExit (const SourceDetails&) override  { dragOver = false; repaint(); }
    void itemDropped (const SourceDetails&) override;

    juce::ComboBox typeBox;

private:
    ChainView& owner;
    const int index;
    ModuleType module = ModuleType::None;
    std::unique_ptr<ParamWidget> onSwitch, mixKnob, soloSwitch, parSwitch;
    bool selected = false, dragOver = false;
};

/** 16-step editor for the Trance Gate pattern (stored in the plug-in state). */
class GatePatternEditor final : public juce::Component, private juce::Timer
{
public:
    explicit GatePatternEditor (AetherfallProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    AetherfallProcessor& processor;
    std::uint32_t shown = 0;
};

/** CHAIN view: the 8 slots in a row and the parameters of the selected slot. */
class ChainView final : public juce::Component, public juce::DragAndDropContainer, private juce::Timer
{
public:
    ChainView (AetherfallProcessor&, juce::AudioProcessorValueTreeState&);
    ~ChainView() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    // Called by the tiles.
    void selectSlot (int index);
    void moveSlot (int from, int to);
    void setSlotModule (int index, ModuleType type);

private:
    void timerCallback() override;
    void refreshFromProcessor (bool force);
    void rebuildPanel();

    AetherfallProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    std::array<std::unique_ptr<SlotTile>, (size_t) numChainSlots> tiles;
    chain::Order shownOrder = chain::emptyOrder();
    int selected = 0;

    Section panel;
    std::vector<std::unique_ptr<ParamWidget>> panelWidgets;
    std::unique_ptr<GatePatternEditor> gateEditor;
};

} // namespace aetherfall
