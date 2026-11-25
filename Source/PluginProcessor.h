#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Parameters/ParamIDs.h"
#include "Parameters/ParamLayout.h"
#include "DSP/Chain/ModContext.h"
#include "DSP/Core/AtmosCore.h"
#include "DSP/Core/InputStage.h"
#include "DSP/Core/OutputStage.h"
#include "DSP/Utils/Meters.h"
#include "DSP/Chain/FXChain.h"
#include "DSP/Chain/ChainOrder.h"
#include "DSP/Modulation/ModulationEngine.h"
#include "Parameters/ModRoutingState.h"
#include "Presets/StateHistory.h"
#include "Presets/FactoryPresets.h"
#include "DSP/Utils/Smoothers.h"
#include "DSP/Utils/DelayLine.h"

namespace aetherfall
{

class AetherfallProcessor final : public juce::AudioProcessor,
                                  private juce::ValueTree::Listener,
                                  private juce::AudioProcessorParameter::Listener
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

    // Trance Gate pattern (16 steps, bit 0 = step 1), stored in the GATE node.
    void setGatePattern (std::uint32_t pattern);
    std::uint32_t getGatePattern() const;

    static inline const juce::Identifier gateNodeId { "GATE" };
    static inline const juce::Identifier gatePatternProp { "pattern" };

    //==========================================================================
    // Modulation routing (matrix sources/destinations, macro maps, sequencer
    // steps, custom LFO shapes). Message thread; stored in the state tree.
    ModRouting getModRouting() const { return modstate::read (apvts.state); }
    void setModSlot (int slot, const ModSlot& s, juce::UndoManager* um = nullptr);
    void setMacroTarget (int macro, int index, const MacroTarget& t, juce::UndoManager* um = nullptr);
    void setSeqSteps (int seq, const std::array<float, (size_t) seqMaxSteps>& steps, juce::UndoManager* um = nullptr);
    void setLfoShape (int lfo, const std::array<float, (size_t) lfoCustomPoints>& points, juce::UndoManager* um = nullptr);

    /** Modulation offset currently applied to a parameter (normalised, GUI rings). */
    float getModOffset (PID p) const noexcept { return modEngine.getModOffset (p); }

    /** Assigns `source` to `dest` in the first free matrix slot (amount 50 %).
        Returns the slot index or -1 if the matrix is full / dest not modulatable. */
    int assignModulation (ModSource source, PID dest, float amount = 0.5f);

    /** Clears every matrix slot that targets `dest`. */
    void removeModulation (PID dest);

    //==========================================================================
    // Undo / redo (message thread). Every UI gesture and state edit is undoable.
    void checkpoint();
    bool undo();
    bool redo();
    bool canUndo() const noexcept { return history.canUndo(); }
    bool canRedo() const noexcept { return history.canRedo(); }

    // A/B comparison of two complete states.
    void toggleAB();
    void copyCurrentToOtherAB();
    int getABIndex() const noexcept { return abIndex; }

    // Smart randomisation of one section with an intensity 0..1.
    enum class RandomSection { Core, Chain, Modulation, Output };
    void randomize (RandomSection section, float intensity, std::uint32_t seed);

    /** Pops up to `max` mono samples of the wet signal for the spectrum
        analyser (GUI thread; lock-free single reader). */
    int readSpectrumSamples (float* dest, int max) noexcept;

    //==========================================================================
    // Presets (message thread).
    class PresetManager& getPresetManager();
    void loadPresetState (const juce::ValueTree& state, const juce::String& name, PresetCategory category);
    void setPresetInfo (const juce::String& name, PresetCategory category);
    juce::String getPresetName() const;
    PresetCategory getPresetCategory() const;
    static inline const juce::Identifier presetNodeId { "PRESET" };

    /** Level meters and gain reductions (GUI). */
    const Meters& getMeters() const noexcept { return meters; }

    /** Live modulation source values (GUI meters / rings). */
    float getModSourceValue (ModSource s) const noexcept { return modEngine.getSourceValue (s); }

    /** Samples per control-rate chunk: parameters/modulation are evaluated once
        per chunk, so behaviour does not depend on the host buffer size. */
    static constexpr int controlBlockSize = 64;

private:
    TransportState readTransport() const;

    juce::ValueTree getChainNode();
    juce::ValueTree getGateNode();
    void pushChainOrderToAudio();
    void pushGatePatternToAudio();
    void pushModRoutingToAudio();
    void pushAllStateToAudio();
    void handleMidi (const juce::MidiBuffer&);
    void restoreState (const juce::ValueTree&);

    // AudioProcessorParameter::Listener — gestures create undo checkpoints.
    void parameterValueChanged (int, float) override {}
    void parameterGestureChanged (int, bool gestureIsStarting) override;

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

    // --- Presets / Undo / A-B (message thread) ---------------------------------
    std::unique_ptr<class PresetManager> presetManager;
    StateHistory history;
    std::array<juce::ValueTree, 2> abStates;
    int abIndex = 0;
    int gestureDepth = 0;
    bool restoringState = false;

    // --- Modulation -----------------------------------------------------------
    ModulationEngine modEngine;
    juce::SpinLock routingWriteLock;   // writer side only (never taken on the audio thread)
    std::array<std::array<float, controlBlockSize>, 2> lastWet {};

    // Wet signal for the GUI spectrum analyser (single producer / consumer).
    static constexpr int spectrumFifoSize = 8192;
    juce::AbstractFifo spectrumFifo { spectrumFifoSize };
    std::array<float, spectrumFifoSize> spectrumBuffer {};
    int lastWetLength = 0;

    // --- DSP stages -----------------------------------------------------------
    FXChain chain;
    AtmosCore core;
    std::atomic<std::uint64_t> chainOrderPacked { 0 };
    std::atomic<std::uint32_t> gatePatternBits { 0xFFFFu };

    // Dry copy of the current chunk (for dry/wet and bypass).
    std::array<std::array<float, controlBlockSize>, 2> dryChunk {};

    // Latency compensation of the dry path (the chain may add latency, e.g.
    // the Spectral module). Changes of the delay are crossfaded.
    std::array<dsp::DelayLine<float>, 2> dryDelay;
    int dryDelayCurrent = 0, dryDelayPrevious = 0;
    dsp::LinearSmoother<float> dryDelayFade;
    int reportedLatency = 0;
    void updateLatency (int newLatency);

    InputStage inputStage;
    OutputStage outputStage;
    Meters meters;
    LevelMeter inMeter;

    double currentSampleRate = 48000.0;
    int maxBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AetherfallProcessor)
};

} // namespace aetherfall
