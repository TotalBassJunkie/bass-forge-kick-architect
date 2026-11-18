#pragma once
// B-122: KEY-Chip im Transport, zeigt den Tonart-Zustand und schaltet den Modus weiter.

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

#include "engine/KeyState.h"

namespace bfka
{

/**
 * KEY-Chip im Transport (Ticket B-122).
 * Zeigt den chipText und schaltet per Klick den Modus weiter
 * (auto -> fixed -> off -> auto).
 */
class KeyChip final : public juce::Component
{
public:
    explicit KeyChip (KeyStateHolder& stateToUse);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    int preferredWidth() const noexcept;   ///< 96

    std::function<void()> onChange;

private:
    KeyStateHolder& state;
};

} // namespace bfka
