# Reverb source selector + VVV voicing — implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a 2-position toggle that routes the reverb input between the post-engine signal (current) and a phantom-only signal (just the synth contribution), plus four targeted DSP tweaks to push the reverb closer to Valhalla Vintage Verb "Concert Hall 1970s" character.

**Architecture:** A new global APVTS `bool` param (`reverb_source`) selects between two stereo source buffers. PhantomEngine writes the phantom contribution to a parallel scratch buffer per process call; DualEngineHost crossfades the two engines' phantom-only buffers with the same weights used for the main outputs. PluginProcessor picks the source per block. Four constant tweaks in SingleKnobReverb cover the voicing pass.

**Tech Stack:** JUCE 8 / C++ / APVTS / WebView2 frontend / EtchedToggle native widget.

**Spec:** `docs/superpowers/specs/2026-05-21-reverb-source-and-vvv-voicing-design.md`

**Type deviation from spec:** spec said `AudioParameterChoice` for `reverb_source`. Switched to `AudioParameterBool` with a custom value-to-string function (`Post` / `Phantom`) so it binds directly to the existing `EtchedToggle` widget (which takes a `boolParamId`) and the existing WebView `.tog` button pattern. Same UX, less plumbing.

---

### Task 1: Add `reverb_source` to the APVTS layout

**Files:**
- Modify: `Source/Parameters.h`

- [ ] **Step 1: Add the ID constant in the `dispersa::ParamID` namespace**

Open `Source/Parameters.h`. Locate `inline constexpr auto REVERB_MIX = "reverb_mix";` (line ~96).

Replace with:

```cpp
    inline constexpr auto REVERB_MIX    = "reverb_mix";
    inline constexpr auto REVERB_SOURCE = "reverb_source";
```

- [ ] **Step 2: Register in `getAllParameterIDs()`**

Find the existing `ids.push_back(ParamID::REVERB_MIX);` (line ~183). Replace the surrounding lines:

```cpp
    ids.push_back(ParamID::REVERB_MIX);

    return ids;
```

with:

```cpp
    ids.push_back(ParamID::REVERB_MIX);
    ids.push_back(ParamID::REVERB_SOURCE);

    return ids;
```

- [ ] **Step 3: Add the parameter to `createParameterLayout()`**

Find the existing `REVERB_MIX` APF push_back (line ~422). Replace:

```cpp
    params.push_back(std::make_unique<APF>(
        ParamID::REVERB_MIX, "Reverb",
        NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    return { params.begin(), params.end() };
```

with:

```cpp
    params.push_back(std::make_unique<APF>(
        ParamID::REVERB_MIX, "Reverb",
        NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    // Reverb routing: false (default) = reverb processes the full
    // post-engine signal (input + synth). true = reverb processes only
    // the synth contribution (phantomOut * ghostAmount), regardless of
    // ghost mode.
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamID::REVERB_SOURCE, "Reverb Source", false,
        juce::AudioParameterBoolAttributes()
            .withStringFromValueFunction([](bool b, int) {
                return b ? juce::String("Phantom") : juce::String("Post");
            })));

    return { params.begin(), params.end() };
```

- [ ] **Step 4: Verify build**

Run:

```
cd "C:/Documents/NEw project/Kaigen Phantom/build"
cmake .
MSBuild KaigenPhantom.sln /p:Configuration=Release /p:Platform=x64 /m /nologo /v:minimal
```

Expected: 0 errors. (CMake re-run picks up no new files but ensures the param is in.)

- [ ] **Step 5: Commit**

```
git add Source/Parameters.h
git commit -m "feat(reverb): add reverb_source bool param (Post/Phantom routing)"
```

---

### Task 2: PhantomEngine exposes a phantom-only output buffer

**Files:**
- Modify: `Source/Engines/PhantomEngine.h`
- Modify: `Source/Engines/PhantomEngine.cpp`

- [ ] **Step 1: Add `phantomOnlyBuf` member in the header**

Open `Source/Engines/PhantomEngine.h`. Find `juce::AudioBuffer<float> lowBuf;` (line ~101). Replace:

```cpp
    juce::AudioBuffer<float> lowBuf;
    juce::AudioBuffer<float> highBuf;
```

with:

```cpp
    juce::AudioBuffer<float> lowBuf;
    juce::AudioBuffer<float> highBuf;

    // Captures phantomOut * ghostAmount * outputGainLin per sample per
    // channel in the main process loop. Used by the reverb-source
    // selector to feed only the synth contribution into the reverb,
    // independently of ghost mode. Sized in prepare(), populated in
    // process(), exposed via getPhantomOnlyOutput().
    juce::AudioBuffer<float> phantomOnlyBuf;
```

