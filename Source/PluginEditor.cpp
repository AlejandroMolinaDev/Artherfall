#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "PluginInfo.h"

namespace aetherfall
{

static constexpr int baseWidth  = 1000;
static constexpr int baseHeight = 640;
static constexpr int headerHeight = 56;

AetherfallEditor::AetherfallEditor (AetherfallProcessor& p)
    : AudioProcessorEditor (p), processor (p), generic (p)
{
    addAndMakeVisible (generic);

    setResizable (true, true);
    setResizeLimits (baseWidth * 3 / 4, baseHeight * 3 / 4, baseWidth * 2, baseHeight * 2);
    setSize (baseWidth, baseHeight);
}

AetherfallEditor::~AetherfallEditor() = default;

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
}

void AetherfallEditor::resized()
{
    generic.setBounds (getLocalBounds().withTrimmedTop (headerHeight));
}

} // namespace aetherfall
