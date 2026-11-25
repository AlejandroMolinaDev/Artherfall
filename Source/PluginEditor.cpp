#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "PluginInfo.h"
#include "DSP/Modules/ModuleFactory.h"

namespace aetherfall
{

static constexpr int baseWidth  = 1000;
static constexpr int baseHeight = 640;
static constexpr int headerHeight = 56;
static constexpr int chainRowHeight = 44;

AetherfallEditor::AetherfallEditor (AetherfallProcessor& p)
    : AudioProcessorEditor (p), processor (p), generic (p)
{
    for (size_t i = 0; i < slots.size(); ++i)
    {
        auto& box = slots[i];
        box.addItem ("-", 1);
        for (int t = 1; t <= numModuleTypes; ++t)
        {
            const auto& mi = moduleInfo ((ModuleType) t);
            juce::String name (std::string (mi.name));
            if (! isModuleImplemented ((ModuleType) t))
                name << " (F4)";
            box.addItem (name, t + 1);
        }
        box.setTooltip ("Chain slot " + juce::String ((int) i + 1));
        box.onChange = [this] { slotChanged(); };
        addAndMakeVisible (box);
    }

    addAndMakeVisible (generic);
    refreshSlotsFromProcessor();
    startTimerHz (4);

    setResizable (true, true);
    setResizeLimits (baseWidth * 3 / 4, baseHeight * 3 / 4, baseWidth * 2, baseHeight * 2);
    setSize (baseWidth, baseHeight);
}

AetherfallEditor::~AetherfallEditor() = default;

void AetherfallEditor::refreshSlotsFromProcessor()
{
    const auto order = processor.getChainOrder();
    updatingSlots = true;
    for (size_t i = 0; i < slots.size(); ++i)
        slots[i].setSelectedId ((int) order[i] + 1, juce::dontSendNotification);
    updatingSlots = false;
}

void AetherfallEditor::slotChanged()
{
    if (updatingSlots)
        return;

    auto order = chain::emptyOrder();
    for (size_t i = 0; i < slots.size(); ++i)
        order[i] = (ModuleType) std::max (0, slots[i].getSelectedId() - 1);

    processor.setChainOrder (order);   // duplicates are removed here...
    refreshSlotsFromProcessor();        // ...and the UI shows the result
}

void AetherfallEditor::timerCallback()
{
    // Reflect state changes coming from the host (project/preset load).
    const auto order = processor.getChainOrder();
    for (size_t i = 0; i < slots.size(); ++i)
        if (slots[i].getSelectedId() != (int) order[i] + 1)
        {
            refreshSlotsFromProcessor();
            break;
        }
}

void AetherfallEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0f14));

    auto header = getLocalBounds().removeFromTop (headerHeight);
    g.setColour (juce::Colour (0xff151922));
    g.fillRect (header);

    g.setColour (juce::Colour (0xffb9c6ff));
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText (product::name, header.reduced (20, 0), juce::Justification::centredLeft);

    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (juce::String ("v") + product::version + "  -  dev build (GUI arrives in F7)",
                header.reduced (20, 0), juce::Justification::centredRight);

    auto row = getLocalBounds().withTrimmedTop (headerHeight).removeFromTop (chainRowHeight);
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("CHAIN", row.removeFromLeft (70).reduced (12, 0), juce::Justification::centredLeft);
}

void AetherfallEditor::resized()
{
    auto area = getLocalBounds().withTrimmedTop (headerHeight);
    auto row = area.removeFromTop (chainRowHeight).reduced (8, 8);
    row.removeFromLeft (62);

    const int w = row.getWidth() / (int) slots.size();
    for (auto& box : slots)
        box.setBounds (row.removeFromLeft (w).reduced (3, 0));

    generic.setBounds (area);
}

} // namespace aetherfall
