# PR 2: A | B | LINK Engine Tabs Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an `A | B | LINK` tab toggle to the editor header. Clicking `A` shows/edits Engine A's params, clicking `B` shows/edits Engine B's. `LINK` mirrors knob value writes to both engines without changing which side is visually active. Focus state persists across editor opens (saved as a UI preference inside plugin state, NOT inside preset state).

**Architecture:** PR1 left two engines + `a_*`/`b_*` APVTS layout in place; relays are registered with bare leaf names (`"ghost"`) and attached to `a_*` APVTS params via `WebSliderParameterAttachment`. PR2 adds a parallel set of `b_*`-named relays attached to `b_*` APVTS params, plus a JS-side **stable-wrapper layer** that dispatches reads/writes to the active side (or both, for LINK) based on `window.__kaigenActiveTab` and `window.__kaigenLinkOn`. Tab clicks call native `setEngineFocus()`, which persists the focus on the processor and broadcasts a global JS tab-change event so every wrapper fires its `valueChangedEvent` listeners — refreshing every knob to the new side's value without any per-knob rebinding.

**Tech Stack:** C++20, JUCE 8.0.4 (`WebSliderRelay`, `WebSliderParameterAttachment`, native function bindings via `withNativeFunction`), HTML/CSS/JS, WebView2.

---

## Decisions baked into this plan

- **`engineFocus` is two flags, not three values.** PR1's plan over-simplified `engineFocus` as `'A' | 'B' | 'LINK'`. The spec is clearer: tabs are mutually exclusive (`A` or `B`), and `LINK` is an *independent* toggle that changes write semantics without changing which side is displayed. So PR2 stores `{ activeTab: ActiveTab, linkOn: bool }`. The JS side mirrors this with two globals: `window.__kaigenActiveTab` and `window.__kaigenLinkOn`. The PR1-era single-flag `__kaigenEngineFocus` is replaced.
- **Always-attached pair, not detach-and-reattach.** Both `a_*` and `b_*` relays stay alive and bound to their respective APVTS params at all times. The JS layer routes reads/writes between them based on focus. This is cheaper than rebuilding `WebSliderParameterAttachment`s on every tab click and avoids any "knob freezes mid-drag" windows.
- **LINK mode does NOT mirror modulator routings** — only knob *values*. The drag-to-assign UI for modulators (PR3) will explicitly create routings on the active tab's engine only, regardless of LINK state. This was a locked decision in brainstorming; documenting here so PR3 doesn't accidentally implement otherwise.
- **Focus is editor-state, not preset-state.** Save in `<EditorFocus>` child of `<PluginState>` in `getStateInformation` / `setStateInformation`. Do NOT include in preset save/load — preset switching shouldn't move the user's tab.
- **Diverged-value indicator is NOT in this PR.** The spec mentions a small marker that appears when LINK is on and a knob's `a_X` ≠ `b_X`. It's polish; defer to a small follow-up. Plan-text mentions it only to say it's deferred.

---

## File Plan

**Created:**
- `tests/EngineFocusTests.cpp` — Catch2 tests for the `EngineFocus` struct + persistence round-trip.

**Modified:**
- `Source/PluginProcessor.h` — add `EngineFocus` struct + member + getter/setter; declare native-binding helpers if they're declared here (likely not — bindings are inline lambdas in `PluginEditor.cpp`).
- `Source/PluginProcessor.cpp` — initialize `engineFocus`, persist it in `getStateInformation` / `setStateInformation` as a `<EditorFocus>` child.
- `Source/PluginEditor.h` — add ~40 mirror b_* `WebSliderRelay` / `WebToggleRelay` / `WebComboBoxRelay` member declarations alongside the existing a_*-attached set.
- `Source/PluginEditor.cpp` — declare the `b_*` relays (matching the same logical-name set), add `WebSliderParameterAttachment` (etc.) for each pointing at the b_* APVTS param, register both relays with the WebView. Add two native function bindings: `engineGetFocus` / `engineSetFocus`.
- `Source/WebUI/index.html` — add `<div id="engine-tabs">` to the header with three buttons (A, B, LINK). Order: A | B | LINK. ARIA-friendly.
- `Source/WebUI/styles.css` — tab styling. Cool-spectrum aesthetic to match the design spec's headline-white morph; muted blue when inactive, brighter blue when active.
- `Source/WebUI/juce-frontend.js` — replace PR1's pass-through `getSliderStateLogical` with a real wrapper-construction layer. Add `getToggleStateLogical` / `getComboBoxStateLogical` wrappers symmetrically. Add a `kaigen-tab-changed` global event broadcast.
- `Source/WebUI/phantom.js` — only changes if call sites change; the resolver swap is internal so most call sites keep working unchanged. May need to remove any `__kaigenEngineFocus` references and replace with `__kaigenActiveTab` / `__kaigenLinkOn`.
- `tests/CMakeLists.txt` — add `EngineFocusTests.cpp`.

