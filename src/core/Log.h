#pragma once

#include <juce_core/juce_core.h>

/**
 * Schlankes Logging.

 * Regeln: Der Audio-Thread loggt nie. Diagnose laeuft ueber `Diagnostics`, das
 * nur schreibt, wenn es freigeschaltet ist.
 */
namespace bfka::log
{

enum class Level { debug, info, warn, error };

void write (Level level, const juce::String& message);

inline void d (const juce::String& m) { write (Level::debug, m); }
inline void i (const juce::String& m) { write (Level::info,  m); }
inline void w (const juce::String& m) { write (Level::warn,  m); }
inline void e (const juce::String& m) { write (Level::error, m); }

/** Schaltet Debug-Ausgaben frei (Standard: nur in Debug-Builds). */
void setDebugEnabled (bool shouldBeEnabled);
bool isDebugEnabled();

} // namespace bfka::log
