#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Parameters/ParamIDs.h"
#include "Parameters/ParamLayout.h"
#include "DSP/Chain/ModContext.h"
#include "DSP/Core/AtmosCore.h"
#include "DSP/Utils/Smoothers.h"

namespace aetherfall
{

class AetherfallProcessor final : public juce::AudioProcessor
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

    /** Samples per control-rate chunk: parameters/modulation are evaluated once
        per chunk, so behaviour does not depend on the host buffer size. */
    static constexpr int controlBlockSize = 64;

private:
    TransportState readTransport() const;
    void processChunk (juce::AudioBuffer<float>& io, int start, int length, const TransportState&);

    juce::AudioProcessorValueTreeState apvts;
    ParameterCache params;
    juce::AudioProcessorParameter* bypassParam = nullptr;

    /** Effective (modulated) parameter values for the current chunk. */
    std::array<float, (size_t) numParams> effective {};

    // --- DSP stages -----------------------------------------------------------
    AtmosCore core;

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
