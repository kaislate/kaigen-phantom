# Pack authoring + factory bake-in — design

**Date:** 2026-05-23
**Branch:** `integration/native-plus-reverb`
**Status:** approved (in-conversation)

## Goal

Establish a workflow where:

1. A sound designer is given a special `DEVELOPER_MODE=ON` build of the
   plugin with pack authoring UI.
2. They create / edit a pack inside the plugin (new pack, save into it,
   edit metadata, set cover art, rename, delete).
3. They export the pack as a single `.kaipack` file and send it back.
4. The developer unpacks it into `Source/FactoryPacks/<pack-name>/`,
   rebuilds with `DEVELOPER_MODE=OFF`, and ships a final binary that
   has the pack baked in as Factory content with no authoring UI.

End-user builds (`DEVELOPER_MODE=OFF`) treat the embedded packs as
read-only Factory content alongside the existing disk-based User packs.

## Three components

This spec covers three independent pieces of work that combine into the
workflow:

| Component | Files | Audience |
|---|---|---|
| **A. Pack authoring UI** | `PresetBrowser.{cpp,h}`, `PresetManager.{cpp,h}` | designer build |
| **B. `.kaipack` import/export** | `PresetManager`, plus a small `PackArchive` helper | designer build |
| **C. Factory bake-in (embedded packs)** | `CMakeLists.txt`, `PresetManager` | shipped build |

A and B are `DEVELOPER_MODE`-gated. C is always on (reads whatever
factory data the build was compiled with).

## DEVELOPER_MODE gating

The flag already exists in `CMakeLists.txt:8-14` as a CMake option that
defines `DEVELOPER_MODE` to 1 or 0. All authoring UI and authoring API
on `PresetManager` is wrapped in:

```cpp
#if DEVELOPER_MODE
    // authoring code
#endif
```

End-user builds (`DEVELOPER_MODE=0`) compile away the entire authoring
surface — no buttons in the preset browser, no `exportPack` API on
`PresetManager`, no `.kaipack` reader. The factory-pack reading code
(component C) is always present.

## Component A: pack authoring UI

### New buttons in PresetBrowser

Six new actions, all visible only in `DEVELOPER_MODE`:

1. **"+ New Pack"** — opens a small modal dialog asking for pack name,
   description, designer. Creates `<userPresetRoot>/<sanitized-name>/`
   plus a `pack.json` containing the metadata. New pack appears in the
   browser's pack list immediately.

2. **"Save preset into pack…"** — extends the existing Save modal with
   a pack-picker (combo of all editable User packs, plus the active
   one as default). Today the save dialog writes to the user root;
   designer mode lets the user save into any user-owned pack.

3. **"Edit pack metadata"** — when a User pack is selected, this button
   opens a dialog to edit name (renames the folder + updates pack.json),
   description, and designer. Live preview in the side panel.

4. **"Set Cover Art"** — opens a file picker (PNG / JPG). Copies and
   resizes the picked image to ≤ 512 × 512 px PNG at
   `<packDir>/cover.png`. Replaces the existing cover if one is
   present.

5. **"Delete Pack"** — confirms via dialog, then `removeRecursively()`
   the pack folder. Factory packs (which live in BinaryData, not on
   disk) are never offered for deletion.

6. **"Export Pack…"** — see Component B below.

### Layout

All six buttons sit in a new **"AUTHORING" section** that appears below
the existing preset list in `PresetBrowser` when `DEVELOPER_MODE=1`.
End-user builds (`=0`) don't render the section at all (zero-size).

### PresetManager API additions

```cpp
#if DEVELOPER_MODE
    // Returns true on success. packName is sanitised to a folder-safe form.
    bool createPack(const juce::String& packName,
                    const juce::String& description,
                    const juce::String& designer);

    // Renames the folder + rewrites pack.json. Returns false if name
    // collides or pack is factory (read-only).
    bool renamePack(const juce::String& oldName, const juce::String& newName);

    bool setPackMetadata(const juce::String& packName,
                         const juce::String& description,
                         const juce::String& designer);

    // Copies + resizes the source image to <packDir>/cover.png.
    bool setPackCover(const juce::String& packName, const juce::File& sourceImage);

    // Recursively deletes the pack folder. Refuses to delete factory packs.
    bool deletePack(const juce::String& packName);

    // Saves the current ValueTree state to <packDir>/<presetName>.fxp.
    bool savePresetIntoPack(const juce::String& packName,
                            const juce::String& presetName,
                            const juce::String& description = {});

    // .kaipack export — see Component B.
    bool exportPack(const juce::String& packName, const juce::File& destZipFile);
#endif
```

