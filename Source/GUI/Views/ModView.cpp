#include "ModView.h"
#include "PluginProcessor.h"
#include "GUI/LookAndFeel/AetherLookAndFeel.h"

namespace aetherfall
{

namespace
{
    juce::Colour accentOf (const juce::Component& c)
    {
        auto* lf = dynamic_cast<AetherLookAndFeel*> (&c.getLookAndFeel());
        return lf != nullptr ? lf->getAccent() : palette::accentDefault;
    }

    juce::String nameOf (PID p) { return juce::String (std::string (info (p).name)); }

    /** Fills a combo with every modulatable destination, grouped in sub-menus.
        Item id = PID + 2 (1 = none). */
    void fillDestinations (juce::ComboBox& box)
    {
        auto* root = box.getRootMenu();
        root->addItem (1, "-");

        auto group = [root] (const juce::String& title, PID first, PID last)
        {
            juce::PopupMenu sub;
            for (int p = (int) first; p <= (int) last; ++p)
                if (paramInfos[(size_t) p].isModulatable())
                    sub.addItem (p + 2, nameOf ((PID) p));
            if (sub.getNumItems() > 0)
                root->addSubMenu (title, sub);
        };

        group ("Global / Input", PID::GlobModDepth, PID::InBassMono);
        group ("ATMOS Core", PID::CoreMix, PID::CoreWidth);
        group ("Output", PID::OutTilt, PID::OutGain);
        for (const auto& m : moduleInfos)
            group (juce::String (std::string (m.name)), m.mix, m.lastParam);
        group ("LFOs", PID::Lfo1Shape, PID::Lfo8Smooth);
        group ("Envelopes", PID::Env1Attack, PID::Env4Gain);
        group ("Sequencers / Chaos", PID::Seq1Div, PID::Chaos2Smooth);
    }

    void fillSources (juce::ComboBox& box)
    {
        for (int s = 0; s < numModSources; ++s)
            box.addItem (juce::String (std::string (sourceNames[(size_t) s])), s + 1);
    }
}

//==============================================================================
SourceChip::SourceChip (AetherfallProcessor& p, ModSource s) : processor (p), source (s)
{
    setTooltip ("Drag onto a knob to modulate it with " + juce::String (std::string (sourceNames[(size_t) s])));
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void SourceChip::refresh()
{
    const float v = processor.getModSourceValue (source);
    if (std::abs (v - value) > 1.0e-3f) { value = v; repaint(); }
}

void SourceChip::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (1.5f);
    const auto accent = accentOf (*this);
    g.setColour (palette::panelRaised);
    g.fillRoundedRectangle (r, 4.0f);
    // Live value bar (bipolar sources centred).
    const float v = juce::jlimit (-1.0f, 1.0f, value);
    auto bar = r.reduced (2.0f).removeFromBottom (3.0f);
    g.setColour (accent.withAlpha (0.85f));
    if (v >= 0.0f) g.fillRect (bar.withWidth (bar.getWidth() * v));
    else           g.fillRect (bar.withLeft (bar.getRight() + bar.getWidth() * v));
    g.setColour (palette::outline);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    g.setColour (palette::text);
    g.setFont (AetherLookAndFeel::font (11.0f));
    g.drawFittedText (juce::String (std::string (sourceNames[(size_t) source])), getLocalBounds().withTrimmedBottom (4),
                      juce::Justification::centred, 1, 0.7f);
}

void SourceChip::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() < 4)
        return;
    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
        if (! container->isDragAndDropActive())
            container->startDragging ("modsrc:" + juce::String (std::string (sourceIds[(size_t) source])), this);
}

//==============================================================================
LfoShapeView::LfoShapeView (AetherfallProcessor& p, int index) : processor (p), lfo (index)
{
    points = processor.getModRouting().lfoCustom[(size_t) lfo];
    setTooltip ("LFO shape preview. With shape = Custom, draw the curve here.");
}