- [ ] **Step 2: Add the public getter**

In the same header, find the existing `process()` declaration (line ~80). Replace:

```cpp
    // ─── Audio processing ────────────────────────────────────────────────
    void process(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>* sidechain = nullptr);
```

with:

```cpp
    /** Phantom-only output for the most recent process() call. Holds
     *  phantomOut * ghostAmount * outputGainLin per sample per channel.
     *  Same lifetime as DualEngineHost::getEngineAOutput: stable until
     *  the next process() call. */
    const juce::AudioBuffer<float>& getPhantomOnlyOutput() const noexcept { return phantomOnlyBuf; }

    // ─── Audio processing ────────────────────────────────────────────────
    void process(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>* sidechain = nullptr);
```

- [ ] **Step 3: Size + zero the buffer in `prepare()`**

Open `Source/Engines/PhantomEngine.cpp`. Find `void PhantomEngine::prepare(` and the existing `highBuf.setSize(...)` line. Immediately after that line, add:

```cpp
    phantomOnlyBuf.setSize(numChannels, blockSize, false, true, true);
    phantomOnlyBuf.clear();
```

- [ ] **Step 4: Clear in `reset()`**

Find `void PhantomEngine::reset()`. After the existing buffer clears (`lowBuf.clear();` etc), add:

```cpp
    phantomOnlyBuf.clear();
```

- [ ] **Step 5: Resize + clear per block in `process()`**

In `process()`, locate the loop `for (int ch = 0; ch < nCh; ++ch)` around line 331. Immediately before that loop, add:

```cpp
    // Phantom-only side buffer for the reverb-source selector — sized
    // to match the current block, zeroed so unwritten channels stay
    // clean.
    phantomOnlyBuf.setSize(nCh, n, false, false, true);
    phantomOnlyBuf.clear();
```

- [ ] **Step 6: Capture `phantomOut` into the side buffer**

In the same `process()`, locate (around line 411):

```cpp
            // Ghost mix (coefficients hoisted above).
            const float mixedLow = low[i] * dryLowCoef + phantomOut * ghostAmount;
            out[i] = (mixedLow + high[i] * highCoef) * outputGainLin;
```

Replace with:

```cpp
            // Ghost mix (coefficients hoisted above).
            const float phantomContribution = phantomOut * ghostAmount;
            const float mixedLow = low[i] * dryLowCoef + phantomContribution;
            out[i] = (mixedLow + high[i] * highCoef) * outputGainLin;

            // Side tap for the reverb-source selector — same gain stage
            // as the contribution appears in the main output, so the
            // reverb hears the synth at the same level whichever source
            // it's pointed at.
            phantomOnlyBuf.getWritePointer(ch)[i] = phantomContribution * outputGainLin;
```

- [ ] **Step 7: Verify build**

```
cd "C:/Documents/NEw project/Kaigen Phantom/build"
MSBuild KaigenPhantom.sln /p:Configuration=Release /p:Platform=x64 /m /nologo /v:minimal
```

Expected: 0 errors. Audio is unchanged (nothing consumes `phantomOnlyBuf` yet).

- [ ] **Step 8: Commit**

```
git add Source/Engines/PhantomEngine.h Source/Engines/PhantomEngine.cpp
git commit -m "feat(engine): expose per-engine phantom-only output buffer"
```

---

### Task 3: DualEngineHost aggregates phantom-only outputs

**Files:**
- Modify: `Source/DualEngineHost.h`
- Modify: `Source/DualEngineHost.cpp`

- [ ] **Step 1: Add `phantomOnlyMix` member**

Open `Source/DualEngineHost.h`. Find:

```cpp
    juce::AudioBuffer<float> aScratch;
    juce::AudioBuffer<float> bScratch;
```

Replace with:

```cpp
    juce::AudioBuffer<float> aScratch;
    juce::AudioBuffer<float> bScratch;

    // Crossfaded mix of engineA.getPhantomOnlyOutput() and
    // engineB.getPhantomOnlyOutput() using the same weights as the main
    // crossfader. Exposed to PluginProcessor for the reverb-source
    // selector. Stable until the next process() call.
    juce::AudioBuffer<float> phantomOnlyMix;
```

- [ ] **Step 2: Add the public getter**

In the same header, find the existing pair:

```cpp
    const juce::AudioBuffer<float>& getEngineAOutput() const noexcept { return aScratch; }
    const juce::AudioBuffer<float>& getEngineBOutput() const noexcept { return bScratch; }
```

