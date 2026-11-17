include(FetchContent)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(Catch2)

juce_add_console_app(BFKA_Tests PRODUCT_NAME "BFKA Tests")

target_sources(BFKA_Tests PRIVATE
    SmokeTest.cpp
    ParameterRegistryTests.cpp
    KickStateTests.cpp
    TestMain.cpp
    ../src/core/Version.cpp
    ../src/core/Log.cpp
    ../src/core/ParameterRegistry.cpp
    ../src/core/KickState.cpp
    )

target_include_directories(BFKA_Tests PRIVATE ../src)

target_compile_definitions(BFKA_Tests PRIVATE
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    BFKA_VERSION_STRING="${PROJECT_VERSION}"
    )

target_link_libraries(BFKA_Tests PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    Catch2::Catch2WithMain
    )

target_compile_features(BFKA_Tests PRIVATE cxx_std_20)

if (MSVC)
    target_compile_options(BFKA_Tests PRIVATE /MP /bigobj)
endif()

add_test(NAME BFKA_UnitTests COMMAND BFKA_Tests)
