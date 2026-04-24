# Manual Integration Test Checklist - Task 12

**Plugin Location:** `C:\Users\kaislate\Downloads\KAIGEN\Kaigen Phantom.vst3`  
**Preset Directory:** `C:\Users\kaislate\AppData\Roaming\Kaigen\KaigenPhantom\Presets\User\`  
**Build Status:** ✅ SUCCESS (0 errors, 0 warnings)

---

## Pre-Test Checklist
- [ ] Close Ableton Live completely (not just minimize)
- [ ] Restart Ableton Live
- [ ] Create new Audio/Instrument track
- [ ] Add "Kaigen Phantom" plugin to track

---

## Test 1: Plugin Loads ✅ CRITICAL
- [ ] Plugin appears without crash
- [ ] Preset header visible
- [ ] "Default" preset name shown
- **Expected:** UI fully responsive, no error dialogs

---

## Test 2: Preset Header Buttons 🎛️
Press each button and observe behavior:

### 2.1 Preset Dropdown
- [ ] Click preset name
- [ ] **Expected:** Dropdown menu appears showing categories and presets
- [ ] Click category header
- [ ] **Expected:** Category expands/collapses
- [ ] Click a preset name
- [ ] **Expected:** Dropdown closes, preset loads

### 2.2 Previous/Next Arrows (▲ ▼)
- [ ] Click ▲ arrow
- [ ] **Expected:** Nothing happens (not implemented in MVP - OK if no response)
- [ ] Click ▼ arrow
- [ ] **Expected:** Nothing happens (not implemented in MVP - OK if no response)

### 2.3 Favorite Heart (♡)
- [ ] Click heart icon
- [ ] **Expected:** Heart toggles to filled ♥, changes to red/accent color
- [ ] Click again
- [ ] **Expected:** Heart toggles back to outline ♡, returns to normal color

### 2.4 Preset Browser (|||)
- [ ] Click books icon (|||)
- [ ] **Expected:** Browser modal opens
- [ ] **Expected:** Icon changes to ✕
- [ ] Click ✕
- [ ] **Expected:** Browser closes
- [ ] **Expected:** Icon changes back to |||

---

## Test 3: Save Workflow 💾 CRITICAL
### 3.1 Create Modified State
- [ ] Turn any knob/control to change a parameter
- [ ] **Expected:** Red asterisk (*) appears next to preset name
- [ ] **Expected:** Red "Save" button appears (prominent)

### 3.2 Open Save Dialog
- [ ] Click "Save" button
- [ ] **Expected:** Modal dialog opens with:
  - [ ] Text field showing current preset name
  - [ ] "Overwrite" checkbox (unchecked initially)
  - [ ] "Save" button
  - [ ] "Cancel" button

### 3.3 Save as New Preset
- [ ] Clear text field
- [ ] Type: `My Test Preset`
- [ ] Click "Save"
- [ ] **Expected:** Dialog closes
- [ ] **Expected:** Preset name changes to "My Test Preset"
- [ ] **Expected:** Red asterisk (*) disappears
- [ ] **Expected:** Save button hides
- [ ] Verify file exists: `%APPDATA%\Kaigen\KaigenPhantom\Presets\User\My Test Preset.fxp`

---

## Test 4: Load Saved Preset 📂
### 4.1 Load from Dropdown
- [ ] Click preset name
- [ ] **Expected:** Dropdown appears
- [ ] Find and click "My Test Preset"
- [ ] **Expected:** Preset loads, dropdown closes
- [ ] **Expected:** Parameters match what you saved in Test 3

### 4.2 Modify and Verify Modified Indicator
- [ ] Turn a different knob
- [ ] **Expected:** Red asterisk (*) reappears
- [ ] **Expected:** Red "Save" button reappears

---

## Test 5: Browser Modal Search 🔍
### 5.1 Open Browser
- [ ] Click books icon (|||)
- [ ] **Expected:** Full browser modal opens
- [ ] **Expected:** Shows list of available presets

### 5.2 Search Function
- [ ] Type "Test" in search bar
- [ ] **Expected:** "My Test Preset" appears in filtered results
- [ ] Click on "My Test Preset" row
- [ ] **Expected:** Preset loads
- [ ] **Expected:** Browser closes automatically
- [ ] **Expected:** Preset name shows "My Test Preset"

---

## Test 6: Summary

### ✅ All Tests Passed?
- [ ] Test 1: Plugin loads without crash
- [ ] Test 2: All button responses correct
- [ ] Test 3: Save workflow works, file created
- [ ] Test 4: Load from dropdown works, parameters match
- [ ] Test 5: Browser search and load work

### Issues Found?
- [ ] Document which test failed
- [ ] Describe the issue
- [ ] Note any error messages

### Final Step
If all tests pass:
```bash
cd C:\Documents\NEw project\Kaigen Phantom
git add .
git commit -m "test: verify end-to-end preset system functionality"
```

---

## Troubleshooting

### If plugin doesn't load:
- [ ] Verify VST3 file exists at: `C:\Users\kaislate\Downloads\KAIGEN\Kaigen Phantom.vst3`
- [ ] Restart Ableton completely (not just close window)
- [ ] Check Ableton's plugin search settings
- [ ] Check Windows Event Viewer for crash details

### If buttons don't respond:
- [ ] Try reloading the plugin on the track
- [ ] Check browser console for JavaScript errors
- [ ] Verify WebView component is working

### If preset doesn't save:
- [ ] Check if directory exists: `C:\Users\kaislate\AppData\Roaming\Kaigen\`
- [ ] Verify write permissions on directory
- [ ] Check if file actually created in User folder

### If loaded preset parameters don't match:
- [ ] This might indicate a DSP/parameter binding issue separate from preset system
- [ ] Focus on preset UI/workflow tests for this task
- [ ] Log issue for later investigation

---

## A/B Compare

Build with Debug config, copy VST3 to `C:\Users\kaislate\Downloads\KAIGEN\`, fully restart Ableton Live 12, load the plugin on a fresh track.

- [ ] **Snap with include-discrete OFF (default).** Set slot A Ghost Mode = Replace. Snap to B. Set slot B Ghost Mode = Phantom Only. Snap A↔B a few times — Ghost Mode stays fixed (no audible click at crossover). Only continuous params flip.
- [ ] **Preset `<MorphConfig>` round-trip.** Hand-craft a `.fxp` file containing `<SlotB>` *and* `<MorphConfig defaultPosition="0.5" curve="linear" />`, drop it into `%APPDATA%/Kaigen/KaigenPhantom/Presets/User/`. Load it — both slots restored; the morph knob should move to 0.5 (since defaultPosition is honored now that morph ships unconditionally).
- [ ] **Snap with include-discrete ON.** Open settings, enable "Include discrete parameters when snapping A/B". Repeat the previous test — now Ghost Mode flips on snap and a click is audible if the two engines are in materially different states.
- [ ] **Designer-authored preset override.** Turn include-discrete OFF. Load a factory A/B preset that has different Ghost Modes between slots (may need to create one for testing). Snap A↔B — discrete params flip regardless of the setting (designer intent).
- [ ] **Designer-authored clears on edit.** From the previous state, tweak any knob. Snap A↔B — discrete params no longer flip (designerAuthored cleared).
- [ ] **Per-slot modified indicator.** Snap to B, tweak a param, snap back to A. The "B" pill should show a small red dot under the letter. Top-level preset asterisk reflects only the active slot.
- [ ] **DAW project save/reload.** Configure distinct slot A and B content, set active = B, enable include-discrete, save the Ableton project. Close and reopen. Plugin state: slots restore with correct content, active = B, include-discrete still ON.
- [ ] **Browser A/B column.** Open preset browser. Existing presets render "—". Save a preset as A/B → row shows "A|B" badge. Sort by column works (Single → A/B → Morph). Filter dropdown narrows list correctly.
- [ ] **Save modal Preset Kind.** With slots identical: A/B radio dimmed + helper text. With slots different: A/B radio enabled. Save as A/B — reload the preset — both slots match what was saved.

## Morph (arc modulation + Scene Crossfade — always shipped)

Build and deploy:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --target KaigenPhantom_VST3 --config Release
    cp -r "build/KaigenPhantom_artefacts/Release/VST3/Kaigen Phantom.vst3" "C:/Users/kaislate/Downloads/KAIGEN/"

Fully restart Ableton. Load the plugin.

- [ ] **Modulation panel visible.** Dark horizontal strip between header and body. MORPH label, Lane 1 badge, slider, value, CAPTURE button, "0 armed" status.
- [ ] **Enable toggle works.** Click the enable dot — lights amber. Knobs grow a thin outer track ring (barely visible since no arcs yet). Click again — rings vanish.
- [ ] **Capture mode batch setup.** With morph enabled, press CAPTURE. Button switches to COMMIT; CANCEL appears. Drag 3–4 knobs to different positions. Press COMMIT. Knob rings now show blue modulation segments. "N armed" status updates. Move morph slider — knobs animate to the captured targets.
- [ ] **Capture cancel restores.** Enter capture, drag a knob, press CANCEL. Knob returns to original position; no arc set.
- [ ] **Direct arc drag.** With an arc armed, grab its blue handle at the tip of the arc. Drag around the ring — depth adjusts live.
- [ ] **Plateau clamping.** Set an arc that pushes target past the knob's max (e.g., base 80%, depth +50%). Move morph slider — live pointer reaches max mid-sweep and stays there while morph continues.
- [ ] **Scene Crossfade toggle.** Open settings → Morph → enable "Scene Crossfade". Panel grows a second SCENE row with its own slider. Set slot A and slot B to different sounds (via A/B compare). Sweep scene slider — audio crossfades between them. CPU in Ableton increases noticeably (~doubled).
- [ ] **Save + reload preset with morph.** Arm 3 arcs, save as "A/B + Morph". Load a Single preset to wipe state. Reload the A/B + Morph preset. Arcs restore; slots restore; morph position restores (to 0 if saved at 0).
- [ ] **Project save/reload.** Configure morph state, save Ableton project, close + reopen. Plugin state restores including arcs + enabled flag + morph position.