Replace with:

```cpp
    const juce::AudioBuffer<float>& getEngineAOutput() const noexcept { return aScratch; }
    const juce::AudioBuffer<float>& getEngineBOutput() const noexcept { return bScratch; }

    /** Crossfaded phantom-only outputs of both engines. Used by the
     *  reverb-source selector to feed only the synth contribution
     *  into the reverb. Same lifetime as getEngineAOutput. */
    const juce::AudioBuffer<float>& getPhantomOnlyOutput() const noexcept { return phantomOnlyMix; }
```

- [ ] **Step 3: Size + clear in `prepareToPlay()`**

Open `Source/DualEngineHost.cpp`. Find `void DualEngineHost::prepareToPlay(` (line ~138). Replace:

```cpp
    bScratch.setSize(numChannels, blockSize, false, true, true);
    aScratch.setSize(numChannels, blockSize, false, true, true);
}
```

with:

```cpp
    bScratch.setSize(numChannels, blockSize, false, true, true);
    aScratch.setSize(numChannels, blockSize, false, true, true);
    phantomOnlyMix.setSize(numChannels, blockSize, false, true, true);
    phantomOnlyMix.clear();
}
```

- [ ] **Step 4: Clear in `reset()`**

Find `void DualEngineHost::reset()`. Replace:

```cpp
void DualEngineHost::reset()
{
    engineA.reset();
    engineB.reset();
}
```

with:

```cpp
void DualEngineHost::reset()
{
    engineA.reset();
    engineB.reset();
    phantomOnlyMix.clear();
}
```

- [ ] **Step 5: Crossfade phantom-only outputs at the end of `process()`**

Find the end of `process()` (line ~281). Replace:

```cpp
    // Crossfade into `buffer` (in place).
    crossfader.mix(aScratch, bypassA, bScratch, bypassB, buffer);
}
```

with:

```cpp
    // Crossfade into `buffer` (in place).
    crossfader.mix(aScratch, bypassA, bScratch, bypassB, buffer);

    // Same crossfade applied to the per-engine phantom-only side
    // buffers. Re-uses the crossfader state set just above, so the
    // weights match the main mix exactly — the reverb-source selector
    // hears the synth at the same relative gain as the main output.
    phantomOnlyMix.setSize(nCh, n, false, false, true);
    crossfader.mix(engineA.getPhantomOnlyOutput(), bypassA,
                   engineB.getPhantomOnlyOutput(), bypassB,
                   phantomOnlyMix);
}
```

- [ ] **Step 6: Verify build**

```
cd "C:/Documents/NEw project/Kaigen Phantom/build"
MSBuild KaigenPhantom.sln /p:Configuration=Release /p:Platform=x64 /m /nologo /v:minimal
```

Expected: 0 errors. Still no audio change.

- [ ] **Step 7: Commit**

```
git add Source/DualEngineHost.h Source/DualEngineHost.cpp
git commit -m "feat(host): aggregate phantom-only outputs via the morph crossfader"
```

---

### Task 4: PluginProcessor routes the reverb input based on `reverb_source`

**Files:**
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`

- [ ] **Step 1: Add the cached param pointer**

Open `Source/PluginProcessor.h`. Find `reverbMixParam` (line ~239). Replace:

```cpp
    std::atomic<float>*               reverbMixParam    { nullptr };
```

with:

```cpp
    std::atomic<float>*               reverbMixParam    { nullptr };
    std::atomic<float>*               reverbSourceParam { nullptr };  // 0 = Post, 1 = Phantom
```

- [ ] **Step 2: Cache the pointer in the constructor**

Open `Source/PluginProcessor.cpp`. Find the existing `reverbMixParam = apvts.getRawParameterValue(...)` in the constructor (line ~49). Replace:

```cpp
    reverbMixParam = apvts.getRawParameterValue(ParamID::REVERB_MIX);
```

with:

```cpp
    reverbMixParam    = apvts.getRawParameterValue(ParamID::REVERB_MIX);
    reverbSourceParam = apvts.getRawParameterValue(ParamID::REVERB_SOURCE);
```

- [ ] **Step 3: Switch the reverb-source buffer in `processBlock`**

Find the existing reverb-source copy in `processBlock` (line ~407). Replace:

```cpp
            // Copy the post-engine signal into the reverb scratch — this is
            // the wet input. The main buffer stays as the dry signal we'll
            // mix back into.
            for (int c = 0; c < nCh && c < reverbScratch.getNumChannels(); ++c)
                reverbScratch.copyFrom(c, 0, buffer, c, 0, n);