float LfoShapeView::shapeAt (float ph) const
{
    const auto& apvts = const_cast<AetherfallProcessor&> (processor).getValueTreeState();
    const int shape = (int) std::lround (apvts.getRawParameterValue (juce::String (std::string (info (lfoParam (lfo, 0)).id)))->load());
    const float off = apvts.getRawParameterValue (juce::String (std::string (info (lfoParam (lfo, 4)).id)))->load();
    float p = ph + off;
    p -= std::floor (p);
    switch (shape)
    {
        case 0: return std::sin (juce::MathConstants<float>::twoPi * p);
        case 1: return 1.0f - 4.0f * std::abs (p - 0.5f);
        case 2: return 2.0f * p - 1.0f;
        case 3: return 1.0f - 2.0f * p;
        case 4: return p < 0.5f ? 1.0f : -1.0f;
        case 5: { const int step = (int) (p * 8.0f); return std::sin ((float) step * 2.3f + 0.7f); }
        case 6: { return std::sin (p * 9.0f) * 0.6f + std::sin (p * 3.1f) * 0.4f; }
        case 7:
        {
            const float x = p * lfoCustomPoints;
            const int i0 = (int) x % lfoCustomPoints, i1 = (i0 + 1) % lfoCustomPoints;
            return points[(size_t) i0] + (x - std::floor (x)) * (points[(size_t) i1] - points[(size_t) i0]);
        }
        default: return 0.0f;
    }
}

void LfoShapeView::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (palette::panelRaised);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (palette::outline);
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());

    juce::Path path;
    const int n = 120;
    for (int i = 0; i <= n; ++i)
    {
        const float ph = (float) i / n;
        const float y = r.getCentreY() - shapeAt (ph) * (r.getHeight() * 0.42f);
        const float x = r.getX() + ph * r.getWidth();
        if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
    }
    g.setColour (accentOf (*this));
    g.strokePath (path, juce::PathStrokeType (2.0f));

    // Live value marker.
    const float v = processor.getModSourceValue ((ModSource) ((int) ModSource::Lfo1 + lfo));
    g.setColour (palette::warm);
    g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ r.getRight() - 6.0f, r.getCentreY() - juce::jlimit (-1.0f, 1.0f, v) * r.getHeight() * 0.42f }));
}

void LfoShapeView::edit (const juce::MouseEvent& e)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    const int i = juce::jlimit (0, lfoCustomPoints - 1, (int) ((e.position.x - r.getX()) / r.getWidth() * lfoCustomPoints));
    points[(size_t) i] = juce::jlimit (-1.0f, 1.0f, (r.getCentreY() - e.position.y) / (r.getHeight() * 0.42f));
    repaint();
}

void LfoShapeView::mouseDown (const juce::MouseEvent& e) { editing = true; edit (e); }
void LfoShapeView::mouseDrag (const juce::MouseEvent& e) { if (editing) edit (e); }
void LfoShapeView::mouseUp (const juce::MouseEvent&)
{
    if (editing)
        processor.setLfoShape (lfo, points);
    editing = false;
}

//==============================================================================
SeqStepsView::SeqStepsView (AetherfallProcessor& p, int index) : processor (p), seq (index)
{
    steps = processor.getModRouting().seqSteps[(size_t) seq];
    setTooltip ("Click / drag to draw the step values");
}

void SeqStepsView::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (palette::panelRaised);
    g.fillRoundedRectangle (r, 6.0f);
    const int count = (int) std::lround (const_cast<AetherfallProcessor&> (processor).getValueTreeState()
                                            .getRawParameterValue (juce::String (std::string (info (seqParam (seq, 1)).id)))->load());
    const float w = r.getWidth() / seqMaxSteps;
    const auto accent = accentOf (*this);
    for (int s = 0; s < seqMaxSteps; ++s)
    {
        auto cell = juce::Rectangle<float> (r.getX() + (float) s * w, r.getY(), w, r.getHeight()).reduced (1.0f, 3.0f);
        g.setColour (palette::track);
        g.fillRect (cell);
        g.setColour (s < count ? accent : accent.withAlpha (0.25f));
        g.fillRect (cell.withTop (cell.getBottom() - cell.getHeight() * steps[(size_t) s]));
    }
}

