# A/B Compare Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add Standard-build A/B snap compare to Kaigen Phantom — two independent slots, binary snap toggle, copy, preset save/load with `<SlotB>` extension, browser column, settings toggle — with an architectural seam (`ABSlotManager::getSlot` + `<MorphConfig>` preset child) for a future Pro Morph Module.

**Architecture:** A new `ABSlotManager` class owned by `PhantomProcessor` holds two `juce::ValueTree` snapshots plus an active-slot index. `snapTo()` commits live APVTS → active slot, then `replaceState()`s live from the target slot. Discrete params (Mode, Bypass, Ghost Mode, Binaural Mode) are excluded from ad-hoc snap by default; the exclusion is bypassed when a designer-authored A/B preset is loaded. `PresetManager` gains a `presetKind` enum and writes/reads an optional `<SlotB>` child in the preset file. Frontend logic lives in the existing `preset-system.js` IIFE (no new JS file). Pro-only code paths are gated by `#ifdef KAIGEN_PRO_BUILD` (flag not defined in this plan).

**Tech Stack:** C++20, JUCE 8.0.4, Catch2 v3.5.2 (unit tests), WebView2 (UI), CMake 3.22+. Matches the existing project stack — no new dependencies.

**Spec:** `docs/superpowers/specs/2026-04-22-ab-compare-design.md`

---

## File Structure

### New files

| Path | Responsibility |
|------|----------------|
| `Source/ABSlotManager.h` | Class declaration. `Slot` enum, public API: `snapTo`, `copy`, `syncActiveSlotFromLive`, `toStateTree`/`fromStateTree`, `loadSinglePresetIntoActive`, `loadABPreset`, `buildPresetSlotBChild`, modified/preset-ref accessors, `getSlot`. |
| `Source/ABSlotManager.cpp` | Implementation, APVTS listener subclass for modified-flag tracking. |
| `tests/ABSlotManagerTests.cpp` | 15 unit tests covering snap/copy/designer-override/state round-trip/preset-load paths. |

### Modified files

| Path | Change |
|------|--------|
| `CMakeLists.txt` | Add `Source/ABSlotManager.cpp` to the `KaigenPhantom` target sources. |
| `tests/CMakeLists.txt` | Add `ABSlotManagerTests.cpp` and `../Source/ABSlotManager.cpp`. |
| `Source/PluginProcessor.h` | Add `ABSlotManager abSlots;` member + accessor; forward-declare header. |
| `Source/PluginProcessor.cpp` | Initialize `abSlots(apvts)`; update `getStateInformation` / `setStateInformation` to serialize the `<ABSlots>` child. |
| `Source/PluginEditor.h` | (No change — native functions are added only to PluginEditor.cpp's `withNativeFunction` chain.) |
| `Source/PluginEditor.cpp` | Add 4 native functions: `abGetState`, `abSnapTo`, `abCopy`, `abSetIncludeDiscrete`. |
| `Source/PresetManager.h` | Add `PresetKind` enum + `presetKind` field on `PresetMetadata`; update `savePreset` signature to accept `kind`; add `loadPresetInto(ABSlotManager&, ...)` method. |
| `Source/PresetManager.cpp` | Read `presetKind` and `<SlotB>` on scan; write them on save; route load to either `ABSlotManager::loadSinglePresetIntoActive` or `::loadABPreset`. |
| `Source/WebUI/index.html` | A/B cluster markup in the header between save button and mode toggle; Compare section in settings modal. |
| `Source/WebUI/styles.css` | `.ab-group`, `.ab-btn`, `.cp-btn`, `.kind-badge` styles. |
| `Source/WebUI/preset-system.js` | A/B wiring (native-function refs, snap/copy handlers, active-state rendering, inactive-slot modified dot), browser A/B column + sort + filter dropdown, save-modal Preset Kind radio + disable-when-identical helper, settings panel Compare toggle. |
| `MANUAL_TEST_CHECKLIST.md` | Add A/B compare section (8 checks per spec). |

---

## Phase 1 — `ABSlotManager` Core

### Task 1: Create `ABSlotManager.h` skeleton + register in both CMakeLists

**Files:**
- Create: `Source/ABSlotManager.h`
- Create: `Source/ABSlotManager.cpp` (stub)
- Modify: `CMakeLists.txt:42-52` (add source)
- Modify: `tests/CMakeLists.txt:10-28` (add test + source)

- [ ] **Step 1: Create the header**

Write `Source/ABSlotManager.h`:

```cpp
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_data_structures/juce_data_structures.h>

namespace kaigen::phantom
{

class ABSlotManager : private juce::AudioProcessorValueTreeState::Listener
{
public:
    enum class Slot { A = 0, B = 1 };

    explicit ABSlotManager(juce::AudioProcessorValueTreeState& apvts);
    ~ABSlotManager() override;

    // Snap/copy operations
    void snapTo(Slot target);
    void copy(Slot from, Slot to);
    Slot getActive() const noexcept { return active; }

    // Modified-indicator state
    bool isModified(Slot s) const noexcept;
    juce::String getPresetRef(Slot s) const;

    // Plugin state persistence (called from getStateInformation/setStateInformation)
    void syncActiveSlotFromLive();
    juce::ValueTree toStateTree() const;
    void fromStateTree(const juce::ValueTree& abSlotsTree);

    // Designer-authored preset application
    void loadSinglePresetIntoActive(const juce::ValueTree& presetState,
                                    const juce::String& presetRef);
    void loadABPreset(const juce::ValueTree& presetRootState,
                      const juce::String& presetRef);

    // Called by PresetManager::savePreset when building the tree to write
    juce::ValueTree buildPresetSlotBChild() const;

    // Setting accessors
    bool getIncludeDiscreteInSnap() const noexcept { return includeDiscreteInSnap; }
    void setIncludeDiscreteInSnap(bool on) noexcept { includeDiscreteInSnap = on; }

    // Read-only slot access (Pro MorphEngine seam)
    const juce::ValueTree& getSlot(Slot s) const noexcept { return slots[(int) s]; }

    // Returns the four discrete param IDs that are excluded from ad-hoc snaps
    // when includeDiscreteInSnap == false. Public for tests.
    static const juce::StringArray& discreteParamIDs();

private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    juce::AudioProcessorValueTreeState& apvts;
    juce::ValueTree slots[2];
    Slot active = Slot::A;
    bool designerAuthored = false;
    bool includeDiscreteInSnap = false;

    bool modified[2] { false, false };
    juce::String presetRef[2];

    // Guards the APVTS listener from firing during internal snap/copy/load.
    bool suppressModifiedUpdates = false;
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Create the implementation stub**

Write `Source/ABSlotManager.cpp`:

```cpp
#include "ABSlotManager.h"
#include "Parameters.h"

namespace kaigen::phantom
{

ABSlotManager::ABSlotManager(juce::AudioProcessorValueTreeState& apvtsRef)
    : apvts(apvtsRef)
{
    // Registration of APVTS listeners happens in a later task.
}

ABSlotManager::~ABSlotManager() = default;

void ABSlotManager::parameterChanged(const juce::String&, float) {}

void ABSlotManager::snapTo(Slot) {}
void ABSlotManager::copy(Slot, Slot) {}
bool ABSlotManager::isModified(Slot s) const noexcept { return modified[(int) s]; }
juce::String ABSlotManager::getPresetRef(Slot s) const { return presetRef[(int) s]; }

void ABSlotManager::syncActiveSlotFromLive() {}
juce::ValueTree ABSlotManager::toStateTree() const { return {}; }
void ABSlotManager::fromStateTree(const juce::ValueTree&) {}

void ABSlotManager::loadSinglePresetIntoActive(const juce::ValueTree&, const juce::String&) {}
void ABSlotManager::loadABPreset(const juce::ValueTree&, const juce::String&) {}
juce::ValueTree ABSlotManager::buildPresetSlotBChild() const { return {}; }

const juce::StringArray& ABSlotManager::discreteParamIDs()
{
    static const juce::StringArray ids { ParamID::MODE, ParamID::BYPASS,
                                         ParamID::GHOST_MODE, ParamID::BINAURAL_MODE };
    return ids;
}

} // namespace kaigen::phantom
```

- [ ] **Step 3: Register `ABSlotManager.cpp` in the plugin build**

Edit `CMakeLists.txt`, find the `target_sources(KaigenPhantom PRIVATE ...` block (around line 42) and add `Source/ABSlotManager.cpp` after `Source/PresetManager.cpp`:

```cmake
target_sources(KaigenPhantom PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Engines/BinauralStage.cpp
    Source/Engines/BassExtractor.cpp
    Source/Engines/ZeroCrossingSynth.cpp
    Source/Engines/WaveletSynth.cpp
    Source/Engines/EnvelopeFollower.cpp
    Source/Engines/PhantomEngine.cpp
    Source/PresetManager.cpp
    Source/ABSlotManager.cpp
)
```

- [ ] **Step 4: Register test file + source in the test build**

Edit `tests/CMakeLists.txt`, find the `add_executable(KaigenPhantomTests ...)` block (around line 10). Add `ABSlotManagerTests.cpp` after `PresetPreviewTests.cpp` and `../Source/ABSlotManager.cpp` after `../Source/PresetManager.cpp`:

```cmake
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
```

- [ ] **Step 5: Create the test file with a compile-check test**

Write `tests/ABSlotManagerTests.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "ABSlotManager.h"
#include "Parameters.h"

namespace
{
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
}

TEST_CASE("ABSlotManager compiles and constructs")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };
    CHECK(abSlots.getActive() == kaigen::phantom::ABSlotManager::Slot::A);
}
```

- [ ] **Step 6: Build and run tests to verify the skeleton compiles**

Run:

```bash
cmake --build build --target KaigenPhantomTests --config Debug
./build/tests/KaigenPhantomTests.exe "ABSlotManager compiles and constructs"
```

Expected: test passes.

- [ ] **Step 7: Commit**

```bash
git add Source/ABSlotManager.h Source/ABSlotManager.cpp tests/ABSlotManagerTests.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(ab-compare): ABSlotManager skeleton + test harness

Declares the full public API per the spec; all methods stub out for
now. Registers the new source in both the plugin and test CMakeLists.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Construction initializes slots from current APVTS

**Files:**
- Modify: `Source/ABSlotManager.cpp` (constructor)
- Modify: `tests/ABSlotManagerTests.cpp` (add test)

- [ ] **Step 1: Write the failing test**

Append to `tests/ABSlotManagerTests.cpp`:

```cpp
TEST_CASE("ABSlotManager constructor initializes both slots from live APVTS")
{
    TestProcessor proc;
    // Set a non-default value so we can check both slots capture it.
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.42f);

    kaigen::phantom::ABSlotManager abSlots { proc.apvts };

    auto slotA = abSlots.getSlot(kaigen::phantom::ABSlotManager::Slot::A);
    auto slotB = abSlots.getSlot(kaigen::phantom::ABSlotManager::Slot::B);

    REQUIRE(slotA.isValid());
    REQUIRE(slotB.isValid());
    // Both slots should match live state (copyState of same APVTS).
    CHECK(slotA.toXmlString() == slotB.toXmlString());
    CHECK(slotA.toXmlString() == proc.apvts.copyState().toXmlString());
}
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cmake --build build --target KaigenPhantomTests --config Debug
./build/tests/KaigenPhantomTests.exe "ABSlotManager constructor initializes both slots from live APVTS"
```

Expected: FAIL — slots are invalid (stub returns default-constructed ValueTrees).

- [ ] **Step 3: Implement the constructor**

Replace the stub constructor in `Source/ABSlotManager.cpp`:

```cpp
ABSlotManager::ABSlotManager(juce::AudioProcessorValueTreeState& apvtsRef)
    : apvts(apvtsRef)
{
    slots[0] = apvts.copyState();
    slots[1] = apvts.copyState();
}
```

- [ ] **Step 4: Run test to verify it passes**

