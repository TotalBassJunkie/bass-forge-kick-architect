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

    // Schliessen-Knopf fuer JEDES Overlay (B-105). Er gehoert dem Host, nicht
    // den einzelnen Overlays: so kann kein Overlay vergessen werden.
    closeButton_.setComponentID ("overlay-close");
    closeButton_.setTooltip ("Overlay schliessen (Esc)");
    closeButton_.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff3a3a3a));
    closeButton_.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe0663a));
    closeButton_.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    closeButton_.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    closeButton_.onClick = [this] { pop(); };
    addChildComponent (closeButton_);
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

    closeButton_.setVisible (true);
    closeButton_.toFront (false);

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
    closeButton_.setVisible (isOpen());

    if (isOpen())
        closeButton_.toFront (false);
}

void OverlayHost::resized()
{
    auto area = getLocalBounds().reduced (kOverlayInset);

    // Eigene Kopfleiste ueber dem Overlay: dort sitzt der Schliessen-Knopf,
    // damit er nie Inhalt eines Overlays ueberdeckt (Tabs, Titel, Listen).
    auto chrome = area.removeFromTop (kCloseButtonSize + 8);

    for (auto& overlay : stack)
        if (overlay != nullptr)
            overlay->setBounds (area);

    closeButton_.setBounds (chrome.getRight() - kCloseButtonSize, chrome.getY(),
                            kCloseButtonSize, kCloseButtonSize);
    closeButton_.toFront (false);
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