void SeqStepsView::edit (const juce::MouseEvent& e)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    const int s = juce::jlimit (0, seqMaxSteps - 1, (int) ((e.position.x - r.getX()) / r.getWidth() * seqMaxSteps));
    steps[(size_t) s] = juce::jlimit (0.0f, 1.0f, (r.getBottom() - e.position.y) / r.getHeight());
    repaint();
}

void SeqStepsView::mouseDown (const juce::MouseEvent& e) { editing = true; edit (e); }
void SeqStepsView::mouseDrag (const juce::MouseEvent& e) { if (editing) edit (e); }
void SeqStepsView::mouseUp (const juce::MouseEvent&)
{
    if (editing)
        processor.setSeqSteps (seq, steps);
    editing = false;
}

//==============================================================================
MatrixRow::MatrixRow (AetherfallProcessor& p, juce::AudioProcessorValueTreeState& apvts, int s)
    : processor (p), slot (s)
{
    fillSources (source);
    fillDestinations (dest);
    for (int c = 0; c < (int) ModCurve::Count; ++c)
        curve.addItem (juce::String (std::string (curveNames[(size_t) c])), c + 1);
    fillSources (via);
    via.setTextWhenNothingSelected ("via -");

    for (auto* b : { &source, &dest, &curve, &via })
    {
        b->onChange = [this] { commit(); };
        addAndMakeVisible (*b);
    }
    source.setTooltip ("Source");
    dest.setTooltip ("Destination");
    curve.setTooltip ("Response curve");
    via.setTooltip ("Via: optional second source that scales the amount");

    amount.setSliderStyle (juce::Slider::LinearHorizontal);
    amount.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    amount.setTooltip ("Amount (bipolar)");
    amount.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (amount);
    amountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, juce::String (std::string (info (modAmountParam (slot)).id)), amount);

    clear.setTooltip ("Clear this slot");
    clear.onClick = [this] { processor.setModSlot (slot, {}); };
    addAndMakeVisible (clear);
}

void MatrixRow::setSlot (const ModSlot& s)
{
    const juce::ScopedValueSetter<bool> svs (updating, true);
    source.setSelectedId ((int) s.source + 1, juce::dontSendNotification);
    dest.setSelectedId (s.dest >= 0 ? s.dest + 2 : 1, juce::dontSendNotification);
    curve.setSelectedId ((int) s.curve + 1, juce::dontSendNotification);
    via.setSelectedId ((int) s.via + 1, juce::dontSendNotification);
    repaint();
}

void MatrixRow::commit()
{
    if (updating)
        return;
    ModSlot s;
    s.source = (ModSource) std::max (0, source.getSelectedId() - 1);
    s.dest = (std::int16_t) (dest.getSelectedId() >= 2 ? dest.getSelectedId() - 2 : -1);
    s.curve = (ModCurve) std::max (0, curve.getSelectedId() - 1);
    s.via = (ModSource) std::max (0, via.getSelectedId() - 1);
    processor.setModSlot (slot, s);
}

void MatrixRow::paint (juce::Graphics& g)
{
    g.setColour ((slot & 1) ? palette::panel : palette::panelRaised.withAlpha (0.5f));
    g.fillRect (getLocalBounds());
    g.setColour (palette::textDim);
    g.setFont (AetherLookAndFeel::font (11.0f));
    g.drawText (juce::String (slot + 1), getLocalBounds().removeFromLeft (26), juce::Justification::centred);
}

void MatrixRow::resized()
{
    auto r = getLocalBounds().reduced (2, 3);
    r.removeFromLeft (26);
    clear.setBounds (r.removeFromRight (24));
    r.removeFromRight (4);
    source.setBounds (r.removeFromLeft (96));
    r.removeFromLeft (4);
    dest.setBounds (r.removeFromLeft (136));
    r.removeFromLeft (6);
    via.setBounds (r.removeFromRight (78));
    r.removeFromRight (4);
    curve.setBounds (r.removeFromRight (70));
    r.removeFromRight (6);
    amount.setBounds (r);
}

