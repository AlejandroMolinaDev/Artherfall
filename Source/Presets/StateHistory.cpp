#include "StateHistory.h"

namespace aetherfall
{

void StateHistory::checkpoint (const juce::ValueTree& before)
{
    if (! undoStack.empty() && undoStack.back().isEquivalentTo (before))
        return;

    undoStack.push_back (before.createCopy());
    while (undoStack.size() > capacity)
        undoStack.pop_front();
    redoStack.clear();
}

juce::ValueTree StateHistory::undo (const juce::ValueTree& current)
{
    if (undoStack.empty())
        return {};
    auto state = undoStack.back();
    undoStack.pop_back();
    redoStack.push_back (current.createCopy());
    return state;
}

juce::ValueTree StateHistory::redo (const juce::ValueTree& current)
{
    if (redoStack.empty())
        return {};
    auto state = redoStack.back();
    redoStack.pop_back();
    undoStack.push_back (current.createCopy());
    return state;
}

} // namespace aetherfall
