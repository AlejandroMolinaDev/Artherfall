#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>

namespace aetherfall
{

/** Vertical stereo level meter (peak bar with hold + RMS), polls atomics at 30 Hz. */
class LevelMeterView final : public juce::Component, private juce::Timer
{
public:
    LevelMeterView (const std::atomic<float>& peakL, const std::atomic<float>& peakR,
                    const std::atomic<float>& rmsL, const std::atomic<float>& rmsR, juce::String caption);
    ~LevelMeterView() override;
    void paint (juce::Graphics&) override;
    void visibilityChanged() override;

private:
    void timerCallback() override;
    const std::atomic<float> &pl, &pr, &rl, &rr;
    juce::String caption;
    float dispL = 0.0f, dispR = 0.0f, holdL = 0.0f, holdR = 0.0f, rmsL = 0.0f, rmsR = 0.0f;
    int holdCountL = 0, holdCountR = 0;
};

/** Horizontal gain-reduction meter (0 .. -24 dB). */
class GainReductionView final : public juce::Component, private juce::Timer
{
public:
    GainReductionView (const std::atomic<float>& grDb, juce::String caption);
    ~GainReductionView() override;
    void paint (juce::Graphics&) override;
    void visibilityChanged() override;

private:
    void timerCallback() override;
    const std::atomic<float>& gr;
    juce::String caption;
    float disp = 0.0f;
};

/** Stereo correlation meter (-1 .. +1). */
class CorrelationView final : public juce::Component, private juce::Timer
{
public:
    explicit CorrelationView (const std::atomic<float>& corr);
    ~CorrelationView() override;
    void paint (juce::Graphics&) override;
    void visibilityChanged() override;

private:
    void timerCallback() override;
    const std::atomic<float>& corr;
    float disp = 1.0f;
};

} // namespace aetherfall
