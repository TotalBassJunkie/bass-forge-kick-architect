// B-121/B-122: Tonart-Zustand, Chip-Text, P-Flag-Prioritaet und Slot-Sortierung.

#include "engine/KeyState.h"

#include <algorithm>

namespace bfka
{

namespace
{
    /** Klemmt einen Grundton auf -1..11. */
    int clampNote (int keyNote) noexcept
    {
        return juce::jlimit (-1, 11, keyNote);
    }
}

const char* noteName (int keyNote) noexcept
{
    static const char* const names[12] =
    {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    if (keyNote < 0 || keyNote > 11)
        return "";

    return names[keyNote];
}

void KeyStateHolder::setMode (KeyChipMode newMode) noexcept
{
    const auto clamped = juce::jlimit (0, 2, (int) newMode);
    chipMode = (KeyChipMode) clamped;
}

KeyChipMode KeyStateHolder::mode() const noexcept
{
    return chipMode;
}

void KeyStateHolder::setFixedRoot (int keyNote) noexcept
{
    fixedNote = clampNote (keyNote);
}

int KeyStateHolder::fixedRoot() const noexcept
{
    return fixedNote;
}

void KeyStateHolder::setDetectedRoot (int keyNote) noexcept
{
    detectedNote = clampNote (keyNote);
}

int KeyStateHolder::detectedRoot() const noexcept
{
    return detectedNote;
}

int KeyStateHolder::effectiveRoot() const noexcept
{
    switch (chipMode)
    {
        case KeyChipMode::off:
            return -1;

        case KeyChipMode::fixed:
            return fixedNote >= 0 ? fixedNote : detectedNote;

        case KeyChipMode::autoDetect:
        default:
            return detectedNote;
    }
}

juce::String KeyStateHolder::chipText() const
{
    const auto nameOf = [] (int keyNote)
    {
        const auto* name = noteName (keyNote);
        return juce::String (name != nullptr && *name != '\0' ? name : "?");
    };

    switch (chipMode)
    {
        case KeyChipMode::off:
            return "X";

        case KeyChipMode::fixed:
            return "FIX " + nameOf (fixedNote);

        case KeyChipMode::autoDetect:
        default:
            break;
    }

    // Automatik: erkannter Grundton, optional mit abweichendem festen Grundton.
    if (detectedNote < 0)
        return "AUTO";

    if (fixedNote >= 0 && fixedNote != detectedNote)
        return nameOf (detectedNote) + "+" + nameOf (fixedNote);

    return nameOf (detectedNote);
}

int KeyStateHolder::priorityForRole (Role role) noexcept
{
    switch (role)
    {
        case Role::crunch: return 0;
        case Role::rumble: return 1;
        default:           return 2;
    }
}

std::vector<int> KeyStateHolder::slotsByPriority (const KickState& state)
{
    std::vector<int> order;
    order.reserve ((size_t) KickState::kNumSlots);

    for (int slot = 0; slot < KickState::kNumSlots; ++slot)
        order.push_back (slot);

    std::stable_sort (order.begin(), order.end(),
                      [&state] (int a, int b)
                      {
                          return priorityForRole (state.getRole (a))
                               < priorityForRole (state.getRole (b));
                      });

    return order;
}

} // namespace bfka
