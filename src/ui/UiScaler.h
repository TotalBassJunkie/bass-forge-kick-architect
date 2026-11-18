#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// B-89: Fenster-Resize, UIScaler und HiDPI (70..200 %).

namespace bfka
{

/**
 * Skaliert das Editor-Fenster zwischen 70 % und 200 % (Ticket B-89).
 *
 * Die Komponenten werden NICHT nachgebaut: der Wurzelkomponente wird eine
 * Affine-Transformation zugewiesen und ihre Groesse auf die
 * Entwurfsgroesse * Faktor gesetzt. Die Entwurfsgroesse (Design) bleibt
 * die Bezugsgroesse fuer alle Layouts.
 */
class UiScaler
{
public:
    static constexpr int kMinPercent     = 70;
    static constexpr int kMaxPercent     = 200;
    static constexpr int kDefaultPercent = 100;
    static constexpr int kStepPercent    = 10;

    /** Entwurfsgroesse ohne Skalierung. */
    static constexpr int kDesignWidth  = 1180;
    static constexpr int kDesignHeight = 760;

    UiScaler();

    /** Prozentwert setzen (wird auf 70..200 geklemmt und auf 10er gerundet). */
    void setPercent (int percent);
    int  percent() const noexcept;

    /** Faktor fuer Layout-Rechnungen (0.70 .. 2.00). */
    float scaleFactor() const noexcept;

    /** Naechster/ voriger Rasterschritt. */
    int nextPercent() const noexcept;
    int previousPercent() const noexcept;

    /** Fenstergroesse fuer einen Prozentwert. */
    static juce::Rectangle<int> windowBoundsFor (int percent);

    /**
     * true, wenn das Layout bei dieser Groesse lesbar bleibt:
     * Mindestbreite 640 px und Mindesthoehe 400 px bei 100 %,
     * skaliert mit dem Faktor, und beide Achsen muessen ganzzahlig
     * mindestens 320 px behalten.
     */
    static bool isReadable (int percent);

    /** Setzt Transformation und Groesse an einer Wurzelkomponente. */
    void applyTo (juce::Component& root) const;

    /** Merkt sich den Faktor in einer Komponente (Repaint). */
    void applyTo (juce::Component& root, int percent);

private:
    int currentPercent = kDefaultPercent;
};

} // namespace bfka