```

with:

```cpp
            // Select the wet input. By default (reverb_source = 0) the
            // reverb processes the full post-engine signal (input + synth).
            // When reverb_source = 1 it processes ONLY the synth
            // contribution — the dry input still passes to the output
            // through `buffer` unchanged, but the reverb tail is
            // generated from just the synth.
            const bool reverbOnPhantomOnly =
                reverbSourceParam && reverbSourceParam->load() > 0.5f;
            const juce::AudioBuffer<float>& reverbSource =
                reverbOnPhantomOnly
                    ? dualEngineHost.getPhantomOnlyOutput()
                    : buffer;

            // Copy the chosen source into the reverb scratch — the main
            // buffer stays as the dry signal we'll mix back into.
            for (int c = 0; c < nCh && c < reverbScratch.getNumChannels(); ++c)
                reverbScratch.copyFrom(c, 0, reverbSource,
                                       juce::jmin(c, reverbSource.getNumChannels() - 1),
                                       0, n);
```

- [ ] **Step 4: Verify build**

```
cd "C:/Documents/NEw project/Kaigen Phantom/build"
MSBuild KaigenPhantom.sln /p:Configuration=Release /p:Platform=x64 /m /nologo /v:minimal
```

Expected: 0 errors. With `reverb_source` default false, behaviour is unchanged.

- [ ] **Step 5: Commit**

```
git add Source/PluginProcessor.h Source/PluginProcessor.cpp
git commit -m "feat(reverb): route reverb input based on reverb_source param"
```

---

### Task 5: Expose `reverb_source` through the WebView UI

**Files:**
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`
- Modify: `Source/WebUI/juce-frontend.js`
- Modify: `Source/WebUI/index.html`
- Modify: `Source/WebUI/phantom.js`

- [ ] **Step 1: Add the `WebToggleRelay` to `PluginEditor.h`**

Open `Source/PluginEditor.h`. Find `reverbMixRelay` (line ~129). Replace:

```cpp
    // Reverb send — global. Single-knob algorithmic reverb on the post-engine
    // signal; same character for both engines.
    juce::WebSliderRelay reverbMixRelay              { "reverb_mix" };
```

with:

```cpp
    // Reverb send — global. Single-knob algorithmic reverb on the post-engine
    // signal; same character for both engines.
    juce::WebSliderRelay reverbMixRelay              { "reverb_mix" };

    // Reverb source — global bool. false = Post-Engine (default),
    // true = Phantom Only (reverb hears synth contribution only).
    juce::WebToggleButtonRelay reverbSourceRelay     { "reverb_source" };
```

(If the project uses `juce::WebToggleRelay` instead of `juce::WebToggleButtonRelay`, match the existing toggle relays in this file — search for `inputGainAutoRelay` or `bypassRelay` to see the local naming.)

- [ ] **Step 2: Register the toggle relay in `PluginEditor.cpp`**

Open `Source/PluginEditor.cpp`. Find the existing toggle-relay registration. There are two integration points:

(a) The `.withOptionsFrom(...)` chain on the WebBrowserComponent options builder. Search for `inputGainAutoRelay` to find existing toggle registrations. Add right after:

```cpp
        .withOptionsFrom(self.reverbSourceRelay)
```

(b) The attachment construction. Search for `ButtonParameterAttachment` to find the local idiom. The existing pattern is likely either a per-attachment `push_back` or a struct-driven loop. Add:

```cpp
    toggleAttachments.push_back(std::make_unique<juce::ButtonParameterAttachment>(
        *dynamic_cast<juce::AudioParameterBool*>(processor.apvts.getParameter(ParamID::REVERB_SOURCE)),
        reverbSourceRelay, nullptr));
```

following the local convention (variable names may differ).

- [ ] **Step 3: Add `reverb_source` to `KAIGEN_GLOBAL_PARAMS`**

Open `Source/WebUI/juce-frontend.js`. Find (line ~599):

```javascript
  const KAIGEN_GLOBAL_PARAMS = new Set([
    'bypass', 'input_gain', 'input_gain_auto', 'advanced_open',
    'morph_amount', 'morph_curve', 'morph_a_level_db',
    'morph_b_level_db', 'morph_bypass_idle_engine',
    'macro1', 'macro2', 'macro3', 'macro4',
    'reverb_mix'
  ]);
```

Replace with:

