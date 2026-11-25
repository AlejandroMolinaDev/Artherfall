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
    modstate::ensureNodes (apvts.state);
    apvts.state.addListener (this);
    pushAllStateToAudio();

    for (auto* p : getParameters())
        p->addListener (this);
}

AetherfallProcessor::~AetherfallProcessor()
{
    for (auto* p : getParameters())
        p->removeListener (this);
    apvts.state.removeListener (this);
}

//==============================================================================
void AetherfallProcessor::checkpoint()
{
    if (restoringState || ! juce::MessageManager::existsAndIsCurrentThread())
        return;
    history.checkpoint (apvts.copyState());
}

void AetherfallProcessor::parameterGestureChanged (int, bool starting)
{
    if (! juce::MessageManager::existsAndIsCurrentThread())
        return;
    if (starting)
    {
        if (gestureDepth++ == 0)
            checkpoint();
    }
    else
    {
        gestureDepth = std::max (0, gestureDepth - 1);
    }
}

void AetherfallProcessor::restoreState (const juce::ValueTree& state)
{
    if (! state.isValid())
        return;
    const juce::ScopedValueSetter<bool> svs (restoringState, true);
    apvts.replaceState (state.createCopy());
    getChainNode();
    getGateNode();
    modstate::ensureNodes (apvts.state);
    pushAllStateToAudio();
}

bool AetherfallProcessor::undo()
{
    const auto s = history.undo (apvts.copyState());
    restoreState (s);
    return s.isValid();
}

bool AetherfallProcessor::redo()
{
    const auto s = history.redo (apvts.copyState());
    restoreState (s);
    return s.isValid();
}

void AetherfallProcessor::toggleAB()
{
    checkpoint();
    abStates[(size_t) abIndex] = apvts.copyState();
    abIndex = 1 - abIndex;
    if (abStates[(size_t) abIndex].isValid())
        restoreState (abStates[(size_t) abIndex]);
    else
        abStates[(size_t) abIndex] = apvts.copyState();   // first switch: B starts as a copy of A
}

void AetherfallProcessor::copyCurrentToOtherAB()
{
    abStates[(size_t) (1 - abIndex)] = apvts.copyState();
}

int AetherfallProcessor::assignModulation (ModSource source, PID dest, float amount)
{
    if (source == ModSource::None || ! info (dest).isModulatable())
        return -1;

    const auto routing = getModRouting();
    for (int s = 0; s < numModSlots; ++s)
    {
        if (routing.slots[(size_t) s].source != ModSource::None)
            continue;

        checkpoint();
        {
            const juce::ScopedValueSetter<bool> svs (restoringState, true);   // single undo step
            setModSlot (s, { source, (std::int16_t) dest, ModCurve::Linear, ModSource::None });
            if (auto* p = apvts.getParameter (juce::String (std::string (info (modAmountParam (s)).id))))
                p->setValueNotifyingHost (p->convertTo0to1 (amount));
        }
        return s;
    }
    return -1;
}

void AetherfallProcessor::removeModulation (PID dest)
{
    const auto routing = getModRouting();
    bool first = true;
    for (int s = 0; s < numModSlots; ++s)
        if (routing.slots[(size_t) s].dest == (std::int16_t) dest)
        {
            if (first) { checkpoint(); first = false; }
            const juce::ScopedValueSetter<bool> svs (restoringState, true);
            setModSlot (s, {});
        }
}

