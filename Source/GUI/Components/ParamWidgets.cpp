#include "ParamWidgets.h"
#include "GUI/LookAndFeel/AetherLookAndFeel.h"

namespace aetherfall
{

namespace
{
    juce::RangedAudioParameter* findParam (juce::AudioProcessorValueTreeState& apvts, PID pid)
    {
        return apvts.getParameter (juce::String (std::string (info (pid).id)));
    }

    juce::String paramId (PID pid) { return juce::String (std::string (info (pid).id)); }
}

juce::String shortLabel (PID pid)
{
    const juce::String name (std::string (info (pid).name));
    // Drop the section prefix ("Core ", "Gran ", "Glue "...) when there is one.
    static const char* prefixes[] = { "Core ", "Gran ", "Spec ", "ShDly ", "Tape ", "MTap ", "Drone ", "Reso ",
                                      "Comb ", "Filt ", "ModFX ", "Sat ", "Crush ", "Ring ", "Stereo ", "Gate ",
                                      "RevSw ", "Tex ", "LoFi ", "Glue ", "Duck ", "In ", "Out " };
    for (auto* p : prefixes)
        if (name.startsWith (p) && name.length() > (int) std::strlen (p))
            return name.substring ((int) std::strlen (p));
    return name;
}

//==============================================================================
FineSlider::FineSlider()
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    setMouseDragSensitivity (250);
}

void FineSlider::updateSensitivity (const juce::ModifierKeys& mods)
{
    setMouseDragSensitivity ((mods.isCommandDown() || mods.isCtrlDown()) ? 1250 : 250);
}

void FineSlider::mouseDown (const juce::MouseEvent& e)
{
    updateSensitivity (e.mods);
    juce::Slider::mouseDown (e);
}

void FineSlider::mouseDrag (const juce::MouseEvent& e)
{
    updateSensitivity (e.mods);
    juce::Slider::mouseDrag (e);
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& apvts, PID p, Size s, const juce::String& labelOverride)
    : ParamWidget (p), size (s)
{
    param = findParam (apvts, p);
    label = labelOverride.isNotEmpty() ? labelOverride : shortLabel (p);

    const auto& inf = info (p);
    slider.getProperties().set (AetherLookAndFeel::bipolarProperty, inf.minValue < 0.0f && inf.maxValue > 0.0f);
    slider.getProperties().set (AetherLookAndFeel::macroProperty, s == Size::Macro);
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId (p), slider);
    slider.setDoubleClickReturnValue (true, (double) inf.defaultValue);
    slider.onValueChange = [this] { refreshText(); };
    slider.onDragStart = [this] { showValue = true; refreshText(); };
    slider.onDragEnd = [this] { showValue = isMouseOver (true); refreshText(); };
    slider.addMouseListener (this, false);
    refreshText();
}

Knob::~Knob() { slider.removeMouseListener (this); }

void Knob::refreshText()
{
    if (param != nullptr)
        valueText = param->getCurrentValueAsText();
    slider.setTooltip (juce::String (std::string (info (pid).name)) + ": " + valueText);
    repaint();
}

void Knob::mouseEnter (const juce::MouseEvent&) { showValue = true; refreshText(); }
void Knob::mouseExit (const juce::MouseEvent&)  { showValue = slider.isMouseButtonDown(); refreshText(); }

void Knob::resized()
{
    // Knob + caption as one block, vertically centred in the cell, so the
    // caption always sits right under the knob whatever the cell height.
    const int textH = size == Size::Macro ? 22 : 16;
    const int maxD = size == Size::Macro ? 150 : (size == Size::Small ? 64 : 96);
    const int d = std::max (16, std::min ({ getWidth() - 4, getHeight() - textH, maxD }));
    const int y0 = (getHeight() - (d + textH)) / 2;
    slider.setBounds ((getWidth() - d) / 2, y0, d, d);
    textArea = { 0, y0 + d, getWidth(), textH };
}

