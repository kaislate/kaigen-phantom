# Recipe wheel: auto-switch to Custom + save/delete pills — implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** When the user edits an H value, auto-promote to the first empty Custom slot (or lock the edit if all customs are full). Add save + delete pills under each "Cust N" word with blinking save-pill while dirty.

**Architecture:** Native preset-loading subsystem (built-in tables and Custom slot data both flow through `applyRecipePreset`), H-change APVTS listener that triggers auto-switch / revert / mark-dirty, per-engine Custom slot storage serialised in plugin state, RecipeSlotPills widget below the WordSelector reading derived state at 4 Hz, RecipeWheel `triggerLockFlash` driven by a ChangeBroadcaster on the processor.

**Tech Stack:** JUCE 8 / C++ / APVTS / `juce::AudioProcessorValueTreeState::Listener` / `juce::ValueTree` serialisation / `juce::ChangeBroadcaster`.

**Spec:** `docs/superpowers/specs/2026-05-22-recipe-custom-slots-design.md`

---

### Task 1: `RecipeSlot` data + processor accessors

**Files:**
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`

Add per-engine Custom slot storage and the public accessor surface. No behaviour change yet; subsequent tasks consume these.

- [ ] **Step 1: Add `RecipeSlot` struct + state members to PluginProcessor.h**

Open `Source/PluginProcessor.h`. Find the existing reverb members near the bottom:

```cpp
    juce::AudioBuffer<float>          reverbScratch;
    float                             reverbMixSmoothed { 0.0f };
    std::atomic<float>*               reverbMixParam    { nullptr };
    std::atomic<float>*               reverbSourceParam { nullptr };  // 0 = Post, 1 = Phantom

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomProcessor)
};
```

Replace with (inserts the new members + struct above the leak-detector macro):

```cpp
    juce::AudioBuffer<float>          reverbScratch;
    float                             reverbMixSmoothed { 0.0f };
    std::atomic<float>*               reverbMixParam    { nullptr };
    std::atomic<float>*               reverbSourceParam { nullptr };  // 0 = Post, 1 = Phantom

    // ─── Recipe Custom slots ─────────────────────────────────────────────
    // Per-engine, per-Custom-slot stored H values (H2..H8 normalised [0..1]).
    // When the user manually edits an H value while a built-in preset is
    // selected, the processor auto-switches to the first EMPTY slot. The
    // RecipeSlotPills widget reads these and renders the save/delete UI.
    struct RecipeSlot
    {
        bool                 filled { false };
        std::array<float, 7> savedH {};   // H2..H8
    };

    // [engineIdx 0=A, 1=B][slotIdx 0=Cust1, 1=Cust2, 2=Cust3]
    std::array<std::array<RecipeSlot, 3>, 2> recipeSlots {};

    // Last selected built-in preset per engine — used by clearRecipeSlot()
    // as the fall-back when the user deletes the currently-active slot and
    // no other Custom slot is filled. Indexed [engineIdx]. Initialised to
    // 0 ("Warm").
    std::array<int, 2> lastBuiltInPreset { 0, 0 };

    // Guard that suppresses the auto-switch H-listener while we're loading
    // a preset's H values into the params (so the load itself doesn't read
    // as a user edit and re-trigger auto-switch). UI thread only.
    bool loadingPreset { false };

    // Per-engine "previous H" buffer used by the revert path when an H edit
    // is rejected (all Custom slots full + on a built-in). Updated before
    // every accepted edit and every preset-load. Indexed [engineIdx][hIdx].
    std::array<std::array<float, 7>, 2> previousH {};

    // Fired on the message thread when an H edit was rejected. RecipeWheel
    // listens and triggers its lock-flash overlay.
    juce::ChangeBroadcaster wheelLockBroadcaster;

public:
    // ── Recipe slot public surface (UI-thread only) ──────────────────────
    const RecipeSlot& getRecipeSlot(int engineIdx, int slotIdx) const noexcept;

    /** Save current live H values into the given slot. Marks filled. */
    void saveRecipeSlot(int engineIdx, int slotIdx);

    /** Mark slot empty. If this slot is the active preset, falls back to
     *  the first other filled Custom slot, else lastBuiltInPreset, else 0. */
    void clearRecipeSlot(int engineIdx, int slotIdx);

    /** Returns 0..2 for the first empty Custom slot, or -1 if all filled. */
    int findFirstEmptyCustomSlot(int engineIdx) const noexcept;

    int getLastBuiltInPreset(int engineIdx) const noexcept { return lastBuiltInPreset[(size_t) engineIdx]; }

    juce::ChangeBroadcaster& getWheelLockBroadcaster() noexcept { return wheelLockBroadcaster; }

private:

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomProcessor)
};
```

Note the inserted `public:` / `private:` swap brackets so the new accessors stay public.

- [ ] **Step 2: Add accessor stubs to PluginProcessor.cpp**

Open `Source/PluginProcessor.cpp`. Find the existing `void PhantomProcessor::setEngineFocus(...)` definition (around line 569). Right after its closing `}`, add:

```cpp
const PhantomProcessor::RecipeSlot&
PhantomProcessor::getRecipeSlot(int engineIdx, int slotIdx) const noexcept
{
    const int e = juce::jlimit(0, 1, engineIdx);
    const int s = juce::jlimit(0, 2, slotIdx);
    return recipeSlots[(size_t) e][(size_t) s];
}

int PhantomProcessor::findFirstEmptyCustomSlot(int engineIdx) const noexcept
{
    const int e = juce::jlimit(0, 1, engineIdx);
    for (int s = 0; s < 3; ++s)
        if (! recipeSlots[(size_t) e][(size_t) s].filled)
            return s;
    return -1;
}

void PhantomProcessor::saveRecipeSlot(int /*engineIdx*/, int /*slotIdx*/)
{
    // Implemented in Task 3 once the live-H reader is wired in.
    jassertfalse;
}

