// B-15 -- Undo/Redo ueber vollstaendige Kick-Zustaende.
#include "core/UndoHistory.h"

namespace bfka
{

UndoHistory::UndoHistory (int maxStepsToKeep)
    : maxStepCount (maxStepsToKeep > 0 ? maxStepsToKeep : 1)
{
}

void UndoHistory::beginEdit (const juce::String& name, const KickState& stateBeforeEdit)
{
    // Ein bereits offener Edit wird verworfen; nur der neue zaehlt.
    pendingName  = name;
    pendingState = stateBeforeEdit;
    editOpen     = true;
}

void UndoHistory::endEdit()
{
    if (! editOpen)
        return;

    undoStack.push_back ({ pendingName, pendingState });
    redoStack.clear();

    while ((int) undoStack.size() > maxStepCount)
        undoStack.erase (undoStack.begin());

    editOpen = false;
}

bool UndoHistory::isEditOpen() const noexcept
{
    return editOpen;
}

bool UndoHistory::canUndo() const noexcept
{
    return ! undoStack.empty();
}

bool UndoHistory::canRedo() const noexcept
{
    return ! redoStack.empty();
}

juce::String UndoHistory::undoName() const
{
    return undoStack.empty() ? juce::String() : undoStack.back().name;
}

juce::String UndoHistory::redoName() const
{
    return redoStack.empty() ? juce::String() : redoStack.back().name;
}

bool UndoHistory::undo (KickState& stateToRestore)
{
    if (undoStack.empty())
        return false;

    Step s = undoStack.back();
    undoStack.pop_back();
    redoStack.push_back ({ s.name, stateToRestore });
    stateToRestore = s.state;
    return true;
}

bool UndoHistory::redo (KickState& stateToRestore)
{
    if (redoStack.empty())
        return false;

    Step s = redoStack.back();
    redoStack.pop_back();
    undoStack.push_back ({ s.name, stateToRestore });
    stateToRestore = s.state;
    return true;
}

void UndoHistory::clear()
{
    undoStack.clear();
    redoStack.clear();
    editOpen = false;
}

int UndoHistory::undoDepth() const noexcept
{
    return (int) undoStack.size();
}

int UndoHistory::redoDepth() const noexcept
{
    return (int) redoStack.size();
}

} // namespace bfka