//==============================================================================
namespace
{
    /** Macro target editor row: destination + amount (writes the MACROS state). */
    class MacroTargetRow final : public juce::Component
    {
    public:
        MacroTargetRow (AetherfallProcessor& p, int m, int i) : processor (p), macro (m), index (i)
        {
            fillDestinations (dest);
            dest.onChange = [this] { commit(); };
            addAndMakeVisible (dest);
            amount.setSliderStyle (juce::Slider::LinearHorizontal);
            amount.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 18);
            amount.setRange (-1.0, 1.0, 0.01);
            amount.setDoubleClickReturnValue (true, 0.0);
            amount.onDragEnd = [this] { commit(); };
            addAndMakeVisible (amount);
            const auto t = processor.getModRouting().macros[(size_t) macro][(size_t) index];
            dest.setSelectedId (t.dest >= 0 ? t.dest + 2 : 1, juce::dontSendNotification);
            amount.setValue (t.amount, juce::dontSendNotification);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (2);
            dest.setBounds (r.removeFromLeft (r.getWidth() / 2));
            r.removeFromLeft (6);
            amount.setBounds (r);
        }

    private:
        void commit()
        {
            MacroTarget t;
            t.dest = (std::int16_t) (dest.getSelectedId() >= 2 ? dest.getSelectedId() - 2 : -1);
            t.amount = (float) amount.getValue();
            processor.setMacroTarget (macro, index, t);
        }

        AetherfallProcessor& processor;
        int macro, index;
        juce::ComboBox dest;
        juce::Slider amount;
    };
}

//==============================================================================
ModView::ModView (AetherfallProcessor& p, juce::AudioProcessorValueTreeState& s)
    : processor (p), apvts (s)
{
    addAndMakeVisible (sourcesSection);
    addAndMakeVisible (matrixSection);

    static const char* pageNames[] = { "LFO", "ENV", "SEQ", "CHAOS", "MACRO" };
    for (size_t i = 0; i < pageButtons.size(); ++i)
    {
        pageButtons[i].setButtonText (pageNames[i]);
        pageButtons[i].onClick = [this, i] { showPage ((Page) i, 0); };
        sourcesSection.addAndMakeVisible (pageButtons[i]);
    }
    for (size_t i = 0; i < instanceButtons.size(); ++i)
    {
        instanceButtons[i].setButtonText (juce::String ((int) i + 1));
        instanceButtons[i].onClick = [this, i] { showPage (page, (int) i); };
        sourcesSection.addAndMakeVisible (instanceButtons[i]);
    }

    matrixViewport.setViewedComponent (&matrixContent, false);
    matrixViewport.setScrollBarsShown (true, false);
    matrixSection.addAndMakeVisible (matrixViewport);
    shownRouting = processor.getModRouting();
    for (int i = 0; i < numModSlots; ++i)
    {
        rows.push_back (std::make_unique<MatrixRow> (processor, apvts, i));
        rows.back()->setSlot (shownRouting.slots[(size_t) i]);
        matrixContent.addAndMakeVisible (*rows.back());
    }

    for (int src = 1; src < numModSources; ++src)
    {
        chips.push_back (std::make_unique<SourceChip> (processor, (ModSource) src));
        addAndMakeVisible (*chips.back());
    }

    showPage (Page::Lfo, 0);
}

ModView::~ModView() { stopTimer(); }

void ModView::visibilityChanged()
{
    if (isShowing()) startTimerHz (20); else stopTimer();
}

void ModView::timerCallback()
{
    for (auto& c : chips) c->refresh();
    if (pageEditor != nullptr) pageEditor->repaint();

    // Reflect routing edits made elsewhere (knob drops, undo, presets).
    const auto r = processor.getModRouting();
    for (int i = 0; i < numModSlots; ++i)
    {
        const auto& a = r.slots[(size_t) i];
        const auto& b = shownRouting.slots[(size_t) i];
        if (a.source != b.source || a.dest != b.dest || a.curve != b.curve || a.via != b.via)
            rows[(size_t) i]->setSlot (a);
    }
    shownRouting = r;
}

