# PR 3b: Macro Editor UI Implementation Plan

> **Superseded by PR3b-rev (2026-05-06):** The drawer + per-macro editor described in Tasks 4–6 of this plan was replaced by a Vital/Roar-style matrix view. See `docs/superpowers/specs/2026-05-06-pr3b-rev-matrix-view-design.md` and `docs/superpowers/plans/2026-05-06-pr3b-rev-matrix-view.md`. Tasks 1–3 (C++ framework + native bindings) of this plan are still in effect; their code shipped in PR3a (commit `e4ed112`).

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the bottom modulation panel (11-slot row from the spec), drawer-expansion mechanics, and a working macro editor inside the drawer. Users can directly control macros from the plugin UI (no longer host-automation only), name them ("Motion", "Shape", etc.), add/remove routing destinations via a param-picker dropdown, and adjust per-routing depth. PR3a's hardcoded proof-of-life routing is removed once users can create their own.

**Architecture:** Foundational tasks first — replace `std::vector<Routing>` in `ModulationEngine` with `std::atomic<std::shared_ptr<const std::vector<Routing>>>` so audio-thread reads are lock-free during UI-driven add/remove/setDepth at runtime. Wrap `DualEngineHost`'s ModulationEngine pointers in `std::atomic` for the same reason. New native bindings (`modulationGetState`, `modulationAddRouting`, `modulationRemoveRouting`, `modulationSetRoutingDepth`, `modulationSetMacroName`) expose CRUD to the WebUI. Bottom panel + drawer + macro editor are built in `Source/WebUI/`. The macro editor's "+ Add destination" button surfaces a dropdown of all per-engine eligible params (filtered by the macro's owning engine) using each param's user-facing display name.

**Tech Stack:** C++20, JUCE 8.0.4, Catch2 v3.5.2, WebView2, HTML/CSS/JS.

---

## Decisions baked into this plan

- **Routing-create UX is a dropdown picker** (not drag-to-assign — that's PR3c). Click "+ Add destination" in the macro editor → dropdown of eligible per-engine params with display names ("Ghost", "Phantom Threshold", "Recipe H2", etc.) → click to add at +50% default depth.
- **Persistence stays plugin state.** `<ModulationConfig>` keeps living in `<PluginState>`. Routings follow the plugin instance, not the preset. PR3c (or later) revisits if usage warrants per-preset routings.
- **Lock-free pattern is `std::atomic<std::shared_ptr<const vector>>`.** Message thread allocates the new vector + swaps; audio thread atomic-loads the shared_ptr and iterates. Bounded work on writes; truly lock-free reads. If profiling later shows the audio-thread's shared_ptr destructor matters, switch to a `juce::AbstractFifo` SPSC queue of routing-change commands. PR3b accepts the simpler pattern.
- **`setModulationEngines` pointers wrapped in `std::atomic<ModulationEngine*>`.** Cheap future-proofing flagged in PR3a's review.
- **Bottom panel layout: 11 slots, but only 4 macro slots interactive.** LFO 1/2/3/4 slots, Random A/B slots, Morph slot all render but are placeholder/inert in PR3b. PR4 (LFO), PR5 (Random) wire them. Morph slot already has its own existing slider — the bottom-panel slot for Morph is a navigational mirror.
- **Drawer mechanics (B from PR2 brainstorm):** drawer slides up *above* the row. Plugin window grows by ~140px when drawer open. Row stays visible underneath with the active macro slot highlighted. Click another macro slot → drawer reshapes instantly. Click the active slot or a collapse caret → drawer hides.
- **Macro slot visual identity:** uses the cyan-blue Macro color from the design spec (#5DD3E0 or its CSS-color sibling). Other slot types use placeholder gray until their PRs land.
- **Macro editor layout (single drawer state per type):** left = big macro knob + numeric value + editable name field; right = destinations list (target name, depth slider centered at 0, ×) + "+ Add destination" button.
- **Param picker:** native modal-style dropdown rendered inside the drawer. Lists eligible per-engine params with display names. Clicking adds the routing at default +50% depth.
- **Remove hardcoded proof-of-life routing** in `PhantomProcessor` constructor as the last task before the smoke test. Past this point, only user-created (or persisted-from-saved-state) routings exist.

---

## File Plan

**Created:**
- `Source/WebUI/modulation-panel.js` — bottom panel renderer + slot click handlers + drawer state machine.
- `Source/WebUI/macro-editor.js` — drawer content for macro editor (knob + name + destinations list + add picker).
- `tests/RoutingMutationTests.cpp` — verifies the atomic-snapshot pattern (sequential adds/removes from "audio-thread" perspective don't tear).

**Modified:**
- `Source/Modulation/ModulationEngine.h` — replace `std::vector<Routing> routings` with `std::atomic<std::shared_ptr<const std::vector<Routing>>>`. Helper `setRoutingDepth(sourceId, paramId, depth)` added. `getRoutings()` returns by value (snapshot).
- `Source/Modulation/ModulationEngine.cpp` — implement the atomic-snapshot pattern.
- `Source/DualEngineHost.h` — wrap `modA`/`modB` in `std::atomic<ModulationEngine*>`.
- `Source/DualEngineHost.cpp` — atomic load in `syncEngineFromPrefix`'s prefix-dispatch.
- `Source/PluginEditor.h` — new ToggleRelays / ComboBoxRelays for the macro editor controls (the macro knobs are existing relays via the macro1-4 APVTS params; we need WebSliderRelays for those if not already added).
- `Source/PluginEditor.cpp` — new `withNativeFunction` registrations for routing CRUD; macro1-4 WebSliderRelay registrations + attachments.
- `Source/PluginProcessor.cpp` — remove the hardcoded proof-of-life routing.
- `Source/WebUI/index.html` — add the bottom modulation panel container.
- `Source/WebUI/styles.css` — bottom panel + slot + drawer + macro editor styling.
- `Source/WebUI/phantom.js` — bootstrap for `modulation-panel.js` + `macro-editor.js`.
- `tests/CMakeLists.txt` — add `RoutingMutationTests.cpp`.

**Deleted:** none.

---

## Task 1: Lock-free routing mutation (foundational, TDD)

**Files:**
- Modify: `Source/Modulation/ModulationEngine.h`
- Modify: `Source/Modulation/ModulationEngine.cpp`
- Create: `tests/RoutingMutationTests.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Update `ModulationEngine.h`**

Replace the existing `routings` declaration:

```cpp
    std::vector<Routing> routings;
```

with:

```cpp
    using RoutingsList = std::vector<Routing>;
    using RoutingsSnapshot = std::shared_ptr<const RoutingsList>;
    std::atomic<RoutingsSnapshot> routingsAtomic { std::make_shared<const RoutingsList>() };
```

(Note: `std::atomic<std::shared_ptr<T>>` is C++20; available in MSVC 2022 + Clang 14+. Verify the toolchain supports it; if not, use `std::atomic_load`/`std::atomic_store` free functions on a regular `shared_ptr` — pre-C++20 fallback.)

Update accessor signatures:

```cpp
    /** Returns a snapshot of the current routing list. Caller can iterate
     *  safely; the underlying vector is immutable. Subsequent mutations
     *  (add/remove/setDepth) produce a new snapshot, not in-place changes. */
    RoutingsSnapshot getRoutingsSnapshot() const noexcept;

    /** For convenience: returns by-value copy of the current snapshot's
     *  vector. Use sparingly — copies the vector. Prefer getRoutingsSnapshot
     *  + iteration for hot paths. */
    std::vector<Routing> getRoutings() const;
```

Add a depth-mutator:

```cpp
    /** Update the depth of an existing routing. Returns false if no routing
     *  matches sourceId+paramId. */
    bool setRoutingDepth(const juce::String& sourceId, const juce::String& paramId, float newDepth);
```

- [ ] **Step 2: Update `ModulationEngine.cpp`** with the atomic-snapshot mutators

Replace the existing add/remove/clear implementations:

```cpp
bool ModulationEngine::addRouting(const Routing& r)
{
    if (! r.paramId.startsWith(prefix)) return false;
    if (findModulator(r.sourceId) == nullptr) return false;

    auto current = routingsAtomic.load();
    auto next = std::make_shared<RoutingsList>(*current);
    next->push_back(r);
    routingsAtomic.store(std::shared_ptr<const RoutingsList>(next));
    return true;
}

void ModulationEngine::removeRouting(const juce::String& sourceId, const juce::String& paramId)
{
    auto current = routingsAtomic.load();
    auto next = std::make_shared<RoutingsList>(*current);
    next->erase(std::remove_if(next->begin(), next->end(),
        [&](const Routing& r) { return r.sourceId == sourceId && r.paramId == paramId; }),
        next->end());
    routingsAtomic.store(std::shared_ptr<const RoutingsList>(next));
}

void ModulationEngine::clearRoutings()
{
    routingsAtomic.store(std::make_shared<const RoutingsList>());
}

bool ModulationEngine::setRoutingDepth(const juce::String& sourceId, const juce::String& paramId, float newDepth)
{
    auto current = routingsAtomic.load();
    auto next = std::make_shared<RoutingsList>(*current);
    bool found = false;
    for (auto& r : *next)
    {
        if (r.sourceId == sourceId && r.paramId == paramId)
        {
            r.depth = newDepth;
            found = true;
            break;
        }
    }
    if (! found) return false;
    routingsAtomic.store(std::shared_ptr<const RoutingsList>(next));
    return true;
}

ModulationEngine::RoutingsSnapshot ModulationEngine::getRoutingsSnapshot() const noexcept
{
    return routingsAtomic.load();
}

std::vector<Routing> ModulationEngine::getRoutings() const
{
    return *routingsAtomic.load();
}
```

Update `getModulatedValue` to use the snapshot:

```cpp
float ModulationEngine::getModulatedValue(const juce::String& paramId, float base) const
{
    auto* paramPtr = apvts.getParameter(paramId);
    if (paramPtr == nullptr) return base;
    const auto& range = paramPtr->getNormalisableRange();
    const float span = range.end - range.start;

    auto snapshot = routingsAtomic.load();
    if (! snapshot || snapshot->empty()) return base;

    float modulated = base;
    for (const auto& r : *snapshot)
    {
        if (r.paramId != paramId) continue;
        auto* m = findModulator(r.sourceId);
        if (m == nullptr) continue;
        const float modVal = m->getCurrentValue();
        const float effDepth = r.polarityInverted ? -r.depth : r.depth;
        modulated += effDepth * modVal * span;
    }

    return juce::jlimit(range.start, range.end, modulated);
}
```

Update `toValueTree` / `fromValueTree`:

```cpp
juce::ValueTree ModulationEngine::toValueTree() const
{
    juce::ValueTree node("Engine");
    node.setProperty("prefix", prefix, nullptr);
    juce::ValueTree mods("Modulators");
    for (auto& m : modulators) m->writeToTree(mods);
    node.appendChild(mods, nullptr);
    juce::ValueTree routes("Routings");
    auto snapshot = routingsAtomic.load();
    for (const auto& r : *snapshot) routes.appendChild(r.toValueTree(), nullptr);
    node.appendChild(routes, nullptr);
    return node;
}

void ModulationEngine::fromValueTree(const juce::ValueTree& engineNode)
{
    if (! engineNode.hasType("Engine")) return;
    if (engineNode.getProperty("prefix").toString() != prefix) return;

    auto modsNode = engineNode.getChildWithName("Modulators");
    for (auto& m : modulators) m->readFromTree(modsNode);

    auto next = std::make_shared<RoutingsList>();
    auto routesNode = engineNode.getChildWithName("Routings");
    for (int i = 0; i < routesNode.getNumChildren(); ++i)
    {
        auto child = routesNode.getChild(i);
        if (! child.hasType("Route")) continue;
        Routing r = Routing::fromValueTree(child);
        // Validate (same as addRouting): correct prefix + known modulator.
        if (! r.paramId.startsWith(prefix)) continue;
        if (findModulator(r.sourceId) == nullptr) continue;
        next->push_back(r);
    }
    routingsAtomic.store(std::shared_ptr<const RoutingsList>(next));
}
```

- [ ] **Step 3: Write `tests/RoutingMutationTests.cpp`**

Tests verify the snapshot pattern: a getModulatedValue call (the audio-thread analog) sees a consistent view even if a mutation is interleaved. Catch2 doesn't trivially run two threads, so we test the snapshot semantics deterministically.

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Modulation/ModulationEngine.h"
#include "Modulation/Macro.h"
#include <juce_audio_processors/juce_audio_processors.h>

using namespace kaigen::phantom;
using Catch::Approx;

namespace {

class StubProc : public juce::AudioProcessor
{
public:
    StubProc()
        : AudioProcessor(BusesProperties().withInput("In",  juce::AudioChannelSet::stereo(), true)
                                          .withOutput("Out", juce::AudioChannelSet::stereo(), true))
    {}
    const juce::String getName() const override { return "StubProc"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}
};

juce::AudioProcessorValueTreeState::ParameterLayout makeRoutingMutationLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;
    params.push_back(std::make_unique<AudioParameterFloat>(
        "macro1", "Macro 1", NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "a_ghost", "A. Ghost", NormalisableRange<float>(0.0f, 100.0f), 50.0f));
    return { params.begin(), params.end() };
}

} // namespace

TEST_CASE("RoutingsSnapshot independent of subsequent add", "[routing-mut]")
{
    StubProc proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "RM_TEST", makeRoutingMutationLayout());
    ModulationEngine modA(apvts, "a_");
    modA.addModulator(std::make_unique<Macro>("macro1", apvts, "macro1"));

    auto snapshotBefore = modA.getRoutingsSnapshot();
    REQUIRE(snapshotBefore->empty());

    Routing r; r.sourceId = "macro1"; r.paramId = "a_ghost"; r.depth = 0.5f;
    REQUIRE(modA.addRouting(r));

    // Snapshot taken before the add still sees the empty list.
    REQUIRE(snapshotBefore->empty());

    auto snapshotAfter = modA.getRoutingsSnapshot();
    REQUIRE(snapshotAfter->size() == 1);
}

TEST_CASE("setRoutingDepth changes existing routing", "[routing-mut]")
{
    StubProc proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "RM_TEST", makeRoutingMutationLayout());
    ModulationEngine modA(apvts, "a_");
    modA.addModulator(std::make_unique<Macro>("macro1", apvts, "macro1"));

    Routing r; r.sourceId = "macro1"; r.paramId = "a_ghost"; r.depth = 0.5f;
    modA.addRouting(r);

    REQUIRE(modA.setRoutingDepth("macro1", "a_ghost", 0.75f));

    auto snapshot = modA.getRoutingsSnapshot();
    REQUIRE(snapshot->size() == 1);
    REQUIRE((*snapshot)[0].depth == 0.75f);
}

TEST_CASE("setRoutingDepth returns false for unknown routing", "[routing-mut]")
{
    StubProc proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "RM_TEST", makeRoutingMutationLayout());
    ModulationEngine modA(apvts, "a_");
    modA.addModulator(std::make_unique<Macro>("macro1", apvts, "macro1"));

    REQUIRE_FALSE(modA.setRoutingDepth("macro1", "a_ghost", 0.5f));
}

TEST_CASE("clearRoutings replaces snapshot atomically", "[routing-mut]")
{
    StubProc proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "RM_TEST", makeRoutingMutationLayout());
    ModulationEngine modA(apvts, "a_");
    modA.addModulator(std::make_unique<Macro>("macro1", apvts, "macro1"));

    Routing r; r.sourceId = "macro1"; r.paramId = "a_ghost"; r.depth = 0.5f;
    modA.addRouting(r);

    auto before = modA.getRoutingsSnapshot();
    REQUIRE(before->size() == 1);

    modA.clearRoutings();

    REQUIRE(before->size() == 1);   // before-snapshot still sees the routing
    REQUIRE(modA.getRoutingsSnapshot()->empty());
}

TEST_CASE("getModulatedValue uses current snapshot, not stale", "[routing-mut]")
{
    StubProc proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "RM_TEST", makeRoutingMutationLayout());
    ModulationEngine modA(apvts, "a_");
    modA.addModulator(std::make_unique<Macro>("macro1", apvts, "macro1"));

    REQUIRE(modA.getModulatedValue("a_ghost", 50.0f) == 50.0f);

    Routing r; r.sourceId = "macro1"; r.paramId = "a_ghost"; r.depth = 0.5f;
    modA.addRouting(r);

    // macro1 default 0.5; depth 0.5; range 100; modulation = 0.5*0.5*100 = 25.
    // base 50 + 25 = 75.
    REQUIRE(modA.getModulatedValue("a_ghost", 50.0f) == Approx(75.0f).margin(1.0e-3f));
}
```

- [ ] **Step 4: Wire `RoutingMutationTests.cpp` into `tests/CMakeLists.txt`**

Add `RoutingMutationTests.cpp` to the test target's source list, alongside `ModulationEngineTests.cpp`.

- [ ] **Step 5: Build + run all tests**

```
cmake --build build --target KaigenPhantomTests --config Debug
./build/tests/Debug/KaigenPhantomTests.exe
```

Expected: 100 cases (95 PR3a baseline + 5 new under `[routing-mut]`). All pass.

```
cmake --build build --target KaigenPhantom_VST3 --config Debug
```

Expected: clean build (the existing `[modengine]` tests should still pass — they exercise the same `addRouting`/`getModulatedValue` API).

- [ ] **Step 6: Commit**

```bash
git add Source/Modulation/ModulationEngine.h Source/Modulation/ModulationEngine.cpp \
        tests/RoutingMutationTests.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(modulation): atomic-snapshot routing mutation pattern

Replaces std::vector<Routing> with std::atomic<std::shared_ptr<const
std::vector<Routing>>>. Message-thread mutators (add/remove/setDepth/
clear) build a new vector and atomically swap. Audio-thread reads
(getModulatedValue) atomic-load the snapshot and iterate; lock-free.

Bounded work on writes (one allocation + one atomic store). Reads are
truly lock-free. shared_ptr destructor on the audio thread is the
remaining concern — acceptable for visualization-grade modulation;
revisit with juce::AbstractFifo SPSC pattern if profiling shows it
matters.

Adds setRoutingDepth(sourceId, paramId, depth) for the upcoming UI's
depth-slider drag handler. 5 new test cases verify snapshot semantics.

Foundational for PR3b's UI-driven CRUD (I-1 from PR3a's review).
EOF
)"
```

---

## Task 2: Atomic ModulationEngine pointers in DualEngineHost

**Files:**
- Modify: `Source/DualEngineHost.h`
- Modify: `Source/DualEngineHost.cpp`

- [ ] **Step 1: Wrap pointers in `std::atomic<ModulationEngine*>`**

In `DualEngineHost.h`, change:

```cpp
    kaigen::phantom::ModulationEngine* modA { nullptr };
    kaigen::phantom::ModulationEngine* modB { nullptr };
```

to:

```cpp
    std::atomic<kaigen::phantom::ModulationEngine*> modA { nullptr };
    std::atomic<kaigen::phantom::ModulationEngine*> modB { nullptr };
```

- [ ] **Step 2: Update setter to `store` atomically**

In `DualEngineHost.cpp::setModulationEngines`:

```cpp
void DualEngineHost::setModulationEngines(kaigen::phantom::ModulationEngine* a,
                                           kaigen::phantom::ModulationEngine* b) noexcept
{
    modA.store(a, std::memory_order_release);
    modB.store(b, std::memory_order_release);
}
```

- [ ] **Step 3: Update reads in `syncEngineFromPrefix` to `load`**

In `DualEngineHost.cpp::syncEngineFromPrefix`, where `modEng` is resolved from `prefix`:

```cpp
    auto* modEng = (juce::String(prefix) == "a_") ? modA.load(std::memory_order_acquire)
                 : (juce::String(prefix) == "b_") ? modB.load(std::memory_order_acquire)
                 : nullptr;
```

- [ ] **Step 4: Build + run tests**

```
cmake --build build --target KaigenPhantom_VST3 --config Debug
./build/tests/Debug/KaigenPhantomTests.exe
```

Expected: 100 cases pass. Clean build.

- [ ] **Step 5: Commit**

```bash
git add Source/DualEngineHost.h Source/DualEngineHost.cpp
git commit -m "$(cat <<'EOF'
fix(modulation): atomic ModulationEngine pointers in DualEngineHost

Wraps modA/modB in std::atomic<ModulationEngine*>. Future-proofs
against any code path that calls setModulationEngines after audio
has started; today's caller (PhantomProcessor constructor) only
sets once before prepareToPlay, so behavior is unchanged.

Closes I-2 from PR3a review.
EOF
)"
```

---

## Task 3: Native bindings for routing CRUD + macro queries

**Files:**
- Modify: `Source/PluginEditor.h` — add WebSliderRelays for macro1-4 (these are now user-facing knobs, not host-automation only)
- Modify: `Source/PluginEditor.cpp` — register relays + 5 new native bindings

- [ ] **Step 1: Add macro WebSliderRelays in `PluginEditor.h`**

In the `WebSliderRelay` declarations section (alongside `bypassRelay` etc.), add:

```cpp
    juce::WebSliderRelay macro1Relay { "macro1" };
    juce::WebSliderRelay macro2Relay { "macro2" };
    juce::WebSliderRelay macro3Relay { "macro3" };
    juce::WebSliderRelay macro4Relay { "macro4" };
```

(These are global params, not per-engine, so they're NOT `RelayA`/`RelayB` paired — single relay each, like `bypass`.)

- [ ] **Step 2: Add WebSliderParameterAttachments in `PluginEditor.cpp`**

In the attachment declarations:

```cpp
    WebSliderParameterAttachment macro1Attachment { *processor.apvts.getParameter(ParamID::MACRO1), macro1Relay, nullptr };
    WebSliderParameterAttachment macro2Attachment { *processor.apvts.getParameter(ParamID::MACRO2), macro2Relay, nullptr };
    WebSliderParameterAttachment macro3Attachment { *processor.apvts.getParameter(ParamID::MACRO3), macro3Relay, nullptr };
    WebSliderParameterAttachment macro4Attachment { *processor.apvts.getParameter(ParamID::MACRO4), macro4Relay, nullptr };
```

- [ ] **Step 3: Register relays with WebView**

In the `withOptionsFrom` chain (in `buildWebViewOptions` or wherever the WebView's options are configured):

```cpp
        .withOptionsFrom(macro1Relay)
        .withOptionsFrom(macro2Relay)
        .withOptionsFrom(macro3Relay)
        .withOptionsFrom(macro4Relay)
```

- [ ] **Step 4: Add 5 new native function bindings**

In the `withNativeFunction` chain (alongside existing `engineGetFocus`, `spectrumGetViewMode`, etc.):

```cpp
        .withNativeFunction("modulationGetState", [&self]
            (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
        {
            // Build the response object containing both engines' modulators + routings
            // + per-engine eligible params for the picker.
            juce::DynamicObject::Ptr root = new juce::DynamicObject();

            auto buildEngine = [&](kaigen::phantom::ModulationEngine& eng, const juce::String& prefix) -> juce::var
            {
                juce::DynamicObject::Ptr engObj = new juce::DynamicObject();

                // Modulators (just macros for PR3b)
                juce::Array<juce::var> mods;
                // Each macro: { id, name }
                for (int i = 0; i < eng.getNumModulators(); ++i)
                {
                    // findModulator returns Modulator* by id; iterate via known ids since Modulator has no enumeration API
                    // (For PR3b's purposes we know macros are at known ids.)
                }
                // Simpler: hardcode macros per prefix
                if (prefix == "a_")
                {
                    for (const auto& mid : { "macro1", "macro2" })
                    {
                        auto* m = eng.findModulator(mid);
                        if (m != nullptr)
                        {
                            juce::DynamicObject::Ptr mObj = new juce::DynamicObject();
                            mObj->setProperty("id", mid);
                            mObj->setProperty("name", static_cast<kaigen::phantom::Macro*>(m)->getName());
                            mods.add(juce::var(mObj.get()));
                        }
                    }
                }
                else if (prefix == "b_")
                {
                    for (const auto& mid : { "macro3", "macro4" })
                    {
                        auto* m = eng.findModulator(mid);
                        if (m != nullptr)
                        {
                            juce::DynamicObject::Ptr mObj = new juce::DynamicObject();
                            mObj->setProperty("id", mid);
                            mObj->setProperty("name", static_cast<kaigen::phantom::Macro*>(m)->getName());
                            mods.add(juce::var(mObj.get()));
                        }
                    }
                }
                engObj->setProperty("modulators", mods);

                // Routings
                juce::Array<juce::var> routes;
                auto snapshot = eng.getRoutingsSnapshot();
                for (const auto& r : *snapshot)
                {
                    juce::DynamicObject::Ptr rObj = new juce::DynamicObject();
                    rObj->setProperty("source", r.sourceId);
                    rObj->setProperty("param",  r.paramId);
                    rObj->setProperty("depth",  r.depth);
                    rObj->setProperty("invert", r.polarityInverted);
                    routes.add(juce::var(rObj.get()));
                }
                engObj->setProperty("routings", routes);

                // Eligible params (for the "+ Add destination" picker)
                juce::Array<juce::var> eligible;
                for (const auto& leaf : kaigen::phantom::PresetMigration::getPerEngineLeaves())
                {
                    const juce::String pid = prefix + leaf;
                    if (auto* p = self.processor.apvts.getParameter(pid))
                    {
                        juce::DynamicObject::Ptr pObj = new juce::DynamicObject();
                        pObj->setProperty("id",   pid);
                        pObj->setProperty("name", p->getName(64));   // user-facing name truncated to 64
                        eligible.add(juce::var(pObj.get()));
                    }
                }
                engObj->setProperty("eligible", eligible);

                return juce::var(engObj.get());
            };

            root->setProperty("engineA", buildEngine(self.processor.getModulationEngineA(), "a_"));
            root->setProperty("engineB", buildEngine(self.processor.getModulationEngineB(), "b_"));

            complete(juce::var(root.get()));
        })
        .withNativeFunction("modulationAddRouting", [&self]
            (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
        {
            // args: { source: "macro1", param: "a_ghost", depth: 0.5 }
            if (args.size() < 1 || ! args[0].isObject()) { complete(juce::var(false)); return; }
            auto* obj = args[0].getDynamicObject();
            if (obj == nullptr) { complete(juce::var(false)); return; }

            kaigen::phantom::Routing r;
            r.sourceId = obj->getProperty("source").toString();
            r.paramId  = obj->getProperty("param").toString();
            r.depth    = (float) obj->getProperty("depth", 0.5);

            const bool isA = r.paramId.startsWith("a_");
            auto& eng = isA ? self.processor.getModulationEngineA()
                           : self.processor.getModulationEngineB();
            const bool ok = eng.addRouting(r);
            complete(juce::var(ok));
        })
        .withNativeFunction("modulationRemoveRouting", [&self]
            (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
        {
            if (args.size() < 1 || ! args[0].isObject()) { complete({}); return; }
            auto* obj = args[0].getDynamicObject();
            if (obj == nullptr) { complete({}); return; }
            const auto src = obj->getProperty("source").toString();
            const auto pid = obj->getProperty("param").toString();
            const bool isA = pid.startsWith("a_");
            auto& eng = isA ? self.processor.getModulationEngineA()
                           : self.processor.getModulationEngineB();
            eng.removeRouting(src, pid);
            complete({});
        })
        .withNativeFunction("modulationSetRoutingDepth", [&self]
            (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
        {
            if (args.size() < 1 || ! args[0].isObject()) { complete(juce::var(false)); return; }
            auto* obj = args[0].getDynamicObject();
            if (obj == nullptr) { complete(juce::var(false)); return; }
            const auto src = obj->getProperty("source").toString();
            const auto pid = obj->getProperty("param").toString();
            const float depth = (float) obj->getProperty("depth", 0.5);
            const bool isA = pid.startsWith("a_");
            auto& eng = isA ? self.processor.getModulationEngineA()
                           : self.processor.getModulationEngineB();
            const bool ok = eng.setRoutingDepth(src, pid, depth);
            complete(juce::var(ok));
        })
        .withNativeFunction("modulationSetMacroName", [&self]
            (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
        {
            if (args.size() < 1 || ! args[0].isObject()) { complete({}); return; }
            auto* obj = args[0].getDynamicObject();
            if (obj == nullptr) { complete({}); return; }
            const auto sourceId = obj->getProperty("source").toString();
            const auto name     = obj->getProperty("name").toString();

            // Find which engine owns this macro id
            const bool isA = (sourceId == "macro1" || sourceId == "macro2");
            auto& eng = isA ? self.processor.getModulationEngineA()
                           : self.processor.getModulationEngineB();
            auto* m = eng.findModulator(sourceId);
            if (auto* macro = dynamic_cast<kaigen::phantom::Macro*>(m))
                macro->setName(name);
            complete({});
        })
```

The capture style and signature should match the existing PR2/PR3a native bindings (`engineGetFocus` etc.).

The eligible-params helper uses `kaigen::phantom::PresetMigration::getPerEngineLeaves()` (introduced in PR1) which already lists every per-engine leaf — perfect for filtering.

You'll need `#include "Modulation/Macro.h"` (for `dynamic_cast` to work — make sure `Macro` is the dynamic type). Confirm `Macro.h` is in scope; if not, add it near other Source/ includes.

You'll also need `#include "PresetMigration.h"` if not already in scope.

- [ ] **Step 5: Build to verify**

```
cmake --build build --target KaigenPhantom_VST3 --config Debug
```

Expected: clean build. The bindings exist but aren't called yet (Tasks 5+ wire JS callers).

- [ ] **Step 6: Commit**

```bash
git add Source/PluginEditor.h Source/PluginEditor.cpp
git commit -m "$(cat <<'EOF'
feat(modulation): macro relays + routing CRUD native bindings

Adds 4 WebSliderRelays (macro1-4) so the in-plugin macro knobs can
read/write the APVTS macro params (preserves host-automation
semantics — these are still the same APVTS params PR3a registered).

5 new native bindings:
- modulationGetState(): returns both engines' { modulators (id+name),
  routings, eligible-params (id+display-name) } for the WebUI.
- modulationAddRouting({source, param, depth}): returns bool success.
- modulationRemoveRouting({source, param}).
- modulationSetRoutingDepth({source, param, depth}): returns bool.
- modulationSetMacroName({source, name}).

WebUI bottom panel + macro editor consume these in subsequent tasks.
EOF
)"
```

---

## Task 4: Bottom modulation panel HTML + CSS

**Files:**
- Modify: `Source/WebUI/index.html`
- Modify: `Source/WebUI/styles.css`

The 11-slot row: LFO 1, LFO 2, RANDOM, MACRO 1, MACRO 2, MORPH, MACRO 3, MACRO 4, RANDOM, LFO 3, LFO 4. PR3b only makes the macro slots interactive; the rest render as placeholders.

- [ ] **Step 1: Add the panel container to `index.html`**

Find a sensible insertion point in the editor body — near the bottom of the existing structure, after the existing `#mod-panel` (which is the small morph slider container). Add:

```html
    <!-- Bottom modulation panel (PR3b: macros only; LFO/Random slots are
         placeholders until PR4/5). Drawer slides up above this row when
         a slot is clicked. -->
    <div id="modulation-panel" class="modulation-panel" aria-label="Modulation panel">
      <div id="modulation-drawer" class="modulation-drawer" aria-hidden="true"></div>
      <div class="modulation-row" role="tablist">
        <!-- Engine A side -->
        <button class="mod-slot mod-slot-lfo" data-slot-type="lfo" data-slot-id="lfo1" disabled
                title="LFO 1 (PR4)" aria-disabled="true">
          <span class="mod-slot-dot"></span>
          <span class="mod-slot-name">LFO 1</span>
        </button>
        <button class="mod-slot mod-slot-lfo" data-slot-type="lfo" data-slot-id="lfo2" disabled
                title="LFO 2 (PR4)" aria-disabled="true">
          <span class="mod-slot-dot"></span>
          <span class="mod-slot-name">LFO 2</span>
        </button>
        <button class="mod-slot mod-slot-random" data-slot-type="random" data-slot-id="randomA" disabled
                title="Random A (PR5)" aria-disabled="true">
          <span class="mod-slot-dot"></span>
          <span class="mod-slot-name">RAND</span>
        </button>
        <button class="mod-slot mod-slot-macro" data-slot-type="macro" data-slot-id="macro1"
                title="Macro 1">
          <span class="mod-slot-dot"></span>
          <span class="mod-slot-name">MAC 1</span>
        </button>
        <button class="mod-slot mod-slot-macro" data-slot-type="macro" data-slot-id="macro2"
                title="Macro 2">
          <span class="mod-slot-dot"></span>
          <span class="mod-slot-name">MAC 2</span>
        </button>

        <!-- Center morph slot (placeholder; existing morph slider is the canonical control) -->
        <button class="mod-slot mod-slot-morph" data-slot-type="morph" data-slot-id="morph"
                title="Morph (use existing slider)" aria-disabled="true" disabled>
          <span class="mod-slot-dot"></span>
          <span class="mod-slot-name">MORPH</span>
        </button>

        <!-- Engine B side -->
        <button class="mod-slot mod-slot-macro" data-slot-type="macro" data-slot-id="macro3"
                title="Macro 3">
          <span class="mod-slot-dot"></span>
          <span class="mod-slot-name">MAC 3</span>
        </button>
        <button class="mod-slot mod-slot-macro" data-slot-type="macro" data-slot-id="macro4"
                title="Macro 4">
          <span class="mod-slot-dot"></span>
          <span class="mod-slot-name">MAC 4</span>
        </button>
        <button class="mod-slot mod-slot-random" data-slot-type="random" data-slot-id="randomB" disabled
                title="Random B (PR5)" aria-disabled="true">
          <span class="mod-slot-dot"></span>
          <span class="mod-slot-name">RAND</span>
        </button>
        <button class="mod-slot mod-slot-lfo" data-slot-type="lfo" data-slot-id="lfo3" disabled
                title="LFO 3 (PR4)" aria-disabled="true">
          <span class="mod-slot-dot"></span>
          <span class="mod-slot-name">LFO 3</span>
        </button>
        <button class="mod-slot mod-slot-lfo" data-slot-type="lfo" data-slot-id="lfo4" disabled
                title="LFO 4 (PR4)" aria-disabled="true">
          <span class="mod-slot-dot"></span>
          <span class="mod-slot-name">LFO 4</span>
        </button>
      </div>
    </div>
```

(Place this AFTER the existing `<div id="mod-panel">` (the morph slider container) — it lives at the bottom of the editor body. If `#mod-panel` is the last element in the editor body, just append after it.)

- [ ] **Step 2: Add styles to `styles.css`**

Append:

```css
/* ═══ BOTTOM MODULATION PANEL (PR3b) ═══ */
.modulation-panel {
  position: relative;
  background: rgba(14, 17, 22, 0.95);
  border-top: 1px solid rgba(255, 255, 255, 0.05);
}

.modulation-drawer {
  overflow: hidden;
  max-height: 0;
  transition: max-height 220ms ease-out;
  background: rgba(20, 24, 30, 0.98);
  border-bottom: 1px solid rgba(255, 255, 255, 0.06);
}
.modulation-drawer.is-open {
  max-height: 220px;
}

.modulation-row {
  display: grid;
  grid-template-columns: repeat(11, 1fr);
  gap: 6px;
  padding: 8px 12px;
}

.mod-slot {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 6px;
  padding: 10px 6px;
  background: rgba(31, 36, 44, 0.6);
  border: 1px solid rgba(255, 255, 255, 0.05);
  border-radius: 6px;
  cursor: pointer;
  user-select: none;
  transition: background 120ms, border-color 120ms;
}
.mod-slot[disabled] {
  cursor: not-allowed;
  opacity: 0.45;
}
.mod-slot:not([disabled]):hover {
  background: rgba(40, 46, 56, 0.7);
}
.mod-slot.is-active {
  background: rgba(40, 60, 84, 0.6);
  border-color: rgba(93, 211, 224, 0.55);
  box-shadow: 0 0 8px rgba(93, 211, 224, 0.3);
}

.mod-slot-dot {
  width: 36px; height: 36px;
  border-radius: 50%;
  background: radial-gradient(circle, #444 30%, #1f242c 70%);
  border: 2px solid #555;
  box-shadow: 0 0 0 transparent;
  transition: box-shadow 120ms;
}
.mod-slot-macro .mod-slot-dot {
  background: radial-gradient(circle, #5DD3E0 30%, #1f242c 70%);
  border: 2px solid #5DD3E0;
  box-shadow: 0 0 6px rgba(93, 211, 224, 0.5);
}
.mod-slot-lfo .mod-slot-dot {
  background: radial-gradient(circle, #4A90E2 30%, #1f242c 70%);
  border: 2px solid #4A90E2;
}
.mod-slot-random .mod-slot-dot {
  background: radial-gradient(circle, #6B5DC9 30%, #1f242c 70%);
  border: 2px solid #6B5DC9;
}
.mod-slot-morph .mod-slot-dot {
  background: radial-gradient(circle at 35% 35%, #fafaff 10%, #b8b8c4 50%, #4a4d54 80%);
  border: 2px solid #EFEFF2;
  box-shadow: 0 0 8px rgba(239, 239, 242, 0.5);
}

.mod-slot-name {
  font: 700 9px/1 'Space Grotesk', sans-serif;
  letter-spacing: 1px;
  color: #6a7a8a;
}
.mod-slot-macro .mod-slot-name { color: #5DD3E0; }
.mod-slot-lfo .mod-slot-name { color: #4A90E2; }
.mod-slot-random .mod-slot-name { color: #9990E0; }
.mod-slot-morph .mod-slot-name { color: #EFEFF2; }
```

- [ ] **Step 3: Build + verify the panel renders (manual)**

```
cmake --build build --target KaigenPhantom_VST3 --config Debug
```

Drop the plugin in a DAW. The bottom panel should be visible with 11 slots in a row. Macro slots are interactive (hover changes background). LFO/Random/Morph slots are dimmed and not clickable. No drawer mechanics yet — that's Task 5.

- [ ] **Step 4: Commit**

```bash
git add Source/WebUI/index.html Source/WebUI/styles.css
git commit -m "$(cat <<'EOF'
feat(webui): bottom modulation panel — 11-slot row

Symmetric row with the 11-slot layout from the design spec. Engine A
side: LFO 1, LFO 2, RAND, MAC 1, MAC 2. Center: MORPH. Engine B side:
MAC 3, MAC 4, RAND, LFO 3, LFO 4. Each slot has a colored dot + name
label using the cool-spectrum palette (LFO steel blue, MAC cyan,
RAND indigo, MORPH phantom-white).

PR3b: only macro slots are interactive. LFO and Random slots are
placeholders for PR4/5; the morph slot is dimmed because the
existing morph slider in #mod-panel is the canonical control.

Drawer mechanics + macro editor land in subsequent tasks.
EOF
)"
```

---

## Task 5: Drawer mechanics + modulation-panel.js

**Files:**
- Create: `Source/WebUI/modulation-panel.js`
- Modify: `Source/WebUI/index.html` (add `<script>` reference)
- Modify: `Source/WebUI/phantom.js` (or whatever bootstraps modules)

- [ ] **Step 1: Write `Source/WebUI/modulation-panel.js`**

```javascript
// Source/WebUI/modulation-panel.js
//
// Bottom modulation panel controller. Tracks which slot (if any) is
// currently expanded in the drawer. Click handlers on macro slots
// toggle the drawer + load the macro editor. Other slot types are
// inert until PR4 (LFO) and PR5 (Random) wire them.

(function () {
  'use strict';

  if (typeof window.Juce === 'undefined' || typeof window.Juce.getNativeFunction !== 'function') {
    document.addEventListener('DOMContentLoaded', init, { once: true });
    return;
  }
  init();

  function init() {
    const panel  = document.getElementById('modulation-panel');
    const drawer = document.getElementById('modulation-drawer');
    if (!panel || !drawer) return;

    let activeSlot = null;   // string id or null

    function closeDrawer() {
      drawer.classList.remove('is-open');
      drawer.setAttribute('aria-hidden', 'true');
      drawer.replaceChildren();
      if (activeSlot) {
        const prev = panel.querySelector(`.mod-slot[data-slot-id="${activeSlot}"]`);
        if (prev) prev.classList.remove('is-active');
      }
      activeSlot = null;
    }

    function openSlot(slotId, slotType) {
      // Only macros work in PR3b.
      if (slotType !== 'macro') return;

      // Toggle: clicking the active slot closes the drawer.
      if (activeSlot === slotId) {
        closeDrawer();
        return;
      }

      // Reassign active highlighting.
      if (activeSlot) {
        const prev = panel.querySelector(`.mod-slot[data-slot-id="${activeSlot}"]`);
        if (prev) prev.classList.remove('is-active');
      }
      const next = panel.querySelector(`.mod-slot[data-slot-id="${slotId}"]`);
      if (next) next.classList.add('is-active');
      activeSlot = slotId;

      // Render the macro editor into the drawer.
      drawer.replaceChildren();
      if (typeof window.kaigenRenderMacroEditor === 'function') {
        window.kaigenRenderMacroEditor(drawer, slotId);
      }
      drawer.classList.add('is-open');
      drawer.setAttribute('aria-hidden', 'false');
    }

    // Wire slot click handlers.
    panel.querySelectorAll('.mod-slot').forEach(btn => {
      if (btn.disabled) return;
      btn.addEventListener('click', () => {
        const id   = btn.getAttribute('data-slot-id');
        const type = btn.getAttribute('data-slot-type');
        openSlot(id, type);
      });
    });

    // Expose for macro editor's "close" button.
    window.kaigenCloseModulationDrawer = closeDrawer;
  }
})();
```

- [ ] **Step 2: Add `<script src="modulation-panel.js"></script>` to `index.html`**

Insert near other module `<script>` tags (e.g., right after `phantom.js`):

```html
    <script src="modulation-panel.js"></script>
```

- [ ] **Step 3: Build + verify drawer toggles (no editor content yet)**

```
cmake --build build --target KaigenPhantom_VST3 --config Debug
```

In DAW: click a macro slot → drawer slides open (empty for now since `kaigenRenderMacroEditor` doesn't exist yet — that's Task 6). Active slot highlights with cyan-blue glow. Click the same slot again → drawer closes. Click a different macro slot → drawer reshapes / highlights move.

- [ ] **Step 4: Commit**

```bash
git add Source/WebUI/modulation-panel.js Source/WebUI/index.html
git commit -m "$(cat <<'EOF'
feat(webui): modulation panel drawer mechanics

modulation-panel.js wires click handlers on the 11 slots. Only
macro slots open the drawer in PR3b (LFO/Random/Morph slots are
inert until PR4-6). Active slot highlights with cyan glow; clicking
the active slot toggles closed; clicking a different macro slot
reshapes the drawer instantly.

The drawer's content is rendered by window.kaigenRenderMacroEditor
(implemented in macro-editor.js in the next task). For PR3b's
intermediate commit, the drawer opens empty.
EOF
)"
```

---

## Task 6: Macro editor + param picker

**Files:**
- Create: `Source/WebUI/macro-editor.js`
- Modify: `Source/WebUI/index.html` (add `<script>`)
- Modify: `Source/WebUI/styles.css` (editor styling)

- [ ] **Step 1: Write `Source/WebUI/macro-editor.js`**

```javascript
// Source/WebUI/macro-editor.js
//
// Renders the macro editor inside the modulation drawer. Layout:
//   left column: big macro knob (APVTS-backed) + numeric value + name field
//   right column: destinations list (each with depth slider + remove)
//                 + "+ Add destination" picker
//
// State syncs from native modulationGetState; mutations go through
// modulationAddRouting / modulationRemoveRouting / modulationSetRoutingDepth /
// modulationSetMacroName. After each mutation, re-render from a fresh
// modulationGetState fetch.

(function () {
  'use strict';

  if (typeof window.Juce === 'undefined' || typeof window.Juce.getNativeFunction !== 'function') {
    document.addEventListener('DOMContentLoaded', init, { once: true });
    return;
  }
  init();

  function init() {
    const getState   = window.Juce.getNativeFunction('modulationGetState');
    const addRouting = window.Juce.getNativeFunction('modulationAddRouting');
    const rmRouting  = window.Juce.getNativeFunction('modulationRemoveRouting');
    const setDepth   = window.Juce.getNativeFunction('modulationSetRoutingDepth');
    const setName    = window.Juce.getNativeFunction('modulationSetMacroName');
    if (!getState || !addRouting || !rmRouting || !setDepth || !setName) {
      console.warn('[macro-editor] native bindings missing'); return;
    }

    // Determine which engine owns a macro id.
    function engineForMacro(macroId) {
      return (macroId === 'macro1' || macroId === 'macro2') ? 'engineA' : 'engineB';
    }

    async function render(host, macroId) {
      const state = await getState();
      const eng = state[engineForMacro(macroId)];
      const macro = eng.modulators.find(m => m.id === macroId);
      if (!macro) {
        host.textContent = '[modulator missing]';
        return;
      }
      const macroRoutings = (eng.routings || []).filter(r => r.source === macroId);

      host.replaceChildren();

      // ── Left: knob + name field ──
      const left = document.createElement('div');
      left.className = 'macro-editor-left';

      const knob = document.createElement('phantom-knob');
      knob.setAttribute('data-param', macroId);   // wired via getSliderState (PR3b: bare-name lookup since macros are global)
      knob.classList.add('macro-editor-knob');
      left.appendChild(knob);

      const nameInput = document.createElement('input');
      nameInput.type = 'text';
      nameInput.className = 'macro-editor-name';
      nameInput.value = macro.name || macroId;
      nameInput.placeholder = 'Name';
      nameInput.addEventListener('change', async () => {
        await setName({ source: macroId, name: nameInput.value });
        // No need to re-render; only the name changed locally.
      });
      left.appendChild(nameInput);

      host.appendChild(left);

      // ── Right: destinations list + add picker ──
      const right = document.createElement('div');
      right.className = 'macro-editor-right';

      const listHdr = document.createElement('div');
      listHdr.className = 'macro-editor-list-hdr';
      listHdr.textContent = 'DESTINATIONS';
      right.appendChild(listHdr);

      if (macroRoutings.length === 0) {
        const empty = document.createElement('div');
        empty.className = 'macro-editor-empty';
        empty.textContent = 'No destinations. Click + Add destination below.';
        right.appendChild(empty);
      } else {
        macroRoutings.forEach(r => {
          const eligibleEntry = (eng.eligible || []).find(e => e.id === r.param);
          const displayName = eligibleEntry ? eligibleEntry.name : r.param;

          const row = document.createElement('div');
          row.className = 'macro-editor-row';

          const name = document.createElement('span');
          name.className = 'macro-editor-row-name';
          name.textContent = displayName;
          row.appendChild(name);

          const slider = document.createElement('input');
          slider.type = 'range';
          slider.min = '-1';
          slider.max = '1';
          slider.step = '0.01';
          slider.value = String(r.depth);
          slider.className = 'macro-editor-row-depth';
          slider.addEventListener('input', async () => {
            await setDepth({ source: macroId, param: r.param, depth: parseFloat(slider.value) });
          });
          row.appendChild(slider);

          const valLabel = document.createElement('span');
          valLabel.className = 'macro-editor-row-value';
          valLabel.textContent = (r.depth > 0 ? '+' : '') + Math.round(r.depth * 100) + '%';
          row.appendChild(valLabel);
          slider.addEventListener('input', () => {
            const v = parseFloat(slider.value);
            valLabel.textContent = (v > 0 ? '+' : '') + Math.round(v * 100) + '%';
          });

          const removeBtn = document.createElement('button');
          removeBtn.className = 'macro-editor-row-remove';
          removeBtn.textContent = '×';
          removeBtn.title = 'Remove this destination';
          removeBtn.addEventListener('click', async () => {
            await rmRouting({ source: macroId, param: r.param });
            await render(host, macroId);
          });
          row.appendChild(removeBtn);

          right.appendChild(row);
        });
      }

      // ── Add picker ──
      const picker = document.createElement('div');
      picker.className = 'macro-editor-picker';
      const addBtn = document.createElement('button');
      addBtn.className = 'macro-editor-add';
      addBtn.textContent = '+ Add destination';
      picker.appendChild(addBtn);
      const select = document.createElement('select');
      select.className = 'macro-editor-picker-select';
      select.style.display = 'none';
      const placeholder = document.createElement('option');
      placeholder.value = '';
      placeholder.textContent = '-- choose param --';
      select.appendChild(placeholder);
      // Filter eligible: exclude params already routed from this macro.
      const usedParams = new Set(macroRoutings.map(r => r.param));
      (eng.eligible || []).forEach(e => {
        if (usedParams.has(e.id)) return;
        const opt = document.createElement('option');
        opt.value = e.id;
        opt.textContent = e.name;
        select.appendChild(opt);
      });
      picker.appendChild(select);

      addBtn.addEventListener('click', () => {
        addBtn.style.display = 'none';
        select.style.display = '';
        select.focus();
      });
      select.addEventListener('change', async () => {
        if (!select.value) return;
        await addRouting({ source: macroId, param: select.value, depth: 0.5 });
        await render(host, macroId);
      });

      right.appendChild(picker);

      // ── Close button ──
      const close = document.createElement('button');
      close.className = 'macro-editor-close';
      close.title = 'Close';
      close.textContent = '▼';
      close.addEventListener('click', () => {
        if (typeof window.kaigenCloseModulationDrawer === 'function')
          window.kaigenCloseModulationDrawer();
      });
      right.appendChild(close);

      host.appendChild(right);
    }

    // Expose for modulation-panel.js to call.
    window.kaigenRenderMacroEditor = render;
  }
})();
```

- [ ] **Step 2: Add `<script src="macro-editor.js"></script>` to `index.html`**

Insert right after `modulation-panel.js`'s script tag.

- [ ] **Step 3: Add macro-editor styling to `styles.css`**

Append:

```css
/* ═══ MACRO EDITOR (drawer content) ═══ */
.modulation-drawer {
  display: flex; /* parent flex; macro editor is grid */
}
.macro-editor-left {
  flex: 0 0 180px;
  display: flex; flex-direction: column; align-items: center;
  padding: 16px 12px;
  border-right: 1px solid rgba(255, 255, 255, 0.05);
  gap: 10px;
}
.macro-editor-knob {
  /* phantom-knob web component renders its own SVG; sized externally */
  width: 96px; height: 96px;
}
.macro-editor-name {
  width: 100%;
  background: rgba(20, 24, 30, 0.7);
  border: 1px solid rgba(93, 211, 224, 0.35);
  border-radius: 4px;
  color: #5DD3E0;
  font: 700 12px/1 'Space Grotesk', sans-serif;
  letter-spacing: 1px;
  padding: 6px 8px;
  text-align: center;
}
.macro-editor-right {
  flex: 1;
  position: relative;
  padding: 12px 16px;
  overflow-y: auto;
}
.macro-editor-list-hdr {
  font: 600 9px/1 'Space Grotesk', sans-serif;
  letter-spacing: 2px;
  color: #5d6a7a;
  margin-bottom: 8px;
}
.macro-editor-empty {
  font: 400 11px/1.4 'Space Grotesk', sans-serif;
  color: #5d6a7a;
  font-style: italic;
}
.macro-editor-row {
  display: grid;
  grid-template-columns: 1fr 120px 50px 26px;
  align-items: center;
  gap: 10px;
  padding: 6px 0;
  border-bottom: 1px solid rgba(255, 255, 255, 0.04);
}
.macro-editor-row-name {
  font: 600 11px/1 'Space Grotesk', sans-serif;
  color: #c8d8ea;
}
.macro-editor-row-depth {
  width: 100%;
}
.macro-editor-row-value {
  font: 600 10px/1 monospace;
  color: #5DD3E0;
  text-align: right;
}
.macro-editor-row-remove {
  background: rgba(40, 20, 20, 0.4);
  border: 1px solid rgba(180, 80, 80, 0.4);
  border-radius: 3px;
  color: rgba(220, 120, 120, 0.85);
  font: 700 12px/1 'Space Grotesk', sans-serif;
  cursor: pointer;
  height: 22px;
}
.macro-editor-row-remove:hover {
  background: rgba(70, 30, 30, 0.5);
}
.macro-editor-picker {
  margin-top: 12px;
  display: flex;
  align-items: center;
  gap: 8px;
}
.macro-editor-add {
  background: rgba(40, 60, 84, 0.5);
  border: 1px dashed rgba(93, 211, 224, 0.45);
  border-radius: 4px;
  color: #5DD3E0;
  font: 600 10px/1 'Space Grotesk', sans-serif;
  letter-spacing: 1px;
  padding: 6px 12px;
  cursor: pointer;
}
.macro-editor-add:hover {
  background: rgba(60, 90, 124, 0.5);
}
.macro-editor-picker-select {
  background: rgba(20, 24, 30, 0.9);
  color: #c8d8ea;
  border: 1px solid rgba(93, 211, 224, 0.35);
  border-radius: 4px;
  padding: 6px 8px;
  font: 400 11px/1 'Space Grotesk', sans-serif;
}
.macro-editor-close {
  position: absolute;
  top: 8px; right: 8px;
  background: transparent;
  border: 1px solid rgba(255, 255, 255, 0.15);
  color: rgba(255, 255, 255, 0.55);
  font: 600 10px/1 'Space Grotesk', sans-serif;
  padding: 3px 8px;
  border-radius: 3px;
  cursor: pointer;
}
.macro-editor-close:hover {
  border-color: rgba(255, 255, 255, 0.3);
  color: rgba(255, 255, 255, 0.85);
}
```

- [ ] **Step 4: Build + manual smoke**

```
cmake --build build --target KaigenPhantom_VST3 --config Debug
```

In DAW: click a macro slot. Drawer opens with the macro editor. Knob shows the current macro APVTS value. Name field is editable. Destinations list (initially empty for macros 2-4; macro1 shows the hardcoded a_ghost routing). "+ Add destination" reveals a dropdown of eligible per-engine params. Selecting one adds the routing at default +50% depth. Depth slider adjusts. Remove button (×) deletes the routing. Close button (▼) closes the drawer.

- [ ] **Step 5: Commit**

```bash
git add Source/WebUI/macro-editor.js Source/WebUI/index.html Source/WebUI/styles.css
git commit -m "$(cat <<'EOF'
feat(webui): macro editor with destinations list + param picker

macro-editor.js renders into the modulation drawer when a macro slot
is clicked. Layout: left = big macro knob (phantom-knob bound to the
macro APVTS param) + numeric value + editable name field. Right =
DESTINATIONS list with per-routing depth sliders (-100% to +100%) and
remove (×) buttons + "+ Add destination" picker (dropdown of eligible
per-engine params with display names).

Mutations go through the native bindings added in Task 3
(modulationAddRouting / modulationRemoveRouting /
modulationSetRoutingDepth / modulationSetMacroName). After each
mutation, the editor re-renders from a fresh modulationGetState.

PR3b's macro slots (1-4) all open this editor; LFO and Random slots
remain inert until PR4/5.
EOF
)"
```

---

## Task 7: Remove hardcoded proof-of-life routing + manual smoke

**Files:**
- Modify: `Source/PluginProcessor.cpp`

- [ ] **Step 1: Remove the hardcoded routing block**

In `Source/PluginProcessor.cpp`, find the comment "PR3a proof-of-life routing" and delete the entire block:

```cpp
    // PR3a proof-of-life routing — replaced by user-created routings via the
    // macro editor UI in PR3b. Verifies the framework end-to-end:
    // automating macro1 in the host should audibly affect engine A's ghost.
    {
        kaigen::phantom::Routing r;
        r.sourceId = "macro1";
        r.paramId  = ParamID::A_GHOST;
        r.depth    = 0.5f;
        modEngineA.addRouting(r);
    }
```

The macro modulator setup above it stays (4 Macro instances added to the engines). Only the proof-of-life routing's literal `addRouting` call is removed.

- [ ] **Step 2: Build Release VST3**

```
cmake --build build --target KaigenPhantom_VST3 --config Release
```

Expected: clean build, post-install copy to `%LOCALAPPDATA%/Programs/Common/VST3/`.

- [ ] **Step 3: Manual integration smoke**

In Live 12:
- [ ] Drop the plugin on a track. Bottom modulation panel renders with 11 slots.
- [ ] Click `MAC 1` slot → drawer opens above the row. Macro editor visible: big knob, name field reads "macro1" (default), no destinations.
- [ ] Drag the macro knob 0 → 1. (Sound shouldn't change since no routings exist; this just verifies the knob is functional.)
- [ ] Click "+ Add destination". Dropdown appears with all engine A params (Ghost, Phantom Threshold, etc.). Pick "A. Ghost".
- [ ] A new row appears: "A. Ghost", depth slider centered at +50%, "+50%" label, × button.
- [ ] Now drag macro knob — engine A's ghost effect responds. (Math: at macro=1, depth 0.5, ghost range 100, modulation = 50; ghost goes from base to base+50 clamped at 100.)
- [ ] Drag depth slider to -50% — direction flips.
- [ ] Click × to remove the routing. Row disappears. Macro knob no longer affects ghost.
- [ ] Edit the name field → "Motion". Press Tab/Enter to commit.
- [ ] Switch to MAC 2 (engine A's other macro). Drawer reshapes. No routings (default). Add one to "A. Drive" or whatever's available.
- [ ] Switch to MAC 3 (engine B). Drawer reshapes. The "+ Add destination" dropdown now shows engine B's params (B. Ghost, B. Phantom Threshold, etc.) — confirming per-engine scope.
- [ ] Save Live project, reopen. Routings + macro names should persist.
- [ ] Verify host automation of macro1 still works (route a Live macro to it in Configure mode — though see PR3a discussion, this requires clicking the macro's knob in the plugin UI to expose it).

- [ ] **Step 4: Commit**

```bash
git add Source/PluginProcessor.cpp
git commit -m "$(cat <<'EOF'
chore(modulation): remove hardcoded proof-of-life routing

PR3a's hardcoded routing (Macro 1 -> a_ghost +50%) was a placeholder
to verify the modulation framework end-to-end without UI. PR3b's
macro editor enables user-created routings, so the placeholder is
no longer needed.

Existing user-saved routings (in <ModulationConfig> plugin state)
continue to work — only the constructor's auto-add is removed.
First-launch state has no default routings now; users create them
via the macro editor.
EOF
)"
```

---

## Task 8: Final whole-PR review

**Files:** none modified.

Dispatch a final code-reviewer subagent over the entire PR3b diff for:
- Lock-free pattern correctness (atomic shared_ptr usage; memory ordering on the DualEngineHost pointers)
- Native binding error handling
- WebUI accessibility (keyboard navigation, ARIA roles)
- CSS / DOM hygiene
- Any test coverage gaps
- Cross-task contract drift

If the review finds issues, apply fixes; otherwise proceed to push + PR + merge.

---

## End-of-PR Checklist

- [ ] All ~100 unit tests pass.
- [ ] VST3 builds cleanly Debug + Release.
- [ ] Bottom modulation panel renders with 11 slots; only macros are interactive.
- [ ] Clicking a macro slot opens the drawer with the macro editor.
- [ ] User can: edit macro name, drag macro knob, add/remove routings via picker, adjust per-routing depth.
- [ ] Per-engine scope enforced: macro 1+2 only show engine A's params in picker; macro 3+4 only show engine B's.
- [ ] State persists across DAW save/restore.
- [ ] Hardcoded proof-of-life routing is removed.
- [ ] No regression in PR1/PR2/PR3a/spectrum features.

---

## Self-Review

**Spec coverage:**
- Bottom modulation panel + 11-slot row ✓ (Task 4)
- Drawer expansion ✓ (Task 5)
- Macro editor: big knob, name, destinations list, "+ Add" picker ✓ (Task 6)
- Per-engine scope enforcement (macro 1+2 → a_*, 3+4 → b_*) ✓ (Task 3 native binding filters; Task 6 UI consumes)
- Lock-free runtime mutation ✓ (Task 1)
- Atomic ModulationEngine pointers ✓ (Task 2)
- Persistence stays plugin-state ✓ (no changes from PR3a)
- Hardcoded routing removed ✓ (Task 7)

**Placeholder scan:** none. Every step has actual code.

**Type / name consistency:**
- `RoutingsList` / `RoutingsSnapshot` aliases used consistently in `ModulationEngine.{h,cpp}`.
- Native binding names (`modulationGetState` / `modulationAddRouting` / etc.) match between Task 3 (registration) and Task 6 (consumption).
- HTML data attributes (`data-slot-id`, `data-slot-type`) match between Task 4 (markup) and Task 5 (handlers).
- CSS class names (`.mod-slot`, `.is-active`, `.macro-editor-*`) match between Task 4/5/6.

**Deferred items (PR3c):**
- Drag-to-assign UX (replaces the "+ Add destination" picker with a more direct gesture).
- Pigments-style colored ring on knobs that have routings (visual feedback).
- Hover tooltip listing routed sources on knobs.
- Matrix popover (alternate quick-access to per-knob routings).