void PhantomProcessor::clearRecipeSlot(int /*engineIdx*/, int /*slotIdx*/)
{
    // Implemented in Task 3.
    jassertfalse;
}
```

(`saveRecipeSlot` and `clearRecipeSlot` get real bodies in Task 3 once the H-reader and preset-switch helpers are in place; stub bodies now to make the header compile.)

- [ ] **Step 3: Build**

Run:

```
cd "C:/Documents/NEw project/Kaigen Phantom/build" && "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/amd64/MSBuild.exe" KaigenPhantom.sln //p:Configuration=Release //p:Platform=x64 //m //nologo //v:minimal 2>&1 | tail -6
```

Expected: 0 errors.

- [ ] **Step 4: Commit**

```
cd "C:/Documents/NEw project/Kaigen Phantom" && git add Source/PluginProcessor.h Source/PluginProcessor.cpp && git commit -m "feat(processor): add RecipeSlot storage + accessor stubs"
```

---

### Task 2: Recipe slot serialisation

**Files:**
- Modify: `Source/PluginProcessor.cpp`

Persist `recipeSlots` + `lastBuiltInPreset` across plugin state save/load. Old projects (missing the new tree) load with all slots empty.

- [ ] **Step 1: Write recipe slots to state in `getStateInformation`**

Find the end of `getStateInformation` just before `if (auto xml = wrapper.createXml())`:

```cpp
    // <ModulationConfig> — per-engine modulator + routing tables. Preset-side
    // persistence (within an APVTS-state child or sibling) lands in PR3b; for
    // PR3a we just stage the data alongside <EditorFocus> / <SpectrumView>.
    juce::ValueTree modConfig("ModulationConfig");
    modConfig.appendChild(modEngineA.toValueTree(), nullptr);
    modConfig.appendChild(modEngineB.toValueTree(), nullptr);
    wrapper.appendChild(modConfig, nullptr);

    if (auto xml = wrapper.createXml())
        copyXmlToBinary(*xml, destData);
}
```

Replace with (insert RecipeSlots tree above the createXml call):

```cpp
    // <ModulationConfig> — per-engine modulator + routing tables. Preset-side
    // persistence (within an APVTS-state child or sibling) lands in PR3b; for
    // PR3a we just stage the data alongside <EditorFocus> / <SpectrumView>.
    juce::ValueTree modConfig("ModulationConfig");
    modConfig.appendChild(modEngineA.toValueTree(), nullptr);
    modConfig.appendChild(modEngineB.toValueTree(), nullptr);
    wrapper.appendChild(modConfig, nullptr);

    // <RecipeSlots> — per-engine Custom slot data + last-built-in preset.
    // 6 <Slot engine=E slot=S filled=B h2..h8=F> children, plus two
    // <LastBuiltIn engine=E value=I> entries. Missing on load → all slots
    // empty + lastBuiltIn = 0.
    juce::ValueTree slotsRoot("RecipeSlots");
    for (int e = 0; e < 2; ++e)
    {
        for (int s = 0; s < 3; ++s)
        {
            const auto& slot = recipeSlots[(size_t) e][(size_t) s];
            juce::ValueTree slotNode("Slot");
            slotNode.setProperty("engine", e, nullptr);
            slotNode.setProperty("slot",   s, nullptr);
            slotNode.setProperty("filled", slot.filled, nullptr);
            for (int h = 0; h < 7; ++h)
                slotNode.setProperty(juce::String("h") + juce::String(h + 2),
                                     slot.savedH[(size_t) h], nullptr);
            slotsRoot.appendChild(slotNode, nullptr);
        }

        juce::ValueTree last("LastBuiltIn");
        last.setProperty("engine", e, nullptr);
        last.setProperty("value",  lastBuiltInPreset[(size_t) e], nullptr);
        slotsRoot.appendChild(last, nullptr);
    }
    wrapper.appendChild(slotsRoot, nullptr);

    if (auto xml = wrapper.createXml())
        copyXmlToBinary(*xml, destData);
}
```

- [ ] **Step 2: Read recipe slots in `setStateInformation`**

Find `setStateInformation` and its existing block that reads `EditorFocus`. After the existing children have been read (search for the last `wrapper.getChildWithName(...)` call), add the RecipeSlots reader:

```cpp
        // <RecipeSlots> — per-engine Custom slot data. Missing on legacy
        // projects → all slots remain empty (default-constructed).
        if (auto slotsRoot = wrapper.getChildWithName("RecipeSlots"); slotsRoot.isValid())
        {
            for (int i = 0; i < slotsRoot.getNumChildren(); ++i)
            {
                const auto node = slotsRoot.getChild(i);

                if (node.hasType("Slot"))
                {
                    const int e = (int) node.getProperty("engine", -1);
                    const int s = (int) node.getProperty("slot",   -1);
                    if (e < 0 || e > 1 || s < 0 || s > 2) continue;

                    auto& slot = recipeSlots[(size_t) e][(size_t) s];
                    slot.filled = (bool) node.getProperty("filled", false);
                    for (int h = 0; h < 7; ++h)
                        slot.savedH[(size_t) h] = (float) node.getProperty(
                            juce::String("h") + juce::String(h + 2), 0.0f);
                }
                else if (node.hasType("LastBuiltIn"))
                {
                    const int e = (int) node.getProperty("engine", -1);
                    const int v = (int) node.getProperty("value",  0);
                    if (e >= 0 && e <= 1)
                        lastBuiltInPreset[(size_t) e] = juce::jlimit(0, 5, v);
                }
            }
        }
```

(Place this block just before the final `}` of `setStateInformation`, after the last existing `getChildWithName(...)` consumer.)

- [ ] **Step 3: Build**

```
cd "C:/Documents/NEw project/Kaigen Phantom/build" && "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/amd64/MSBuild.exe" KaigenPhantom.sln //p:Configuration=Release //p:Platform=x64 //m //nologo //v:minimal 2>&1 | tail -6
```

Expected: 0 errors. Old projects load unchanged; new projects will round-trip the (empty) RecipeSlots tree.

- [ ] **Step 4: Commit**

```
cd "C:/Documents/NEw project/Kaigen Phantom" && git add Source/PluginProcessor.cpp && git commit -m "feat(processor): serialise recipe Custom slots in plugin state"
```

---

### Task 3: Native preset-loading + slot save/clear + auto-switch listener

**Files:**
- Modify: `Source/PluginProcessor.cpp`
- Modify: `Source/PluginProcessor.h`

This is the heart of the feature. Rewrites `parameterChanged` to (a) load preset H values from built-in arrays OR Custom slot data, (b) listen to H-param changes and auto-switch / revert / mark dirty. Also fills in the `saveRecipeSlot` and `clearRecipeSlot` bodies.

- [ ] **Step 1: Add private helper declarations to PluginProcessor.h**

Find the existing `void parameterChanged(...)` override in `PluginProcessor.h` (it'll be in the private/protected section, since it's an APVTS::Listener override). After that line, add:

```cpp
    void applyRecipePreset(int engineIdx, int presetIdx);
    void readCurrentH(int engineIdx, std::array<float, 7>& outH) const;
    void writeHParams (int engineIdx, const std::array<float, 7>& inH);
    void writeHParamsRaw01(int engineIdx, const std::array<float, 7>& inH01);
```

If the existing override is in the public section, move the declarations to the same accessibility — these are internal helpers and should be private. If the existing `parameterChanged` declaration is missing, the existing implementation already overrides it; verify by searching for `void parameterChanged` in the header.

- [ ] **Step 2: Register H param listeners in constructor**

Open `Source/PluginProcessor.cpp`. Find the existing `addParameterListener` calls in the constructor (around line 18):

```cpp
    apvts.addParameterListener(ParamID::A_RECIPE_PRESET, this);
    apvts.addParameterListener(ParamID::B_RECIPE_PRESET, this);
