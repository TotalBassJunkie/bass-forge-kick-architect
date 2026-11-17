#include "core/Version.h"

namespace bfka
{

juce::String Version::get()
{
   #ifdef BFKA_VERSION_STRING
    return juce::String (BFKA_VERSION_STRING);
   #else
    return juce::String ("0.0.0-dev");
   #endif
}

} // namespace bfka
