#pragma once

#include <juce_core/juce_core.h>

namespace bfka
{

/** Produkt- und Build-Metadaten. */
struct Version
{
    static constexpr const char* productName    = "Bass Forge Kick Architect";
    static constexpr const char* shortName      = "BFKA";
    static constexpr const char* companyName    = "Bass Forge";
    static constexpr const char* bundleId       = "com.bassforge.kickarchitect";
    static constexpr const char* presetExt      = ".bfkapreset";
    static constexpr const char* presetWildcard = "*.bfkapreset";

    /** Build-Version, zur Compile-Zeit gesetzt (BFKA_VERSION_STRING). */
    static juce::String get();
};

} // namespace bfka
