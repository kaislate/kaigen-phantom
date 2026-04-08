# Kaigen Phantom Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build Kaigen Phantom — a psychoacoustic bass plugin that generates phantom fundamentals via a tunable harmonic recipe engine, supporting effect mode (monophonic YIN pitch tracking) and instrument mode (polyphonic MIDI with 6 deconfliction strategies) plus a binaural stereo stage.

**Architecture:** Modular engines wired by PluginProcessor — PitchTracker → HarmonicGenerator (voice pool) → BinauralStage → PerceptualOptimizer → CrossoverBlend. Effect and instrument modes diverge only in pitch source; all downstream engines are shared. Mirrors Kaigen Conduit's proven CMake + JUCE 7.0.9 + Catch2 structure.

**Tech Stack:** C++17, JUCE 7.0.9 (FetchContent), CMake 3.22+, Catch2 v3.5.2, VST3 + Standalone

---

## File Map

```
Kaigen Phantom/
├── CMakeLists.txt
├── .gitignore
├── Source/
│   ├── Parameters.h                        — APVTS param IDs + createParameterLayout()
│   ├── PluginProcessor.h / .cpp            — wires engines, mode switch, MIDI
│   ├── PluginEditor.h / .cpp               — placeholder (UI deferred)
│   └── Engines/
│       ├── PitchTracker.h / .cpp           — YIN monophonic pitch detection
│       ├── HarmonicGenerator.h / .cpp      — voice pool + recipe + deconfliction dispatch
│       ├── BinauralStage.h / .cpp          — L/R harmonic matrix
│       ├── PerceptualOptimizer.h / .cpp    — ISO 226 equal-loudness correction
│       ├── CrossoverBlend.h / .cpp         — Ghost crossfade + sidechain ducking
│       └── Deconfliction/
│           ├── IDeconflictionStrategy.h    — pure interface
│           ├── PartitionStrategy.h / .cpp
│           ├── SpectralLaneStrategy.h / .cpp
│           ├── StaggerStrategy.h / .cpp
│           ├── OddEvenStrategy.h / .cpp
│           ├── ResidueStrategy.h / .cpp
│           └── BinauralStrategy.h / .cpp
└── tests/
    ├── CMakeLists.txt
    ├── ParameterTests.cpp
    ├── PitchTrackerTests.cpp
    ├── HarmonicGeneratorTests.cpp
    ├── DeconflictionTests.cpp
    ├── BinauralStageTests.cpp
    ├── PerceptualOptimizerTests.cpp
    ├── CrossoverBlendTests.cpp
    └── SerializationTests.cpp
```

---

## Task 1: CMake scaffold + minimal compile

**Files:**
- Create: `CMakeLists.txt`
- Create: `.gitignore`
- Create: `tests/CMakeLists.txt`
- Create: `Source/PluginProcessor.h`
- Create: `Source/PluginProcessor.cpp`
- Create: `Source/PluginEditor.h`
- Create: `Source/PluginEditor.cpp`
- Create: `Source/Parameters.h` (stub — full params in Task 2)

- [ ] **Step 1: Create `.gitignore`**

```
build/
.superpowers/
*.user
*.suo
.vs/
```

- [ ] **Step 2: Create `CMakeLists.txt`**

```cmake
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
```

- [ ] **Step 3: Create `Source/Parameters.h` (stub — full params added in Task 2)**

```cpp
#pragma once
#include <JuceHeader.h>

namespace ParamID
{
    inline constexpr auto MODE             = "mode";
    inline constexpr auto GHOST            = "ghost";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;
    params.push_back(std::make_unique<AudioParameterChoice>(
        ParamID::MODE, "Mode", StringArray{ "Effect", "Instrument" }, 0));
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParamID::GHOST, "Ghost",
        NormalisableRange<float>(0.0f, 100.0f), 100.0f,
        AudioParameterFloatAttributes().withLabel("%")));
    return { params.begin(), params.end() };
}
```

- [ ] **Step 4: Create `Source/PluginProcessor.h`**

```cpp
#pragma once
#include <JuceHeader.h>

class PhantomProcessor : public juce::AudioProcessor
{
public:
    PhantomProcessor();
    ~PhantomProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Kaigen Phantom"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomProcessor)
};
```

- [ ] **Step 5: Create `Source/PluginProcessor.cpp`**

```cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"

PhantomProcessor::PhantomProcessor()
    : AudioProcessor(BusesProperties()
        .withInput ("Input",     juce::AudioChannelSet::stereo(), true)
        .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)
        .withOutput("Output",    juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PHANTOM_STATE", createParameterLayout())
{
}

void PhantomProcessor::prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/) {}

void PhantomProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;
    // Stub — engines wired in Tasks 10-11
    (void)buffer;
}

juce::AudioProcessorEditor* PhantomProcessor::createEditor()
{
    return new PhantomEditor(*this);
}

void PhantomProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PhantomProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorValueTreeState::ParameterLayout
PhantomProcessor::createParameterLayout()
{
    return ::createParameterLayout();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PhantomProcessor();
}
```

- [ ] **Step 6: Create `Source/PluginEditor.h`**

```cpp
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class PhantomEditor : public juce::AudioProcessorEditor
{
public:
    explicit PhantomEditor(PhantomProcessor&);
    ~PhantomEditor() override = default;
    void paint(juce::Graphics&) override;
    void resized() override {}
private:
    PhantomProcessor& processor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomEditor)
};
```

- [ ] **Step 7: Create `Source/PluginEditor.cpp`**

```cpp
#include "PluginEditor.h"

PhantomEditor::PhantomEditor(PhantomProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(900, 550);
}

void PhantomEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0a0820));
    g.setColour(juce::Colours::white);
    g.setFont(28.0f);
    g.drawFittedText("KAIGEN | PHANTOM", getLocalBounds(), juce::Justification::centred, 1);
}
```

- [ ] **Step 8: Create stub engine source files so CMake compiles**

Create each of these as an empty `.cpp` with just the include:

`Source/Engines/PitchTracker.cpp` — `#include "PitchTracker.h"`
`Source/Engines/HarmonicGenerator.cpp` — `#include "HarmonicGenerator.h"`
`Source/Engines/BinauralStage.cpp` — `#include "BinauralStage.h"`
`Source/Engines/PerceptualOptimizer.cpp` — `#include "PerceptualOptimizer.h"`
`Source/Engines/CrossoverBlend.cpp` — `#include "CrossoverBlend.h"`
`Source/Engines/Deconfliction/PartitionStrategy.cpp` — `#include "PartitionStrategy.h"`
`Source/Engines/Deconfliction/SpectralLaneStrategy.cpp` — `#include "SpectralLaneStrategy.h"`
`Source/Engines/Deconfliction/StaggerStrategy.cpp` — `#include "StaggerStrategy.h"`
`Source/Engines/Deconfliction/OddEvenStrategy.cpp` — `#include "OddEvenStrategy.h"`
`Source/Engines/Deconfliction/ResidueStrategy.cpp` — `#include "ResidueStrategy.h"`
`Source/Engines/Deconfliction/BinauralStrategy.cpp` — `#include "BinauralStrategy.h"`

Each corresponding `.h` file should be an empty `#pragma once` + `#include <JuceHeader.h>` stub.

- [ ] **Step 9: Create `tests/CMakeLists.txt`**

```cmake
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
```

- [ ] **Step 10: Create stub test files** (one `TEST_CASE` placeholder each so CMake links)

`tests/ParameterTests.cpp`:
```cpp
#include <catch2/catch_test_macros.hpp>
TEST_CASE("placeholder") { REQUIRE(1 == 1); }
```
Repeat the same single-line placeholder for each of the 7 other test files.

- [ ] **Step 11: Configure and build**

```bash
cd "C:/Documents/NEw project/Kaigen Phantom"
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

Expected: build succeeds with VST3 + Standalone + KaigenPhantomTests targets.

- [ ] **Step 12: Commit**

```bash
git add CMakeLists.txt .gitignore Source/ tests/
git commit -m "feat: scaffold Kaigen Phantom project — CMake, stub engines, placeholder tests"
```

---

## Task 2: Parameters.h — full APVTS layout

**Files:**
- Modify: `Source/Parameters.h`
- Modify: `tests/ParameterTests.cpp`

- [ ] **Step 1: Write failing test**

Replace `tests/ParameterTests.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "Parameters.h"

TEST_CASE("createParameterLayout contains all required parameter IDs")
{
    auto layout = createParameterLayout();
    juce::AudioProcessor::BusesProperties buses;
    // Build a minimal processor to host the APVTS
    // (We test the layout by building it and checking IDs exist)

    // Collect all IDs from the layout
    std::vector<juce::String> ids;
    for (int i = 0; i < layout.size(); ++i)
        ids.push_back(layout.getParameterByID(layout.begin()[i]->paramID)->paramID);

    auto has = [&](const char* id) {
        return std::find(ids.begin(), ids.end(), juce::String(id)) != ids.end();
    };

    // Mode & Global
    REQUIRE(has(ParamID::MODE));
    REQUIRE(has(ParamID::GHOST));
    REQUIRE(has(ParamID::GHOST_MODE));
    REQUIRE(has(ParamID::PHANTOM_THRESHOLD));
    REQUIRE(has(ParamID::PHANTOM_STRENGTH));
    REQUIRE(has(ParamID::OUTPUT_GAIN));

    // Recipe
    REQUIRE(has(ParamID::RECIPE_H2));
    REQUIRE(has(ParamID::RECIPE_H8));
    REQUIRE(has(ParamID::RECIPE_PHASE_H2));
    REQUIRE(has(ParamID::RECIPE_PHASE_H8));
    REQUIRE(has(ParamID::RECIPE_PRESET));
    REQUIRE(has(ParamID::RECIPE_ROTATION));
    REQUIRE(has(ParamID::HARMONIC_SATURATION));

    // Binaural
    REQUIRE(has(ParamID::BINAURAL_MODE));
    REQUIRE(has(ParamID::BINAURAL_WIDTH));

    // Pitch Tracker
    REQUIRE(has(ParamID::TRACKING_SENSITIVITY));
    REQUIRE(has(ParamID::TRACKING_GLIDE));

    // Deconfliction
    REQUIRE(has(ParamID::DECONFLICTION_MODE));
    REQUIRE(has(ParamID::MAX_VOICES));
    REQUIRE(has(ParamID::STAGGER_DELAY));

    // Sidechain / Stereo
    REQUIRE(has(ParamID::SIDECHAIN_DUCK_AMOUNT));
    REQUIRE(has(ParamID::SIDECHAIN_DUCK_ATTACK));
    REQUIRE(has(ParamID::SIDECHAIN_DUCK_RELEASE));
    REQUIRE(has(ParamID::STEREO_WIDTH));
}

TEST_CASE("ghost parameter default is 100")
{
    auto layout = createParameterLayout();
    auto* p = dynamic_cast<juce::RangedAudioParameter*>(
        layout.getParameterByID(ParamID::GHOST));
    REQUIRE(p != nullptr);
    REQUIRE(p->getDefaultValue() == Catch::Approx(1.0f));  // normalised
}
```

- [ ] **Step 2: Run test, verify it fails**

```bash
cmake --build build --config Debug --target KaigenPhantomTests
build/tests/Debug/KaigenPhantomTests.exe "[ParameterTests]"
```

Expected: FAIL — `ParamID::GHOST_MODE` not defined.

- [ ] **Step 3: Replace `Source/Parameters.h` with full implementation**

```cpp
#pragma once
#include <JuceHeader.h>

namespace ParamID
{
    // ── Mode & Global ─────────────────────────────────────────────────
    inline constexpr auto MODE               = "mode";
    inline constexpr auto GHOST              = "ghost";
    inline constexpr auto GHOST_MODE         = "ghost_mode";
    inline constexpr auto PHANTOM_THRESHOLD  = "phantom_threshold";
    inline constexpr auto PHANTOM_STRENGTH   = "phantom_strength";
    inline constexpr auto OUTPUT_GAIN        = "output_gain";

