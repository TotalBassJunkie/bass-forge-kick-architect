#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

namespace bfka
{

/**
 * Stapel von Overlays innerhalb des Editor-Fensters (Ticket B-105).
 *
 * Kein showModalDialog, kein eigener Message-Loop: das Overlay liegt als
 * Kind ueber der Seite, dunkelt den Hintergrund ab und faengt Klicks.
 * Escape schliesst das oberste Overlay, Enter reicht an das Overlay weiter.
 */
class OverlayHost final : public juce::Component
{
public:
    OverlayHost();
    ~OverlayHost() override;

    /** Legt ein Overlay oben auf den Stapel. nullptr wird ignoriert. */
    void push (std::unique_ptr<juce::Component> overlay);
    /** Nimmt das oberste Overlay herunter (ohne es zu zerstoeren, s. takeTop). */
    void pop();
    /** Nimmt das oberste Overlay heraus und liefert es (nullptr bei leer). */
    std::unique_ptr<juce::Component> takeTop();

    bool isOpen() const noexcept { return ! stack.empty(); }
    int  depth() const noexcept { return (int) stack.size(); }

    /** Setzt die Flaeche, die der Host einnimmt (typisch: Elternrechteck). */
    void setBoundsToParent (juce::Rectangle<int> parentBounds);

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    void mouseDown (const juce::MouseEvent& event) override;

    /** Wird gerufen, wenn der Stapel leer wird. */
    std::function<void()> onAllClosed;

    /**
     * Schliessen-Knopf des Hosts (B-105). Er liegt in der oberen rechten Ecke
     * ueber dem obersten Overlay, damit JEDES Overlay ohne eigene Chrome
     * schliessbar ist. Der Knopf traegt die Component-ID `overlay-close`.
     */
    juce::Button* closeButton() noexcept { return &closeButton_; }

private:
    void updateVisibility();
    std::vector<std::unique_ptr<juce::Component>> stack;
    juce::TextButton closeButton_ { "X" };
    static constexpr int kOverlayInset = 24;
    static constexpr int kCloseButtonSize = 28;
};

} // namespace bfka
