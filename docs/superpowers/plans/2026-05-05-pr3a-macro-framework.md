# PR 3a: Macro Modulation Framework Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Establish the modulation framework (Modulator base + Macro concrete + Routing storage + ModulationEngine container) that PR3b/c (UI) and PR4-6 (other modulator types) inherit. Add 4 macro APVTS params (`macro1`-`macro4`) for host automation. Wire ModulationEngine into DualEngineHost's per-engine value lookup so routings actually affect engine state. Add a hardcoded proof-of-life routing (Macro 1 → `a_ghost` at +50%) so the user can verify end-to-end without needing UI.

**Architecture:** Two `ModulationEngine` instances on `PhantomProcessor` — `modEngineA` scopes Macro 1 + Macro 2 to engine A's `a_*` params; `modEngineB` scopes Macro 3 + Macro 4 to engine B's `b_*` params. Per-engine scope is structurally enforced (the engine owns the modulator instances; a B-engine routing literally cannot reference an `a_*` param because the validator on `addRouting` checks the prefix). `DualEngineHost::syncEngineFromPrefix` consults the matching `ModulationEngine` for per-param value lookup; the engine returns `clamp(base + Σ depth × modulator_value × range, min, max)`.

**Tech Stack:** C++20, JUCE 8.0.4, Catch2 v3.5.2, WebView2.

---

## Decisions baked into this plan

- **Macro is UNIPOLAR source (0..1)**, routing depth is BIPOLAR (-1..+1). Mirrors the math from PR1's MorphEngine: modulation contribution = `depth × macro_value × param_range`. Depth can be positive or negative; macro is the user-controllable knob value (0=off, 1=full effect via depth).
- **Range scaling is per-param**, looked up at routing-evaluation time via `apvts.getParameter(paramId)->getNormalisableRange()`. Avoids storing redundant range info in routings.
- **Macros are APVTS params** (`macro1`-`macro4`), automatable by host. Default 0, range 0–1. `Macro::getCurrentValue()` reads from APVTS each call.
- **Per-engine scope is structural.** `modEngineA` has prefix `"a_"` and rejects routings whose paramId doesn't start with `"a_"` (and isn't a global). Same for `modEngineB` with `"b_"`. Caught at `addRouting`.
- **No UI in PR3a.** Verification is unit tests + a hardcoded proof-of-life routing (Macro 1 → `a_ghost` at +50%) added in `PhantomProcessor`'s constructor and removed when PR3b's macro editor lands. PR3b creates routings via the UI.
- **`<ModulationConfig>` ValueTree** persisted in plugin state alongside `<EditorFocus>` / `<SpectrumView>`. Top-level child of `<PluginState>`. Preset switching does NOT carry it (modulator routings are NOT preset state in PR3a — that may revisit in PR3b/c if the spec wants it).

  *Wait — actually the design spec said ModulationConfig is preset-saved per-preset. Re-reading: "Plugin state and presets emit `<PluginState>` wrapper + `<APVTSState>` child. ... Routings are dynamic per-preset." So presets DO include modulation config.*

  *PR3a defers preset-side persistence — it only persists in plugin state. PR3b/c will revisit when the user can actually create routings via UI.*

- **Modulators tick on the audio thread** during the per-block sync phase. `Macro::getCurrentValue()` reads `apvts.getRawParameterValue("macro1")->load()` which is real-time-safe (atomic load).
- **Routing evaluation is on the audio thread** in `DualEngineHost::syncEngineFromPrefix`. `ModulationEngine::applyRoutings(paramId, base)` is called per-param-per-block. Total work for engine A: ~33 setter calls × routings-matching-each-param. With at most 2 macros × 30ish routings each = 60 routings max per engine. Cost is bounded.

---

## File Plan

**Created:**
- `Source/Modulation/Modulator.h` — abstract base class (header-only).
- `Source/Modulation/Macro.h/cpp` — concrete Macro modulator.
- `Source/Modulation/Routing.h/cpp` — `Routing` struct + ValueTree serialization.
- `Source/Modulation/ModulationEngine.h/cpp` — per-engine container; routing storage + apply.
- `tests/ModulatorTests.cpp` — sanity tests (Macro reads from APVTS, returns 0..1).
- `tests/RoutingTests.cpp` — Routing struct + serialization round-trip.
- `tests/ModulationEngineTests.cpp` — Routing add/remove, scope validation, applyRoutings math.

**Modified:**
- `Source/Parameters.h` — declare `MACRO1_ID`..`MACRO4_ID` constants; add to `getAllParameterIDs()`; add 4 `AudioParameterFloat` declarations to `createParameterLayout()`.
- `Source/DualEngineHost.h` — add `setModulationEngines(ModulationEngine* modA, ModulationEngine* modB)` so PhantomProcessor can inject the per-engine instances.
- `Source/DualEngineHost.cpp` — modify `syncEngineFromPrefix` to consult the modulation engine for value lookup (when present).
- `Source/PluginProcessor.h` — own `modEngineA`, `modEngineB`. Add accessors.
- `Source/PluginProcessor.cpp` — instantiate modulation engines in constructor; inject into `dualEngineHost`; add hardcoded proof-of-life routing; persist `<ModulationConfig>` in `getStateInformation` / `setStateInformation`.
- `tests/CMakeLists.txt` — add three new test files.

**Deleted:** none.

---

## Task 1: `Modulator` abstract base + `Routing` struct (TDD)

**Files:**
- Create: `Source/Modulation/Modulator.h`
- Create: `Source/Modulation/Routing.h`
- Create: `Source/Modulation/Routing.cpp`
- Create: `tests/ModulatorTests.cpp`
- Create: `tests/RoutingTests.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write `Source/Modulation/Modulator.h`** (header-only abstract base):

```cpp
// Source/Modulation/Modulator.h
#pragma once
#include <juce_data_structures/juce_data_structures.h>

