#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace aetherfall
{

/** Colour palette of the interface (dark, one accent colour). */
namespace palette
{
    inline const juce::Colour background   { 0xff0b0d12 };
    inline const juce::Colour panel        { 0xff12151d };
    inline const juce::Colour panelRaised  { 0xff181c26 };
    inline const juce::Colour outline      { 0xff262b38 };
    inline const juce::Colour track        { 0xff2a3040 };
    inline const juce::Colour text         { 0xffe6e9f2 };
    inline const juce::Colour textDim      { 0xff8a90a2 };
    inline const juce::Colour accentDefault { 0xff8c7cff };
    inline const juce::Colour warm         { 0xffffb36b };
    inline const juce::Colour good         { 0xff5fd6a4 };
    inline const juce::Colour danger       { 0xffff5f6d };
}

/**
    Vector look and feel ("clean Valhalla-like"): flat dark panels, arc knobs
    with an accent value arc, pill toggles, minimal combo boxes. Everything is
    drawn with paths so the UI stays sharp at any scale (75..200 %).
*/
class AetherLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    AetherLookAndFeel();

    void setAccent (juce::Colour c);
    juce::Colour getAccent() const noexcept { return accent; }

    // Slider property used by drawRotarySlider: "bipolar" (value arc from the centre),
    // "macro" (big knob with glow).
    static inline const juce::Identifier bipolarProperty { "aetherBipolar" };
    static inline const juce::Identifier macroProperty { "aetherMacro" };

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;

    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;

    /** Fonts used across the UI. */
    static juce::Font font (float height, bool bold = false);

private:
    juce::Colour accent = palette::accentDefault;
};

} // namespace aetherfall
