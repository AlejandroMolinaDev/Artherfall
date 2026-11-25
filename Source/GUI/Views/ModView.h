#pragma once

#include <array>
#include <memory>
#include <vector>
#include "GUI/Components/ParamWidgets.h"
#include "DSP/Modulation/ModRouting.h"

namespace aetherfall
{

class AetherfallProcessor;

/** Draggable chip of a modulation source with a live value bar. Drag it onto
    any knob (tabs switch automatically while dragging over them). */
class SourceChip final : public juce::Component, public juce::SettableTooltipClient
{
public:
    SourceChip (AetherfallProcessor&, ModSource);
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void refresh();

private:
    AetherfallProcessor& processor;
    ModSource source;
    float value = 0.0f;
};

/** Preview of an LFO cycle; the custom shape can be drawn with the mouse. */
class LfoShapeView final : public juce::Component, public juce::SettableTooltipClient
{
public:
    LfoShapeView (AetherfallProcessor&, int lfoIndex);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void edit (const juce::MouseEvent&);
    float shapeAt (float phase) const;
    AetherfallProcessor& processor;
    int lfo;
    std::array<float, (size_t) lfoCustomPoints> points {};
    bool editing = false;
};

/** Editable step values of a step sequencer. */
class SeqStepsView final : public juce::Component, public juce::SettableTooltipClient
{
public:
    SeqStepsView (AetherfallProcessor&, int seqIndex);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void edit (const juce::MouseEvent&);
    AetherfallProcessor& processor;
    int seq;
    std::array<float, (size_t) seqMaxSteps> steps {};
    bool editing = false;
};

/** One row of the modulation matrix. */
class MatrixRow final : public juce::Component
{
public:
    MatrixRow (AetherfallProcessor&, juce::AudioProcessorValueTreeState&, int slot);
    void setSlot (const ModSlot&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void commit();
    AetherfallProcessor& processor;
    const int slot;
    juce::ComboBox source, dest, curve, via;
    juce::Slider amount;
    juce::TextButton clear { "x" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;
    bool updating = false;
};

/** MOD view: source editors (LFO / ENV / SEQ / CHAOS / MACRO), the 32-slot
    matrix and the strip of draggable source chips. */
class ModView final : public juce::Component, private juce::Timer
{
public:
    ModView (AetherfallProcessor&, juce::AudioProcessorValueTreeState&);
    ~ModView() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

private:
    enum class Page { Lfo = 0, Env, Seq, Chaos, Macro };
    void showPage (Page, int instance);
    void timerCallback() override;

    AetherfallProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    Section sourcesSection { "Sources" }, matrixSection { "Matrix" };
    std::array<juce::TextButton, 5> pageButtons;
    std::array<juce::TextButton, 8> instanceButtons;
    Page page = Page::Lfo;
    int instance = 0;

    std::vector<std::unique_ptr<ParamWidget>> pageWidgets;
    std::unique_ptr<juce::Component> pageEditor;
    std::vector<std::unique_ptr<juce::Component>> macroRows;

    juce::Viewport matrixViewport;
    juce::Component matrixContent;
    std::vector<std::unique_ptr<MatrixRow>> rows;
    ModRouting shownRouting {};

    std::vector<std::unique_ptr<SourceChip>> chips;
};

} // namespace aetherfall