**Deleted:** none.

---

## Task 1: `EngineFocus` state on `PhantomProcessor` (TDD)

**Files:**
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`
- Create: `tests/EngineFocusTests.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Add the `EngineFocus` struct + members to `Source/PluginProcessor.h`.**

In the public section of `PhantomProcessor`, add:

```cpp
    // ─── Engine focus (editor-state, not preset-state) ────────────────────
    enum class ActiveTab : int { A = 0, B = 1 };

    struct EngineFocus
    {
        ActiveTab activeTab { ActiveTab::A };
        bool      linkOn    { false };
    };

    EngineFocus getEngineFocus() const noexcept { return engineFocus; }
    void setEngineFocus(EngineFocus newFocus) noexcept;
```

In the private section, add the storage:

```cpp
    EngineFocus engineFocus;
```

- [ ] **Step 2: Implement `setEngineFocus` in `Source/PluginProcessor.cpp`.**

Add (anywhere in the file body):

```cpp
void PhantomProcessor::setEngineFocus(EngineFocus newFocus) noexcept
{
    engineFocus = newFocus;
}
```

- [ ] **Step 3: Persist `engineFocus` in `getStateInformation` / `setStateInformation`.**

Locate `PhantomProcessor::getStateInformation` in `Source/PluginProcessor.cpp`. After the `<APVTSState>` child is appended to the wrapper, append a new `<EditorFocus>` child:

```cpp
    juce::ValueTree focusTree("EditorFocus");
    focusTree.setProperty("activeTab", (engineFocus.activeTab == ActiveTab::B) ? "B" : "A", nullptr);
    focusTree.setProperty("linkOn", engineFocus.linkOn, nullptr);
    wrapper.appendChild(focusTree, nullptr);
```

In `PhantomProcessor::setStateInformation`, after the `apvts.replaceState(...)` call, add:

```cpp
    if (auto focusTree = wrapper.getChildWithName("EditorFocus"); focusTree.isValid())
    {
        EngineFocus restored;
        restored.activeTab = (focusTree.getProperty("activeTab").toString() == "B")
                             ? ActiveTab::B : ActiveTab::A;
        restored.linkOn    = (bool) focusTree.getProperty("linkOn");
        engineFocus = restored;
    }
```

- [ ] **Step 4: Write a failing test for the round-trip.**

Create `tests/EngineFocusTests.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "PluginProcessor.h"

TEST_CASE("EngineFocus default is A + LINK off", "[focus]")
{
    PhantomProcessor p;
    auto f = p.getEngineFocus();
    REQUIRE(f.activeTab == PhantomProcessor::ActiveTab::A);
    REQUIRE_FALSE(f.linkOn);
}

TEST_CASE("EngineFocus setEngineFocus stores the value", "[focus]")
{
    PhantomProcessor p;
    p.setEngineFocus({ PhantomProcessor::ActiveTab::B, true });
    auto f = p.getEngineFocus();
    REQUIRE(f.activeTab == PhantomProcessor::ActiveTab::B);
    REQUIRE(f.linkOn);
}

TEST_CASE("EngineFocus survives getStateInformation -> setStateInformation round-trip", "[focus]")
{
    PhantomProcessor p1;
    p1.setEngineFocus({ PhantomProcessor::ActiveTab::B, true });

    juce::MemoryBlock buf;
    p1.getStateInformation(buf);

    PhantomProcessor p2;
    REQUIRE(p2.getEngineFocus().activeTab == PhantomProcessor::ActiveTab::A);
    REQUIRE_FALSE(p2.getEngineFocus().linkOn);

    p2.setStateInformation(buf.getData(), (int) buf.getSize());

    auto f = p2.getEngineFocus();
    REQUIRE(f.activeTab == PhantomProcessor::ActiveTab::B);
    REQUIRE(f.linkOn);
}
```

- [ ] **Step 5: Wire `tests/EngineFocusTests.cpp` into the test target.**

In `tests/CMakeLists.txt`, add `EngineFocusTests.cpp` to the `target_sources` block alongside the other PR1 test files.

- [ ] **Step 6: Build the test target and run the focused tests.**

Run: `cmake --build build --target KaigenPhantomTests --config Debug`
Then: `./build/tests/Debug/KaigenPhantomTests.exe -c "[focus]"`
Expected: 3 cases, all pass.

- [ ] **Step 7: Run the full test suite to verify no regression.**

Run: `./build/tests/Debug/KaigenPhantomTests.exe`
Expected: 72 cases (was 69 + 3 new). All pass.

- [ ] **Step 8: Commit.**

