#include "BasicViews.h"
#include "PluginProcessor.h"

namespace aetherfall
{

Section& ParamView::addSection (const juce::String& title)
{
    sections.push_back (std::make_unique<Section> (title));
    addAndMakeVisible (*sections.back());
    return *sections.back();
}

ParamWidget& ParamView::addWidget (Section& parent, PID pid, Knob::Size size, const juce::String& label)
{
    widgets.emplace_back (&parent, createParamWidget (apvts, pid, size, label));
    parent.addAndMakeVisible (*widgets.back().second);
    return *widgets.back().second;
}

std::vector<juce::Component*> ParamView::widgetsOf (const Section& s) const
{
    std::vector<juce::Component*> out;
    for (const auto& [sec, w] : widgets)
        if (sec == &s)
            out.push_back (w.get());
    return out;
}

//==============================================================================
MainView::MainView (AetherfallProcessor& p, juce::AudioProcessorValueTreeState& s)
    : ParamView (s),
      atmosphere (p, p.getMeters()),
      inMeter (p.getMeters().inPeakL, p.getMeters().inPeakR, p.getMeters().inRmsL, p.getMeters().inRmsR, "IN"),
      outMeter (p.getMeters().outPeakL, p.getMeters().outPeakR, p.getMeters().outRmsL, p.getMeters().outRmsR, "OUT")
{
    globalSec = &addSection ("Global");
    for (auto pid : { PID::GlobKey, PID::GlobScale, PID::CoreColor, PID::GlobQuality })
        addWidget (*globalSec, pid, Knob::Size::Normal, pid == PID::CoreColor ? "Color" : juce::String());
    addWidget (*globalSec, PID::CoreFreeze, Knob::Size::Normal, "Freeze");
    addWidget (*globalSec, PID::CoreDecay, Knob::Size::Small, "Decay");
    addWidget (*globalSec, PID::CoreSize, Knob::Size::Small, "Size");
    addWidget (*globalSec, PID::GlobModDepth, Knob::Size::Small, "Mod Depth");
    addWidget (*globalSec, PID::CorePreDelay, Knob::Size::Small, "PreDelay");

    macroSec = &addSection ({});
    for (auto pid : { PID::MacroHaze, PID::MacroDrift, PID::MacroGrit, PID::MacroBloom })
        addWidget (*macroSec, pid, Knob::Size::Macro);

    outSec = &addSection ("Mix");
    addWidget (*outSec, PID::OutMix);
    addWidget (*outSec, PID::OutWidth);
    addWidget (*outSec, PID::OutDuck);
    addWidget (*outSec, PID::OutGain);

    addAndMakeVisible (atmosphere);
    outSec->addAndMakeVisible (inMeter);
    outSec->addAndMakeVisible (outMeter);
}

void MainView::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto left = r.removeFromLeft (190);
    auto right = r.removeFromRight (210);
    r.reduce (12, 0);

    globalSec->setBounds (left);
    {
        auto c = globalSec->getContentBounds();
        auto items = widgetsOf (*globalSec);
        // Four choices stacked, then the freeze toggle and the mod depth knob.
        for (size_t i = 0; i < 4; ++i)
            items[i]->setBounds (c.removeFromTop (46).reduced (2, 2));
        items[4]->setBounds (c.removeFromTop (34).reduced (2, 4));
        c.removeFromTop (6);
        layoutGrid ({ items[5], items[6], items[7], items[8] }, c.removeFromTop (180), 2, 90);
    }

    outSec->setBounds (right);
    {
        auto c = outSec->getContentBounds();
        auto knobs = widgetsOf (*outSec);
        layoutGrid (knobs, c.removeFromTop (236), 2, 118);
        c.removeFromTop (8);
        auto meters = c.reduced (24, 4);
        inMeter.setBounds (meters.removeFromLeft (meters.getWidth() / 2).reduced (8, 0));
        outMeter.setBounds (meters.reduced (8, 0));
    }

    auto macroArea = r.removeFromBottom (190);
    macroSec->setBounds (macroArea);
    layoutGrid (widgetsOf (*macroSec), macroSec->getContentBounds().reduced (4, 6), 4, macroSec->getContentBounds().getHeight() - 12);

    r.removeFromBottom (12);
    atmosphere.setBounds (r);
}

