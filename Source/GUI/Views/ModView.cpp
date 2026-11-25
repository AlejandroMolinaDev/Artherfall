#include "ModView.h"
#include "GUI/LookAndFeel/AetherLookAndFeel.h"

namespace aetherfall
{

void ModView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (12).toFloat();
    g.setColour (palette::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (palette::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    g.setColour (palette::textDim);
    g.setFont (AetherLookAndFeel::font (15.0f));
    g.drawText ("Modulation editor (LFOs, envelopes, sequencers, matrix) - coming in F8",
                r, juce::Justification::centred);
}

} // namespace aetherfall