```bash
git add Source/PluginProcessor.h Source/PluginProcessor.cpp tests/EngineFocusTests.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(focus): EngineFocus state on PhantomProcessor

ActiveTab (A|B) + linkOn flags. Persisted in getStateInformation /
setStateInformation as <EditorFocus> child of <PluginState> — editor
preference, not preset state. Native bindings + tab UI come in next
tasks.
EOF
)"
```

---

## Task 2: Native function bindings for engine focus

**Files:**
- Modify: `Source/PluginEditor.cpp`

- [ ] **Step 1: Find the existing `withNativeFunction` chain in `Source/PluginEditor.cpp`.**

Look near where the WebView is constructed; native bindings are registered as a chain like `.withOptionsFrom(...).withNativeFunction("name", lambda)`. Read the existing bindings list (they should be the surviving ones from PR1's editor cleanup — probably `getOscilloscopeData`, `getSpectrumData`, `getPitchInfo`, `loadPreset`, `savePreset`, etc., plus the `forwardKeyToHost` keyboard pass-through).

- [ ] **Step 2: Add two new native bindings: `engineGetFocus` and `engineSetFocus`.**

Inside the `withNativeFunction` chain, add:

```cpp
    .withNativeFunction("engineGetFocus", [self = juce::Component::SafePointer(this)]
        (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
    {
        if (self == nullptr) { completion({}); return; }
        const auto f = self->processor.getEngineFocus();
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty("activeTab", f.activeTab == PhantomProcessor::ActiveTab::B ? "B" : "A");
        obj->setProperty("linkOn", f.linkOn);
        completion(juce::var(obj.get()));
    })
    .withNativeFunction("engineSetFocus", [self = juce::Component::SafePointer(this)]
        (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
    {
        if (self == nullptr || args.size() < 1 || !args[0].isObject()) { completion({}); return; }
        auto* obj = args[0].getDynamicObject();
        if (obj == nullptr) { completion({}); return; }
        PhantomProcessor::EngineFocus f;
        f.activeTab = (obj->getProperty("activeTab").toString() == "B")
                      ? PhantomProcessor::ActiveTab::B
                      : PhantomProcessor::ActiveTab::A;
        f.linkOn    = (bool) obj->getProperty("linkOn");
        self->processor.setEngineFocus(f);
        completion({});
    })
```

(Match the exact `withNativeFunction` signature used by neighboring bindings — JUCE 8.0.4's signature takes a `(const Array<var>&, NativeFunctionCompletion)` callable. If the surrounding code uses a different signature shape, adapt.)

- [ ] **Step 3: Build the plugin to verify the new bindings compile.**

Run: `cmake --build build --target KaigenPhantom_VST3 --config Debug`
Expected: clean build.

- [ ] **Step 4: Commit.**

```bash
git add Source/PluginEditor.cpp
git commit -m "$(cat <<'EOF'
feat(focus): engineGetFocus / engineSetFocus native bindings

JS side reads the current focus on editor open (engineGetFocus)
and writes back on tab clicks (engineSetFocus). Persistence is
handled by PhantomProcessor::set/getStateInformation.
EOF
)"
```

---

## Task 3: Register `b_*` `WebSliderRelay`s + `WebSliderParameterAttachment`s

**Files:**
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`

This task duplicates the existing `a_*`-attached relay set with `b_*`-attached siblings. Pattern: every `WebSliderRelay foo { "ghost" }` member becomes `WebSliderRelay aFoo { "a_ghost" }` + `WebSliderRelay bFoo { "b_ghost" }`. Same for toggles and combo boxes. Each is attached to its respective APVTS param via `WebSliderParameterAttachment` (etc.).

PR1 deliberately registered relays with bare leaf names (`"ghost"`) and the JS side passes the bare name through. PR2's switchover renames them to `"a_ghost"` / `"b_ghost"` so the JS resolver can deterministically pick which relay to bind to. The JS resolver in Task 4 always uses prefixed names; the bare-name relays from PR1 are gone after this task.

- [ ] **Step 1: Replace each per-engine `WebSliderRelay foo { "name" }` declaration in `Source/PluginEditor.h` with two declarations.**

For example, the existing line:

```cpp
juce::WebSliderRelay ghostRelay { "ghost" };
```

becomes:

```cpp
juce::WebSliderRelay ghostRelayA { "a_ghost" };
juce::WebSliderRelay ghostRelayB { "b_ghost" };
```

Apply this rename to every per-engine relay (the ones whose attachment in `PluginEditor.cpp` targets `ParamID::A_*`). Do the same for `WebToggleRelay` and `WebComboBoxRelay` declarations:

```cpp
juce::WebToggleRelay punchEnabledRelayA { "a_punch_enabled" };
juce::WebToggleRelay punchEnabledRelayB { "b_punch_enabled" };

juce::WebComboBoxRelay modeRelayA { "a_mode" };
juce::WebComboBoxRelay modeRelayB { "b_mode" };
```

Globals (`bypass`, `input_gain`, `input_gain_auto`, `morph_amount`, etc.) keep their existing single relay each — no renaming.

- [ ] **Step 2: Update each `WebSliderParameterAttachment` (etc.) in `Source/PluginEditor.cpp` to attach the new pair of relays to their respective APVTS params.**

For each per-engine attachment, the existing line:

```cpp
WebSliderParameterAttachment ghostAttachment { *processor.apvts.getParameter(ParamID::A_GHOST), ghostRelay, nullptr };
```

becomes two lines:

```cpp
WebSliderParameterAttachment ghostAttachmentA { *processor.apvts.getParameter(ParamID::A_GHOST), ghostRelayA, nullptr };
WebSliderParameterAttachment ghostAttachmentB { *processor.apvts.getParameter(ParamID::B_GHOST), ghostRelayB, nullptr };
```

Apply across every per-engine attachment. (The exact attachment class names — `WebSliderParameterAttachment`, `WebToggleParameterAttachment`, `WebComboBoxParameterAttachment` — should match what was in the file pre-PR2.)

- [ ] **Step 3: Update the WebView relay-registration call** (the one that hands the relays to the `WebBrowserComponent` so the JS bridge knows about them) **to register both relays of each pair.**

Look for the `.withOptionsFrom(relay)` chain (or equivalent) on the `WebBrowserComponent::Options` builder. For every per-engine relay it currently lists once, list both A and B versions:

```cpp
.withOptionsFrom(ghostRelayA)
.withOptionsFrom(ghostRelayB)
.withOptionsFrom(phantomThresholdRelayA)
.withOptionsFrom(phantomThresholdRelayB)
// ... (and so on for every per-engine relay)
```

Globals stay registered once.

- [ ] **Step 4: Build the plugin to verify everything compiles + links.**

Run: `cmake --build build --target KaigenPhantom_VST3 --config Debug`
Expected: clean build. The JS side will not yet read the new relays — that's Task 4. The editor will still load and show Engine A's values (because PR1's resolver is a pass-through and the bare-name relays are gone, so it will fail to find them and create orphan SliderStates). **This is expected during this task; Task 4 fixes it.**

- [ ] **Step 5: Commit.**

```bash
git add Source/PluginEditor.h Source/PluginEditor.cpp
git commit -m "$(cat <<'EOF'
refactor(editor): paired a_/b_ WebSliderRelay registration

Every per-engine relay is now declared twice (a_<leaf> + b_<leaf>)
and attached to its respective APVTS param. JS-side resolver in
the next task will pick which relay to bind based on the active
tab. WebView is broken at this commit — Task 4 wires it back up.
EOF
)"
```

---

## Task 4: JS stable-wrapper layer (replace PR1's pass-through resolver)

**Files:**
- Modify: `Source/WebUI/juce-frontend.js`

PR1's resolver was `return getSliderState(logicalName);` — a no-op that worked because relays were registered with bare leaf names. Task 3 renamed them to `a_*` / `b_*`. PR2's resolver creates a stable wrapper object per logical name. The wrapper:
- Holds references to both `a_X` and `b_X` relays.
- On `setNormalisedValue(v)`: if LINK is on, write to both. Else write to whichever side `__kaigenActiveTab` selects.
- On `getNormalisedValue()`: read from `__kaigenActiveTab`'s side.
- Forwards underlying relay events to its own `valueChangedEvent` (a `ListenerList`), filtered by which side is currently active.
- Re-fires its `valueChangedEvent` whenever a global `kaigen-tab-changed` window event is dispatched, so every knob refreshes when the tab flips.

- [ ] **Step 1: Replace the existing `getSliderStateLogical` / `getToggleStateLogical` / `getComboBoxStateLogical` block in `Source/WebUI/juce-frontend.js` with the wrapper layer.**

Locate the existing resolver block (around lines 594-636). Replace with:

```javascript
  // ────── Logical-name wrappers (PR2: tab-driven A/B dispatch) ──────
  // window.__kaigenActiveTab : 'A' | 'B'
  // window.__kaigenLinkOn    : boolean
  window.__kaigenActiveTab = window.__kaigenActiveTab || 'A';
  window.__kaigenLinkOn    = !!window.__kaigenLinkOn;

  const KAIGEN_GLOBAL_PARAMS = new Set([
    'bypass', 'input_gain', 'input_gain_auto', 'advanced_open',
    'morph_amount', 'morph_curve', 'morph_a_level_db',
    'morph_b_level_db', 'morph_bypass_idle_engine'
  ]);

  // Cache of wrappers keyed by logical name.
  const _logicalSliderCache = new Map();
  const _logicalToggleCache = new Map();
  const _logicalComboCache  = new Map();

  // Re-broadcast underlying-relay value changes through wrappers when the
  // active tab matches the side that fired. Plus a global tab-changed
  // event re-fires every wrapper's listeners so knobs refresh on tab
  // switch without per-knob rebinding.
  const TAB_CHANGED_EVENT = "kaigen-tab-changed";

  function _makeLogicalWrapper(logicalName, getStatePrefixed) {
    const aState = getStatePrefixed("a_" + logicalName);
    const bState = getStatePrefixed("b_" + logicalName);
    if (!aState || !bState) {
      console.warn("[kaigen] missing a_/b_ relay for logical name", logicalName);
      return null;
    }

    const listeners = new ListenerList();

    // Forward underlying relay events, filtered by active tab.
    aState.valueChangedEvent.addListener(() => {
      if (window.__kaigenActiveTab !== 'B') listeners.callListeners();
    });
    bState.valueChangedEvent.addListener(() => {
      if (window.__kaigenActiveTab === 'B') listeners.callListeners();
    });

    // Re-fire on tab change so listeners (knob redraw etc.) re-read.
    window.addEventListener(TAB_CHANGED_EVENT, () => listeners.callListeners());

    return {
      setNormalisedValue: (v) => {
        if (window.__kaigenLinkOn) {
          aState.setNormalisedValue(v);
          bState.setNormalisedValue(v);
        } else if (window.__kaigenActiveTab === 'B') {
          bState.setNormalisedValue(v);
        } else {
          aState.setNormalisedValue(v);
        }
      },
      getNormalisedValue: () =>
        (window.__kaigenActiveTab === 'B' ? bState : aState).getNormalisedValue(),
      valueChangedEvent: listeners,
    };
  }

  function getSliderStateLogical(logicalName) {
    if (KAIGEN_GLOBAL_PARAMS.has(logicalName)) return getSliderState(logicalName);
    if (_logicalSliderCache.has(logicalName)) return _logicalSliderCache.get(logicalName);
    const w = _makeLogicalWrapper(logicalName, getSliderState);
    if (w) _logicalSliderCache.set(logicalName, w);
    return w;
  }

  function getToggleStateLogical(logicalName) {
    if (KAIGEN_GLOBAL_PARAMS.has(logicalName)) return getToggleState(logicalName);
    if (_logicalToggleCache.has(logicalName)) return _logicalToggleCache.get(logicalName);
    const w = _makeLogicalWrapper(logicalName, getToggleState);
    if (w) _logicalToggleCache.set(logicalName, w);
    return w;
  }

  function getComboBoxStateLogical(logicalName) {
    if (KAIGEN_GLOBAL_PARAMS.has(logicalName)) return getComboBoxState(logicalName);
    if (_logicalComboCache.has(logicalName)) return _logicalComboCache.get(logicalName);
    const w = _makeLogicalWrapper(logicalName, getComboBoxState);
    if (w) _logicalComboCache.set(logicalName, w);
    return w;
  }

  // Public API for tab UI (Task 6) to broadcast the change.
  function broadcastKaigenTabChanged() {
    window.dispatchEvent(new Event(TAB_CHANGED_EVENT));
  }
```

- [ ] **Step 2: Update the `window.Juce` exposure** to keep these symbols accessible. Find the existing `window.Juce = { ... }` block and confirm it includes (or add):

```javascript
  window.Juce = {
    ...window.Juce,
    getSliderState,
    getSliderStateLogical,
    getToggleState,
    getToggleStateLogical,
    getComboBoxState,
    getComboBoxStateLogical,
    getNativeFunction,
    broadcastKaigenTabChanged,
    // ... whatever else was already there
  };
```

(Adapt to the existing `window.Juce` shape — preserve every existing key.)

- [ ] **Step 3: Remove the legacy `__kaigenEngineFocus` global.**

Search the file for `__kaigenEngineFocus` and delete the line that initializes it. PR1 set this; PR2 splits into the two flags above.

- [ ] **Step 4: Build the plugin.**

Run: `cmake --build build --target KaigenPhantom_VST3 --config Debug`
Expected: clean build.

- [ ] **Step 5: Manually verify in Live (smoke check before tab UI lands).**

Drop the plugin on a track. Move a knob. Audio responds — the WebView is now binding to `a_<leaf>` correctly. (Tab UI is not yet present, so users can only edit Engine A. That's the expected interim state.)

If the WebView console emits warnings about "missing a_/b_ relay for logical name X" for any per-engine knob, that means Task 3 missed registering one of the b_* relays. Find the missing pair and fix.

- [ ] **Step 6: Commit.**

```bash
git add Source/WebUI/juce-frontend.js
git commit -m "$(cat <<'EOF'
feat(webui): JS wrapper layer dispatches per-engine reads/writes

getSliderStateLogical / getToggleStateLogical / getComboBoxStateLogical
now build stable wrapper objects that hold both a_<leaf> and b_<leaf>
relays. Reads pull from the active-tab side; writes go to the active
side, or both sides when LINK is on. Wrappers re-fire valueChangedEvent
on a global kaigen-tab-changed window event so every bound knob redraws
without per-knob rebinding.

window.__kaigenEngineFocus replaced by two flags:
window.__kaigenActiveTab ('A'|'B') and window.__kaigenLinkOn (bool).

Tab UI in next task will dispatch the broadcastKaigenTabChanged event.
EOF
)"
```

---

## Task 5: Tab UI HTML + CSS

**Files:**
- Modify: `Source/WebUI/index.html`
- Modify: `Source/WebUI/styles.css`

- [ ] **Step 1: Add the tab markup to `Source/WebUI/index.html`.**

Insert the following inside the editor header (near other top-of-editor elements like the preset selector — you'll find it by searching for `header` or `top-bar`):

```html
    <div id="engine-tabs" class="engine-tabs" role="tablist" aria-label="Engine focus">
      <button type="button" class="engine-tab is-active" id="engine-tab-a"
              data-tab="A" role="tab" aria-selected="true">A</button>
      <button type="button" class="engine-tab" id="engine-tab-b"
              data-tab="B" role="tab" aria-selected="false">B</button>
      <button type="button" class="engine-tab engine-tab-link" id="engine-tab-link"
              role="button" aria-pressed="false">LINK</button>
    </div>