```bash
cmake --build build --target KaigenPhantomTests --config Debug
./build/tests/KaigenPhantomTests.exe "ABSlotManager constructor initializes both slots from live APVTS"
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add Source/ABSlotManager.cpp tests/ABSlotManagerTests.cpp
git commit -m "feat(ab-compare): initialize both slots from live APVTS on construction"
```

---

### Task 3: `snapTo` — same-slot no-op + basic swap

**Files:**
- Modify: `Source/ABSlotManager.cpp`
- Modify: `tests/ABSlotManagerTests.cpp`

- [ ] **Step 1: Write the failing test — same-slot no-op**

Append to `tests/ABSlotManagerTests.cpp`:

```cpp
TEST_CASE("snapTo to already-active slot is a no-op")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };

    // Change live state after construction (slot A has original, slot B too)
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.9f);

    // Active is A. snapTo(A) should NOT commit live to A.
    const auto beforeA = abSlots.getSlot(kaigen::phantom::ABSlotManager::Slot::A).toXmlString();
    abSlots.snapTo(kaigen::phantom::ABSlotManager::Slot::A);
    const auto afterA  = abSlots.getSlot(kaigen::phantom::ABSlotManager::Slot::A).toXmlString();

    CHECK(beforeA == afterA);
    CHECK(abSlots.getActive() == kaigen::phantom::ABSlotManager::Slot::A);
}
```

- [ ] **Step 2: Write the failing test — snapTo A→B commits live to A, loads B**

Append to `tests/ABSlotManagerTests.cpp`:

```cpp
TEST_CASE("snapTo commits live to active slot, then loads target slot")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };

    // Edit live — should be captured into slot A on snapTo(B).
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.3f);

    abSlots.snapTo(kaigen::phantom::ABSlotManager::Slot::B);
    CHECK(abSlots.getActive() == kaigen::phantom::ABSlotManager::Slot::B);

    // Slot A now has the live edit baked in.
    auto slotATree = abSlots.getSlot(kaigen::phantom::ABSlotManager::Slot::A);
    auto* ghostParamA = slotATree.getChildWithProperty("id", juce::var(ParamID::GHOST))
                                 .getPropertyPointer("value");
    REQUIRE(ghostParamA != nullptr);
    CHECK(ghostParamA->operator float() == Catch::Approx(0.3f * 100.0f).epsilon(0.01));
    // (GHOST is a 0..100 % parameter; setValueNotifyingHost takes 0..1 normalized.)
}
```

- [ ] **Step 3: Run tests — verify they fail**

```bash
cmake --build build --target KaigenPhantomTests --config Debug
./build/tests/KaigenPhantomTests.exe "snapTo*"
```

Expected: FAIL — both tests fail since `snapTo` is a stub.

- [ ] **Step 4: Implement `snapTo` without discrete-param handling yet**

Replace the `snapTo` stub in `Source/ABSlotManager.cpp`:

```cpp
void ABSlotManager::snapTo(Slot target)
{
    if (target == active) return;

    // Commit live → currently-active slot.
    slots[(int) active] = apvts.copyState();

    // Replace live from target.
    {
        const juce::ScopedValueSetter<bool> guard { suppressModifiedUpdates, true };
        apvts.replaceState(slots[(int) target]);
    }

    active = target;
}
```

Note: `#include <juce_core/juce_core.h>` already provides `ScopedValueSetter`. No new include needed beyond what's in the header.

- [ ] **Step 5: Run tests to verify they pass**

```bash
cmake --build build --target KaigenPhantomTests --config Debug
./build/tests/KaigenPhantomTests.exe "snapTo*"
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add Source/ABSlotManager.cpp tests/ABSlotManagerTests.cpp
git commit -m "feat(ab-compare): snapTo commits live to active and loads target slot"
```

---

### Task 4: Discrete-parameter preservation (ad-hoc snaps)

**Files:**
- Modify: `Source/ABSlotManager.cpp` (`snapTo`)
- Modify: `tests/ABSlotManagerTests.cpp`

- [ ] **Step 1: Write the failing test — includeDiscrete=false preserves Ghost Mode**

Append to `tests/ABSlotManagerTests.cpp`:

```cpp
TEST_CASE("snapTo with includeDiscreteInSnap=false preserves discrete params")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };

    // Configure two slots with differing Ghost Mode via a sequence of snaps:
    // Slot A: Ghost Mode = 0 (Replace)
    proc.apvts.getParameter(ParamID::GHOST_MODE)->setValueNotifyingHost(0.0f);
    abSlots.snapTo(kaigen::phantom::ABSlotManager::Slot::B);
    // Slot B: Ghost Mode = 1 (Combine)   [mapping: 0/0.5/1.0 → 0/1/2]
    proc.apvts.getParameter(ParamID::GHOST_MODE)->setValueNotifyingHost(0.5f);
    abSlots.snapTo(kaigen::phantom::ABSlotManager::Slot::A);
    // Now live = slot A's value = 0 (Replace). Good.

    REQUIRE(abSlots.getIncludeDiscreteInSnap() == false);

    // Snap to B. With include-discrete OFF, the live Ghost Mode should
    // remain at slot A's value (0), NOT flip to slot B's (1).
    abSlots.snapTo(kaigen::phantom::ABSlotManager::Slot::B);

    const int liveGhostMode = (int) proc.apvts.getRawParameterValue(ParamID::GHOST_MODE)->load();
    CHECK(liveGhostMode == 0);
}
```

- [ ] **Step 2: Write the failing test — includeDiscrete=true flips it**

Append to `tests/ABSlotManagerTests.cpp`:

```cpp
TEST_CASE("snapTo with includeDiscreteInSnap=true flips discrete params")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };
    abSlots.setIncludeDiscreteInSnap(true);

    // Same setup as the previous test.
    proc.apvts.getParameter(ParamID::GHOST_MODE)->setValueNotifyingHost(0.0f);
    abSlots.snapTo(kaigen::phantom::ABSlotManager::Slot::B);
    proc.apvts.getParameter(ParamID::GHOST_MODE)->setValueNotifyingHost(0.5f);
    abSlots.snapTo(kaigen::phantom::ABSlotManager::Slot::A);

    abSlots.snapTo(kaigen::phantom::ABSlotManager::Slot::B);

    const int liveGhostMode = (int) proc.apvts.getRawParameterValue(ParamID::GHOST_MODE)->load();
    CHECK(liveGhostMode == 1);
}
```

- [ ] **Step 3: Run tests — verify they fail**

```bash
cmake --build build --target KaigenPhantomTests --config Debug
./build/tests/KaigenPhantomTests.exe "snapTo with includeDiscreteInSnap*"
```

Expected: FAIL — both fail because current `snapTo` ignores discrete-param setting.

- [ ] **Step 4: Implement discrete-param exclusion**

Replace `snapTo` in `Source/ABSlotManager.cpp`:

```cpp
void ABSlotManager::snapTo(Slot target)
{
    if (target == active) return;

    slots[(int) active] = apvts.copyState();

    // Capture discrete-param values BEFORE replaceState if we may need to restore them.
    const bool preserveDiscrete = !designerAuthored && !includeDiscreteInSnap;
    std::map<juce::String, float> savedDiscrete;
    if (preserveDiscrete)
    {
        for (const auto& id : discreteParamIDs())
        {
            if (auto* p = apvts.getParameter(id))
                savedDiscrete[id] = p->getValue();
        }
    }

    {
        const juce::ScopedValueSetter<bool> guard { suppressModifiedUpdates, true };
        apvts.replaceState(slots[(int) target]);
    }

    if (preserveDiscrete)
    {
        const juce::ScopedValueSetter<bool> guard { suppressModifiedUpdates, true };
        for (const auto& [id, value] : savedDiscrete)
        {
            if (auto* p = apvts.getParameter(id))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost(value);
                p->endChangeGesture();
            }
        }
    }

    active = target;
}
```

