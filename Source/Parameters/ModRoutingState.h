#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "DSP/Modulation/ModRouting.h"

// -----------------------------------------------------------------------------
// ModRouting <-> state tree. Nodes (children of the APVTS state):
//   MODMATRIX  { SLOT  i, src, dst, curve, via }      (32 slots, amount = param)
//   MACROS     { MACRO i { TARGET dst, amt } }         (4 x 8 targets)
//   SEQ        steps1="..", steps2=".."                (32 values each, 0..1)
//   LFOSHAPES  lfo1=".." .. lfo8=".."                  (16 values each, -1..1)
// Sources and destinations are stored by string id, never by index.
// -----------------------------------------------------------------------------
namespace aetherfall::modstate
{

inline const juce::Identifier matrixNode { "MODMATRIX" }, slotNode { "SLOT" };
inline const juce::Identifier macrosNode { "MACROS" }, macroNode { "MACRO" }, targetNode { "TARGET" };
inline const juce::Identifier seqNode { "SEQ" }, lfoShapesNode { "LFOSHAPES" };

/** True if the tree is one of the modulation state nodes (or inside one). */
bool isModulationNode (const juce::ValueTree& tree);

/** Creates any missing node with default content (fresh or old states). */
void ensureNodes (juce::ValueTree& root);

/** Builds the routing from the state (missing parts -> defaults). */
ModRouting read (const juce::ValueTree& root);

/** Writers (message thread). Pass an UndoManager to make edits undoable. */
void writeSlot (juce::ValueTree& root, int slot, const ModSlot& s, juce::UndoManager* um = nullptr);
void writeMacroTarget (juce::ValueTree& root, int macro, int index, const MacroTarget& t, juce::UndoManager* um = nullptr);
void writeSeqSteps (juce::ValueTree& root, int seq, const std::array<float, (size_t) seqMaxSteps>& steps, juce::UndoManager* um = nullptr);
void writeLfoShape (juce::ValueTree& root, int lfo, const std::array<float, (size_t) lfoCustomPoints>& points, juce::UndoManager* um = nullptr);

/** Destination helpers: PID index <-> parameter string id. */
juce::String destToId (int dest);
int destFromId (const juce::String& id);

} // namespace aetherfall::modstate