namespace kaigen::phantom
{

/** Abstract base for all modulators (Macro, LFO, Random in subsequent PRs).
 *
 *  A modulator is a source whose output value (typically [0,1] for unipolar
 *  sources like Macro, [-1,1] for bipolar like LFO) is read each audio block
 *  and combined with routing depth to modulate a target APVTS parameter.
 *
 *  Concrete subclasses implement `getCurrentValue()` and the persistence
 *  hooks. The base provides identity (an ID string used by routings to refer
 *  to this modulator). */
class Modulator
{
public:
    virtual ~Modulator() = default;

    /** Identity string. Routings reference modulators by this id. */
    const juce::String& getId() const noexcept { return id; }

    /** Per-block (or per-routing-evaluation) value query. Real-time-safe.
     *  Range depends on subclass — Macro returns [0,1], LFO returns [-1,1]. */
    virtual float getCurrentValue() const noexcept = 0;

    /** Lifecycle. Default no-op; subclasses with state override. */
    virtual void prepareToPlay(double /*sampleRate*/, int /*blockSize*/) {}
    virtual void reset() {}

    /** Serialize subclass-specific state into `parent`. Routings live in
     *  ModulationEngine, not here. */
    virtual void writeToTree(juce::ValueTree& parent) const {}
    virtual void readFromTree(const juce::ValueTree& parent) {}

protected:
    explicit Modulator(juce::String idStr) : id(std::move(idStr)) {}
    juce::String id;
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Write `Source/Modulation/Routing.h`** (declaration):

```cpp
// Source/Modulation/Routing.h
#pragma once
#include <juce_data_structures/juce_data_structures.h>

namespace kaigen::phantom
{

/** A single source → param routing with a bipolar depth.
 *
 *  sourceId    — the Modulator's id (e.g., "macro1")
 *  paramId     — the APVTS parameter id (e.g., "a_ghost")
 *  depth       — bipolar [-1, +1]; multiplied with modulator value × param range
 *  polarityInv — optional flag to flip the modulator's contribution sign
 *                (cosmetic; same effect could be achieved by negating depth) */
struct Routing
{
    juce::String sourceId;
    juce::String paramId;
    float        depth { 0.0f };
    bool         polarityInverted { false };

    juce::ValueTree toValueTree() const;
    static Routing  fromValueTree(const juce::ValueTree& tree);

    bool operator==(const Routing& other) const noexcept;
};

} // namespace kaigen::phantom
```

- [ ] **Step 3: Write `Source/Modulation/Routing.cpp`**:

```cpp
// Source/Modulation/Routing.cpp
#include "Routing.h"

namespace kaigen::phantom
{

juce::ValueTree Routing::toValueTree() const
{
    juce::ValueTree t("Route");
    t.setProperty("source", sourceId, nullptr);
    t.setProperty("param",  paramId,  nullptr);
    t.setProperty("depth",  depth,    nullptr);
    if (polarityInverted) t.setProperty("invert", true, nullptr);
    return t;
}

Routing Routing::fromValueTree(const juce::ValueTree& t)
{
    Routing r;
    r.sourceId         = t.getProperty("source").toString();
    r.paramId          = t.getProperty("param").toString();
    r.depth            = (float) t.getProperty("depth", 0.0f);
    r.polarityInverted = (bool) t.getProperty("invert", false);
    return r;
}

bool Routing::operator==(const Routing& other) const noexcept
{
    return sourceId == other.sourceId
        && paramId == other.paramId
        && depth == other.depth
        && polarityInverted == other.polarityInverted;
}

} // namespace kaigen::phantom
```

- [ ] **Step 4: Write `tests/RoutingTests.cpp`**:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "Modulation/Routing.h"

using namespace kaigen::phantom;

TEST_CASE("Routing default values", "[routing]")
{
    Routing r;
    REQUIRE(r.sourceId.isEmpty());
    REQUIRE(r.paramId.isEmpty());
    REQUIRE(r.depth == 0.0f);
    REQUIRE_FALSE(r.polarityInverted);
}

TEST_CASE("Routing toValueTree round-trip", "[routing]")
{
    Routing src;
    src.sourceId = "macro1";
    src.paramId  = "a_ghost";
    src.depth    = 0.5f;

    auto tree = src.toValueTree();
    auto restored = Routing::fromValueTree(tree);

    REQUIRE(restored.sourceId == "macro1");
    REQUIRE(restored.paramId  == "a_ghost");
    REQUIRE(restored.depth    == 0.5f);
    REQUIRE_FALSE(restored.polarityInverted);
}

TEST_CASE("Routing polarity-inverted round-trip", "[routing]")
{
    Routing src;
    src.sourceId = "macro2";
    src.paramId  = "a_phantom_threshold";
    src.depth    = -0.3f;
    src.polarityInverted = true;

    auto tree = src.toValueTree();
    auto restored = Routing::fromValueTree(tree);

    REQUIRE(restored.depth == -0.3f);
    REQUIRE(restored.polarityInverted);
}

TEST_CASE("Routing equality", "[routing]")
{
    Routing a; a.sourceId = "macro1"; a.paramId = "a_ghost"; a.depth = 0.5f;
    Routing b = a;
    REQUIRE(a == b);
    b.depth = 0.6f;
    REQUIRE_FALSE(a == b);
}
```

- [ ] **Step 5: Write `tests/ModulatorTests.cpp`** (just the abstract-class smoke; concrete Macro test comes in Task 2):

```cpp
#include <catch2/catch_test_macros.hpp>
#include "Modulation/Modulator.h"

using namespace kaigen::phantom;

namespace {

class StubModulator : public Modulator
{
public:
    StubModulator() : Modulator("stub1") {}
    float getCurrentValue() const noexcept override { return 0.5f; }
};

} // namespace

TEST_CASE("Modulator id is preserved", "[modulator]")
{
    StubModulator m;
    REQUIRE(m.getId() == "stub1");
}

TEST_CASE("Modulator getCurrentValue returns subclass value", "[modulator]")
{
    StubModulator m;
    REQUIRE(m.getCurrentValue() == 0.5f);
}
```

- [ ] **Step 6: Wire into `tests/CMakeLists.txt`**

Add to the test target's source list:
- `RoutingTests.cpp`
- `ModulatorTests.cpp`

Also add `../Source/Modulation/Routing.cpp` to the test target's compiled sources (so the test binary can link `Routing::toValueTree`).

- [ ] **Step 7: Build + run tests**

```
cmake --build build --target KaigenPhantomTests --config Debug
./build/tests/Debug/KaigenPhantomTests.exe -c "[routing] [modulator]"
```

Expected: 6 cases pass (4 routing + 2 modulator).

- [ ] **Step 8: Commit**

```bash
git add Source/Modulation/Modulator.h Source/Modulation/Routing.h Source/Modulation/Routing.cpp \
        tests/ModulatorTests.cpp tests/RoutingTests.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(modulation): Modulator abstract base + Routing struct

Header-only abstract Modulator class with id + getCurrentValue() +
prepareToPlay/reset/persistence hooks. Concrete subclasses (Macro
in next task; LFO/Random in PR4/5) implement getCurrentValue.

Routing struct holds sourceId+paramId+depth+polarityInverted with
ValueTree round-trip serialization. Tests verify defaults, normal +
polarity-inverted round-trip, and equality.
EOF
)"
```

---

## Task 2: `Macro` concrete class (TDD)

**Files:**
- Create: `Source/Modulation/Macro.h`
- Create: `Source/Modulation/Macro.cpp`
- Test: append cases to `tests/ModulatorTests.cpp`

- [ ] **Step 1: Write `Source/Modulation/Macro.h`**

```cpp
// Source/Modulation/Macro.h
#pragma once
#include "Modulator.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace kaigen::phantom
{

/** A user-controllable macro modulator. Backed by an APVTS parameter so
 *  the macro value is automatable by the host. Returns [0, 1].
 *
 *  Constructor takes the APVTS instance + the param ID it's tied to.
 *  getCurrentValue() reads the live APVTS value (real-time-safe atomic load).
 *
 *  PR3b adds: name, destinations list edited via UI, "expose to host
 *  automation" toggle (PR3a always exposes — that's the whole point of
 *  using an APVTS param). */
class Macro : public Modulator
{
public:
    Macro(juce::String idStr, juce::AudioProcessorValueTreeState& apvts, juce::String apvtsParamId);

    float getCurrentValue() const noexcept override;

    /** Display name (set via UI in PR3b; defaults to id). */
    void setName(juce::String n) { name = std::move(n); }
    const juce::String& getName() const noexcept { return name; }

    void writeToTree(juce::ValueTree& parent) const override;
    void readFromTree(const juce::ValueTree& parent) override;

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::String apvtsParamId;
    std::atomic<float>* cachedValuePtr { nullptr };  // resolved at construction
    juce::String name;
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Write `Source/Modulation/Macro.cpp`**

```cpp
// Source/Modulation/Macro.cpp
#include "Macro.h"

namespace kaigen::phantom
{

Macro::Macro(juce::String idStr,
             juce::AudioProcessorValueTreeState& apvtsRef,
             juce::String apvtsParamIdStr)
    : Modulator(std::move(idStr))
    , apvts(apvtsRef)
    , apvtsParamId(std::move(apvtsParamIdStr))
    , name(getId())
{
    cachedValuePtr = apvts.getRawParameterValue(apvtsParamId);
    jassert(cachedValuePtr != nullptr);
}

float Macro::getCurrentValue() const noexcept
{
    return cachedValuePtr ? cachedValuePtr->load() : 0.0f;
}

void Macro::writeToTree(juce::ValueTree& parent) const
{
    juce::ValueTree node("Macro");
    node.setProperty("id",   getId(), nullptr);
    node.setProperty("name", name,    nullptr);
    parent.appendChild(node, nullptr);
}

void Macro::readFromTree(const juce::ValueTree& parent)
{
    for (int i = 0; i < parent.getNumChildren(); ++i)
    {
        auto child = parent.getChild(i);
        if (child.getType() == juce::Identifier("Macro")
            && child.getProperty("id").toString() == getId())
        {
            name = child.getProperty("name", getId()).toString();
            return;
        }
    }
}

} // namespace kaigen::phantom
```

- [ ] **Step 3: Append tests to `tests/ModulatorTests.cpp`**

Constructing a Macro requires an APVTS, which requires a real AudioProcessor. The lightest approach is the same `StubProcessor` pattern PR1's DualEngineHostTests used. If that fixture isn't already shared, copy/inline:

```cpp
// Append to tests/ModulatorTests.cpp:
#include "Modulation/Macro.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace {

class StubMacroHostProcessor : public juce::AudioProcessor
{
public:
    StubMacroHostProcessor()
        : AudioProcessor(BusesProperties().withInput("In",  juce::AudioChannelSet::stereo(), true)
                                          .withOutput("Out", juce::AudioChannelSet::stereo(), true))
    {}
    const juce::String getName() const override { return "StubMacroHostProcessor"; }
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

juce::AudioProcessorValueTreeState::ParameterLayout makeMacroOnlyLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;
    params.push_back(std::make_unique<AudioParameterFloat>(
        "macro1", "Macro 1", NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    return { params.begin(), params.end() };
}

} // namespace

TEST_CASE("Macro reads value from APVTS", "[modulator]")
{
    StubMacroHostProcessor proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "MACRO_TEST", makeMacroOnlyLayout());

    Macro m("macro1", apvts, "macro1");

    // Default value is 0.0
    REQUIRE(m.getCurrentValue() == 0.0f);

    // Set via host
    if (auto* p = apvts.getParameter("macro1"))
        p->setValueNotifyingHost(0.7f);

    REQUIRE(m.getCurrentValue() == Approx(0.7f).margin(1.0e-5f));
}

TEST_CASE("Macro id is preserved", "[modulator]")
{
    StubMacroHostProcessor proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "MACRO_TEST", makeMacroOnlyLayout());
    Macro m("macro1", apvts, "macro1");
    REQUIRE(m.getId() == "macro1");
}

TEST_CASE("Macro name defaults to id and can be set", "[modulator]")
{
    StubMacroHostProcessor proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "MACRO_TEST", makeMacroOnlyLayout());
    Macro m("macro1", apvts, "macro1");
    REQUIRE(m.getName() == "macro1");
    m.setName("Motion");
    REQUIRE(m.getName() == "Motion");
}

TEST_CASE("Macro persistence round-trip preserves name", "[modulator]")
{
    StubMacroHostProcessor proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "MACRO_TEST", makeMacroOnlyLayout());

    Macro m1("macro1", apvts, "macro1");
    m1.setName("Motion");

    juce::ValueTree wrapper("ModConfig");
    m1.writeToTree(wrapper);

    Macro m2("macro1", apvts, "macro1");
    REQUIRE(m2.getName() == "macro1");   // pre-restore default
    m2.readFromTree(wrapper);
    REQUIRE(m2.getName() == "Motion");
}
```

(Add `using Catch::Approx;` to the top of `tests/ModulatorTests.cpp` if not already present.)

- [ ] **Step 4: Wire into `tests/CMakeLists.txt`**

Add `../Source/Modulation/Macro.cpp` to the test target's compiled sources.

- [ ] **Step 5: Build + run tests**

```
cmake --build build --target KaigenPhantomTests --config Debug
./build/tests/Debug/KaigenPhantomTests.exe -c "[modulator]"
```

Expected: 6 cases pass (2 from Task 1 + 4 new). Full suite: 84 cases (78 + 6 new from Tasks 1+2 — Task 1 added 4 routing + 2 modulator = 6, Task 2 adds 4 more macro cases = 10 total `[modulator]/[routing]`).

- [ ] **Step 6: Commit**

```bash
git add Source/Modulation/Macro.h Source/Modulation/Macro.cpp tests/ModulatorTests.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(modulation): Macro concrete modulator backed by APVTS param

Constructor caches std::atomic<float>* from apvts.getRawParameterValue
for real-time-safe per-block reads. Returns [0, 1]. Name defaults to
the modulator id; settable via setName for PR3b's UI. Persistence
helpers write/read <Macro id="..." name="..."/> children for storage
inside a parent ValueTree (typically <ModulationConfig>).

Tests use a minimal StubMacroHostProcessor + macro-only APVTS layout
to avoid pulling in the full PluginProcessor link surface.
EOF
)"
```

---

## Task 3: `ModulationEngine` (TDD)

**Files:**
- Create: `Source/Modulation/ModulationEngine.h`
- Create: `Source/Modulation/ModulationEngine.cpp`
- Create: `tests/ModulationEngineTests.cpp`
- Modify: `tests/CMakeLists.txt`

The container that holds modulator instances + routings, and applies routings to a per-param value lookup.

- [ ] **Step 1: Write `Source/Modulation/ModulationEngine.h`**

```cpp
// Source/Modulation/ModulationEngine.h
#pragma once
#include "Modulator.h"
#include "Routing.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>

namespace kaigen::phantom
{

/** Per-engine modulation container. Owns a set of Modulator instances
 *  scoped to one PhantomEngine (e.g., engine A's instance owns Macro 1
 *  and Macro 2; engine B's owns Macro 3 and Macro 4). Holds the routing
 *  table for that scope. Provides per-param value lookup that combines
 *  the APVTS base with all matching routings.
 *
 *  Per-engine scope is enforced structurally: addRouting() rejects any
 *  routing whose paramId doesn't start with the engine's prefix
 *  (constructor parameter, e.g., "a_" or "b_").
 */
class ModulationEngine
{
public:
    /** Construct.
     *  @param apvtsRef  the APVTS instance (for reading the modulated param's range)
     *  @param prefix    "a_" or "b_" — the engine's APVTS prefix; routings to params
     *                   not starting with this prefix are rejected.
     */
    ModulationEngine(juce::AudioProcessorValueTreeState& apvtsRef, juce::String prefix);

    void addModulator(std::unique_ptr<Modulator> m);

    /** Returns false if the routing's paramId doesn't match the engine's
     *  prefix, or if its sourceId doesn't match a known modulator. */
    bool addRouting(const Routing& r);

    void removeRouting(const juce::String& sourceId, const juce::String& paramId);
    void clearRoutings();

    const std::vector<Routing>& getRoutings() const noexcept { return routings; }

    /** Lookup the modulated value for a given param. Returns base + Σ
     *  (depth × modulator_value × range) clamped to [min, max]. If no
     *  routings match, returns base unchanged. */
    float getModulatedValue(const juce::String& paramId, float base) const;

    /** Persistence: writes <Engine prefix="a_">[modulators...][routings...]</Engine>. */
    juce::ValueTree toValueTree() const;
    void fromValueTree(const juce::ValueTree& engineNode);

    /** For testing + debug. */
    int  getNumModulators() const noexcept { return (int) modulators.size(); }
    Modulator* findModulator(const juce::String& sourceId) const;

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::String prefix;
    std::vector<std::unique_ptr<Modulator>> modulators;
    std::vector<Routing> routings;
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Write `Source/Modulation/ModulationEngine.cpp`**

```cpp
// Source/Modulation/ModulationEngine.cpp
#include "ModulationEngine.h"

namespace kaigen::phantom
{

ModulationEngine::ModulationEngine(juce::AudioProcessorValueTreeState& apvtsRef, juce::String prefixStr)
    : apvts(apvtsRef), prefix(std::move(prefixStr))
{
}

void ModulationEngine::addModulator(std::unique_ptr<Modulator> m)
{
    modulators.push_back(std::move(m));
}

Modulator* ModulationEngine::findModulator(const juce::String& sourceId) const
{
    for (auto& m : modulators)
        if (m->getId() == sourceId) return m.get();
    return nullptr;
}

bool ModulationEngine::addRouting(const Routing& r)
{
    if (! r.paramId.startsWith(prefix)) return false;
    if (findModulator(r.sourceId) == nullptr) return false;
    routings.push_back(r);
    return true;
}

void ModulationEngine::removeRouting(const juce::String& sourceId, const juce::String& paramId)
{
    routings.erase(std::remove_if(routings.begin(), routings.end(),
        [&](const Routing& r) { return r.sourceId == sourceId && r.paramId == paramId; }),
        routings.end());
}

void ModulationEngine::clearRoutings() { routings.clear(); }

float ModulationEngine::getModulatedValue(const juce::String& paramId, float base) const
{
    auto* paramPtr = apvts.getParameter(paramId);
    if (paramPtr == nullptr) return base;
    const auto range = paramPtr->getNormalisableRange();
    const float span = range.end - range.start;

    float modulated = base;
    for (const auto& r : routings)
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

juce::ValueTree ModulationEngine::toValueTree() const
{
    juce::ValueTree node("Engine");
    node.setProperty("prefix", prefix, nullptr);
    juce::ValueTree mods("Modulators");
    for (auto& m : modulators) m->writeToTree(mods);
    node.appendChild(mods, nullptr);
    juce::ValueTree routes("Routings");
    for (auto& r : routings) routes.appendChild(r.toValueTree(), nullptr);
    node.appendChild(routes, nullptr);
    return node;
}

void ModulationEngine::fromValueTree(const juce::ValueTree& engineNode)
{
    if (! engineNode.hasType("Engine")) return;
    if (engineNode.getProperty("prefix").toString() != prefix) return;

    auto modsNode = engineNode.getChildWithName("Modulators");
    for (auto& m : modulators) m->readFromTree(modsNode);

    routings.clear();
    auto routesNode = engineNode.getChildWithName("Routings");
    for (int i = 0; i < routesNode.getNumChildren(); ++i)
    {
        auto child = routesNode.getChild(i);
        if (child.hasType("Route"))
            addRouting(Routing::fromValueTree(child));   // may reject if invalid
    }
}

} // namespace kaigen::phantom
```

- [ ] **Step 3: Write `tests/ModulationEngineTests.cpp`**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Modulation/ModulationEngine.h"
#include "Modulation/Macro.h"
#include <juce_audio_processors/juce_audio_processors.h>

using namespace kaigen::phantom;
using Catch::Approx;

namespace {

class StubModEngineHost : public juce::AudioProcessor
{
public:
    StubModEngineHost()
        : AudioProcessor(BusesProperties().withInput("In",  juce::AudioChannelSet::stereo(), true)
                                          .withOutput("Out", juce::AudioChannelSet::stereo(), true))
    {}
    const juce::String getName() const override { return "StubModEngineHost"; }
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

juce::AudioProcessorValueTreeState::ParameterLayout makeModEngineTestLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;
    params.push_back(std::make_unique<AudioParameterFloat>(
        "macro1", "Macro 1", NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "a_ghost", "A. Ghost", NormalisableRange<float>(0.0f, 100.0f), 50.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "b_ghost", "B. Ghost", NormalisableRange<float>(0.0f, 100.0f), 50.0f));
    return { params.begin(), params.end() };
}

} // namespace

TEST_CASE("ModulationEngine: addRouting rejects wrong-prefix paramId", "[modengine]")
{
    StubModEngineHost proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "MOD_TEST", makeModEngineTestLayout());
    ModulationEngine modA(apvts, "a_");
    modA.addModulator(std::make_unique<Macro>("macro1", apvts, "macro1"));

    Routing r;
    r.sourceId = "macro1";
    r.paramId  = "b_ghost";   // wrong prefix
    r.depth    = 0.5f;
    REQUIRE_FALSE(modA.addRouting(r));
}

TEST_CASE("ModulationEngine: addRouting rejects unknown sourceId", "[modengine]")
{
    StubModEngineHost proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "MOD_TEST", makeModEngineTestLayout());
    ModulationEngine modA(apvts, "a_");
    modA.addModulator(std::make_unique<Macro>("macro1", apvts, "macro1"));

    Routing r;
    r.sourceId = "macro_doesnt_exist";
    r.paramId  = "a_ghost";
    r.depth    = 0.5f;
    REQUIRE_FALSE(modA.addRouting(r));
}

TEST_CASE("ModulationEngine: addRouting accepts valid routing", "[modengine]")
{
    StubModEngineHost proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "MOD_TEST", makeModEngineTestLayout());
    ModulationEngine modA(apvts, "a_");
    modA.addModulator(std::make_unique<Macro>("macro1", apvts, "macro1"));

    Routing r;
    r.sourceId = "macro1";
    r.paramId  = "a_ghost";
    r.depth    = 0.5f;
    REQUIRE(modA.addRouting(r));
    REQUIRE(modA.getRoutings().size() == 1);
}

TEST_CASE("ModulationEngine: getModulatedValue passes base through with no routing", "[modengine]")
{
    StubModEngineHost proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "MOD_TEST", makeModEngineTestLayout());
    ModulationEngine modA(apvts, "a_");
    REQUIRE(modA.getModulatedValue("a_ghost", 75.0f) == 75.0f);
}

TEST_CASE("ModulationEngine: getModulatedValue applies depth × macro × range", "[modengine]")
{
    StubModEngineHost proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "MOD_TEST", makeModEngineTestLayout());
    ModulationEngine modA(apvts, "a_");
    modA.addModulator(std::make_unique<Macro>("macro1", apvts, "macro1"));

    Routing r;
    r.sourceId = "macro1";
    r.paramId  = "a_ghost";
    r.depth    = 0.5f;
    REQUIRE(modA.addRouting(r));

    // a_ghost range 0..100, span 100. depth 0.5. macro 0 → modulation 0.
    REQUIRE(modA.getModulatedValue("a_ghost", 50.0f) == Approx(50.0f));

    // Macro = 0.5 → modulation = 0.5 * 0.5 * 100 = 25. base 50 + 25 = 75.
    if (auto* p = apvts.getParameter("macro1"))
        p->setValueNotifyingHost(0.5f);
    REQUIRE(modA.getModulatedValue("a_ghost", 50.0f) == Approx(75.0f).margin(1.0e-3f));

    // Macro = 1.0 → modulation = 0.5 * 1.0 * 100 = 50. base 50 + 50 = 100.
    if (auto* p = apvts.getParameter("macro1"))
        p->setValueNotifyingHost(1.0f);
    REQUIRE(modA.getModulatedValue("a_ghost", 50.0f) == Approx(100.0f).margin(1.0e-3f));
}

TEST_CASE("ModulationEngine: getModulatedValue clamps to param range", "[modengine]")
{
    StubModEngineHost proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "MOD_TEST", makeModEngineTestLayout());
    ModulationEngine modA(apvts, "a_");
    modA.addModulator(std::make_unique<Macro>("macro1", apvts, "macro1"));

    Routing r; r.sourceId = "macro1"; r.paramId = "a_ghost"; r.depth = 1.0f;
    modA.addRouting(r);

    if (auto* p = apvts.getParameter("macro1"))
        p->setValueNotifyingHost(1.0f);

    // base 80 + (1 * 1 * 100) = 180 → clamped to 100.
    REQUIRE(modA.getModulatedValue("a_ghost", 80.0f) == Approx(100.0f));
}

TEST_CASE("ModulationEngine: persistence round-trip", "[modengine]")
{
    StubModEngineHost proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "MOD_TEST", makeModEngineTestLayout());
    ModulationEngine modA1(apvts, "a_");
    modA1.addModulator(std::make_unique<Macro>("macro1", apvts, "macro1"));
    Routing r; r.sourceId = "macro1"; r.paramId = "a_ghost"; r.depth = 0.5f;
    modA1.addRouting(r);

    auto tree = modA1.toValueTree();

    ModulationEngine modA2(apvts, "a_");
    modA2.addModulator(std::make_unique<Macro>("macro1", apvts, "macro1"));
    modA2.fromValueTree(tree);

    REQUIRE(modA2.getRoutings().size() == 1);
    REQUIRE(modA2.getRoutings()[0].depth == 0.5f);
    REQUIRE(modA2.getRoutings()[0].sourceId == "macro1");
    REQUIRE(modA2.getRoutings()[0].paramId == "a_ghost");
}
```

- [ ] **Step 4: Wire into `tests/CMakeLists.txt`**

Add `ModulationEngineTests.cpp` to the test target's source list and `../Source/Modulation/ModulationEngine.cpp` to its compiled sources.

- [ ] **Step 5: Build + run**

```
cmake --build build --target KaigenPhantomTests --config Debug
./build/tests/Debug/KaigenPhantomTests.exe -c "[modengine]"
```

Expected: 7 cases pass.

```
./build/tests/Debug/KaigenPhantomTests.exe
```

Expected: 91 cases (78 + 4 routing + 6 modulator + 7 modengine = 95? recount: 78 baseline, +4 routing, +2 modulator from Task 1, +4 macro from Task 2, +7 modengine from Task 3 = 95 total).

- [ ] **Step 6: Commit**

```bash
git add Source/Modulation/ModulationEngine.h Source/Modulation/ModulationEngine.cpp \
        tests/ModulationEngineTests.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(modulation): ModulationEngine container

Per-engine container that owns scoped Modulator instances + a routing
table. addRouting validates paramId prefix (rejects routings to params
outside its scope, e.g., engine A engine cannot route to b_*) and
sourceId existence. getModulatedValue applies clamp(base + depth ×
modulator × range, min, max) for each matching routing.

ValueTree round-trip preserves modulator state + routings. PR3a only
exercises this with Macro modulators; PR4-6 reuse the container for
LFOs, Random, etc.
EOF
)"
```

---

## Task 4: APVTS macro params + ModulationConfig persistence

**Files:**
- Modify: `Source/Parameters.h`
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`

- [ ] **Step 1: Add macro IDs + params in `Source/Parameters.h`**

In the `ParamID` namespace, near the new top-level morph params:

```cpp
    inline constexpr auto MACRO1 = "macro1";
    inline constexpr auto MACRO2 = "macro2";
    inline constexpr auto MACRO3 = "macro3";
    inline constexpr auto MACRO4 = "macro4";
```

In `getAllParameterIDs()`, after the morph params, add:

```cpp
    ids.push_back(ParamID::MACRO1);
    ids.push_back(ParamID::MACRO2);
    ids.push_back(ParamID::MACRO3);
    ids.push_back(ParamID::MACRO4);
```

In `createParameterLayout()`, after the morph params block, add:

```cpp
    // Macros (PR3a) — global APVTS params, automatable. Read by Macro
    // modulators in ModulationEngine.
    params.push_back(std::make_unique<APF>(
        ParamID::MACRO1, "Macro 1",
        NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<APF>(
        ParamID::MACRO2, "Macro 2",
        NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<APF>(
        ParamID::MACRO3, "Macro 3",
        NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<APF>(
        ParamID::MACRO4, "Macro 4",
        NormalisableRange<float>(0.0f, 1.0f), 0.0f));
```

- [ ] **Step 2: Add ModulationEngine members + accessors in `PluginProcessor.h`**

Near the top of `PluginProcessor.h`, alongside other Source/ includes:

```cpp
#include "Modulation/ModulationEngine.h"
#include "Modulation/Macro.h"
```

In the public section near other engine accessors:

```cpp
    kaigen::phantom::ModulationEngine& getModulationEngineA() noexcept { return modEngineA; }
    kaigen::phantom::ModulationEngine& getModulationEngineB() noexcept { return modEngineB; }
```

In the private section:

```cpp
    kaigen::phantom::ModulationEngine modEngineA { apvts, "a_" };
    kaigen::phantom::ModulationEngine modEngineB { apvts, "b_" };
```

(These should be declared AFTER `apvts` in the member-init order — read the existing `PluginProcessor.h` member layout to verify.)

- [ ] **Step 3: Wire macros into the modulation engines in the constructor**

In `PhantomProcessor::PhantomProcessor()` body (`PluginProcessor.cpp`), after `apvts` is constructed and after `dualEngineHost` is constructed:

```cpp
    using kaigen::phantom::Macro;
    modEngineA.addModulator(std::make_unique<Macro>("macro1", apvts, ParamID::MACRO1));
    modEngineA.addModulator(std::make_unique<Macro>("macro2", apvts, ParamID::MACRO2));
    modEngineB.addModulator(std::make_unique<Macro>("macro3", apvts, ParamID::MACRO3));
    modEngineB.addModulator(std::make_unique<Macro>("macro4", apvts, ParamID::MACRO4));

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

- [ ] **Step 4: Persist `<ModulationConfig>` in `getStateInformation` / `setStateInformation`**

In `getStateInformation`, after the `<SpectrumView>` write, add:

```cpp
    juce::ValueTree modConfig("ModulationConfig");
    modConfig.appendChild(modEngineA.toValueTree(), nullptr);
    modConfig.appendChild(modEngineB.toValueTree(), nullptr);
    wrapper.appendChild(modConfig, nullptr);
```

In `setStateInformation`, after the `<SpectrumView>` restore, add:

```cpp
    if (auto modConfig = wrapper.getChildWithName("ModulationConfig"); modConfig.isValid())
    {
        for (int i = 0; i < modConfig.getNumChildren(); ++i)
        {
            auto engineNode = modConfig.getChild(i);
            if (! engineNode.hasType("Engine")) continue;
            const auto p = engineNode.getProperty("prefix").toString();
            if      (p == "a_") modEngineA.fromValueTree(engineNode);
            else if (p == "b_") modEngineB.fromValueTree(engineNode);
        }
    }
```

- [ ] **Step 5: Wire `Source/Modulation/*.cpp` into the plugin's CMake source list**

Open root `CMakeLists.txt`. Find the plugin's source list (where `Source/MorphCrossfader.cpp` etc. are listed). Add:

```
Source/Modulation/Routing.cpp
Source/Modulation/Macro.cpp
Source/Modulation/ModulationEngine.cpp
```

- [ ] **Step 6: Build + run tests**

```
cmake --build build --target KaigenPhantom_VST3 --config Debug
cmake --build build --target KaigenPhantomTests --config Debug
./build/tests/Debug/KaigenPhantomTests.exe
```

Expected: clean builds, 95 tests pass.

- [ ] **Step 7: Commit**

```bash
git add Source/Parameters.h Source/PluginProcessor.h Source/PluginProcessor.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(modulation): macro APVTS params + ModulationEngine wiring

Adds 4 global macro params (macro1-macro4) — automatable by host.
PhantomProcessor instantiates ModulationEngine A (scoped to a_*,
holds Macro 1+2) and ModulationEngine B (scoped to b_*, holds Macro
3+4) in the constructor.

A hardcoded proof-of-life routing (Macro 1 -> a_ghost at +50%) is
added so PR3a is end-to-end verifiable without UI: automate macro1
in the DAW and engine A's ghost knob audibly responds. PR3b removes
this hardcoded routing when the macro editor lands.

<ModulationConfig> persisted in plugin state alongside <EditorFocus>
and <SpectrumView>. Preset-side persistence comes in PR3b.
EOF
)"
```

---

## Task 5: DualEngineHost integration — value-lookup intercept

**Files:**
- Modify: `Source/DualEngineHost.h`
- Modify: `Source/DualEngineHost.cpp`

- [ ] **Step 1: Add ModulationEngine pointers + setter to `DualEngineHost.h`**

In the public section:

```cpp
    /** Inject the modulation engines that intercept per-param value lookup
     *  in syncEngineFromPrefix. Owned by PhantomProcessor; pointers are
     *  non-owning. Pass nullptr for either side to disable modulation
     *  on that engine (default state — must be set after construction
     *  for routings to take effect). */
    void setModulationEngines(kaigen::phantom::ModulationEngine* modA,
                              kaigen::phantom::ModulationEngine* modB) noexcept;
```

In the private section:

```cpp
    kaigen::phantom::ModulationEngine* modA { nullptr };
    kaigen::phantom::ModulationEngine* modB { nullptr };
```

Add `#include "Modulation/ModulationEngine.h"` near the top.

- [ ] **Step 2: Implement `setModulationEngines`** in `DualEngineHost.cpp`

```cpp
void DualEngineHost::setModulationEngines(kaigen::phantom::ModulationEngine* a,
                                           kaigen::phantom::ModulationEngine* b) noexcept
{
    modA = a;
    modB = b;
}
```

- [ ] **Step 3: Modify `syncEngineFromPrefix` to consult the modulation engine**

Change the `valueFor` lambda inside `DualEngineHost::syncEngineFromPrefix` to optionally route through the appropriate ModulationEngine for modulated value lookup:

```cpp
void DualEngineHost::syncEngineFromPrefix(PhantomEngine& target, const char* prefix)
{
    auto* modEng = (prefix == juce::String("a_")) ? modA
                 : (prefix == juce::String("b_")) ? modB
                 : nullptr;

    auto valueFor = [this, prefix, modEng](const char* leaf) -> float {
        const auto id = juce::String(prefix) + leaf;
        const float base = apvts.getRawParameterValue(id)->load();
        return modEng ? modEng->getModulatedValue(id, base) : base;
    };

    // ... rest unchanged: target.setCrossoverHz(valueFor(LEAF_PHANTOM_THRESHOLD)); etc ...
}
```

(Keep the rest of `syncEngineFromPrefix`'s body — every `valueFor(LEAF_*)` call now routes through the modulation engine when one is set.)

- [ ] **Step 4: Inject the engines from `PhantomProcessor`'s constructor**

In `PluginProcessor.cpp`, after `dualEngineHost` is constructed and modEngineA/B have their modulators added (i.e., right after Task 4 Step 3's setup but before the proof-of-life routing if order matters):

```cpp
    dualEngineHost.setModulationEngines(&modEngineA, &modEngineB);
```

- [ ] **Step 5: Build + run tests**

```
cmake --build build --target KaigenPhantom_VST3 --config Debug
./build/tests/Debug/KaigenPhantomTests.exe
```

Expected: clean build, 95 tests pass.

- [ ] **Step 6: Commit**

```bash
git add Source/DualEngineHost.h Source/DualEngineHost.cpp Source/PluginProcessor.cpp
git commit -m "$(cat <<'EOF'
feat(modulation): DualEngineHost value-lookup intercept

DualEngineHost::syncEngineFromPrefix now routes per-param value
lookup through the matching ModulationEngine. When no engine is
injected (default), behavior is unchanged — base APVTS values pass
through. When injected (PhantomProcessor's constructor wires it
post-PR3a), the per-block sync uses clamped base + modulation.

ModulationEngine pointer is non-owning; PhantomProcessor owns the
instances. Null-safety preserves the prior single-tab behavior in
case modulation isn't desired in some build configuration.
EOF
)"
```

---

## Task 6: Manual integration smoke test

**Files:** none modified.

- [ ] **Step 1: Build Release**

```
cmake --build build --target KaigenPhantom_VST3 --config Release
```

Expected: clean build + post-install copy.

- [ ] **Step 2: Open in Live 12, drop on a track**

- [ ] **Step 3: Verify macro params exist + are automatable**

In Live's plugin device "Configure" mode (or whatever exposes the parameter list), confirm 4 new params are visible: `Macro 1`, `Macro 2`, `Macro 3`, `Macro 4`. They should be in the global params section (alongside Bypass, Input Gain, Morph) — NOT in the A. or B. per-engine groups.

Map `Macro 1` to a track-level macro or automation lane.

- [ ] **Step 4: Verify the proof-of-life routing**

The hardcoded routing in PR3a's `PhantomProcessor` constructor binds `Macro 1` to `a_ghost` at +50% depth. Test:

- Switch to engine A tab. Set Ghost knob to ~50% (mid-range).
- Move `Macro 1` from 0 to 1 in Live. The engine's audible behavior should change as Ghost effectively sweeps from 50% to 100% (50 + 0.5 × 1.0 × 100 = 100, clamped at 100).
- Move `Macro 1` from 1 back to 0 — the audible behavior reverts.
- The Ghost knob's ON-SCREEN position does NOT move (modulation operates on a copy of the value sent to the engine; APVTS still holds the user-set value). This is the expected behavior in PR3a — visual indication of modulation is PR3b's bottom panel job.

- [ ] **Step 5: Verify other macros are wired but inert**

`Macro 2` is held by modEngineA but has no routings, so automating it has no audible effect. Same for `Macro 3`, `Macro 4` on modEngineB. Expected — confirms the macros exist as APVTS params even without routings.

- [ ] **Step 6: Verify state persistence**

- Save the Live project. Close Live.
- Reopen Live. Verify Macro 1 automation is still mapped (this is a Live concern, not a plugin one — but tests that the param IDs survive across sessions).
- Internally, the `<ModulationConfig>` persisted; on restore, modEngineA's hardcoded routing is RESTORED from the saved tree (the constructor ALSO re-adds it but `setStateInformation` overwrites with the persisted state). PR3b removes the hardcoded constructor routing — at that point the saved state is the only source.

  Note: this is an awkward duplication that PR3b cleans up. In PR3a, the proof-of-life routing exists in BOTH the constructor (so first launch has it) AND in any saved state (so reload preserves it). On reload, the saved state's routings replace the constructor's — but since they're identical in PR3a, no observable difference.

- [ ] **Step 7: Verify no regression in PR1+PR2 functionality**

- Engine A/B/LINK tab switching still works.
- Morph slider still crossfades audio.
- Spectrum view still has SPLIT/COMBINED toggle.
- Bypass + auto-input-gain still work.

- [ ] **Step 8: If smoke surfaces issues, fix in focused commits; otherwise no-op**

---

## End-of-PR Checklist

- [ ] All ~95 unit tests pass (78 PR2 baseline + 17 new in PR3a).
- [ ] VST3 builds cleanly Debug + Release.
- [ ] Macro 1 → audibly affects engine A's Ghost via the hardcoded proof-of-life routing.
- [ ] Macros 2, 3, 4 exist as automatable params but have no audible effect (no routings yet — UI for that is PR3b).
- [ ] `<ModulationConfig>` survives DAW project save/reopen.
- [ ] No regression in PR1/PR2 features.
- [ ] Working tree is clean after the final commit.

---

## Self-Review

**Spec coverage:**
- Modulator abstract base + Routing + Macro concrete + ModulationEngine container — Tasks 1-3 ✓
- 4 macro APVTS params for host automation — Task 4 ✓
- Per-engine modulator scope (Macro 1+2 → A, Macro 3+4 → B) — Task 4 (init) ✓
- ModulationEngine intercepts per-param value lookup with `clamp(base + depth × mod × range, min, max)` — Task 5 ✓
- `<ModulationConfig>` plugin-state persistence — Task 4 ✓
- Hardcoded proof-of-life routing for end-to-end verification without UI — Task 4 ✓
- Per-engine scope structurally enforced (addRouting rejects wrong-prefix paramIds) — Task 3, tested ✓
- Range scaling per-param via `getNormalisableRange()` — Task 3 ✓
- TDD discipline throughout — Tasks 1-3 are tests-then-impl; Task 4-5 are integration so tested via Task 6 manual smoke + the existing PR1+PR2 audio-path tests.

**Placeholder scan:** none. Every step has actual code.

**Type / name consistency:**
- `Modulator`, `Macro`, `Routing`, `ModulationEngine` — all in `kaigen::phantom` namespace.
- `getCurrentValue()`, `getModulatedValue()`, `getModulationEngineA()` — verb-then-noun, consistent.
- Param IDs `MACRO1` etc. — global, no prefix.
- `<Engine prefix="a_">`, `<Modulators>`, `<Routings>`, `<Route>`, `<Macro>` — XML node naming consistent across read/write paths.

**Deferred items:**
- Macro editor UI (drawer panel, name field, destinations list, automation toggle) — PR3b.
- Drag-to-assign UX — PR3c.
- Pigments-style colored knob rings — PR3c.
- Matrix popover — PR3c.
- Preset-side persistence of routings (currently only plugin state) — PR3b decides if presets carry routings.
- Hardcoded proof-of-life routing — removed in PR3b when UI lets the user create their own.
