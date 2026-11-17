#include "ui/overlays/DialogHost.h"

namespace bfka
{

OverlayHost::OverlayHost()
{
    // Der Host selbst nimmt Klicks an und schluckt sie, damit nichts auf die
    // Seite darunter durchfaellt. Tastaturfokus liegt beim Host, solange ein
    // Overlay offen ist.
    setInterceptsMouseClicks (true, true);
    setWantsKeyboardFocus (true);
    setVisible (false);
}

OverlayHost::~OverlayHost()
{
    // Kinder zuerst abmelden, dann den Stapel abbauen: so bleibt die
    // Zerstoerungsreihenfolge eindeutig.
    for (auto& overlay : stack)
        if (overlay != nullptr)
            removeChildComponent (overlay.get());

    stack.clear();
}

void OverlayHost::push (std::unique_ptr<juce::Component> overlay)
{
    if (overlay == nullptr)
        return;

    addAndMakeVisible (*overlay);
    stack.push_back (std::move (overlay));

    setVisible (true);
    toFront (true);
    grabKeyboardFocus();

    resized();
    repaint();
}

void OverlayHost::pop()
{
    if (stack.empty())
        return;

    auto top = std::move (stack.back());
    stack.pop_back();

    if (top != nullptr)
        removeChildComponent (top.get());

    // Erst nach dem Abmelden zerstoeren, sonst haengt ein toter Zeiger im Baum.
    top.reset();

    updateVisibility();
    resized();
    repaint();

    if (stack.empty() && onAllClosed != nullptr)
        onAllClosed();
}

std::unique_ptr<juce::Component> OverlayHost::takeTop()
{
    if (stack.empty())
        return {};

    auto top = std::move (stack.back());
    stack.pop_back();

    if (top != nullptr)
        removeChildComponent (top.get());

    updateVisibility();
    resized();
    repaint();

    if (stack.empty() && onAllClosed != nullptr)
        onAllClosed();

    return top;
}

void OverlayHost::setBoundsToParent (juce::Rectangle<int> parentBounds)
{
    setBounds (parentBounds);
}

void OverlayHost::updateVisibility()
{
    setVisible (isOpen());
}

void OverlayHost::resized()
{
    const auto area = getLocalBounds().reduced (kOverlayInset);

    for (auto& overlay : stack)
        if (overlay != nullptr)
            overlay->setBounds (area);
}

void OverlayHost::paint (juce::Graphics& g)
{
    // Abdunkeln des Hintergrunds; das Overlay selbst liegt darueber.
    g.fillAll (juce::Colours::black.withAlpha (0.55f));
}

bool OverlayHost::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        pop();
        return true;
    }

    // Enter absichtlich nicht schlucken: das Overlay soll es bekommen.
    return false;
}

void OverlayHost::mouseDown (const juce::MouseEvent& event)
{
    // Absichtlich leer: die Ueberschreibung verhindert, dass Klicks auf die
    // Seite unter dem Overlay durchfallen.
    juce::ignoreUnused (event);
}

} // namespace bfka