```

Replace with (adds 14 more listeners — 7 H params per engine):

```cpp
    apvts.addParameterListener(ParamID::A_RECIPE_PRESET, this);
    apvts.addParameterListener(ParamID::B_RECIPE_PRESET, this);

    // H param listeners drive the auto-switch / revert / mark-dirty path.
    const char* aH[] = { ParamID::A_RECIPE_H2, ParamID::A_RECIPE_H3,
                          ParamID::A_RECIPE_H4, ParamID::A_RECIPE_H5,
                          ParamID::A_RECIPE_H6, ParamID::A_RECIPE_H7,
                          ParamID::A_RECIPE_H8 };
    const char* bH[] = { ParamID::B_RECIPE_H2, ParamID::B_RECIPE_H3,
                          ParamID::B_RECIPE_H4, ParamID::B_RECIPE_H5,
                          ParamID::B_RECIPE_H6, ParamID::B_RECIPE_H7,
                          ParamID::B_RECIPE_H8 };
    for (auto* id : aH) apvts.addParameterListener(id, this);
    for (auto* id : bH) apvts.addParameterListener(id, this);
```

Find the matching `removeParameterListener` block in the destructor (around line 45). Replace:

```cpp
    apvts.removeParameterListener(ParamID::A_RECIPE_PRESET, this);
    apvts.removeParameterListener(ParamID::B_RECIPE_PRESET, this);
```

with:

```cpp
    apvts.removeParameterListener(ParamID::A_RECIPE_PRESET, this);
    apvts.removeParameterListener(ParamID::B_RECIPE_PRESET, this);

    const char* aH[] = { ParamID::A_RECIPE_H2, ParamID::A_RECIPE_H3,
                          ParamID::A_RECIPE_H4, ParamID::A_RECIPE_H5,
                          ParamID::A_RECIPE_H6, ParamID::A_RECIPE_H7,
                          ParamID::A_RECIPE_H8 };
    const char* bH[] = { ParamID::B_RECIPE_H2, ParamID::B_RECIPE_H3,
                          ParamID::B_RECIPE_H4, ParamID::B_RECIPE_H5,
                          ParamID::B_RECIPE_H6, ParamID::B_RECIPE_H7,
                          ParamID::B_RECIPE_H8 };
    for (auto* id : aH) apvts.removeParameterListener(id, this);
    for (auto* id : bH) apvts.removeParameterListener(id, this);
```

- [ ] **Step 3: Add the H read/write helpers + applyRecipePreset**

In `Source/PluginProcessor.cpp`, after the (existing) `parameterChanged` function (around line 557), add:

```cpp
namespace
{
    const char* hParamId(int engineIdx, int hIdx)
    {
        // hIdx is 0..6 (H2..H8). engineIdx 0=A, 1=B.
        static const char* aIds[7] = {
            ParamID::A_RECIPE_H2, ParamID::A_RECIPE_H3, ParamID::A_RECIPE_H4,
            ParamID::A_RECIPE_H5, ParamID::A_RECIPE_H6, ParamID::A_RECIPE_H7,
            ParamID::A_RECIPE_H8
        };
        static const char* bIds[7] = {
            ParamID::B_RECIPE_H2, ParamID::B_RECIPE_H3, ParamID::B_RECIPE_H4,
            ParamID::B_RECIPE_H5, ParamID::B_RECIPE_H6, ParamID::B_RECIPE_H7,
            ParamID::B_RECIPE_H8
        };
        return (engineIdx == 1 ? bIds : aIds)[juce::jlimit(0, 6, hIdx)];
    }

    int hIndexFromParamId(const juce::String& paramId, int& engineIdxOut)
    {
        for (int e = 0; e < 2; ++e)
            for (int h = 0; h < 7; ++h)
                if (paramId == hParamId(e, h))
                {
                    engineIdxOut = e;
                    return h;
                }
        return -1;
    }
}

void PhantomProcessor::readCurrentH(int engineIdx, std::array<float, 7>& outH) const
{
    for (int h = 0; h < 7; ++h)
    {
        const auto* raw = apvts.getRawParameterValue(hParamId(engineIdx, h));
        // Params are stored as [0..100] (percent). Normalise to [0..1].
        outH[(size_t) h] = (raw != nullptr) ? raw->load() * 0.01f : 0.0f;
    }
}

void PhantomProcessor::writeHParams(int engineIdx, const std::array<float, 7>& inH)
{
    // inH is [0..1]; the params want [0..100]; setValueNotifyingHost wants normalised [0..1].
    for (int h = 0; h < 7; ++h)
    {
        if (auto* p = apvts.getParameter(hParamId(engineIdx, h)))
        {
            const float pct = juce::jlimit(0.0f, 100.0f, inH[(size_t) h] * 100.0f);
            p->setValueNotifyingHost(p->convertTo0to1(pct));
        }
    }
}

void PhantomProcessor::writeHParamsRaw01(int engineIdx, const std::array<float, 7>& inH01)
{
    // Same as writeHParams but inputs already 0..1 (used by the revert
    // path which has the previous percent-normalised values cached as
    // 0..1 of the param range).
    writeHParams(engineIdx, inH01);
}

