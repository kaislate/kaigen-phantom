# Pack Authoring + Factory Bake-In Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a `DEVELOPER_MODE`-gated pack authoring workflow (create / save into / rename / delete / cover-art / metadata-edit / export / import `.kaipack`) and a factory bake-in path (`Source/FactoryPacks/` + `juce_add_binary_data`) so designers can build packs and the developer can ship them embedded.

**Architecture:** Three independent components implemented in build-order: (C) factory bake-in foundation — extends `PresetInfo` with `embeddedData` and `loadFactoryPacks()` reading from a generated `KaigenFactoryPacks` namespace, gated by `KAIGEN_HAS_FACTORY_PACKS`; (B) a small `PackArchive` helper that wraps `juce::ZipFile` for `.kaipack` export/import; (A) the DEV-only `PresetManager` authoring API + `PresetBrowser` AUTHORING section with buttons and modal dialogs, every symbol wrapped in `#if DEVELOPER_MODE`. End-user builds (`DEVELOPER_MODE=0`) compile (A) and most of (B) away while keeping (C) active to read whatever factory data the build was compiled with.

**Tech Stack:** JUCE 8 (`juce::ZipFile`, `juce::JSON`, `juce::ValueTree`, `juce::AlertWindow`), CMake 3.22 (`juce_add_binary_data`, `file(GLOB_RECURSE … CONFIGURE_DEPENDS)`), Catch2 v3 (existing test target), C++20, MSBuild on Windows.

---

## Spec

This plan implements `docs/superpowers/specs/2026-05-23-pack-authoring-design.md`. Each task references the spec section(s) it satisfies.

## File map

**Modified:**
- `CMakeLists.txt:130-134` — replace the commented-out factory-presets block with the `KaigenFactoryPacks` glob + bake-in.
- `Source/PresetManager.h` — extend `PresetInfo`/`PackInfo`, add factory-pack loading + DEV authoring API.
- `Source/PresetManager.cpp` — implement embedded-pack scanning + loading + all DEV authoring functions.
- `Source/UI/panels/PresetBrowser.h/.cpp` — add the DEV-only AUTHORING section with 8 buttons and 3 modal dialogs.
- `tests/CMakeLists.txt` — add `PackArchiveTests.cpp` and `FactoryPackLoadingTests.cpp` to the test target, plus a tiny embedded test fixture.

**Created:**
- `Source/Pack/PackArchive.h` — declares `exportPack(packDir, destZip)` and `importPack(zipFile, destRoot)`.
- `Source/Pack/PackArchive.cpp` — JUCE `ZipFile` / `ZipFile::Builder` implementation.
- `Source/FactoryPacks/.gitkeep` — keep the empty directory checked in so the CMake glob doesn't fail on a fresh clone.
- `tests/PackArchiveTests.cpp` — Catch2 roundtrip + edge-case tests.
- `tests/FactoryPackLoadingTests.cpp` — Catch2 tests that the embedded test fixture is discovered and loadable.
- `tests/fixtures/FactoryPacksTest/TestPack/pack.json` — fixture pack manifest.
- `tests/fixtures/FactoryPacksTest/TestPack/Empty.fxp` — fixture preset (XML).

---

## Build & test commands (reference)

- Build (designer): `cmake -B build-dev -DDEVELOPER_MODE=ON -A x64` then `cmake --build build-dev --config Release --target KaigenPhantom_VST3`
- Build (ship): `cmake -B build -DDEVELOPER_MODE=OFF -A x64` then `cmake --build build --config Release --target KaigenPhantom_VST3`
- Tests: `cmake --build build --config Release --target KaigenPhantomTests` then `./build/tests/Release/KaigenPhantomTests.exe`
- Run a single Catch2 test: `./build/tests/Release/KaigenPhantomTests.exe "<test-name-or-tag>"`

---

## Task 1: Factory-packs scaffold + CMake bake-in block

**Spec:** Component C — CMake change. Establishes the directory, the glob, and the `KAIGEN_HAS_FACTORY_PACKS` macro. Loading code comes in Task 2.

**Files:**
- Create: `Source/FactoryPacks/.gitkeep`
- Modify: `CMakeLists.txt:130-134`

- [ ] **Step 1: Create the placeholder directory**

```bash
mkdir -p "Source/FactoryPacks"
printf '' > "Source/FactoryPacks/.gitkeep"
```

- [ ] **Step 2: Replace the commented-out factory-presets block in CMakeLists.txt**

Replace lines 131-134 of `CMakeLists.txt` (the three-line comment block starting with `# Factory presets (optional, for Phase 2 implementation)`) with:

```cmake
# Factory packs — drop pack folders under Source/FactoryPacks/ and they
# get embedded as binary data, appearing as read-only packs in the
# preset browser. To add a designer-authored pack: unzip the .kaipack
# into Source/FactoryPacks/<pack-name>/ and rebuild.
file(GLOB_RECURSE FACTORY_PACK_FILES
     CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/Source/FactoryPacks/*")
list(FILTER FACTORY_PACK_FILES EXCLUDE REGEX "/\\.gitkeep$")
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

- [ ] **Step 3: Verify the build succeeds in both DEVELOPER_MODE settings (no embedded packs yet)**

```powershell
cmake -B build -A x64
cmake --build build --config Release --target KaigenPhantom_VST3
cmake -B build-dev -DDEVELOPER_MODE=ON -A x64
cmake --build build-dev --config Release --target KaigenPhantom_VST3
```

Expected: both builds succeed; binary size unchanged (no embedded data yet).

- [ ] **Step 4: Commit**

```bash
git add Source/FactoryPacks/.gitkeep CMakeLists.txt
git commit -m "build: scaffold Source/FactoryPacks/ + KAIGEN_HAS_FACTORY_PACKS macro

Adds the empty Source/FactoryPacks/ directory (with .gitkeep) and the
CMake glob block that turns any packs dropped into that folder into
embedded binary data via juce_add_binary_data. KAIGEN_HAS_FACTORY_PACKS
is defined to 1 when packs are present, 0 otherwise — loading code in
the next task keys off this macro."
```

---

## Task 2: Extend PresetInfo + PackInfo for embedded data; scan factory packs

**Spec:** Component C — PresetManager loading. Adds `embeddedData` to `PresetInfo`, `isReadOnly` to `PackInfo`, and `loadFactoryPacks()` that walks the `KaigenFactoryPacks` namespace, parses each pack's `pack.json`, and stages a `PresetInfo` for each embedded `*.fxp` whose `file` is empty and whose `embeddedData` holds the bytes.

**Files:**
- Modify: `Source/PresetManager.h` (struct extensions + new private method)
- Modify: `Source/PresetManager.cpp` (implementation + integration into `scanPresetsFromDisk`)
- Create: `tests/fixtures/FactoryPacksTest/TestPack/pack.json`
- Create: `tests/fixtures/FactoryPacksTest/TestPack/Empty.fxp`
- Modify: `tests/CMakeLists.txt` (embed the fixture, add new test file)
- Create: `tests/FactoryPackLoadingTests.cpp`

- [ ] **Step 1: Create the test fixture pack**

Create `tests/fixtures/FactoryPacksTest/TestPack/pack.json`:

```json
{
    "name": "Test Pack",
    "description": "Embedded fixture for FactoryPackLoadingTests",
    "designer": "Kaigen Test"
}
```

Create `tests/fixtures/FactoryPacksTest/TestPack/Empty.fxp` (a minimal valid APVTS state — name must match the live state type "KaigenPhantomState"):

```xml
<?xml version="1.0" encoding="UTF-8"?>

<KaigenPhantomState>
  <Metadata name="Empty" type="Experimental" designer="Kaigen Test" description="" presetKind="single"/>
</KaigenPhantomState>
```

- [ ] **Step 2: Extend the structs in `Source/PresetManager.h`**

In `Source/PresetManager.h`, modify the `PresetInfo` struct (lines 48-53) to add the `embeddedData` field:

```cpp
struct PresetInfo
{
    PresetMetadata metadata;
    PreviewData    preview;
    juce::File     file;            // Empty when sourced from embedded BinaryData
    juce::MemoryBlock embeddedData; // Non-empty only for embedded factory packs
};
```

Modify the `PackInfo` struct (lines 60-67) to add `isReadOnly`:

```cpp
struct PackInfo
{
    juce::String name;
    juce::String displayName;
    juce::String description;
    juce::String designer;
    bool         hasCoverArt = false;
    bool         isReadOnly  = false;   // Embedded factory packs are read-only
    int          presetCount = 0;
};
```

Inside the `PresetManager` class body, declare the new private method beside `scanPresetsFromDisk` (after line 152):

```cpp
    void scanPresetsFromDisk();
    void loadFactoryPacksFromBinaryData();
```

- [ ] **Step 3: Implement embedded-pack scanning in `Source/PresetManager.cpp`**

Add the include + implementation. At the top of `Source/PresetManager.cpp`, after the existing `#include` block, add:

```cpp
#if KAIGEN_HAS_FACTORY_PACKS
 #include "KaigenFactoryPacks.h"
#endif
```

After the `scanPresetsFromDisk()` function (around line 288), add `loadFactoryPacksFromBinaryData()`:

```cpp
void PresetManager::loadFactoryPacksFromBinaryData()
{
#if KAIGEN_HAS_FACTORY_PACKS
    // Group embedded resources by their first path segment (= pack name).
    // KaigenFactoryPacks::originalFilenames[i] looks like "TestPack/Empty.fxp"
    // or "TestPack/pack.json"; we route metadata files to PackInfo and *.fxp
    // files to PresetInfo entries with embeddedData populated.
    struct PendingPack
    {
        PackInfo info;
        std::vector<PresetInfo> presets;
        juce::MemoryBlock packJsonBytes;
    };
    std::map<juce::String, PendingPack> pending;

    for (int i = 0; i < KaigenFactoryPacks::namedResourceListSize; ++i)
    {
        const juce::String resourceName = KaigenFactoryPacks::namedResourceList[i];
        const juce::String origPath     = KaigenFactoryPacks::originalFilenames[i];

        int size = 0;
        const char* data = KaigenFactoryPacks::getNamedResource(
            resourceName.toRawUTF8(), size);
        if (data == nullptr || size <= 0) continue;

        // Split on first '/'. The CMake glob always uses forward slashes
        // in namedResourceList path strings (juce_add_binary_data normalises).
        const int slash = origPath.indexOfChar('/');
        if (slash < 1 || slash >= origPath.length() - 1) continue;
        const auto packName = origPath.substring(0, slash);
        const auto fileName = origPath.substring(slash + 1);

        auto& pp = pending[packName];
        if (pp.info.name.isEmpty())
        {
            pp.info.name = packName;
            pp.info.displayName = packName;
            pp.info.isReadOnly = true;
        }

        if (fileName.equalsIgnoreCase("pack.json"))
        {
            pp.packJsonBytes.append(data, (size_t) size);
        }
        else if (fileName.equalsIgnoreCase("cover.png")
              || fileName.equalsIgnoreCase("cover.jpg"))
        {
            pp.info.hasCoverArt = true;
        }
        else if (fileName.endsWithIgnoreCase(".fxp"))
        {
            PresetInfo pi;
            pi.embeddedData.append(data, (size_t) size);
            pi.metadata.name = juce::File::createLegalFileName(
                fileName.upToLastOccurrenceOf(".", false, true));
            pi.metadata.packName = packName;
            pi.metadata.isFactory = true;
            pi.metadata.type = "Experimental";

            // Parse metadata + preview from the embedded XML bytes.
            if (auto xml = juce::parseXML(juce::String::createStringFromData(data, size)))
            {
                auto tree = juce::ValueTree::fromXml(*xml);
                if (tree.isValid())
                {
                    if (auto meta = tree.getChildWithName("Metadata"); meta.isValid())
                    {
                        pi.metadata.name        = meta.getProperty("name", pi.metadata.name).toString();
                        pi.metadata.type        = meta.getProperty("type", "Experimental").toString();
                        pi.metadata.designer    = meta.getProperty("designer", "").toString();
                        pi.metadata.description = meta.getProperty("description", "").toString();
                    }
                    pi.preview = readPreviewFromState(tree);
                }
            }
            pi.metadata.isFavorite = isFavorite(pi.metadata.name, packName);
            pp.presets.push_back(std::move(pi));
        }
    }

    // Parse pack.json bytes (if present) and merge into PackInfo.
    for (auto& [packName, pp] : pending)
    {
        if (pp.packJsonBytes.getSize() > 0)
        {
            auto json = juce::String::createStringFromData(
                pp.packJsonBytes.getData(), (int) pp.packJsonBytes.getSize());
            auto parsed = juce::JSON::parse(json);
            if (auto* obj = parsed.getDynamicObject())
            {
                const auto get = [obj](const char* k, const juce::String& fb)
                {
                    auto v = obj->getProperty(k);
                    return v.toString().isNotEmpty() ? v.toString() : fb;
                };
                pp.info.displayName = get("name", packName);
                pp.info.description = get("description", "");
                pp.info.designer    = get("designer", "");
            }
        }

        if (! pp.presets.empty())
        {
            std::sort(pp.presets.begin(), pp.presets.end(),
                [](const PresetInfo& a, const PresetInfo& b)
                { return a.metadata.name.compareIgnoreCase(b.metadata.name) < 0; });
            pp.info.presetCount = (int) pp.presets.size();
            allPresets[packName] = std::move(pp.presets);
        }
        packs[packName] = pp.info;
    }
#endif
}
```

Modify `scanPresetsFromDisk()` (line 201) to call the new method as its final step. Find the function's closing brace (around line 288) and insert this call just before it:

```cpp
    // Embedded factory packs supplement the on-disk packs. They get scanned
    // last so a disk pack of the same name would win — useful during designer
    // review iterations where they want a live copy to override the baked-in
    // one without rebuilding.
    loadFactoryPacksFromBinaryData();
}
```

(Place the comment + call **inside** `scanPresetsFromDisk`, immediately before the closing `}` at line 288. Do not duplicate the closing brace.)

Mark the on-disk Factory pack as read-only too. In `scanPresetsFromDisk()`'s `registerPack` lambda (after line 233 where `info.hasCoverArt` is set), add:

```cpp
        info.isReadOnly = (packName == kFactoryPackName);
```

- [ ] **Step 4: Wire the test fixture into `tests/CMakeLists.txt`**

After the `add_executable(KaigenPhantomTests ...)` block (around line 36), insert:

```cmake
# Embedded fixture pack used by FactoryPackLoadingTests. This is a TEST-ONLY
# binary-data target — the product target has its own KaigenFactoryPacks
# that is empty by default.
juce_add_binary_data(KaigenFactoryPacks
    HEADER_NAME KaigenFactoryPacks.h
    NAMESPACE   KaigenFactoryPacks
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/FactoryPacksTest/TestPack/pack.json
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/FactoryPacksTest/TestPack/Empty.fxp)
target_link_libraries(KaigenPhantomTests PRIVATE KaigenFactoryPacks)
target_compile_definitions(KaigenPhantomTests PRIVATE KAIGEN_HAS_FACTORY_PACKS=1)

# Add FactoryPackLoadingTests.cpp to the test sources.
target_sources(KaigenPhantomTests PRIVATE FactoryPackLoadingTests.cpp)
```

- [ ] **Step 5: Write the failing test**

Create `tests/FactoryPackLoadingTests.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "../Source/PresetManager.h"

using namespace kaigen::phantom;

TEST_CASE("FactoryPackLoading: embedded TestPack is discovered", "[factory-pack]")
{
    PresetManager pm;
    pm.initialize();

    const auto packs = pm.getAllPacks();
    auto it = std::find_if(packs.begin(), packs.end(),
        [](const PackInfo& p) { return p.name == "TestPack"; });

    REQUIRE(it != packs.end());
    CHECK(it->displayName == "Test Pack");
    CHECK(it->description == "Embedded fixture for FactoryPackLoadingTests");
    CHECK(it->isReadOnly == true);
    CHECK(it->presetCount == 1);
}

TEST_CASE("FactoryPackLoading: embedded preset appears in getAllPresets", "[factory-pack]")
{
    PresetManager pm;
    pm.initialize();

    const auto all = pm.getAllPresets();
    auto packIt = all.find("TestPack");
    REQUIRE(packIt != all.end());
    REQUIRE(packIt->second.size() == 1);

    const auto& preset = packIt->second.front();
    CHECK(preset.metadata.name == "Empty");
    CHECK(preset.metadata.designer == "Kaigen Test");
    CHECK(preset.embeddedData.getSize() > 0);
    CHECK(preset.file.getFullPathName().isEmpty());
}
```

- [ ] **Step 6: Run the tests to confirm they fail (red phase)**

```powershell
cmake -B build -A x64
cmake --build build --config Release --target KaigenPhantomTests
./build/tests/Release/KaigenPhantomTests.exe "[factory-pack]"
```

Expected: tests build successfully but FAIL because `loadFactoryPacksFromBinaryData()` may not be wired through yet, OR pass if all Step 3 wiring is correct. If they fail with "no pack TestPack", the wiring needs verification; this is the safety net.

- [ ] **Step 7: Run the full test suite to ensure nothing else broke**

```powershell
./build/tests/Release/KaigenPhantomTests.exe
```

Expected: every test passes including the two new `[factory-pack]` tests.

- [ ] **Step 8: Commit**

```bash
git add Source/PresetManager.h Source/PresetManager.cpp \
        tests/CMakeLists.txt tests/FactoryPackLoadingTests.cpp \
        tests/fixtures/FactoryPacksTest/TestPack/pack.json \
        tests/fixtures/FactoryPacksTest/TestPack/Empty.fxp
git commit -m "feat: scan embedded factory packs into PresetManager

PresetInfo gains an embeddedData MemoryBlock for presets sourced from
juce_add_binary_data; PackInfo gains an isReadOnly flag (true for
embedded packs and for the on-disk Factory pack). A new
loadFactoryPacksFromBinaryData() walks the KaigenFactoryPacks namespace
and stages PackInfo + PresetInfo entries alongside the disk scan.

Includes a test-only binary fixture (tests/fixtures/FactoryPacksTest)
and Catch2 tests asserting discovery + metadata round-trip."
```

---

## Task 3: Load presets from embedded MemoryBlock

**Spec:** Component C — `loadPreset` reads from `embeddedData` when present, else from disk.

