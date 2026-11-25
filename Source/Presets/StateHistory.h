#pragma once

#include <deque>
#include <juce_data_structures/juce_data_structures.h>

namespace aetherfall
{

/**
    Undo / redo by full-state snapshots (message thread only).

    A snapshot is taken BEFORE every user edit (start of a knob gesture, chain
    or modulation edit...). Host automation never creates history entries, so
    the undo list only contains what the user did in the UI. Capacity: 100.
*/
class StateHistory
{
public:
    static constexpr size_t capacity = 100;

    /** Records `stateBeforeEdit`; clears the redo list. Skipped if identical to the last entry. */
    void checkpoint (const juce::ValueTree& stateBeforeEdit);

    /** Returns the state to restore (or an invalid tree). `current` goes to the redo list. */
    juce::ValueTree undo (const juce::ValueTree& current);
    juce::ValueTree redo (const juce::ValueTree& current);

    bool canUndo() const noexcept { return ! undoStack.empty(); }
    bool canRedo() const noexcept { return ! redoStack.empty(); }
    size_t undoSize() const noexcept { return undoStack.size(); }
    void clear() { undoStack.clear(); redoStack.clear(); }

private:
    std::deque<juce::ValueTree> undoStack, redoStack;
};

} // namespace aetherfall
