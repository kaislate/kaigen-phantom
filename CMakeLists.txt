cmake_minimum_required(VERSION 3.22)
project(KaigenPhantom VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.9
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(KaigenPhantom
    COMPANY_NAME              "Kaigen"
    PLUGIN_MANUFACTURER_CODE  Kgna
    PLUGIN_CODE               Kgph
    FORMATS                   VST3 Standalone
    PRODUCT_NAME              "Kaigen Phantom"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           "Fx" "Tools"
)

juce_generate_juce_header(KaigenPhantom)

target_sources(KaigenPhantom PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Engines/PitchTracker.cpp
    Source/Engines/HarmonicGenerator.cpp
    Source/Engines/BinauralStage.cpp
    Source/Engines/PerceptualOptimizer.cpp
    Source/Engines/CrossoverBlend.cpp
    Source/Engines/Deconfliction/PartitionStrategy.cpp
    Source/Engines/Deconfliction/SpectralLaneStrategy.cpp
    Source/Engines/Deconfliction/StaggerStrategy.cpp
    Source/Engines/Deconfliction/OddEvenStrategy.cpp
    Source/Engines/Deconfliction/ResidueStrategy.cpp
    Source/Engines/Deconfliction/BinauralStrategy.cpp
)

target_include_directories(KaigenPhantom PRIVATE Source)

target_compile_definitions(KaigenPhantom PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(KaigenPhantom
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

add_subdirectory(tests)