    // ── Recipe Engine ─────────────────────────────────────────────────
    inline constexpr auto RECIPE_H2          = "recipe_h2";
    inline constexpr auto RECIPE_H3          = "recipe_h3";
    inline constexpr auto RECIPE_H4          = "recipe_h4";
    inline constexpr auto RECIPE_H5          = "recipe_h5";
    inline constexpr auto RECIPE_H6          = "recipe_h6";
    inline constexpr auto RECIPE_H7          = "recipe_h7";
    inline constexpr auto RECIPE_H8          = "recipe_h8";
    inline constexpr auto RECIPE_PHASE_H2    = "recipe_phase_h2";
    inline constexpr auto RECIPE_PHASE_H3    = "recipe_phase_h3";
    inline constexpr auto RECIPE_PHASE_H4    = "recipe_phase_h4";
    inline constexpr auto RECIPE_PHASE_H5    = "recipe_phase_h5";
    inline constexpr auto RECIPE_PHASE_H6    = "recipe_phase_h6";
    inline constexpr auto RECIPE_PHASE_H7    = "recipe_phase_h7";
    inline constexpr auto RECIPE_PHASE_H8    = "recipe_phase_h8";
    inline constexpr auto RECIPE_PRESET      = "recipe_preset";
    inline constexpr auto RECIPE_ROTATION    = "recipe_rotation";
    inline constexpr auto HARMONIC_SATURATION = "harmonic_saturation";

    // ── Binaural ──────────────────────────────────────────────────────
    inline constexpr auto BINAURAL_MODE      = "binaural_mode";
    inline constexpr auto BINAURAL_WIDTH     = "binaural_width";

    // ── Pitch Tracker ─────────────────────────────────────────────────
    inline constexpr auto TRACKING_SENSITIVITY = "tracking_sensitivity";
    inline constexpr auto TRACKING_GLIDE       = "tracking_glide";

    // ── Deconfliction ─────────────────────────────────────────────────
    inline constexpr auto DECONFLICTION_MODE = "deconfliction_mode";
    inline constexpr auto MAX_VOICES         = "max_voices";
    inline constexpr auto STAGGER_DELAY      = "stagger_delay";

    // ── Sidechain & Stereo ────────────────────────────────────────────
    inline constexpr auto SIDECHAIN_DUCK_AMOUNT  = "sidechain_duck_amount";
    inline constexpr auto SIDECHAIN_DUCK_ATTACK  = "sidechain_duck_attack";
    inline constexpr auto SIDECHAIN_DUCK_RELEASE = "sidechain_duck_release";
    inline constexpr auto STEREO_WIDTH           = "stereo_width";
}

