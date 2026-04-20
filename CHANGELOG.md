## [Unreleased]

### Added
- **Preset pack system.** Any folder in the preset root is a pack. Optional
  `pack.json` provides display name, description, and designer; optional
  `cover.png`/`cover.jpg` provides album art. Factory and User always
  appear, even when empty.
- **Explore grid view** in the preset browser — shows a card for each pack
  with cover art (gradient fallback when no image), preset count, and a
  hover-preview panel with pack metadata.
- **Description field on presets** — optional multi-line notes stored
  inside the preset's ValueTree, captured in the Save modal, displayed in
  the preview panel.
- **Preset count indicator** in the browser header ("XXX presets") that
  updates live with the current view and search filter.
- **Favorite indicator in the dropdown** — ♥ marker next to favorited
  presets in the category preset list.

### Changed
- **Preset system rewritten** (see *Removed* and *Fixed* for details):
  - `PresetManager` now owned by `AudioProcessor` instead of the Editor,
    fixing a latent lifetime hazard where WebView callbacks could outlive
    the Editor-scoped manager.
  - Presets are single `.fxp` files containing the APVTS `ValueTree` with
    a `<Metadata>` child node (name / type / designer / description);
    save and load are atomic.
  - Favorites persist in a separate `favorites.index` JSON file keyed by
    `pack/preset` — works for read-only pack directories.
  - All JSON is built and parsed through `juce::JSON` / `juce::DynamicObject`.
- **Preset dropdown** redesigned to Arturia's two-column layout:
  categories (with counts and ✓ marker for the active one) on the left,
  alphabetical preset list on the right.
- **Header pill** widened; heart (favorite) button moved inside the pill
  on the left.
- **JS preset UI consolidated** from three files (`preset-header.js`,
  `preset-dropdown.js`, `preset-browser.js`) into a single
  `preset-system.js` with a single init path.

### Fixed
- **Keyboard pass-through to the DAW.** Spacebar, A–L (keyboard MIDI
  mode), arrow keys, Enter, Ctrl+S, Ctrl+Z, etc. all now reach the host
  even when the plugin UI has been interacted with. Implemented as a
  layered fix:
  - JUCE-level: `EDITOR_WANTS_KEYBOARD_FOCUS FALSE`,
    `setMouseClickGrabsKeyboardFocus(false)` on editor and WebView.
  - DOM-level: `mousedown preventDefault` on non-input targets blocks
    Chromium's default focus transfer during knob drags.
  - Win32 subclass on Chromium child windows returns `MA_NOACTIVATE` on
    `WM_MOUSEACTIVATE` and forwards `WM_KEYDOWN`/`KEYUP` to
    `GetAncestor(GA_ROOT)` when no text input is focused. Uses a
    long-running rescan timer so helper HWNDs created after first
    interaction are also subclassed.
  - JS `keydown`/`keyup` forwarder `PostMessage`s keys directly to the
    DAW's top-level window — the guarantee path, independent of focus
    state.
- **JS→C++ bridge calls** now use `window.Juce.getNativeFunction("name")`
  with `await`, per JUCE 8's bridge API. Previously every call was
  `undefined` and silently failed.
- **APVTS mutation on message thread** — `loadPreset` now marshals via
  `MessageManager::callAsync`; previously WebView callbacks mutated
  parameter state on arbitrary threads.
- **JSON escaping** of preset names / types / designers — previous
  hand-rolled concatenation broke on names containing `"` or `\`.
- **Accidental text selection during knob drags** — global
  `user-select: none` with re-enable on `<input>` / `<textarea>` /
  `[contenteditable]`.

### Removed
- `.json` sidecar metadata files (replaced by in-ValueTree metadata).
- Hand-rolled JSON parser (`PresetManager::extractJsonString`) and its
  dedicated test (`tests/PresetManagerTest.cpp`); replaced by
  `juce::JSON::parse`.
- `PresetTypes.h` (merged into `PresetManager.h`).
- `preset-header.js`, `preset-dropdown.js`, `preset-browser.js`
  (consolidated into `preset-system.js`).
- `PhantomProcessor::loadPresetFromFile` / `savePresetToFile` /
  `getStateAsMemoryBlock` (the logic now lives inside `PresetManager`
  and operates directly on the APVTS).