**Files:**
- Modify: `Source/PresetManager.cpp:381-407` (`loadPreset`)
- Modify: `tests/FactoryPackLoadingTests.cpp` (add load test)

- [ ] **Step 1: Write the failing test**

Append to `tests/FactoryPackLoadingTests.cpp`:

```cpp
#include "../Source/Parameters.h"
#include <juce_audio_processors/juce_audio_processors.h>

TEST_CASE("FactoryPackLoading: loadPreset reads from embedded data", "[factory-pack]")
{
    // Build a minimal APVTS matching the live state type name.
    juce::AudioProcessor dummyProc;
    juce::AudioProcessorValueTreeState apvts(dummyProc, nullptr, "KaigenPhantomState", {});

    PresetManager pm;
    pm.initialize();

    const bool ok = pm.loadPreset(apvts, "Empty", "TestPack");
    REQUIRE(ok);
    CHECK(apvts.state.getType().toString() == "KaigenPhantomState");
    CHECK(apvts.state.getChildWithName("Metadata").isValid());
}
```

Note: the test uses a bare `juce::AudioProcessor`; if Catch2 cannot instantiate one due to the abstract base, replace with an inline subclass that stubs all pure-virtuals to no-ops — the existing test suite has precedent (see `tests/EngineFocusTests.cpp` or `tests/EditorViewStateTests.cpp` for the dummy-processor pattern). Use that exact pattern.

- [ ] **Step 2: Run the test, expect failure**

```powershell
cmake --build build --config Release --target KaigenPhantomTests
./build/tests/Release/KaigenPhantomTests.exe "[factory-pack]"
```

Expected: FAIL — `loadPreset` returns false for embedded packs (the file path doesn't exist on disk).

- [ ] **Step 3: Modify `PresetManager::loadPreset` to handle embedded data**

Replace `loadPreset()` in `Source/PresetManager.cpp:381-407` with:

```cpp
bool PresetManager::loadPreset(juce::AudioProcessorValueTreeState& apvts,
                               const juce::String& presetName,
                               const juce::String& packName)
{
    std::unique_ptr<juce::XmlElement> xml;

    // Check in-memory cache first — embedded factory packs live entirely
    // in BinaryData and have no disk file. We look the entry up by name
    // so we can read from its embeddedData MemoryBlock.
    auto packIt = allPresets.find(packName);
    if (packIt != allPresets.end())
    {
        const auto& list = packIt->second;
        auto entryIt = std::find_if(list.begin(), list.end(),
            [&](const PresetInfo& p) { return p.metadata.name == presetName; });
        if (entryIt != list.end() && entryIt->embeddedData.getSize() > 0)
        {
            xml = juce::parseXML(juce::String::createStringFromData(
                entryIt->embeddedData.getData(),
                (int) entryIt->embeddedData.getSize()));
        }
    }

    // Fall through to the on-disk path when not found in embedded data.
    if (xml == nullptr)
    {
        auto file = getPresetFile(presetName, packName);
        if (!file.existsAsFile()) return false;
        xml = juce::parseXML(file);
    }

    if (xml == nullptr) return false;

    auto tree = juce::ValueTree::fromXml(*xml);
    if (!tree.isValid()) return false;

    PresetMigration::migrateInPlace(tree);

    if (tree.getType() != apvts.state.getType())
        return false;

    apvts.replaceState(tree);
    return true;
}
```

- [ ] **Step 4: Run the test, expect pass**

```powershell
cmake --build build --config Release --target KaigenPhantomTests
./build/tests/Release/KaigenPhantomTests.exe "[factory-pack]"
```

Expected: all three `[factory-pack]` tests pass.

- [ ] **Step 5: Run the full test suite**

```powershell
./build/tests/Release/KaigenPhantomTests.exe
```

Expected: every test passes.

- [ ] **Step 6: Commit**

```bash
git add Source/PresetManager.cpp tests/FactoryPackLoadingTests.cpp
git commit -m "feat: loadPreset reads embedded factory-pack bytes

When a preset belongs to an embedded pack (PresetInfo::embeddedData is
non-empty), loadPreset now parses the XML from the MemoryBlock instead
of failing the on-disk file check. Falls through to the existing disk
path otherwise — disk presets and disk Factory continue to work
unchanged."
```

---

## Task 4: PackArchive helper (.kaipack export + import)

**Spec:** Component B — `.kaipack` format and the export/import primitives. `juce::ZipFile` for reads, `juce::ZipFile::Builder` for writes.

**Files:**
- Create: `Source/Pack/PackArchive.h`
- Create: `Source/Pack/PackArchive.cpp`
- Modify: `CMakeLists.txt:42-93` (`target_sources` block — add `Source/Pack/PackArchive.cpp`)
- Modify: `tests/CMakeLists.txt` (add `PackArchive.cpp` to the test executable and `PackArchiveTests.cpp`)
- Create: `tests/PackArchiveTests.cpp`

- [ ] **Step 1: Create `Source/Pack/PackArchive.h`**

```cpp
#pragma once
#include <juce_core/juce_core.h>

namespace kaigen::phantom
{

// PackArchive — small wrapper around juce::ZipFile / ZipFile::Builder for
// reading and writing .kaipack archives. A .kaipack is a renamed ZIP whose
// top-level entry is a single folder named after the pack:
//   <pack-name>/pack.json
//   <pack-name>/cover.png        (optional)
//   <pack-name>/*.fxp
class PackArchive
{
public:
    // Zip every file inside packDir into destZipFile. The archive entries
    // are stored with paths relative to packDir.getParentDirectory() so the
    // pack-name folder is preserved inside the zip.
    // Returns true on success; false if packDir is missing or write fails.
    static bool exportPack(const juce::File& packDir, const juce::File& destZipFile);

    // Inspect a .kaipack: return the pack-folder name (the first path
    // segment of any entry), or an empty string if the archive is empty
    // or malformed.
    static juce::String peekPackName(const juce::File& sourceZipFile);

    // Unzip a .kaipack into destRoot. The pack-name folder from inside the
    // archive becomes a sibling subdirectory of destRoot.
    // overwriteExisting=true wipes any prior <destRoot>/<packName>/ before
    // extraction. Returns the pack name on success, empty string on failure.
    static juce::String importPack(const juce::File& sourceZipFile,
                                   const juce::File& destRoot,
                                   bool overwriteExisting);
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Create `Source/Pack/PackArchive.cpp`**

```cpp
#include "PackArchive.h"

namespace kaigen::phantom
{

bool PackArchive::exportPack(const juce::File& packDir, const juce::File& destZipFile)
{
    if (! packDir.isDirectory()) return false;

    juce::ZipFile::Builder builder;

    // Find every regular file under packDir recursively.
    juce::Array<juce::File> files;
    packDir.findChildFiles(files, juce::File::findFiles, /*recursive*/ true);

    for (const auto& f : files)
    {
        // Store path relative to packDir's parent so the archive contains
        // <pack-name>/<rel-path> entries.
        const auto rel = f.getRelativePathFrom(packDir.getParentDirectory());
        // JUCE's ZipFile uses forward slashes regardless of host OS.
        const auto normalized = rel.replaceCharacter('\\', '/');
        builder.addFile(f, /*compression*/ 9, normalized);
    }

    if (! destZipFile.getParentDirectory().createDirectory().wasOk())
        return false;

    destZipFile.deleteFile();
    juce::FileOutputStream out(destZipFile);
    if (! out.openedOk()) return false;

    return builder.writeToStream(out, nullptr);
}

juce::String PackArchive::peekPackName(const juce::File& sourceZipFile)
{
    if (! sourceZipFile.existsAsFile()) return {};

    juce::ZipFile zip(sourceZipFile);
    if (zip.getNumEntries() == 0) return {};

    const auto* entry = zip.getEntry(0);
    if (entry == nullptr) return {};

    const auto firstSlash = entry->filename.indexOfChar('/');
    if (firstSlash < 1) return {};
    return entry->filename.substring(0, firstSlash);
}

juce::String PackArchive::importPack(const juce::File& sourceZipFile,
                                     const juce::File& destRoot,
                                     bool overwriteExisting)
{
    if (! sourceZipFile.existsAsFile()) return {};
    if (! destRoot.isDirectory() && ! destRoot.createDirectory().wasOk()) return {};

    juce::ZipFile zip(sourceZipFile);
    if (zip.getNumEntries() == 0) return {};

    const auto packName = peekPackName(sourceZipFile);
    if (packName.isEmpty()) return {};

    auto packDir = destRoot.getChildFile(packName);
    if (packDir.exists())
    {
        if (! overwriteExisting) return {};
        if (! packDir.deleteRecursively()) return {};
    }
    if (! packDir.createDirectory().wasOk()) return {};

    const auto result = zip.uncompressTo(destRoot, /*shouldOverwriteFiles*/ true);
    if (result.failed()) return {};

    return packName;
}

} // namespace kaigen::phantom
```

- [ ] **Step 3: Add `Source/Pack/PackArchive.cpp` to the product target**

In `CMakeLists.txt`, inside the `target_sources(KaigenPhantom PRIVATE ...)` block (lines 42-93), add a line:

```cmake
    Source/Pack/PackArchive.cpp
```

(Place it alphabetically — after `Source/PresetMigration.cpp` is fine, or grouped near `Source/PresetManager.cpp` for readability.)

- [ ] **Step 4: Wire PackArchive into the test target**

In `tests/CMakeLists.txt`, append to the `add_executable(KaigenPhantomTests ...)` source list (just before the closing `)`):

```cmake
    ../Source/Pack/PackArchive.cpp
    PackArchiveTests.cpp