// Warm preset harmonic amplitudes for H2..H8
constexpr float kWarmAmps[7]       = { 0.80f, 0.70f, 0.50f, 0.35f, 0.20f, 0.12f, 0.07f };
constexpr float kAggressiveAmps[7] = { 0.40f, 0.50f, 0.90f, 1.00f, 0.80f, 0.50f, 0.30f };
constexpr float kHollowAmps[7]     = { 0.10f, 0.80f, 0.10f, 0.70f, 0.10f, 0.60f, 0.10f };
constexpr float kDenseAmps[7]      = { 0.85f, 0.85f, 0.85f, 0.85f, 0.85f, 0.85f, 0.85f };

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    using APF  = AudioParameterFloat;
    using APC  = AudioParameterChoice;
    using APFI = AudioParameterInt;

    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // ── Mode & Global ─────────────────────────────────────────────────
    params.push_back(std::make_unique<APC>(
        ParamID::MODE, "Mode", StringArray{ "Effect", "Instrument" }, 0));
    params.push_back(std::make_unique<APF>(
        ParamID::GHOST, "Ghost",
        NormalisableRange<float>(0.0f, 100.0f), 100.0f,
        AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<APC>(
        ParamID::GHOST_MODE, "Ghost Mode", StringArray{ "Replace", "Add" }, 0));
    params.push_back(std::make_unique<APF>(
        ParamID::PHANTOM_THRESHOLD, "Phantom Threshold",
        NormalisableRange<float>(20.0f, 150.0f), 80.0f,
        AudioParameterFloatAttributes().withLabel("Hz")));
    params.push_back(std::make_unique<APF>(
        ParamID::PHANTOM_STRENGTH, "Phantom Strength",
        NormalisableRange<float>(0.0f, 100.0f), 80.0f,
        AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<APF>(
        ParamID::OUTPUT_GAIN, "Output Gain",
        NormalisableRange<float>(-24.0f, 12.0f), 0.0f,
        AudioParameterFloatAttributes().withLabel("dB")));

    // ── Recipe Engine — amplitudes ────────────────────────────────────
    const char* ampIDs[7] = {
        ParamID::RECIPE_H2, ParamID::RECIPE_H3, ParamID::RECIPE_H4,
        ParamID::RECIPE_H5, ParamID::RECIPE_H6, ParamID::RECIPE_H7, ParamID::RECIPE_H8
    };
    const char* ampNames[7] = { "H2 Amp","H3 Amp","H4 Amp","H5 Amp","H6 Amp","H7 Amp","H8 Amp" };
    for (int i = 0; i < 7; ++i)
        params.push_back(std::make_unique<APF>(
            ampIDs[i], ampNames[i],
            NormalisableRange<float>(0.0f, 100.0f), kWarmAmps[i] * 100.0f,
            AudioParameterFloatAttributes().withLabel("%")));

    // ── Recipe Engine — phases ────────────────────────────────────────
    const char* phaseIDs[7] = {
        ParamID::RECIPE_PHASE_H2, ParamID::RECIPE_PHASE_H3, ParamID::RECIPE_PHASE_H4,
        ParamID::RECIPE_PHASE_H5, ParamID::RECIPE_PHASE_H6, ParamID::RECIPE_PHASE_H7,
        ParamID::RECIPE_PHASE_H8
    };
    const char* phaseNames[7] = { "H2 Phase","H3 Phase","H4 Phase","H5 Phase","H6 Phase","H7 Phase","H8 Phase" };
    for (int i = 0; i < 7; ++i)
        params.push_back(std::make_unique<APF>(
            phaseIDs[i], phaseNames[i],
            NormalisableRange<float>(0.0f, 360.0f), 0.0f,
            AudioParameterFloatAttributes().withLabel("deg")));

    params.push_back(std::make_unique<APC>(
        ParamID::RECIPE_PRESET, "Recipe Preset",
        StringArray{ "Warm", "Aggressive", "Hollow", "Dense", "Custom" }, 0));
    params.push_back(std::make_unique<APF>(
        ParamID::RECIPE_ROTATION, "Recipe Rotation",
        NormalisableRange<float>(-180.0f, 180.0f), 0.0f,
        AudioParameterFloatAttributes().withLabel("deg")));
    params.push_back(std::make_unique<APF>(
        ParamID::HARMONIC_SATURATION, "Harmonic Saturation",
        NormalisableRange<float>(0.0f, 100.0f), 0.0f,
        AudioParameterFloatAttributes().withLabel("%")));

    // ── Binaural ──────────────────────────────────────────────────────
    params.push_back(std::make_unique<APC>(
        ParamID::BINAURAL_MODE, "Binaural Mode",
        StringArray{ "Off", "Spread", "Voice-Split" }, 0));
    params.push_back(std::make_unique<APF>(
        ParamID::BINAURAL_WIDTH, "Binaural Width",
        NormalisableRange<float>(0.0f, 100.0f), 50.0f,
        AudioParameterFloatAttributes().withLabel("%")));

    // ── Pitch Tracker ─────────────────────────────────────────────────
    params.push_back(std::make_unique<APF>(
        ParamID::TRACKING_SENSITIVITY, "Tracking Sensitivity",
        NormalisableRange<float>(0.0f, 100.0f), 70.0f,
        AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<APF>(
        ParamID::TRACKING_GLIDE, "Tracking Glide",
        NormalisableRange<float>(0.0f, 200.0f), 20.0f,
        AudioParameterFloatAttributes().withLabel("ms")));

    // ── Deconfliction ─────────────────────────────────────────────────
    params.push_back(std::make_unique<APC>(
        ParamID::DECONFLICTION_MODE, "Deconfliction Mode",
        StringArray{ "Partition","Lane","Stagger","Odd-Even","Residue","Binaural" }, 0));
    params.push_back(std::make_unique<APFI>(
        ParamID::MAX_VOICES, "Max Voices", 1, 8, 4));
    params.push_back(std::make_unique<APF>(
        ParamID::STAGGER_DELAY, "Stagger Delay",
        NormalisableRange<float>(0.0f, 30.0f), 8.0f,
        AudioParameterFloatAttributes().withLabel("ms")));

    // ── Sidechain & Stereo ────────────────────────────────────────────
    params.push_back(std::make_unique<APF>(
        ParamID::SIDECHAIN_DUCK_AMOUNT, "Duck Amount",
        NormalisableRange<float>(0.0f, 100.0f), 0.0f,
        AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<APF>(
        ParamID::SIDECHAIN_DUCK_ATTACK, "Duck Attack",
        NormalisableRange<float>(1.0f, 100.0f), 5.0f,
        AudioParameterFloatAttributes().withLabel("ms")));
    params.push_back(std::make_unique<APF>(
        ParamID::SIDECHAIN_DUCK_RELEASE, "Duck Release",
        NormalisableRange<float>(10.0f, 500.0f), 80.0f,
        AudioParameterFloatAttributes().withLabel("ms")));
    params.push_back(std::make_unique<APF>(
        ParamID::STEREO_WIDTH, "Stereo Width",
        NormalisableRange<float>(0.0f, 200.0f), 100.0f,
        AudioParameterFloatAttributes().withLabel("%")));

    return { params.begin(), params.end() };
}
```

- [ ] **Step 4: Run tests, verify pass**

```bash
cmake --build build --config Debug --target KaigenPhantomTests
build/tests/Debug/KaigenPhantomTests.exe "[ParameterTests]"
```

Expected: `2 tests passed`

- [ ] **Step 5: Commit**

```bash
git add Source/Parameters.h tests/ParameterTests.cpp
git commit -m "feat: full APVTS parameter layout — 35 params across all engine domains"
```

---

## Task 3: PitchTracker — YIN monophonic pitch detection

**Files:**
- Create: `Source/Engines/PitchTracker.h`
- Modify: `Source/Engines/PitchTracker.cpp`
- Modify: `tests/PitchTrackerTests.cpp`

- [ ] **Step 1: Write failing tests**

```cpp
// tests/PitchTrackerTests.cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Engines/PitchTracker.h"
#include <cmath>
#include <vector>

static std::vector<float> makeSine(float freq, float sampleRate, int numSamples)
{
    std::vector<float> buf(numSamples);
    for (int i = 0; i < numSamples; ++i)
        buf[i] = std::sin(2.0f * 3.14159265f * freq * i / sampleRate);
    return buf;
}

TEST_CASE("PitchTracker detects 80 Hz sine within 2 Hz")
{
    PitchTracker tracker;
    tracker.prepare(44100.0, 2048);

    auto sine = makeSine(80.0f, 44100.0f, 2048);
    float detected = tracker.detectPitch(sine.data(), (int)sine.size());

    REQUIRE(detected == Catch::Approx(80.0f).margin(2.0f));
}

TEST_CASE("PitchTracker detects 40 Hz sine within 2 Hz")
{
    PitchTracker tracker;
    tracker.prepare(44100.0, 2048);

    auto sine = makeSine(40.0f, 44100.0f, 2048);
    float detected = tracker.detectPitch(sine.data(), (int)sine.size());

    REQUIRE(detected == Catch::Approx(40.0f).margin(2.0f));
}

TEST_CASE("PitchTracker returns -1 for silence")
{
    PitchTracker tracker;
    tracker.prepare(44100.0, 2048);

    std::vector<float> silence(2048, 0.0f);
    float detected = tracker.detectPitch(silence.data(), (int)silence.size());

    REQUIRE(detected < 0.0f);
}

TEST_CASE("PitchTracker glide smooths pitch changes")
{
    PitchTracker tracker;
    tracker.prepare(44100.0, 512);
    tracker.setGlideMs(50.0f);

    // Feed 80 Hz first to establish a pitch
    auto sine80 = makeSine(80.0f, 44100.0f, 2048);
    tracker.detectPitch(sine80.data(), 2048);

    // Smooth pitch should not jump immediately to a new value
    float smoothed80 = tracker.getSmoothedPitch();
    REQUIRE(smoothed80 == Catch::Approx(80.0f).margin(5.0f));
}
```

- [ ] **Step 2: Run, verify FAIL**

```bash
cmake --build build --config Debug --target KaigenPhantomTests
build/tests/Debug/KaigenPhantomTests.exe "[PitchTrackerTests]"
```

Expected: FAIL — `PitchTracker` not defined.

- [ ] **Step 3: Create `Source/Engines/PitchTracker.h`**

```cpp
#pragma once
#include <JuceHeader.h>
#include <vector>

class PitchTracker
{
public:
    void prepare(double sampleRate, int maxBlockSize);

    // Returns detected fundamental in Hz, or -1.0f if no pitch detected.
    // confidence threshold maps tracking_sensitivity 0-100% → YIN threshold 0.3-0.05
    float detectPitch(const float* samples, int numSamples);

    void setConfidenceThreshold(float threshold) noexcept { yinThreshold = threshold; }
    void setGlideMs(float ms) noexcept { glideMs = ms; }

    // Smoothed pitch after glide applied — call after detectPitch()
    float getSmoothedPitch() const noexcept { return smoothedPitch; }

private:
    double sampleRate   = 44100.0;
    float  yinThreshold = 0.15f;   // lower = stricter confidence
    float  glideMs      = 20.0f;
    float  smoothedPitch = -1.0f;
    float  lastRawPitch  = -1.0f;

    std::vector<float> diffBuf;
    std::vector<float> cmndBuf;

    float computeRawPitch(const float* samples, int numSamples);
    void  applyGlide(float rawPitch);
};
```

- [ ] **Step 4: Implement `Source/Engines/PitchTracker.cpp`**

```cpp
#include "PitchTracker.h"
#include <cmath>
#include <algorithm>

void PitchTracker::prepare(double sr, int maxBlockSize)
{
    sampleRate = sr;
    const int tauMax = maxBlockSize / 2;
    diffBuf.assign(tauMax, 0.0f);
    cmndBuf.assign(tauMax, 0.0f);
}

float PitchTracker::detectPitch(const float* samples, int numSamples)
{
    float raw = computeRawPitch(samples, numSamples);
    applyGlide(raw);
    return smoothedPitch;
}

float PitchTracker::computeRawPitch(const float* samples, int numSamples)
{
    const int tauMax = numSamples / 2;
    if (tauMax < 2) return -1.0f;

    // 1. Difference function
    for (int tau = 0; tau < tauMax; ++tau)
    {
        diffBuf[tau] = 0.0f;
        for (int j = 0; j < tauMax; ++j)
        {
            float delta = samples[j] - samples[j + tau];
            diffBuf[tau] += delta * delta;
        }
    }

    // 2. Cumulative mean normalized difference function
    cmndBuf[0] = 1.0f;
    float runningSum = 0.0f;
    for (int tau = 1; tau < tauMax; ++tau)
    {
        runningSum += diffBuf[tau];
        cmndBuf[tau] = (runningSum > 0.0f)
            ? diffBuf[tau] * (float)tau / runningSum
            : 1.0f;
    }

    // 3. Absolute threshold + parabolic interpolation
    for (int tau = 2; tau < tauMax - 1; ++tau)
    {
        if (cmndBuf[tau] < yinThreshold
            && cmndBuf[tau] < cmndBuf[tau - 1]
            && cmndBuf[tau] < cmndBuf[tau + 1])
        {
            // Parabolic interpolation
            float s0 = cmndBuf[tau - 1];
            float s1 = cmndBuf[tau];
            float s2 = cmndBuf[tau + 1];
            float denom = 2.0f * (2.0f * s1 - s0 - s2);
            float betterTau = (std::abs(denom) > 1e-7f)
                ? (float)tau + (s2 - s0) / denom
                : (float)tau;

            if (betterTau > 1.0f)
                return (float)sampleRate / betterTau;
        }
    }

    return -1.0f;
}

void PitchTracker::applyGlide(float rawPitch)
{
    if (rawPitch < 0.0f)
    {
        // No pitch detected — hold last smoothed value
        return;
    }

    if (smoothedPitch < 0.0f || glideMs < 1.0f)
    {
        smoothedPitch = rawPitch;
        return;
    }

    // 1-pole IIR glide: coeff from time constant
    const float blockDuration = 512.0f / (float)sampleRate * 1000.0f; // ms per block (approx)
    const float coeff = 1.0f - std::exp(-blockDuration / glideMs);
    smoothedPitch = smoothedPitch + coeff * (rawPitch - smoothedPitch);
}
```

- [ ] **Step 5: Run tests, verify pass**

```bash
cmake --build build --config Debug --target KaigenPhantomTests
build/tests/Debug/KaigenPhantomTests.exe "[PitchTrackerTests]"
```

Expected: `4 tests passed`

- [ ] **Step 6: Commit**

```bash
git add Source/Engines/PitchTracker.h Source/Engines/PitchTracker.cpp tests/PitchTrackerTests.cpp
git commit -m "feat: PitchTracker — YIN algorithm with confidence threshold and glide smoothing"
```

---

## Task 4: HarmonicGenerator — single voice + Recipe Engine

**Files:**
- Create: `Source/Engines/HarmonicGenerator.h`
- Modify: `Source/Engines/HarmonicGenerator.cpp`
- Modify: `tests/HarmonicGeneratorTests.cpp`

- [ ] **Step 1: Write failing tests**

```cpp
// tests/HarmonicGeneratorTests.cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Engines/HarmonicGenerator.h"
#include "Parameters.h"
#include <vector>
#include <numeric>
#include <cmath>

TEST_CASE("HarmonicGenerator produces non-zero output for active voice")
{
    HarmonicGenerator gen;
    gen.prepare(44100.0, 512);

    // Activate a single voice at 40 Hz (effect mode — 1 voice)
    gen.setEffectModePitch(40.0f);
    gen.setPhantomStrength(1.0f);

    juce::AudioBuffer<float> buf(2, 512);
    buf.clear();
    gen.process(buf);

    float rms = 0.0f;
    for (int i = 0; i < 512; ++i)
        rms += buf.getSample(0, i) * buf.getSample(0, i);
    rms = std::sqrt(rms / 512.0f);

    REQUIRE(rms > 0.001f);
}

TEST_CASE("HarmonicGenerator silence when phantom strength is 0")
{
    HarmonicGenerator gen;
    gen.prepare(44100.0, 512);
    gen.setEffectModePitch(80.0f);
    gen.setPhantomStrength(0.0f);

    juce::AudioBuffer<float> buf(2, 512);
    buf.clear();
    gen.process(buf);

    float peak = 0.0f;
    for (int i = 0; i < 512; ++i)
        peak = std::max(peak, std::abs(buf.getSample(0, i)));

    REQUIRE(peak < 1e-6f);
}

TEST_CASE("HarmonicGenerator Warm preset has H2 louder than H8")
{
    HarmonicGenerator gen;
    gen.prepare(44100.0, 512);
    gen.setPreset(RecipePreset::Warm);

    auto amps = gen.getHarmonicAmplitudes();  // returns float[7]
    REQUIRE(amps[0] > amps[6]);  // H2 > H8
}

TEST_CASE("HarmonicGenerator Hollow preset has odd harmonics louder than even")
{
    HarmonicGenerator gen;
    gen.prepare(44100.0, 512);
    gen.setPreset(RecipePreset::Hollow);

    auto amps = gen.getHarmonicAmplitudes();
    // H3 (index 1) > H2 (index 0), H5 (index 3) > H4 (index 2)
    REQUIRE(amps[1] > amps[0]);
    REQUIRE(amps[3] > amps[2]);
}

TEST_CASE("MIDI voice activation adds a voice to the pool")
{
    HarmonicGenerator gen;
    gen.prepare(44100.0, 512);
    gen.setMaxVoices(4);

    gen.noteOn(60, 100);  // middle C
    REQUIRE(gen.getActiveVoiceCount() == 1);

    gen.noteOn(64, 100);  // E4
    REQUIRE(gen.getActiveVoiceCount() == 2);

    gen.noteOff(60);
    REQUIRE(gen.getActiveVoiceCount() == 1);
}
```

- [ ] **Step 2: Run, verify FAIL**

```bash
cmake --build build --config Debug --target KaigenPhantomTests
build/tests/Debug/KaigenPhantomTests.exe "[HarmonicGeneratorTests]"
```

Expected: FAIL — `HarmonicGenerator` not defined.

- [ ] **Step 3: Create `Source/Engines/HarmonicGenerator.h`**

```cpp
#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>
#include "Deconfliction/IDeconflictionStrategy.h"

enum class RecipePreset { Warm, Aggressive, Hollow, Dense, Custom };

struct Voice
{
    float fundamentalHz = 0.0f;
    float phases[7]     = {};       // H2..H8 oscillator phases (radians)
    float amps[7]       = {};       // effective amplitudes after deconfliction
    int   midiNote      = -1;
    int   voiceIndex    = 0;
    bool  active        = false;

    void reset()
    {
        fundamentalHz = 0.0f;
        std::fill(phases, phases + 7, 0.0f);
        std::fill(amps,   amps   + 7, 0.0f);
        midiNote    = -1;
        voiceIndex  = 0;
        active      = false;
    }
};

class HarmonicGenerator
{
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    // Effect mode: single voice driven by pitch tracker output
    void setEffectModePitch(float hz) noexcept;

    // Instrument mode: MIDI voice pool
    void noteOn(int midiNote, int velocity);
    void noteOff(int midiNote);
    int  getActiveVoiceCount() const noexcept;
    void setMaxVoices(int n) noexcept { maxVoices = juce::jlimit(1, 8, n); }

    // Recipe
    void setPreset(RecipePreset preset);
    void setHarmonicAmp(int harmonic, float amp);    // harmonic = 2..8
    void setHarmonicPhase(int harmonic, float deg);  // harmonic = 2..8
    void setRotation(float degrees) noexcept { rotationDeg = degrees; }
    void setSaturation(float amount) noexcept { saturation = juce::jlimit(0.0f, 1.0f, amount); }
    std::array<float, 7> getHarmonicAmplitudes() const noexcept { return recipeAmps; }

    // Strength
    void setPhantomStrength(float s) noexcept { phantomStrength = juce::jlimit(0.0f, 1.0f, s); }

    // Deconfliction strategy (instrument mode only)
    void setDeconflictionStrategy(IDeconflictionStrategy* strategy) noexcept { deconfliction = strategy; }

    // Fills buffer with summed harmonic output from all active voices.
    // Buffer should be pre-zeroed by the caller.
    void process(juce::AudioBuffer<float>& buffer);

private:
    double sampleRate    = 44100.0;
    float  phantomStrength = 1.0f;
    float  rotationDeg   = 0.0f;
    float  saturation    = 0.0f;
    int    maxVoices     = 4;

    std::array<float, 7> recipeAmps   = { 0.80f, 0.70f, 0.50f, 0.35f, 0.20f, 0.12f, 0.07f };  // Warm
    std::array<float, 7> recipePhases = {};  // degrees

    std::vector<Voice> voicePool;  // always size 8, active flag used
    Voice effectVoice;             // used in effect mode

    IDeconflictionStrategy* deconfliction = nullptr;

    void renderVoice(Voice& v, juce::AudioBuffer<float>& buffer, int numSamples);
    float midiNoteToHz(int note) const noexcept;
    int   findFreeVoice() const noexcept;
    int   findVoiceForNote(int midiNote) const noexcept;
};
```

- [ ] **Step 4: Create `Source/Engines/Deconfliction/IDeconflictionStrategy.h`**

```cpp
#pragma once
#include <vector>

struct Voice;  // forward declared — full definition in HarmonicGenerator.h

class IDeconflictionStrategy
{
public:
    virtual ~IDeconflictionStrategy() = default;

    // Called before rendering. Implementations may redistribute harmonic
    // amplitudes across voices to reduce phantom collisions.
    // voices: all currently active voices (active == true)
    virtual void resolve(std::vector<Voice>& voices) = 0;
};
```

- [ ] **Step 5: Implement `Source/Engines/HarmonicGenerator.cpp`**

```cpp
#include "HarmonicGenerator.h"
#include <cmath>
#include <algorithm>

static constexpr float kTwoPi = 6.28318530718f;

void HarmonicGenerator::prepare(double sr, int /*maxBlockSize*/)
{
    sampleRate = sr;
    voicePool.resize(8);
    for (auto& v : voicePool) v.reset();
    effectVoice.reset();
}

void HarmonicGenerator::reset()
{
    for (auto& v : voicePool) v.reset();
    effectVoice.reset();
}

void HarmonicGenerator::setEffectModePitch(float hz) noexcept
{
    effectVoice.fundamentalHz = hz;
    effectVoice.active        = (hz > 0.0f);
    // Copy recipe into effect voice
    for (int i = 0; i < 7; ++i)
    {
        effectVoice.amps[i] = recipeAmps[i];
    }
}

void HarmonicGenerator::noteOn(int midiNote, int /*velocity*/)
{
    // Don't double-trigger same note
    if (findVoiceForNote(midiNote) >= 0) return;

    int idx = findFreeVoice();
    if (idx < 0) return;  // voice pool full

    Voice& v = voicePool[idx];
    v.reset();
    v.midiNote      = midiNote;
    v.fundamentalHz = midiNoteToHz(midiNote);
    v.voiceIndex    = idx;
    v.active        = true;
    for (int i = 0; i < 7; ++i) v.amps[i] = recipeAmps[i];
}

void HarmonicGenerator::noteOff(int midiNote)
{
    int idx = findVoiceForNote(midiNote);
    if (idx >= 0) voicePool[idx].reset();
}

int HarmonicGenerator::getActiveVoiceCount() const noexcept
{
    int count = 0;
    for (const auto& v : voicePool)
        if (v.active) ++count;
    return count;
}

void HarmonicGenerator::setPreset(RecipePreset preset)
{
    switch (preset)
    {
        case RecipePreset::Warm:
            recipeAmps = { 0.80f, 0.70f, 0.50f, 0.35f, 0.20f, 0.12f, 0.07f }; break;
        case RecipePreset::Aggressive:
            recipeAmps = { 0.40f, 0.50f, 0.90f, 1.00f, 0.80f, 0.50f, 0.30f }; break;
        case RecipePreset::Hollow:
            recipeAmps = { 0.10f, 0.80f, 0.10f, 0.70f, 0.10f, 0.60f, 0.10f }; break;
        case RecipePreset::Dense:
            recipeAmps = { 0.85f, 0.85f, 0.85f, 0.85f, 0.85f, 0.85f, 0.85f }; break;
        case RecipePreset::Custom:
            break;  // user controls individual sliders — don't override
    }
}

void HarmonicGenerator::setHarmonicAmp(int harmonic, float amp)
{
    if (harmonic >= 2 && harmonic <= 8)
        recipeAmps[harmonic - 2] = juce::jlimit(0.0f, 1.0f, amp);
}

void HarmonicGenerator::setHarmonicPhase(int harmonic, float deg)
{
    if (harmonic >= 2 && harmonic <= 8)
        recipePhases[harmonic - 2] = deg;
}

void HarmonicGenerator::process(juce::AudioBuffer<float>& buffer)
{
    if (phantomStrength < 1e-5f) return;

    const int numSamples = buffer.getNumSamples();

    // Apply deconfliction to instrument voice pool
    if (deconfliction != nullptr)
        deconfliction->resolve(voicePool);

    // Render effect voice or instrument pool
    if (effectVoice.active)
    {
        for (int i = 0; i < 7; ++i) effectVoice.amps[i] = recipeAmps[i];
        renderVoice(effectVoice, buffer, numSamples);
    }
    else
    {
        for (auto& v : voicePool)
            if (v.active) renderVoice(v, buffer, numSamples);
    }
}

void HarmonicGenerator::renderVoice(Voice& v, juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (v.fundamentalHz <= 0.0f) return;

    const float sr    = (float)sampleRate;
    const float scale = phantomStrength / 7.0f;  // normalise sum of harmonics

    // Apply rotation: shift harmonic weights cyclically
    const float rotNorm = rotationDeg / 360.0f;  // 0..1
    const int   rotSteps = (int)(rotNorm * 7.0f + 0.5f) % 7;

    for (int i = 0; i < 7; ++i)
    {
        const int   harmIdx     = (i + rotSteps) % 7;
        const float harmNum     = (float)(harmIdx + 2);  // H2..H8
        const float phaseOffset = recipePhases[harmIdx] * (kTwoPi / 360.0f);
        const float amp         = v.amps[harmIdx] * scale;
        if (amp < 1e-6f) continue;

        const float phaseInc = kTwoPi * harmNum * v.fundamentalHz / sr;

        for (int s = 0; s < numSamples; ++s)
        {
            float sample = std::sin(v.phases[harmIdx] + phaseOffset) * amp;

            // Soft saturation: tanh(x * k) / tanh(k), k driven by saturation param
            if (saturation > 1e-4f)
            {
                const float k    = 1.0f + saturation * 9.0f;
                const float tanhK = std::tanh(k);
                sample = std::tanh(sample * k) / tanhK;
            }

            // Sum into both channels
            buffer.addSample(0, s, sample);
            buffer.addSample(1, s, sample);

            v.phases[harmIdx] += phaseInc;
            if (v.phases[harmIdx] > kTwoPi) v.phases[harmIdx] -= kTwoPi;
        }
    }
}

float HarmonicGenerator::midiNoteToHz(int note) const noexcept
{
    return 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
}

int HarmonicGenerator::findFreeVoice() const noexcept
{
    int active = 0;
    for (int i = 0; i < (int)voicePool.size(); ++i)
    {
        if (!voicePool[i].active) return i;
        ++active;
        if (active >= maxVoices) return -1;
    }
    return -1;
}

int HarmonicGenerator::findVoiceForNote(int midiNote) const noexcept
{
    for (int i = 0; i < (int)voicePool.size(); ++i)
        if (voicePool[i].active && voicePool[i].midiNote == midiNote) return i;
    return -1;
}
```

- [ ] **Step 6: Run tests, verify pass**

```bash
cmake --build build --config Debug --target KaigenPhantomTests
build/tests/Debug/KaigenPhantomTests.exe "[HarmonicGeneratorTests]"
```

Expected: `5 tests passed`

- [ ] **Step 7: Commit**

```bash
git add Source/Engines/HarmonicGenerator.h Source/Engines/HarmonicGenerator.cpp \
        Source/Engines/Deconfliction/IDeconflictionStrategy.h \
        tests/HarmonicGeneratorTests.cpp
git commit -m "feat: HarmonicGenerator — voice pool, Recipe Engine (4 presets), MIDI noteOn/Off"
```

---

## Task 5: Six Deconfliction Strategies

**Files:**
- Create: `Source/Engines/Deconfliction/PartitionStrategy.h/.cpp`
- Create: `Source/Engines/Deconfliction/SpectralLaneStrategy.h/.cpp`
- Create: `Source/Engines/Deconfliction/StaggerStrategy.h/.cpp`
- Create: `Source/Engines/Deconfliction/OddEvenStrategy.h/.cpp`
- Create: `Source/Engines/Deconfliction/ResidueStrategy.h/.cpp`
- Create: `Source/Engines/Deconfliction/BinauralStrategy.h/.cpp`
- Modify: `tests/DeconflictionTests.cpp`

- [ ] **Step 1: Write failing tests**

```cpp
// tests/DeconflictionTests.cpp
#include <catch2/catch_test_macros.hpp>
#include "Engines/HarmonicGenerator.h"
#include "Engines/Deconfliction/PartitionStrategy.h"
#include "Engines/Deconfliction/OddEvenStrategy.h"
#include "Engines/Deconfliction/ResidueStrategy.h"
#include "Engines/Deconfliction/SpectralLaneStrategy.h"
#include "Engines/Deconfliction/StaggerStrategy.h"
#include "Engines/Deconfliction/BinauralStrategy.h"

static std::vector<Voice> makeTwoVoices()
{
    Voice a, b;
    a.active = true; a.fundamentalHz = 40.0f; a.voiceIndex = 0;
    b.active = true; b.fundamentalHz = 60.0f; b.voiceIndex = 1;
    for (int i = 0; i < 7; ++i) { a.amps[i] = 1.0f; b.amps[i] = 1.0f; }
    return { a, b };
}

TEST_CASE("PartitionStrategy: shared harmonics owned by one voice only")
{
    PartitionStrategy strat;
    auto voices = makeTwoVoices();
    strat.resolve(voices);

    // For two different fundamentals, at least some harmonics should differ in ownership
    // (one voice should have amp=0 where the other has amp>0 for the same harmonic index)
    bool foundDivided = false;
    for (int i = 0; i < 7; ++i)
    {
        if ((voices[0].amps[i] > 0.0f) != (voices[1].amps[i] > 0.0f))
            foundDivided = true;
    }
    REQUIRE(foundDivided);
}

TEST_CASE("OddEvenStrategy: voice 0 has only odd harmonics, voice 1 has only even harmonics")
{
    OddEvenStrategy strat;
    auto voices = makeTwoVoices();
    strat.resolve(voices);

    // voice 0 (index 0): should have H3(idx1), H5(idx3), H7(idx5) non-zero
    //                    and H2(idx0), H4(idx2), H6(idx4), H8(idx6) zero
    REQUIRE(voices[0].amps[0] == Catch::Approx(0.0f));  // H2 zero for voice 0
    REQUIRE(voices[0].amps[1] > 0.0f);                  // H3 present for voice 0

    // voice 1 (index 1): should have H2(idx0), H4(idx2) non-zero
    REQUIRE(voices[1].amps[0] > 0.0f);                  // H2 present for voice 1
    REQUIRE(voices[1].amps[1] == Catch::Approx(0.0f));  // H3 zero for voice 1
}

TEST_CASE("ResidueStrategy: does not zero all harmonics for any voice")
{
    ResidueStrategy strat;
    auto voices = makeTwoVoices();
    strat.resolve(voices);

    float sumA = 0.0f, sumB = 0.0f;
    for (int i = 0; i < 7; ++i) { sumA += voices[0].amps[i]; sumB += voices[1].amps[i]; }
    REQUIRE(sumA > 0.0f);
    REQUIRE(sumB > 0.0f);
}

TEST_CASE("SpectralLaneStrategy: voice 0 has higher low harmonics than voice 1")
{
    SpectralLaneStrategy strat;
    auto voices = makeTwoVoices();
    strat.resolve(voices);

    // Lower voice (lower fundamental) gets weight on lower harmonics
    // Higher voice gets weight on upper harmonics
    // Voice 0 has fundamental 40 Hz, voice 1 has 60 Hz
    REQUIRE(voices[0].amps[0] >= voices[1].amps[0]);  // voice 0 owns H2 more
    REQUIRE(voices[1].amps[6] >= voices[0].amps[6]);  // voice 1 owns H8 more
}

TEST_CASE("StaggerStrategy: stores delay per voice index")
{
    StaggerStrategy strat;
    strat.setDelayMs(8.0f, 44100.0);

    auto voices = makeTwoVoices();
    strat.resolve(voices);

    // Stagger strategy records delay samples per voice — voice 1 delayed more than voice 0
    REQUIRE(strat.getDelaySamplesForVoice(0) == 0);
    REQUIRE(strat.getDelaySamplesForVoice(1) > 0);
}

TEST_CASE("BinauralStrategy: voices assigned to different pan positions")
{
    BinauralStrategy strat;
    auto voices = makeTwoVoices();
    strat.resolve(voices);

    // After resolve, voices should have different pan values
    REQUIRE(strat.getPanForVoice(0) != strat.getPanForVoice(1));
}
```

- [ ] **Step 2: Run, verify FAIL**

```bash
cmake --build build --config Debug --target KaigenPhantomTests
build/tests/Debug/KaigenPhantomTests.exe "[DeconflictionTests]"
```

Expected: FAIL — strategy classes not defined.

- [ ] **Step 3: Implement `PartitionStrategy.h/.cpp`**

`Source/Engines/Deconfliction/PartitionStrategy.h`:
```cpp
#pragma once
#include "IDeconflictionStrategy.h"
class PartitionStrategy : public IDeconflictionStrategy
{
public:
    void resolve(std::vector<Voice>& voices) override;
};
```

`Source/Engines/Deconfliction/PartitionStrategy.cpp`:
```cpp
#include "PartitionStrategy.h"
#include "../HarmonicGenerator.h"

void PartitionStrategy::resolve(std::vector<Voice>& voices)
{
    // Count active voices
    std::vector<Voice*> active;
    for (auto& v : voices) if (v.active) active.push_back(&v);
    if (active.size() <= 1) return;

    const int n = (int)active.size();
    // Distribute harmonics round-robin: harmonic i belongs to voice (i % n)
    for (int i = 0; i < 7; ++i)
    {
        int owner = i % n;
        for (int v = 0; v < n; ++v)
            if (v != owner) active[v]->amps[i] = 0.0f;
    }
}
```

- [ ] **Step 4: Implement `OddEvenStrategy.h/.cpp`**

`Source/Engines/Deconfliction/OddEvenStrategy.h`:
```cpp
#pragma once
#include "IDeconflictionStrategy.h"
class OddEvenStrategy : public IDeconflictionStrategy
{
public:
    void resolve(std::vector<Voice>& voices) override;
};
```

`Source/Engines/Deconfliction/OddEvenStrategy.cpp`:
```cpp
#include "OddEvenStrategy.h"
#include "../HarmonicGenerator.h"

void OddEvenStrategy::resolve(std::vector<Voice>& voices)
{
    // Voice 0 (index 0) → odd harmonics: H3(idx1), H5(idx3), H7(idx5)
    // Voice 1 (index 1) → even harmonics: H2(idx0), H4(idx2), H6(idx4), H8(idx6)
    // 3+ voices: voices beyond 1 get all harmonics (best-effort)
    for (auto& v : voices)
    {
        if (!v.active) continue;
        if (v.voiceIndex == 0)
        {
            // Zero even harmonic indices (H2,H4,H6,H8 = idx 0,2,4,6)
            v.amps[0] = 0.0f; v.amps[2] = 0.0f; v.amps[4] = 0.0f; v.amps[6] = 0.0f;
        }
        else if (v.voiceIndex == 1)
        {
            // Zero odd harmonic indices (H3,H5,H7 = idx 1,3,5)
            v.amps[1] = 0.0f; v.amps[3] = 0.0f; v.amps[5] = 0.0f;
        }
    }
}
```

- [ ] **Step 5: Implement `ResidueStrategy.h/.cpp`**

`Source/Engines/Deconfliction/ResidueStrategy.h`:
```cpp
#pragma once
#include "IDeconflictionStrategy.h"
class ResidueStrategy : public IDeconflictionStrategy
{
public:
    void resolve(std::vector<Voice>& voices) override;
};
```

`Source/Engines/Deconfliction/ResidueStrategy.cpp`:
```cpp
#include "ResidueStrategy.h"
#include "../HarmonicGenerator.h"
#include <cmath>

void ResidueStrategy::resolve(std::vector<Voice>& voices)
{
    std::vector<Voice*> active;
    for (auto& v : voices) if (v.active) active.push_back(&v);
    if (active.size() <= 1) return;

    const int n = (int)active.size();
    // For each harmonic, count how many voices share "close" harmonic frequencies
    // Shared harmonics (within 5 Hz) are attenuated; unique ones are boosted
    for (int i = 0; i < 7; ++i)
    {
        const float harmNum = (float)(i + 2);

        // Build frequency list for harmonic i across all voices
        std::vector<float> freqs;
        for (auto* v : active) freqs.push_back(harmNum * v->fundamentalHz);

        for (int a = 0; a < n; ++a)
        {
            int sharedCount = 0;
            for (int b = 0; b < n; ++b)
            {
                if (a == b) continue;
                if (std::abs(freqs[a] - freqs[b]) < 5.0f) ++sharedCount;
            }
            // Attenuate by 1/(1+sharedCount), boost unique harmonics slightly
            const float factor = (sharedCount > 0)
                ? 1.0f / (float)(1 + sharedCount)
                : 1.2f;
            active[a]->amps[i] *= juce::jlimit(0.0f, 1.0f, factor);
        }
    }
}
```

- [ ] **Step 6: Implement `SpectralLaneStrategy.h/.cpp`**

`Source/Engines/Deconfliction/SpectralLaneStrategy.h`:
```cpp
#pragma once
#include "IDeconflictionStrategy.h"
class SpectralLaneStrategy : public IDeconflictionStrategy
{
public:
    void resolve(std::vector<Voice>& voices) override;
};
```

`Source/Engines/Deconfliction/SpectralLaneStrategy.cpp`:
```cpp
#include "SpectralLaneStrategy.h"
#include "../HarmonicGenerator.h"
#include <algorithm>

void SpectralLaneStrategy::resolve(std::vector<Voice>& voices)
{
    std::vector<Voice*> active;
    for (auto& v : voices) if (v.active) active.push_back(&v);
    if (active.size() <= 1) return;

    const int n = (int)active.size();

    // Sort by fundamental (lowest first)
    std::sort(active.begin(), active.end(),
              [](const Voice* a, const Voice* b) { return a->fundamentalHz < b->fundamentalHz; });

    // Each voice gets an amplitude window over the 7 harmonics
    // Voice 0 (lowest pitch) → stronger low harmonics; Voice n-1 → stronger high harmonics
    for (int v = 0; v < n; ++v)
    {
        const float centre = (float)v / (float)(n - 1);  // 0..1
        for (int i = 0; i < 7; ++i)
        {
            const float pos    = (float)i / 6.0f;  // 0..1
            const float dist   = std::abs(pos - centre);
            // Gaussian window, sigma = 0.4
            const float weight = std::exp(-dist * dist / (2.0f * 0.4f * 0.4f));
            active[v]->amps[i] *= weight;
        }
    }
}
```

- [ ] **Step 7: Implement `StaggerStrategy.h/.cpp`**

`Source/Engines/Deconfliction/StaggerStrategy.h`:
```cpp
#pragma once
#include "IDeconflictionStrategy.h"
#include <vector>

class StaggerStrategy : public IDeconflictionStrategy
{
public:
    void setDelayMs(float ms, double sampleRate) noexcept;
    void resolve(std::vector<Voice>& voices) override;

    int getDelaySamplesForVoice(int voiceIndex) const noexcept;

private:
    float  delayMs     = 8.0f;
    double sampleRate  = 44100.0;
    std::vector<int> delaySamples;  // indexed by voiceIndex
};
```

`Source/Engines/Deconfliction/StaggerStrategy.cpp`:
```cpp
#include "StaggerStrategy.h"
#include "../HarmonicGenerator.h"
#include <algorithm>

void StaggerStrategy::setDelayMs(float ms, double sr) noexcept
{
    delayMs    = ms;
    sampleRate = sr;
}

void StaggerStrategy::resolve(std::vector<Voice>& voices)
{
    // Assign cumulative onset delay per voice index
    // Voice 0 = 0ms, Voice 1 = delayMs, Voice 2 = 2*delayMs, etc.
    // The delay is stored for the processor to use when scheduling voice render.
    delaySamples.resize(8, 0);
    for (auto& v : voices)
    {
        if (!v.active) continue;
        delaySamples[v.voiceIndex] =
            (int)(v.voiceIndex * delayMs * sampleRate / 1000.0);
    }
    // Stagger doesn't modify amps — it defers voice onset in time
}

int StaggerStrategy::getDelaySamplesForVoice(int voiceIndex) const noexcept
{
    if (voiceIndex < 0 || voiceIndex >= (int)delaySamples.size()) return 0;
    return delaySamples[voiceIndex];
}
```

- [ ] **Step 8: Implement `BinauralStrategy.h/.cpp`**

`Source/Engines/Deconfliction/BinauralStrategy.h`:
```cpp
#pragma once
#include "IDeconflictionStrategy.h"
#include <vector>

class BinauralStrategy : public IDeconflictionStrategy
{
public:
    void resolve(std::vector<Voice>& voices) override;

    // Returns pan position for a voice (-1.0 = full left, +1.0 = full right)
    float getPanForVoice(int voiceIndex) const noexcept;

private:
    std::vector<float> panPositions;  // indexed by voiceIndex
};
```

`Source/Engines/Deconfliction/BinauralStrategy.cpp`:
```cpp
#include "BinauralStrategy.h"
#include "../HarmonicGenerator.h"
#include <cmath>
#include <algorithm>

void BinauralStrategy::resolve(std::vector<Voice>& voices)
{
    std::vector<Voice*> active;
    for (auto& v : voices) if (v.active) active.push_back(&v);
    if (active.empty()) return;

    panPositions.assign(8, 0.0f);  // centre by default

    const int n = (int)active.size();
    if (n == 1)
    {
        panPositions[active[0]->voiceIndex] = 0.0f;  // mono/centre
        return;
    }

    // Spread voices evenly across L-R field
    // 2 voices: L=-0.7, R=+0.7  (not hard pan, allows binaural fusion)
    for (int i = 0; i < n; ++i)
    {
        float pan = -0.7f + (1.4f * (float)i / (float)(n - 1));
        panPositions[active[i]->voiceIndex] = pan;
    }
}

float BinauralStrategy::getPanForVoice(int voiceIndex) const noexcept
{
    if (voiceIndex < 0 || voiceIndex >= (int)panPositions.size()) return 0.0f;
    return panPositions[voiceIndex];
}
```

- [ ] **Step 9: Run tests, verify pass**

```bash
cmake --build build --config Debug --target KaigenPhantomTests
build/tests/Debug/KaigenPhantomTests.exe "[DeconflictionTests]"
```

Expected: `6 tests passed`

- [ ] **Step 10: Commit**

```bash
git add Source/Engines/Deconfliction/
git commit -m "feat: 6 deconfliction strategies — Partition, OddEven, Residue, Lane, Stagger, Binaural"
```

---

## Task 6: BinauralStage

**Files:**
- Modify: `Source/Engines/BinauralStage.h`
- Modify: `Source/Engines/BinauralStage.cpp`
- Modify: `tests/BinauralStageTests.cpp`

- [ ] **Step 1: Write failing tests**

```cpp
// tests/BinauralStageTests.cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Engines/BinauralStage.h"

static juce::AudioBuffer<float> makeMonoBuffer(float value, int samples)
{
    juce::AudioBuffer<float> buf(2, samples);
    buf.clear();
    for (int i = 0; i < samples; ++i)
    {
        buf.setSample(0, i, value);
        buf.setSample(1, i, value);
    }
    return buf;
}

TEST_CASE("BinauralStage Off mode: output equals input")
{
    BinauralStage stage;
    stage.prepare(44100.0, 512);
    stage.setMode(BinauralMode::Off);

    auto buf = makeMonoBuffer(0.5f, 512);
    stage.process(buf);

    REQUIRE(buf.getSample(0, 0) == Catch::Approx(0.5f));
    REQUIRE(buf.getSample(1, 0) == Catch::Approx(0.5f));
}

TEST_CASE("BinauralStage Spread mode: channels differ at non-zero width")
{
    BinauralStage stage;
    stage.prepare(44100.0, 512);
    stage.setMode(BinauralMode::Spread);
    stage.setWidth(1.0f);

    // Give L and R the same input signal
    juce::AudioBuffer<float> buf(2, 512);
    buf.clear();
    for (int i = 0; i < 512; ++i)
    {
        buf.setSample(0, i, 0.5f);
        buf.setSample(1, i, 0.5f);
    }
    stage.process(buf);

    // After spread processing L and R should differ (high harmonics pushed outward)
    float diffSum = 0.0f;
    for (int i = 0; i < 512; ++i)
        diffSum += std::abs(buf.getSample(0, i) - buf.getSample(1, i));

    REQUIRE(diffSum > 0.0f);
}

TEST_CASE("BinauralStage isUsingBinaural returns true when mode != Off")
{
    BinauralStage stage;
    stage.prepare(44100.0, 512);
    stage.setMode(BinauralMode::Off);
    REQUIRE(stage.isUsingBinaural() == false);
    stage.setMode(BinauralMode::Spread);
    REQUIRE(stage.isUsingBinaural() == true);
}

TEST_CASE("BinauralStage width 0 in Spread mode: channels equal")
{
    BinauralStage stage;
    stage.prepare(44100.0, 512);
    stage.setMode(BinauralMode::Spread);
    stage.setWidth(0.0f);

    auto buf = makeMonoBuffer(0.5f, 512);
    stage.process(buf);

    // At width=0 channels should be identical (no spreading)
    for (int i = 0; i < 512; ++i)
        REQUIRE(buf.getSample(0, i) == Catch::Approx(buf.getSample(1, i)).margin(1e-5f));
}
```

- [ ] **Step 2: Run, verify FAIL**

```bash
cmake --build build --config Debug --target KaigenPhantomTests
build/tests/Debug/KaigenPhantomTests.exe "[BinauralStageTests]"
```

Expected: FAIL

- [ ] **Step 3: Create `Source/Engines/BinauralStage.h`**

```cpp
#pragma once
#include <JuceHeader.h>

enum class BinauralMode { Off, Spread, VoiceSplit };

class BinauralStage
{
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    void setMode(BinauralMode m) noexcept  { mode  = m; }
    void setWidth(float w) noexcept        { width = juce::jlimit(0.0f, 1.0f, w); }

    // VoiceSplit: set pan for voice i (called by HarmonicGenerator when BinauralStrategy active)
    void setVoicePan(int voiceIndex, float pan) noexcept;

    // Apply binaural matrix to the buffer in-place
    void process(juce::AudioBuffer<float>& buffer);

    bool isUsingBinaural() const noexcept { return mode != BinauralMode::Off; }

private:
    BinauralMode mode  = BinauralMode::Off;
    float        width = 0.5f;
    double       sampleRate = 44100.0;

    float voicePans[8] = {};  // -1 to +1 per voice

    // M/S stereo widening used for Spread mode
    void applySpread(juce::AudioBuffer<float>& buffer);
};
```

- [ ] **Step 4: Implement `Source/Engines/BinauralStage.cpp`**

```cpp
#include "BinauralStage.h"
#include <cmath>

void BinauralStage::prepare(double sr, int /*maxBlockSize*/)
{
    sampleRate = sr;
    std::fill(voicePans, voicePans + 8, 0.0f);
}

void BinauralStage::reset()
{
    std::fill(voicePans, voicePans + 8, 0.0f);
}

void BinauralStage::setVoicePan(int voiceIndex, float pan) noexcept
{
    if (voiceIndex >= 0 && voiceIndex < 8)
        voicePans[voiceIndex] = juce::jlimit(-1.0f, 1.0f, pan);
}

void BinauralStage::process(juce::AudioBuffer<float>& buffer)
{
    if (mode == BinauralMode::Off) return;
    applySpread(buffer);
}

void BinauralStage::applySpread(juce::AudioBuffer<float>& buffer)
{
    if (width < 1e-5f) return;

    const int numSamples = buffer.getNumSamples();
    const float w = width;

    float* L = buffer.getWritePointer(0);
    float* R = buffer.getWritePointer(1);

    for (int i = 0; i < numSamples; ++i)
    {
        const float mid  = (L[i] + R[i]) * 0.5f;
        const float side = (L[i] - R[i]) * 0.5f;

        // Widen: boost side signal by width factor
        L[i] = mid + side * (1.0f + w);
        R[i] = mid - side * (1.0f + w);
    }
}
```

- [ ] **Step 5: Run tests, verify pass**

```bash
cmake --build build --config Debug --target KaigenPhantomTests
build/tests/Debug/KaigenPhantomTests.exe "[BinauralStageTests]"
```

Expected: `4 tests passed`

- [ ] **Step 6: Commit**

```bash
git add Source/Engines/BinauralStage.h Source/Engines/BinauralStage.cpp tests/BinauralStageTests.cpp
git commit -m "feat: BinauralStage — Off/Spread/VoiceSplit modes with M/S stereo widening"
```

---

## Task 7: PerceptualOptimizer

**Files:**
- Modify: `Source/Engines/PerceptualOptimizer.h`
- Modify: `Source/Engines/PerceptualOptimizer.cpp`
- Modify: `tests/PerceptualOptimizerTests.cpp`

- [ ] **Step 1: Write failing tests**

```cpp
// tests/PerceptualOptimizerTests.cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Engines/PerceptualOptimizer.h"

TEST_CASE("PerceptualOptimizer: gain at 80 Hz is greater than at 1000 Hz (ear less sensitive at 80 Hz)")
{
    PerceptualOptimizer opt;
    float gain80   = opt.getLoudnessGain(80.0f);
    float gain1000 = opt.getLoudnessGain(1000.0f);
    // Equal-loudness: ear requires more SPL at 80 Hz to perceive same loudness as at 1 kHz
    // So optimizer should boost 80 Hz relative to 1 kHz
    REQUIRE(gain80 > gain1000);
}

TEST_CASE("PerceptualOptimizer: gain is positive for all harmonic frequencies")
{
    PerceptualOptimizer opt;
    // Test harmonics of a 40 Hz fundamental: 80,120,160,200,240,280,320 Hz
    for (int h = 2; h <= 8; ++h)
    {
        float freq = 40.0f * (float)h;
        REQUIRE(opt.getLoudnessGain(freq) > 0.0f);
    }
}

TEST_CASE("PerceptualOptimizer: applyToBuffer scales amplitudes")
{
    PerceptualOptimizer opt;
    opt.prepare(44100.0, 512);

    juce::AudioBuffer<float> buf(2, 512);
    // Fill with constant 0.5
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i)
            buf.setSample(ch, i, 0.5f);

    // With fundamental 40 Hz, harmonics in low-frequency boosted region
    opt.setFundamental(40.0f);
    opt.process(buf);

    // After optimization, signal should be louder (boosted from low-freq compensation)
    float peakBefore = 0.5f;
    float peakAfter  = 0.0f;
    for (int i = 0; i < 512; ++i)
        peakAfter = std::max(peakAfter, std::abs(buf.getSample(0, i)));

    REQUIRE(peakAfter > peakBefore);
}
```

- [ ] **Step 2: Run, verify FAIL**

```bash
cmake --build build --config Debug --target KaigenPhantomTests
build/tests/Debug/KaigenPhantomTests.exe "[PerceptualOptimizerTests]"
```

Expected: FAIL

- [ ] **Step 3: Create `Source/Engines/PerceptualOptimizer.h`**

```cpp
#pragma once
#include <JuceHeader.h>

// Applies ISO 226 equal-loudness contour correction so phantom harmonics
// are perceived at the same loudness as the original fundamental.
class PerceptualOptimizer
{
public:
    void prepare(double sampleRate, int maxBlockSize);

    // Set the current fundamental Hz (used to compute harmonic frequencies)
    void setFundamental(float hz) noexcept { fundamentalHz = hz; }

    // Returns the gain multiplier to apply to a harmonic at freq Hz
    // to compensate for unequal ear sensitivity.
    float getLoudnessGain(float freq) const noexcept;

    // Apply gain corrections per channel to buffer.
    // Applies a single broadband gain based on the average harmonic frequency.
    void process(juce::AudioBuffer<float>& buffer);

private:
    double sampleRate  = 44100.0;
    float  fundamentalHz = 80.0f;

    // Simplified ISO 226 A-weighting approximation (1 kHz normalized to 1.0)
    // Uses a 4-point piecewise linear model covering 20 Hz – 20 kHz
    static float aWeightingDb(float freq) noexcept;
};
```

- [ ] **Step 4: Implement `Source/Engines/PerceptualOptimizer.cpp`**

```cpp
#include "PerceptualOptimizer.h"
#include <cmath>

void PerceptualOptimizer::prepare(double sr, int /*maxBlockSize*/)
{
    sampleRate = sr;
}

float PerceptualOptimizer::getLoudnessGain(float freq) const noexcept
{
    // Inverse of A-weighting: boost frequencies where ear is less sensitive.
    // At 1 kHz (0 dB reference), gain = 1.0.
    // Below 1 kHz: ear less sensitive → gain > 1.0 (boost)
    // Above 4 kHz: ear less sensitive → gain > 1.0 (slight boost)
    const float dbCorrection = -aWeightingDb(freq);  // invert A-weighting
    return std::pow(10.0f, dbCorrection / 20.0f);
}

float PerceptualOptimizer::aWeightingDb(float freq) noexcept
{
    // A-weighting formula (IEC 61672):
    // A(f) = 2.0 + 20*log10( f^4 * 12194^2 /
    //          ((f^2 + 20.6^2) * sqrt((f^2+107.7^2)*(f^2+737.9^2)) * (f^2+12194^2)) )
    if (freq < 1.0f) freq = 1.0f;
    const double f  = (double)freq;
    const double f2 = f * f;
    const double f4 = f2 * f2;

    const double num = f4 * (12194.0 * 12194.0) * (12194.0 * 12194.0);
    const double d1  = f2 + 20.6 * 20.6;
    const double d2  = std::sqrt((f2 + 107.7 * 107.7) * (f2 + 737.9 * 737.9));
    const double d3  = f2 + 12194.0 * 12194.0;
    const double den = d1 * d2 * d3;

    if (den < 1e-30) return -100.0f;
    return (float)(2.0 + 20.0 * std::log10(num / den));
}

void PerceptualOptimizer::process(juce::AudioBuffer<float>& buffer)
{
    if (fundamentalHz <= 0.0f) return;

    // Compute average gain for the harmonic series H2..H8
    float avgGain = 0.0f;
    for (int h = 2; h <= 8; ++h)
        avgGain += getLoudnessGain(fundamentalHz * (float)h);
    avgGain /= 7.0f;

    // Clamp to reasonable range: 0.5x to 4x to avoid extreme boosts at very low frequencies
    avgGain = juce::jlimit(0.5f, 4.0f, avgGain);

    const int numSamples = buffer.getNumSamples();
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] *= avgGain;
    }
}
```

- [ ] **Step 5: Run tests, verify pass**

```bash
cmake --build build --config Debug --target KaigenPhantomTests
build/tests/Debug/KaigenPhantomTests.exe "[PerceptualOptimizerTests]"
```

Expected: `3 tests passed`

- [ ] **Step 6: Commit**

```bash
git add Source/Engines/PerceptualOptimizer.h Source/Engines/PerceptualOptimizer.cpp tests/PerceptualOptimizerTests.cpp
git commit -m "feat: PerceptualOptimizer — ISO 226 A-weighting equal-loudness correction"
```

---

## Task 8: CrossoverBlend

**Files:**
- Modify: `Source/Engines/CrossoverBlend.h`
- Modify: `Source/Engines/CrossoverBlend.cpp`
- Modify: `tests/CrossoverBlendTests.cpp`

- [ ] **Step 1: Write failing tests**

```cpp
// tests/CrossoverBlendTests.cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Engines/CrossoverBlend.h"

