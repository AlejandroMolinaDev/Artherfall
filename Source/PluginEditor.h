#pragma once

#include <array>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

namespace aetherfall
{

class AetherfallProcessor;

/** Development editor until the real GUI lands (phase F7):
    product header, 8 chain-slot selectors and the generic parameter list. */
class AetherfallEditor final : public juce::AudioProcessorEditor,
                               private juce::Timer
{
public:
    explicit AetherfallEditor (AetherfallProcessor&);
    ~AetherfallEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void slotChanged();
    void refreshSlotsFromProcessor();

    AetherfallProcessor& processor;
    std::array<juce::ComboBox, 8> slots;
    juce::GenericAudioProcessorEditor generic;
    bool updatingSlots = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AetherfallEditor)
};

} // namespace aetherfall