```

The `is-active` class on `#engine-tab-a` reflects the default state (A active, LINK off). JS in Task 6 syncs this on editor open from native focus state.

- [ ] **Step 2: Add tab styling to `Source/WebUI/styles.css`.**

Append (or merge with existing header styles):

```css
.engine-tabs {
    display: inline-flex;
    align-items: center;
    gap: 4px;
    padding: 2px;
    background: rgba(20, 24, 30, 0.6);
    border: 1px solid rgba(74, 144, 226, 0.25);
    border-radius: 6px;
    user-select: none;
}
.engine-tab {
    background: transparent;
    border: 1px solid transparent;
    color: #6a7a8a;
    font: 600 11px/1 var(--ui-font, sans-serif);
    letter-spacing: 1.5px;
    padding: 4px 10px;
    cursor: pointer;
    border-radius: 4px;
    transition: color 120ms, background 120ms, border-color 120ms;
}
.engine-tab:hover {
    color: #c8d8ea;
}
.engine-tab.is-active {
    color: #f5f8fb;
    background: rgba(74, 144, 226, 0.18);
    border-color: rgba(74, 144, 226, 0.55);
    box-shadow: 0 0 8px rgba(74, 144, 226, 0.25);
}
.engine-tab-link {
    margin-left: 6px;
    border-left: 1px solid rgba(74, 144, 226, 0.18);
    padding-left: 12px;
}
.engine-tab-link.is-on {
    color: #fafafa;
    background: rgba(122, 168, 208, 0.22);
    border-color: rgba(122, 168, 208, 0.6);
    box-shadow: 0 0 8px rgba(122, 168, 208, 0.3);
}
```