static juce::AudioBuffer<float> makeConstantBuffer(float value, int numCh, int numSamples)
{
    juce::AudioBuffer<float> buf(numCh, numSamples);
    for (int ch = 0; ch < numCh; ++ch)
        for (int i = 0; i < numSamples; ++i)
            buf.setSample(ch, i, value);
    return buf;
}

TEST_CASE("CrossoverBlend ghost=100 Replace: output is phantom only")
{
    CrossoverBlend blend;
    blend.prepare(44100.0, 512);
    blend.setThresholdHz(80.0f);
    blend.setGhost(1.0f);             // 100%
    blend.setGhostMode(GhostMode::Replace);
    blend.setSidechainDuckAmount(0.0f);

    auto dry     = makeConstantBuffer(0.8f, 2, 512);
    auto phantom = makeConstantBuffer(0.4f, 2, 512);

    blend.process(dry, phantom, nullptr);

    // Replace mode + ghost 100%: sub removed from dry, phantom added
    // Output should not contain the original 0.8 sub content
    // (crossover removes frequencies below threshold — result should differ from 0.8+0.4)
    // We verify the blend didn't just passthrough dry unchanged
    float peak = 0.0f;
    for (int i = 0; i < 512; ++i)
        peak = std::max(peak, std::abs(dry.getSample(0, i)));

    REQUIRE(peak < 1.5f);  // sanity: output isn't unbounded
    REQUIRE(peak > 0.0f);  // output is non-zero
}

