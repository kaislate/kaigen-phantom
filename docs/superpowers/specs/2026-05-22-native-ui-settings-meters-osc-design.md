# Native UI: settings overlay, binaural quick toggle, meter relocation, oscilloscope Auto, collapse-Advanced reflow — design

**Date:** 2026-05-22
**Branch:** `integration/native-plus-reverb`
**Status:** approved (in-conversation)

## Goal

Six related native-UI improvements, all in `Source/UI/` and `Source/UI/panels/`:

1. **Settings overlay** — port the WebView's modal settings panel to the native editor (Binaural / Envelope Source / MIDI Triggering sections).
2. **Top-bar gear button** — opens the settings overlay.
3. **Binaural quick toggle** — small button above the Advanced row's Width mini-knob, cycles `a_binaural_mode` between 0 (Off) and 1 (Spread).
4. **Meter relocation** — remove `inMeter` / `outMeter` from the Levels card; add a fixed-size meter column in the bottom-right of the visualizer area.
5. **Oscilloscope Auto button** — small overlay button in the oscilloscope's bottom-right corner, toggles `autoScale` (editor-local state, matches WebView behaviour).
6. **Collapse-Advanced layout reflow** — when Advanced collapses, only the spectrum's height changes; oscilloscope and meters stay anchored to the bottom; oscilloscope moves *below* the spectrum so the bottom row is genuinely fixed.

## Architectural shape

Final visualizer area layout (top → bottom):

```
┌─────────────────────────────────────┐
│  Knob row (Levels, HE, Stereo)      │
├─────────────────────────────────────┤
│  Advanced toggle                    │
│  Advanced mini-knobs (when expanded)│
├─────────────────────────────────────┤
│                                     │
│  Spectrum   (VARIABLE HEIGHT)       │  ← only this resizes
│                                     │
├──────────────────────────┬──────────┤
│  Oscilloscope (fixed)    │ Meters   │  ← whole bottom row never moves
└──────────────────────────┴──────────┘
```

Spectrum is bottom-anchored. Its bottom edge sits at `(visualizerAreaBottom − oscRowHeight − rowGap)`. Its top edge is `(advancedRowBottom + topGap)`. Height varies with `advancedExpanded`. Oscilloscope and meter column are both anchored to `visualizerAreaBottom` with fixed height; their tops are computed from that bottom.

## Detailed designs

### 1. SettingsOverlay component

**File:** new `Source/UI/panels/SettingsOverlay.h` + `.cpp`.

Mirrors `PresetBrowser` / `MatrixView` as a modal overlay component. Constructed in `NativePluginEditor` and added with `setVisible(false)`. Toggled by the gear button.

Structure (matches WebView `index.html:436-470`):

