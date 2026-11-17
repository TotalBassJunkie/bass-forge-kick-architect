#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "core/Version.h"

using namespace bfka;

TEST_CASE ("Metadaten sind gesetzt", "[meta]")
{
    REQUIRE (juce::String (Version::productName).isNotEmpty());
    REQUIRE (juce::String (Version::presetExt) == ".bfkapreset");
    REQUIRE (juce::String (Version::bundleId) == "com.bassforge.kickarchitect");
    REQUIRE (Version::get().isNotEmpty());
}