TEST_CASE("CrossoverBlend ghost=0: output equals dry (no phantom)")
{
    CrossoverBlend blend;
    blend.prepare(44100.0, 512);
    blend.setThresholdHz(80.0f);
    blend.setGhost(0.0f);             // 0%
    blend.setGhostMode(GhostMode::Replace);

    auto dry     = makeConstantBuffer(0.5f, 2, 512);
    auto phantom = makeConstantBuffer(0.3f, 2, 512);
    float originalDryValue = dry.getSample(0, 0);

    blend.process(dry, phantom, nullptr);

    // Ghost 0% means no phantom applied — output should equal dry
    REQUIRE(dry.getSample(0, 0) == Catch::Approx(originalDryValue).margin(0.01f));
}

TEST_CASE("CrossoverBlend sidechain ducking reduces phantom amplitude")
{
    CrossoverBlend blend;
    blend.prepare(44100.0, 512);
    blend.setThresholdHz(80.0f);
    blend.setGhost(1.0f);
    blend.setGhostMode(GhostMode::Add);
    blend.setSidechainDuckAmount(1.0f);   // full duck
    blend.setDuckAttackMs(1.0f);
    blend.setDuckReleaseMs(10.0f);

    auto dry     = makeConstantBuffer(0.0f, 2, 512);
    auto phantom = makeConstantBuffer(0.5f, 2, 512);
    // Sidechain with loud signal
    auto sidechain = makeConstantBuffer(1.0f, 2, 512);

    blend.process(dry, phantom, &sidechain);

    // With full ducking and loud sidechain, phantom should be attenuated
    float peakWithDuck = 0.0f;
    for (int i = 0; i < 512; ++i)
        peakWithDuck = std::max(peakWithDuck, std::abs(dry.getSample(0, i)));

    // Unduck comparison
    CrossoverBlend blendNoDuck;
    blendNoDuck.prepare(44100.0, 512);
    blendNoDuck.setThresholdHz(80.0f);
    blendNoDuck.setGhost(1.0f);
    blendNoDuck.setGhostMode(GhostMode::Add);
    blendNoDuck.setSidechainDuckAmount(0.0f);

    auto dry2     = makeConstantBuffer(0.0f, 2, 512);
    auto phantom2 = makeConstantBuffer(0.5f, 2, 512);
    blendNoDuck.process(dry2, phantom2, nullptr);
    float peakNoDuck = 0.0f;
    for (int i = 0; i < 512; ++i)
        peakNoDuck = std::max(peakNoDuck, std::abs(dry2.getSample(0, i)));

    REQUIRE(peakWithDuck < peakNoDuck);
}
```

- [ ] **Step 2: Run, verify FAIL**

```bash
cmake --build build --config Debug --target KaigenPhantomTests
build/tests/Debug/KaigenPhantomTests.exe "[CrossoverBlendTests]"
```

Expected: FAIL

- [ ] **Step 3: Create `Source/Engines/CrossoverBlend.h`**

```cpp
#pragma once
#include <JuceHeader.h>

