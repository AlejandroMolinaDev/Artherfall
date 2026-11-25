#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters/ParamIDs.h"

namespace aetherfall
{

/** Slider with fine adjustment: hold Ctrl (Cmd on macOS) while dragging. */
class FineSlider final : public juce::Slider
{
public:
    FineSlider();
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    void updateSensitivity (const juce::ModifierKeys&);
};

/** Base class of every parameter control (knob, choice, toggle). */
class ParamWidget : public juce::Component
{
public:
    explicit ParamWidget (PID p) : pid (p) {}
    PID getPid() const noexcept { return pid; }

protected:
    const PID pid;
};

/**
    Rotary knob bound to a parameter: name under the knob, the value replaces
    the name while hovering or dragging. Double-click = default value,
    Ctrl/Cmd-drag = fine, tooltip with name and value.
*/
class Knob final : public ParamWidget
{
public:
    enum class Size { Small, Normal, Macro };

    Knob (juce::AudioProcessorValueTreeState&, PID, Size = Size::Normal, const juce::String& labelOverride = {});
    ~Knob() override;

    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    juce::Slider& getSlider() noexcept { return slider; }
    Size getSize() const noexcept { return size; }

private:
    void refreshText();

    juce::RangedAudioParameter* param = nullptr;
    FineSlider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    Size size;
    juce::String label, valueText;
    juce::Rectangle<int> textArea;
    bool showValue = false;
};

/** Combo box bound to a choice parameter, with a small caption. */
class ChoiceBox final : public ParamWidget
{
public:
    ChoiceBox (juce::AudioProcessorValueTreeState&, PID, const juce::String& labelOverride = {});
    ~ChoiceBox() override;
    void resized() override;
    void paint (juce::Graphics&) override;

    juce::ComboBox& getComboBox() noexcept { return box; }

private:
    juce::ComboBox box;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    juce::String label;
};

/** Pill toggle bound to a bool parameter. */
class ToggleSwitch final : public ParamWidget
{
public:
    ToggleSwitch (juce::AudioProcessorValueTreeState&, PID, const juce::String& labelOverride = {});
    ~ToggleSwitch() override;
    void resized() override;

    juce::ToggleButton& getButton() noexcept { return button; }

private:
    juce::ToggleButton button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

/** Creates the natural widget for a parameter (knob / choice / toggle). */
std::unique_ptr<ParamWidget> createParamWidget (juce::AudioProcessorValueTreeState&, PID,
                                                Knob::Size size = Knob::Size::Normal,
                                                const juce::String& labelOverride = {});

/** Short label for a parameter inside its own section ("Core Decay" -> "Decay"). */
juce::String shortLabel (PID pid);

/** Titled, rounded panel used to group controls. */
class Section : public juce::Component
{
public:
    explicit Section (const juce::String& sectionTitle = {}) : title (sectionTitle) {}
    void setTitle (const juce::String& t) { title = t; repaint(); }
    void paint (juce::Graphics&) override;

    /** Area available for children (inside the title bar and padding). */
    juce::Rectangle<int> getContentBounds() const;

    static constexpr int titleHeight = 22;

private:
    juce::String title;
};

/** Lays widgets out in a grid of `columns` columns inside `area`. */
void layoutGrid (const std::vector<juce::Component*>& items, juce::Rectangle<int> area, int columns, int rowHeight);

} // namespace aetherfall