Add `#include <map>` at the top of the file if not already present (it isn't).

- [ ] **Step 5: Run tests to verify they pass**

```bash
cmake --build build --target KaigenPhantomTests --config Debug
./build/tests/KaigenPhantomTests.exe "snapTo with includeDiscreteInSnap*"
```

Expected: PASS. Also rerun the earlier snap tests to make sure they still pass:

```bash
./build/tests/KaigenPhantomTests.exe "snapTo*"
```

Expected: all pass.

- [ ] **Step 6: Commit**

```bash
git add Source/ABSlotManager.cpp tests/ABSlotManagerTests.cpp
git commit -m "feat(ab-compare): preserve discrete params on ad-hoc snap unless setting is on"
```

---

### Task 5: `copy(active → other)`

**Files:**
- Modify: `Source/ABSlotManager.cpp`
- Modify: `tests/ABSlotManagerTests.cpp`

- [ ] **Step 1: Write the failing test**

Append to `tests/ABSlotManagerTests.cpp`:

```cpp
TEST_CASE("copy A to B overwrites slot B with slot A contents")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };

    // Make slot A and slot B differ.
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.3f);
    abSlots.snapTo(kaigen::phantom::ABSlotManager::Slot::B);
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.7f);
    abSlots.snapTo(kaigen::phantom::ABSlotManager::Slot::A);
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.3f);  // live = slot A state
    // Now slot A's stored ghost value is ≠ slot B's. Active = A.

    abSlots.copy(kaigen::phantom::ABSlotManager::Slot::A,
                 kaigen::phantom::ABSlotManager::Slot::B);

    auto slotA = abSlots.getSlot(kaigen::phantom::ABSlotManager::Slot::A);
    auto slotB = abSlots.getSlot(kaigen::phantom::ABSlotManager::Slot::B);
    CHECK(slotA.toXmlString() == slotB.toXmlString());

    // Active did not change.
    CHECK(abSlots.getActive() == kaigen::phantom::ABSlotManager::Slot::A);
}
```

- [ ] **Step 2: Run test — verify it fails**

```bash
cmake --build build --target KaigenPhantomTests --config Debug
./build/tests/KaigenPhantomTests.exe "copy A to B*"
```

Expected: FAIL.

- [ ] **Step 3: Implement `copy`**

Replace `copy` in `Source/ABSlotManager.cpp`:

```cpp
void ABSlotManager::copy(Slot from, Slot to)
{
    if (from == to) return;

    // If source is active, commit in-flight edits first so `from` is current.
    if (from == active)
        slots[(int) active] = apvts.copyState();

    slots[(int) to] = slots[(int) from].createCopy();
    modified[(int) to] = false;
    presetRef[(int) to] = presetRef[(int) from];
}
```

- [ ] **Step 4: Run test to verify it passes**

```bash
cmake --build build --target KaigenPhantomTests --config Debug
./build/tests/KaigenPhantomTests.exe "copy A to B*"
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add Source/ABSlotManager.cpp tests/ABSlotManagerTests.cpp
git commit -m "feat(ab-compare): copy snapshots source slot into destination"
```

---

### Task 6: Modified-flag tracking via APVTS listener

**Files:**
- Modify: `Source/ABSlotManager.h` (no — already has the listener inheritance)
- Modify: `Source/ABSlotManager.cpp` (constructor/destructor, `parameterChanged`)
- Modify: `tests/ABSlotManagerTests.cpp`

- [ ] **Step 1: Write the failing test**

Append to `tests/ABSlotManagerTests.cpp`:

```cpp
TEST_CASE("modified flag set on APVTS change for active slot only")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };

    REQUIRE(abSlots.isModified(kaigen::phantom::ABSlotManager::Slot::A) == false);
    REQUIRE(abSlots.isModified(kaigen::phantom::ABSlotManager::Slot::B) == false);

    // Change a param while on slot A → modified[A] should flip to true.
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.4f);

    CHECK(abSlots.isModified(kaigen::phantom::ABSlotManager::Slot::A) == true);
    CHECK(abSlots.isModified(kaigen::phantom::ABSlotManager::Slot::B) == false);
}

TEST_CASE("modified flag NOT set during internal snap/copy/load")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };

    // First make slots differ so snapTo actually does something.
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.4f);
    abSlots.snapTo(kaigen::phantom::ABSlotManager::Slot::B);
    // snapTo fires replaceState which, in the absence of suppression, would
    // flip modified flags. Verify it did not.
    CHECK(abSlots.isModified(kaigen::phantom::ABSlotManager::Slot::B) == false);
}
```

- [ ] **Step 2: Run tests — verify they fail**

```bash
cmake --build build --target KaigenPhantomTests --config Debug
./build/tests/KaigenPhantomTests.exe "modified flag*"
```

Expected: FAIL — the listener isn't registered and `parameterChanged` is a no-op.

- [ ] **Step 3: Register listener in constructor; deregister in destructor**

Update constructor in `Source/ABSlotManager.cpp`:

```cpp
ABSlotManager::ABSlotManager(juce::AudioProcessorValueTreeState& apvtsRef)
    : apvts(apvtsRef)
{
    slots[0] = apvts.copyState();
    slots[1] = apvts.copyState();

    // Subscribe to every parameter so we can flip the active slot's
    // modified flag on any user-initiated change.
    for (const auto& id : getAllParameterIDs())
        apvts.addParameterListener(id, this);
}

ABSlotManager::~ABSlotManager()
{
    for (const auto& id : getAllParameterIDs())
        apvts.removeParameterListener(id, this);
}
```

- [ ] **Step 4: Implement `parameterChanged`**

Replace the stub in `Source/ABSlotManager.cpp`:

```cpp
void ABSlotManager::parameterChanged(const juce::String& /*parameterID*/, float /*newValue*/)
{
    if (suppressModifiedUpdates) return;
    modified[(int) active] = true;
    // Any user edit leaves designer-authored territory.
    designerAuthored = false;
}
```

- [ ] **Step 5: Run tests to verify they pass**

```bash
cmake --build build --target KaigenPhantomTests --config Debug
./build/tests/KaigenPhantomTests.exe "modified flag*"
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add Source/ABSlotManager.cpp tests/ABSlotManagerTests.cpp
git commit -m "feat(ab-compare): track per-slot modified flag via APVTS listener"
```

---

## Phase 2 — State Persistence and Preset Integration

### Task 7: `syncActiveSlotFromLive` + `toStateTree` / `fromStateTree` round-trip

**Files:**
- Modify: `Source/ABSlotManager.cpp`
- Modify: `tests/ABSlotManagerTests.cpp`

- [ ] **Step 1: Write the failing round-trip test**

Append to `tests/ABSlotManagerTests.cpp`:

```cpp
TEST_CASE("toStateTree / fromStateTree round-trips slots, active, and setting")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager src { proc.apvts };

    // Populate slots with distinct data.
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.25f);
    src.snapTo(kaigen::phantom::ABSlotManager::Slot::B);
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.75f);
    src.syncActiveSlotFromLive();   // make sure slot B is current
    src.setIncludeDiscreteInSnap(true);

    const auto tree = src.toStateTree();

    // New manager in a separate processor, restore.
    TestProcessor proc2;
    kaigen::phantom::ABSlotManager dst { proc2.apvts };
    dst.fromStateTree(tree);

    CHECK(dst.getActive() == kaigen::phantom::ABSlotManager::Slot::B);
    CHECK(dst.getIncludeDiscreteInSnap() == true);
    CHECK(dst.getSlot(kaigen::phantom::ABSlotManager::Slot::A).toXmlString()
          == src.getSlot(kaigen::phantom::ABSlotManager::Slot::A).toXmlString());
    CHECK(dst.getSlot(kaigen::phantom::ABSlotManager::Slot::B).toXmlString()
          == src.getSlot(kaigen::phantom::ABSlotManager::Slot::B).toXmlString());
}

TEST_CASE("fromStateTree with invalid/empty tree initializes slots from live APVTS")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };

    abSlots.fromStateTree(juce::ValueTree{});

    CHECK(abSlots.getActive() == kaigen::phantom::ABSlotManager::Slot::A);
    CHECK(abSlots.getIncludeDiscreteInSnap() == false);
    CHECK(abSlots.getSlot(kaigen::phantom::ABSlotManager::Slot::A).toXmlString()
          == proc.apvts.copyState().toXmlString());
    CHECK(abSlots.getSlot(kaigen::phantom::ABSlotManager::Slot::B).toXmlString()
          == proc.apvts.copyState().toXmlString());
}
```

- [ ] **Step 2: Run tests — verify they fail**

```bash
cmake --build build --target KaigenPhantomTests --config Debug
./build/tests/KaigenPhantomTests.exe "toStateTree*" "fromStateTree*"
```

Expected: FAIL.

- [ ] **Step 3: Implement `syncActiveSlotFromLive`, `toStateTree`, `fromStateTree`**

Add near the top of `Source/ABSlotManager.cpp`, inside the anonymous namespace (or after `using` declarations):

```cpp
namespace
{
    constexpr const char* kABSlotsNodeId    = "ABSlots";
    constexpr const char* kSlotNodeId       = "Slot";
    constexpr const char* kActiveAttr       = "active";
    constexpr const char* kIncludeDiscAttr  = "includeDiscrete";
    constexpr const char* kSlotNameAttr     = "name";
}
```

Replace the three method stubs with:

```cpp
void ABSlotManager::syncActiveSlotFromLive()
{
    slots[(int) active] = apvts.copyState();
}

juce::ValueTree ABSlotManager::toStateTree() const
{
    juce::ValueTree tree { kABSlotsNodeId };
    tree.setProperty(kActiveAttr, active == Slot::A ? "A" : "B", nullptr);
    tree.setProperty(kIncludeDiscAttr, includeDiscreteInSnap ? 1 : 0, nullptr);

    for (int i = 0; i < 2; ++i)
    {
        juce::ValueTree slotNode { kSlotNodeId };
        slotNode.setProperty(kSlotNameAttr, i == 0 ? "A" : "B", nullptr);
        if (slots[i].isValid())
            slotNode.appendChild(slots[i].createCopy(), nullptr);
        tree.appendChild(slotNode, nullptr);
    }
    return tree;
}

void ABSlotManager::fromStateTree(const juce::ValueTree& abSlotsTree)
{
    if (!abSlotsTree.isValid() || abSlotsTree.getType().toString() != kABSlotsNodeId)
    {
        slots[0] = apvts.copyState();
        slots[1] = apvts.copyState();
        active = Slot::A;
        includeDiscreteInSnap = false;
        designerAuthored = false;
        modified[0] = modified[1] = false;
        presetRef[0] = presetRef[1] = {};
        return;
    }

    const auto activeStr = abSlotsTree.getProperty(kActiveAttr, juce::var("A")).toString();
    active = (activeStr == "B") ? Slot::B : Slot::A;
    includeDiscreteInSnap = ((int) abSlotsTree.getProperty(kIncludeDiscAttr, 0)) != 0;

    for (int i = 0; i < abSlotsTree.getNumChildren(); ++i)
    {
        auto slotNode = abSlotsTree.getChild(i);
        if (slotNode.getType().toString() != kSlotNodeId) continue;
        const auto nameStr = slotNode.getProperty(kSlotNameAttr).toString();
        const int idx = (nameStr == "B") ? 1 : 0;
        if (slotNode.getNumChildren() > 0)
            slots[idx] = slotNode.getChild(0).createCopy();
    }

    // Default/empty-slot fallback: fill with live state so they're always valid.
    if (!slots[0].isValid()) slots[0] = apvts.copyState();
    if (!slots[1].isValid()) slots[1] = apvts.copyState();

    designerAuthored = false;
    modified[0] = modified[1] = false;
    presetRef[0] = presetRef[1] = {};
}
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
cmake --build build --target KaigenPhantomTests --config Debug
./build/tests/KaigenPhantomTests.exe "toStateTree*" "fromStateTree*"
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add Source/ABSlotManager.cpp tests/ABSlotManagerTests.cpp
git commit -m "feat(ab-compare): serialize slots + active + setting to ValueTree"
```

---

### Task 8: `loadSinglePresetIntoActive` — preset into active slot only

**Files:**
- Modify: `Source/ABSlotManager.cpp`
- Modify: `tests/ABSlotManagerTests.cpp`

- [ ] **Step 1: Write the failing test**

Append to `tests/ABSlotManagerTests.cpp`:

```cpp
TEST_CASE("loadSinglePresetIntoActive writes only the active slot")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };

    // Snapshot slot B's initial content so we can verify it's untouched.
    const auto slotBBefore = abSlots.getSlot(kaigen::phantom::ABSlotManager::Slot::B).toXmlString();

    // Build a preset tree that differs from live state.
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.9f);
    auto presetTree = proc.apvts.copyState();
    // Reset live to default so we can see the load took effect.
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.0f);

    abSlots.loadSinglePresetIntoActive(presetTree, "Factory/Warm Bass");

    // Slot A should be the preset.
    CHECK(abSlots.getSlot(kaigen::phantom::ABSlotManager::Slot::A).toXmlString()
          == presetTree.toXmlString());
    // Slot B untouched.
    CHECK(abSlots.getSlot(kaigen::phantom::ABSlotManager::Slot::B).toXmlString()
          == slotBBefore);
    // Live state is now the preset.
    const float liveGhost = proc.apvts.getRawParameterValue(ParamID::GHOST)->load();
    CHECK(liveGhost == Catch::Approx(90.0f).epsilon(0.01));  // 0.9 normalized × 100 range
    // Preset ref set for slot A only, modified cleared.
    CHECK(abSlots.getPresetRef(kaigen::phantom::ABSlotManager::Slot::A) == "Factory/Warm Bass");
    CHECK(abSlots.isModified(kaigen::phantom::ABSlotManager::Slot::A) == false);
}
```

- [ ] **Step 2: Run test — verify it fails**

```bash
cmake --build build --target KaigenPhantomTests --config Debug
./build/tests/KaigenPhantomTests.exe "loadSinglePresetIntoActive*"
```

Expected: FAIL.

- [ ] **Step 3: Implement `loadSinglePresetIntoActive`**

Replace the stub in `Source/ABSlotManager.cpp`:

```cpp
void ABSlotManager::loadSinglePresetIntoActive(const juce::ValueTree& presetState,
                                               const juce::String& ref)
{
    if (!presetState.isValid() || presetState.getType() != apvts.state.getType())
        return;

    slots[(int) active] = presetState.createCopy();

    {
        const juce::ScopedValueSetter<bool> guard { suppressModifiedUpdates, true };
        apvts.replaceState(slots[(int) active]);
    }

    modified[(int) active] = false;
    presetRef[(int) active] = ref;
    designerAuthored = false;
}
```

- [ ] **Step 4: Run test to verify it passes**

```bash
cmake --build build --target KaigenPhantomTests --config Debug
./build/tests/KaigenPhantomTests.exe "loadSinglePresetIntoActive*"
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add Source/ABSlotManager.cpp tests/ABSlotManagerTests.cpp
git commit -m "feat(ab-compare): loadSinglePresetIntoActive writes only active slot"
```

---

### Task 9: `loadABPreset` + designer-authored flag + `buildPresetSlotBChild`

**Files:**
- Modify: `Source/ABSlotManager.cpp`
- Modify: `tests/ABSlotManagerTests.cpp`

- [ ] **Step 1: Write the failing tests**

Append to `tests/ABSlotManagerTests.cpp`:

```cpp
TEST_CASE("loadABPreset populates both slots and sets designerAuthored")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };

    // Build two distinct state trees for A and B.
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.2f);
    const auto stateA = proc.apvts.copyState();
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.8f);
    const auto stateB = proc.apvts.copyState();

    // Compose a preset root: root = stateA, with SlotB child containing stateB.
    auto presetRoot = stateA.createCopy();
    juce::ValueTree slotBChild { "SlotB" };
    slotBChild.appendChild(stateB.createCopy(), nullptr);
    presetRoot.appendChild(slotBChild, nullptr);

    abSlots.loadABPreset(presetRoot, "Factory/Bright vs Dark");

    // Slot A gets the root state, MINUS the SlotB child (stripping covered later).
    // For now, just check slot B equals the SlotB child's payload.
    CHECK(abSlots.getSlot(kaigen::phantom::ABSlotManager::Slot::B).toXmlString()
          == stateB.toXmlString());
    CHECK(abSlots.getPresetRef(kaigen::phantom::ABSlotManager::Slot::A) == "Factory/Bright vs Dark");
    CHECK(abSlots.getPresetRef(kaigen::phantom::ABSlotManager::Slot::B) == "Factory/Bright vs Dark");
    CHECK(abSlots.isModified(kaigen::phantom::ABSlotManager::Slot::A) == false);
    CHECK(abSlots.isModified(kaigen::phantom::ABSlotManager::Slot::B) == false);
}

TEST_CASE("designerAuthored overrides includeDiscreteInSnap=false")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };
    abSlots.setIncludeDiscreteInSnap(false);

    // Slot A: Ghost Mode = 0 (Replace); Slot B: Ghost Mode = 2 (Phantom Only).
    proc.apvts.getParameter(ParamID::GHOST_MODE)->setValueNotifyingHost(0.0f);
    const auto stateA = proc.apvts.copyState();
    proc.apvts.getParameter(ParamID::GHOST_MODE)->setValueNotifyingHost(1.0f);
    const auto stateB = proc.apvts.copyState();

    auto presetRoot = stateA.createCopy();
    juce::ValueTree slotBChild { "SlotB" };
    slotBChild.appendChild(stateB.createCopy(), nullptr);
    presetRoot.appendChild(slotBChild, nullptr);

    abSlots.loadABPreset(presetRoot, "Factory/Test");

    // Active is still A (load doesn't change it). Snap to B: discrete should flip
    // because designer-authored bit overrides the off-by-default user setting.
    abSlots.snapTo(kaigen::phantom::ABSlotManager::Slot::B);
    const int liveGhostMode = (int) proc.apvts.getRawParameterValue(ParamID::GHOST_MODE)->load();
    CHECK(liveGhostMode == 2);
}

TEST_CASE("designerAuthored cleared on any parameter edit")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };
    abSlots.setIncludeDiscreteInSnap(false);

    proc.apvts.getParameter(ParamID::GHOST_MODE)->setValueNotifyingHost(0.0f);
    const auto stateA = proc.apvts.copyState();
    proc.apvts.getParameter(ParamID::GHOST_MODE)->setValueNotifyingHost(1.0f);
    const auto stateB = proc.apvts.copyState();

    auto presetRoot = stateA.createCopy();
    juce::ValueTree slotBChild { "SlotB" };
    slotBChild.appendChild(stateB.createCopy(), nullptr);
    presetRoot.appendChild(slotBChild, nullptr);

    abSlots.loadABPreset(presetRoot, "Factory/Test");

    // User tweaks any param → designerAuthored cleared.
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.5f);

    // Now snap to B → discrete stays at slot A's value because the setting is off.
    abSlots.snapTo(kaigen::phantom::ABSlotManager::Slot::B);
    const int liveGhostMode = (int) proc.apvts.getRawParameterValue(ParamID::GHOST_MODE)->load();
    CHECK(liveGhostMode == 0);
}

TEST_CASE("buildPresetSlotBChild wraps slot B in a <SlotB> node")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };

    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.33f);
    abSlots.snapTo(kaigen::phantom::ABSlotManager::Slot::B);
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.77f);
    abSlots.syncActiveSlotFromLive();

    const auto child = abSlots.buildPresetSlotBChild();
    REQUIRE(child.isValid());
    CHECK(child.getType().toString() == "SlotB");
    REQUIRE(child.getNumChildren() == 1);
    const auto slotBState = child.getChild(0);
    CHECK(slotBState.toXmlString()
          == abSlots.getSlot(kaigen::phantom::ABSlotManager::Slot::B).toXmlString());
}
```

- [ ] **Step 2: Run tests — verify they fail**

```bash
cmake --build build --target KaigenPhantomTests --config Debug
./build/tests/KaigenPhantomTests.exe "loadABPreset*" "designerAuthored*" "buildPresetSlotBChild*"
```

Expected: FAIL.

- [ ] **Step 3: Implement `loadABPreset` and `buildPresetSlotBChild`**

Replace the two stubs in `Source/ABSlotManager.cpp`:

```cpp
void ABSlotManager::loadABPreset(const juce::ValueTree& presetRootState,
                                 const juce::String& ref)
{
    if (!presetRootState.isValid() || presetRootState.getType() != apvts.state.getType())
        return;

    // Slot A = root state, WITHOUT <SlotB> or <MorphConfig> children.
    auto slotA = presetRootState.createCopy();
    if (auto existing = slotA.getChildWithName("SlotB"); existing.isValid())
        slotA.removeChild(existing, nullptr);
    if (auto existingMorph = slotA.getChildWithName("MorphConfig"); existingMorph.isValid())
        slotA.removeChild(existingMorph, nullptr);
    slots[0] = slotA;

    // Slot B = contents of <SlotB>'s first child, if present. Otherwise leave empty
    // (treated as "A/B preset with empty B" — unusual but not an error).
    const auto slotBChild = presetRootState.getChildWithName("SlotB");
    if (slotBChild.isValid() && slotBChild.getNumChildren() > 0)
        slots[1] = slotBChild.getChild(0).createCopy();
    else
        slots[1] = slots[0].createCopy();

    {
        const juce::ScopedValueSetter<bool> guard { suppressModifiedUpdates, true };
        apvts.replaceState(slots[(int) active]);
    }

    modified[0] = modified[1] = false;
    presetRef[0] = presetRef[1] = ref;
    designerAuthored = true;
}

juce::ValueTree ABSlotManager::buildPresetSlotBChild() const
{
    juce::ValueTree child { "SlotB" };
    if (slots[1].isValid())
        child.appendChild(slots[1].createCopy(), nullptr);
    return child;
}
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
cmake --build build --target KaigenPhantomTests --config Debug
./build/tests/KaigenPhantomTests.exe "loadABPreset*" "designerAuthored*" "buildPresetSlotBChild*"
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add Source/ABSlotManager.cpp tests/ABSlotManagerTests.cpp
git commit -m "feat(ab-compare): loadABPreset + designer-authored lifecycle + SlotB builder"
```

---

## Phase 3 — `PhantomProcessor` integration

### Task 10: Wire `ABSlotManager` into `PhantomProcessor`

**Files:**
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`

- [ ] **Step 1: Add member + accessor to `PluginProcessor.h`**

Edit `Source/PluginProcessor.h`. After the existing includes, add:

```cpp
#include "ABSlotManager.h"
```

Inside the class, after the `kaigen::phantom::PresetManager presetManager;` member (around line 50), add:

```cpp
    kaigen::phantom::ABSlotManager abSlots { apvts };

    kaigen::phantom::ABSlotManager& getABSlotManager() { return abSlots; }
```

- [ ] **Step 2: Update `getStateInformation` to append `<ABSlots>` child**

Edit `Source/PluginProcessor.cpp:391`. Replace the existing function body:

```cpp
void PhantomProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Ensure any in-flight edits on the active slot are captured before serializing.
    abSlots.syncActiveSlotFromLive();

    auto state = apvts.copyState();

    // Drop any prior <ABSlots> child (shouldn't exist on state from copyState,
    // but guard against re-entrant save paths writing twice).
    if (auto existing = state.getChildWithName("ABSlots"); existing.isValid())
        state.removeChild(existing, nullptr);
    state.appendChild(abSlots.toStateTree(), nullptr);

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}
```

- [ ] **Step 3: Update `setStateInformation` to restore slots**

Edit `Source/PluginProcessor.cpp:398`. Replace the function body:

```cpp
void PhantomProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml == nullptr || !xml->hasTagName(apvts.state.getType())) return;

    auto tree = juce::ValueTree::fromXml(*xml);

    // Peel off the <ABSlots> child (if any) before the parameter replace; APVTS
    // doesn't know about it and would keep it in the live tree where it would
    // shadow future apvts.copyState() calls.
    auto abSlotsTree = tree.getChildWithName("ABSlots");
    if (abSlotsTree.isValid())
        tree.removeChild(abSlotsTree, nullptr);

    apvts.replaceState(tree);
    abSlots.fromStateTree(abSlotsTree);
}
```

- [ ] **Step 4: Build the plugin to verify it compiles**

```bash
cmake --build build --target KaigenPhantom --config Debug
```

Expected: build succeeds. (There is no automated end-to-end test for this integration since the real processor needs the full JUCE plugin host; the next task covers an integration test through the test harness.)

- [ ] **Step 5: Commit**

```bash
git add Source/PluginProcessor.h Source/PluginProcessor.cpp
git commit -m "feat(ab-compare): persist ABSlots under plugin getStateInformation"
```

---

### Task 11: Integration test — plugin state round-trip

**Files:**
- Modify: `tests/ABSlotManagerTests.cpp`

- [ ] **Step 1: Write the integration test**

Append to `tests/ABSlotManagerTests.cpp`. This test simulates the processor-level flow (serialize via `toStateTree`, embed, extract, restore via `fromStateTree`) so it catches mismatches between `PhantomProcessor::get/setStateInformation` and `ABSlotManager`.

```cpp
TEST_CASE("plugin-state round-trip preserves slots + active + setting")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager src { proc.apvts };

    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.15f);
    src.snapTo(kaigen::phantom::ABSlotManager::Slot::B);
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.85f);
    src.syncActiveSlotFromLive();
    src.setIncludeDiscreteInSnap(true);

    // Build what get/setStateInformation would write: root state + <ABSlots> child.
    auto rootForSave = proc.apvts.copyState();
    rootForSave.appendChild(src.toStateTree(), nullptr);
    const auto xml = rootForSave.toXmlString();

    // Now parse that back (simulating setStateInformation).
    auto parsed = juce::ValueTree::fromXml(xml);
    auto abSlotsChild = parsed.getChildWithName("ABSlots");
    REQUIRE(abSlotsChild.isValid());
    parsed.removeChild(abSlotsChild, nullptr);

    TestProcessor proc2;
    proc2.apvts.replaceState(parsed);
    kaigen::phantom::ABSlotManager dst { proc2.apvts };
    dst.fromStateTree(abSlotsChild);

    CHECK(dst.getActive() == kaigen::phantom::ABSlotManager::Slot::B);
    CHECK(dst.getIncludeDiscreteInSnap() == true);
    CHECK(dst.getSlot(kaigen::phantom::ABSlotManager::Slot::A).toXmlString()
          == src.getSlot(kaigen::phantom::ABSlotManager::Slot::A).toXmlString());
    CHECK(dst.getSlot(kaigen::phantom::ABSlotManager::Slot::B).toXmlString()
          == src.getSlot(kaigen::phantom::ABSlotManager::Slot::B).toXmlString());
}
```

- [ ] **Step 2: Run test**

```bash
cmake --build build --target KaigenPhantomTests --config Debug
./build/tests/KaigenPhantomTests.exe "plugin-state round-trip*"
```

Expected: PASS.

- [ ] **Step 3: Commit**

```bash
git add tests/ABSlotManagerTests.cpp
git commit -m "test(ab-compare): integration test for plugin-state round-trip"
```

---

## Phase 4 — `PresetManager` integration

### Task 12: Add `PresetKind` enum + `presetKind` field

**Files:**
- Modify: `Source/PresetManager.h`
- Modify: `Source/PresetManager.cpp`

- [ ] **Step 1: Add enum + field to the header**

Edit `Source/PresetManager.h`. After the namespace opening (around line 10), add:

```cpp
enum class PresetKind
{
    Single,     // no <SlotB>; one state snapshot
    AB,         // <SlotB> present; no <MorphConfig>
    ABMorph     // <SlotB> + <MorphConfig> present (loaded as AB in Standard build)
};