```javascript
  const KAIGEN_GLOBAL_PARAMS = new Set([
    'bypass', 'input_gain', 'input_gain_auto', 'advanced_open',
    'morph_amount', 'morph_curve', 'morph_a_level_db',
    'morph_b_level_db', 'morph_bypass_idle_engine',
    'macro1', 'macro2', 'macro3', 'macro4',
    'reverb_mix', 'reverb_source'
  ]);
```

- [ ] **Step 4: Add the toggle button to index.html**

Open `Source/WebUI/index.html`. Find the existing reverb knob (line ~262):

```html
              <phantom-knob data-param="input_gain" size="medium" label="In" default-value="0.333" id="input-gain-knob"></phantom-knob>
              <phantom-knob data-param="reverb_mix" size="medium" label="Reverb" default-value="0"></phantom-knob>
              <phantom-knob data-param="output_gain" size="medium" label="Out" default-value="0.667"></phantom-knob>
              <canvas id="meterOut" class="io-meter levels-meter"></canvas>
```

Replace with:

```html
              <phantom-knob data-param="input_gain" size="medium" label="In" default-value="0.333" id="input-gain-knob"></phantom-knob>
              <phantom-knob data-param="reverb_mix" size="medium" label="Reverb" default-value="0"></phantom-knob>
              <button class="tog" id="reverb-source-btn" data-param="reverb_source" title="Reverb source: Post-engine signal (off) or Phantom synth only (on)">PHNTM</button>
              <phantom-knob data-param="output_gain" size="medium" label="Out" default-value="0.667"></phantom-knob>
              <canvas id="meterOut" class="io-meter levels-meter"></canvas>
```

- [ ] **Step 5: Wire the button in phantom.js**

Open `Source/WebUI/phantom.js`. Find the existing `// ── Bypass toggle` block near the top. Add a new block after the existing toggle wirings:

```javascript
// ── Reverb source toggle ─────────────────────────────────────────────────
const reverbSourceState = window.Juce.getToggleStateLogical?.("reverb_source");
const reverbSourceBtn   = document.getElementById("reverb-source-btn");
if (reverbSourceState && reverbSourceBtn) {
    reverbSourceBtn.addEventListener("click", () => {
        reverbSourceState.setValue(!reverbSourceState.getValue());
    });
    reverbSourceState.valueChangedEvent.addListener(() => {
        reverbSourceBtn.classList.toggle("active", reverbSourceState.getValue());
    });
    // Initial visual state.
    reverbSourceBtn.classList.toggle("active", reverbSourceState.getValue());
}
```

- [ ] **Step 6: Verify build**

```
cd "C:/Documents/NEw project/Kaigen Phantom/build"
MSBuild KaigenPhantom.sln /p:Configuration=Release /p:Platform=x64 /m /nologo /v:minimal
```

Expected: 0 errors. WebView shows a PHNTM toggle next to the Reverb knob; clicking lights the button and switches the reverb source.

- [ ] **Step 7: Commit**

```
git add Source/PluginEditor.h Source/PluginEditor.cpp Source/WebUI/juce-frontend.js Source/WebUI/index.html Source/WebUI/phantom.js
git commit -m "feat(ui-web): expose reverb_source toggle button in the WebView"
```

---

### Task 6: Add the EtchedToggle to the native UI

**Files:**
- Modify: `Source/UI/panels/RightPanel.h`
- Modify: `Source/UI/panels/RightPanel.cpp`

- [ ] **Step 1: Add the toggle member**

Open `Source/UI/panels/RightPanel.h`. Find `reverbKnob` (line ~57). Replace:

```cpp
    // Reverb send — global (single-knob algorithmic reverb). Sits between
    // Trim and Out in the Levels card to mirror the WebView layout.
    PhantomKnob reverbKnob;
```

with:

```cpp
    // Reverb send — global (single-knob algorithmic reverb). Sits between
    // Trim and Out in the Levels card to mirror the WebView layout.
    PhantomKnob reverbKnob;

    // Reverb-source toggle — global bool. Lit (active) = Phantom Only;
    // unlit = Post-Engine. Sits beneath the Reverb knob in the Levels card.
    EtchedToggle reverbSourceToggle;
```

- [ ] **Step 2: Initialize + addAndMakeVisible**

Open `Source/UI/panels/RightPanel.cpp`. Find the constructor initializer list with `reverbKnob` (line ~46). Replace:

```cpp
      inGainKnob    (apvts, "input_gain",            PhantomKnob::Size::Medium, "PKE"),
      outGainKnob   (apvts, "a_output_gain",         PhantomKnob::Size::Medium, "Out"),
      reverbKnob    (apvts, "reverb_mix",            PhantomKnob::Size::Small,  "Reverb"),
```