void PhantomProcessor::applyRecipePreset(int engineIdx, int presetIdx)
{
    // Snapshot current H into previousH BEFORE the load — so a subsequent
    // user edit can revert to the just-loaded values, not to whatever was
    // there before this load.
    readCurrentH(engineIdx, previousH[(size_t) engineIdx]);

    if (presetIdx >= 0 && presetIdx <= 5)
    {
        const float* tables[6] = {
            kWarmAmps, kAggressiveAmps, kHollowAmps,
            kDenseAmps, kStableAmps, kWeirdAmps
        };
        const float* src = tables[presetIdx];
        std::array<float, 7> values {};
        for (int h = 0; h < 7; ++h) values[(size_t) h] = src[h];

        loadingPreset = true;
        writeHParams(engineIdx, values);
        loadingPreset = false;

        // After loading a built-in, remember it for delete fallback.
        lastBuiltInPreset[(size_t) engineIdx] = presetIdx;

        // Refresh previousH to the just-loaded values.
        previousH[(size_t) engineIdx] = values;
    }
    else if (presetIdx >= 6 && presetIdx <= 8)
    {
        const int slotIdx = presetIdx - 6;
        const auto& slot = recipeSlots[(size_t) engineIdx][(size_t) slotIdx];
        if (slot.filled)
        {
            loadingPreset = true;
            writeHParams(engineIdx, slot.savedH);
            loadingPreset = false;
            previousH[(size_t) engineIdx] = slot.savedH;
        }
        // If !filled, leave H values as-is — the user is editing into an
        // empty slot. previousH stays at the snapshot above so a later
        // revert restores the pre-switch values.
    }
}
```

- [ ] **Step 4: Replace the existing `parameterChanged` body**

In `Source/PluginProcessor.cpp`, find:

```cpp
void PhantomProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Recipe Preset is a per-engine choice: when either engine's selector changes,
    // populate that engine's H2..H8 amplitudes from the chosen recipe table.
    // "Custom" (index 6) leaves the harmonics untouched.
    const bool isA = (parameterID == ParamID::A_RECIPE_PRESET);
    const bool isB = (parameterID == ParamID::B_RECIPE_PRESET);
    if (!isA && !isB) return;

    const int idx = juce::roundToInt(newValue);
    const float* tables[] = {
        kWarmAmps, kAggressiveAmps, kHollowAmps, kDenseAmps,
        kStableAmps, kWeirdAmps,
        nullptr   // Custom (index 6)
    };

    if (idx < 0 || idx >= 6 || tables[idx] == nullptr) return;

    const char* hIds[7] = {
        isA ? ParamID::A_RECIPE_H2 : ParamID::B_RECIPE_H2,
        isA ? ParamID::A_RECIPE_H3 : ParamID::B_RECIPE_H3,
        isA ? ParamID::A_RECIPE_H4 : ParamID::B_RECIPE_H4,
        isA ? ParamID::A_RECIPE_H5 : ParamID::B_RECIPE_H5,
        isA ? ParamID::A_RECIPE_H6 : ParamID::B_RECIPE_H6,
        isA ? ParamID::A_RECIPE_H7 : ParamID::B_RECIPE_H7,
        isA ? ParamID::A_RECIPE_H8 : ParamID::B_RECIPE_H8,
    };
    for (int i = 0; i < 7; ++i)
        if (auto* p = apvts.getParameter(hIds[i]))
            p->setValueNotifyingHost(p->convertTo0to1(tables[idx][i] * 100.0f));
}
```

Replace with:

```cpp
void PhantomProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Branch 1: recipe_preset change → load that preset's H values.
    if (parameterID == ParamID::A_RECIPE_PRESET)
    {
        applyRecipePreset(0, juce::roundToInt(newValue));
        return;
    }
    if (parameterID == ParamID::B_RECIPE_PRESET)
    {
        applyRecipePreset(1, juce::roundToInt(newValue));
        return;
    }

    // Branch 2: H param change → auto-switch / revert / mark-dirty.
    int engineIdx = -1;
    const int hIdx = hIndexFromParamId(parameterID, engineIdx);
    if (hIdx < 0) return;

    // Ignore writes we issued ourselves while loading a preset.
    if (loadingPreset) return;

    const char* presetParamId = (engineIdx == 1)
        ? ParamID::B_RECIPE_PRESET : ParamID::A_RECIPE_PRESET;
    const int currentPreset = juce::roundToInt(
        apvts.getRawParameterValue(presetParamId)->load());

    if (currentPreset >= 6 && currentPreset <= 8)
    {
        // On a Custom slot — accept the edit, refresh previousH so a
        // subsequent preset-load can revert to this state.
        readCurrentH(engineIdx, previousH[(size_t) engineIdx]);
        return;
    }

    // On a built-in. Find the first empty Custom slot.
    const int emptySlot = findFirstEmptyCustomSlot(engineIdx);
    if (emptySlot < 0)
    {
        // All customs full → revert this edit, fire wheel-lock flash.
        loadingPreset = true;
        writeHParamsRaw01(engineIdx, previousH[(size_t) engineIdx]);
        loadingPreset = false;

        juce::MessageManager::callAsync(
            [bc = &wheelLockBroadcaster]() { bc->sendChangeMessage(); });
        return;
    }

    // Remember which built-in we came from.
    lastBuiltInPreset[(size_t) engineIdx] = currentPreset;

    // Switch the preset to the empty slot. applyRecipePreset will fire as
    // a result of the choice change; since the slot is not `filled`, it
    // won't overwrite the just-edited H values.
    if (auto* presetParam = apvts.getParameter(presetParamId))
        presetParam->setValueNotifyingHost(presetParam->convertTo0to1((float) (6 + emptySlot)));
}
```

- [ ] **Step 5: Fill in saveRecipeSlot / clearRecipeSlot**

Find the stubs added in Task 1:

```cpp
void PhantomProcessor::saveRecipeSlot(int /*engineIdx*/, int /*slotIdx*/)
{
    // Implemented in Task 3 once the live-H reader is wired in.
    jassertfalse;
}

void PhantomProcessor::clearRecipeSlot(int /*engineIdx*/, int /*slotIdx*/)
{
    // Implemented in Task 3.
    jassertfalse;
}
```

Replace with:

```cpp
void PhantomProcessor::saveRecipeSlot(int engineIdx, int slotIdx)
{
    const int e = juce::jlimit(0, 1, engineIdx);
    const int s = juce::jlimit(0, 2, slotIdx);

    auto& slot = recipeSlots[(size_t) e][(size_t) s];
    readCurrentH(e, slot.savedH);
    slot.filled = true;
}

void PhantomProcessor::clearRecipeSlot(int engineIdx, int slotIdx)
{
    const int e = juce::jlimit(0, 1, engineIdx);
    const int s = juce::jlimit(0, 2, slotIdx);

    auto& slot = recipeSlots[(size_t) e][(size_t) s];
    slot.filled = false;
    slot.savedH = {};

    // If this slot was the active preset, fall back: first other filled
    // Custom → lastBuiltInPreset → 0 (Warm).
    const char* presetParamId = (e == 1)
        ? ParamID::B_RECIPE_PRESET : ParamID::A_RECIPE_PRESET;
    const int currentPreset = juce::roundToInt(
        apvts.getRawParameterValue(presetParamId)->load());

    if (currentPreset == 6 + s)
    {
        int fallback = -1;
        for (int other = 0; other < 3; ++other)
            if (other != s && recipeSlots[(size_t) e][(size_t) other].filled)
            {
                fallback = 6 + other;
                break;
            }
        if (fallback < 0)
            fallback = juce::jlimit(0, 5, lastBuiltInPreset[(size_t) e]);

        if (auto* presetParam = apvts.getParameter(presetParamId))
            presetParam->setValueNotifyingHost(
                presetParam->convertTo0to1((float) fallback));
    }
}
```

- [ ] **Step 6: Build**

```
cd "C:/Documents/NEw project/Kaigen Phantom/build" && "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/amd64/MSBuild.exe" KaigenPhantom.sln //p:Configuration=Release //p:Platform=x64 //m //nologo //v:minimal 2>&1 | tail -8
```

Expected: 0 errors. Behaviour change is now live: editing an H value on a built-in preset auto-switches to Cust 1 (first call) and saves silently fill that slot; further edits while on a Custom mark it logically dirty (UI in Task 6 surfaces this). Editing when all customs are filled and on a built-in does nothing (param doesn't change) and fires the wheel-lock broadcaster (no listener yet — that's Task 4).

- [ ] **Step 7: Commit**

```
cd "C:/Documents/NEw project/Kaigen Phantom" && git add Source/PluginProcessor.h Source/PluginProcessor.cpp && git commit -m "feat(processor): native preset-load + H auto-switch + recipe slot save/clear"
```

---

### Task 4: RecipeWheel lock-flash visual cue

**Files:**
- Modify: `Source/UI/widgets/RecipeWheel.h`
- Modify: `Source/UI/widgets/RecipeWheel.cpp`

Add a `triggerLockFlash()` method that paints a brief red-tinted overlay, and a `ChangeListener` subscription to `processor.getWheelLockBroadcaster()` that triggers it.

- [ ] **Step 1: Add ChangeListener + flash state to RecipeWheel.h**

Open `Source/UI/widgets/RecipeWheel.h`. Find:

```cpp
class RecipeWheel : public juce::Component, private juce::Timer
{
public:
    static constexpr int kSpokes            = 7;
    static constexpr int kParticlesPerSpoke = 20;
    static constexpr int kNumRings          = 6;