```

- [ ] **Step 5: Write the failing test**

Create `tests/PackArchiveTests.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "../Source/Pack/PackArchive.h"

using namespace kaigen::phantom;

namespace
{
    // Build a temporary pack folder with pack.json + one .fxp + cover.png.
    juce::File makeFixturePack(const juce::File& parent)
    {
        auto packDir = parent.getChildFile("FixturePack");
        packDir.deleteRecursively();
        packDir.createDirectory();

        packDir.getChildFile("pack.json").replaceWithText(
            R"({"name":"FixturePack","description":"d","designer":"me"})");
        packDir.getChildFile("Preset One.fxp").replaceWithText(
            "<KaigenPhantomState/>");
        packDir.getChildFile("cover.png").replaceWithData(
            "\x89PNG\r\n\x1a\n", 8);
        return packDir;
    }
}

TEST_CASE("PackArchive: exportPack writes a valid .kaipack containing the pack folder", "[pack-archive]")
{
    auto tmp = juce::File::createTempFile("kaipack-test");
    tmp.deleteFile();
    tmp.createDirectory();

    const auto packDir = makeFixturePack(tmp);
    const auto destZip = tmp.getChildFile("FixturePack.kaipack");

    REQUIRE(PackArchive::exportPack(packDir, destZip));
    REQUIRE(destZip.existsAsFile());
    REQUIRE(destZip.getSize() > 0);
    CHECK(PackArchive::peekPackName(destZip) == "FixturePack");

    tmp.deleteRecursively();
}

TEST_CASE("PackArchive: roundtrip preserves files and metadata", "[pack-archive]")
{
    auto tmp = juce::File::createTempFile("kaipack-rt");
    tmp.deleteFile();
    tmp.createDirectory();

    const auto packDir = makeFixturePack(tmp);
    const auto destZip = tmp.getChildFile("Roundtrip.kaipack");
    REQUIRE(PackArchive::exportPack(packDir, destZip));

    // Import into a fresh sibling folder.
    auto importRoot = tmp.getChildFile("imported");
    importRoot.createDirectory();
    const auto packName = PackArchive::importPack(destZip, importRoot, /*overwrite*/ true);
    REQUIRE(packName == "FixturePack");

    const auto extracted = importRoot.getChildFile("FixturePack");
    CHECK(extracted.getChildFile("pack.json").existsAsFile());
    CHECK(extracted.getChildFile("Preset One.fxp").existsAsFile());
    CHECK(extracted.getChildFile("cover.png").existsAsFile());

    tmp.deleteRecursively();
}

TEST_CASE("PackArchive: importPack refuses to overwrite when flag is false", "[pack-archive]")
{
    auto tmp = juce::File::createTempFile("kaipack-noov");
    tmp.deleteFile();
    tmp.createDirectory();

    const auto packDir = makeFixturePack(tmp);
    const auto destZip = tmp.getChildFile("Pack.kaipack");
    REQUIRE(PackArchive::exportPack(packDir, destZip));

    auto importRoot = tmp.getChildFile("imported");
    importRoot.createDirectory();
    REQUIRE(PackArchive::importPack(destZip, importRoot, /*overwrite*/ true) == "FixturePack");
    // Second import with overwrite=false must fail.
    CHECK(PackArchive::importPack(destZip, importRoot, /*overwrite*/ false).isEmpty());

    tmp.deleteRecursively();
}

TEST_CASE("PackArchive: malformed archive returns empty pack name", "[pack-archive]")
{
    auto tmp = juce::File::createTempFile("kaipack-bad");
    tmp.deleteFile();
    tmp.replaceWithText("not a zip");

    CHECK(PackArchive::peekPackName(tmp).isEmpty());
    CHECK(PackArchive::importPack(tmp, tmp.getParentDirectory(), true).isEmpty());

    tmp.deleteFile();
}
```

- [ ] **Step 6: Run the failing test**

```powershell
cmake -B build -A x64
cmake --build build --config Release --target KaigenPhantomTests
./build/tests/Release/KaigenPhantomTests.exe "[pack-archive]"
```

Expected: FAIL (link error or test failure since PackArchive.cpp may not be in the build yet — Steps 1-4 should resolve that, so any compile error here is a wiring issue to fix before the implementation steps).

- [ ] **Step 7: After Steps 1-4 implementation is in, run the test, expect pass**

```powershell
./build/tests/Release/KaigenPhantomTests.exe "[pack-archive]"
```

Expected: all four `[pack-archive]` tests pass.

- [ ] **Step 8: Run the full test suite**

```powershell
./build/tests/Release/KaigenPhantomTests.exe
```

Expected: every test passes.

- [ ] **Step 9: Commit**

```bash
git add Source/Pack/PackArchive.h Source/Pack/PackArchive.cpp CMakeLists.txt \
        tests/CMakeLists.txt tests/PackArchiveTests.cpp
git commit -m "feat: PackArchive helper for .kaipack export and import

Small JUCE ZipFile / ZipFile::Builder wrapper. exportPack zips a pack
folder into a .kaipack (pack-name folder preserved inside the archive);
importPack unzips a .kaipack into a destination root, with an overwrite
guard. peekPackName inspects the archive without unpacking. Includes
roundtrip + overwrite-refuse + malformed-archive Catch2 tests."
```

---

## Task 5: PresetManager DEV-only pack management API (create / rename / metadata / delete)

**Spec:** Component A — the four pack-management ops that touch only directories and pack.json, not preset files. All gated by `#if DEVELOPER_MODE`.

**Files:**
- Modify: `Source/PresetManager.h` (add DEV-only public API declarations)
- Modify: `Source/PresetManager.cpp` (implementations)
- Modify: `tests/CMakeLists.txt` (add `PresetManagerAuthoringTests.cpp` + define `DEVELOPER_MODE=1` for the test target)
- Create: `tests/PresetManagerAuthoringTests.cpp`

- [ ] **Step 1: Add DEV-only API to `Source/PresetManager.h`**

Inside the `PresetManager` public block, after the existing `deletePreset` declaration (around line 116), insert:

```cpp
#if DEVELOPER_MODE
    // ── Pack authoring (designer build only) ──────────────────────────
    //
    // All authoring ops refuse PackInfo::isReadOnly packs (embedded
    // factory packs + the on-disk Factory pack). Names are sanitised to
    // safe folder names. Each successful op triggers a rescan + change
    // broadcast so the browser refreshes.

    bool createPack(const juce::String& packName,
                    const juce::String& description,
                    const juce::String& designer);

    bool renamePack(const juce::String& oldName, const juce::String& newName);

    bool setPackMetadata(const juce::String& packName,
                         const juce::String& description,
                         const juce::String& designer);

    bool deletePack(const juce::String& packName);
#endif
```

- [ ] **Step 2: Add the sanitiser helper and pack-folder accessor to `Source/PresetManager.cpp`**

Inside the anonymous namespace at lines 9-24, add a folder-name sanitiser beside `sanitizeName`:

```cpp
    juce::String sanitizePackFolderName(const juce::String& name)
    {
        // Same charset as preset names — no path separators, no oddballs.
        auto s = name.trim().retainCharacters(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-()[]");
        // Reserve the special pack names; refuse them outright.
        if (s.equalsIgnoreCase("Factory") || s.equalsIgnoreCase("User"))
            return {};
        return s;
    }

    juce::File writePackManifest(const juce::File& packDir,
                                  const juce::String& displayName,
                                  const juce::String& description,
                                  const juce::String& designer)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("name",        displayName);
        obj->setProperty("description", description);
        obj->setProperty("designer",    designer);

        auto json = juce::JSON::toString(juce::var(obj));
        auto manifest = packDir.getChildFile("pack.json");
        manifest.replaceWithText(json);
        return manifest;
    }
```

- [ ] **Step 3: Implement the four DEV-only functions in `Source/PresetManager.cpp`**

Append to the bottom of `Source/PresetManager.cpp`, just before the closing namespace brace:

```cpp
#if DEVELOPER_MODE
// ── Pack authoring (designer build only) ───────────────────────────────

bool PresetManager::createPack(const juce::String& packName,
                                const juce::String& description,
                                const juce::String& designer)
{
    const auto sanitized = sanitizePackFolderName(packName);
    if (sanitized.isEmpty()) return false;

    auto packDir = getPresetsRootDirectory().getChildFile(sanitized);
    if (packDir.exists()) return false;
    if (! packDir.createDirectory().wasOk()) return false;

    writePackManifest(packDir, packName.trim(), description, designer);

    rescan();
    return true;
}

bool PresetManager::renamePack(const juce::String& oldName, const juce::String& newName)
{
    auto packIt = packs.find(oldName);
    if (packIt == packs.end() || packIt->second.isReadOnly) return false;

    const auto sanitized = sanitizePackFolderName(newName);
    if (sanitized.isEmpty()) return false;

    auto oldDir = getPresetsRootDirectory().getChildFile(oldName);
    auto newDir = getPresetsRootDirectory().getChildFile(sanitized);
    if (! oldDir.isDirectory()) return false;
    if (newDir.exists()) return false;

    if (! oldDir.moveFileTo(newDir)) return false;

    // Update pack.json displayName so the in-app name follows the folder.
    writePackManifest(newDir,
                       newName.trim(),
                       packIt->second.description,
                       packIt->second.designer);

    rescan();
    return true;
}

bool PresetManager::setPackMetadata(const juce::String& packName,
                                     const juce::String& description,
                                     const juce::String& designer)
{
    auto packIt = packs.find(packName);
    if (packIt == packs.end() || packIt->second.isReadOnly) return false;

    auto packDir = getPresetsRootDirectory().getChildFile(packName);
    if (! packDir.isDirectory()) return false;

    writePackManifest(packDir, packIt->second.displayName, description, designer);

    rescan();
    return true;
}

bool PresetManager::deletePack(const juce::String& packName)
{
    auto packIt = packs.find(packName);
    if (packIt == packs.end() || packIt->second.isReadOnly) return false;
    if (packName == kFactoryPackName || packName == kUserPackName) return false;

    auto packDir = getPresetsRootDirectory().getChildFile(packName);
    if (! packDir.isDirectory()) return false;
    if (! packDir.deleteRecursively()) return false;

    rescan();
    return true;
}
#endif
```

- [ ] **Step 4: Add DEVELOPER_MODE define + new test file to `tests/CMakeLists.txt`**

After the `add_executable(KaigenPhantomTests ...)` block, append:

```cmake
target_compile_definitions(KaigenPhantomTests PRIVATE DEVELOPER_MODE=1)
target_sources(KaigenPhantomTests PRIVATE PresetManagerAuthoringTests.cpp)
```

Note: the existing `KAIGEN_HAS_FACTORY_PACKS=1` definition added in Task 2 stays as-is.

- [ ] **Step 5: Write the failing test**

Create `tests/PresetManagerAuthoringTests.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "../Source/PresetManager.h"

using namespace kaigen::phantom;

namespace
{
    // Helper: each test gets a clean User-root sandbox so we don't pollute
    // the developer machine's real preset library. PresetManager hardcodes
    // its root via getSpecialLocation; we redirect by isolating into a
    // unique subfolder and asserting only against names we create here.
    void cleanupPack(const juce::String& packName)
    {
        // No-op — tests use uniquely-named packs ("AuthTest-xxxx") so we
        // don't have to teardown disk state shared with other tests.
        (void) packName;
    }
}

TEST_CASE("PresetManager DEV API: createPack creates a folder + manifest", "[pm-dev]")
{
    PresetManager pm;
    pm.initialize();

    const juce::String packName = "AuthTest-Create-"
        + juce::String(juce::Time::getCurrentTime().toMilliseconds());

    REQUIRE(pm.createPack(packName, "Some description", "Me"));

    const auto packs = pm.getAllPacks();
    auto it = std::find_if(packs.begin(), packs.end(),
        [&](const PackInfo& p) { return p.name == packName; });

    REQUIRE(it != packs.end());
    CHECK(it->description == "Some description");
    CHECK(it->designer == "Me");

    pm.deletePack(packName);  // teardown
}

TEST_CASE("PresetManager DEV API: createPack refuses reserved names", "[pm-dev]")
{
    PresetManager pm;
    pm.initialize();

    CHECK_FALSE(pm.createPack("Factory", "x", "x"));
    CHECK_FALSE(pm.createPack("User",    "x", "x"));
    CHECK_FALSE(pm.createPack("",        "x", "x"));
}

TEST_CASE("PresetManager DEV API: renamePack moves folder + updates manifest", "[pm-dev]")
{
    PresetManager pm;
    pm.initialize();

    const juce::String oldName = "AuthTest-Old-"
        + juce::String(juce::Time::getCurrentTime().toMilliseconds());
    const juce::String newName = oldName + "-Renamed";

    REQUIRE(pm.createPack(oldName, "d", "me"));
    REQUIRE(pm.renamePack(oldName, newName));

    const auto packs = pm.getAllPacks();
    auto oldIt = std::find_if(packs.begin(), packs.end(),
        [&](const PackInfo& p) { return p.name == oldName; });
    auto newIt = std::find_if(packs.begin(), packs.end(),
        [&](const PackInfo& p) { return p.name == newName; });

    CHECK(oldIt == packs.end());
    REQUIRE(newIt != packs.end());

    pm.deletePack(newName);  // teardown
}

TEST_CASE("PresetManager DEV API: renamePack refuses readonly packs", "[pm-dev]")
{
    PresetManager pm;
    pm.initialize();

    CHECK_FALSE(pm.renamePack("Factory", "Renamed"));
}

TEST_CASE("PresetManager DEV API: setPackMetadata updates pack.json", "[pm-dev]")
{
    PresetManager pm;
    pm.initialize();

    const juce::String packName = "AuthTest-Meta-"
        + juce::String(juce::Time::getCurrentTime().toMilliseconds());
    REQUIRE(pm.createPack(packName, "old desc", "old designer"));

    REQUIRE(pm.setPackMetadata(packName, "new desc", "new designer"));

    const auto packs = pm.getAllPacks();
    auto it = std::find_if(packs.begin(), packs.end(),
        [&](const PackInfo& p) { return p.name == packName; });
    REQUIRE(it != packs.end());
    CHECK(it->description == "new desc");
    CHECK(it->designer == "new designer");

    pm.deletePack(packName);  // teardown
}

TEST_CASE("PresetManager DEV API: deletePack removes folder", "[pm-dev]")
{
    PresetManager pm;
    pm.initialize();

    const juce::String packName = "AuthTest-Del-"
        + juce::String(juce::Time::getCurrentTime().toMilliseconds());
    REQUIRE(pm.createPack(packName, "d", "me"));
    REQUIRE(pm.deletePack(packName));

    const auto packs = pm.getAllPacks();
    auto it = std::find_if(packs.begin(), packs.end(),
        [&](const PackInfo& p) { return p.name == packName; });
    CHECK(it == packs.end());
}

TEST_CASE("PresetManager DEV API: deletePack refuses Factory/User/embedded", "[pm-dev]")
{
    PresetManager pm;
    pm.initialize();

    CHECK_FALSE(pm.deletePack("Factory"));
    CHECK_FALSE(pm.deletePack("User"));
    CHECK_FALSE(pm.deletePack("TestPack"));  // embedded fixture from Task 2
}
```

- [ ] **Step 6: Run the failing test**

```powershell
cmake -B build -A x64
cmake --build build --config Release --target KaigenPhantomTests
./build/tests/Release/KaigenPhantomTests.exe "[pm-dev]"
```

Expected: tests fail because `createPack`/`renamePack`/etc. don't link until Step 3's implementation is compiled. After Step 3 the tests should pass.

- [ ] **Step 7: Run the test, expect pass**

```powershell
cmake --build build --config Release --target KaigenPhantomTests
./build/tests/Release/KaigenPhantomTests.exe "[pm-dev]"
```

Expected: every `[pm-dev]` test passes.

- [ ] **Step 8: Run the full test suite**

```powershell
./build/tests/Release/KaigenPhantomTests.exe
```

Expected: every test passes.

- [ ] **Step 9: Commit**

```bash
git add Source/PresetManager.h Source/PresetManager.cpp \
        tests/CMakeLists.txt tests/PresetManagerAuthoringTests.cpp
git commit -m "feat: DEV-only PresetManager pack authoring API (create/rename/meta/delete)

Four authoring functions guarded by #if DEVELOPER_MODE: createPack,
renamePack, setPackMetadata, deletePack. Each refuses reserved names
(Factory/User) and PackInfo::isReadOnly packs (embedded factory packs);
each rescans and broadcasts on success so the browser refreshes.
Covered by Catch2 tests using uniquely-named sandbox packs."
```

---

## Task 6: PresetManager DEV-only preset/cover/export/import API

**Spec:** Component A — the four ops that touch preset files, cover art, and `.kaipack` files. Also DEV-only.

**Files:**
- Modify: `Source/PresetManager.h` (add the four declarations)
- Modify: `Source/PresetManager.cpp` (implementations using `PackArchive`)
- Modify: `tests/PresetManagerAuthoringTests.cpp` (add tests for the new ops)

- [ ] **Step 1: Extend `Source/PresetManager.h`**

Add to the `#if DEVELOPER_MODE` block from Task 5 (so the block now contains 8 functions):

```cpp
    // Copies + resizes the source image to <packDir>/cover.png. Resizes
    // to <= 512x512, preserving aspect ratio. Replaces any existing cover.
    bool setPackCover(const juce::String& packName, const juce::File& sourceImage);

    // Saves the current APVTS state as <packDir>/<presetName>.fxp.
    // Returns the saved (possibly disambiguated) preset name, empty on failure.
    juce::String savePresetIntoPack(juce::AudioProcessorValueTreeState& apvts,
                                    const juce::String& packName,
                                    const juce::String& presetName,
                                    const juce::String& type,
                                    const juce::String& designer,
                                    const juce::String& description);

    // Zips the pack folder into destZipFile. Returns true on success.
    bool exportPack(const juce::String& packName, const juce::File& destZipFile);

    // Unzips a .kaipack into the User-presets root. Returns the imported
    // pack name on success; empty string if archive is invalid or the
    // pack already exists and overwriteExisting is false.
    juce::String importPack(const juce::File& sourceZipFile, bool overwriteExisting);
```