(The colors match the cool-spectrum palette from the design spec: steel blue for the A/B active state, the lighter "macro" cyan-blue for the LINK-on state.)

- [ ] **Step 3: Build to verify the WebUI BinaryData picks up the new files.**

Run: `cmake --build build --target KaigenPhantom_VST3 --config Debug`
Expected: clean build. The tabs render but don't yet do anything (no JS handlers).

- [ ] **Step 4: Commit.**

```bash
git add Source/WebUI/index.html Source/WebUI/styles.css
git commit -m "$(cat <<'EOF'
feat(webui): A | B | LINK tab markup + cool-palette styling

Tabs visible but inert until JS handlers land in next task.
EOF
)"
```

---

## Task 6: Tab UI JS handlers + initial sync

**Files:**
- Modify: `Source/WebUI/phantom.js` (or a new top-level `Source/WebUI/engine-tabs.js`; reuse phantom.js if it already hosts header logic)

- [ ] **Step 1: Add the tab-handler module.**

If `phantom.js` already initializes header elements, append to its DOMContentLoaded init. Otherwise add a fresh IIFE module that runs at script load. Use this module body:

```javascript
// Engine-tab UI controller. Wires the A/B/LINK buttons to the JS state
// flags (window.__kaigenActiveTab, window.__kaigenLinkOn) and the
// native engineSetFocus binding, and syncs the UI to the persisted
// engineGetFocus on editor open.
(function () {
  'use strict';

  if (typeof window.Juce === 'undefined' || typeof window.Juce.getNativeFunction !== 'function') {
    document.addEventListener('DOMContentLoaded', init, { once: true });
    return;
  }
  init();

  function init() {
    const tabA    = document.getElementById('engine-tab-a');
    const tabB    = document.getElementById('engine-tab-b');
    const tabLink = document.getElementById('engine-tab-link');
    if (!tabA || !tabB || !tabLink) return;

    const engineGetFocus = window.Juce.getNativeFunction('engineGetFocus');
    const engineSetFocus = window.Juce.getNativeFunction('engineSetFocus');
    if (!engineGetFocus || !engineSetFocus) {
      console.warn('[engine-tabs] native bindings missing');
      return;
    }

    function applyToUI(activeTab, linkOn) {
      tabA.classList.toggle('is-active', activeTab === 'A');
      tabA.setAttribute('aria-selected', activeTab === 'A' ? 'true' : 'false');
      tabB.classList.toggle('is-active', activeTab === 'B');
      tabB.setAttribute('aria-selected', activeTab === 'B' ? 'true' : 'false');
      tabLink.classList.toggle('is-on', !!linkOn);
      tabLink.setAttribute('aria-pressed', linkOn ? 'true' : 'false');
    }

    function setActiveTab(newTab) {
      if (window.__kaigenActiveTab === newTab) return;
      window.__kaigenActiveTab = newTab;
      applyToUI(window.__kaigenActiveTab, window.__kaigenLinkOn);
      engineSetFocus({ activeTab: window.__kaigenActiveTab, linkOn: window.__kaigenLinkOn });
      window.Juce.broadcastKaigenTabChanged();
    }

    function toggleLink() {
      window.__kaigenLinkOn = !window.__kaigenLinkOn;
      applyToUI(window.__kaigenActiveTab, window.__kaigenLinkOn);
      engineSetFocus({ activeTab: window.__kaigenActiveTab, linkOn: window.__kaigenLinkOn });
      // No tab-change broadcast: LINK toggle doesn't change which side is read.
    }

    tabA.addEventListener('click', () => setActiveTab('A'));
    tabB.addEventListener('click', () => setActiveTab('B'));
    tabLink.addEventListener('click', toggleLink);

    // Initial sync from persisted state.
    engineGetFocus().then(focus => {
      if (focus && typeof focus === 'object') {
        window.__kaigenActiveTab = (focus.activeTab === 'B') ? 'B' : 'A';
        window.__kaigenLinkOn    = !!focus.linkOn;
      } else {
        window.__kaigenActiveTab = 'A';
        window.__kaigenLinkOn    = false;
      }
      applyToUI(window.__kaigenActiveTab, window.__kaigenLinkOn);
      window.Juce.broadcastKaigenTabChanged();   // make every wrapper redraw to the restored tab
    }).catch(() => {
      window.__kaigenActiveTab = 'A';
      window.__kaigenLinkOn    = false;
      applyToUI('A', false);
    });
  }
})();
```

