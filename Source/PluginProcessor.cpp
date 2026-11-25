#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PluginInfo.h"
#include "DSP/Modules/TranceGate.h"

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

    getChainNode();   // make sure the state nodes exist in a fresh state
    getGateNode();
    apvts.state.addListener (this);
    pushAllStateToAudio();
}

AetherfallProcessor::~AetherfallProcessor()
{
    apvts.state.removeListener (this);
}

//==============================================================================
juce::ValueTree AetherfallProcessor::getChainNode()
{
    auto node = apvts.state.getChildWithName (chainNodeId);
    if (! node.isValid())
    {
        node = juce::ValueTree (chainNodeId);
        node.setProperty (chainOrderProp, juce::String(), nullptr);
        apvts.state.appendChild (node, nullptr);
    }
    return node;
}

juce::ValueTree AetherfallProcessor::getGateNode()
{
    auto node = apvts.state.getChildWithName (gateNodeId);
    if (! node.isValid())
    {
        node = juce::ValueTree (gateNodeId);
        node.setProperty (gatePatternProp, (int) TranceGate::defaultPattern, nullptr);
        apvts.state.appendChild (node, nullptr);
    }
    return node;
}

void AetherfallProcessor::setGatePattern (std::uint32_t pattern)
{
    getGateNode().setProperty (gatePatternProp, (int) (pattern & 0xFFFFu), nullptr);
    pushGatePatternToAudio();
}

std::uint32_t AetherfallProcessor::getGatePattern() const
{
    const auto node = apvts.state.getChildWithName (gateNodeId);
    if (! node.isValid() || ! node.hasProperty (gatePatternProp))
        return TranceGate::defaultPattern;
    return (std::uint32_t) (int) node.getProperty (gatePatternProp) & 0xFFFFu;
}

void AetherfallProcessor::pushGatePatternToAudio()
{
    gatePatternBits.store (getGatePattern(), std::memory_order_release);
}

void AetherfallProcessor::pushAllStateToAudio()
{
    pushChainOrderToAudio();
    pushGatePatternToAudio();
}

void AetherfallProcessor::setChainOrder (const chain::Order& order)
{
    const auto text = chain::toString (chain::sanitize (order));
    getChainNode().setProperty (chainOrderProp, juce::String (text), nullptr);
    pushChainOrderToAudio();
}

chain::Order AetherfallProcessor::getChainOrder() const
{
    const auto node = apvts.state.getChildWithName (chainNodeId);
    if (! node.isValid())
        return chain::emptyOrder();
    return chain::fromString (node.getProperty (chainOrderProp).toString().toStdString());
}

void AetherfallProcessor::pushChainOrderToAudio()
{
    chainOrderPacked.store (chain::pack (getChainOrder()), std::memory_order_release);
}

void AetherfallProcessor::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& prop)
{
    if (tree.hasType (chainNodeId) && prop == chainOrderProp)
        pushChainOrderToAudio();
    else if (tree.hasType (gateNodeId) && prop == gatePatternProp)
        pushGatePatternToAudio();
}

void AetherfallProcessor::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& child)
{
    if (child.hasType (chainNodeId) || child.hasType (gateNodeId))
        pushAllStateToAudio();
}

void AetherfallProcessor::valueTreeRedirected (juce::ValueTree&)
{
    pushAllStateToAudio();
}

const juce::String AetherfallProcessor::getName() const { return product::name; }

double AetherfallProcessor::getTailLengthSeconds() const
{
    return chain.getTailSeconds() + AtmosCore::estimateTailSeconds (params.load (PID::CoreDecay),
                                           (int) std::lround (params.load (PID::CoreColor)),
                                           params.load (PID::CorePreDelay),
                                           params.load (PID::CoreFreeze) >= 0.5f);
}

//==============================================================================
void AetherfallProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    maxBlockSize = std::max (1, samplesPerBlock);

    params.refresh();
    std::copy (params.data(), params.data() + numParams, effective.begin());

    // All stages work on control-rate chunks, so their max block is fixed.
    chain.prepare (currentSampleRate, controlBlockSize);
    core.prepare (currentSampleRate, controlBlockSize, 2);

    for (auto* sm : { &outDryGain, &outWetGain, &outGain, &bypassMix })
        sm->prepare (currentSampleRate, 0.03);

    for (auto& d : dryDelay)
        d.prepare (FXChain::maxModuleLatency * 2 + controlBlockSize);
    dryDelayFade.prepare (currentSampleRate, 0.02);

    reset();
}

