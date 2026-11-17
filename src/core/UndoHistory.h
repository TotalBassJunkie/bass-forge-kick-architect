// B-15 -- Undo/Redo ueber vollstaendige Kick-Zustaende.
#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <vector>

#include "core/KickState.h"

namespace bfka
{

/**
 * Undo/Redo ueber vollstaendige Kick-Zustaende (Ticket B-15).
 * Ein Schritt = der Zustand VOR einer Aenderung. Der Aufrufer klammert
 * jede Zustandsaenderung mit beginEdit()/endEdit().
 * Nur Message-Thread; darf allokieren.
 */
class UndoHistory
{
public:
    explicit UndoHistory (int maxStepsToKeep = 50);

    void beginEdit (const juce::String& name, const KickState& stateBeforeEdit);
    void endEdit();
    bool isEditOpen() const noexcept;

    bool canUndo() const noexcept;
    bool canRedo() const noexcept;
    juce::String undoName() const;
    juce::String redoName() const;

    bool undo (KickState& stateToRestore);
    bool redo (KickState& stateToRestore);

    void clear();
    int  undoDepth() const noexcept;
    int  redoDepth() const noexcept;
    int  maxSteps() const noexcept { return maxStepCount; }

private:
    struct Step { juce::String name; KickState state; };
    std::vector<Step> undoStack;
    std::vector<Step> redoStack;
    juce::String pendingName;
    KickState    pendingState;
    bool         editOpen = false;
    int          maxStepCount = 50;
};

} // namespace bfka