void ModView::showPage (Page newPage, int newInstance)
{
    page = newPage;
    const int counts[] = { numLfos, numEnvFollowers, numStepSeqs, numChaos, numMacros };
    instance = juce::jlimit (0, counts[(int) page] - 1, newInstance);

    for (size_t i = 0; i < pageButtons.size(); ++i)
        pageButtons[i].setToggleState ((int) i == (int) page, juce::dontSendNotification);
    for (size_t i = 0; i < instanceButtons.size(); ++i)
    {
        instanceButtons[i].setVisible ((int) i < counts[(int) page]);
        instanceButtons[i].setToggleState ((int) i == instance, juce::dontSendNotification);
    }

    pageWidgets.clear();
    pageEditor.reset();
    macroRows.clear();

    auto add = [this] (PID pid) { pageWidgets.push_back (createParamWidget (apvts, pid, Knob::Size::Small)); };

    switch (page)
    {
        case Page::Lfo:
            for (int f = 0; f < lfoParamStride; ++f) add (lfoParam (instance, f));
            pageEditor = std::make_unique<LfoShapeView> (processor, instance);
            break;
        case Page::Env:
            for (int f = 0; f < envParamStride; ++f) add (envParam (instance, f));
            break;
        case Page::Seq:
            for (int f = 0; f < seqParamStride; ++f) add (seqParam (instance, f));
            pageEditor = std::make_unique<SeqStepsView> (processor, instance);
            break;
        case Page::Chaos:
            for (int f = 0; f < chaosParamStride; ++f) add (chaosParam (instance, f));
            break;
        case Page::Macro:
            add (macroParam (instance));
            for (int t = 0; t < macroTargetsPerMacro; ++t)
            {
                macroRows.push_back (std::make_unique<MacroTargetRow> (processor, instance, t));
                sourcesSection.addAndMakeVisible (*macroRows.back());
            }
            break;
    }

    for (auto& w : pageWidgets)
        sourcesSection.addAndMakeVisible (*w);
    if (pageEditor != nullptr)
        sourcesSection.addAndMakeVisible (*pageEditor);

    const char* titles[] = { "LFO", "Envelope follower", "Step sequencer", "Chaos", "Macro" };
    sourcesSection.setTitle (juce::String (titles[(int) page]) + " " + juce::String (instance + 1));
    resized();
}

void ModView::paint (juce::Graphics&) {}

void ModView::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto chipArea = r.removeFromBottom (34);
    r.removeFromBottom (10);

    sourcesSection.setBounds (r.removeFromLeft (380));
    r.removeFromLeft (12);
    matrixSection.setBounds (r);

    // Sources panel.
    auto c = sourcesSection.getContentBounds();
    auto tabs = c.removeFromTop (26);
    const int tw = tabs.getWidth() / (int) pageButtons.size();
    for (auto& b : pageButtons) b.setBounds (tabs.removeFromLeft (tw).reduced (2, 0));
    c.removeFromTop (6);
    auto inst = c.removeFromTop (22);
    for (auto& b : instanceButtons) b.setBounds (inst.removeFromLeft (36).reduced (2, 0));
    c.removeFromTop (8);

    if (page == Page::Macro)
    {
        if (! pageWidgets.empty())
            pageWidgets[0]->setBounds (c.removeFromTop (100).withSizeKeepingCentre (110, 100));
        c.removeFromTop (6);
        for (auto& row : macroRows)
            row->setBounds (c.removeFromTop (30));
    }
    else
    {
        if (pageEditor != nullptr)
        {
            pageEditor->setBounds (c.removeFromTop (110).reduced (4, 0));
            c.removeFromTop (8);
        }
        std::vector<juce::Component*> items;
        for (auto& w : pageWidgets) items.push_back (w.get());
        layoutGrid (items, c, 4, std::min (90, c.getHeight() / 2));
    }

    // Matrix.
    auto m = matrixSection.getContentBounds();
    matrixViewport.setBounds (m);
    const int rowH = 32;
    matrixContent.setSize (m.getWidth() - matrixViewport.getScrollBarThickness(), rowH * numModSlots);
    for (int i = 0; i < numModSlots; ++i)
        rows[(size_t) i]->setBounds (0, i * rowH, matrixContent.getWidth(), rowH);

    // Source chips.
    const int cw = chipArea.getWidth() / (int) chips.size();
    for (auto& chip : chips)
        chip->setBounds (chipArea.removeFromLeft (cw).reduced (1, 0));
}

} // namespace aetherfall