If you're adding this as a new file (`Source/WebUI/engine-tabs.js`), also add a `<script src="engine-tabs.js"></script>` to `Source/WebUI/index.html` alongside the other module scripts.

- [ ] **Step 2: Build the plugin.**

Run: `cmake --build build --target KaigenPhantom_VST3 --config Debug`
Expected: clean build.

- [ ] **Step 3: Manual smoke test (functional verification).**

Drop the plugin in Live. Verify:
- Tabs render in the header. A is highlighted by default; LINK is unhighlighted.
- Click `B` → A loses highlight, B gains highlight. Knobs throughout the UI update to Engine B's values (which mirror A on first load — the visual change will be invisible until you've diverged the engines' params).
- To verify the tab is actually switching: with B active, move a knob (e.g., Ghost). Click `A` — the knob's value should flip back to A's value (still default).
- Click `LINK`. LINK button gains the cyan-blue highlight. Move any knob — both A and B receive the write. Switch tabs to confirm both sides updated.
- Click `LINK` again. LINK turns off. Subsequent edits go to only the active tab.
- Save the project, close Live, reopen. Plugin opens with the same tab + LINK state restored.

- [ ] **Step 4: Commit.**

```bash
git add Source/WebUI/phantom.js
git commit -m "$(cat <<'EOF'
feat(webui): A | B | LINK tab handlers + initial-state sync

Tab clicks update window.__kaigenActiveTab / __kaigenLinkOn, push to
PhantomProcessor via engineSetFocus native binding, and dispatch the
kaigen-tab-changed event so every JS slider wrapper redraws to the
new side. Editor open reads engineGetFocus and restores UI + JS state.
EOF
)"
```

