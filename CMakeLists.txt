cmake_minimum_required(VERSION 3.22)
project(KaigenPhantom VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Preset system developer mode
option(DEVELOPER_MODE "Enable developer preset management panel" OFF)

if(DEVELOPER_MODE)
    add_definitions(-DDEVELOPER_MODE=1)
else()
    add_definitions(-DDEVELOPER_MODE=0)
endif()

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
    COPY_PLUGIN_AFTER_BUILD   TRUE
    VST3_COPY_DIR             "$ENV{LOCALAPPDATA}/Programs/Common/VST3"
    VST3_CATEGORIES           "Fx" "Tools"
)

juce_generate_juce_header(KaigenPhantom)

target_sources(KaigenPhantom PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/Theme.cpp
    Source/UI/PhantomLookAndFeel.cpp
    Source/UI/NativePluginEditor.cpp
    Source/UI/widgets/PhantomKnob.cpp
    Source/UI/widgets/PhantomMiniKnob.cpp
    Source/UI/widgets/ToggleGroup.cpp
    Source/UI/widgets/WordSelector.cpp
    Source/UI/widgets/EtchedToggle.cpp
    Source/UI/widgets/ChoiceToggle.cpp
    Source/UI/widgets/EyeButton.cpp
    Source/UI/widgets/GifPlayer.cpp
    Source/ThirdParty/stb_image_impl.cpp
    Source/UI/widgets/PitchDisplay.cpp
    Source/UI/widgets/LevelReadout.cpp
    Source/UI/widgets/RecipeSlotPills.cpp
    Source/UI/widgets/LinkButton.cpp
    Source/UI/widgets/IOMeter.cpp
    Source/UI/widgets/RecipeWheel.cpp
    Source/UI/widgets/PresetSelector.cpp
    Source/UI/widgets/PresetDropdown.cpp
    Source/UI/widgets/EngineTabsWidget.cpp
    Source/UI/widgets/ModeTogglePill.cpp
    Source/UI/widgets/HeaderButton.cpp
    Source/UI/widgets/BuildTagPill.cpp
    Source/UI/widgets/ModSlot.cpp
    Source/UI/widgets/MatrixModRow.cpp
    Source/UI/widgets/MatrixCell.cpp
    Source/UI/visualizers/Oscilloscope.cpp
    Source/UI/visualizers/Spectrum.cpp
    Source/UI/panels/RightPanel.cpp
    Source/UI/panels/LeftPanel.cpp
    Source/UI/panels/PresetBrowser.cpp
    Source/UI/panels/TopBar.cpp
    Source/UI/panels/ModulationPanel.cpp
    Source/UI/panels/MatrixView.cpp
    Source/UI/panels/SettingsOverlay.cpp
    Source/Engines/BinauralStage.cpp
    Source/Engines/BassExtractor.cpp
    Source/Engines/ZeroCrossingSynth.cpp
    Source/Engines/WaveletSynth.cpp
    Source/Engines/EnvelopeFollower.cpp
    Source/Engines/PhantomEngine.cpp
    Source/Pack/PackArchive.cpp
    Source/PresetManager.cpp
    Source/PresetMigration.cpp
    Source/MorphCrossfader.cpp
    Source/DualEngineHost.cpp
    Source/Modulation/Routing.cpp
    Source/Modulation/Macro.cpp
    Source/Modulation/ModulationEngine.cpp
    Source/DSP/SingleKnobReverb.cpp
)

juce_add_binary_data(PhantomWebUI SOURCES
    Source/WebUI/index.html
    Source/WebUI/styles.css
    Source/WebUI/matrix.css
    Source/WebUI/knob.js
    Source/WebUI/knob-mini.js
    Source/WebUI/phantom.js
    Source/WebUI/spectrum.js
    Source/WebUI/recipe-wheel.js
    Source/WebUI/oscilloscope.js
    Source/WebUI/circuit-board.js
    Source/WebUI/juce-frontend.js
    Source/WebUI/preset-spectrum.js
    Source/WebUI/preset-system.js
    Source/WebUI/morph.js
    Source/WebUI/modulation-panel.js
    Source/WebUI/live-modulation.js
    Source/WebUI/matrix.js
)

