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
/** Top bar: logo, view tabs, bypass. */
class AetherfallEditor::Header final : public juce::Component
{
public:
    Header (AetherfallEditor& e, juce::AudioProcessorValueTreeState& apvts) : editor (e)
    {
        static const char* names[] = { "MAIN", "CHAIN", "CORE", "MOD", "OUTPUT" };
        for (size_t i = 0; i < tabs.size(); ++i)
        {
            auto& b = tabs[i];
            b.setButtonText (names[i]);
            b.setClickingTogglesState (false);
            b.onClick = [this, i] { editor.showTab ((Tab) i); };
            addAndMakeVisible (b);
        }
        bypass = std::make_unique<ToggleSwitch> (apvts, PID::Bypass, "Bypass");
        addAndMakeVisible (*bypass);
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
        auto logo = r.removeFromLeft (220).reduced (18, 0);
        juce::ColourGradient grad (palette::text, (float) logo.getX(), 0.0f, accent, (float) logo.getRight(), 0.0f, false);
        g.setGradientFill (grad);
        g.setFont (AetherLookAndFeel::font (22.0f, true));
        g.drawText ("AETHERFALL", logo, juce::Justification::centredLeft);

        g.setColour (palette::textDim);
        g.setFont (AetherLookAndFeel::font (11.0f));
        g.drawText (juce::String ("v") + product::version, r.removeFromRight (190).withTrimmedRight (110),
                    juce::Justification::centredRight);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (0, 12);
        r.removeFromLeft (230);
        bypass->setBounds (r.removeFromRight (110).reduced (8, 0));
        r.removeFromRight (90);
        for (auto& b : tabs)
            b.setBounds (r.removeFromLeft (92).reduced (3, 0));
    }

private:
    AetherfallEditor& editor;
    std::array<juce::TextButton, 5> tabs;
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
    views[(size_t) Tab::Mod]    = std::make_unique<ModView> (processor);
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