void AetherfallProcessor::updateLatency (int newLatency)
{
    if (newLatency == dryDelayCurrent)
        return;

    // Crossfade the dry path from the old delay to the new one.
    dryDelayPrevious = dryDelayCurrent;
    dryDelayCurrent = newLatency;
    dryDelayFade.setCurrentAndTarget (0.0f);
    dryDelayFade.setTarget (1.0f);

    if (newLatency != reportedLatency)
    {
        reportedLatency = newLatency;
        setLatencySamples (newLatency);
    }
}

void AetherfallProcessor::releaseResources() {}

void AetherfallProcessor::reset()
{
    chain.setRequestedOrder (chainOrderPacked.load (std::memory_order_acquire));
    chain.reset();
    core.reset();

    for (auto& d : dryDelay) d.reset();
    dryDelayCurrent = dryDelayPrevious = chain.getLatencySamples();
    dryDelayFade.setCurrentAndTarget (1.0f);
    reportedLatency = dryDelayCurrent;
    setLatencySamples (reportedLatency);

    const float mix = params.load (PID::OutMix);
    outDryGain.setCurrentAndTarget (std::cos (mix * juce::MathConstants<float>::halfPi));
    outWetGain.setCurrentAndTarget (std::sin (mix * juce::MathConstants<float>::halfPi));
    outGain.setCurrentAndTarget (juce::Decibels::decibelsToGain (params.load (PID::OutGain)));
    bypassMix.setCurrentAndTarget (params.load (PID::Bypass) >= 0.5f ? 1.0f : 0.0f);
}

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
    ctx.gatePattern = gatePatternBits.load (std::memory_order_acquire);
    if (ctx.transport.hasPpq)
        ctx.transport.ppqPosition += (double) start / currentSampleRate * (ctx.transport.bpm / 60.0);

    // 4) Keep the dry signal for the dry/wet and bypass crossfades, delayed by
    //    the chain latency so dry and wet stay aligned.
    for (int ch = 0; ch < 2; ++ch)
    {
        const float* src = io.getReadPointer (ch, start);
        float* dst = dryChunk[(size_t) ch].data();
        auto& dl = dryDelay[(size_t) ch];
        for (int i = 0; i < length; ++i)
            dl.write (src[i]);
        for (int i = 0; i < length; ++i)
        {
            const int back = length - 1 - i;   // samples written after sample i
            dst[i] = dl.tap (dryDelayCurrent + back);
        }
        if (dryDelayFade.isSmoothing())
        {
            auto fade = dryDelayFade;   // same ramp for both channels
            for (int i = 0; i < length; ++i)
            {
                const float g = fade.next();
                const float old = dl.tap (dryDelayPrevious + length - 1 - i);
                dst[i] = old + g * (dst[i] - old);
            }
        }
    }
    dryDelayFade.skip (length);

    // A view of the chunk (2 channels: no heap allocation).
    juce::AudioBuffer<float> chunk (io.getArrayOfWritePointers(), 2, start, length);

    // 5) DSP stages: [input stage F6] -> FX chain -> ATMOS CORE -> output.
    chain.setRequestedOrder (chainOrderPacked.load (std::memory_order_acquire));
    chain.process (chunk, ctx);
    core.process (chunk, ctx);

    // The chain latency may change when its order changes (Spectral added or
    // removed). Applied from the next chunk on.
    updateLatency (chain.getLatencySamples());

    // 6) Provisional output stage: dry/wet, output gain, bypass.
    const float mix = ctx.get (PID::OutMix);
    outDryGain.setTarget (std::cos (mix * juce::MathConstants<float>::halfPi));
    outWetGain.setTarget (std::sin (mix * juce::MathConstants<float>::halfPi));
    outGain.setTarget (juce::Decibels::decibelsToGain (ctx.get (PID::OutGain)));
    bypassMix.setTarget (ctx.getBool (PID::Bypass) ? 1.0f : 0.0f);

    float* L = chunk.getWritePointer (0);
    float* R = chunk.getWritePointer (1);
    const float* dL = dryChunk[0].data();
    const float* dR = dryChunk[1].data();

    for (int i = 0; i < length; ++i)
    {
        const float dg = outDryGain.next(), wg = outWetGain.next();
        const float og = outGain.next(), bp = bypassMix.next();

        const float l = (dg * dL[i] + wg * L[i]) * og;
        const float r = (dg * dR[i] + wg * R[i]) * og;
        L[i] = l + bp * (dL[i] - l);
        R[i] = r + bp * (dR[i] - r);
    }
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
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            getChainNode();
            getGateNode();
            pushAllStateToAudio();
        }
}

} // namespace aetherfall

// This creates new instances of the plug-in.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new aetherfall::AetherfallProcessor();
}