juce::String presetKindToString(PresetKind);
PresetKind   presetKindFromString(const juce::String&);
```

In the `PresetMetadata` struct (around line 12), add a `presetKind` field:

```cpp
struct PresetMetadata
{
    juce::String name;
    juce::String type;
    juce::String designer;
    juce::String description;
    juce::String packName;
    bool isFactory = false;
    bool isFavorite = false;
    PresetKind   presetKind = PresetKind::Single;   // NEW
};
```

- [ ] **Step 2: Implement the string helpers and update metadata read path**

Edit `Source/PresetManager.cpp`. Near the top, add the helpers after the anonymous namespace:

```cpp
juce::String presetKindToString(PresetKind k)
{
    switch (k)
    {
        case PresetKind::Single:  return "single";
        case PresetKind::AB:      return "ab";
        case PresetKind::ABMorph: return "ab_morph";
    }
    return "single";
}

PresetKind presetKindFromString(const juce::String& s)
{
    if (s == "ab")       return PresetKind::AB;
    if (s == "ab_morph") return PresetKind::ABMorph;
    return PresetKind::Single;
}
```

Find `PresetManager::readMetadataFromFile` (search for `readMetadataFromFile`). Inside the body, after reading the existing `name/type/designer/description` properties, add:

```cpp
    const auto kindStr = meta.getProperty("presetKind", juce::var("single")).toString();
    metadata.presetKind = presetKindFromString(kindStr);
