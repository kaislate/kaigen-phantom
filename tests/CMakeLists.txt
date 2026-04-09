include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.2
)
FetchContent_MakeAvailable(Catch2)

add_executable(KaigenPhantomTests
    ParameterTests.cpp
    PitchTrackerTests.cpp
    HarmonicGeneratorTests.cpp
    DeconflictionTests.cpp
    BinauralStageTests.cpp
    PerceptualOptimizerTests.cpp
    CrossoverBlendTests.cpp
    SerializationTests.cpp
    ../Source/Engines/PitchTracker.cpp
    ../Source/Engines/HarmonicGenerator.cpp
    ../Source/Engines/BinauralStage.cpp
    ../Source/Engines/PerceptualOptimizer.cpp
    ../Source/Engines/CrossoverBlend.cpp
    ../Source/Engines/Deconfliction/PartitionStrategy.cpp
    ../Source/Engines/Deconfliction/SpectralLaneStrategy.cpp
    ../Source/Engines/Deconfliction/StaggerStrategy.cpp
    ../Source/Engines/Deconfliction/OddEvenStrategy.cpp
    ../Source/Engines/Deconfliction/ResidueStrategy.cpp
    ../Source/Engines/Deconfliction/BinauralStrategy.cpp
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

include(CTest)
include(Catch)
catch_discover_tests(KaigenPhantomTests)
