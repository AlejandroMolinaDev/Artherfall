#include "AetherLookAndFeel.h"

namespace aetherfall
{

AetherLookAndFeel::AetherLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, palette::background);
    setColour (juce::Label::textColourId, palette::text);
    setColour (juce::Slider::textBoxTextColourId, palette::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, palette::panelRaised);
    setColour (juce::ComboBox::outlineColourId, palette::outline);
    setColour (juce::ComboBox::textColourId, palette::text);
    setColour (juce::ComboBox::arrowColourId, palette::textDim);
    setColour (juce::PopupMenu::backgroundColourId, palette::panelRaised);
    setColour (juce::PopupMenu::textColourId, palette::text);
    setColour (juce::PopupMenu::highlightedTextColourId, palette::background);
    setColour (juce::TextButton::buttonColourId, palette::panelRaised);
    setColour (juce::TextButton::textColourOffId, palette::textDim);
    setColour (juce::TextButton::textColourOnId, palette::text);
    setColour (juce::TooltipWindow::backgroundColourId, palette::panelRaised);
    setColour (juce::TooltipWindow::textColourId, palette::text);
    setColour (juce::TooltipWindow::outlineColourId, palette::outline);
    setAccent (palette::accentDefault);
}

void AetherLookAndFeel::setAccent (juce::Colour c)
{
    accent = c;
    setColour (juce::Slider::thumbColourId, c);
    setColour (juce::Slider::rotarySliderFillColourId, c);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, c);
    setColour (juce::TextButton::buttonOnColourId, c.withAlpha (0.25f));
}

juce::Font AetherLookAndFeel::font (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

//==============================================================================
void AetherLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                          float startAngle, float endAngle, juce::Slider& slider)
{
    const bool macro = slider.getProperties()[macroProperty];
    const bool bipolar = slider.getProperties()[bipolarProperty];
    const bool enabled = slider.isEnabled();

    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (macro ? 6.0f : 3.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float thickness = std::max (2.0f, radius * (macro ? 0.10f : 0.13f));
    const float arcRadius = radius - thickness * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto col = enabled ? accent : palette::textDim;

    // Body.
    const float bodyR = arcRadius - thickness * (macro ? 1.6f : 1.4f);
    if (macro)
    {
        // Soft glow proportional to the value.
        juce::ColourGradient glow (col.withAlpha (0.10f + 0.25f * pos), centre.x, centre.y,
                                   col.withAlpha (0.0f), centre.x + radius * 1.1f, centre.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (bounds.expanded (4.0f));
    }

    juce::ColourGradient body (palette::panelRaised.brighter (0.08f), centre.x, centre.y - bodyR,
                               palette::panel, centre.x, centre.y + bodyR, false);
    g.setGradientFill (body);
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (palette::outline);
    g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

    // Track arc.
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (palette::track);
    g.strokePath (track, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc (from the start, or from the centre for bipolar parameters).
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                             std::min (from, angle), std::max (from, angle), true);
        g.setColour (col);
        g.strokePath (value, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Pointer.
    const float pLen = bodyR * 0.62f;
    const juce::Point<float> tip (centre.x + std::sin (angle) * pLen, centre.y - std::cos (angle) * pLen);
    const juce::Point<float> base (centre.x + std::sin (angle) * bodyR * 0.18f, centre.y - std::cos (angle) * bodyR * 0.18f);
    g.setColour (enabled ? palette::text : palette::textDim);
    g.drawLine ({ base, tip }, std::max (1.5f, radius * 0.06f));
}

void AetherLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                          float, float, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    const auto r = juce::Rectangle<int> (x, y, width, height).toFloat();
    const bool horizontal = style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar;
    const float t = 4.0f;

    juce::Rectangle<float> trackRect = horizontal ? r.withSizeKeepingCentre (r.getWidth(), t)
                                                  : r.withSizeKeepingCentre (t, r.getHeight());
    g.setColour (palette::track);
    g.fillRoundedRectangle (trackRect, t * 0.5f);

    g.setColour (slider.isEnabled() ? accent : palette::textDim);
    if (horizontal)
        g.fillRoundedRectangle (trackRect.withRight (sliderPos), t * 0.5f);
    else
        g.fillRoundedRectangle (trackRect.withTop (sliderPos), t * 0.5f);

    const auto thumb = horizontal ? juce::Point<float> (sliderPos, r.getCentreY()) : juce::Point<float> (r.getCentreX(), sliderPos);
    g.setColour (palette::text);
    g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (thumb));
}

void AetherLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto r = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();

    // Pill switch on the left, text on the right.
    const float h = std::min (16.0f, r.getHeight() - 4.0f);
    const juce::Rectangle<float> pill (r.getX() + 2.0f, r.getCentreY() - h * 0.5f, h * 1.8f, h);
    g.setColour (on ? accent.withAlpha (0.9f) : palette::track);
    g.fillRoundedRectangle (pill, h * 0.5f);
    if (highlighted)
    {
        g.setColour (palette::text.withAlpha (0.15f));
        g.drawRoundedRectangle (pill, h * 0.5f, 1.0f);
    }
    const float knobD = h - 4.0f;
    const float kx = on ? pill.getRight() - knobD - 2.0f : pill.getX() + 2.0f;
    g.setColour (on ? palette::background : palette::textDim);
    g.fillEllipse (kx, pill.getY() + 2.0f, knobD, knobD);

    if (b.getButtonText().isNotEmpty())
    {
        g.setColour (on ? palette::text : palette::textDim);
        g.setFont (font (13.0f));
        g.drawText (b.getButtonText(), r.withTrimmedLeft (pill.getWidth() + 8.0f), juce::Justification::centredLeft, true);
    }
}

void AetherLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                              bool highlighted, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    auto fill = on ? accent.withAlpha (0.22f) : palette::panelRaised;
    if (highlighted) fill = fill.brighter (0.08f);
    if (down) fill = fill.brighter (0.15f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (on ? accent.withAlpha (0.8f) : palette::outline);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

void AetherLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? palette::text : palette::textDim);
    g.setFont (font (std::min (14.0f, (float) b.getHeight() * 0.5f), b.getToggleState()));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, true);
}

void AetherLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                      int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    g.setColour (palette::panelRaised);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.hasKeyboardFocus (true) ? accent : palette::outline);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    // Small chevron.
    const float cx = (float) width - 12.0f, cy = (float) height * 0.5f;
    juce::Path p;
    p.startNewSubPath (cx - 4.0f, cy - 2.0f);
    p.lineTo (cx, cy + 2.0f);
    p.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (palette::textDim);
    g.strokePath (p, juce::PathStrokeType (1.5f));
}

juce::Font AetherLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return font (std::min (14.0f, (float) box.getHeight() * 0.55f));
}

void AetherLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 0, box.getWidth() - 24, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

juce::Font AetherLookAndFeel::getPopupMenuFont() { return font (14.0f); }

void AetherLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (palette::panelRaised);
    g.setColour (palette::outline);
    g.drawRect (0, 0, width, height);
}

void AetherLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    g.setColour (palette::panelRaised);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (palette::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    g.setColour (palette::text);
    g.setFont (font (13.0f));
    g.drawFittedText (text, r.reduced (8.0f, 4.0f).toNearestInt(), juce::Justification::centredLeft, 3);
}

} // namespace aetherfall