## Component B: `.kaipack` import / export

### Format

A `.kaipack` is a renamed ZIP archive (`.zip` extension is also accepted).
Internally:

```
<pack-name>/
├── pack.json          (required: name, description, designer, version)
├── cover.png          (optional)
└── *.fxp              (the presets — same files as the on-disk pack)
```

JUCE provides `juce::ZipFile` (read) and `juce::ZipFile::Builder`
(write) — no external dependency needed.

### Export flow

`PresetManager::exportPack(packName, destFile)` builds a ZIP containing
the entire pack folder's contents, preserving the `<pack-name>/` root
inside the archive so the import flow can detect the pack name from
the archive. Triggered from the preset browser's "Export Pack…" button
with a file save dialog defaulting to `<packName>.kaipack`.

### Import flow (DESIGNER MODE ONLY — for review iterations)

For ad-hoc review where the developer wants to preview a designer's
`.kaipack` without rebuilding, designer-mode builds get an additional
**"Import Pack…"** button that:

1. Prompts for a `.kaipack` file.
2. Unzips it into `<userPresetRoot>/<pack-name>/` (overwriting any
   existing pack of the same name after a confirmation dialog).
3. Refreshes the pack list.

End-user builds DO NOT have this — packs come exclusively from the
embedded factory data or user-saved presets.

## Component C: factory bake-in

### CMake change

The commented-out block at `CMakeLists.txt:130-133` is partially right
already. Replace with:

```cmake
# Factory packs — drop pack folders under Source/FactoryPacks/ and they
# get embedded as binary data, appearing as read-only Factory packs in
# the preset browser. To add a designer's pack: unzip the .kaipack
# into Source/FactoryPacks/<pack-name>/ and rebuild.
file(GLOB_RECURSE FACTORY_PACK_FILES
     CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/Source/FactoryPacks/*")
if(FACTORY_PACK_FILES)
    juce_add_binary_data(KaigenFactoryPacks
        HEADER_NAME KaigenFactoryPacks.h
        NAMESPACE   KaigenFactoryPacks
        SOURCES     ${FACTORY_PACK_FILES})
    target_link_libraries(KaigenPhantom PRIVATE KaigenFactoryPacks)
    target_compile_definitions(KaigenPhantom PRIVATE KAIGEN_HAS_FACTORY_PACKS=1)
else()
    target_compile_definitions(KaigenPhantom PRIVATE KAIGEN_HAS_FACTORY_PACKS=0)
endif()
```

`GLOB_RECURSE` with `CONFIGURE_DEPENDS` makes CMake re-glob on every
build (cost: trivial for a small directory), so dropping new pack
folders into `Source/FactoryPacks/` and rebuilding picks them up
automatically without editing CMakeLists.

### PresetManager loading

`PresetManager` currently scans `<userPresetRoot>/` for pack folders.
Extend it to ALSO scan the embedded `KaigenFactoryPacks` namespace:

```cpp
void PresetManager::loadFactoryPacks()
{
#if KAIGEN_HAS_FACTORY_PACKS
    // Iterate KaigenFactoryPacks::namedResourceList[]; group by the
    // first path segment (= pack name). For each pack, parse the
    // embedded pack.json, then for each *.fxp entry create a virtual
    // PresetEntry whose load source is the embedded byte array
    // instead of a juce::File.
    for (int i = 0; i < KaigenFactoryPacks::namedResourceListSize; ++i)
    {
        const char* originalName = KaigenFactoryPacks::originalFilenames[i];
        int size = 0;
        const char* data = KaigenFactoryPacks::getNamedResource(
            KaigenFactoryPacks::namedResourceList[i], size);
        // Parse path segments → packName / fileName.
        // Stash into a Pack { name, presets[], coverData, metadata }.
    }
#endif
}
```

The existing `PresetEntry` struct gains a `juce::MemoryBlock embeddedData;`
field (zero-size for disk-based presets). The load path reads from
`embeddedData` when present, else from the file.

