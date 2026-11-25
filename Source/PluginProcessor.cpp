#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PluginInfo.h"

namespace aetherfall
{

static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "out_mix", 1 },
                                                             "Dry/Wet",
                                                             juce::NormalisableRange<float> (0.0f, 1.0f),
                                                             0.35f));
    return layout;
}

AetherfallProcessor::AetherfallProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AETHERFALL", createLayout())
{
}

AetherfallProcessor::~AetherfallProcessor() = default;

const juce::String AetherfallProcessor::getName() const { return info::name; }

double AetherfallProcessor::getTailLengthSeconds() const { return 0.0; }

void AetherfallProcessor::prepareToPlay (double, int) {}
void AetherfallProcessor::releaseResources() {}
void AetherfallProcessor::reset() {}

bool AetherfallProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Supported: mono -> stereo and stereo -> stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void AetherfallProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();

    // Mono input: duplicate to the right channel.
    if (numIn == 1 && numOut > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());

    for (int ch = std::max (numIn, 1); ch < numOut; ++ch)
        if (! (numIn == 1 && ch == 1))
            buffer.clear (ch, 0, buffer.getNumSamples());
}

juce::AudioProcessorEditor* AetherfallProcessor::createEditor()
{
    return new AetherfallEditor (*this);
}

void AetherfallProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", info::stateVersion, nullptr);
    state.setProperty ("pluginVersion", juce::String (info::version), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AetherfallProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

} // namespace aetherfall

// This creates new instances of the plug-in.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new aetherfall::AetherfallProcessor();
}
