#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PluginInfo.h"

namespace aetherfall
{

AetherfallProcessor::AetherfallProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AETHERFALL", createParameterLayout()),
      params (apvts)
{
    bypassParam = apvts.getParameter (juce::String (std::string (info (PID::Bypass).id)));
    jassert (bypassParam != nullptr);
}

AetherfallProcessor::~AetherfallProcessor() = default;

const juce::String AetherfallProcessor::getName() const { return product::name; }

double AetherfallProcessor::getTailLengthSeconds() const
{
    // Updated with the real ATMOS CORE decay once it exists (F2).
    return 0.0;
}

//==============================================================================
void AetherfallProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    maxBlockSize = std::max (1, samplesPerBlock);

    params.refresh();
    std::copy (params.data(), params.data() + numParams, effective.begin());
}

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

//==============================================================================
TransportState AetherfallProcessor::readTransport() const
{
    TransportState t;

    if (auto* ph = getPlayHead())
    {
        if (const auto pos = ph->getPosition())
        {
            if (const auto bpm = pos->getBpm())
                t.bpm = juce::jlimit (20.0, 999.0, *bpm);

            if (const auto ppq = pos->getPpqPosition())
            {
                t.ppqPosition = *ppq;
                t.hasPpq = true;
            }

            if (const auto sig = pos->getTimeSignature())
            {
                t.timeSigNumerator   = std::max (1, sig->numerator);
                t.timeSigDenominator = std::max (1, sig->denominator);
            }

            t.isPlaying = pos->getIsPlaying();
        }
    }

    return t;
}

void AetherfallProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();

    if (numSamples <= 0 || numOut < 2)
        return;

    // Internal processing is always stereo: duplicate a mono input.
    if (numIn == 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);
    else if (numIn == 0)
        buffer.clear();

    const auto transport = readTransport();

    for (int start = 0; start < numSamples; start += controlBlockSize)
    {
        const int len = std::min (controlBlockSize, numSamples - start);
        processChunk (buffer, start, len, transport);
    }
}

void AetherfallProcessor::processChunk (juce::AudioBuffer<float>& io, int start, int length,
                                        const TransportState& blockTransport)
{
    juce::ignoreUnused (io);

    // 1) Control rate: snapshot host parameter values.
    params.refresh();

    // 2) Modulation (F5) will write offsets on top of the host values here.
    std::copy (params.data(), params.data() + numParams, effective.begin());

    // 3) Build the context for this chunk (transport advanced to chunk start).
    ModContext ctx;
    ctx.values = effective.data();
    ctx.sampleRate = currentSampleRate;
    ctx.numSamples = length;
    ctx.transport = blockTransport;
    if (ctx.transport.hasPpq)
        ctx.transport.ppqPosition += (double) start / currentSampleRate * (ctx.transport.bpm / 60.0);

    // 4) DSP stages (F2+): input stage -> chain -> ATMOS CORE -> output stage.
    //    Currently a clean passthrough.
}

//==============================================================================
juce::AudioProcessorEditor* AetherfallProcessor::createEditor()
{
    return new AetherfallEditor (*this);
}

void AetherfallProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", product::stateVersion, nullptr);
    state.setProperty ("pluginVersion", juce::String (product::version), nullptr);

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
