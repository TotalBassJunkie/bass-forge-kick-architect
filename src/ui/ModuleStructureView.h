#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

#include "plugin/PluginProcessor.h"

namespace bfka
{

/**
 * Struktur-Editor einer Modulkarte (Ticket B-92).
 *
 * Ein Struktur-Editor zeigt und bearbeitet, was **nicht** als Skalarparam
 * existiert: Crossover-Grenzen, EQ-/Decay-Kurven, IR-Pfad, Bandknoten. Er
 * schreibt immer zuerst in das Modul (live) und ruft danach
 * `PluginProcessor::writeBackModuleStates()`, damit der Zustand mitkommt.
 *
 * Der Rahmen (Titel, Flaeche, Raster) wird hier gezeichnet; die Unterklasse
 * fuellt nur `paintContent()` und reagiert auf Maus/Timer.
 *
 * Threading: nur Message-Thread (`refresh()` kommt vom Timer der FX-Seite).
 */
class ModuleStructureView : public juce::Component
{
public:
    ModuleStructureView (PluginProcessor& processor, int slotIndex);
    ~ModuleStructureView() override = default;

    /** Vom Timer der Seite aufgerufen (ca. 15 Hz). */
    virtual void refresh() {}

    /** Der Kettenplatz kann sich nach einem Rebuild geaendert haben. */
    int  slot() const noexcept { return moduleSlot; }
    void setSlot (int newSlot) noexcept { moduleSlot = newSlot; }

    /** Standardhoehe dieser Ansicht (Karte fragt danach). */
    virtual int preferredHeight() const = 0;

    void paint (juce::Graphics& g) override;

protected:
    /** Zeichnet den Inhalt innerhalb von `contentArea` (ohne Rahmen). */
    virtual void paintContent (juce::Graphics& g, juce::Rectangle<int> contentArea) = 0;

    /** Flaeche im Look des Themas mit Beschriftung oben links. */
    static void paintTitle (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text);

    // --- Parameter-Zugriff in Anzeige-Einheiten ---------------------------

    /** Index des Parameters `id` im Modul an diesem Kettenplatz (-1 = unbekannt). */
    int   findParamIndex (const juce::String& id) const;
    /** Aktueller Wert in Anzeige-Einheiten (0, wenn unbekannt). */
    float getParamValue (const juce::String& id) const;
    /** Setzt einen Parameter live (ohne Rebuild) und schreibt ihn in den Zustand. */
    void  setParamValue (const juce::String& id, float value);

    /** Modulzeiger als `T*` (nullptr, wenn der Platz leer/falscher Typ ist). */
    template <typename T>
    T* moduleAs() const
    {
        return dynamic_cast<T*> (proc.getModule (moduleSlot));
    }

    PluginProcessor& proc;
    int              moduleSlot = 0;
};

/**
 * Erzeugt den passenden Struktur-Editor fuer einen Kettenplatz.
 *
 * @returns `nullptr` fuer Module ohne Struktur (die Karte zeigt dann nur
 *          Parameter und Wellenform).
 */
std::unique_ptr<ModuleStructureView> createStructureView (PluginProcessor& processor, int slotIndex);

} // namespace bfka