with:

```cpp
      inGainKnob         (apvts, "input_gain",       PhantomKnob::Size::Medium, "PKE"),
      outGainKnob        (apvts, "a_output_gain",    PhantomKnob::Size::Medium, "Out"),
      reverbKnob         (apvts, "reverb_mix",       PhantomKnob::Size::Small,  "Reverb"),
      reverbSourceToggle (apvts, "reverb_source",    "PHNTM"),
```

Find `addAndMakeVisible(reverbKnob);` (line ~123). Replace with:

```cpp
    addAndMakeVisible(reverbKnob);
    addAndMakeVisible(reverbSourceToggle);
```

- [ ] **Step 3: Lay out the toggle in `resized()`**

Find the Levels block (search for `// --- Levels: In + Trim + Reverb + Out`, line ~258). Replace:

```cpp
    const int outGainX = reverbX + kSmallSide - kMedSmallOverlap;
    outGainKnob.setBounds(outGainX, knobRowTop, kMedium, kMedium);
    // Right meter — mirror of the left, outside Out's right shadow halo.
    outMeter.setBounds(juce::jmin(levelsCardRight - kMeterEdgeMargin - kMeterW,
                                   outGainX + kMedium + kMeterKnobGap),
                        knobRowTop + (kMedium - 90) / 2, kMeterW, 90);

    levelsCardBounds = juce::Rectangle<int>(levelsCardX, cardTop,
                                             levelsCardW, cardHeight);
```

with:

```cpp
    const int outGainX = reverbX + kSmallSide - kMedSmallOverlap;
    outGainKnob.setBounds(outGainX, knobRowTop, kMedium, kMedium);
    // Right meter — mirror of the left, outside Out's right shadow halo.
    outMeter.setBounds(juce::jmin(levelsCardRight - kMeterEdgeMargin - kMeterW,
                                   outGainX + kMedium + kMeterKnobGap),
                        knobRowTop + (kMedium - 90) / 2, kMeterW, 90);

    // Reverb-source toggle — small etched word centred under the Reverb
    // knob, in the empty space below the small-knob row inside the
    // Levels card. ~50 x 14 px to match the Auto-gain toggle's feel.
    constexpr int kReverbToggleW = 50;
    constexpr int kReverbToggleH = 14;
    const int reverbToggleX = reverbX + (kSmallSide - kReverbToggleW) / 2;
    const int reverbToggleY = smallY + kSmallSide + 2;
    reverbSourceToggle.setBounds(reverbToggleX, reverbToggleY,
                                  kReverbToggleW, kReverbToggleH);

    levelsCardBounds = juce::Rectangle<int>(levelsCardX, cardTop,
                                             levelsCardW, cardHeight);
```

- [ ] **Step 4: Verify build**

```
cd "C:/Documents/NEw project/Kaigen Phantom/build"
MSBuild KaigenPhantom.sln /p:Configuration=Release /p:Platform=x64 /m /nologo /v:minimal
```

Expected: 0 errors. Native UI shows a small PHNTM toggle beneath the Reverb knob.

- [ ] **Step 5: Commit**

```
git add Source/UI/panels/RightPanel.h Source/UI/panels/RightPanel.cpp
git commit -m "feat(ui-native): add reverb_source EtchedToggle in the Levels card"
```

---

### Task 7: VVV voicing pass (four constant tweaks, one commit)

**Files:**
- Modify: `Source/DSP/SingleKnobReverb.h`
- Modify: `Source/DSP/SingleKnobReverb.cpp`

All four tweaks ship as one commit. User A/B tests in the DAW.

- [ ] **Step 1: Bump early diffusion 4 → 6 stages (header)**

Open `Source/DSP/SingleKnobReverb.h`. Find:

```cpp
    static constexpr int kNumEarlyAPs = 4;
    static constexpr int kEarlyAPSamples[kNumEarlyAPs] = { 113, 197, 313, 421 };
```

Replace with:

```cpp
    static constexpr int kNumEarlyAPs = 6;
    static constexpr int kEarlyAPSamples[kNumEarlyAPs] = { 113, 197, 313, 421, 571, 691 };
```

- [ ] **Step 2: Drop pretank LPF 10 kHz → 8 kHz (header)**

In the same file, find:

```cpp
    static constexpr float kPretankLpfHz = 10000.0f;
```

Replace with:

```cpp
    static constexpr float kPretankLpfHz = 8000.0f;
```

