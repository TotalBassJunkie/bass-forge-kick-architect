cmake_minimum_required(VERSION 3.22)

project(BassForgeKickArchitect
        VERSION 0.1.0
        DESCRIPTION "Bass Forge Kick Architect - techno kick builder (VST3 + Standalone)"
        LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# ---------------------------------------------------------------------------
# Abhaengigkeiten
# ---------------------------------------------------------------------------
include(FetchContent)

# Optionale Offline-Nutzung: liegt JUCE bereits in ../JUCE oder in _extern/JUCE,
# wird dieser Baum verwendet statt erneut zu laden.
set(BFKA_JUCE_SOURCE_DIR "" CACHE PATH "Vorhandener JUCE-Baum (leer = automatisch holen)")

if (NOT BFKA_JUCE_SOURCE_DIR)
    foreach(_candidate "${CMAKE_CURRENT_SOURCE_DIR}/_extern/JUCE" "${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
        if (EXISTS "${_candidate}/CMakeLists.txt")
            set(BFKA_JUCE_SOURCE_DIR "${_candidate}")
            break()
        endif()
    endforeach()
endif()

if (BFKA_JUCE_SOURCE_DIR)
    message(STATUS "BFKA: verwende vorhandenes JUCE unter ${BFKA_JUCE_SOURCE_DIR}")
    add_subdirectory("${BFKA_JUCE_SOURCE_DIR}" JUCE EXCLUDE_FROM_ALL)
else()
    message(STATUS "BFKA: hole JUCE 8.0.7 via FetchContent")
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.7
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# Quellen (bewusst explizit statt GLOB: Build muss reproduzierbar bleiben)
# ---------------------------------------------------------------------------
set(BFKA_CORE_SOURCES
    src/core/Version.cpp
    src/core/Log.cpp
    src/core/ParameterRegistry.cpp
    src/core/KickState.cpp
    src/core/FxChainIO.cpp
    )

set(BFKA_ENGINE_SOURCES
    src/engine/SampleVoice.cpp
    src/engine/KickRenderer.cpp
    src/engine/KickAssembler.cpp
    src/engine/WsolaStretcher.cpp
    )

set(BFKA_FX_SOURCES
    src/fx/FxFactory.cpp
    src/fx/FxChainProcessor.cpp
    src/fx/modules/ConvolverModule.cpp
    src/fx/modules/DistortionModule.cpp
    src/fx/modules/DynamicEqModule.cpp
    src/fx/modules/EqualizerModule.cpp
    src/fx/modules/FiltersModule.cpp
    src/fx/modules/MultibandModule.cpp
    src/fx/modules/TransientShaperModule.cpp
    )

set(BFKA_LIBRARY_SOURCES
    src/library/SampleLibrary.cpp
    src/library/StyleModel.cpp
    src/library/RandomizerEngine.cpp
    )

set(BFKA_UI_SOURCES
    src/ui/FxChainModel.cpp
    src/ui/ModuleStructureView.cpp
    src/ui/ModuleCard.cpp
    src/ui/FxPage.cpp
    src/ui/editors/MultibandView.cpp
    src/ui/editors/ConvolverView.cpp
    src/ui/editors/DynamicEqView.cpp
    )

set(BFKA_PLUGIN_SOURCES
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginEditor.cpp
    )

set(BFKA_HEADERS
    src/core/Version.h
    src/core/Log.h
    src/core/ParameterRegistry.h
    src/core/KickState.h
    src/core/FxChainIO.h
    src/fx/FxModule.h
    src/fx/FxFactory.h
    src/fx/PassThroughModule.h
    src/fx/FxChainProcessor.h
    src/fx/dsp/Biquad.h
    src/fx/modules/ConvolverModule.h
    src/fx/modules/DistortionModule.h
    src/fx/modules/DynamicEqModule.h
    src/fx/modules/EqualizerModule.h
    src/fx/modules/FiltersModule.h
    src/fx/modules/MultibandModule.h
    src/fx/modules/TransientShaperModule.h
    src/engine/SampleVoice.h
    src/engine/KickRenderer.h
    src/engine/KickAssembler.h
    src/engine/WsolaStretcher.h
    src/engine/Resampler.h
    src/library/SampleLibrary.h
    src/library/StyleModel.h
    src/library/RandomizerEngine.h
    src/plugin/PluginProcessor.h
    src/plugin/PluginEditor.h
    src/ui/UiTheme.h
    src/ui/FxChainModel.h
    src/ui/ModuleStructureView.h
    src/ui/ModuleCard.h
    src/ui/FxPage.h
    src/ui/editors/MultibandView.h
    src/ui/editors/ConvolverView.h
    src/ui/editors/DynamicEqView.h
    )

# ---------------------------------------------------------------------------
# Plugin-Target
# ---------------------------------------------------------------------------
juce_add_plugin(BassForgeKickArchitect
    COMPANY_NAME              "Bass Forge"
    BUNDLE_ID                 "com.bassforge.kickarchitect"
    IS_SYNTH                  TRUE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    PLUGIN_MANUFACTURER_CODE  Bsfg
    PLUGIN_CODE               BFKA
    FORMATS                   VST3 Standalone
    PRODUCT_NAME              "Bass Forge Kick Architect"
    VST3_CATEGORIES           Instrument Synth
    AU_MAIN_TYPE              kAudioUnitType_MusicDevice
    VST3_AUTO_MANIFEST        TRUE
    )

target_sources(BassForgeKickArchitect PRIVATE
    ${BFKA_CORE_SOURCES}
    ${BFKA_ENGINE_SOURCES}
    ${BFKA_FX_SOURCES}
    ${BFKA_LIBRARY_SOURCES}
    ${BFKA_UI_SOURCES}
    ${BFKA_PLUGIN_SOURCES}
    ${BFKA_HEADERS}
    )

target_compile_features(BassForgeKickArchitect PUBLIC cxx_std_20)

target_include_directories(BassForgeKickArchitect PRIVATE src)

target_compile_definitions(BassForgeKickArchitect PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    BFKA_VERSION_STRING="${PROJECT_VERSION}"
    )

target_link_libraries(BassForgeKickArchitect
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
    )

if (MSVC)
    target_compile_options(BassForgeKickArchitect PRIVATE /MP /bigobj)
    target_compile_definitions(BassForgeKickArchitect PRIVATE _USE_MATH_DEFINES NOMINMAX)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
option(BFKA_BUILD_TESTS "Unit-Tests bauen" ON)

if (BFKA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