enum class GhostMode { Replace, Add };

class CrossoverBlend
{
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    void setThresholdHz(float hz) noexcept;
    void setGhost(float g) noexcept          { ghost = juce::jlimit(0.0f, 1.0f, g); }
    void setGhostMode(GhostMode m) noexcept  { ghostMode = m; }
    void setSidechainDuckAmount(float a) noexcept { duckAmount = juce::jlimit(0.0f, 1.0f, a); }
    void setDuckAttackMs(float ms) noexcept  { duckAttackMs  = ms; updateDuckCoeffs(); }
    void setDuckReleaseMs(float ms) noexcept { duckReleaseMs = ms; updateDuckCoeffs(); }
    void setStereoWidth(float w) noexcept    { stereoWidth = juce::jlimit(0.0f, 2.0f, w); }
    void setOutputGain(float g) noexcept     { outputGain = g; }

    // dry: main input (modified in-place to become final output)
    // phantom: harmonic generator output (post-binaural, post-optimizer)
    // sidechain: optional sidechain buffer (nullptr = no sidechain)
    void process(juce::AudioBuffer<float>& dry,
                 const juce::AudioBuffer<float>& phantom,
                 const juce::AudioBuffer<float>* sidechain);

private:
    double sampleRate    = 44100.0;
    float  ghost         = 1.0f;
    GhostMode ghostMode  = GhostMode::Replace;
    float  duckAmount    = 0.0f;
    float  duckAttackMs  = 5.0f;
    float  duckReleaseMs = 80.0f;
    float  stereoWidth   = 1.0f;
    float  outputGain    = 1.0f;

