// B-82: Linke Style-Sidebar der KICK-Seite.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

#include "library/ChaosEngine.h"

namespace bfka
{

class StyleModel;
class UserStyles;

/**
 * Linke Style-Sidebar der KICK-Seite (Ticket B-82).
 * Zeigt Factory-Styles (StyleModel) und eigene Styles (UserStyles),
 * dazu FREE BUILD, ROLL, CHAOS-Schalter und den Roll-Zaehler.
 * Breite 210 px; nur Message-Thread.
 */
class StyleSidebar final : public juce::Component
{
public:
    StyleSidebar();
    ~StyleSidebar() override;

    /** Beide Zeiger duerfen nullptr sein (dann nur FREE BUILD). */
    void setModel (const StyleModel* model, const UserStyles* userStyles);

    void setActiveStyle (const juce::String& styleId);
    juce::String activeStyle() const;

    void setAccentFor (juce::Colour accent);
    juce::Colour accent() const;

    /** Roll-Zaehler (Anzeige "ROLLS 3"). */
    void setRollCount (int count);
    int  rollCount() const noexcept;

    /** Anzahl der Zeilen in der Liste inkl. FREE BUILD. */
    int rowCount() const;
    /** Anzeigename der Zeile (Diagnose/Test). */
    juce::String rowName (int row) const;
    /** Style-Id der Zeile; "" bei FREE BUILD. */
    juce::String rowStyleId (int row) const;

    int preferredWidth() const noexcept;   ///< 210

    std::function<void (const juce::String& styleId)> onStyleSelected;   // "" = FREE BUILD
    std::function<void()>                   onRandomizeRequested;
    std::function<void (ChaosFlags)>         onChaosToggled;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class StyleListModel;
    std::unique_ptr<StyleListModel> listModel;
    juce::ListBox        list { "Styles", nullptr };
    juce::TextButton     rollButton   { "ROLL" };
    juce::TextButton     addStyleButton { "+ ADD STYLE" };
    juce::TextButton     chaosPitch   { "PITCH" };
    juce::TextButton     chaosStretch { "STRETCH" };
    juce::TextButton     chaosChain   { "CHAIN" };
    juce::TextButton     chaosModules { "MODULES" };
    std::vector<juce::String> styleIds, styleNames;
    juce::String activeId;
    juce::Colour accentColour { 0xffe0663a };
    int rollCounter = 0;
    ChaosFlags chaosFlags;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StyleSidebar)
};

} // namespace bfka
