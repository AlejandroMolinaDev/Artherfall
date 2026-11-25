#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Parameters/ParamIDs.h"
#include "Parameters/ParamLayout.h"
#include "DSP/Chain/ModContext.h"
#include "DSP/Core/AtmosCore.h"
#include "DSP/Chain/FXChain.h"
#include "DSP/Chain/ChainOrder.h"
#include "DSP/Utils/Smoothers.h"

namespace aetherfall
{

class AetherfallProcessor final : public juce::AudioProcessor,
                                  private juce::ValueTree::Listener
{
public:
    AetherfallProcessor();
    ~AetherfallProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==========================================================================
    const juce::String getName() const override;
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    //==========================================================================
    int getNumPrograms() override                         { return 1; }
    int getCurrentProgram() override                      { return 0; }
    void setCurrentProgram (int) override                 {}
    const juce::String getProgramName (int) override      { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return apvts; }

    //==========================================================================
    // FX chain order (message thread). Stored in the CHAIN node of the state
    // tree, so it is saved with the project / presets and is undoable.
    void setChainOrder (const chain::Order& order);
    chain::Order getChainOrder() const;

    /** Order the audio thread is actually running (for tests / GUI feedback). */
    chain::Order getActiveChainOrder() const noexcept { return chain.getActiveOrder(); }

    static inline const juce::Identifier chainNodeId { "CHAIN" };
    static inline const juce::Identifier chainOrderProp { "order" };

    /** Samples per control-rate chunk: parameters/modulation are evaluated once
        per chunk, so behaviour does not depend on the host buffer size. */
    static constexpr int controlBlockSize = 64;

private:
    TransportState readTransport() const;

    juce::ValueTree getChainNode();
    void pushChainOrderToAudio();

    // ValueTree::Listener — keeps the audio-thread mirror in sync with the
    // state (GUI edits, undo/redo, preset/project loading).
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeRedirected (juce::ValueTree&) override;
    void processChunk (juce::AudioBuffer<float>& io, int start, int length, const TransportState&);

    juce::AudioProcessorValueTreeState apvts;
    ParameterCache params;
    juce::AudioProcessorParameter* bypassParam = nullptr;

    /** Effective (modulated) parameter values for the current chunk. */
    std::array<float, (size_t) numParams> effective {};

    // --- DSP stages -----------------------------------------------------------
    FXChain chain;
    AtmosCore core;
    std::atomic<std::uint64_t> chainOrderPacked { 0 };

    // Dry copy of the current chunk (for dry/wet and bypass).
    std::array<std::array<float, controlBlockSize>, 2> dryChunk {};

    // Provisional output stage (dry/wet, gain, bypass) until the full Output
    // Stage of phase F6 replaces it.
    dsp::LinearSmoother<float> outDryGain, outWetGain, outGain, bypassMix;

    double currentSampleRate = 48000.0;
    int maxBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AetherfallProcessor)
};

} // namespace aetherfall
