#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

namespace aetherfall
{

class AetherfallProcessor;

/** Placeholder editor until the real GUI lands (phase F7). It shows the
    product header and a generic parameter editor for testing in Standalone. */
class AetherfallEditor final : public juce::AudioProcessorEditor
{
public:
    explicit AetherfallEditor (AetherfallProcessor&);
    ~AetherfallEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AetherfallProcessor& processor;
    juce::GenericAudioProcessorEditor generic;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AetherfallEditor)
};

} // namespace aetherfall
