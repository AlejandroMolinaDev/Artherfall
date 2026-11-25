#pragma once

#include <array>
#include <memory>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "GUI/LookAndFeel/AetherLookAndFeel.h"
#include "GUI/Components/ParamWidgets.h"

namespace aetherfall
{

class AetherfallProcessor;

/**
    Main editor. All views are laid out at the base size (1000 x 640) inside a
    content component that is scaled with an AffineTransform, so the UI stays
    vector-sharp from 75 % to 200 %. The chosen scale is saved in the plug-in
    state (UI node) and restored when the editor reopens.
*/
class AetherfallEditor final : public juce::AudioProcessorEditor,
                               public juce::DragAndDropContainer,
                               private juce::Timer
{
public:
    explicit AetherfallEditor (AetherfallProcessor&);
    ~AetherfallEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    AetherfallProcessor& getProcessor() noexcept { return processor; }

    static constexpr int baseWidth = 1000, baseHeight = 640, headerHeight = 84;

    enum class Tab { Main = 0, Chain, Core, Mod, Output };
    void showTab (Tab);
    Tab getCurrentTab() const noexcept { return currentTab; }

    /** The scaled content (exposed for snapshot tests). */
    juce::Component& getContent() noexcept { return content; }

    void showPresetBrowser (bool show);
    bool isPresetBrowserVisible() const noexcept { return presetBrowser != nullptr; }

private:
    class Header;
    void timerCallback() override;
    void saveScale (float scale);
    float loadScale() const;

    AetherfallProcessor& processor;
    AetherLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };

    juce::Component content;
    std::unique_ptr<Header> header;
    std::array<std::unique_ptr<juce::Component>, 5> views;
    Tab currentTab = Tab::Main;
    std::unique_ptr<juce::Component> presetBrowser;
    int shownCategory = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AetherfallEditor)
};

} // namespace aetherfall