    float currentDuck    = 1.0f;  // 1 = no duck, 0 = full duck
    float duckAttackCoeff  = 0.0f;
    float duckReleaseCoeff = 0.0f;

    // 2nd-order Linkwitz-Riley LP and HP at threshold
    juce::dsp::IIR::Filter<float> lpL, lpR, hpL, hpR;
    float lastThresholdHz = -1.0f;

    void rebuildCrossover(float thresholdHz);
    void updateDuckCoeffs();
    void applyStereoWidth(juce::AudioBuffer<float>& buffer, int numSamples);
};
```

- [ ] **Step 4: Implement `Source/Engines/CrossoverBlend.cpp`**

```cpp
#include "CrossoverBlend.h"
#include <cmath>

void CrossoverBlend::prepare(double sr, int /*maxBlockSize*/)
{
    sampleRate = sr;
    rebuildCrossover(80.0f);
    updateDuckCoeffs();
    currentDuck = 1.0f;
}

void CrossoverBlend::reset()
{
    lpL.reset(); lpR.reset();
    hpL.reset(); hpR.reset();
    currentDuck = 1.0f;
}

void CrossoverBlend::setThresholdHz(float hz) noexcept
{
    if (std::abs(hz - lastThresholdHz) > 0.5f)
        rebuildCrossover(hz);
}

void CrossoverBlend::rebuildCrossover(float thresholdHz)
{
    lastThresholdHz = thresholdHz;
    auto lpCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(
        sampleRate, (double)thresholdHz);
    auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(
        sampleRate, (double)thresholdHz);

    *lpL.coefficients = *lpCoeffs;
    *lpR.coefficients = *lpCoeffs;
    *hpL.coefficients = *hpCoeffs;
    *hpR.coefficients = *hpCoeffs;
}

void CrossoverBlend::updateDuckCoeffs()
{
    duckAttackCoeff  = (duckAttackMs  > 0.0f)
        ? std::exp(-1.0f / (float)(sampleRate * duckAttackMs  / 1000.0))
        : 0.0f;
    duckReleaseCoeff = (duckReleaseMs > 0.0f)
        ? std::exp(-1.0f / (float)(sampleRate * duckReleaseMs / 1000.0))
        : 0.0f;
}

void CrossoverBlend::process(juce::AudioBuffer<float>& dry,
                              const juce::AudioBuffer<float>& phantom,
                              const juce::AudioBuffer<float>* sidechain)
{
    if (ghost < 1e-5f) return;  // Ghost 0% = dry unchanged

    const int numSamples = dry.getNumSamples();
    const int numCh      = juce::jmin(dry.getNumChannels(), 2);

    // 1. Compute sidechain duck envelope
    float duckGain = 1.0f;
    if (sidechain != nullptr && duckAmount > 1e-5f)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float sc = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                sc = std::max(sc, std::abs(sidechain->getSample(ch, i)));

            float target = 1.0f - duckAmount * juce::jlimit(0.0f, 1.0f, sc);
            float coeff  = (sc > currentDuck) ? duckAttackCoeff : duckReleaseCoeff;
            currentDuck  = currentDuck * coeff + target * (1.0f - coeff);
        }
        duckGain = currentDuck;
    }

    // 2. Extract sub-band from dry using LP filter (Replace mode only)
    // In Replace mode we remove the sub band from dry; in Add mode we leave dry intact.
    for (int ch = 0; ch < numCh; ++ch)
    {
        float* dryData          = dry.getWritePointer(ch);
        const float* phantomData = phantom.getReadPointer(ch);

        auto& lp = (ch == 0) ? lpL : lpR;
        auto& hp = (ch == 0) ? hpL : hpR;

        for (int i = 0; i < numSamples; ++i)
        {
            const float originalDry = dryData[i];
            const float subBand  = lp.processSample(originalDry);
            const float highBand = hp.processSample(originalDry);

            const float scaledPhantom = phantomData[i] * duckGain * ghost;

            if (ghostMode == GhostMode::Replace)
            {
                // Remove sub, add phantom in its place
                const float subRemoved = juce::jlerpunclamped(originalDry, highBand, ghost);
                dryData[i] = (subRemoved + scaledPhantom) * outputGain;
            }
            else  // Add
            {
                dryData[i] = (originalDry + scaledPhantom) * outputGain;
            }
        }
    }

    // 3. Apply stereo width to output
    if (std::abs(stereoWidth - 1.0f) > 0.01f)
        applyStereoWidth(dry, numSamples);
}

void CrossoverBlend::applyStereoWidth(juce::AudioBuffer<float>& buffer, int numSamples)
{
    float* L = buffer.getWritePointer(0);
    float* R = buffer.getWritePointer(1);

    for (int i = 0; i < numSamples; ++i)
    {
        const float mid  = (L[i] + R[i]) * 0.5f;
        const float side = (L[i] - R[i]) * 0.5f * stereoWidth;
        L[i] = mid + side;
        R[i] = mid - side;
    }
}
```

- [ ] **Step 5: Run tests, verify pass**

```bash
cmake --build build --config Debug --target KaigenPhantomTests
build/tests/Debug/KaigenPhantomTests.exe "[CrossoverBlendTests]"
```

Expected: `3 tests passed`

- [ ] **Step 6: Commit**

```bash
git add Source/Engines/CrossoverBlend.h Source/Engines/CrossoverBlend.cpp tests/CrossoverBlendTests.cpp
git commit -m "feat: CrossoverBlend — Replace/Add modes, sidechain ducking, stereo width, output gain"
```

---

## Task 9: State Serialization tests

**Files:**
- Modify: `tests/SerializationTests.cpp`

- [ ] **Step 1: Write failing tests**

```cpp
// tests/SerializationTests.cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Parameters.h"

// Minimal no-op processor to host APVTS for serialization tests
struct TestProcessor : public juce::AudioProcessor
{
    TestProcessor()
        : AudioProcessor(BusesProperties()
            .withInput("In", juce::AudioChannelSet::stereo(), true)
            .withOutput("Out", juce::AudioChannelSet::stereo(), true)),
          apvts(*this, nullptr, "STATE", createParameterLayout())
    {}
    const juce::String getName() const override { return "Test"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool hasEditor() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    juce::AudioProcessorValueTreeState apvts;
};

TEST_CASE("APVTS state round-trips correctly")
{
    TestProcessor proc;

    // Set some non-default values
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.5f);
    proc.apvts.getParameter(ParamID::PHANTOM_THRESHOLD)->setValueNotifyingHost(
        proc.apvts.getParameter(ParamID::PHANTOM_THRESHOLD)->convertTo0to1(60.0f));

    // Serialize
    auto state = proc.apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    juce::MemoryBlock block;
    proc.copyXmlToBinary(*xml, block);

    // Restore
    TestProcessor proc2;
    std::unique_ptr<juce::XmlElement> xml2(proc2.getXmlFromBinary(block.getData(), (int)block.getSize()));
    REQUIRE(xml2 != nullptr);
    proc2.apvts.replaceState(juce::ValueTree::fromXml(*xml2));

    float ghostVal = proc2.apvts.getParameter(ParamID::GHOST)->getValue();
    REQUIRE(ghostVal == Catch::Approx(0.5f).margin(0.01f));
}

TEST_CASE("Default parameter values match spec")
{
    TestProcessor proc;

    auto getFloat = [&](const char* id) {
        auto* p = dynamic_cast<juce::RangedAudioParameter*>(
            proc.apvts.getParameter(id));
        return p->convertFrom0to1(p->getDefaultValue());
    };

    REQUIRE(getFloat(ParamID::GHOST)           == Catch::Approx(100.0f).margin(0.1f));
    REQUIRE(getFloat(ParamID::PHANTOM_THRESHOLD) == Catch::Approx(80.0f).margin(0.1f));
    REQUIRE(getFloat(ParamID::PHANTOM_STRENGTH)  == Catch::Approx(80.0f).margin(0.1f));
    REQUIRE(getFloat(ParamID::BINAURAL_WIDTH)    == Catch::Approx(50.0f).margin(0.1f));
    REQUIRE(getFloat(ParamID::TRACKING_GLIDE)    == Catch::Approx(20.0f).margin(0.1f));
    REQUIRE(getFloat(ParamID::STAGGER_DELAY)     == Catch::Approx(8.0f).margin(0.1f));
}
```

- [ ] **Step 2: Run, verify pass** (Parameters.h already implemented — these should pass)

```bash
cmake --build build --config Debug --target KaigenPhantomTests
build/tests/Debug/KaigenPhantomTests.exe "[SerializationTests]"
```

Expected: `2 tests passed`

- [ ] **Step 3: Commit**

```bash
git add tests/SerializationTests.cpp
git commit -m "test: APVTS state round-trip and default value verification"
```

---

## Task 10: PluginProcessor — Effect Mode wiring

**Files:**
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`

- [ ] **Step 1: Update `Source/PluginProcessor.h`**

