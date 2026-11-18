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
    FxChainTests.cpp
    SampleLibraryFileNameTests.cpp
    StyleModelTests.cpp
    RandomizerEngineTests.cpp
    VoiceAndRenderTests.cpp
    WsolaTests.cpp
    DistortionModuleTests.cpp
    EqualizerModuleTests.cpp
    FiltersModuleTests.cpp
    FinalizerModuleTests.cpp
    TransientShaperModuleTests.cpp
    MultibandModuleTests.cpp
    ConvolverModuleTests.cpp
    DynamicEqModuleTests.cpp
    KickShaperModuleTests.cpp
    MidSideEqModuleTests.cpp
    KickAssemblerTests.cpp
    FxChainIOTests.cpp
    PluginProcessorStateTests.cpp
    ModuleStateRoundtripTests.cpp
    FxChainModelTests.cpp
    FxPageTests.cpp
    FxDemoChainTests.cpp
    SidechainTests.cpp
    RenderLengthTests.cpp
    SmoothingTests.cpp
    CpuProfileTests.cpp
    PresetIOTests.cpp
    StructureViewTests.cpp
    KickPageTests.cpp
    TailLengthTests.cpp
    ModuleCardLayoutTests.cpp
    ImpulseLibraryTests.cpp
    PresetEmbedTests.cpp
    PitchStretchTests.cpp
    PitchShifterTests.cpp
    PreRenderTests.cpp
    QualityMetricsTests.cpp
    UndoHistoryTests.cpp
    ChaosEngineTests.cpp
    DialogHostTests.cpp
    OnsetDetectorTests.cpp
    AnalysisCacheTests.cpp
    ExportIOTests.cpp
    OfflineRenderTests.cpp
    KnobHelpersTests.cpp
    KeyDetectorTests.cpp
    AnalysisJobTests.cpp
    KeyChipTests.cpp
    PhaseAlignTests.cpp
    KeyTrackTests.cpp
    StyleIOTests.cpp
    RobustnessTests.cpp
    StyleSidebarTests.cpp
    LibraryOverlayTests.cpp
    AlignEngineTests.cpp
    PrefsTests.cpp
    PresetManagerTests.cpp
    AddPartTests.cpp
    AlignReportTests.cpp
    MidiModeTests.cpp
    UiScalerTests.cpp
    ParityTests.cpp
    StateMigrationTests.cpp
    FollowDawTests.cpp
    AlignStretchTests.cpp
    TestMain.cpp
    ../src/core/Version.cpp
    ../src/core/Log.cpp
    ../src/core/ParameterRegistry.cpp
    ../src/core/KickState.cpp
    ../src/core/FxChainIO.cpp
    ../src/core/PresetIO.cpp
    ../src/core/UndoHistory.cpp
    ../src/engine/PitchShifter.cpp
    ../src/engine/PreRenderCache.cpp
    ../src/library/ChaosEngine.cpp
    ../src/library/analysis/OnsetDetector.cpp
    ../src/library/analysis/AnalysisCache.cpp
    ../src/ui/overlays/DialogHost.cpp
    ../src/core/ExportNaming.cpp
    ../src/core/WavWriter.cpp
    ../src/core/StyleIO.cpp
    ../src/engine/OfflineRenderer.cpp
    ../src/engine/PhaseAlign.cpp
    ../src/engine/KeyState.cpp
    ../src/engine/KeyTracker.cpp
    ../src/library/UserStyles.cpp
    ../src/library/analysis/KeyDetector.cpp
    ../src/library/analysis/AnalysisJob.cpp
    ../src/ui/KnobHelpers.cpp
    ../src/ui/KeyChip.cpp
    ../src/core/Prefs.cpp
    ../src/engine/AlignEngine.cpp
    ../src/ui/StyleSidebar.cpp
    ../src/ui/overlays/LibraryOverlay.cpp
    ../src/ui/overlays/SettingsOverlay.cpp
    ../src/ui/overlays/PresetManagerOverlay.cpp
    ../src/ui/overlays/AddPartOverlay.cpp
    ../src/engine/AlignReport.cpp
    ../src/engine/MidiModes.cpp
    ../src/engine/ParityHarness.cpp
    ../src/ui/UiScaler.cpp
    ../src/ui/FxChainModel.cpp
    ../src/ui/ModuleStructureView.cpp
    ../src/ui/ModuleCard.cpp
    ../src/ui/FxPage.cpp
    ../src/ui/SlotCard.cpp
    ../src/ui/KickPage.cpp
    ../src/ui/editors/MultibandView.cpp
    ../src/ui/editors/ConvolverView.cpp
    ../src/ui/editors/DynamicEqView.cpp
    ../src/ui/editors/MidSideEqView.cpp
    ../src/ui/editors/KickShaperView.cpp
    ../src/ui/editors/FinalizerView.cpp
    ../src/fx/FxFactory.cpp
    ../src/fx/FxChainProcessor.cpp
    ../src/fx/modules/ConvolverModule.cpp
    ../src/fx/modules/DistortionModule.cpp
    ../src/fx/modules/DynamicEqModule.cpp
    ../src/fx/modules/EqualizerModule.cpp
    ../src/fx/modules/FiltersModule.cpp
    ../src/fx/modules/FinalizerModule.cpp
    ../src/fx/modules/KickShaperModule.cpp
    ../src/fx/modules/MidSideEqModule.cpp
    ../src/fx/modules/MultibandModule.cpp
    ../src/fx/modules/TransientShaperModule.cpp
    ../src/library/SampleLibrary.cpp
    ../src/library/StyleModel.cpp
    ../src/library/RandomizerEngine.cpp
    ../src/library/ImpulseLibrary.cpp
    ../src/engine/SampleVoice.cpp
    ../src/engine/KickRenderer.cpp
    ../src/engine/KickAssembler.cpp
    ../src/engine/WsolaStretcher.cpp
    ../src/plugin/PluginProcessor.cpp
    ../src/plugin/PluginEditor.cpp
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