Add `#include "Pack/PackArchive.h"` near the existing includes (the `#include "PresetMigration.h"` line at the top of `PresetManager.cpp` is a good neighbour — but the header doesn't strictly need this include if `PackArchive` is only used in the .cpp; in that case add the include in the .cpp instead).

- [ ] **Step 2: Implement the four functions in `Source/PresetManager.cpp`**

Add `#include "Pack/PackArchive.h"` at the top with the other includes.

Append to the `#if DEVELOPER_MODE` block at the bottom of `Source/PresetManager.cpp` (below the four functions from Task 5, still inside the `#if`):

```cpp
bool PresetManager::setPackCover(const juce::String& packName, const juce::File& sourceImage)
{
    auto packIt = packs.find(packName);
    if (packIt == packs.end() || packIt->second.isReadOnly) return false;
    if (! sourceImage.existsAsFile()) return false;

    auto packDir = getPresetsRootDirectory().getChildFile(packName);
    if (! packDir.isDirectory()) return false;

    // Load -> resize to fit 512x512 -> write as PNG. Format is forced to
    // PNG regardless of source extension so the rest of the code only
    // needs to look for cover.png / cover.jpg.
    auto img = juce::ImageFileFormat::loadFrom(sourceImage);
    if (! img.isValid()) return false;

    const int maxEdge = 512;
    if (img.getWidth() > maxEdge || img.getHeight() > maxEdge)
    {
        const float scale = (float) maxEdge
            / (float) juce::jmax(img.getWidth(), img.getHeight());
        img = img.rescaled((int) (img.getWidth()  * scale),
                            (int) (img.getHeight() * scale),
                            juce::Graphics::highResamplingQuality);
    }

    // Remove a stale cover.jpg if present so the new cover.png wins.
    packDir.getChildFile("cover.jpg").deleteFile();

    auto destPng = packDir.getChildFile("cover.png");
    destPng.deleteFile();
    juce::FileOutputStream out(destPng);
    if (! out.openedOk()) return false;

    juce::PNGImageFormat fmt;
    if (! fmt.writeImageToStream(img, out)) return false;
    out.flush();

    rescan();
    return true;
}

juce::String PresetManager::savePresetIntoPack(juce::AudioProcessorValueTreeState& apvts,
                                                const juce::String& packName,
                                                const juce::String& presetName,
                                                const juce::String& type,
                                                const juce::String& designer,
                                                const juce::String& description)
{
    auto packIt = packs.find(packName);
    if (packIt == packs.end() || packIt->second.isReadOnly) return {};

    auto sanitized = sanitizeName(presetName);
    if (sanitized.isEmpty()) return {};

    const auto validType = kValidTypes.contains(type) ? type
                                                       : juce::String("Experimental");
    const auto effectiveDesigner = designer.isEmpty() ? juce::String("User") : designer;

    auto packDir = getPresetsRootDirectory().getChildFile(packName);
    if (! packDir.isDirectory()) return {};

    auto target = packDir.getChildFile(sanitized + ".fxp");
    // Disambiguate name with a numeric suffix if the file already exists.
    if (target.existsAsFile())
    {
        int suffix = 2;
        while (true)
        {
            auto candidate = packDir.getChildFile(sanitized + " "
                + juce::String(suffix) + ".fxp");
            if (! candidate.existsAsFile())
            {
                target = candidate;
                sanitized = sanitized + " " + juce::String(suffix);
                break;
            }
            if (++suffix > 999) return {};
        }
    }

    juce::ValueTree state = apvts.copyState();
    if (auto existingMeta = state.getChildWithName(kMetadataNodeId); existingMeta.isValid())
        state.removeChild(existingMeta, nullptr);

    auto metadataTree = buildMetadataTree(sanitized, validType, effectiveDesigner, description);
    metadataTree.setProperty("presetKind", presetKindToString(PresetKind::Single), nullptr);
    state.appendChild(metadataTree, nullptr);

    auto xml = state.createXml();
    if (xml == nullptr) return {};
    if (! target.replaceWithText(xml->toString())) return {};

    rescan();
    return sanitized;
}

bool PresetManager::exportPack(const juce::String& packName, const juce::File& destZipFile)
{
    auto packIt = packs.find(packName);
    // Note: exportPack works on read-only packs too — designers can export
    // a Factory/embedded pack for inspection if they want. The interesting
    // restriction is on import (next).
    if (packIt == packs.end()) return false;

    auto packDir = getPresetsRootDirectory().getChildFile(packName);
    if (! packDir.isDirectory()) return false;

    return PackArchive::exportPack(packDir, destZipFile);
}

juce::String PresetManager::importPack(const juce::File& sourceZipFile, bool overwriteExisting)
{
    const auto packName = PackArchive::peekPackName(sourceZipFile);
    if (packName.isEmpty()) return {};

    // Refuse to import on top of a read-only pack — embedded factory
    // packs shouldn't be shadowed by user-imported ones (it would just
    // be confusing).
    auto packIt = packs.find(packName);
    if (packIt != packs.end() && packIt->second.isReadOnly) return {};
    if (packName == "Factory" || packName == "User") return {};

    const auto result = PackArchive::importPack(sourceZipFile,
                                                 getPresetsRootDirectory(),
                                                 overwriteExisting);
    if (! result.isEmpty()) rescan();
    return result;
}
```

- [ ] **Step 3: Add the include in `Source/PresetManager.cpp`**

Near the existing includes at the top of `Source/PresetManager.cpp` (after `#include "PresetMigration.h"`), add:

```cpp
#include "Pack/PackArchive.h"
```

- [ ] **Step 4: Add tests**

Append to `tests/PresetManagerAuthoringTests.cpp`:

```cpp
TEST_CASE("PresetManager DEV API: setPackCover writes cover.png", "[pm-dev]")
{
    PresetManager pm;
    pm.initialize();

    const juce::String packName = "AuthTest-Cover-"
        + juce::String(juce::Time::getCurrentTime().toMilliseconds());
    REQUIRE(pm.createPack(packName, "d", "me"));

    // Build a tiny PNG on disk to use as the source.
    auto tmpDir = juce::File::createTempFile("kp-cover-src");
    tmpDir.deleteFile();
    tmpDir.createDirectory();
    auto src = tmpDir.getChildFile("art.png");
    {
        juce::Image img(juce::Image::RGB, 32, 32, true);
        juce::FileOutputStream out(src);
        juce::PNGImageFormat fmt;
        REQUIRE(out.openedOk());
        REQUIRE(fmt.writeImageToStream(img, out));
    }

    REQUIRE(pm.setPackCover(packName, src));
    CHECK(pm.getPackCoverFile(packName).existsAsFile());

    pm.deletePack(packName);
    tmpDir.deleteRecursively();
}

TEST_CASE("PresetManager DEV API: savePresetIntoPack writes into the target pack", "[pm-dev]")
{
    PresetManager pm;
    pm.initialize();

    const juce::String packName = "AuthTest-Save-"
        + juce::String(juce::Time::getCurrentTime().toMilliseconds());
    REQUIRE(pm.createPack(packName, "d", "me"));

    juce::AudioProcessorValueTreeState apvts(
        *static_cast<juce::AudioProcessor*>(nullptr) /* see note below */,
        nullptr, "KaigenPhantomState", {});
    // The DummyProcessor pattern from other tests in this directory should
    // be used here — the line above is a placeholder. See
    // tests/EngineFocusTests.cpp for the canonical no-op processor.

    const auto saved = pm.savePresetIntoPack(apvts, packName,
                                              "MyPreset", "Synth", "me", "");
    REQUIRE(saved == "MyPreset");

    const auto all = pm.getAllPresets();
    auto it = all.find(packName);
    REQUIRE(it != all.end());
    CHECK(it->second.size() == 1);
    CHECK(it->second.front().metadata.name == "MyPreset");

    pm.deletePack(packName);
}

TEST_CASE("PresetManager DEV API: exportPack + importPack roundtrip", "[pm-dev]")
{
    PresetManager pm;
    pm.initialize();

    const juce::String packName = "AuthTest-RT-"
        + juce::String(juce::Time::getCurrentTime().toMilliseconds());
    REQUIRE(pm.createPack(packName, "d", "me"));

    auto tmp = juce::File::createTempFile("kp-export");
    tmp.deleteFile();
    tmp.createDirectory();
    const auto zip = tmp.getChildFile(packName + ".kaipack");
    REQUIRE(pm.exportPack(packName, zip));
    REQUIRE(zip.existsAsFile());

    // Delete the live pack, then import the .kaipack back.
    REQUIRE(pm.deletePack(packName));
    const auto imported = pm.importPack(zip, /*overwrite*/ false);
    CHECK(imported == packName);

    pm.deletePack(packName);
    tmp.deleteRecursively();
}

TEST_CASE("PresetManager DEV API: importPack refuses overwriting embedded packs", "[pm-dev]")
{
    PresetManager pm;
    pm.initialize();

    // Manufacture a fake .kaipack whose top folder is "TestPack" (the
    // embedded fixture name) — peekPackName should match and importPack
    // should refuse.
    auto tmp = juce::File::createTempFile("kp-conflict");
    tmp.deleteFile();
    tmp.createDirectory();
    auto fakePack = tmp.getChildFile("TestPack");
    fakePack.createDirectory();
    fakePack.getChildFile("pack.json").replaceWithText(R"({"name":"TestPack"})");

    auto zip = tmp.getChildFile("TestPack.kaipack");
    REQUIRE(PackArchive::exportPack(fakePack, zip));

    CHECK(pm.importPack(zip, /*overwrite*/ true).isEmpty());
    tmp.deleteRecursively();
}
```

(For the `savePresetIntoPack` test, follow the dummy-processor pattern in `tests/EngineFocusTests.cpp` or `tests/EditorViewStateTests.cpp` — open that file first to copy the exact subclass shape; do not use the placeholder `static_cast<AudioProcessor*>(nullptr)` shown above, which is just a marker for where the dummy goes.)

- [ ] **Step 5: Run the test, expect pass**

```powershell
cmake --build build --config Release --target KaigenPhantomTests
./build/tests/Release/KaigenPhantomTests.exe "[pm-dev]"
```

Expected: every `[pm-dev]` test passes.

- [ ] **Step 6: Run the full test suite**

```powershell
./build/tests/Release/KaigenPhantomTests.exe
```

Expected: every test passes.

- [ ] **Step 7: Commit**

```bash
git add Source/PresetManager.h Source/PresetManager.cpp \
        tests/PresetManagerAuthoringTests.cpp
git commit -m "feat: DEV-only PresetManager cover/save/export/import API

Four more authoring functions still guarded by #if DEVELOPER_MODE:
setPackCover (resizes to <=512 PNG), savePresetIntoPack (current APVTS
state -> <packDir>/<name>.fxp), exportPack (PackArchive zip), and
importPack (unzips into User root, refuses to shadow read-only packs).
Covered by Catch2 roundtrip + conflict tests."
```

---

## Task 7: PresetBrowser AUTHORING section + buttons

**Spec:** Component A — six authoring buttons + Import-for-review button in a DEV-only AUTHORING section beneath the existing preset list.

**Files:**
- Modify: `Source/UI/panels/PresetBrowser.h` (add the buttons as members under `#if DEVELOPER_MODE`)
- Modify: `Source/UI/panels/PresetBrowser.cpp` (paint + layout + wiring for the new section)

- [ ] **Step 1: Open `Source/UI/panels/PresetBrowser.cpp` and locate the existing button/layout code**

(This step is purely investigative — read the file once to understand where the existing `closeButton` and `deleteButton` are added in the constructor and laid out in `resized()`. The new AUTHORING section follows the same pattern.)

- [ ] **Step 2: Add the new button members to `Source/UI/panels/PresetBrowser.h`**

After the `juce::TextButton deleteButton { "Delete" };` line (around line 130), insert:

```cpp
#if DEVELOPER_MODE
    juce::TextButton newPackButton       { "+ New Pack" };
    juce::TextButton saveIntoPackButton  { "Save Into Pack" };
    juce::TextButton editPackMetaButton  { "Edit Metadata" };
    juce::TextButton setCoverButton      { "Set Cover" };
    juce::TextButton renamePackButton    { "Rename" };
    juce::TextButton deletePackButton    { "Delete Pack" };
    juce::TextButton exportPackButton    { "Export Pack" };
    juce::TextButton importPackButton    { "Import Pack" };

    // Bounds of the AUTHORING strip — laid out under the row table.
    juce::Rectangle<int> authoringStripBounds() const;

    // Returns the currently-selected pack from the sidebar, or empty when
    // the active sidebar entry is Explore / Favorites / Packs (not a
    // specific pack drill-in).
    juce::String activePackName() const;
#endif

    // Layout constant for the new strip.
    static constexpr int kAuthoringStripH = 56;
```

(Keep `kAuthoringStripH` outside the `#if` so layout math compiles either way; in non-DEV builds it'll just be unused space — but to avoid even that, gate it `#if DEVELOPER_MODE` too and reference it only inside DEV-guarded layout code in the .cpp.)

- [ ] **Step 3: Wire the buttons in the constructor in `Source/UI/panels/PresetBrowser.cpp`**

In the `PresetBrowser` constructor, after the `deleteButton` is added, insert a DEV-gated block. Read the file to find the right spot (search for `addAndMakeVisible(deleteButton)`); just after that line add:

```cpp
#if DEVELOPER_MODE
    auto setupAuthoringButton = [this](juce::TextButton& b)
    {
        b.setLookAndFeel(closeButton.getLookAndFeel());  // reuse the same flat look
        addAndMakeVisible(b);
    };
    setupAuthoringButton(newPackButton);
    setupAuthoringButton(saveIntoPackButton);
    setupAuthoringButton(editPackMetaButton);
    setupAuthoringButton(setCoverButton);
    setupAuthoringButton(renamePackButton);
    setupAuthoringButton(deletePackButton);
    setupAuthoringButton(exportPackButton);
    setupAuthoringButton(importPackButton);

    // === Button callbacks ===

    newPackButton.onClick = [this]
    {
        // Modal dialog implementation comes in Task 8 — for now, prompt
        // via AlertWindow with three text fields.
        auto* aw = new juce::AlertWindow("New Pack",
            "Create a new pack:", juce::AlertWindow::NoIcon);
        aw->addTextEditor("name",        "",  "Name:");
        aw->addTextEditor("description", "",  "Description:");
        aw->addTextEditor("designer",    "",  "Designer:");
        aw->addButton("Create", 1, juce::KeyPress(juce::KeyPress::returnKey));
        aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        aw->enterModalState(true, juce::ModalCallbackFunction::create(
            [this, aw](int r)
            {
                if (r == 1)
                {
                    processor.getPresetManager().createPack(
                        aw->getTextEditorContents("name"),
                        aw->getTextEditorContents("description"),
                        aw->getTextEditorContents("designer"));
                }
                delete aw;
            }), false);
    };

    saveIntoPackButton.onClick = [this]
    {
        const auto packName = activePackName();
        if (packName.isEmpty()) return;

        auto* aw = new juce::AlertWindow("Save Into " + packName,
            "Preset name:", juce::AlertWindow::NoIcon);
        aw->addTextEditor("name", "", "Name:");
        aw->addButton("Save",   1, juce::KeyPress(juce::KeyPress::returnKey));
        aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        aw->enterModalState(true, juce::ModalCallbackFunction::create(
            [this, aw, packName](int r)
            {
                if (r == 1)
                {
                    processor.getPresetManager().savePresetIntoPack(
                        apvts, packName,
                        aw->getTextEditorContents("name"),
                        "Experimental", "User", "");
                }
                delete aw;
            }), false);
    };

    editPackMetaButton.onClick = [this]
    {
        const auto packName = activePackName();
        if (packName.isEmpty()) return;

        // Find current values from the pack list.
        const auto packs = processor.getPresetManager().getAllPacks();
        auto it = std::find_if(packs.begin(), packs.end(),
            [&](const PackInfo& p) { return p.name == packName; });
        if (it == packs.end()) return;

        auto* aw = new juce::AlertWindow("Edit Pack",
            "Edit metadata for " + packName, juce::AlertWindow::NoIcon);
        aw->addTextEditor("description", it->description, "Description:");
        aw->addTextEditor("designer",    it->designer,    "Designer:");
        aw->addButton("Save",   1, juce::KeyPress(juce::KeyPress::returnKey));
        aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        aw->enterModalState(true, juce::ModalCallbackFunction::create(
            [this, aw, packName](int r)
            {
                if (r == 1)
                {
                    processor.getPresetManager().setPackMetadata(packName,
                        aw->getTextEditorContents("description"),
                        aw->getTextEditorContents("designer"));
                }
                delete aw;
            }), false);
    };

    setCoverButton.onClick = [this]
    {
        const auto packName = activePackName();
        if (packName.isEmpty()) return;

        auto chooser = std::make_shared<juce::FileChooser>(
            "Choose cover art",
            juce::File::getSpecialLocation(juce::File::userPicturesDirectory),
            "*.png;*.jpg;*.jpeg");
        chooser->launchAsync(juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectFiles,
            [this, packName, chooser](const juce::FileChooser& fc)
            {
                const auto src = fc.getResult();
                if (src.existsAsFile())
                    processor.getPresetManager().setPackCover(packName, src);
            });
    };

    renamePackButton.onClick = [this]
    {
        const auto packName = activePackName();
        if (packName.isEmpty()) return;

        auto* aw = new juce::AlertWindow("Rename Pack",
            "New name for " + packName + ":", juce::AlertWindow::NoIcon);
        aw->addTextEditor("name", packName, "Name:");
        aw->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
        aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        aw->enterModalState(true, juce::ModalCallbackFunction::create(
            [this, aw, packName](int r)
            {
                if (r == 1)
                {
                    processor.getPresetManager().renamePack(packName,
                        aw->getTextEditorContents("name"));
                }
                delete aw;
            }), false);
    };

    deletePackButton.onClick = [this]
    {
        const auto packName = activePackName();
        if (packName.isEmpty()) return;

        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::WarningIcon)
                .withTitle("Delete Pack")
                .withMessage("Delete pack \"" + packName
                    + "\" and all its presets? This cannot be undone.")
                .withButton("Delete")
                .withButton("Cancel"),
            [this, packName](int r)
            {
                if (r == 1)
                    processor.getPresetManager().deletePack(packName);
            });
    };

    exportPackButton.onClick = [this]
    {
        const auto packName = activePackName();
        if (packName.isEmpty()) return;

        auto chooser = std::make_shared<juce::FileChooser>(
            "Export " + packName + " as .kaipack",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                .getChildFile(packName + ".kaipack"),
            "*.kaipack");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode
                             | juce::FileBrowserComponent::canSelectFiles,
            [this, packName, chooser](const juce::FileChooser& fc)
            {
                const auto dest = fc.getResult();
                if (dest != juce::File{})
                    processor.getPresetManager().exportPack(packName, dest);
            });
    };

    importPackButton.onClick = [this]
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Import .kaipack",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.kaipack");
        chooser->launchAsync(juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc)
            {
                const auto src = fc.getResult();
                if (src.existsAsFile())
                    processor.getPresetManager().importPack(src, /*overwrite*/ false);
            });
    };
#endif
```

If `processor.getPresetManager()` doesn't already exist as an accessor, search `Source/PluginProcessor.h` for an existing accessor (likely `getPresetManager()`). If absent, add `kaigen::phantom::PresetManager& getPresetManager() { return presetManager; }` to PluginProcessor.h beside the other public accessors.

- [ ] **Step 4: Implement `activePackName()` and `authoringStripBounds()` in `Source/UI/panels/PresetBrowser.cpp`**

Append near the bottom of the file (inside `namespace kaigen::phantom`):

```cpp
#if DEVELOPER_MODE
juce::String PresetBrowser::activePackName() const
{
    if (activeCategoryIdx < 0 || activeCategoryIdx >= (int) categories.size())
        return {};
    const auto& cat = categories[(size_t) activeCategoryIdx];
    if (cat.kind == CategoryKind::Pack) return cat.packFilter;
    return {};
}

juce::Rectangle<int> PresetBrowser::authoringStripBounds() const
{
    const auto card = cardBounds();
    return { card.getX(), card.getBottom() - kAuthoringStripH,
             card.getWidth(), kAuthoringStripH };
}
#endif
```

- [ ] **Step 5: Lay out the buttons in `resized()`**

In `resized()` of `PresetBrowser.cpp`, after the existing layout that positions `closeButton` / `deleteButton`, append:

```cpp
#if DEVELOPER_MODE
    {
        auto strip = authoringStripBounds().reduced(8);
        const int gap = 6;
        const int buttons = 8;
        const int btnW = (strip.getWidth() - gap * (buttons - 1)) / buttons;

        auto place = [&](juce::TextButton& b)
        {
            b.setBounds(strip.removeFromLeft(btnW));
            strip.removeFromLeft(gap);
        };
        place(newPackButton);
        place(saveIntoPackButton);
        place(editPackMetaButton);
        place(setCoverButton);
        place(renamePackButton);
        place(deletePackButton);
        place(exportPackButton);
        place(importPackButton);
    }
#endif
```

Also shrink the list area so the strip doesn't overlap rows. Find where `contentHeightForList()` is used or where the list-row paint area is computed, and subtract `kAuthoringStripH` from its bottom edge inside an `#if DEVELOPER_MODE` block. (Grep the file for `kAuthoringStripH` after editing and verify nothing references it without the guard.)

- [ ] **Step 6: Paint a thin separator above the strip**

In `paint()` of `PresetBrowser.cpp`, just before the final closing brace, append:

```cpp
#if DEVELOPER_MODE
    {
        const auto strip = authoringStripBounds();
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.fillRect(strip.getX(), strip.getY(), strip.getWidth(), 1);
    }
#endif
```

- [ ] **Step 7: Verify DEVELOPER_MODE=ON build succeeds**

```powershell
cmake -B build-dev -DDEVELOPER_MODE=ON -A x64
cmake --build build-dev --config Release --target KaigenPhantom_VST3
```

Expected: build succeeds. Open the plugin in a host; the preset browser now has an AUTHORING strip with 8 buttons at the bottom.

- [ ] **Step 8: Verify DEVELOPER_MODE=OFF build still succeeds and lacks authoring UI**

```powershell
cmake -B build -DDEVELOPER_MODE=OFF -A x64
cmake --build build --config Release --target KaigenPhantom_VST3
```

Expected: build succeeds. Open the plugin in a host; the preset browser has the regular layout with no AUTHORING strip.

- [ ] **Step 9: Manual smoke test (designer build)**

In the DEV build, open the preset browser, switch to a User pack drill-in, and click each authoring button to verify the dialogs / file chooser appear and dispatch to the right `PresetManager` call (you can verify by watching the User-presets folder on disk).

- [ ] **Step 10: Commit**

```bash
git add Source/UI/panels/PresetBrowser.h Source/UI/panels/PresetBrowser.cpp \
        Source/PluginProcessor.h
git commit -m "feat: AUTHORING strip in PresetBrowser (DEV-only)

Eight DEVELOPER_MODE-gated buttons under the preset list: New Pack,
Save Into Pack, Edit Metadata, Set Cover, Rename, Delete Pack,
Export Pack, Import Pack. Each wires to the matching PresetManager
DEV API via AlertWindow dialogs or FileChooser. End-user builds
compile the entire strip + handlers away."
```

---

## Task 8: Manual end-to-end + ship-build sanity

**Spec:** Workflow walkthrough validation.

**Files:** none modified — this is a verification task that produces no commit unless a regression is found.

- [ ] **Step 1: Designer build full workflow walkthrough**

Build with `cmake -B build-dev -DDEVELOPER_MODE=ON -A x64 && cmake --build build-dev --config Release --target KaigenPhantom_VST3`. Open in your DAW.

1. Click **+ New Pack** → name it "WalkthroughTest", click Create. Verify it appears in the sidebar.
2. Switch the sidebar to the WalkthroughTest pack drill-in.
3. Tweak knobs, click **Save Into Pack** → "Preset A". Repeat for "Preset B".
4. Click **Set Cover** → pick any image. Verify pack tile cover updates.
5. Click **Edit Metadata** → change description. Verify the side panel reflects it.
6. Click **Rename** → "Walkthrough2". Verify the sidebar entry updates.
7. Click **Export Pack** → save `Walkthrough2.kaipack` to Desktop. Verify file exists.
8. Click **Delete Pack** → confirm. Verify the pack is gone.
9. Click **Import Pack** → pick `Walkthrough2.kaipack`. Verify the pack reappears with both presets and cover.

- [ ] **Step 2: Developer bake-in walkthrough**

1. Unzip `Walkthrough2.kaipack` into `Source/FactoryPacks/Walkthrough2/`. Verify the folder contains `pack.json`, `cover.png`, `Preset A.fxp`, `Preset B.fxp`.
2. Rebuild ship binary: `cmake -B build -DDEVELOPER_MODE=OFF -A x64 && cmake --build build --config Release --target KaigenPhantom_VST3`.
3. Open the ship build in your DAW. Verify the preset browser:
   - Shows the "Walkthrough2" pack alongside Factory / User.
   - Loads "Preset A" / "Preset B" correctly.
   - Has NO AUTHORING strip.
   - Cannot Delete / Rename / Save Into the Walkthrough2 pack (no buttons present).

- [ ] **Step 3: Cleanup**

Remove `Source/FactoryPacks/Walkthrough2/` (it was a smoke-test artifact, not a real pack). Confirm `cmake --build build --config Release --target KaigenPhantom_VST3` still succeeds with an empty FactoryPacks directory (the macro flips back to `KAIGEN_HAS_FACTORY_PACKS=0`).

- [ ] **Step 4: No commit unless regressions found**

If the walkthrough surfaces a bug, fix it and commit with a `fix:` prefix describing the specific issue. Otherwise no commit needed — the feature is shipped via Tasks 1-7.

---

## Self-review notes (post-plan)

- **Spec coverage:** Component A (8 ops + UI) → Tasks 5-7. Component B (.kaipack) → Task 4. Component C (factory bake-in) → Tasks 1-3. Workflow validation → Task 8. ✓
- **Placeholder check:** Two known sharp edges flagged inline:
  - Task 3 Step 1 + Task 6 Step 4 reference the "DummyProcessor pattern from `tests/EngineFocusTests.cpp`" — the implementer must open that file and copy the actual subclass shape rather than use the placeholder code shown. This is unavoidable: writing out the full dummy here would bloat the plan and risk drift from the canonical pattern already in the codebase.
- **Type consistency:** `PresetInfo::embeddedData` (added Task 2) is used in Task 3 (`loadPreset`) and never renamed. `PackInfo::isReadOnly` (added Task 2) checked in Tasks 5 & 6 authoring guards. `KAIGEN_HAS_FACTORY_PACKS` (defined Task 1) used in Task 2. `DEVELOPER_MODE` macro pre-existing, used identically throughout. ✓
- **No new abstractions:** No factory class for buttons, no separate "PackAuthoring" subsystem — buttons live in PresetBrowser and call PresetManager directly, matching existing structure.