```

If there's no existing `presetKind` prop (old presets), the default `"single"` yields `PresetKind::Single`.

- [ ] **Step 3: Compile check**

```bash
cmake --build build --target KaigenPhantomTests --config Debug
```

Expected: builds clean. No new tests required yet — covered in later tasks through integration.

- [ ] **Step 4: Commit**

```bash
git add Source/PresetManager.h Source/PresetManager.cpp
git commit -m "feat(ab-compare): add PresetKind enum + presetKind metadata field"
```

---

### Task 13: Update `savePreset` signature + write `<SlotB>` for AB saves

**Files:**
- Modify: `Source/PresetManager.h`
- Modify: `Source/PresetManager.cpp`

- [ ] **Step 1: Update the `savePreset` signature in the header**

Edit `Source/PresetManager.h`. Forward-declare `ABSlotManager` at the top of the namespace:

```cpp
class ABSlotManager;   // fwd decl
```

Replace the existing `savePreset` declaration with:

```cpp
    // Save APVTS state as a new preset in User/. If overwrite=false and a
    // preset with this name exists, a numeric suffix is appended.
    // Returns the saved preset's name (possibly disambiguated), or empty on failure.
    //
    // `kind` controls whether a <SlotB> child is attached:
    //   Single → no slot B (ignores abSlots)
    //   AB     → emits <SlotB> from abSlots.buildPresetSlotBChild()
    //   ABMorph→ same as AB in Standard builds; emits <MorphConfig> only under
    //            KAIGEN_PRO_BUILD (not defined in this spec).
    //
    // When kind != Single and the caller's slot B equals slot A (or is empty),
    // the method returns empty to signal "rejected". The UI is expected to
    // prevent this case; the check here is a safety net.
    juce::String savePreset(juce::AudioProcessorValueTreeState& apvts,
                            ABSlotManager* abSlots,        // may be nullptr for Single
                            const juce::String& presetName,
                            const juce::String& type,
                            const juce::String& designer,
                            const juce::String& description,
                            PresetKind kind,
                            bool overwrite);
```

- [ ] **Step 2: Update the implementation**

Edit `Source/PresetManager.cpp`. Add `#include "ABSlotManager.h"` near the top includes. Replace the existing `savePreset` implementation (around line 323) with this version — preserving the existing sanitize / disambiguate logic but adding `kind`-specific tree-building:

```cpp
juce::String PresetManager::savePreset(juce::AudioProcessorValueTreeState& apvts,
                                       ABSlotManager* abSlots,
                                       const juce::String& presetName,
                                       const juce::String& type,
                                       const juce::String& designer,
                                       const juce::String& description,
                                       PresetKind kind,
                                       bool overwrite)
{
    auto sanitized = sanitizeName(presetName);
    if (sanitized.isEmpty()) return {};

    const auto validType = kValidTypes.contains(type) ? type : juce::String("Experimental");
    const auto effectiveDesigner = designer.isEmpty() ? juce::String("User") : designer;

    // Reject AB / AB+Morph saves when slots are identical (safety net — UI
    // should disable the radio in that state).
    if ((kind == PresetKind::AB || kind == PresetKind::ABMorph) && abSlots != nullptr)
    {
        const auto slotA = abSlots->getSlot(ABSlotManager::Slot::A);
        const auto slotB = abSlots->getSlot(ABSlotManager::Slot::B);
        if (slotA.toXmlString() == slotB.toXmlString())
            return {};
    }

    auto userDir = getUserPresetsDirectory();
    auto target = userDir.getChildFile(sanitized + ".fxp");

    if (target.existsAsFile() && !overwrite)
    {
        int suffix = 2;
        while (true)
        {
            auto candidate = userDir.getChildFile(sanitized + " " + juce::String(suffix) + ".fxp");
            if (!candidate.existsAsFile())
            {
                target = candidate;
                sanitized = sanitized + " " + juce::String(suffix);
                break;
            }
            if (++suffix > 999) return {};
        }
    }

    // Build the root state: for Single, use live APVTS (current behavior).
    // For AB / AB+Morph, the root is user's SLOT A (regardless of active slot).
    juce::ValueTree state;
    if (kind == PresetKind::Single || abSlots == nullptr)
    {
        state = apvts.copyState();
    }
    else
    {
        state = abSlots->getSlot(ABSlotManager::Slot::A).createCopy();
    }

    // Remove any pre-existing children that we're about to re-emit.
    if (auto existingMeta = state.getChildWithName(kMetadataNodeId); existingMeta.isValid())
        state.removeChild(existingMeta, nullptr);
    if (auto existingSlotB = state.getChildWithName("SlotB"); existingSlotB.isValid())
        state.removeChild(existingSlotB, nullptr);
    if (auto existingMorph = state.getChildWithName("MorphConfig"); existingMorph.isValid())
        state.removeChild(existingMorph, nullptr);

    // Metadata child, with presetKind prop.
    auto metadataTree = buildMetadataTree(sanitized, validType, effectiveDesigner, description);
    metadataTree.setProperty("presetKind", presetKindToString(kind), nullptr);
    state.appendChild(metadataTree, nullptr);

    // Slot B for AB / AB+Morph saves.
    if ((kind == PresetKind::AB || kind == PresetKind::ABMorph) && abSlots != nullptr)
    {
        state.appendChild(abSlots->buildPresetSlotBChild(), nullptr);
    }

    // <MorphConfig> is Pro-build only — not emitted in this plan.

    auto xml = state.createXml();
    if (xml == nullptr) return {};

    if (!target.replaceWithText(xml->toString()))
        return {};

    // Update in-memory cache (same as before, plus presetKind).
    PresetInfo info;
    info.file = target;
    info.metadata.name = sanitized;
    info.metadata.type = validType;
    info.metadata.designer = effectiveDesigner;
    info.metadata.description = description;
    info.metadata.packName = kUserPackName;
    info.metadata.isFactory = false;
    info.metadata.isFavorite = isFavorite(sanitized, kUserPackName);
    info.metadata.presetKind = kind;
    info.preview = readPreviewFromState(state);

    auto& userList = allPresets[kUserPackName];
    auto it = std::find_if(userList.begin(), userList.end(),
        [&](const PresetInfo& p) { return p.metadata.name == sanitized; });
    if (it != userList.end())
        *it = info;
    else
        userList.push_back(info);

    std::sort(userList.begin(), userList.end(),
        [](const PresetInfo& a, const PresetInfo& b)
        { return a.metadata.name.compareIgnoreCase(b.metadata.name) < 0; });

    return sanitized;
}
```

- [ ] **Step 3: Update the caller in `PluginEditor.cpp`**

Edit `Source/PluginEditor.cpp`. Find the `.withNativeFunction("savePreset", ...)` block (around line 420). Replace the call inside the lambda:

```cpp
                const auto name        = args[0].toString();
                const auto type        = args.size() > 1 ? args[1].toString() : juce::String("Experimental");
                const auto designer    = args.size() > 2 ? args[2].toString() : juce::String("User");
                const auto description = args.size() > 3 ? args[3].toString() : juce::String();
                const bool overwrite   = args.size() > 4 && args[4].isBool() && (bool) args[4];
                const auto kindStr     = args.size() > 5 ? args[5].toString() : juce::String("single");

                const auto kind = kaigen::phantom::presetKindFromString(kindStr);

                auto savedName = self.processor.getPresetManager().savePreset(
                    self.processor.apvts,
                    &self.processor.getABSlotManager(),
                    name, type, designer, description, kind, overwrite);
                complete(juce::var(savedName));
```

- [ ] **Step 4: Build to verify everything compiles**

```bash
cmake --build build --target KaigenPhantom --config Debug
cmake --build build --target KaigenPhantomTests --config Debug
```

Expected: both build clean.

- [ ] **Step 5: Commit**

```bash
git add Source/PresetManager.h Source/PresetManager.cpp Source/PluginEditor.cpp
git commit -m "feat(ab-compare): savePreset emits SlotB and presetKind metadata"
```

---

### Task 14: Update `loadPreset` to route by kind

**Files:**
- Modify: `Source/PresetManager.h`
- Modify: `Source/PresetManager.cpp`
- Modify: `Source/PluginEditor.cpp`

- [ ] **Step 1: Add a new `loadPresetInto` method in the header**

