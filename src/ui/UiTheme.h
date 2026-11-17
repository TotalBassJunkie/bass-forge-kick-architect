#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace bfka::ui
{

/**
 * Farben, Schriften und Masse der FX-Seite (Ticket B-90).
 *
 * Die Werte waren bisher als anonyme Konstanten in `PluginEditor.cpp`
 * verstreut; hier stehen sie einmal, damit KICK-Seite, FX-Seite und Karten
 * dieselbe Optik benutzen.
 */
namespace theme
{
    // --- Farben ------------------------------------------------------------
    inline const juce::Colour background { 0xff14161a };
    inline const juce::Colour panel      { 0xff1d2026 };
    inline const juce::Colour panelAlt   { 0xff23272f };
    inline const juce::Colour panelDeep  { 0xff111317 };
    inline const juce::Colour accent     { 0xffff7a2f };
    inline const juce::Colour accentDim  { 0xffff7a2f };
    inline const juce::Colour text       { 0xffe8eaed };
    inline const juce::Colour textDim    { 0xff8b919b };
    inline const juce::Colour ok         { 0xff56c271 };
    inline const juce::Colour warn       { 0xffe0b44a };
    inline const juce::Colour error      { 0xffe05a5a };
    inline const juce::Colour grid       { 0xff2c313a };

    /** Farben fuer die drei OTT-Baender bzw. Modul-Akzente. */
    inline const juce::Colour bandLow    { 0xff4a90d9 };
    inline const juce::Colour bandMid    { 0xff56c271 };
    inline const juce::Colour bandHigh   { 0xffe0b44a };

    // --- Masse -------------------------------------------------------------
    inline constexpr int sidebarWidth   = 210;
    inline constexpr int toolbarHeight  = 32;
    inline constexpr int cardHeaderH    = 26;
    inline constexpr int cardGap        = 8;
    inline constexpr int cardPadding    = 8;
    inline constexpr int knobSize       = 54;
    inline constexpr int knobCols       = 6;
    inline constexpr int knobRowHeight  = 72;

    // --- Schriften ---------------------------------------------------------
    inline juce::Font titleFont()      { return juce::Font (juce::FontOptions (24.0f).withStyle ("Bold")); }
    inline juce::Font sectionFont()    { return juce::Font (juce::FontOptions (12.0f).withStyle ("Bold")); }
    inline juce::Font cardTitleFont()  { return juce::Font (juce::FontOptions (13.0f).withStyle ("Bold")); }
    inline juce::Font bodyFont()       { return juce::Font (juce::FontOptions (12.0f)); }
    inline juce::Font smallFont()      { return juce::Font (juce::FontOptions (10.5f)); }

    /** Hintergrund mit Panel-Optik (Rundung + dezenter Rahmen). */
    inline void paintPanel (juce::Graphics& g, juce::Rectangle<float> area,
                            juce::Colour fill, float corner = 8.0f)
    {
        g.setColour (fill);
        g.fillRoundedRectangle (area, corner);
        g.setColour (grid);
        g.drawRoundedRectangle (area, corner, 1.0f);
    }

    /** Logarithmische Abbildung [minHz, maxHz] -> [0, 1]. */
    inline float freqToNorm (double hz, double minHz, double maxHz) noexcept
    {
        const auto lo = std::log (juce::jmax (1.0, minHz));
        const auto hi = std::log (juce::jmax (lo + 1.0, maxHz));
        const auto v  = std::log (juce::jlimit (minHz, maxHz, hz));

        return (float) ((v - lo) / (hi - lo));
    }

    /** Umkehrung von freqToNorm(). */
    inline double normToFreq (float norm, double minHz, double maxHz) noexcept
    {
        const auto lo = std::log (juce::jmax (1.0, minHz));
        const auto hi = std::log (juce::jmax (lo + 1.0, maxHz));

        return std::exp (lo + juce::jlimit (0.0f, 1.0f, norm) * (float) (hi - lo));
    }
} // namespace theme

} // namespace bfka::ui