juce_add_binary_data(PhantomNativeAssets
    HEADER_NAME PhantomNativeAssets.h
    NAMESPACE   PhantomNativeAssets
    SOURCES
        Source/Assets/svg/knob_large.svg
        Source/Assets/svg/knob_medium.svg
        Source/Assets/svg/knob_small.svg
        Source/Assets/svg/knob_mini.svg
        Source/Assets/svg/link_icon.svg
        Source/Assets/fonts/SpaceGrotesk-Light.ttf
        Source/Assets/fonts/SpaceGrotesk-Regular.ttf
        Source/Assets/fonts/SpaceGrotesk-Medium.ttf
        Source/Assets/fonts/SpaceGrotesk-SemiBold.ttf
        Source/Assets/fonts/SpaceGrotesk-Bold.ttf
)

# Factory packs — drop pack folders under Source/FactoryPacks/ and they
# get embedded as binary data, appearing as read-only packs in the
# preset browser. To add a designer-authored pack: unzip the .kaipack
# into Source/FactoryPacks/<pack-name>/ and rebuild.
#
# juce_add_binary_data uses only the basename of each input file for the
# originalFilenames[] entry, so we mangle "<PackName>/<file>" into
# "<PackName>__<file>" at stage-time. PresetManager::loadFactoryPacksFromBinaryData
# splits on "__" to recover the pack name.
set(FACTORY_PACK_SRC_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/Source/FactoryPacks")
set(FACTORY_PACK_STAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/factory_pack_stage")
file(MAKE_DIRECTORY "${FACTORY_PACK_STAGE_DIR}")
file(GLOB_RECURSE FACTORY_PACK_FILES
     CONFIGURE_DEPENDS
     "${FACTORY_PACK_SRC_DIR}/*")
list(FILTER FACTORY_PACK_FILES EXCLUDE REGEX "/\\.gitkeep$")
set(STAGED_FACTORY_PACK_FILES "")
foreach(src ${FACTORY_PACK_FILES})
    file(RELATIVE_PATH rel "${FACTORY_PACK_SRC_DIR}" "${src}")
    string(REGEX REPLACE "/.*" "" first_segment "${rel}")
    string(FIND "${first_segment}" "__" double_underscore_hit)
    if(NOT double_underscore_hit EQUAL -1)
        message(FATAL_ERROR
            "Factory pack folder name '${first_segment}' contains '__', which "
            "conflicts with the juce_add_binary_data staging convention. "
            "Rename the pack folder under Source/FactoryPacks/.")
    endif()
    string(REPLACE "/" "__" mangled "${rel}")
    set(staged "${FACTORY_PACK_STAGE_DIR}/${mangled}")
    configure_file("${src}" "${staged}" COPYONLY)
    list(APPEND STAGED_FACTORY_PACK_FILES "${staged}")
endforeach()
if(STAGED_FACTORY_PACK_FILES)
    juce_add_binary_data(KaigenFactoryPacks
        HEADER_NAME KaigenFactoryPacks.h
        NAMESPACE   KaigenFactoryPacks
        SOURCES     ${STAGED_FACTORY_PACK_FILES})
    target_link_libraries(KaigenPhantom PRIVATE KaigenFactoryPacks)
    target_compile_definitions(KaigenPhantom PRIVATE KAIGEN_HAS_FACTORY_PACKS=1)
else()
    target_compile_definitions(KaigenPhantom PRIVATE KAIGEN_HAS_FACTORY_PACKS=0)
endif()

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
    # Enable JUCE 8's Direct2D renderer on Windows. Produces softer ClearType
    # text + smoother sub-pixel positioning, matching what Chromium gives the
    # webview side. No effect on non-Windows builds.
    JUCE_DIRECT2D=1
)

target_link_libraries(KaigenPhantom
    PRIVATE
        juce::juce_audio_utils
        juce::juce_gui_extra
        juce::juce_dsp
        PhantomWebUI
        PhantomNativeAssets
        "${CMAKE_BINARY_DIR}/webview2/build/native/x64/WebView2LoaderStatic.lib"
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

add_subdirectory(tests)
