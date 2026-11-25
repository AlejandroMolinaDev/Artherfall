#include "Meters.h"
#include "GUI/LookAndFeel/AetherLookAndFeel.h"
#include "DSP/Utils/DspMath.h"

namespace aetherfall
{

namespace
{
    // Meter scale: -60..+6 dBFS mapped to 0..1.
    float toPos (float linear) { return dsp::clamp ((dsp::gainToDb (linear) + 60.0f) / 66.0f, 0.0f, 1.0f); }
}

LevelMeterView::LevelMeterView (const std::atomic<float>& peakL, const std::atomic<float>& peakR,
                                const std::atomic<float>& rmsLIn, const std::atomic<float>& rmsRIn, juce::String c)
    : pl (peakL), pr (peakR), rl (rmsLIn), rr (rmsRIn), caption (std::move (c))
{
    startTimerHz (30);
}

LevelMeterView::~LevelMeterView() { stopTimer(); }

void LevelMeterView::visibilityChanged()
{
    if (isShowing()) startTimerHz (30); else stopTimer();
}

void LevelMeterView::timerCallback()
{
    auto update = [] (float in, float& disp, float& hold, int& holdCount)
    {
        disp = std::max (in, disp * 0.85f);               // fast attack, ~smooth fall
        if (in >= hold) { hold = in; holdCount = 45; }    // 1.5 s peak hold
        else if (--holdCount <= 0) hold *= 0.9f;
    };
    update (pl.load(), dispL, holdL, holdCountL);
    update (pr.load(), dispR, holdR, holdCountR);
    rmsL = rl.load();
    rmsR = rr.load();
    repaint();
}

void LevelMeterView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto captionArea = r.removeFromBottom (14.0f);
    g.setColour (palette::textDim);
    g.setFont (AetherLookAndFeel::font (10.5f));
    g.drawText (caption, captionArea, juce::Justification::centred);

    const float gap = 3.0f;
    const float w = (r.getWidth() - gap) * 0.5f;
    auto drawBar = [&] (juce::Rectangle<float> bar, float peak, float hold, float rms)
    {
        g.setColour (palette::track);
        g.fillRoundedRectangle (bar, 2.0f);

        const float hP = bar.getHeight() * toPos (peak);
        const float hR = bar.getHeight() * toPos (rms);
        juce::ColourGradient grad (palette::good, bar.getX(), bar.getBottom(), palette::danger, bar.getX(), bar.getY(), false);
        grad.addColour (0.82, palette::warm);
        g.setGradientFill (grad);
        g.setOpacity (0.45f);
        g.fillRoundedRectangle (bar.withTop (bar.getBottom() - hP), 2.0f);
        g.setOpacity (1.0f);
        g.fillRoundedRectangle (bar.withTop (bar.getBottom() - hR), 2.0f);

        const float yh = bar.getBottom() - bar.getHeight() * toPos (hold);
        g.setColour (hold > 1.0f ? palette::danger : palette::text);
        g.fillRect (bar.getX(), yh - 1.0f, bar.getWidth(), 2.0f);
    };
    drawBar (r.withWidth (w), dispL, holdL, rmsL);
    drawBar (r.withTrimmedLeft (w + gap), dispR, holdR, rmsR);

    // 0 dB tick.
    const float y0 = r.getBottom() - r.getHeight() * toPos (1.0f);
    g.setColour (palette::textDim.withAlpha (0.6f));
    g.drawHorizontalLine ((int) y0, r.getX() - 2.0f, r.getRight() + 2.0f);
}

//==============================================================================
GainReductionView::GainReductionView (const std::atomic<float>& grDb, juce::String c)
    : gr (grDb), caption (std::move (c))
{
    startTimerHz (30);
}

GainReductionView::~GainReductionView() { stopTimer(); }

void GainReductionView::visibilityChanged()
{
    if (isShowing()) startTimerHz (30); else stopTimer();
}

void GainReductionView::timerCallback()
{
    const float v = std::min (0.0f, gr.load());
    disp = v < disp ? v : disp + (v - disp) * 0.25f;
    repaint();
}

void GainReductionView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (palette::textDim);
    g.setFont (AetherLookAndFeel::font (10.5f));
    g.drawText (caption + "  " + juce::String (disp, 1) + " dB", r.removeFromTop (13.0f), juce::Justification::centredLeft);

    auto bar = r.reduced (0.0f, 2.0f);
    g.setColour (palette::track);
    g.fillRoundedRectangle (bar, 2.0f);
    const float frac = dsp::clamp (-disp / 24.0f, 0.0f, 1.0f);
    g.setColour (palette::warm);
    g.fillRoundedRectangle (bar.withLeft (bar.getRight() - bar.getWidth() * frac), 2.0f);
}

//==============================================================================
CorrelationView::CorrelationView (const std::atomic<float>& c) : corr (c) { startTimerHz (30); }
CorrelationView::~CorrelationView() { stopTimer(); }

void CorrelationView::visibilityChanged()
{
    if (isShowing()) startTimerHz (30); else stopTimer();
}

void CorrelationView::timerCallback()
{
    disp += (corr.load() - disp) * 0.2f;
    repaint();
}

void CorrelationView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (palette::textDim);
    g.setFont (AetherLookAndFeel::font (10.5f));
    g.drawText ("Correlation  " + juce::String (disp, 2), r.removeFromTop (13.0f), juce::Justification::centredLeft);

    auto bar = r.reduced (0.0f, 2.0f);
    g.setColour (palette::track);
    g.fillRoundedRectangle (bar, 2.0f);
    const float cx = bar.getCentreX();
    const float x = cx + disp * bar.getWidth() * 0.5f;
    g.setColour (disp < 0.0f ? palette::danger : palette::good);
    g.fillRect (juce::Rectangle<float> (std::min (cx, x), bar.getY(), std::abs (x - cx), bar.getHeight()));
    g.setColour (palette::textDim);
    g.drawVerticalLine ((int) cx, bar.getY(), bar.getBottom());
}

} // namespace aetherfall