- **Settings panel card** centred in the editor, ~480 × 380 px, neumorphic card style matching `PresetBrowser`'s card.
- **Header row:** "SETTINGS" etched label + close × button.
- **Three sections**, each with an etched section label and a content row:
  - **Binaural:** `WordSelector` for `a_binaural_mode` with choices "Off" / "Spread" (the existing 3-choice param's "Voice-Split" is stubbed, so the selector only exposes the two working states by skipping index 2). Plus a `PhantomKnob` for `a_binaural_width` (Medium size).
  - **Envelope Source:** `WordSelector` for `a_env_source` with choices "Input" / "Sidechain".
  - **MIDI Triggering:** two `EtchedToggle` widgets for `a_midi_trigger_enabled` and `a_midi_gate_release`, labeled "Trigger" and "Gate Release".

**Mutual exclusion:** opening the overlay dismisses the PresetBrowser and MatrixView (using the same pattern `NativePluginEditor` already uses for those two — adapt the existing `topBar.getPresetSelector().onBrowseRequested` lambda's mutex code).

**Dismiss:** click on backdrop (anywhere outside the card) or close × button.

**Param surface:** all three sections bind to existing per-engine `a_*` params via the standard JUCE attachment pattern. `binaural_mode` is `AudioParameterChoice` so the `WordSelector` works directly. `env_source` is also `AudioParameterChoice`. `midi_trigger_enabled` and `midi_gate_release` are `AudioParameterBool` so `EtchedToggle` works.

**Engine-prefix retargeting:** the overlay's widgets are per-engine, so `NativePluginEditor::applyEngineFocus` needs to call `settingsOverlay.setEnginePrefix(activePrefix, mirrorPrefix)` alongside the existing left/right panel calls. The overlay's `setEnginePrefix` retargets the four widgets that have engine-prefix support (skip ones whose param isn't per-engine, which in this case is none — all four are per-engine).

### 2. Top-bar gear button

**Files:** modify `Source/UI/panels/TopBar.h` + `.cpp`.

Add a new `HeaderButton settingsButton { "⚙" }` (gear glyph) member. Place it in the top bar's right cluster, between the bypass button and the existing matrix/advanced controls. Expose an `onSettingsRequested` `std::function<void()>` callback.

In `NativePluginEditor`, wire `topBar.onSettingsRequested = [this] { ... }` to show the settings overlay with the same mutual-exclusion logic used for PresetBrowser. Symmetric: opening settings hides browser + dropdown + matrix.

### 3. Binaural quick toggle

**File:** new `Source/UI/widgets/ChoiceToggle.h` + `.cpp`. (Small new widget — `EtchedToggle` only handles bools; we need one that toggles a 2-value choice param between index 0 and 1.)

Implementation mirrors `EtchedToggle` but uses `juce::ParameterAttachment` (generic) rather than `juce::ButtonParameterAttachment`. On click: read current value, set to `(currentInt == 0) ? 1.0f : 0.0f` via the attachment's `setValueAsCompleteGesture`. On param-value-changed: update `isActive` and repaint. Lit when value rounds to 1, unlit at 0.

Constructor: `ChoiceToggle(apvts, paramId, label, onChoiceIndex = 1)` — `onChoiceIndex` lets it work for params where "on" isn't necessarily index 1. We pass 1 here.

**Wiring in RightPanel:**

- New member: `ChoiceToggle binauralToggle;` directly after the `miniKnobs` array declaration.
- Initialize: `binauralToggle(apvts, "a_binaural_mode", "BIN")`.
- `addAndMakeVisible(binauralToggle);` near the mini-knob addition.
- Layout: in `resized()`'s advanced-row block, place a small ~40 × 12 px toggle directly above the position computed for the "Width" mini-knob. The 14-px gap between the advanced toggle button and the mini-knob row currently has nothing in it — the binaural toggle fits there above Width without moving any mini-knob.

**Engine-prefix retargeting:** `binauralToggle` is per-engine; add it to the retarget list in `RightPanel::setEnginePrefix` alongside the existing knobs.

### 4. Meter relocation

**Files:** modify `Source/UI/panels/RightPanel.h` + `.cpp`.

**Remove from Levels card:** delete the `inMeter.setBounds(...)` and `outMeter.setBounds(...)` calls in the Levels block in `resized()`. The meters stay as members; just their layout call moves.

**Add new bottom-right meter column:** in `resized()`, after the visualizer/spectrum bounds are computed:

```cpp
constexpr int kMeterColWidth     = 56;   // wide enough for both meters side by side with a small gap
constexpr int kMeterColPad       = 4;
constexpr int kMeterHeight       = 90;
constexpr int kIndividualMeterW  = 24;
constexpr int kMeterColGap       = (kMeterColWidth - 2 * kIndividualMeterW) / 3;

// Meter column anchored to visualizer-area bottom-right.
const int meterColX  = panelRight - kMeterColWidth - kMeterColPad;
const int meterColY  = visualizerBottom - kMeterHeight - kMeterColPad;
const int inMeterX   = meterColX + kMeterColGap;
const int outMeterX  = inMeterX + kIndividualMeterW + kMeterColGap;

inMeter.setBounds (inMeterX,  meterColY, kIndividualMeterW, kMeterHeight);
outMeter.setBounds(outMeterX, meterColY, kIndividualMeterW, kMeterHeight);
```

Plus two small etched "IN" / "OUT" labels painted in `RightPanel::paint` below each meter (~10 px tall etched text).

### 5. Oscilloscope Auto button

**Files:** modify `Source/UI/visualizers/Oscilloscope.h` + `.cpp`.

Add a `juce::TextButton autoButton { "AUTO" };` member. Style it via lookAndFeel or direct `setColour` calls so it matches the WebView's small monospace bottom-right pill (`8 px` bold mono, dim border, transparent bg).

In the Oscilloscope constructor:

```cpp
addAndMakeVisible(autoButton);
autoButton.setClickingTogglesState(true);
autoButton.setToggleState(autoScale, juce::dontSendNotification);
autoButton.onClick = [this] {
    autoScale = autoButton.getToggleState();
    repaint();
};
```

In `Oscilloscope::resized()`, place the button at the bottom-right:

```cpp
constexpr int kAutoBtnW = 28;
constexpr int kAutoBtnH = 12;
constexpr int kAutoBtnPad = 4;
autoButton.setBounds(getWidth()  - kAutoBtnW - kAutoBtnPad,
                     getHeight() - kAutoBtnH - kAutoBtnPad,
                     kAutoBtnW, kAutoBtnH);
```

Editor-local state — `autoScale` is not persisted to APVTS. Matches the WebView's behaviour. Toggle does not survive plugin window close.

### 6. Collapse-Advanced layout reflow

**File:** modify `Source/UI/panels/RightPanel.cpp` (`resized()` and supporting layout constants).

Current behaviour: visualizers are laid out top-down after Advanced. When Advanced collapses, the freed space sits *between* the Advanced toggle and the visualizers — the visualizers stay put.

New behaviour: anchor the bottom row to the panel bottom; spectrum fills the variable area above it.

```cpp
constexpr int kOscRowHeight = 100;   // fixed bottom-row height (oscilloscope + meter column)
constexpr int kRowGap       = 6;
constexpr int kSpectrumMin  = 60;    // safety floor

const int visualizerBottom = panelBottom - kBottomPad;
const int oscRowTop        = visualizerBottom - kOscRowHeight;

// Oscilloscope: full width minus meter column.
const int oscWidth = panelRight - panelLeft - kMeterColWidth - 2 * kMeterColPad;
oscilloscope.setBounds(panelLeft, oscRowTop, oscWidth, kOscRowHeight);

// Spectrum: top = right after advanced row + gap; bottom = oscRowTop - rowGap.
const int spectrumTop    = advancedRowBottom + kRowGap;
const int spectrumBottom = oscRowTop - kRowGap;
const int spectrumHeight = juce::jmax(kSpectrumMin, spectrumBottom - spectrumTop);
spectrum.setBounds(panelLeft, spectrumBottom - spectrumHeight,
                   panelRight - panelLeft, spectrumHeight);

// Meter column (see section 4) — anchored to visualizerBottom.
```

**Key invariant:** `oscRowTop`, `visualizerBottom`, and `meterColY` are computed *independently of `advancedExpanded`*. Only `spectrumHeight` (and therefore `spectrumTop`) changes when Advanced toggles. `advancedRowBottom` is the only Advanced-state-dependent value upstream.

## Components and files affected

| Component | File(s) | Change |
|---|---|---|
| Settings overlay | `Source/UI/panels/SettingsOverlay.{h,cpp}` | NEW |
| Top-bar gear | `Source/UI/panels/TopBar.{h,cpp}` | add `settingsButton` + `onSettingsRequested` |
| Binaural toggle widget | `Source/UI/widgets/ChoiceToggle.{h,cpp}` | NEW |
| RightPanel | `Source/UI/panels/RightPanel.{h,cpp}` | binaural toggle member, meter relocation, layout reflow |
| Oscilloscope | `Source/UI/visualizers/Oscilloscope.{h,cpp}` | `autoButton` member |
| NativePluginEditor | `Source/UI/NativePluginEditor.{h,cpp}` | own `settingsOverlay`, wire gear button + mutual exclusion |
| Build | `CMakeLists.txt` | register the new source files |

## Out of scope

- The WebView equivalents stay as they are. No back-port required.
- Voice-Split binaural mode stays stubbed in `BinauralStage.cpp`. The settings overlay simply doesn't expose it.
- No new APVTS params introduced (all bindings are to existing per-engine params).
- The oscilloscope's `autoScale` stays editor-local; not adding an APVTS bool for it.

## Migration / compatibility

- No APVTS changes, so existing presets and DAW projects load unchanged.
- The meter visual moves but the underlying `IOMeter` widgets are unchanged. The Levels card visually loses its flanking meters; the In/Out knobs sit alone in the Levels card now (the Reverb toggle + small knob still sit there too, per the prior work).

## Testing notes

- Build with 0 errors / 0 warnings.
- Visual verification by the user in Ableton:
  - Gear button opens settings overlay; binaural mode change reflects in audio.
  - Settings overlay closes via × or backdrop click.
  - Binaural quick toggle in Advanced row lights when Spread; the settings overlay's Binaural selector reflects the same value.
  - Meters appear in bottom-right; respond to audio; visually distinct as IN / OUT.
  - Oscilloscope AUTO button toggles auto-scale; persists for the editor's life, resets on window close.
  - Advanced toggle: collapse expands spectrum upward; bottom row (oscilloscope + meters) stays put.