    /** Constructor takes apvts and the 7 parameter IDs in H2-H8 order. */
    RecipeWheel(juce::AudioProcessorValueTreeState& apvts,
                const std::array<juce::String, kSpokes>& paramIDs);
    ~RecipeWheel() override;
```

Replace with (adds ChangeListener base + the ctor signature for processor wheel-lock subscription):

```cpp
class PhantomProcessor;   // forward declare for the wheel-lock broadcaster

class RecipeWheel : public juce::Component,
                     private juce::Timer,
                     private juce::ChangeListener
{
public:
    static constexpr int kSpokes            = 7;
    static constexpr int kParticlesPerSpoke = 20;
    static constexpr int kNumRings          = 6;

    /** @param apvts      The plugin APVTS.
     *  @param paramIDs   7 H param IDs (H2..H8) in order.
     *  @param processor  Optional pointer for wheel-lock subscription;
     *                    pass nullptr if the wheel is used outside of the
     *                    main editor (no flash on lock-reject). */
    RecipeWheel(juce::AudioProcessorValueTreeState& apvts,
                const std::array<juce::String, kSpokes>& paramIDs,
                ::PhantomProcessor* processor = nullptr);
    ~RecipeWheel() override;
```

In the same class, find the existing private members (after `mouseExit` / etc.). Add:

```cpp
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    // Lock-flash overlay state. Set true by changeListenerCallback when the
    // processor rejects an H edit; cleared after ~120 ms by the existing
    // animation timer.
    bool  lockFlashActive  { false };
    int   lockFlashFramesRemaining { 0 };

    ::PhantomProcessor* processorRef { nullptr };
```

- [ ] **Step 2: Wire processor subscription in the ctor / dtor**

Open `Source/UI/widgets/RecipeWheel.cpp`. Find the existing constructor body (it should take apvts + paramIDs only — add the processor parameter and registration). Find:

```cpp
RecipeWheel::RecipeWheel(juce::AudioProcessorValueTreeState& apvts,
                          const std::array<juce::String, kSpokes>& paramIDs)
```

Replace the signature with:

```cpp
RecipeWheel::RecipeWheel(juce::AudioProcessorValueTreeState& apvts,
                          const std::array<juce::String, kSpokes>& paramIDs,
                          ::PhantomProcessor* processor)
```

At the end of the constructor body (just before the closing `}`), add:

```cpp
    processorRef = processor;
    if (processorRef != nullptr)
        processorRef->getWheelLockBroadcaster().addChangeListener(this);
```

Then in the destructor (`RecipeWheel::~RecipeWheel`), add at the top:

```cpp
    if (processorRef != nullptr)
        processorRef->getWheelLockBroadcaster().removeChangeListener(this);
```

You'll need `#include "../../PluginProcessor.h"` near the top of `RecipeWheel.cpp` for the broadcaster access — search the file for existing includes and add it if not already present.

- [ ] **Step 3: Implement the listener callback + flash hook into the existing timer**

In `Source/UI/widgets/RecipeWheel.cpp`, find the `timerCallback()` body. At its top (before existing animation work), add:

```cpp
    if (lockFlashFramesRemaining > 0)
    {
        --lockFlashFramesRemaining;
        if (lockFlashFramesRemaining == 0)
            lockFlashActive = false;
        repaint();
    }
```

Then anywhere after the function definitions, add:

```cpp
void RecipeWheel::changeListenerCallback(juce::ChangeBroadcaster* /*source*/)
{
    // Fired by PhantomProcessor::wheelLockBroadcaster when an H edit is
    // rejected because all Custom slots are filled. Flash for ~120 ms
    // (timer runs at 60 Hz → 7 frames).
    lockFlashActive          = true;
    lockFlashFramesRemaining = 7;
    repaint();
}
```

- [ ] **Step 4: Paint the flash overlay**

In `RecipeWheel::paint(juce::Graphics& g)`, at the very END of the function (just before its closing `}`), add:

```cpp
    if (lockFlashActive)
    {
        // Red-tinted overlay over the full wheel bounds. 0.18 alpha is
        // visible but doesn't obliterate the wheel underneath.
        g.setColour(juce::Colour::fromFloatRGBA(1.0f, 0.25f, 0.20f, 0.18f));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);
    }
```

- [ ] **Step 5: Update the caller (LeftPanel) to pass the processor**

Open `Source/UI/panels/LeftPanel.cpp`. Find the ctor initializer list with:

```cpp
      recipeWheel(apvts,
                  std::array<juce::String, 7>{
                      "a_recipe_h2", "a_recipe_h3", "a_recipe_h4",
                      "a_recipe_h5", "a_recipe_h6", "a_recipe_h7", "a_recipe_h8"
                  }),
```

`LeftPanel`'s ctor currently only takes `apvts` — but for this change we need the processor too. Check the existing ctor signature; if it already takes a processor (it does in some panels), pass it through. If not, this task needs LeftPanel to accept a processor too. The most likely state given other panels is that LeftPanel only has APVTS — verify by reading the header.

If LeftPanel needs the processor:

In `LeftPanel.h` find the constructor declaration and replace:

```cpp
    LeftPanel(juce::AudioProcessorValueTreeState& apvts);
```

with:

```cpp
    LeftPanel(juce::AudioProcessorValueTreeState& apvts, ::PhantomProcessor& processor);
```

In `LeftPanel.cpp` update the ctor signature and the initializer list:

```cpp
LeftPanel::LeftPanel(juce::AudioProcessorValueTreeState& a, ::PhantomProcessor& p)
    : apvts(a),
      recipeWheel(apvts,
                  std::array<juce::String, 7>{
                      "a_recipe_h2", "a_recipe_h3", "a_recipe_h4",
                      "a_recipe_h5", "a_recipe_h6", "a_recipe_h7", "a_recipe_h8"
                  },
                  &p),
```

Then in `NativePluginEditor.cpp` find the construction of `leftPanel`:

```cpp
      rightPanel(a, p), leftPanel(a), topBar(p, a), presetBrowser(p, a),
```

Update to:

```cpp
      rightPanel(a, p), leftPanel(a, p), topBar(p, a), presetBrowser(p, a),
```

(If LeftPanel already takes a processor, just pass it through to RecipeWheel — skip the signature changes.)

- [ ] **Step 6: Build**

```
cd "C:/Documents/NEw project/Kaigen Phantom/build" && "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/amd64/MSBuild.exe" KaigenPhantom.sln //p:Configuration=Release //p:Platform=x64 //m //nologo //v:minimal 2>&1 | tail -6
```

Expected: 0 errors. With all 3 customs filled, editing an H value on a built-in fires the flash (visible red tint for ~120 ms over the wheel).

- [ ] **Step 7: Commit**

```
cd "C:/Documents/NEw project/Kaigen Phantom" && git add Source/UI/widgets/RecipeWheel.h Source/UI/widgets/RecipeWheel.cpp Source/UI/panels/LeftPanel.h Source/UI/panels/LeftPanel.cpp Source/UI/NativePluginEditor.cpp && git commit -m "feat(ui-native): wheel lock-flash overlay on rejected H edit"
```

---

### Task 5: Restore per-engine recipe-preset selector retargeting

**Files:**
- Modify: `Source/UI/panels/LeftPanel.cpp`

The existing comment in `LeftPanel::setEnginePrefix` explicitly skips `recipePresetSelector` from per-engine sync. For this feature to work on engine B, that exclusion needs to be removed.

- [ ] **Step 1: Update setEnginePrefix to retarget the selector**

Open `Source/UI/panels/LeftPanel.cpp`. Find:

```cpp
void LeftPanel::setEnginePrefix(const juce::String& activePrefix,
                                  const juce::String& mirrorPrefix)
{
    for (auto* k : { &ghostAmountKnob, &crossoverKnob, &strengthKnob,
                     &lpfKnob, &hpfKnob })
        k->setEnginePrefix(activePrefix, mirrorPrefix);

    ghostModeToggle  .setEnginePrefix(activePrefix, mirrorPrefix);
    filterSlopeToggle.setEnginePrefix(activePrefix, mirrorPrefix);
    // recipePresetSelector is a_recipe_preset — UI-only, excluded from
    // per-engine sync at the audio level. Leaving it bound to A keeps
    // the visual highlight reflecting the engine A recipe preset; if we
    // want per-engine recipe-preset memory later, add it here.
}
```

Replace with:

```cpp
void LeftPanel::setEnginePrefix(const juce::String& activePrefix,
                                  const juce::String& mirrorPrefix)
{
    for (auto* k : { &ghostAmountKnob, &crossoverKnob, &strengthKnob,
                     &lpfKnob, &hpfKnob })
        k->setEnginePrefix(activePrefix, mirrorPrefix);

    ghostModeToggle  .setEnginePrefix(activePrefix, mirrorPrefix);
    filterSlopeToggle.setEnginePrefix(activePrefix, mirrorPrefix);
    recipePresetSelector.setEnginePrefix(activePrefix, mirrorPrefix);
}
```

- [ ] **Step 2: Build**

```
cd "C:/Documents/NEw project/Kaigen Phantom/build" && "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/amd64/MSBuild.exe" KaigenPhantom.sln //p:Configuration=Release //p:Platform=x64 //m //nologo //v:minimal 2>&1 | tail -6
```

Expected: 0 errors. Switching engine focus from A → B now retargets the recipe selector to `b_recipe_preset`; auto-switch logic for engine B now reads the right preset value.

- [ ] **Step 3: Commit**

```
cd "C:/Documents/NEw project/Kaigen Phantom" && git add Source/UI/panels/LeftPanel.cpp && git commit -m "fix(ui-native): retarget recipe preset selector on engine focus change"
```

---

### Task 6: RecipeSlotPills widget

**Files:**
- Create: `Source/UI/widgets/RecipeSlotPills.h`
- Create: `Source/UI/widgets/RecipeSlotPills.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the header**

Create `Source/UI/widgets/RecipeSlotPills.h`:

```cpp
// Source/UI/widgets/RecipeSlotPills.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

class PhantomProcessor;

namespace kaigen::phantom
{

/** Save + delete pills sitting beneath the WordSelector's Cust 1/2/3
 *  entries. Renders three rows (one per Custom slot); each row has two
 *  small circular dots:
 *
 *      [ save-dot ]  [ delete-dot ]
 *
 *  Save dot:
 *    - grey   when this slot is not the active preset
 *    - green  solid when active preset + filled + clean
 *    - red    blinking (~2 Hz) when active preset + (DIRTY or empty-with-pending-edits)
 *
 *  Delete dot:
 *    - grey   when slot is empty
 *    - red    when slot is filled (clickable)
 *
 *  Click handling delegates to PhantomProcessor::saveRecipeSlot /
 *  clearRecipeSlot. State is polled on a 4 Hz timer (blink). */
class RecipeSlotPills : public juce::Component, private juce::Timer
{
public:
    explicit RecipeSlotPills(PhantomProcessor& processor);
    ~RecipeSlotPills() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;

