# Phantom Preset System — Design Specification

**Date:** 2026-04-19  
**Status:** Design Phase  
**Scope:** Save/load/manage presets in Arturia-style UI with developer mode for release preset creation

---

## Overview

Phantom will have a complete preset system modeled after Arturia plugins (Acid-V, DIST COLDFIRE, etc.). Users can browse, load, save, and favorite presets. A developer mode allows creating/editing factory presets that ship with the plugin. Preset storage follows platform conventions with separation of factory, user, and imported pack presets.

---

## UI Architecture

### Header Bar (Single Line)

The preset system lives in the plugin header, matching Arturia's minimalist approach:

```
[|||  ♡]  [Preset Name]  [▲ ▼] [Save*]
Books Heart  (centered)   Browse buttons, Save (appears when modified)
```

**Components:**
- **Books icon (|||)** — opens full preset browser (changes to ✕ when open)
- **Heart (♡/♥)** — favorite/unfavorite current preset
- **Preset Name** — centered, prominent. Appends `*` in red when preset is modified
- **Up/Down arrows (▲ ▼)** — browse presets in current category
- **Save button** — appears only when `*` is present (preset modified). Red button, high contrast

### Simple Dropdown (Click Preset Name)

Quick preset browsing when user clicks the preset name:

- Vertical list organized by category (Piano, Drone, Synth, Bass, Experimental)
- Categories can be collapsed/expanded
- Currently loaded preset is highlighted
- Click to load a preset
- Click elsewhere to close

### Full Preset Browser (Click Books Icon)

Comprehensive preset explorer when user clicks the books icon:

**Layout:**
- **Left sidebar:** Category/pack navigation (Explore, All Presets, Piano, Drone, etc.)
- **Main area:** Search bar, columns (Name, Type, Designer), scrollable preset list
- **Right panel:** Preview of selected preset (shows preset metadata)

**Columns:**
- **Name** — preset name, clickable to load
- **Type** — preset category (Piano, Drone, Synth, Bass, Experimental)
- **Designer** — creator name (factory presets = "Kai Slate", user presets = username or blank)
- **♡/♥** — favorite toggle (click to favorite/unfavorite)

**Search & Filtering:**
- Search box filters presets by name in real-time
- Click "Clear" to reset search

**Interactions:**
- Click preset name to load it
- Click heart to favorite/unfavorite
- Click category in sidebar to filter list
- Books icon changes to ✕ while open; click to close

---

## Save Workflow

### Modified Preset Indicator

When user adjusts parameters on the current preset:
1. Preset name in header appends `*` (red asterisk)
2. Save button appears (red, high-contrast)
3. Heart button turns red if preset is favorited

### Save Modal

Clicking the Save button opens a modal dialog:

```
┌─────────────────────────────┐
│ Save Preset                 │
├─────────────────────────────┤
│ Preset Name: [input field]  │
│                             │
│ ☐ Overwrite existing preset │
│                             │
│ [Save]  [Cancel]            │
└─────────────────────────────┘
```

**Behavior:**
- Input field pre-filled with current preset name
- If user checks "Overwrite existing preset": saves over current preset (if user-created or modified factory copy)
- If unchecked (default): saves as new preset
- Cancel closes modal without saving
- After save: `*` disappears from header, new preset name appears

**Special Cases:**
- **Factory presets:** Cannot be overwritten. If user modifies a factory preset and saves, it always saves as a new user preset (ignoring overwrite checkbox, or checkbox is disabled)
- **User presets:** Can be overwritten if user selects that option

---

## Preset Storage

### Directory Structure

Presets are stored in platform-specific user data directories:

**Windows:**
```
%APPDATA%\Kaigen\KaigenPhantom\Presets\
├── Factory/
│   ├── Warm Bass Boost.fxp
│   ├── Aggressive.fxp
│   ├── Hollow Dub.fxp
│   └── ...
├── User/
│   ├── My Custom Preset.fxp
│   ├── Deep Drone v2.fxp
│   └── ...
├── Analog Vibes/
│   ├── Analog Bass.fxp
│   ├── Analog Keys.fxp
│   └── ...
└── Dark Synths/
    └── ...
```

**macOS:**
```
~/Library/Application Support/Kaigen/KaigenPhantom/Presets/
└── (same structure as Windows)
```

**Linux:**
```
~/.config/Kaigen/KaigenPhantom/Presets/
└── (same structure as Windows)
```