- [ ] **Step 3: Add output low-shelf state (header)**

Find the existing output-filter members (line ~114):

```cpp
    // Output filters (per channel).
    float outLpfL { 0.0f }, outLpfR { 0.0f };
    float outLpfCoef { 0.0f };
    float outHpfStateL { 0.0f }, outHpfStateR { 0.0f };
    float outHpfPrevInL { 0.0f },  outHpfPrevInR { 0.0f };
    float outHpfCoef    { 0.0f };
```

Replace with:

```cpp
    // Output filters (per channel).
    float outLpfL { 0.0f }, outLpfR { 0.0f };
    float outLpfCoef { 0.0f };
    float outHpfStateL { 0.0f }, outHpfStateR { 0.0f };
    float outHpfPrevInL { 0.0f },  outHpfPrevInR { 0.0f };
    float outHpfCoef    { 0.0f };

    // Static output low-shelf — VVV "bass mult" style coloration applied
    // post-tank, post-band-limit. One-pole LPF state + a fractional
    // add-back gives ~+4 dB below ~250 Hz, flat above. Always on;
    // stability is irrelevant because it sits outside the feedback loop.
    float outShelfL { 0.0f }, outShelfR { 0.0f };
    float outShelfCoef { 0.0f };
    static constexpr float kOutShelfHz  = 250.0f;
    static constexpr float kOutShelfMul = 0.585f;  // ~+4 dB at DC (1 + 0.585 = 1.585)
```

- [ ] **Step 4: Bump mod depth 2 ms → 3.5 ms (.cpp)**

Open `Source/DSP/SingleKnobReverb.cpp`. Find:

```cpp
    constexpr float kModDepthMs = 2.0f;
```

Replace with:

```cpp
    // 2 ms → 3.5 ms — lusher, more VVV-like chorussing in the tank.
    // Still well under the shortest delay-line headroom (shortest line
    // ~40 ms at 44.1 kHz; 3.5 ms uses ~9 % of headroom and
    // prepareDelayLines reserves modDepthSamples * 1.5 + 8 samples
    // per line, which still fits).
    constexpr float kModDepthMs = 3.5f;
```

- [ ] **Step 5: Initialize shelf coefficient + state in `prepareFilters()`**

Find `void SingleKnobReverb::prepareFilters()`. Replace:

```cpp
void SingleKnobReverb::prepareFilters()
{
    pretankLpfState = 0.0f;
    pretankLpfCoef = onePoleCoef(kPretankLpfHz, sr);

    outLpfL = outLpfR = 0.0f;
    outLpfCoef = onePoleCoef(kOutLpfHz, sr);

    outHpfStateL = outHpfStateR = 0.0f;
    outHpfPrevInL = outHpfPrevInR = 0.0f;
    // One-pole DC blocker coefficient: pole at 1 - 2*pi*fc/fs.
    outHpfCoef = std::exp(-juce::MathConstants<float>::twoPi * kOutHpfHz / (float) sr);
}
```

with:

```cpp
void SingleKnobReverb::prepareFilters()
{
    pretankLpfState = 0.0f;
    pretankLpfCoef = onePoleCoef(kPretankLpfHz, sr);

    outLpfL = outLpfR = 0.0f;
    outLpfCoef = onePoleCoef(kOutLpfHz, sr);

    outHpfStateL = outHpfStateR = 0.0f;
    outHpfPrevInL = outHpfPrevInR = 0.0f;
    // One-pole DC blocker coefficient: pole at 1 - 2*pi*fc/fs.
    outHpfCoef = std::exp(-juce::MathConstants<float>::twoPi * kOutHpfHz / (float) sr);

    // Output low-shelf.
    outShelfL = outShelfR = 0.0f;
    outShelfCoef = onePoleCoef(kOutShelfHz, sr);
}
```

- [ ] **Step 6: Clear shelf state in `reset()`**

Find `void SingleKnobReverb::reset()`. Replace:

```cpp
    pretankLpfState = 0.0f;
    outLpfL = outLpfR = 0.0f;
    outHpfStateL = outHpfStateR = 0.0f;
    outHpfPrevInL = outHpfPrevInR = 0.0f;
}
```

with:

```cpp
    pretankLpfState = 0.0f;
    outLpfL = outLpfR = 0.0f;
    outHpfStateL = outHpfStateR = 0.0f;
    outHpfPrevInL = outHpfPrevInR = 0.0f;
    outShelfL = outShelfR = 0.0f;
}
```

- [ ] **Step 7: Apply the shelf in `process()` after the output LPF**