Factory packs are flagged `readOnly = true` so the authoring UI (which
only appears in `DEVELOPER_MODE` anyway) never offers Delete / Rename /
Save-into for them.

### Visual distinction in the browser

The pack list already has separate "Factory" and "User" groupings (per
the existing browser). Embedded packs go into Factory automatically.
A small label or icon could differentiate "embedded" from "shipped on
disk" Factory packs, but that's optional polish — leave it identical
for v1.

## File summary

| File | Change |
|---|---|
| `CMakeLists.txt` | Add `juce_add_binary_data(KaigenFactoryPacks ...)` block, define `KAIGEN_HAS_FACTORY_PACKS` |
| `Source/PresetManager.h/.cpp` | Add authoring API (DEV-only), add factory-pack scan, add `embeddedData` to `PresetEntry` |
| `Source/UI/panels/PresetBrowser.cpp/.h` | Add DEV-only AUTHORING section with 7 buttons (6 ops + Import for review) |
| `Source/Pack/PackArchive.h/.cpp` *(NEW)* | `juce::ZipFile`-backed export/import helpers — small, focused module |
| `Source/FactoryPacks/.gitkeep` *(NEW dir)* | Empty placeholder so CMake's glob doesn't fail on a fresh checkout |

## Workflow walkthrough

**Sound designer:**

1. Receives `Kaigen Phantom_dev.vst3` (a build with `-DDEVELOPER_MODE=ON`).
2. Opens it in Ableton. Sees an **AUTHORING** section in the preset
   browser.
3. Clicks **+ New Pack**, enters name "Underworld", description, their
   name. Pack folder appears.
4. Tweaks knobs to taste, clicks **Save**. Save dialog has a "Pack:"
   dropdown — picks "Underworld". Preset saved into the pack.
5. Repeats for 20–40 presets.
6. Clicks **Set Cover Art**, picks `underworld-cover.jpg`. Cover image
   appears on the pack tile.
7. Clicks **Edit Pack Metadata**, refines the description.
8. Clicks **Export Pack…**, saves as `Underworld.kaipack`. Sends to
   developer.

**Developer:**

1. Receives `Underworld.kaipack`.
2. Optional review iteration: opens their own DEV build, clicks
   **Import Pack…**, picks the file, previews it.
3. When happy: unzips `Underworld.kaipack` into
   `Source/FactoryPacks/Underworld/`.
4. Rebuilds release with `-DDEVELOPER_MODE=OFF`. Authoring UI
   disappears from the binary; the Underworld pack appears as a
   Factory pack alongside any other shipped packs.
5. Ships the binary.

## Out of scope

- Pack signing / encryption (designer signature, integrity check).
  Could add later; for v1 trust your collaborators.
- Per-preset metadata beyond what already exists (the existing system
  has `<Metadata>` ValueTree child with name / type / designer /
  description).
- Pack versioning / changelog. Designers iterate by sending new
  `.kaipack` files.
- Marketplace / web download flow. Bundling and side-load only.
- License / DRM. Separate discussion.
- Cover-art animation / video covers. Static PNG/JPG only.

## Migration / compatibility

- End-user builds with no factory packs (just `Source/FactoryPacks/`
  empty + `.gitkeep`) behave exactly as today — only User packs
  appear. `KAIGEN_HAS_FACTORY_PACKS=0` short-circuits the load path.
- The first factory pack added forces the binary to grow by the
  pack's content size (typical ~50–200 KB depending on preset count
  and cover image size).
- Existing user presets continue to load from disk unchanged.
- `pack.json` schema stays as documented in the existing changelog:
  `name`, `description`, `designer`, optional `cover` filename (default
  `cover.png`). No breaking changes.

## Testing notes

- Build with `-DDEVELOPER_MODE=ON` and `-DDEVELOPER_MODE=OFF` both
  succeed with 0 errors.
- Designer flow: create pack → save 3 presets → set cover → export
  `.kaipack` → import on a fresh DEV build → presets and cover appear.
- Developer flow: unzip a `.kaipack` into `Source/FactoryPacks/`,
  rebuild with `DEVELOPER_MODE=OFF`, verify the pack appears as
  Factory and Delete / Save-into are not offered for it.
- Round-trip: save state → reload state → custom slot data (and any
  other plugin state) survives unchanged regardless of `DEVELOPER_MODE`.
