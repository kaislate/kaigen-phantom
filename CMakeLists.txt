cmake_minimum_required(VERSION 3.22)
project(KaigenPhantom VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
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

juce_add_binary_data(PhantomWebUI SOURCES
    Source/WebUI/index.html
    Source/WebUI/styles.css
    Source/WebUI/knob.js
    Source/WebUI/phantom.js
    Source/WebUI/spectrum.js
    Source/WebUI/recipe-wheel.js
)

target_include_directories(KaigenPhantom PRIVATE Source)

# Fetch WebView2 SDK for Windows
set(WEBVIEW2_DIR "${CMAKE_BINARY_DIR}/webview2")
if(NOT EXISTS "${WEBVIEW2_DIR}/build/native/include/WebView2.h")
    file(DOWNLOAD
        "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.2739.15"
        "${WEBVIEW2_DIR}/webview2.zip"
        STATUS DL_STATUS)
    file(ARCHIVE_EXTRACT INPUT "${WEBVIEW2_DIR}/webview2.zip"
         DESTINATION "${WEBVIEW2_DIR}")
endif()
target_include_directories(KaigenPhantom PRIVATE
    "${WEBVIEW2_DIR}/build/native/include")

target_compile_definitions(KaigenPhantom PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(KaigenPhantom
    PRIVATE
        juce::juce_audio_utils
        juce::juce_gui_extra
        juce::juce_dsp
        PhantomWebUI
        "${CMAKE_BINARY_DIR}/webview2/build/native/x64/WebView2LoaderStatic.lib"
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

add_subdirectory(tests)
