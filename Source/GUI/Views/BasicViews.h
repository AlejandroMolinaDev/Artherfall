#pragma once

#include <memory>
#include <vector>
#include "GUI/Components/ParamWidgets.h"
#include "GUI/Components/Meters.h"
#include "GUI/Visualizer/AtmosphereView.h"

namespace aetherfall
{

class AetherfallProcessor;

/** Helper base: owns sections and parameter widgets. */
class ParamView : public juce::Component
{
protected:
    explicit ParamView (juce::AudioProcessorValueTreeState& s) : apvts (s) {}

    Section& addSection (const juce::String& title);
    ParamWidget& addWidget (Section& parent, PID pid, Knob::Size size = Knob::Size::Normal, const juce::String& label = {});
    std::vector<juce::Component*> widgetsOf (const Section& s) const;

    juce::AudioProcessorValueTreeState& apvts;
    std::vector<std::unique_ptr<Section>> sections;
    std::vector<std::pair<Section*, std::unique_ptr<ParamWidget>>> widgets;
};

/** MAIN: macros, visualiser, global controls, dry/wet, width, ducking, output, meters. */
class MainView final : public ParamView
{
public:
    MainView (AetherfallProcessor&, juce::AudioProcessorValueTreeState&);
    void resized() override;

private:
    Section *globalSec = nullptr, *macroSec = nullptr, *outSec = nullptr;
    AtmosphereView atmosphere;
    LevelMeterView inMeter, outMeter;
};

/** CORE: every ATMOS CORE parameter. */
class CoreView final : public ParamView
{
public:
    explicit CoreView (juce::AudioProcessorValueTreeState&);
    void resized() override;
};

/** OUTPUT: input stage, output EQ, glue, ducker, clipper/limiter and meters. */
class OutputView final : public ParamView
{
public:
    OutputView (AetherfallProcessor&, juce::AudioProcessorValueTreeState&);
    void resized() override;

private:
    LevelMeterView inMeter, outMeter;
    GainReductionView glueGr, duckGr, limitGr;
    CorrelationView correlation;
    Section* meterSec = nullptr;
};

} // namespace aetherfall