    /** Returns 0..2 for which row contains the point, or -1. */
    int rowAtY(int y) const noexcept;

    PhantomProcessor& processor;

    // Per-row geometry, computed in resized().
    std::array<juce::Rectangle<int>, 3> saveDots;
    std::array<juce::Rectangle<int>, 3> deleteDots;

    bool blinkPhase { false };   // toggled at 2 Hz by the 4 Hz timer

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecipeSlotPills)
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Write the .cpp**

Create `Source/UI/widgets/RecipeSlotPills.cpp`:

```cpp
// Source/UI/widgets/RecipeSlotPills.cpp
#include "RecipeSlotPills.h"
#include "../../PluginProcessor.h"
#include "../../EngineFocus.h"
#include "../../Parameters.h"
#include <cmath>

namespace kaigen::phantom
{

namespace
{
    constexpr int kDotSize     = 6;
    constexpr int kDotGap      = 4;
    constexpr int kRowH        = 12;
    constexpr int kRowGap      = 2;

    // Slot DIRTY check tolerance — H param values round-trip through
    // 0..100 floats; treat anything closer than 0.005 (0.5 %) as equal.
    constexpr float kDirtyEpsilon = 0.005f;
}

RecipeSlotPills::RecipeSlotPills(PhantomProcessor& p)
    : processor(p)
{
    startTimerHz(4);
}

RecipeSlotPills::~RecipeSlotPills()
{
    stopTimer();
}

void RecipeSlotPills::timerCallback()
{
    blinkPhase = ! blinkPhase;
    repaint();
}

void RecipeSlotPills::resized()
{
    // Three rows stacked vertically; pill pair centred horizontally in
    // each row. Width of the component should match the bottom row of
    // the WordSelector (the LeftPanel sets that).
    const int cx = getWidth() / 2;
    const int pairW = 2 * kDotSize + kDotGap;

    for (int row = 0; row < 3; ++row)
    {
        const int y    = row * (kRowH + kRowGap);
        const int rowY = y + (kRowH - kDotSize) / 2;
        const int saveX = cx - pairW / 2;
        const int delX  = saveX + kDotSize + kDotGap;
        saveDots  [(size_t) row] = { saveX, rowY, kDotSize, kDotSize };
        deleteDots[(size_t) row] = { delX,  rowY, kDotSize, kDotSize };
    }
}

int RecipeSlotPills::rowAtY(int y) const noexcept
{
    if (y < 0) return -1;
    const int row = y / (kRowH + kRowGap);
    return (row < 0 || row > 2) ? -1 : row;
}

void RecipeSlotPills::mouseDown(const juce::MouseEvent& e)
{
    const int row = rowAtY(e.y);
    if (row < 0) return;

    const int engineIdx = (processor.getEngineFocus().activeTab
                            == kaigen::phantom::ActiveTab::B) ? 1 : 0;

    if (saveDots[(size_t) row].contains(e.getPosition()))
    {
        // Save only meaningful when this slot is the active preset.
        const char* presetParamId = (engineIdx == 1)
            ? ParamID::B_RECIPE_PRESET : ParamID::A_RECIPE_PRESET;
        const int currentPreset = juce::roundToInt(
            processor.apvts.getRawParameterValue(presetParamId)->load());
        if (currentPreset == 6 + row)
            processor.saveRecipeSlot(engineIdx, row);
    }
    else if (deleteDots[(size_t) row].contains(e.getPosition()))
    {
        if (processor.getRecipeSlot(engineIdx, row).filled)
            processor.clearRecipeSlot(engineIdx, row);
    }
}

void RecipeSlotPills::paint(juce::Graphics& g)
{
    const int engineIdx = (processor.getEngineFocus().activeTab
                            == kaigen::phantom::ActiveTab::B) ? 1 : 0;

    const char* presetParamId = (engineIdx == 1)
        ? ParamID::B_RECIPE_PRESET : ParamID::A_RECIPE_PRESET;
    const int currentPreset = juce::roundToInt(
        processor.apvts.getRawParameterValue(presetParamId)->load());

    // Read live H values once per paint for the DIRTY comparison.
    std::array<float, 7> liveH {};
    static const char* aHIds[7] = {
        ParamID::A_RECIPE_H2, ParamID::A_RECIPE_H3, ParamID::A_RECIPE_H4,
        ParamID::A_RECIPE_H5, ParamID::A_RECIPE_H6, ParamID::A_RECIPE_H7,
        ParamID::A_RECIPE_H8
    };
    static const char* bHIds[7] = {
        ParamID::B_RECIPE_H2, ParamID::B_RECIPE_H3, ParamID::B_RECIPE_H4,
        ParamID::B_RECIPE_H5, ParamID::B_RECIPE_H6, ParamID::B_RECIPE_H7,
        ParamID::B_RECIPE_H8
    };
    const char* const* hIds = (engineIdx == 1) ? bHIds : aHIds;
    for (int h = 0; h < 7; ++h)
    {
        const auto* raw = processor.apvts.getRawParameterValue(hIds[h]);
        liveH[(size_t) h] = (raw != nullptr) ? raw->load() * 0.01f : 0.0f;
    }

    for (int row = 0; row < 3; ++row)
    {
        const auto& slot = processor.getRecipeSlot(engineIdx, row);
        const bool isCurrent = (currentPreset == 6 + row);

        // Save dot colour.
        juce::Colour saveColour;
        if (! isCurrent)
        {
            saveColour = juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 0.15f);  // grey
        }
        else if (! slot.filled)
        {
            // Empty + current → blinking red (calling for save).
            saveColour = blinkPhase
                ? juce::Colour::fromFloatRGBA(1.0f, 0.30f, 0.25f, 0.95f)
                : juce::Colour::fromFloatRGBA(1.0f, 0.30f, 0.25f, 0.25f);
        }
        else
        {
            // Filled + current → compare savedH to liveH for DIRTY.
            bool dirty = false;
            for (int h = 0; h < 7; ++h)
                if (std::abs(slot.savedH[(size_t) h] - liveH[(size_t) h]) > kDirtyEpsilon)
                { dirty = true; break; }

            if (dirty)
            {
                saveColour = blinkPhase
                    ? juce::Colour::fromFloatRGBA(1.0f, 0.30f, 0.25f, 0.95f)
                    : juce::Colour::fromFloatRGBA(1.0f, 0.30f, 0.25f, 0.25f);
            }
            else
            {
                saveColour = juce::Colour::fromFloatRGBA(0.30f, 0.95f, 0.40f, 0.90f);   // green
            }
        }

        g.setColour(saveColour);
        g.fillEllipse(saveDots[(size_t) row].toFloat());

        // Delete dot colour.
        const juce::Colour delColour = slot.filled
            ? juce::Colour::fromFloatRGBA(1.0f, 0.30f, 0.25f, 0.85f)   // red
            : juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 0.12f);    // grey
        g.setColour(delColour);
        g.fillEllipse(deleteDots[(size_t) row].toFloat());
    }
}

} // namespace kaigen::phantom
```

- [ ] **Step 3: Register in CMakeLists.txt**

Open `CMakeLists.txt`. Find the existing widget sources block (search for `LevelReadout.cpp` to find recent additions). Add `Source/UI/widgets/RecipeSlotPills.cpp` in the same group.

- [ ] **Step 4: Build**

```
cd "C:/Documents/NEw project/Kaigen Phantom/build" && cmake . > /dev/null 2>&1 && "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/amd64/MSBuild.exe" KaigenPhantom.sln //p:Configuration=Release //p:Platform=x64 //m //nologo //v:minimal 2>&1 | tail -8
```

Expected: 0 errors. Widget compiles; nothing yet instantiates it.

- [ ] **Step 5: Commit**

```
cd "C:/Documents/NEw project/Kaigen Phantom" && git add Source/UI/widgets/RecipeSlotPills.h Source/UI/widgets/RecipeSlotPills.cpp CMakeLists.txt && git commit -m "feat(ui-native): add RecipeSlotPills widget for Custom slot save/delete UI"
```

---

### Task 7: Wire RecipeSlotPills into LeftPanel

**Files:**
- Modify: `Source/UI/panels/LeftPanel.h`
- Modify: `Source/UI/panels/LeftPanel.cpp`

- [ ] **Step 1: Add include + member to LeftPanel.h**

Open `Source/UI/panels/LeftPanel.h`. Find the include for `WordSelector`:

```cpp
#include "../widgets/WordSelector.h"
```

Add right after:

```cpp
#include "../widgets/RecipeSlotPills.h"
```

Find the `recipePresetSelector` member declaration:

```cpp
    WordSelector recipePresetSelector;
```

Add right after:

```cpp
    RecipeSlotPills recipeSlotPills;
```

- [ ] **Step 2: Initialise in the constructor**

Open `Source/UI/panels/LeftPanel.cpp`. Find the ctor initializer list. After the existing `recipePresetSelector(apvts, ...)` line, add:

```cpp
      recipeSlotPills(p),
```

(`p` is the `PhantomProcessor` parameter added in Task 4 Step 5. If the ctor doesn't take `p` yet, complete Task 4 Step 5 first.)

Then in the constructor body, find:

```cpp
    addAndMakeVisible(recipePresetSelector);
```

Add right after:

```cpp
    addAndMakeVisible(recipeSlotPills);
```

- [ ] **Step 3: Position the pills strip in `resized()`**

In `LeftPanel::resized()`, find where `recipePresetSelector` gets `.setBounds(...)`. After that call, add a positioning block that lays the pills strip directly below the bottom row of the WordSelector (the row containing Cust 1/2/3 — which is the third row of three).

The WordSelector is laid out as 3 rows × 3 columns. Each row is `selector.getHeight() / 3` tall. The pills strip needs to sit beneath the WordSelector and align with its bottom-row column centres (positioning width = WordSelector width, then internal centring per row inside `RecipeSlotPills` handles per-row dot placement).

Add (after `recipePresetSelector.setBounds(...)`):

```cpp
    // Pills strip — directly below the WordSelector, full width so each
    // row of pills sits under the corresponding "Cust N" column.
    constexpr int kPillsStripH = 12 * 3 + 2 * 2 + 2;   // 3 rows × 12 + 2 gaps × 2 + 2 px pad
    auto pillsBounds = recipePresetSelector.getBounds()
                          .translated(0, recipePresetSelector.getHeight() + 2)
                          .withHeight(kPillsStripH);
    recipeSlotPills.setBounds(pillsBounds);
```

Note: the pills strip needs vertical space inside the panel. If the existing layout has no slack below the WordSelector, the strip may overlap whatever follows (likely the ghost-amount knob). If it does, the layout block immediately after `recipePresetSelector.setBounds(...)` should be nudged down by ~`kPillsStripH + 4` px. Inspect by building + visually checking; adjust by reducing the WordSelector's height or shifting the next widget.

- [ ] **Step 4: Build**

```
cd "C:/Documents/NEw project/Kaigen Phantom/build" && "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/amd64/MSBuild.exe" KaigenPhantom.sln //p:Configuration=Release //p:Platform=x64 //m //nologo //v:minimal 2>&1 | tail -8
```

Expected: 0 errors. Restart Ableton: under the Cust 1/2/3 row of the WordSelector you should see three rows of 2 small dots each (initially all grey since nothing is current/filled).

- [ ] **Step 5: Commit**

```
cd "C:/Documents/NEw project/Kaigen Phantom" && git add Source/UI/panels/LeftPanel.h Source/UI/panels/LeftPanel.cpp && git commit -m "feat(ui-native): wire RecipeSlotPills into LeftPanel below the WordSelector"
```

---

### Task 8: Final verification + listen-test handoff

- [ ] **Step 1: Confirm VST3 timestamps**

```
stat -c '%y' "C:/Documents/NEw project/Kaigen Phantom/build/KaigenPhantom_artefacts/Release/VST3/Kaigen Phantom.vst3/Contents/x86_64-win/Kaigen Phantom.vst3"
stat -c '%y' "C:/Users/kaislate/AppData/Local/Programs/Common/VST3/Kaigen Phantom.vst3/Contents/x86_64-win/Kaigen Phantom.vst3"
```

Expected: both within the last few minutes.

- [ ] **Step 2: Branch commit log**

```
cd "C:/Documents/NEw project/Kaigen Phantom" && git log --oneline integration/native-plus-reverb ^master | head -15
```

Expected: the seven feature commits from this plan plus the prior history.

- [ ] **Step 3: User manual verification in Ableton**

- Restart Ableton / rescan plugins.
- **Auto-switch flow:**
  - Default state: preset Warm. All three Custom slots show grey save + grey delete pills (none current, none filled).
  - Drag H4 spoke → preset auto-switches to Cust 1. Save pill on Cust 1's row goes blinking red.
  - Click the blinking save pill → goes solid green. Drag H4 again → goes blinking red (DIRTY).
  - Click delete pill on Cust 1 → preset reverts to Warm (last built-in), Cust 1 slot back to grey/grey.
- **All-customs-filled lock:**
  - Repeat above for Cust 1, Cust 2, Cust 3 (fill all three with `Save` clicks).
  - Switch preset to Warm. Drag H4 → wheel briefly tints red, H4 value does not change.
  - Click delete on any Custom → next H drag auto-switches into that now-empty slot.
- **Engine A vs B:**
  - Save a recipe to A's Cust 1. Switch to engine B. B's Cust 1 should still be empty (per-engine).
  - On engine B, drag H4 → B's Cust 1 fills with B's recipe (different from A's).
- **Project save/reload:**
  - Save the Ableton project. Close and reopen. Custom slot contents survive.

---

## Self-Review

**Spec coverage:** every spec section maps to a task.
- Storage (`RecipeSlot`, `recipeSlots`, `lastBuiltInPreset`) → Task 1 + Task 2.
- `applyRecipePreset` (built-in arrays + Custom slot data) → Task 3.
- H-change auto-switch / revert / mark-dirty → Task 3.
- Wheel-lock visual cue → Task 4.
- Per-engine preset selector retargeting → Task 5.
- `RecipeSlotPills` widget → Task 6.
- LeftPanel wiring → Task 7.
- Listen test → Task 8.

**Placeholder scan:** none. Steps include exact paths, full code, expected build outputs. The Task 4 Step 5 "verify by reading the header" note is a real verification step the implementer must do, not a placeholder — the LeftPanel constructor signature is the only thing I can't pre-determine without re-reading the header.

**Type consistency:**
- `RecipeSlot { bool filled; std::array<float,7> savedH; }` consistent across Tasks 1, 2, 3, 6.
- `getRecipeSlot(int engineIdx, int slotIdx)` returns `const RecipeSlot&` — Task 1 declares, Task 6 consumes.
- `saveRecipeSlot`/`clearRecipeSlot`/`findFirstEmptyCustomSlot` signatures consistent.
- `wheelLockBroadcaster` declared in Task 1, fired in Task 3 (`callAsync` + `sendChangeMessage`), subscribed in Task 4.
- Engine index convention `0 = A, 1 = B` consistent across tasks.

**Scope check:** one cohesive feature with 8 related tasks. Suitable for a single execution session.
