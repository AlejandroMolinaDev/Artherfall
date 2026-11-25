#include "ModRoutingState.h"

namespace aetherfall::modstate
{

namespace
{
    template <size_t N>
    juce::String floatsToString (const std::array<float, N>& values)
    {
        juce::StringArray parts;
        for (float v : values)
            parts.add (juce::String (v, 4));
        return parts.joinIntoString (",");
    }

    template <size_t N>
    bool stringToFloats (const juce::String& text, std::array<float, N>& out, float lo, float hi)
    {
        juce::StringArray parts;
        parts.addTokens (text, ",", {});
        if (parts.size() < (int) N)
            return false;
        for (size_t i = 0; i < N; ++i)
            out[i] = juce::jlimit (lo, hi, parts[(int) i].getFloatValue());
        return true;
    }

    juce::ValueTree childWithIndex (juce::ValueTree parent, const juce::Identifier& type, int index, juce::UndoManager* um)
    {
        for (auto c : parent)
            if (c.hasType (type) && (int) c.getProperty ("i", -1) == index)
                return c;

        juce::ValueTree c (type);
        c.setProperty ("i", index, nullptr);
        parent.appendChild (c, um);
        return c;
    }

    juce::String sourceId (ModSource s) { return juce::String (std::string (sourceIds[(size_t) s])); }
}

juce::String destToId (int dest)
{
    if (dest < 0 || dest >= numParams)
        return {};
    return juce::String (std::string (paramInfos[(size_t) dest].id));
}

int destFromId (const juce::String& id)
{
    if (id.isEmpty())
        return -1;
    const auto s = id.toStdString();
    for (const auto& p : paramInfos)
        if (p.id == s)
            return (int) p.pid;
    return -1;
}

bool isModulationNode (const juce::ValueTree& tree)
{
    for (auto t = tree; t.isValid(); t = t.getParent())
        if (t.hasType (matrixNode) || t.hasType (macrosNode) || t.hasType (seqNode) || t.hasType (lfoShapesNode))
            return true;
    return false;
}

void ensureNodes (juce::ValueTree& root)
{
    const auto defaults = routing::makeDefault();

    if (! root.getChildWithName (matrixNode).isValid())
        root.appendChild (juce::ValueTree (matrixNode), nullptr);

    if (! root.getChildWithName (macrosNode).isValid())
    {
        root.appendChild (juce::ValueTree (macrosNode), nullptr);
        for (int m = 0; m < numMacros; ++m)
            for (int i = 0; i < macroTargetsPerMacro; ++i)
                if (defaults.macros[(size_t) m][(size_t) i].dest >= 0)
                    writeMacroTarget (root, m, i, defaults.macros[(size_t) m][(size_t) i]);
    }

    if (! root.getChildWithName (seqNode).isValid())
    {
        root.appendChild (juce::ValueTree (seqNode), nullptr);
        for (int s = 0; s < numStepSeqs; ++s)
            writeSeqSteps (root, s, defaults.seqSteps[(size_t) s]);
    }

    if (! root.getChildWithName (lfoShapesNode).isValid())
    {
        root.appendChild (juce::ValueTree (lfoShapesNode), nullptr);
        for (int l = 0; l < numLfos; ++l)
            writeLfoShape (root, l, defaults.lfoCustom[(size_t) l]);
    }
}

ModRouting read (const juce::ValueTree& root)
{
    ModRouting r = routing::makeDefault();

    if (auto matrix = root.getChildWithName (matrixNode); matrix.isValid())
    {
        for (auto c : matrix)
        {
            const int i = c.getProperty ("i", -1);
            if (! c.hasType (slotNode) || i < 0 || i >= numModSlots)
                continue;
            auto& s = r.slots[(size_t) i];
            s.source = sourceFromId (c.getProperty ("src").toString().toStdString());
            s.dest = (std::int16_t) destFromId (c.getProperty ("dst").toString());
            s.curve = (ModCurve) juce::jlimit (0, (int) ModCurve::Count - 1, (int) c.getProperty ("curve", 0));
            s.via = sourceFromId (c.getProperty ("via").toString().toStdString());
        }
    }

    if (auto macros = root.getChildWithName (macrosNode); macros.isValid())
    {
        r.macros = {};
        for (auto m : macros)
        {
            const int mi = m.getProperty ("i", -1);
            if (! m.hasType (macroNode) || mi < 0 || mi >= numMacros)
                continue;
            for (auto t : m)
            {
                const int ti = t.getProperty ("i", -1);
                if (! t.hasType (targetNode) || ti < 0 || ti >= macroTargetsPerMacro)
                    continue;
                r.macros[(size_t) mi][(size_t) ti] = { (std::int16_t) destFromId (t.getProperty ("dst").toString()),
                                                       juce::jlimit (-1.0f, 1.0f, (float) t.getProperty ("amt", 0.0f)) };
            }
        }
    }

    if (auto seq = root.getChildWithName (seqNode); seq.isValid())
        for (int s = 0; s < numStepSeqs; ++s)
            stringToFloats (seq.getProperty ("steps" + juce::String (s + 1)).toString(), r.seqSteps[(size_t) s], 0.0f, 1.0f);

    if (auto shapes = root.getChildWithName (lfoShapesNode); shapes.isValid())
        for (int l = 0; l < numLfos; ++l)
            stringToFloats (shapes.getProperty ("lfo" + juce::String (l + 1)).toString(), r.lfoCustom[(size_t) l], -1.0f, 1.0f);

    return r;
}

void writeSlot (juce::ValueTree& root, int slot, const ModSlot& s, juce::UndoManager* um)
{
    if (slot < 0 || slot >= numModSlots)
        return;
    auto matrix = root.getOrCreateChildWithName (matrixNode, um);
    auto c = childWithIndex (matrix, slotNode, slot, um);
    c.setProperty ("src", sourceId (s.source), um);
    c.setProperty ("dst", destToId (s.dest), um);
    c.setProperty ("curve", (int) s.curve, um);
    c.setProperty ("via", sourceId (s.via), um);
}

void writeMacroTarget (juce::ValueTree& root, int macro, int index, const MacroTarget& t, juce::UndoManager* um)
{
    if (macro < 0 || macro >= numMacros || index < 0 || index >= macroTargetsPerMacro)
        return;
    auto macros = root.getOrCreateChildWithName (macrosNode, um);
    auto m = childWithIndex (macros, macroNode, macro, um);
    auto c = childWithIndex (m, targetNode, index, um);
    c.setProperty ("dst", destToId (t.dest), um);
    c.setProperty ("amt", t.amount, um);
}

void writeSeqSteps (juce::ValueTree& root, int seq, const std::array<float, (size_t) seqMaxSteps>& steps, juce::UndoManager* um)
{
    if (seq < 0 || seq >= numStepSeqs)
        return;
    root.getOrCreateChildWithName (seqNode, um).setProperty ("steps" + juce::String (seq + 1), floatsToString (steps), um);
}

void writeLfoShape (juce::ValueTree& root, int lfo, const std::array<float, (size_t) lfoCustomPoints>& points, juce::UndoManager* um)
{
    if (lfo < 0 || lfo >= numLfos)
        return;
    root.getOrCreateChildWithName (lfoShapesNode, um).setProperty ("lfo" + juce::String (lfo + 1), floatsToString (points), um);
}

} // namespace aetherfall::modstate