Edit `Source/PresetManager.h`. Add this declaration **next to** the existing `loadPreset` (don't delete the old one — it's still used by PluginProcessor state init and any callers that don't have an ABSlotManager):

```cpp
    // Load a preset via ABSlotManager. Dispatches on the preset's kind:
    //   Single → abSlots.loadSinglePresetIntoActive
    //   AB / ABMorph → abSlots.loadABPreset
    // Returns true on success, false if missing/parse/unsupported.
    bool loadPresetInto(ABSlotManager& abSlots,
                        const juce::String& presetName,
                        const juce::String& packName);
```

- [ ] **Step 2: Implement the routing function**

Edit `Source/PresetManager.cpp`. After the existing `loadPreset` implementation, add:

```cpp
bool PresetManager::loadPresetInto(ABSlotManager& abSlots,
                                   const juce::String& presetName,
                                   const juce::String& packName)
{
    auto file = getPresetFile(presetName, packName);
    if (!file.existsAsFile()) return false;

    auto xml = juce::parseXML(file);
    if (xml == nullptr) return false;

    auto tree = juce::ValueTree::fromXml(*xml);
    if (!tree.isValid()) return false;

    // Determine kind: prefer explicit metadata prop; else infer from SlotB presence.
    PresetKind kind = PresetKind::Single;
    if (auto meta = tree.getChildWithName(kMetadataNodeId); meta.isValid())
    {
        const auto s = meta.getProperty("presetKind", juce::var("")).toString();
        if (!s.isEmpty())
            kind = presetKindFromString(s);
        else if (tree.getChildWithName("SlotB").isValid())
            kind = PresetKind::AB;
    }
    else if (tree.getChildWithName("SlotB").isValid())
    {
        kind = PresetKind::AB;
    }

    const juce::String ref = packName + "/" + presetName;

    if (kind == PresetKind::Single)
    {
        // Active-slot-only load. Strip metadata before handing to abSlots so the
        // live APVTS doesn't get polluted by the Metadata child.
        auto stateForLoad = tree.createCopy();
        if (auto existingMeta = stateForLoad.getChildWithName(kMetadataNodeId); existingMeta.isValid())
            stateForLoad.removeChild(existingMeta, nullptr);
        abSlots.loadSinglePresetIntoActive(stateForLoad, ref);
    }
    else
    {
        // AB / ABMorph: hand the whole tree to abSlots.loadABPreset; it strips
        // <SlotB> and <MorphConfig> internally for slot A and reads <SlotB> for slot B.
        auto stateForLoad = tree.createCopy();
        if (auto existingMeta = stateForLoad.getChildWithName(kMetadataNodeId); existingMeta.isValid())
            stateForLoad.removeChild(existingMeta, nullptr);
        abSlots.loadABPreset(stateForLoad, ref);
    }

    return true;
}
```

- [ ] **Step 3: Update the `loadPreset` native function in `PluginEditor.cpp`**

Edit `Source/PluginEditor.cpp`, find the `.withNativeFunction("loadPreset", ...)` block (around line 397). Inside the `MessageManager::callAsync` body, replace the existing `loadPreset` call with `loadPresetInto`:

```cpp
                juce::MessageManager::callAsync(
                    [weakSelf = juce::Component::SafePointer<PhantomEditor>(&self), presetName, packName]
                    {
                        if (auto* ed = weakSelf.getComponent())
                            ed->processor.getPresetManager().loadPresetInto(
                                ed->processor.getABSlotManager(), presetName, packName);
                    });
```

- [ ] **Step 4: Build to verify**

```bash
cmake --build build --target KaigenPhantom --config Debug
cmake --build build --target KaigenPhantomTests --config Debug
```

Expected: builds clean.

- [ ] **Step 5: Commit**

```bash
git add Source/PresetManager.h Source/PresetManager.cpp Source/PluginEditor.cpp
git commit -m "feat(ab-compare): loadPresetInto dispatches by presetKind"
```

---

## Phase 5 — Native function bridge

### Task 15: Add four A/B native functions

**Files:**
- Modify: `Source/PluginEditor.cpp`

- [ ] **Step 1: Add the four `.withNativeFunction` blocks**

Edit `Source/PluginEditor.cpp`. Find the last existing `.withNativeFunction` in the chain (the one that ends the builder, look for `getAllPacks` around line 470). After its closing paren and before the next method call, insert:

```cpp
        .withNativeFunction("abGetState",
            [&self](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                auto& ab = self.processor.getABSlotManager();

                juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                obj->setProperty("active",
                    ab.getActive() == kaigen::phantom::ABSlotManager::Slot::A ? "A" : "B");
                obj->setProperty("modifiedA",
                    ab.isModified(kaigen::phantom::ABSlotManager::Slot::A));
                obj->setProperty("modifiedB",
                    ab.isModified(kaigen::phantom::ABSlotManager::Slot::B));

                const bool identical =
                    ab.getSlot(kaigen::phantom::ABSlotManager::Slot::A).toXmlString() ==
                    ab.getSlot(kaigen::phantom::ABSlotManager::Slot::B).toXmlString();
                obj->setProperty("slotsIdentical", identical);
                obj->setProperty("includeDiscrete", ab.getIncludeDiscreteInSnap());

                complete(juce::var(obj.get()));
            })
        .withNativeFunction("abSnapTo",
            [&self](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                if (args.size() < 1) { complete(juce::var(false)); return; }
                const auto slotStr = args[0].toString();
                const auto target = (slotStr == "B") ? kaigen::phantom::ABSlotManager::Slot::B
                                                     : kaigen::phantom::ABSlotManager::Slot::A;

                juce::MessageManager::callAsync(
                    [weakSelf = juce::Component::SafePointer<PhantomEditor>(&self), target]
                    {
                        if (auto* ed = weakSelf.getComponent())
                            ed->processor.getABSlotManager().snapTo(target);
                    });
                complete(juce::var(true));
            })
        .withNativeFunction("abCopy",
            [&self](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                juce::MessageManager::callAsync(
                    [weakSelf = juce::Component::SafePointer<PhantomEditor>(&self)]
                    {
                        if (auto* ed = weakSelf.getComponent())
                        {
                            auto& ab = ed->processor.getABSlotManager();
                            const auto src  = ab.getActive();
                            const auto dest = (src == kaigen::phantom::ABSlotManager::Slot::A)
                                              ? kaigen::phantom::ABSlotManager::Slot::B
                                              : kaigen::phantom::ABSlotManager::Slot::A;
                            ab.copy(src, dest);
                        }
                    });
                complete(juce::var(true));
            })
        .withNativeFunction("abSetIncludeDiscrete",
            [&self](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                if (args.size() < 1) { complete(juce::var(false)); return; }
                const bool on = args[0].isBool() ? (bool) args[0] : (((int) args[0]) != 0);
                self.processor.getABSlotManager().setIncludeDiscreteInSnap(on);
                complete(juce::var(true));
            })
```

- [ ] **Step 2: Update `getAllPresets` to emit `presetKind` in each row**

Edit `Source/PluginEditor.cpp`. Find the `.withNativeFunction("getAllPresets", ...)` block (around line 358). Inside the loop that builds each preset's JSON object, add a line for `presetKind`:

Look for lines that set `meta.name`, `meta.type`, `meta.designer`, etc. Nearby, add:

```cpp
                        obj->setProperty("presetKind",
                            kaigen::phantom::presetKindToString(pi.metadata.presetKind));
```

(The exact surrounding code already builds a `DynamicObject` per preset; you're adding one more `setProperty` call in that block.)

- [ ] **Step 3: Build to verify**

```bash
cmake --build build --target KaigenPhantom --config Debug
```

Expected: build succeeds.

- [ ] **Step 4: Commit**

```bash
git add Source/PluginEditor.cpp
git commit -m "feat(ab-compare): four native functions + presetKind in getAllPresets"
```

---

## Phase 6 — UI: header cluster

**Manual verification convention for Phase 6–8:** there is no JS test framework in this codebase. Each UI task includes a "verify manually in Ableton" step. The engineer must build the plugin, copy the VST3 to `C:\Users\kaislate\Downloads\KAIGEN\` (the user's manual-testing location per project memory), fully quit and restart Ableton Live 12 (DLLs are cached aggressively), load the plugin on a track, and visually confirm the behavior.

### Task 16: Add A/B cluster markup in the header + styles

**Files:**
- Modify: `Source/WebUI/index.html`
- Modify: `Source/WebUI/styles.css`

- [ ] **Step 1: Insert markup in the header between the save button and the mode toggle**

Edit `Source/WebUI/index.html`. Find the header closing section around line 113-115:

```html
      </div>
      <div style="flex: 1;"></div>
      <div class="mt"><div class="mb active" data-mode="0">Effect</div><div class="mb" data-mode="1">Resyn</div></div>
```

Replace that `<div style="flex: 1;"></div>` line *between* `</div>` (preset-controls) and `<div class="mt">` with:

```html
      </div>
      <div class="ab-group" id="ab-cluster" style="margin: 0 12px;">
        <button class="ab-btn active" id="ab-slot-a" title="Snap to slot A">A</button>
        <button class="cp-btn" id="ab-copy" title="Copy active slot to other slot">A→B</button>
        <button class="ab-btn" id="ab-slot-b" title="Snap to slot B">B
          <span class="ab-mod-dot" id="ab-mod-b"></span>
        </button>
      </div>
      <div style="flex: 1;"></div>
      <div class="mt"><div class="mb active" data-mode="0">Effect</div><div class="mb" data-mode="1">Resyn</div></div>
```

Note: slot A also needs a modified dot, but it's the initial active slot so hidden by default. Add it too — update both buttons:

```html
        <button class="ab-btn active" id="ab-slot-a" title="Snap to slot A">A
          <span class="ab-mod-dot" id="ab-mod-a"></span>
        </button>
        <button class="cp-btn" id="ab-copy" title="Copy active slot to other slot">A→B</button>
        <button class="ab-btn" id="ab-slot-b" title="Snap to slot B">B
          <span class="ab-mod-dot" id="ab-mod-b"></span>
        </button>
```

- [ ] **Step 2: Append styles to `styles.css`**

At the end of `Source/WebUI/styles.css`, append:

```css
/* ── A/B compare cluster ─────────────────────────────────────────── */
.ab-group {
  display: inline-flex;
  align-items: center;
  gap: 3px;
}

.ab-btn {
  width: 22px;
  height: 22px;
  border-radius: 50%;
  background: rgba(0, 0, 0, 0.08);
  color: rgba(0, 0, 0, 0.45);
  font-size: 10px;
  font-weight: 700;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  cursor: pointer;
  border: 1px solid rgba(0, 0, 0, 0.08);
  box-shadow: inset 0 1px 0 rgba(255, 255, 255, 0.4);
  padding: 0;
  position: relative;
}

.ab-btn.active {
  background: linear-gradient(180deg, #E8EAEC, #D4D6D8);
  color: rgba(0, 0, 0, 0.9);
  box-shadow: inset 0 1px 0 rgba(255, 255, 255, 0.6), 0 1px 2px rgba(0, 0, 0, 0.15);
  border-color: rgba(0, 0, 0, 0.15);
}

.ab-btn .ab-mod-dot {
  position: absolute;
  bottom: 1px;
  left: 50%;
  transform: translateX(-50%);
  width: 3px;
  height: 3px;
  border-radius: 50%;
  background: #c74a4a;
  display: none;
}

.ab-btn:not(.active) .ab-mod-dot.show {
  display: block;
}

.cp-btn {
  width: 26px;
  height: 22px;
  border-radius: 3px;
  background: rgba(0, 0, 0, 0.05);
  color: rgba(0, 0, 0, 0.55);
  font-size: 9px;
  font-weight: 600;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  cursor: pointer;
  border: 1px solid rgba(0, 0, 0, 0.08);
  padding: 0;
  letter-spacing: 0.3px;
}

.cp-btn:disabled,
.cp-btn.is-disabled {
  opacity: 0.4;
  cursor: not-allowed;
}

/* ── Browser A/B column badges ───────────────────────────────────── */
.kind-badge {
  display: inline-block;
  font-size: 9px;
  font-weight: 600;
  letter-spacing: 0.5px;
  padding: 2px 6px;
  border-radius: 3px;
}
.kind-badge--single {
  color: rgba(0, 0, 0, 0.25);
  padding: 0;
}
.kind-badge--ab {
  background: rgba(0, 0, 0, 0.06);
  color: rgba(0, 0, 0, 0.55);
}
.kind-badge--abm {
  background: rgba(0, 0, 0, 0.14);
  color: rgba(0, 0, 0, 0.75);
}
```

- [ ] **Step 3: Build and verify manually**

```bash
cmake --build build --target KaigenPhantom --config Debug
```

Copy the built VST3 to `C:\Users\kaislate\Downloads\KAIGEN\`. Fully close Ableton Live 12 and reopen. Load the plugin. Visually confirm:
- The A and B pills appear between the save button and the mode toggle.
- The "A" pill is lit (active style); the "B" pill is dim.
- The copy button shows `A→B`.
- Clicking does nothing yet (no JS wiring).

- [ ] **Step 4: Commit**

```bash
git add Source/WebUI/index.html Source/WebUI/styles.css
git commit -m "feat(ab-compare): add A/B cluster markup and neumorphic styles"
```

---

### Task 17: Wire A/B cluster behavior in `preset-system.js`

**Files:**
- Modify: `Source/WebUI/preset-system.js`

- [ ] **Step 1: Locate the init area and add A/B wiring**

Edit `Source/WebUI/preset-system.js`. Find the `initNativeBridge` function (search for `initNativeBridge`). Inside it, near where other native functions are resolved, add these four:

```javascript
    native.abGetState            = window.Juce.getNativeFunction('abGetState');
    native.abSnapTo              = window.Juce.getNativeFunction('abSnapTo');
    native.abCopy                = window.Juce.getNativeFunction('abCopy');
    native.abSetIncludeDiscrete  = window.Juce.getNativeFunction('abSetIncludeDiscrete');
```

- [ ] **Step 2: Add element refs in the `el` initialization area**

In the section of `preset-system.js` that caches DOM elements (search for `el.presetName` or similar), add:

```javascript
    el.abSlotA   = document.getElementById('ab-slot-a');
    el.abSlotB   = document.getElementById('ab-slot-b');
    el.abCopy    = document.getElementById('ab-copy');
    el.abModDotA = document.getElementById('ab-mod-a');
    el.abModDotB = document.getElementById('ab-mod-b');
```

- [ ] **Step 3: Add A/B state + render function**

Near the existing `state` object declaration, add:

```javascript
    state.ab = {
        active:         'A',
        modifiedA:      false,
        modifiedB:      false,
        slotsIdentical: true,
        includeDiscrete:false,
    };
```

Add a rendering function (somewhere near other render functions):

```javascript
    function renderABState() {
        if (!el.abSlotA || !el.abSlotB || !el.abCopy) return;
        const s = state.ab;

        // Active pill highlight
        el.abSlotA.classList.toggle('active', s.active === 'A');
        el.abSlotB.classList.toggle('active', s.active === 'B');

        // Copy label dynamic
        el.abCopy.textContent = s.active === 'A' ? 'A→B' : 'B→A';

        // Disabled state
        if (s.slotsIdentical) el.abCopy.classList.add('is-disabled');
        else                  el.abCopy.classList.remove('is-disabled');

        // Inactive-slot modified dot
        if (el.abModDotA) {
            el.abModDotA.classList.toggle('show',
                s.modifiedA && s.active !== 'A');
        }
        if (el.abModDotB) {
            el.abModDotB.classList.toggle('show',
                s.modifiedB && s.active !== 'B');
        }
    }

    async function refreshABState() {
        if (!native.abGetState) return;
        try {
            const r = await native.abGetState();
            if (r && typeof r === 'object') {
                state.ab = Object.assign(state.ab, r);
                renderABState();
            }
        } catch (e) { /* native not ready yet */ }
    }
```

- [ ] **Step 4: Wire button click handlers**

Near other DOM-wiring code in the init flow, add:

```javascript
    if (el.abSlotA) el.abSlotA.addEventListener('click', async () => {
        if (!native.abSnapTo) return;
        await native.abSnapTo('A');
        await refreshABState();
    });

    if (el.abSlotB) el.abSlotB.addEventListener('click', async () => {
        if (!native.abSnapTo) return;
        await native.abSnapTo('B');
        await refreshABState();
    });

    if (el.abCopy) el.abCopy.addEventListener('click', async () => {
        if (state.ab.slotsIdentical) return;
        if (!native.abCopy) return;
        await native.abCopy();
        await refreshABState();
    });
```

- [ ] **Step 5: Call `refreshABState()` on plugin load**

At the end of the init function (where other initial fetches happen), add:

```javascript
    await refreshABState();
```

- [ ] **Step 6: Hook into APVTS change events to flip local modified flag**

The existing WebSliderRelay chain listens to parameter changes. Find the handler that runs when a user-driven parameter change fires (search for `preset-modified` or where `el.presetModifiedIndicator` is toggled). Add a parallel update:

```javascript
    // Within the existing onParamChange handler:
    if (state.ab.active === 'A') state.ab.modifiedA = true;
    else                         state.ab.modifiedB = true;
    renderABState();
```

If no such central handler exists, add one after DOM ready — listen to `change` on all `<phantom-knob>` and `<toggle-group>` elements, then `renderABState()`.

- [ ] **Step 7: Build + manual verification**

```bash
cmake --build build --target KaigenPhantom --config Debug
```

Copy to KAIGEN, restart Ableton, load plugin. Verify:
- Click A/B — slot toggles, active style moves, copy-button label flips.
- Tweak a knob — red dot appears under the *other* slot's pill (once you snap away).
- Click copy when slots identical — button is dim and no-ops.
- Tweak to make slots differ, click copy — button activates; both slots now equal.

- [ ] **Step 8: Commit**

```bash
git add Source/WebUI/preset-system.js
git commit -m "feat(ab-compare): wire A/B snap/copy + active state + modified dot"
```

---

## Phase 7 — UI: browser column + filter

### Task 18: Add A/B column to the preset browser

**Files:**
- Modify: `Source/WebUI/preset-system.js` (`renderBrowserList` + row-render path)

- [ ] **Step 1: Add `kind` to the header column list**

In `preset-system.js:663-669` the `headerCols` array needs an entry for the new column. Edit:

```javascript
    const headerCols = [
        { id: 'name',     label: 'Name' },
        { id: 'type',     label: 'Type' },
        { id: 'designer', label: 'Designer' },
        { id: 'kind',     label: 'A/B' },
        { id: 'shape',    label: 'Shape' },
        { id: 'skip',     label: 'Skip' },
        { id: 'heart',    label: '♥', title: 'Sort by favorites' },
    ];
```

- [ ] **Step 2: Update the row grid template**

Find the row rendering in `preset-system.js:711`:

```javascript
                 style="display: grid; grid-template-columns: 1fr 72px 72px 140px 40px 30px; gap: 8px; padding: 6px 12px; background: ${bg}; border-radius: 3px; align-items: center; cursor: pointer; font-size: 11px; margin-bottom: 2px;">
```

Change the `grid-template-columns` to `1fr 72px 72px 54px 140px 40px 30px` (inserting 54px after Designer):

```javascript
                 style="display: grid; grid-template-columns: 1fr 72px 72px 54px 140px 40px 30px; gap: 8px; padding: 6px 12px; background: ${bg}; border-radius: 3px; align-items: center; cursor: pointer; font-size: 11px; margin-bottom: 2px;">
```

- [ ] **Step 3: Render the badge in the row**

Also in `renderBrowserList` (around line 710-718), insert a badge `<div>` between the Designer cell and the Shape SVG:

```javascript
        return `
            <div class="browser-row" data-name="${escapeAttr(r.meta.name)}" data-pack="${escapeAttr(r.pack)}"
                 style="display: grid; grid-template-columns: 1fr 72px 72px 54px 140px 40px 30px; gap: 8px; padding: 6px 12px; background: ${bg}; border-radius: 3px; align-items: center; cursor: pointer; font-size: 11px; margin-bottom: 2px;">
                <div style="color: rgba(0,0,0,0.85); font-weight: 500;">${escapeHtml(r.meta.name)}</div>
                <div style="color: rgba(0,0,0,0.60);">${escapeHtml(r.meta.type || '')}</div>
                <div style="color: rgba(0,0,0,0.60);">${escapeHtml(r.meta.designer || '')}</div>
                <div>${renderKindBadge(r.meta.presetKind)}</div>
                <svg class="browser-shape" viewBox="0 0 170 26" preserveAspectRatio="none"></svg>
                <div style="color: rgba(0,0,0,0.60); font-variant-numeric: tabular-nums;">${skipVal}</div>
                <div class="browser-heart" style="color: ${heartCol}; text-align: center; cursor: pointer;">${heart}</div>
            </div>
        `;
```

The header row uses the `.browser-header` CSS class (defined in `styles.css:536-538`), which already sets `grid-template-columns`. Update the CSS rule to match the new 7-column layout. Edit `Source/WebUI/styles.css`:

```css
.browser-header {
    display: grid;
    grid-template-columns: 1fr 72px 72px 54px 140px 40px 30px;
    /* …existing padding/font/color properties unchanged… */
}
```

(The row's inline `grid-template-columns` — updated in Step 2 above — must stay in sync with this CSS rule. Both are `1fr 72px 72px 54px 140px 40px 30px`.)

- [ ] **Step 4: Add the `renderKindBadge` helper**

Near the other small helper functions at the top of `preset-system.js` (alongside `escapeHtml` / `escapeAttr`), add:

```javascript
    function renderKindBadge(kind) {
        if (kind === 'ab')       return '<span class="kind-badge kind-badge--ab">A|B</span>';
        if (kind === 'ab_morph') return '<span class="kind-badge kind-badge--abm">A|B·M</span>';
        return '<span class="kind-badge kind-badge--single">—</span>';
    }
```

- [ ] **Step 5: Build + manual verification**

```bash
cmake --build build --target KaigenPhantom --config Debug
```

Copy, restart Ableton, load plugin, open preset browser. Confirm:
- New A/B column header appears between Designer and Shape.
- Every existing preset renders "—" (since no `presetKind` metadata on old files).
- Save a new preset as Single (default) → renders "—" in the column.

(Save-as-A/B isn't wired yet — Task 22.)

- [ ] **Step 6: Commit**

```bash
git add Source/WebUI/preset-system.js
git commit -m "feat(ab-compare): browser A/B column with softer-gray badges"
```

---

### Task 19: Add sort support for the `kind` column

**Files:**
- Modify: `Source/WebUI/preset-system.js` (`compareRows`)

- [ ] **Step 1: Add kind sort case**

Find the `compareRows` function (around line 144). It has a `switch` on `browserSort.column`. Add a case:

```javascript
        if (col === 'kind') {
            const order = { 'single': 0, 'ab': 1, 'ab_morph': 2 };
            const va = order[a.meta.presetKind || 'single'] ?? 0;
            const vb = order[b.meta.presetKind || 'single'] ?? 0;
            return (va - vb) * dir;
        }
```

(Place alongside existing cases like `col === 'name'`, `col === 'type'`, etc.)

- [ ] **Step 2: Build + manually verify**

```bash
cmake --build build --target KaigenPhantom --config Debug
```

Load plugin, open browser, click the "A/B" column header to sort. Expected: Single presets group together, then any A/B presets, then A/B+Morph. Click again → reverses.

- [ ] **Step 3: Commit**

```bash
git add Source/WebUI/preset-system.js
git commit -m "feat(ab-compare): sort preset browser by A/B kind"
```

---

### Task 20: Add filter dropdown (All / Single / A/B / Morph)

**Files:**
- Modify: `Source/WebUI/index.html` (or `preset-system.js` if the browser markup is JS-rendered)
- Modify: `Source/WebUI/preset-system.js`

- [ ] **Step 1: Locate the search input in the browser header markup**

Grep for `browser-search` to find where the search input is created (likely in `renderBrowser` in `preset-system.js`). It's rendered from JavaScript; check its surrounding container.

- [ ] **Step 2: Add a `<select>` next to the search input**

In the browser header render (around the `renderBrowser` block starting at line 462), find the `<input>` for search and add a sibling `<select>` immediately after:

```html
<select id="browser-kind-filter" style="margin-left: 8px; padding: 4px 8px; font-size: 11px; background: rgba(0,0,0,0.06); border: 1px solid rgba(0,0,0,0.10); border-radius: 3px; color: rgba(0,0,0,0.75);">
    <option value="all">All</option>
    <option value="single">Single</option>
    <option value="ab">A/B</option>
    <option value="morph">Morph</option>
</select>
```

(The exact insertion depends on the current render code — insert the `<select>` string into the HTML literal that renders the search row.)

- [ ] **Step 3: Track filter state**

Add to `state` declaration:

```javascript
    state.browserKindFilter = 'all';
```

- [ ] **Step 4: Wire the filter and apply it**

In `renderBrowserList`, after the row-filtering by search term, add a second pass filtering by kind:

```javascript
    let filtered = rows;   // rename the existing filtered-rows variable if different
    if (state.browserKindFilter === 'single') {
        filtered = filtered.filter(r => !r.meta.presetKind || r.meta.presetKind === 'single');
    } else if (state.browserKindFilter === 'ab') {
        filtered = filtered.filter(r => r.meta.presetKind === 'ab');
    } else if (state.browserKindFilter === 'morph') {
        filtered = filtered.filter(r => r.meta.presetKind === 'ab_morph');
    }
    // ... then proceed with existing sort + render on `filtered`
```

Wire the select change in the same place the search input handler is set up:

```javascript
    const kindFilter = document.getElementById('browser-kind-filter');
    if (kindFilter) {
        kindFilter.value = state.browserKindFilter;
        kindFilter.addEventListener('change', (e) => {
            state.browserKindFilter = e.target.value;
            renderBrowserList(document.getElementById('browser-search').value);
        });
    }
```

- [ ] **Step 5: Build + manually verify**

```bash
cmake --build build --target KaigenPhantom --config Debug
```

Open browser. Cycle through filter options. Expected: list narrows/broadens as the filter changes. "All" shows everything.

- [ ] **Step 6: Commit**

```bash
git add Source/WebUI/preset-system.js
git commit -m "feat(ab-compare): kind-filter dropdown in preset browser"
```

---

## Phase 8 — UI: save modal + settings

### Task 21: Add Preset Kind radio to save modal

**Files:**
- Modify: `Source/WebUI/preset-system.js` (save modal render path)

- [ ] **Step 1: Locate the save modal render**

Grep for `preset-save-modal` or `openSaveModal` (the modal is "Populated by preset-system.js" per `index.html:74`). Find the function that builds the modal body.

- [ ] **Step 2: Insert the radio group below the Type dropdown**

In the modal body's HTML template, after the Type `<select>` element, insert:

```html
<div style="margin-top: 12px;">
    <label style="font-size: 10px; color: rgba(0,0,0,0.60); text-transform: uppercase; letter-spacing: 0.5px;">Preset Kind</label>
    <div id="save-kind-row" style="display: flex; gap: 14px; align-items: center; margin-top: 6px; font-size: 12px;">
        <label style="display: flex; align-items: center; gap: 4px; cursor: pointer;">
            <input type="radio" name="save-kind" value="single" checked> Single
        </label>
        <label id="save-kind-ab-wrap" style="display: flex; align-items: center; gap: 4px; cursor: pointer;">
            <input type="radio" name="save-kind" value="ab" id="save-kind-ab"> A/B
        </label>
    </div>
    <div id="save-kind-helper" style="display: none; font-size: 10px; color: rgba(199,74,74,0.9); margin-top: 4px;">
        Slot B is unchanged from Slot A. Snap to B and make edits first, or save as Single.
    </div>
</div>
```

(For Standard build we do NOT render the "A/B + Morph" option — Pro-only per spec.)

- [ ] **Step 3: Add disable logic based on `slotsIdentical`**

In the function that opens the save modal, add before showing it:

```javascript
    const abRadio      = document.getElementById('save-kind-ab');
    const abWrap       = document.getElementById('save-kind-ab-wrap');
    const kindHelper   = document.getElementById('save-kind-helper');

    await refreshABState();  // ensure slotsIdentical is current
    const disabled = !!state.ab.slotsIdentical;

    if (abRadio) abRadio.disabled = disabled;
    if (abWrap)  abWrap.style.opacity = disabled ? '0.4' : '1';
    if (abWrap)  abWrap.style.cursor = disabled ? 'not-allowed' : 'pointer';
    if (kindHelper) kindHelper.style.display = disabled ? 'block' : 'none';

    // Force selection back to Single if AB was selected and is now disabled.
    if (disabled) {
        const singleRadio = document.querySelector('input[name="save-kind"][value="single"]');
        if (singleRadio) singleRadio.checked = true;
    }
```

- [ ] **Step 4: Read the radio value on save submit**

In the save modal's submit handler (the function that calls `native.savePreset(...)`), read the kind first:

```javascript
    const kindChecked = document.querySelector('input[name="save-kind"]:checked');
    const kindStr = kindChecked ? kindChecked.value : 'single';
```

Append `kindStr` to the arguments passed to `native.savePreset(...)`:

```javascript
    const savedName = await native.savePreset(
        name, type, designer, description, overwrite, kindStr
    );
```

(The C++ side was updated in Task 13 to read `args[5]` as the kind string.)

- [ ] **Step 5: Build + manually verify**

```bash
cmake --build build --target KaigenPhantom --config Debug
```

Load plugin in Ableton. Click Save. Expected:
- Radio group shows *Single* (default) + *A/B*.
- With slots identical (fresh plugin): A/B is dimmed and helper text visible.
- Save-as-Single works exactly like before.
- Tweak a knob on slot A, snap to B, tweak different knobs, open Save → A/B now enabled, helper hidden.
- Save as A/B → new preset appears in the browser with the "A|B" badge.

- [ ] **Step 6: Commit**

```bash
git add Source/WebUI/preset-system.js
git commit -m "feat(ab-compare): save modal Preset Kind radio + disable-when-identical"
```

---

### Task 22: Add Compare section to settings panel + wire toggle

**Files:**
- Modify: `Source/WebUI/index.html` (settings modal markup)
- Modify: `Source/WebUI/preset-system.js` (or existing settings-panel handler)

- [ ] **Step 1: Find the settings modal**

Grep `index.html` for `settings-header` (seen at line 313). Note the nearby structure — find its content area.

- [ ] **Step 2: Append a Compare subsection**

In the settings modal body, after any existing sections, add:

```html
<div class="settings-section" style="margin-top: 20px;">
    <div style="font-size: 10px; color: rgba(0,0,0,0.50); text-transform: uppercase; letter-spacing: 1px; margin-bottom: 8px;">COMPARE</div>
    <label style="display: flex; align-items: flex-start; gap: 8px; cursor: pointer; font-size: 12px; color: rgba(0,0,0,0.80);">
        <input type="checkbox" id="setting-include-discrete" style="margin-top: 3px;">
        <span>
            Include discrete parameters when snapping A/B
            <div style="font-size: 10px; color: rgba(0,0,0,0.50); margin-top: 2px;">
                Mode, Bypass, Ghost Mode, and Binaural Mode also flip between slots.
            </div>
        </span>
    </label>
</div>
```

(If the settings modal is rendered from JS, insert this HTML into the render path instead.)

- [ ] **Step 3: Wire the checkbox**

In `preset-system.js`, in the init area (alongside other DOM wiring), add:

```javascript
    const discreteCheckbox = document.getElementById('setting-include-discrete');
    if (discreteCheckbox) {
        // Initialize from current state
        refreshABState().then(() => {
            discreteCheckbox.checked = !!state.ab.includeDiscrete;
        });

        discreteCheckbox.addEventListener('change', async (e) => {
            if (!native.abSetIncludeDiscrete) return;
            await native.abSetIncludeDiscrete(e.target.checked);
            await refreshABState();
        });
    }
```

- [ ] **Step 4: Build + manually verify**

```bash
cmake --build build --target KaigenPhantom --config Debug
```

Open settings panel. Verify:
- Compare subsection visible with one checkbox + helper text.
- Toggling persists across plugin close/reopen within same project (per-project storage).
- Toggling changes snap behavior: with it ON, differing Ghost Modes between slots flip on snap; with OFF, they don't (unless a designer-authored preset is loaded).

- [ ] **Step 5: Commit**

```bash
git add Source/WebUI/index.html Source/WebUI/preset-system.js
git commit -m "feat(ab-compare): settings-panel Compare toggle"
```

---

## Phase 9 — Manual verification + checklist

### Task 23: Update `MANUAL_TEST_CHECKLIST.md` + final verification

**Files:**
- Modify: `MANUAL_TEST_CHECKLIST.md`

- [ ] **Step 1: Append an A/B compare section**

Open `MANUAL_TEST_CHECKLIST.md`. At the end, add:

```markdown

## A/B Compare

Build with Debug config, copy VST3 to `C:\Users\kaislate\Downloads\KAIGEN\`, fully restart Ableton Live 12, load the plugin on a fresh track.

- [ ] **Snap with include-discrete OFF (default).** Set slot A Ghost Mode = Replace. Snap to B. Set slot B Ghost Mode = Phantom Only. Snap A↔B a few times — Ghost Mode stays fixed (no audible click at crossover). Only continuous params flip.
- [ ] **Standard build ignores `<MorphConfig>`.** Hand-craft a `.fxp` file containing `<SlotB>` *and* `<MorphConfig defaultPosition="0.5" curve="linear" />`, drop it into `%APPDATA%/Kaigen/KaigenPhantom/Presets/User/`. Load it — the plugin should treat it as a plain A/B preset (both slots restored; morph metadata silently ignored since `KAIGEN_PRO_BUILD` is not defined in this build).
- [ ] **Snap with include-discrete ON.** Open settings, enable "Include discrete parameters when snapping A/B". Repeat the previous test — now Ghost Mode flips on snap and a click is audible if the two engines are in materially different states.
- [ ] **Designer-authored preset override.** Turn include-discrete OFF. Load a factory A/B preset that has different Ghost Modes between slots (may need to create one for testing). Snap A↔B — discrete params flip regardless of the setting (designer intent).
- [ ] **Designer-authored clears on edit.** From the previous state, tweak any knob. Snap A↔B — discrete params no longer flip (designerAuthored cleared).
- [ ] **Per-slot modified indicator.** Snap to B, tweak a param, snap back to A. The "B" pill should show a small red dot under the letter. Top-level preset asterisk reflects only the active slot.
- [ ] **DAW project save/reload.** Configure distinct slot A and B content, set active = B, enable include-discrete, save the Ableton project. Close and reopen. Plugin state: slots restore with correct content, active = B, include-discrete still ON.
- [ ] **Browser A/B column.** Open preset browser. Existing presets render "—". Save a preset as A/B → row shows "A|B" badge. Sort by column works (Single → A/B → Morph). Filter dropdown narrows list correctly.
- [ ] **Save modal Preset Kind.** With slots identical: A/B radio dimmed + helper text. With slots different: A/B radio enabled. Save as A/B — reload the preset — both slots match what was saved.
```

- [ ] **Step 2: Run ALL unit tests one last time**

```bash
cmake --build build --target KaigenPhantomTests --config Debug
./build/tests/KaigenPhantomTests.exe
```

Expected: all tests pass (existing + new).

- [ ] **Step 3: Build the Release VST3 + copy for Ableton**

```bash
cmake --build build --target KaigenPhantom --config Release
# Copy command depends on user setup; the plugin's post-build step handles
# the %LOCALAPPDATA% copy. The manual-test destination is:
#   C:\Users\kaislate\Downloads\KAIGEN\
```

Walk through the `MANUAL_TEST_CHECKLIST.md` A/B section end-to-end.

- [ ] **Step 4: Commit the checklist + any bugfixes from manual testing**

```bash
git add MANUAL_TEST_CHECKLIST.md
git commit -m "docs(ab-compare): add A/B manual test checklist"
```

- [ ] **Step 5: Squash / rebase if desired, then confirm the feature branch is ready for PR review**

At this point the feature is complete. Push the branch and open a PR.

---

## Done

All spec requirements implemented:

- [x] `ABSlotManager` class with full API (snap/copy/load/persist)
- [x] Discrete-param exclusion with designer-authored override
- [x] Per-slot modified tracking
- [x] Plugin state persistence (`<ABSlots>` in getStateInformation)
- [x] `PresetManager` extensions (PresetKind enum, savePreset accepts kind, loadPresetInto routes by kind)
- [x] Native function bridge (abGetState, abSnapTo, abCopy, abSetIncludeDiscrete)
- [x] Header A/B cluster UI with neumorphic styling
- [x] Browser A/B column with softer-gray badges, sort, filter
- [x] Save modal Preset Kind radio with disable-when-identical helper
- [x] Settings panel Compare toggle
- [x] Pro seam: `ABSlotManager::getSlot` accessor + `<SlotB>`/`<MorphConfig>`/`presetKind="ab_morph"` format support (writing `<MorphConfig>` is gated by `KAIGEN_PRO_BUILD`, not yet defined)
- [x] 15+ unit tests + manual test checklist