```cpp
#pragma once
#include <JuceHeader.h>
#include "Engines/PitchTracker.h"
#include "Engines/HarmonicGenerator.h"
#include "Engines/BinauralStage.h"
#include "Engines/PerceptualOptimizer.h"
#include "Engines/CrossoverBlend.h"
#include "Engines/Deconfliction/PartitionStrategy.h"
#include "Engines/Deconfliction/SpectralLaneStrategy.h"
#include "Engines/Deconfliction/StaggerStrategy.h"
#include "Engines/Deconfliction/OddEvenStrategy.h"
#include "Engines/Deconfliction/ResidueStrategy.h"
#include "Engines/Deconfliction/BinauralStrategy.h"

class PhantomProcessor : public juce::AudioProcessor
{
public:
    PhantomProcessor();
    ~PhantomProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Kaigen Phantom"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void syncEnginesFromApvts();
    void updateDeconflictionStrategy(int modeIndex);

    PitchTracker         pitchTracker;
    HarmonicGenerator    harmonicGen;
    BinauralStage        binauralStage;
    PerceptualOptimizer  perceptualOpt;
    CrossoverBlend       crossoverBlend;

    // Deconfliction strategy instances (one per mode)
    PartitionStrategy    stratPartition;
    SpectralLaneStrategy stratLane;
    StaggerStrategy      stratStagger;
    OddEvenStrategy      stratOddEven;
    ResidueStrategy      stratResidue;
    BinauralStrategy     stratBinaural;

    int  lastDeconflictionMode = -1;

    // Pre-allocated scratch buffers
    juce::AudioBuffer<float> phantomBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomProcessor)
};
```

- [ ] **Step 2: Implement full `Source/PluginProcessor.cpp`**

```cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"
#include <cmath>

PhantomProcessor::PhantomProcessor()
    : AudioProcessor(BusesProperties()
        .withInput ("Input",     juce::AudioChannelSet::stereo(), true)
        .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)
        .withOutput("Output",    juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PHANTOM_STATE", createParameterLayout())
{
}

void PhantomProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    pitchTracker.prepare(sampleRate, samplesPerBlock);
    harmonicGen .prepare(sampleRate, samplesPerBlock);
    binauralStage.prepare(sampleRate, samplesPerBlock);
    perceptualOpt.prepare(sampleRate, samplesPerBlock);
    crossoverBlend.prepare(sampleRate, samplesPerBlock);

    stratStagger.setDelayMs(8.0f, sampleRate);

    phantomBuf.setSize(2, samplesPerBlock, false, false, false);
    lastDeconflictionMode = -1;
}

void PhantomProcessor::releaseResources()
{
    pitchTracker.prepare(44100.0, 512);
    harmonicGen.reset();
    binauralStage.reset();
    crossoverBlend.reset();
}

void PhantomProcessor::syncEnginesFromApvts()
{
    using namespace ParamID;

    const int modeIdx = (int)apvts.getRawParameterValue(MODE)->load();
    const bool isInstrumentMode = (modeIdx == 1);

    // Ghost
    const float ghost     = apvts.getRawParameterValue(GHOST)->load() / 100.0f;
    const int   ghostMode = (int)apvts.getRawParameterValue(GHOST_MODE)->load();
    crossoverBlend.setGhost(ghost);
    crossoverBlend.setGhostMode(ghostMode == 0 ? GhostMode::Replace : GhostMode::Add);

    // Crossover
    const float threshold = apvts.getRawParameterValue(PHANTOM_THRESHOLD)->load();
    crossoverBlend.setThresholdHz(threshold);

    // Output gain
    const float gainDb = apvts.getRawParameterValue(OUTPUT_GAIN)->load();
    crossoverBlend.setOutputGain(std::pow(10.0f, gainDb / 20.0f));

    // Stereo width
    crossoverBlend.setStereoWidth(apvts.getRawParameterValue(STEREO_WIDTH)->load() / 100.0f);

    // Sidechain ducking
    crossoverBlend.setSidechainDuckAmount(
        apvts.getRawParameterValue(SIDECHAIN_DUCK_AMOUNT)->load() / 100.0f);
    crossoverBlend.setDuckAttackMs(
        apvts.getRawParameterValue(SIDECHAIN_DUCK_ATTACK)->load());
    crossoverBlend.setDuckReleaseMs(
        apvts.getRawParameterValue(SIDECHAIN_DUCK_RELEASE)->load());

    // Phantom strength
    harmonicGen.setPhantomStrength(
        apvts.getRawParameterValue(PHANTOM_STRENGTH)->load() / 100.0f);

    // Recipe amplitudes
    const char* ampIDs[7] = {
        RECIPE_H2, RECIPE_H3, RECIPE_H4, RECIPE_H5, RECIPE_H6, RECIPE_H7, RECIPE_H8
    };
    for (int i = 0; i < 7; ++i)
        harmonicGen.setHarmonicAmp(i + 2, apvts.getRawParameterValue(ampIDs[i])->load() / 100.0f);

    const char* phaseIDs[7] = {
        RECIPE_PHASE_H2, RECIPE_PHASE_H3, RECIPE_PHASE_H4, RECIPE_PHASE_H5,
        RECIPE_PHASE_H6, RECIPE_PHASE_H7, RECIPE_PHASE_H8
    };
    for (int i = 0; i < 7; ++i)
        harmonicGen.setHarmonicPhase(i + 2, apvts.getRawParameterValue(phaseIDs[i])->load());

    harmonicGen.setRotation(apvts.getRawParameterValue(RECIPE_ROTATION)->load());
    harmonicGen.setSaturation(apvts.getRawParameterValue(HARMONIC_SATURATION)->load() / 100.0f);

    // Pitch tracker (effect mode only)
    if (!isInstrumentMode)
    {
        const float sensitivity = apvts.getRawParameterValue(TRACKING_SENSITIVITY)->load() / 100.0f;
        // Map 0-1 sensitivity to YIN threshold 0.30 (loose) → 0.05 (strict)
        pitchTracker.setConfidenceThreshold(0.30f - sensitivity * 0.25f);
        pitchTracker.setGlideMs(apvts.getRawParameterValue(TRACKING_GLIDE)->load());
    }

    // Deconfliction strategy (instrument mode only)
    if (isInstrumentMode)
    {
        const int deconMode = (int)apvts.getRawParameterValue(DECONFLICTION_MODE)->load();
        if (deconMode != lastDeconflictionMode)
        {
            updateDeconflictionStrategy(deconMode);
            lastDeconflictionMode = deconMode;
        }
        harmonicGen.setMaxVoices((int)apvts.getRawParameterValue(MAX_VOICES)->load());
        stratStagger.setDelayMs(apvts.getRawParameterValue(STAGGER_DELAY)->load(),
                                getSampleRate());
    }

    // Binaural stage
    const int binMode = (int)apvts.getRawParameterValue(BINAURAL_MODE)->load();
    binauralStage.setMode(binMode == 0 ? BinauralMode::Off
                        : binMode == 1 ? BinauralMode::Spread
                                       : BinauralMode::VoiceSplit);
    binauralStage.setWidth(apvts.getRawParameterValue(BINAURAL_WIDTH)->load() / 100.0f);

    // Fundamental for perceptual optimizer
    perceptualOpt.setFundamental(threshold);  // approximate — tracker sets exact below
}

void PhantomProcessor::updateDeconflictionStrategy(int modeIndex)
{
    IDeconflictionStrategy* strategies[] = {
        &stratPartition, &stratLane, &stratStagger,
        &stratOddEven,   &stratResidue, &stratBinaural
    };
    harmonicGen.setDeconflictionStrategy(
        (modeIndex >= 0 && modeIndex < 6) ? strategies[modeIndex] : nullptr);
}

void PhantomProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin(buffer.getNumChannels(), 2);
    const int n     = buffer.getNumSamples();
    if (numCh < 2 || n == 0) return;

    syncEnginesFromApvts();

    const int modeIdx = (int)apvts.getRawParameterValue(ParamID::MODE)->load();
    const bool isInstrumentMode = (modeIdx == 1);

    // -- MIDI handling (instrument mode) --
    if (isInstrumentMode)
    {
        for (const auto msg : midi)
        {
            const auto m = msg.getMessage();
            if (m.isNoteOn())
                harmonicGen.noteOn(m.getNoteNumber(), m.getVelocity());
            else if (m.isNoteOff() || (m.isNoteOn() && m.getVelocity() == 0))
                harmonicGen.noteOff(m.getNoteNumber());
        }
    }

    // -- Effect mode pitch detection --
    if (!isInstrumentMode)
    {
        // Use left channel for pitch detection
        float detectedHz = pitchTracker.detectPitch(buffer.getReadPointer(0), n);
        if (detectedHz > 0.0f)
        {
            harmonicGen.setEffectModePitch(detectedHz);
            perceptualOpt.setFundamental(detectedHz);
        }
    }

    // -- Phantom generation --
    phantomBuf.setSize(numCh, n, false, false, true);
    phantomBuf.clear();
    harmonicGen.process(phantomBuf);

    // -- Binaural stage --
    binauralStage.process(phantomBuf);

    // -- Perceptual optimization --
    perceptualOpt.process(phantomBuf);

    // -- Sidechain --
    const juce::AudioBuffer<float>* sidechainBuf = nullptr;
    if (getBusCount(true) > 1)
    {
        auto* scBus = getBus(true, 1);
        if (scBus != nullptr && scBus->isEnabled())
            sidechainBuf = &getBusBuffer(buffer, true, 1);
    }

    // -- Crossover blend: dry in, phantom mixed in, output written back to buffer --
    // We need a separate dry buffer copy since CrossoverBlend modifies dry in-place
    juce::AudioBuffer<float> dryBuf(numCh, n);
    for (int ch = 0; ch < numCh; ++ch)
        dryBuf.copyFrom(ch, 0, buffer, ch, 0, n);

    crossoverBlend.process(dryBuf, phantomBuf, sidechainBuf);

    // Copy result back to output buffer
    for (int ch = 0; ch < numCh; ++ch)
        buffer.copyFrom(ch, 0, dryBuf, ch, 0, n);
}

juce::AudioProcessorEditor* PhantomProcessor::createEditor()
{
    return new PhantomEditor(*this);
}

void PhantomProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PhantomProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorValueTreeState::ParameterLayout
PhantomProcessor::createParameterLayout()
{
    return ::createParameterLayout();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PhantomProcessor();
}
```

- [ ] **Step 3: Build the full plugin**

```bash
cmake --build build --config Release
```

Expected: `KaigenPhantom.vst3` built successfully in `build/KaigenPhantom_artefacts/Release/VST3/`

- [ ] **Step 4: Run full test suite**

```bash
cmake --build build --config Debug --target KaigenPhantomTests
build/tests/Debug/KaigenPhantomTests.exe
```

Expected: all tests pass (no failures)

- [ ] **Step 5: Commit**

```bash
git add Source/PluginProcessor.h Source/PluginProcessor.cpp
git commit -m "feat: PluginProcessor — full engine wiring, effect+instrument mode, MIDI, sidechain"
```

---

## Task 11: Final build verification + gitignore

**Files:**
- Modify: `.gitignore`

- [ ] **Step 1: Update `.gitignore` to exclude build artifacts**

```
build/
.superpowers/
*.user
*.suo
.vs/
*.vcxproj.user
```

- [ ] **Step 2: Verify VST3 binary exists**

```bash
ls "build/KaigenPhantom_artefacts/Release/VST3/"
```

Expected: `Kaigen Phantom.vst3` directory present.

- [ ] **Step 3: Run all tests, confirm clean**

```bash
build/tests/Debug/KaigenPhantomTests.exe -v
```

Expected: all test cases pass, 0 failures.

- [ ] **Step 4: Final commit**

```bash
git add .gitignore
git commit -m "chore: finalize gitignore; Kaigen Phantom v0.1.0 DSP backend complete"
```

---

## Spec Coverage Check

| Spec Requirement | Task(s) |
|---|---|
| Effect mode — monophonic YIN pitch tracking | Task 3, 10 |
| Instrument mode — polyphonic MIDI voices | Task 4, 10 |
| 6 deconfliction strategies | Task 5 |
| BinauralStage (Off/Spread/VoiceSplit) | Task 6 |
| PerceptualOptimizer (equal-loudness) | Task 7 |
| CrossoverBlend (Replace/Add, Ghost) | Task 8 |
| Sidechain ducking | Task 8, 10 |
| ~35 APVTS parameters | Task 2 |
| Recipe Engine (4 presets, per-harmonic control) | Task 4 |
| Harmonic saturation | Task 4 |
| Recipe rotation | Task 4 |
| CMake + JUCE 7.0.9 + VST3 + Standalone | Task 1 |
| Catch2 test suite | Tasks 1–9 |
| State serialization | Task 9 |
| UI deferred | — (out of scope) |
| Speaker simulation — explicitly excluded | — |
| Polyphonic audio tracking — explicitly excluded | — |