//==============================================================================
CoreView::CoreView (juce::AudioProcessorValueTreeState& s) : ParamView (s)
{
    auto& space = addSection ("Space");
    for (auto pid : { PID::CoreSize, PID::CoreDecay, PID::CorePreDelay, PID::CoreFreeze, PID::CorePreSync, PID::CorePreDiv })
        addWidget (space, pid);

    auto& diff = addSection ("Diffusion");
    for (auto pid : { PID::CoreDiffEarly, PID::CoreDiffLate })
        addWidget (diff, pid);

    auto& modu = addSection ("Modulation");
    for (auto pid : { PID::CoreModDepth, PID::CoreModRate })
        addWidget (modu, pid);

    auto& damp = addSection ("Damping");
    for (auto pid : { PID::CoreDampHF, PID::CoreDampLF })
        addWidget (damp, pid);

    auto& shim = addSection ("Shimmer");
    for (auto pid : { PID::CoreShimmer, PID::CoreShimBlend, PID::CoreShimA, PID::CoreShimB })
        addWidget (shim, pid, Knob::Size::Normal,
                   pid == PID::CoreShimmer ? "Amount" : (pid == PID::CoreShimBlend ? "Blend" : juce::String()));

    auto& chr = addSection ("Character");
    for (auto pid : { PID::CoreBloom, PID::CoreWidth, PID::CoreMix, PID::CoreColor })
        addWidget (chr, pid);
}

void CoreView::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto top = r.removeFromTop (r.getHeight() / 2).withTrimmedBottom (6);
    auto bottom = r.withTrimmedTop (6);

    // Top row: Space (wide), Diffusion, Modulation.
    sections[0]->setBounds (top.removeFromLeft (top.getWidth() / 2).withTrimmedRight (6));
    sections[1]->setBounds (top.removeFromLeft (top.getWidth() / 2).withTrimmedRight (6));
    sections[2]->setBounds (top);

    // Bottom row: Damping, Shimmer (wide), Character (wide).
    sections[3]->setBounds (bottom.removeFromLeft (bottom.getWidth() / 5).withTrimmedRight (6));
    sections[4]->setBounds (bottom.removeFromLeft (bottom.getWidth() / 2).withTrimmedRight (6));
    sections[5]->setBounds (bottom);

    auto layoutSection = [this] (size_t idx, int cols)
    {
        auto& sec = *sections[idx];
        auto items = widgetsOf (sec);
        const auto c = sec.getContentBounds();
        const int rows = ((int) items.size() + cols - 1) / cols;
        layoutGrid (items, c, cols, c.getHeight() / std::max (1, rows));
    };

    // Space: knobs on top row, sync controls below.
    {
        auto& sec = *sections[0];
        auto items = widgetsOf (sec);
        auto c = sec.getContentBounds();
        auto knobRow = c.removeFromTop (c.getHeight() * 2 / 3);
        layoutGrid ({ items[0], items[1], items[2] }, knobRow, 3, knobRow.getHeight());
        layoutGrid ({ items[3], items[4], items[5] }, c.reduced (0, 4), 3, c.getHeight() - 8);
    }
    layoutSection (1, 1);
    layoutSection (2, 1);
    layoutSection (3, 1);
    {
        auto& sec = *sections[4];
        auto items = widgetsOf (sec);
        auto c = sec.getContentBounds();
        auto knobRow = c.removeFromTop (c.getHeight() * 2 / 3);
        layoutGrid ({ items[0], items[1] }, knobRow, 2, knobRow.getHeight());
        layoutGrid ({ items[2], items[3] }, c.reduced (8, 4), 2, c.getHeight() - 8);
    }
    {
        auto& sec = *sections[5];
        auto items = widgetsOf (sec);
        auto c = sec.getContentBounds();
        auto knobRow = c.removeFromTop (c.getHeight() * 2 / 3);
        layoutGrid ({ items[0], items[1], items[2] }, knobRow, 3, knobRow.getHeight());
        items[3]->setBounds (c.reduced (c.getWidth() / 4, 4));
    }
}