(If you instead created a new `engine-tabs.js`, the `git add` line should reference that file plus `Source/WebUI/index.html`.)

---

## Task 7: End-to-end smoke + manual integration test

**Files:** none modified

- [ ] **Step 1: Build the Release VST3 with the post-build install step.**

Run: `cmake --build build --target KaigenPhantom_VST3 --config Release`
The post-install copies the new VST3 to `%LOCALAPPDATA%/Programs/Common/VST3/`.

- [ ] **Step 2: Open Ableton Live 12 and load Kaigen Phantom on an audio track.**

- [ ] **Step 3: Functional checklist.**

- [ ] Tabs render: A highlighted, B dim, LINK dim.
- [ ] Click `B` → all knobs reflect B's values. Move Ghost on tab B, click A: Ghost reverts to A's value. Click B again: Ghost shows the new B value.
- [ ] Click `LINK` (with A active). LINK gains the cyan highlight. Move a knob — switch tabs to confirm both sides got the write.
- [ ] LINK off → edits target only the active tab.
- [ ] Morph slider still works as audio crossfade (PR1 functionality preserved).
- [ ] Bypass + auto input gain still work.
- [ ] Save preset, reload — both A and B states round-trip correctly through APVTS.
- [ ] Save the Live project, close Live, reopen. Plugin restores the same active tab + LINK state.
- [ ] WebView dev console (if accessible) is silent — no warnings about missing relays.
- [ ] No orphan ghost parameters in Live's Configure mode that say something like "ghost" without the A./B. prefix. Only `A. <name>` and `B. <name>` should be visible (this is mostly about whether the user's existing project state has stale mappings — a fresh plugin instance shouldn't show any).

