include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.2
)
FetchContent_MakeAvailable(Catch2)

add_executable(KaigenPhantomTests
    ParameterTests.cpp
    WaveletSynthTests.cpp
    BinauralStageTests.cpp
    SerializationTests.cpp
    BassExtractorTests.cpp
    WaveshaperTests.cpp
    EnvelopeFollowerTests.cpp
    PhantomEngineTests.cpp
    PresetPreviewTests.cpp
    ABSlotManagerTests.cpp
    ../Source/Engines/BinauralStage.cpp
    ../Source/Engines/BassExtractor.cpp
    ../Source/Engines/Waveshaper.cpp
    ../Source/Engines/EnvelopeFollower.cpp
    ../Source/Engines/PhantomEngine.cpp
    ../Source/Engines/ZeroCrossingSynth.cpp
    ../Source/Engines/WaveletSynth.cpp
    ../Source/PresetManager.cpp
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

if(KAIGEN_PRO_BUILD)
    # MorphEngineTests.cpp is created in Task 5; guard avoids configure error until then.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/MorphEngineTests.cpp")
        target_sources(KaigenPhantomTests PRIVATE
            MorphEngineTests.cpp
        )
    endif()
    target_sources(KaigenPhantomTests PRIVATE
        ../Source/MorphEngine.cpp
    )
    target_compile_definitions(KaigenPhantomTests PRIVATE KAIGEN_PRO_BUILD=1)
endif()

include(CTest)
include(Catch)
catch_discover_tests(KaigenPhantomTests)
