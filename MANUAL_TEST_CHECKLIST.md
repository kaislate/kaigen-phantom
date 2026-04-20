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