//==============================================================================
OutputView::OutputView (AetherfallProcessor& p, juce::AudioProcessorValueTreeState& s)
    : ParamView (s),
      inMeter (p.getMeters().inPeakL, p.getMeters().inPeakR, p.getMeters().inRmsL, p.getMeters().inRmsR, "IN"),
      outMeter (p.getMeters().outPeakL, p.getMeters().outPeakR, p.getMeters().outRmsL, p.getMeters().outRmsR, "OUT"),
      glueGr (p.getMeters().glueGainReductionDb, "Glue GR"),
      duckGr (p.getMeters().duckGainReductionDb, "Duck GR"),
      limitGr (p.getMeters().limiterGainReductionDb, "Limiter GR"),
      correlation (p.getMeters().correlation)
{
    auto& in = addSection ("Input");
    for (auto pid : { PID::InGain, PID::InHPF, PID::InLPF, PID::InDrive, PID::InBassMono })
        addWidget (in, pid, Knob::Size::Normal, pid == PID::InBassMono ? "Bass Mono" : juce::String());

    auto& eq = addSection ("Output EQ");
    for (auto pid : { PID::OutTilt, PID::OutHPF, PID::OutLPF, PID::OutWidth })
        addWidget (eq, pid, Knob::Size::Normal, pid == PID::OutTilt ? "Tilt" : (pid == PID::OutHPF ? "HPF" : (pid == PID::OutLPF ? "LPF" : juce::String())));

    auto& glue = addSection ("Glue");
    for (auto pid : { PID::OutGlueThresh, PID::OutGlueRatio, PID::OutGlueAttack, PID::OutGlueRelease, PID::OutGlueMakeup, PID::OutGlueOn })
        addWidget (glue, pid, Knob::Size::Small, pid == PID::OutGlueOn ? "On" : juce::String());
    glue.addAndMakeVisible (glueGr);

    auto& duck = addSection ("Ducker");
    for (auto pid : { PID::OutDuck, PID::OutDuckThresh, PID::OutDuckAttack, PID::OutDuckRelease })
        addWidget (duck, pid, Knob::Size::Small, pid == PID::OutDuck ? "Amount" : juce::String());
    duck.addAndMakeVisible (duckGr);

    auto& lim = addSection ("Clip / Limit");
    for (auto pid : { PID::OutClipDrive, PID::OutCeiling, PID::OutClipOn, PID::OutLimitOn })
        addWidget (lim, pid, Knob::Size::Small, pid == PID::OutClipDrive ? "Clip Drive" : (pid == PID::OutClipOn ? "Clip" : (pid == PID::OutLimitOn ? "Limiter" : juce::String())));
    lim.addAndMakeVisible (limitGr);

    auto& out = addSection ("Output");
    for (auto pid : { PID::OutMix, PID::OutGain })
        addWidget (out, pid);

    meterSec = &addSection ("Meters");
    meterSec->addAndMakeVisible (inMeter);
    meterSec->addAndMakeVisible (outMeter);
    meterSec->addAndMakeVisible (correlation);
}

void OutputView::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto meters = r.removeFromRight (150);
    r.removeFromRight (12);

    auto top = r.removeFromTop (r.getHeight() / 2).withTrimmedBottom (6);
    auto bottom = r.withTrimmedTop (6);

    sections[0]->setBounds (top.removeFromLeft (top.getWidth() * 5 / 9).withTrimmedRight (6));   // Input
    sections[1]->setBounds (top);                                                                  // EQ
    sections[2]->setBounds (bottom.removeFromLeft (bottom.getWidth() * 3 / 8).withTrimmedRight (6)); // Glue
    sections[3]->setBounds (bottom.removeFromLeft (bottom.getWidth() * 2 / 5).withTrimmedRight (6)); // Ducker
    sections[4]->setBounds (bottom.removeFromLeft (bottom.getWidth() * 3 / 5).withTrimmedRight (6)); // Clip/limit
    sections[5]->setBounds (bottom);                                                                 // Output
    meterSec->setBounds (meters);

    auto rowLayout = [this] (size_t idx)
    {
        auto& sec = *sections[idx];
        auto items = widgetsOf (sec);
        layoutGrid (items, sec.getContentBounds(), (int) items.size(), sec.getContentBounds().getHeight());
    };
    rowLayout (0);
    rowLayout (1);

    auto withMeter = [this] (size_t idx, juce::Component& meter, int cols)
    {
        auto& sec = *sections[idx];
        auto items = widgetsOf (sec);
        auto c = sec.getContentBounds();
        meter.setBounds (c.removeFromBottom (30).reduced (4, 0));
        const int rows = ((int) items.size() + cols - 1) / cols;
        layoutGrid (items, c, cols, c.getHeight() / rows);
    };
    withMeter (2, glueGr, 3);
    withMeter (3, duckGr, 2);
    withMeter (4, limitGr, 2);
    {
        auto& sec = *sections[5];
        auto items = widgetsOf (sec);
        layoutGrid (items, sec.getContentBounds(), 1, sec.getContentBounds().getHeight() / 2);
    }

    auto c = meterSec->getContentBounds();
    correlation.setBounds (c.removeFromBottom (32));
    c.removeFromBottom (8);
    inMeter.setBounds (c.removeFromLeft (c.getWidth() / 2).reduced (10, 0));
    outMeter.setBounds (c.reduced (10, 0));
}

} // namespace aetherfall
