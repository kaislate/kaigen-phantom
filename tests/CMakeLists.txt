include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.2
)
FetchContent_MakeAvailable(Catch2)

# Strip the parent CMakeLists' directory-scope DEVELOPER_MODE=0 so the
# per-target target_compile_definitions(DEVELOPER_MODE=1) below wins
# unambiguously. Without this, both definitions land on the test compile
# command line and the later (=0) silently wins on MSVC.
remove_definitions(-DDEVELOPER_MODE=0)

add_executable(KaigenPhantomTests
    WaveletSynthTests.cpp
    BinauralStageTests.cpp
    BassExtractorTests.cpp
    WaveshaperTests.cpp
    EnvelopeFollowerTests.cpp
    PhantomEngineTests.cpp
    PresetMigrationTests.cpp
    MorphCrossfaderTests.cpp
    DualEngineHostTests.cpp
    EditorViewStateTests.cpp
    EngineFocusTests.cpp
    SpectrumViewModeTests.cpp
    MatrixViewStatePersistenceTest.cpp
    RoutingTests.cpp
    ModulatorTests.cpp
    ModulationEngineTests.cpp
    RoutingMutationTests.cpp
    ../Source/Engines/BinauralStage.cpp
    ../Source/Engines/BassExtractor.cpp
    ../Source/Engines/Waveshaper.cpp
    ../Source/Engines/EnvelopeFollower.cpp
    ../Source/Engines/PhantomEngine.cpp
    ../Source/Engines/ZeroCrossingSynth.cpp
    ../Source/Engines/WaveletSynth.cpp
    ../Source/PresetManager.cpp
    ../Source/PresetMigration.cpp
    ../Source/MorphCrossfader.cpp
    ../Source/DualEngineHost.cpp
    ../Source/Modulation/Routing.cpp
    ../Source/Modulation/Macro.cpp
    ../Source/Modulation/ModulationEngine.cpp
    ../Source/Pack/PackArchive.cpp
    PackArchiveTests.cpp
    ../Source/DSP/PhantomSampler.cpp
    PhantomSamplerTests.cpp
)

# Embedded fixture pack used by FactoryPackLoadingTests. This is a TEST-ONLY
# binary-data target — the product target has its own KaigenFactoryPacks
# that is empty by default.
#
# juce_add_binary_data strips path components and uses only basenames for
# the generated originalFilenames[] entries, so we mangle "<PackName>/<file>"
# into "<PackName>__<file>" at stage-time. The C++ side splits on "__" to
# recover the pack name. See loadFactoryPacksFromBinaryData() in
# Source/PresetManager.cpp.
set(FACTORY_PACK_FIXTURE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/FactoryPacksTest)
set(FACTORY_PACK_STAGE_DIR   ${CMAKE_CURRENT_BINARY_DIR}/factory_pack_stage)
file(MAKE_DIRECTORY ${FACTORY_PACK_STAGE_DIR})
set(STAGED_FACTORY_PACK_FILES "")
file(GLOB_RECURSE FIXTURE_PACK_FILES
     CONFIGURE_DEPENDS
     "${FACTORY_PACK_FIXTURE_DIR}/*")
foreach(src ${FIXTURE_PACK_FILES})
    file(RELATIVE_PATH rel ${FACTORY_PACK_FIXTURE_DIR} ${src})
    string(REGEX REPLACE "/.*" "" first_segment "${rel}")
    string(FIND "${first_segment}" "__" double_underscore_hit)
    if(NOT double_underscore_hit EQUAL -1)
        message(FATAL_ERROR
            "Factory pack folder name '${first_segment}' contains '__', which "
            "conflicts with the juce_add_binary_data staging convention. "
            "Rename the pack folder under tests/fixtures/FactoryPacksTest/.")
    endif()
    # rel looks like "TestPack/pack.json"; mangle to "TestPack__pack.json".
    string(REPLACE "/" "__" mangled ${rel})
    set(staged ${FACTORY_PACK_STAGE_DIR}/${mangled})
    configure_file(${src} ${staged} COPYONLY)
    list(APPEND STAGED_FACTORY_PACK_FILES ${staged})
endforeach()

juce_add_binary_data(KaigenFactoryPacks
    HEADER_NAME KaigenFactoryPacks.h
    NAMESPACE   KaigenFactoryPacks
    SOURCES     ${STAGED_FACTORY_PACK_FILES})
target_link_libraries(KaigenPhantomTests PRIVATE KaigenFactoryPacks)
target_compile_definitions(KaigenPhantomTests PRIVATE KAIGEN_HAS_FACTORY_PACKS=1)

# Add FactoryPackLoadingTests.cpp to the test sources.
target_sources(KaigenPhantomTests PRIVATE FactoryPackLoadingTests.cpp)

target_compile_definitions(KaigenPhantomTests PRIVATE DEVELOPER_MODE=1)
target_sources(KaigenPhantomTests PRIVATE PresetManagerAuthoringTests.cpp)

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

include(CTest)
include(Catch)
catch_discover_tests(KaigenPhantomTests)
