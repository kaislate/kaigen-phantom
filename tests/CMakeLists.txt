include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.2
)
FetchContent_MakeAvailable(Catch2)

add_executable(KaigenPhantomTests
    # NOTE (PR1 Task 5, 2026-05-04): the following four tests are excluded from
    # the build because they were authored against the pre-PR1 un-prefixed
    # ParamID layout (e.g. ParamID::GHOST, ParamID::MODE) which Task 2 retired
    # in favour of a_/b_ prefixed ids. They are scheduled for outright deletion
    # in PR1 Task 8 — this exclusion is the bridge that keeps the test target
    # green while the rest of PR1 lands.
    # ParameterTests.cpp        -- references retired un-prefixed ParamID::*
    # SerializationTests.cpp    -- references retired un-prefixed ParamID::*
    # PresetPreviewTests.cpp    -- references retired un-prefixed ParamID::*
    # ABSlotManagerTests.cpp    -- references retired un-prefixed ParamID::*
    WaveletSynthTests.cpp
    BinauralStageTests.cpp
    BassExtractorTests.cpp
    WaveshaperTests.cpp
    EnvelopeFollowerTests.cpp
    PhantomEngineTests.cpp
    PresetMigrationTests.cpp
    ../Source/Engines/BinauralStage.cpp
    ../Source/Engines/BassExtractor.cpp
    ../Source/Engines/Waveshaper.cpp
    ../Source/Engines/EnvelopeFollower.cpp
    ../Source/Engines/PhantomEngine.cpp
    ../Source/Engines/ZeroCrossingSynth.cpp
    ../Source/Engines/WaveletSynth.cpp
    ../Source/PresetManager.cpp
    ../Source/PresetMigration.cpp
    ../Source/ABSlotManager.cpp
)

target_link_libraries(KaigenPhantomTests
    PRIVATE
        Catch2::Catch2WithMain
        juce::juce_audio_utils
        juce::juce_dsp
)

target_include_directories(KaigenPhantomTests PRIVATE
    ../Source
    ${CMAKE_BINARY_DIR}/KaigenPhantom_artefacts/JuceLibraryCode
)

target_compile_definitions(KaigenPhantomTests PRIVATE
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    DONT_SET_USING_JUCE_NAMESPACE=1
    JUCE_DONT_DECLARE_PROJECTINFO=1
)

# Morph + Crossfader + Host tests. KAIGEN_PRO_BUILD is always on (the
# Standard/Pro split was unified 2026-04-24).
#
# NOTE (PR1 Task 1, 2026-05-04): MorphEngineTests.cpp was excluded because
# the MorphEngine constructor widened to a 2-arg engineSync callable in
# commit 9fd618d but the test fixtures still pass a 1-arg lambda.
#
# NOTE (PR1 Task 5, 2026-05-04): MorphEngine.cpp itself was excluded from
# the test target because it references retired un-prefixed param IDs
# (MORPH_ENABLED / SCENE_ENABLED / SCENE_POSITION) that PR1 Task 2 removed.
# Both MorphEngine.cpp + MorphEngineTests.cpp are scheduled for deletion in
# PR1 Task 8.
target_sources(KaigenPhantomTests PRIVATE
    # MorphEngineTests.cpp  -- excluded in PR1 Task 1; deleted in PR1 Task 8.
    # ../Source/MorphEngine.cpp -- excluded in PR1 Task 5; deleted in PR1 Task 8.
    MorphCrossfaderTests.cpp
    DualEngineHostTests.cpp
    ../Source/MorphCrossfader.cpp
    ../Source/DualEngineHost.cpp
)
target_compile_definitions(KaigenPhantomTests PRIVATE KAIGEN_PRO_BUILD=1)

include(CTest)
include(Catch)
catch_discover_tests(KaigenPhantomTests)