- [ ] **Step 4: If the smoke test surfaces any issues, fix them in focused commits.**

If the test passes cleanly, no commits in this step.

---

## End-of-PR Checklist

- [ ] All unit tests pass (target count: 72 — 69 from PR1 plus 3 new `[focus]` cases). Run: `./build/tests/Debug/KaigenPhantomTests.exe`
- [ ] VST3 builds cleanly in Debug AND Release configs.
- [ ] Manual smoke test in Ableton Live 12 passes every checklist item.
- [ ] No regressions in PR1 functionality (morph crossfade, idle bypass, preset migration, bypass/input-gain).
- [ ] Working tree is clean after the final commit.
- [ ] No remaining references to the legacy `__kaigenEngineFocus` global anywhere in `Source/WebUI/`.

---

## Self-Review

**1. Spec coverage:**

| Spec section | Implemented in |
|---|---|
| Engine tabs `A | B | LINK` toggle in header | Task 5 (HTML/CSS), Task 6 (JS) |
| `A` clicks: knobs reflect/edit Engine A; `B` clicks: same for B | Tasks 4, 6 (wrapper dispatch + tab-change broadcast) |
| `LINK` toggles independently of which tab is active | Task 6 (`toggleLink` doesn't change `__kaigenActiveTab`) |
| LINK on → both engines get the write | Task 4 (`setNormalisedValue` fan-out) |
| If A and B currently differ, LINK on doesn't change values; next edit re-converges | Task 4 (no implicit copy on link-toggle) |
| Diverged-value indicator (small marker, fades after first link-mode write) | **Deferred** to a polish follow-up PR. Plan-text mentions it under "Decisions baked into this plan". |
| `engineFocus` is editor-state, persisted to plugin state (not preset) | Task 1 (`<EditorFocus>` child of `<PluginState>`) |
| Default: A active, LINK off | Task 1 (struct defaults) + Task 5 (initial HTML class) + Task 6 (`engineGetFocus` fallback) |
| LINK affects knob *values* but not modulator routings | Locked in plan-text "Decisions". PR3 implements modulator routing and will explicitly bind to active tab regardless of LINK. |

**2. Placeholder scan:** searched for "TBD", "TODO", "implement later", "fill in details", "similar to Task N" — none present. Every step that changes code carries the actual code in a fenced block. Every command step has the exact command + expected outcome.

**3. Type / name consistency:**

- `EngineFocus` struct: defined in Task 1 (`PhantomProcessor::EngineFocus { ActiveTab activeTab; bool linkOn; }`). Used in Task 2 native bindings — consistent.
- `ActiveTab` enum: defined in Task 1, used in Task 2 (`PhantomProcessor::ActiveTab::B`) and Task 6 JS side as the string `"A"` / `"B"` — bridged through `engineGet/SetFocus` JSON.
- Relay member naming: Task 3 establishes `<name>RelayA` / `<name>RelayB` pattern; Task 4's JS resolver uses `"a_" + logicalName` / `"b_" + logicalName` to look them up. Consistent.
- `__kaigenActiveTab` / `__kaigenLinkOn` JS globals: defined in Task 4, written in Task 6, broadcast via `kaigen-tab-changed` window event. Consistent.
- `broadcastKaigenTabChanged` helper: defined in Task 4 on `window.Juce`, called in Task 6. Consistent.
- Native binding names: `engineGetFocus` and `engineSetFocus`. Task 2 registers them; Task 6 JS calls them. Match.

**4. No spec gap left without a task.** The diverged-indicator is intentionally deferred (called out explicitly).
