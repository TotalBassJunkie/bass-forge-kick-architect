// B-122: Zeichnung und Klick-Weiterschaltung des KEY-Chips.

#include "ui/KeyChip.h"

#include "ui/UiTheme.h"

namespace bfka
{

namespace
{
    constexpr int kPreferredWidth = 96;
    constexpr int kCornerSize     = 4;
}

KeyChip::KeyChip (KeyStateHolder& stateToUse)
    : state (stateToUse)
{
    setInterceptsMouseClicks (true, false);
}

void KeyChip::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (ui::theme::panelAlt);
    g.fillRoundedRectangle (bounds, (float) kCornerSize);

    g.setColour (ui::theme::grid);
    g.drawRoundedRectangle (bounds, (float) kCornerSize, 1.0f);

    g.setColour (ui::theme::text);
    g.setFont (ui::theme::sectionFont());
    g.drawText (state.chipText(), getLocalBounds().reduced (6, 0),
                juce::Justification::centred, true);
}

void KeyChip::mouseDown (const juce::MouseEvent&)
{
    state.setMode ((KeyChipMode) (((int) state.mode() + 1) % 3));
    repaint();

    if (onChange)
        onChange();
}

int KeyChip::preferredWidth() const noexcept
{
    return kPreferredWidth;
}

} // namespace bfka