In `process()`, find:

```cpp
        // LPF (one-pole towards input).
        outLpfL += outLpfCoef * (wetL - outLpfL);
        outLpfR += outLpfCoef * (wetR - outLpfR);

        L[i] = outLpfL;
        if (nCh > 1) R[i] = outLpfR;
```

Replace with:

```cpp
        // LPF (one-pole towards input).
        outLpfL += outLpfCoef * (wetL - outLpfL);
        outLpfR += outLpfCoef * (wetR - outLpfR);

        // Output low-shelf — same one-pole-LPF-plus-add-back structure
        // as the in-loop bass shelf, but post-tank where stability
        // isn't a concern. Track the low band, add back kOutShelfMul
        // * state for an effective +4 dB bump below ~250 Hz with no
        // boost above it.
        outShelfL += outShelfCoef * (outLpfL - outShelfL);
        outShelfR += outShelfCoef * (outLpfR - outShelfR);
        const float shelfedL = outLpfL + kOutShelfMul * outShelfL;
        const float shelfedR = outLpfR + kOutShelfMul * outShelfR;

        L[i] = shelfedL;
        if (nCh > 1) R[i] = shelfedR;
```

- [ ] **Step 8: Verify build**

```
cd "C:/Documents/NEw project/Kaigen Phantom/build"
MSBuild KaigenPhantom.sln /p:Configuration=Release /p:Platform=x64 /m /nologo /v:minimal
```

Expected: 0 errors. Reverb should sound lusher with more bass body and slightly darker tank input.

- [ ] **Step 9: Commit**

```
git add Source/DSP/SingleKnobReverb.h Source/DSP/SingleKnobReverb.cpp
git commit -m "tune(reverb): push voicing closer to VVV Concert Hall 1970s"
```

---

### Task 8: Final verification + listen test

- [ ] **Step 1: Confirm fresh VST3 timestamps**

```
stat -c '%y' "C:/Documents/NEw project/Kaigen Phantom/build/KaigenPhantom_artefacts/Release/VST3/Kaigen Phantom.vst3/Contents/x86_64-win/Kaigen Phantom.vst3"
stat -c '%y' "C:/Users/kaislate/AppData/Local/Programs/Common/VST3/Kaigen Phantom.vst3/Contents/x86_64-win/Kaigen Phantom.vst3"
```

Expected: both within the last few minutes.

- [ ] **Step 2: User listen-test in Ableton**

Manual verification, no automated check.

- Restart Ableton / rescan plugins.
- Load Kaigen Phantom; push Reverb knob to ~50 %.
- Toggle PHNTM in the Levels card on/off.
  - PHNTM off: reverb hears full post-engine signal (current behaviour).
  - PHNTM on: reverb hears only the synth contribution; dry input passes through unaffected.
- Compare voicing against the prior build (or VVV "Concert Hall 1970s" 4 s preset side-by-side). Expect lusher chorussing, slightly darker tank input, more bass body.

- [ ] **Step 3: Branch summary check**

```
cd "C:/Documents/NEw project/Kaigen Phantom"
git log --oneline integration/native-plus-reverb ^master | head -20
```

Expected: the seven new commits from this plan plus the prior native+reverb integration commits.

---

## Self-Review

**Spec coverage:** every spec section maps to a task.
- New APVTS param → Task 1
- PhantomEngine phantom-only buf + accessor → Task 2
- DualEngineHost crossfaded mix + accessor → Task 3
- PluginProcessor routing → Task 4
- WebView UI (relay + KAIGEN_GLOBAL_PARAMS + HTML + JS) → Task 5
- Native UI (RightPanel EtchedToggle) → Task 6
- Voicing tweaks (mod depth / diffusion / pretank / output shelf) → Task 7
- Build verification → Task 8

**Placeholder scan:** none. All steps have exact paths and exact code. The two "match the local convention" notes in Task 5 Step 2 are pointed at specific existing patterns to follow (the WebView toggle relay registration idiom varies between projects — instructing the implementer to match what's already in the file rather than dictating a possibly-wrong pattern).

**Type consistency:** `reverb_source` is `AudioParameterBool` throughout (Task 1 declaration; Task 4 read via `> 0.5f`; Task 5 `WebToggleButtonRelay` / `ButtonParameterAttachment`; Task 6 `EtchedToggle` bool binding). `phantomOnlyBuf` / `phantomOnlyMix` types and accessors match across the engine and host.

**Scope check:** one branch, one feature pair, all tasks chain cleanly. Suitable for a single implementation session.
