#include <catch2/catch_test_macros.hpp>
#include "PluginProcessor.h"

using aetherfall::AetherfallProcessor;

TEST_CASE ("Processor passes audio in F0 (stereo)", "[processor]")
{
    AetherfallProcessor p;
    p.setPlayConfigDetails (2, 2, 48000.0, 256);
    p.prepareToPlay (48000.0, 256);

    juce::AudioBuffer<float> buffer (2, 256);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 256; ++i)
            buffer.setSample (ch, i, std::sin (0.01f * (float) i));

    juce::MidiBuffer midi;
    p.processBlock (buffer, midi);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 256; ++i)
            REQUIRE (std::isfinite (buffer.getSample (ch, i)));
}

TEST_CASE ("Bus layouts: mono->stereo and stereo->stereo only", "[processor]")
{
    AetherfallProcessor p;
    juce::AudioProcessor::BusesLayout l;

    l.inputBuses.add (juce::AudioChannelSet::stereo());
    l.outputBuses.add (juce::AudioChannelSet::stereo());
    REQUIRE (p.checkBusesLayoutSupported (l));

    l.inputBuses.getReference (0) = juce::AudioChannelSet::mono();
    REQUIRE (p.checkBusesLayoutSupported (l));

    l.outputBuses.getReference (0) = juce::AudioChannelSet::mono();
    REQUIRE_FALSE (p.checkBusesLayoutSupported (l));
}