void Knob::paint (juce::Graphics& g)
{
    const float fontH = size == Size::Macro ? 15.0f : (size == Size::Small ? 11.0f : 12.0f);

    g.setFont (AetherLookAndFeel::font (fontH, size == Size::Macro));
    g.setColour (showValue ? palette::text : (size == Size::Macro ? palette::text : palette::textDim));
    g.drawFittedText (showValue ? valueText : label, textArea, juce::Justification::centred, 1, 0.8f);
}

//==============================================================================
ChoiceBox::ChoiceBox (juce::AudioProcessorValueTreeState& apvts, PID p, const juce::String& labelOverride)
    : ParamWidget (p)
{
    label = labelOverride.isNotEmpty() ? labelOverride : shortLabel (p);
    const auto list = choices::get (info (p).choiceSet);
    for (size_t i = 0; i < list.size; ++i)
        box.addItem (juce::String (std::string (list.data[i])), (int) i + 1);
    box.setTooltip (juce::String (std::string (info (p).name)));
    addAndMakeVisible (box);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, paramId (p), box);
}

ChoiceBox::~ChoiceBox() = default;

void ChoiceBox::resized()
{
    // Caption (15 px) + box (24 px) as one block, vertically centred.
    const int boxH = std::min (24, std::max (12, getHeight() - 15));
    const int y0 = std::max (0, (getHeight() - (15 + boxH)) / 2);
    box.setBounds (0, y0 + 15, getWidth(), boxH);
}

void ChoiceBox::paint (juce::Graphics& g)
{
    g.setFont (AetherLookAndFeel::font (12.0f));
    g.setColour (palette::textDim);
    g.drawText (label, juce::Rectangle<int> (0, box.getY() - 15, getWidth(), 15), juce::Justification::centredLeft, true);
}

//==============================================================================
ToggleSwitch::ToggleSwitch (juce::AudioProcessorValueTreeState& apvts, PID p, const juce::String& labelOverride)
    : ParamWidget (p)
{
    button.setButtonText (labelOverride.isNotEmpty() ? labelOverride : shortLabel (p));
    button.setTooltip (juce::String (std::string (info (p).name)));
    addAndMakeVisible (button);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, paramId (p), button);
}

ToggleSwitch::~ToggleSwitch() = default;

void ToggleSwitch::resized()
{
    button.setBounds (getLocalBounds().withSizeKeepingCentre (getWidth(), std::min (24, getHeight())));
}

//==============================================================================
std::unique_ptr<ParamWidget> createParamWidget (juce::AudioProcessorValueTreeState& apvts, PID pid,
                                                Knob::Size size, const juce::String& labelOverride)
{
    switch (info (pid).kind)
    {
        case Kind::Bool:   return std::make_unique<ToggleSwitch> (apvts, pid, labelOverride);
        case Kind::Choice: return std::make_unique<ChoiceBox> (apvts, pid, labelOverride);
        case Kind::Float:  break;
    }
    return std::make_unique<Knob> (apvts, pid, size, labelOverride);
}

//==============================================================================
void Section::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (palette::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (palette::outline);
    g.drawRoundedRectangle (r, 8.0f, 1.0f);

    if (title.isNotEmpty())
    {
        g.setColour (palette::textDim);
        g.setFont (AetherLookAndFeel::font (11.5f, true));
        g.drawText (title.toUpperCase(), getLocalBounds().removeFromTop (titleHeight).reduced (12, 0),
                    juce::Justification::centredLeft, true);
    }
}

juce::Rectangle<int> Section::getContentBounds() const
{
    auto r = getLocalBounds().reduced (8, 6);
    if (title.isNotEmpty())
        r.removeFromTop (titleHeight - 4);
    return r;
}

void layoutGrid (const std::vector<juce::Component*>& items, juce::Rectangle<int> area, int columns, int rowHeight)
{
    if (items.empty() || columns <= 0)
        return;
    const int colW = area.getWidth() / columns;
    for (size_t i = 0; i < items.size(); ++i)
    {
        const int col = (int) i % columns, row = (int) i / columns;
        items[i]->setBounds (area.getX() + col * colW, area.getY() + row * rowHeight, colW, rowHeight);
    }
}

} // namespace aetherfall