void AetherfallProcessor::randomize (RandomSection section, float intensity, std::uint32_t seed)
{
    intensity = juce::jlimit (0.0f, 1.0f, intensity);
    juce::Random rng ((juce::int64) seed);
    std::vector<PID> targets;

    auto addRange = [&targets] (PID first, PID last)
    {
        for (int p = (int) first; p <= (int) last; ++p)
            targets.push_back ((PID) p);
    };

    switch (section)
    {
        case RandomSection::Core:
            addRange (PID::CoreSize, PID::CoreWidth);
            break;
        case RandomSection::Chain:
            for (auto t : getChainOrder())
                if (t != ModuleType::None)
                    addRange (moduleInfo (t).firstParam, moduleInfo (t).lastParam);
            break;
        case RandomSection::Modulation:
            addRange (PID::Lfo1Shape, PID::Chaos2Smooth);
            break;
        case RandomSection::Output:
            addRange (PID::OutTilt, PID::OutDuckRelease);
            break;
    }

    // Never randomise things that could surprise badly (freeze/hold switches,
    // safety stages, levels).
    const PID excluded[] = { PID::CoreFreeze, PID::GranFreeze, PID::SpecFreeze, PID::DroneHold,
                             PID::OutGlueOn, PID::OutGlueMakeup };

    checkpoint();
    const juce::ScopedValueSetter<bool> svs (restoringState, true);
    for (auto pid : targets)
    {
        if (std::find (std::begin (excluded), std::end (excluded), pid) != std::end (excluded))
            continue;
        auto* p = apvts.getParameter (juce::String (std::string (info (pid).id)));
        if (p == nullptr)
            continue;

        const float current = p->getValue();
        float next = current;
        if (info (pid).kind == Kind::Float)
            next = current + intensity * (rng.nextFloat() - current);
        else if (rng.nextFloat() < intensity * 0.5f)
            next = rng.nextFloat();
        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, next));
    }
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
    checkpoint();
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

void AetherfallProcessor::pushModRoutingToAudio()
{
    const auto routing = modstate::read (apvts.state);
    const juce::SpinLock::ScopedLockType lock (routingWriteLock);
    modEngine.setRouting (routing);
}

void AetherfallProcessor::setModSlot (int slot, const ModSlot& s, juce::UndoManager* um)
{
    checkpoint();
    modstate::writeSlot (apvts.state, slot, s, um);
    pushModRoutingToAudio();
}

void AetherfallProcessor::setMacroTarget (int macro, int index, const MacroTarget& t, juce::UndoManager* um)
{
    checkpoint();
    modstate::writeMacroTarget (apvts.state, macro, index, t, um);
    pushModRoutingToAudio();
}

void AetherfallProcessor::setSeqSteps (int seq, const std::array<float, (size_t) seqMaxSteps>& steps, juce::UndoManager* um)
{
    checkpoint();
    modstate::writeSeqSteps (apvts.state, seq, steps, um);
    pushModRoutingToAudio();
}

void AetherfallProcessor::setLfoShape (int lfo, const std::array<float, (size_t) lfoCustomPoints>& points, juce::UndoManager* um)
{
    checkpoint();
    modstate::writeLfoShape (apvts.state, lfo, points, um);
    pushModRoutingToAudio();
}

void AetherfallProcessor::pushAllStateToAudio()
{
    pushChainOrderToAudio();
    pushGatePatternToAudio();
    pushModRoutingToAudio();
}

void AetherfallProcessor::setChainOrder (const chain::Order& order)
{
    checkpoint();
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
    else if (modstate::isModulationNode (tree))
        pushModRoutingToAudio();
}

void AetherfallProcessor::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& child)
{
    if (child.hasType (chainNodeId) || child.hasType (gateNodeId) || modstate::isModulationNode (child))
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
    modEngine.prepare (currentSampleRate);
    chain.prepare (currentSampleRate, controlBlockSize);
    core.prepare (currentSampleRate, controlBlockSize, 2);
    inputStage.prepare (currentSampleRate, controlBlockSize, 2);
    outputStage.prepare (currentSampleRate, controlBlockSize, 2);
    inMeter.prepare (currentSampleRate);

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
    modEngine.reset();
    lastWetLength = 0;

    for (auto& d : dryDelay) d.reset();
    dryDelayCurrent = dryDelayPrevious = chain.getLatencySamples();
    dryDelayFade.setCurrentAndTarget (1.0f);
    reportedLatency = dryDelayCurrent;
    setLatencySamples (reportedLatency);

    inputStage.reset();
    outputStage.reset();
    inMeter.reset();
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

void AetherfallProcessor::handleMidi (const juce::MidiBuffer& midi)
{
    // Block-accurate MIDI is plenty for modulation sources / LFO retrigger.
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
            modEngine.noteOn (msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff())
            modEngine.noteOff (msg.getNoteNumber());
        else if (msg.isController())
            modEngine.controller (msg.getControllerNumber(), (float) msg.getControllerValue() / 127.0f);
        else if (msg.isPitchWheel())
            modEngine.pitchBend ((float) (msg.getPitchWheelValue() - 8192) / 8192.0f);
    }
}

void AetherfallProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
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
    handleMidi (midi);

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

    // 2) Context for this chunk (transport advanced to the chunk start).
    ModContext ctx;
    ctx.values = effective.data();
    ctx.sampleRate = currentSampleRate;
    ctx.numSamples = length;
    ctx.transport = blockTransport;
    ctx.gatePattern = gatePatternBits.load (std::memory_order_acquire);
    if (ctx.transport.hasPpq)
        ctx.transport.ppqPosition += (double) start / currentSampleRate * (ctx.transport.bpm / 60.0);

    // 3) Modulation: host values + macros + matrix -> effective values.
    ModulationEngine::ChunkInput modIn;
    modIn.inL = io.getReadPointer (0, start);
    modIn.inR = io.getReadPointer (1, start);
    modIn.wetL = lastWetLength >= length ? lastWet[0].data() : nullptr;
    modIn.wetR = lastWetLength >= length ? lastWet[1].data() : nullptr;
    modIn.numSamples = length;
    modIn.sampleRate = currentSampleRate;
    modIn.transport = ctx.transport;
    modEngine.process (params.data(), effective.data(), modIn);
    ctx.analysis = modEngine.getAnalysis();

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

    // Input meters (raw input).
    {
        float pl = 0.0f, pr = 0.0f;
        inMeter.process (io.getReadPointer (0, start), io.getReadPointer (1, start), length, pl, pr);
        meters.inPeakL.store (pl, std::memory_order_relaxed);
        meters.inPeakR.store (pr, std::memory_order_relaxed);
        meters.inRmsL.store (inMeter.rmsL(), std::memory_order_relaxed);
        meters.inRmsR.store (inMeter.rmsR(), std::memory_order_relaxed);
    }

    // A view of the chunk (2 channels: no heap allocation).
    juce::AudioBuffer<float> chunk (io.getArrayOfWritePointers(), 2, start, length);

    // 5) Wet path: input stage -> FX chain -> ATMOS CORE.
    inputStage.process (chunk, ctx);
    chain.setRequestedOrder (chainOrderPacked.load (std::memory_order_acquire));
    chain.process (chunk, ctx);
    core.process (chunk, ctx);

    // Keep the wet signal for the "Wet" envelope followers of the next chunk.
    for (int ch = 0; ch < 2; ++ch)
        std::copy_n (chunk.getReadPointer (ch), length, lastWet[(size_t) ch].data());
    lastWetLength = length;

    // Feed the spectrum analyser (dropped silently when the GUI is not reading).
    {
        const auto scope = spectrumFifo.write (length);
        auto put = [this] (int startIndex, int count, int offset)
        {
            for (int i = 0; i < count; ++i)
                spectrumBuffer[(size_t) (startIndex + i)] = 0.5f * (lastWet[0][(size_t) (offset + i)] + lastWet[1][(size_t) (offset + i)]);
        };
        put (scope.startIndex1, scope.blockSize1, 0);
        put (scope.startIndex2, scope.blockSize2, scope.blockSize1);
    }

    // The chain latency may change when its order changes (Spectral added or
    // removed). Applied from the next chunk on.
    updateLatency (chain.getLatencySamples());

    // 6) Output stage: wet processing, ducking, dry/wet, clipper, gain,
    //    safety limiter, bypass and output meters.
    outputStage.process (chunk, dryChunk[0].data(), dryChunk[1].data(), ctx, &meters);
}

int AetherfallProcessor::readSpectrumSamples (float* dest, int max) noexcept
{
    const auto scope = spectrumFifo.read (std::min (max, spectrumFifo.getNumReady()));
    for (int i = 0; i < scope.blockSize1; ++i) dest[i] = spectrumBuffer[(size_t) (scope.startIndex1 + i)];
    for (int i = 0; i < scope.blockSize2; ++i) dest[scope.blockSize1 + i] = spectrumBuffer[(size_t) (scope.startIndex2 + i)];
    return scope.blockSize1 + scope.blockSize2;
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
            modstate::ensureNodes (apvts.state);
            pushAllStateToAudio();
        }
}

} // namespace aetherfall

// This creates new instances of the plug-in.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new aetherfall::AetherfallProcessor();
}