### Folder Categories

1. **Factory/** — Read-only factory presets shipped with plugin
   - Cannot be deleted or edited in place
   - If user modifies and saves, saves as new preset in User/
   
2. **User/** — User-created and modified factory presets
   - Can be edited, deleted, renamed
   - Default destination for "Save as New"
   
3. **[OfficialPackName]/** — Preset packs shipped by Kaigen
   - E.g., "Analog Vibes", "Dark Synths"
   - Read-only like Factory/
   - Show in preset browser under their own category
   
4. **[UserPackName]/** — Custom preset packs created by users
   - Users can organize presets into folders
   - Shows in preset browser as separate categories
   - Can be deleted/reorganized

### File Format

Presets are stored as `.fxp` files (JUCE standard VST preset format):
- Single preset per file
- Contains all 21 plugin parameters (Mode, Ghost, Phantom Threshold, Recipe weights, etc.)
- Portable across systems and plugin versions (with compatibility migration if needed)

---

## Preset Browser Behavior

### Loading Presets

1. User clicks preset name in simple dropdown or full browser
2. Plugin loads preset state (all 21 parameters)
3. UI updates: preset name appears in header, `*` disappears
4. DSP reads new parameter values and updates output

### Browsing Categories

- **Simple dropdown:** Click on category to expand/collapse
- **Full browser:** Click sidebar category to filter main list

### Favoriting Presets

- Click heart icon (♡) to favorite, turns red (♥)
- Favorites persist across sessions (stored as metadata or separate index)
- Can filter by favorites in full browser (optional feature, nice-to-have)

### Preset Sorting

Full browser columns are sortable (click column header to sort A-Z or by type):
- Name (A-Z)
- Type (Piano, Drone, etc.)
- Designer (Kai Slate, User, etc.)

---

## Developer Mode

### Purpose

Create, edit, and test factory presets during development. Stripped out before release via build flag.

### Activation

- Settings panel toggle: "Developer Mode"
- Shows "Manage Factory Presets" panel (in a new tab or floating window)

### Features

**Preset Management Panel:**
- List of all presets (factory, user, imports)
- Buttons: Create New, Edit, Delete, Export for Release
- Edit dialog allows changing Name, Type, Designer
- Delete removes preset file
- Export prepares preset for inclusion in next release binary

**Workflow:**
1. User adjusts parameters to desired state
2. Click "Create New" in Developer panel
3. Enter preset name, type (Piano/Drone/etc.), designer info
4. Preset saved to Factory/ folder (for development)
5. When ready to ship: Export presets from Factory/ to plugin binary via build process
6. Developer mode code stripped via CMake flag before release build

### Build Flag

CMake option: `-DDEVELOPER_MODE=ON/OFF` (default OFF for release builds)

---

## Preset Import

### Import Dialog

User clicks "Import Preset Pack" option in books icon menu (or in developer panel):
- File picker opens
- User selects `.fxp` file(s) or preset pack archive
- User specifies pack name (only for user-created packs; official packs have built-in names)

### Import Behavior

**Official Packs (pre-named):**
- User selects `.fxp` file or folder
- Plugin creates new folder in Presets/ with pack's built-in name
- Files copied to folder
- Appears in preset browser under that pack's category

**User-Created Packs:**
- User imports `.fxp` files
- Dialog prompts: "Create new pack or add to existing?"
- If new: user types pack name → folder created
- Files copied to folder
- Appears in preset browser as custom category

### Import Sources

- Local files (drag-drop or file picker)
- Future: downloading from Kaigen website/shop (out of scope for MVP)

---

## Preset Categories

**Fixed categories (shown in dropdowns and full browser):**
- Piano
- Drone
- Synth
- Bass
- Experimental

Each preset has a Type field (assigned during creation or in developer mode) that determines which category it appears under.

**Pack categories (dynamic):**
- Factory/ presets appear under "Factory"
- Official packs appear under their names (Analog Vibes, Dark Synths, etc.)
- User packs appear under user-assigned names

---

## Data Model

### Preset Metadata

Each preset file contains:
- **All 21 parameters** (Mode, Ghost, Phantom Threshold, Recipe H2-H8, Harmonic Saturation, Env Attack/Release, etc.)
- **Preset Name**
- **Type** (Piano, Drone, Synth, Bass, Experimental)
- **Designer** (Kai Slate, user, or import source)
- **Favorite flag** (stored in preset or in separate index file)

### File Naming

Preset files: `{PresetName}.fxp`
- User creates "Warm Bass Boost" → stored as `Warm Bass Boost.fxp`
- Names can contain spaces
- Case-sensitive on macOS/Linux, case-insensitive on Windows

---

## Integration with Current Plugin

### PluginProcessor Changes

- Add preset load/save functions using JUCE's `AudioProcessorValueTreeState`
- Write/read `.fxp` files using JUCE's preset system
- Manage preset directory structure on startup
- Load factory presets from plugin binary on first run

### PluginEditor Changes

- Add preset header UI (currently shows plugin name and DSP tag)
- Replace with [Books | ♡] [Preset Name] [▲ ▼] [Save*]
- Wire up preset loading to update all 21 parameters via APVTS
- Add simple dropdown and full browser as WebView dialogs or native overlays

### Parameters

No new parameters needed; system uses existing 21 parameters. "Recipe Preset" combo box may be deprecated if recipe selection moves into full preset system (TBD).

### WebUI

Preset browser UI can be:
- **WebView-based** (HTML/CSS/JS like current UI)
- **Native JUCE components** (faster, more integrated)

Recommend WebView for consistency with current design. Mockups shown in this design are Web-based layout.

---

## Platform Compatibility

- **Windows 10/11:** Full support, user data in `%APPDATA%`
- **macOS 11+:** Full support, user data in `~/Library/Application Support`
- **Linux:** Full support (VST3), user data in `~/.config`

---

## Error Handling

- **Missing preset file:** User loads preset, file not found → show error, stay on current preset
- **Corrupted `.fxp` file:** Skip in preset list with warning, don't crash
- **Insufficient permissions:** User tries to import/save → show error dialog
- **Disk full:** Save fails → show error, prompt user to free space

---

## Testing Strategy

**Manual testing:**
- Load/save presets, verify all 21 parameters persist
- Favorite/unfavorite, verify heart icon updates
- Modify preset, save as new, verify overwrite option works
- Browse categories, verify filter works
- Import preset pack, verify folder structure and visibility
- Delete presets, verify file system updates
- Developer mode create/edit/delete, verify Factory/ folder
- Test on Windows, macOS (if available)

**Integration:**
- Preset system doesn't interfere with DSP parameter reading
- Bypass button still works with presets
- Spectrum analyzer updates when preset loads

---

## Future Enhancements (Out of Scope)

- Cloud sync of user presets
- Favorite presets filter in full browser
- Drag-drop preset reordering
- Preset sharing via QR code or link
- Tagging system (beyond Type/Designer)
- Undo/redo for preset edits
- Preset randomizer

---

## Rollout Plan

**Phase 1 (MVP):**
- Header UI + simple dropdown + full browser
- Save/load presets from User/ folder
- Factory presets shipped in binary (no developer mode yet)

**Phase 2:**
- Developer mode toggle + Manage Presets panel
- Create/edit/delete factory presets in development
- Export presets to binary

**Phase 3:**
- Preset import dialog
- Official preset pack support
- User custom pack creation

---

## Build Flags

```cmake
# Enable developer preset management (default OFF for release)
option(DEVELOPER_MODE "Enable developer preset management panel" OFF)

# If ON: include dev panel code, allow create/edit/delete in Factory/
# If OFF: strip dev panel, factory presets read-only
```

---

## Files Affected

- `Source/PluginProcessor.h/cpp` — preset load/save logic
- `Source/PluginEditor.h/cpp` — header UI, preset dropdowns
- `Source/PresetManager.h/cpp` (new) — directory management, file I/O
- `Source/WebUI/index.html` — preset browser UI
- `CMakeLists.txt` — DEVELOPER_MODE flag, BinaryData for factory presets
- `docs/` — this spec + future implementation plan

---

## Open Questions / To Be Decided During Implementation

1. **Sorting direction:** Click column header once to sort A-Z, again to sort Z-A?
2. **Favorite persistence:** Store in separate index file or embedded in `.fxp` metadata?
3. **Native vs WebView:** Preset browser as WebView modal or native JUCE components?
4. **Preset preview:** Show audio waveform, parameter values, or just metadata in right panel?
5. **Delete confirmation:** Show "Are you sure?" dialog, or use undo/redo?
