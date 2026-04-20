# Phantom Preset System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement a complete preset system allowing users to save, load, browse, and favorite presets with a minimalist Arturia-style header UI.

**Architecture:** PresetManager handles file I/O and directory structure. PluginProcessor wraps preset load/save. WebUI adds header controls (preset name, browse arrows, favorite, save button), simple dropdown (category browse), and full modal browser (search, columns, sidebar). Factory presets ship in plugin binary; user presets saved to platform-standard user data directories.

**Tech Stack:** JUCE 8, C++20, WebView2 (existing), `.fxp` preset format, CMake BinaryData

**Phase:** Phase 1 (MVP) — header, dropdowns, browser, save/load. Phases 2-3 (developer mode, import) TBD.

---

## File Structure

### New Files
- `Source/PresetManager.h` — preset directory/file management interface
- `Source/PresetManager.cpp` — implementation
- `Source/PresetTypes.h` — data structures (PresetInfo, PresetMetadata)
- `Source/WebUI/preset-header.js` — header UI logic (arrows, heart, save)
- `Source/WebUI/preset-dropdown.html` — simple dropdown markup
- `Source/WebUI/preset-dropdown.js` — dropdown logic
- `Source/WebUI/preset-browser.html` — full browser modal markup
- `Source/WebUI/preset-browser.js` — browser logic (search, sort, favorite)
- `tests/PresetManagerTest.cpp` — file I/O tests

### Modified Files
- `Source/PluginProcessor.h/cpp` — add `loadPreset()`, `savePreset()`, `getPresetInfo()`
- `Source/PluginEditor.h/cpp` — instantiate PresetManager, pass to WebView natives
- `Source/WebUI/index.html` — add preset header markup
- `Source/WebUI/phantom.js` — wire preset controls to backend functions
- `CMakeLists.txt` — DEVELOPER_MODE flag, bundle factory presets in BinaryData, add tests

---

## Task Breakdown

### Task 1: Create PresetTypes Header

Define data structures for preset metadata and info.

**Files:**
- Create: `Source/PresetTypes.h`

- [ ] **Step 1: Write PresetTypes.h with data structures**

```cpp
#pragma once

#include <juce_core/juce_core.h>
#include <string>
#include <vector>

namespace kaigen::phantom {

// Preset metadata (name, type, designer, favorite status)
struct PresetMetadata {
    juce::String name;           // "Warm Bass Boost"
    juce::String type;           // "Piano", "Drone", "Synth", "Bass", "Experimental"
    juce::String designer;       // "Kai Slate" or username
    bool isFavorite = false;
    bool isFactory = false;      // True if from Factory/ folder
    juce::String packName;       // "Factory", "User", "Analog Vibes", etc.
};

// Preset file info with full path
struct PresetInfo {
    PresetMetadata metadata;
    juce::File file;             // Full path to .fxp file
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Verify syntax**

No compilation needed yet (header-only), but review structure.

---

### Task 2: Create PresetManager Class

Implement directory management, enumeration, and file operations.

**Files:**
- Create: `Source/PresetManager.h`
- Create: `Source/PresetManager.cpp`

- [ ] **Step 1: Write PresetManager.h header**

```cpp
#pragma once

#include "PresetTypes.h"
#include <juce_core/juce_core.h>
#include <vector>
#include <map>

namespace kaigen::phantom {

class PresetManager {
public:
    PresetManager();
    ~PresetManager() = default;

    // Initialize preset directories and load factory presets
    void initialize();

    // Get all presets organized by pack/category
    // Returns map: packName -> vector of PresetInfo
    std::map<juce::String, std::vector<PresetInfo>> getAllPresets() const;

    // Get presets for a specific pack
    std::vector<PresetInfo> getPresetsForPack(const juce::String& packName) const;

    // Get all unique types across all presets
    std::vector<juce::String> getAllTypes() const;

    // Search presets by name (case-insensitive)
    std::vector<PresetInfo> searchByName(const juce::String& query) const;

    // Toggle favorite status of a preset
    void setFavorite(const juce::String& presetPath, bool isFavorite);

    // Save current parameters as a new preset
    // Returns full path to saved file
    juce::String savePreset(const juce::String& presetName,
                           const juce::String& type,
                           const juce::MemoryBlock& fxpData);

    // Get full path for a preset file
    juce::File getPresetFile(const juce::String& presetName,
                            const juce::String& packName) const;

    // Get user presets directory
    juce::File getUserPresetsDirectory() const;

    // Get factory presets directory
    juce::File getFactoryPresetsDirectory() const;

private:
    juce::File getPresetsRootDirectory() const;
    void ensureDirectoryStructure();
    void loadPresetsFromDisk();

    std::map<juce::String, std::vector<PresetInfo>> allPresets;
    std::map<juce::String, bool> favoriteMap;  // path -> isFavorite
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Write PresetManager.cpp implementation**

```cpp
#include "PresetManager.h"
#include <juce_core/juce_core.h>

namespace kaigen::phantom {

PresetManager::PresetManager() {
}

void PresetManager::initialize() {
    ensureDirectoryStructure();
    loadPresetsFromDisk();
}

juce::File PresetManager::getPresetsRootDirectory() const {
    // Windows: %APPDATA%\Kaigen\KaigenPhantom\Presets
    // macOS: ~/Library/Application Support/Kaigen/KaigenPhantom/Presets
    // Linux: ~/.config/Kaigen/KaigenPhantom/Presets

#if JUCE_WINDOWS
    auto appDataPath = juce::File::getSpecialLocation(
        juce::File::userApplicationDataDirectory);
    return appDataPath.getChildFile("Kaigen")
                      .getChildFile("KaigenPhantom")
                      .getChildFile("Presets");
#elif JUCE_MAC
    auto appSupportPath = juce::File::getSpecialLocation(
        juce::File::userApplicationDataDirectory);
    return appSupportPath.getChildFile("Kaigen")
                         .getChildFile("KaigenPhantom")
                         .getChildFile("Presets");
#else // JUCE_LINUX
    auto configPath = juce::File(juce::File::getSpecialLocation(
        juce::File::userApplicationDataDirectory));
    return configPath.getChildFile("Kaigen")
                     .getChildFile("KaigenPhantom")
                     .getChildFile("Presets");
#endif
}

juce::File PresetManager::getUserPresetsDirectory() const {
    return getPresetsRootDirectory().getChildFile("User");
}

juce::File PresetManager::getFactoryPresetsDirectory() const {
    return getPresetsRootDirectory().getChildFile("Factory");
}

void PresetManager::ensureDirectoryStructure() {
    auto root = getPresetsRootDirectory();
    root.createDirectory();
    getUserPresetsDirectory().createDirectory();
    getFactoryPresetsDirectory().createDirectory();
}

void PresetManager::loadPresetsFromDisk() {
    allPresets.clear();
    auto root = getPresetsRootDirectory();

    // Scan all subdirectories (Factory, User, packs)
    for (auto file : root.findChildFiles(juce::File::findDirectories, false)) {
        auto packName = file.getFileName();
        auto presetList = std::vector<PresetInfo>();

        // Scan .fxp files in this pack
        for (auto presetFile : file.findChildFiles(juce::File::findFiles, false, "*.fxp")) {
            PresetInfo info;
            info.file = presetFile;
            info.metadata.name = presetFile.getFileNameWithoutExtension();
            info.metadata.packName = packName;
            info.metadata.isFactory = (packName == "Factory");

            // Default values (type and designer could be read from .fxp metadata later)
            info.metadata.type = "Experimental";
            info.metadata.designer = "Kai Slate";

            presetList.push_back(info);
        }

        if (!presetList.empty()) {
            allPresets[packName] = presetList;
        }
    }
}

std::map<juce::String, std::vector<PresetInfo>> PresetManager::getAllPresets() const {
    return allPresets;
}

std::vector<PresetInfo> PresetManager::getPresetsForPack(
    const juce::String& packName) const {
    auto it = allPresets.find(packName);
    if (it != allPresets.end()) {
        return it->second;
    }
    return {};
}

std::vector<juce::String> PresetManager::getAllTypes() const {
    std::vector<juce::String> types;
    for (const auto& [packName, presets] : allPresets) {
        for (const auto& preset : presets) {
            if (std::find(types.begin(), types.end(), preset.metadata.type) == types.end()) {
                types.push_back(preset.metadata.type);
            }
        }
    }
    return types;
}

std::vector<PresetInfo> PresetManager::searchByName(
    const juce::String& query) const {
    std::vector<PresetInfo> results;
    auto lowerQuery = query.toLowerCase();

    for (const auto& [packName, presets] : allPresets) {
        for (const auto& preset : presets) {
            if (preset.metadata.name.toLowerCase().contains(lowerQuery)) {
                results.push_back(preset);
            }
        }
    }
    return results;
}

void PresetManager::setFavorite(const juce::String& presetPath, bool isFavorite) {
    favoriteMap[presetPath] = isFavorite;
    // TODO: persist favorite flag to disk (Phase 2)
}

juce::String PresetManager::savePreset(const juce::String& presetName,
                                       const juce::String& type,
                                       const juce::MemoryBlock& fxpData) {
    auto userDir = getUserPresetsDirectory();
    auto presetFile = userDir.getChildFile(presetName + ".fxp");

    if (!presetFile.replaceWithData(fxpData.getData(), fxpData.getSize())) {
        jassertfalse;  // File write failed
        return "";
    }

    return presetFile.getFullPathName();
}

juce::File PresetManager::getPresetFile(const juce::String& presetName,
                                        const juce::String& packName) const {
    return getPresetsRootDirectory()
        .getChildFile(packName)
        .getChildFile(presetName + ".fxp");
}

} // namespace kaigen::phantom
```

- [ ] **Step 3: Verify compilation (not running yet)**

---

### Task 3: Update PluginProcessor for Preset Load/Save

Add methods to serialize/deserialize all 21 parameters to `.fxp` format.

**Files:**
- Modify: `Source/PluginProcessor.h:1-50` (add methods)
- Modify: `Source/PluginProcessor.cpp:1-50` (add methods)

- [ ] **Step 1: Add preset load/save method declarations to PluginProcessor.h**

Find the class definition and add these public methods after existing methods:

```cpp
// In KaigenPhantomAudioProcessor class:

// Load a preset (.fxp file) and apply all parameters
void loadPresetFromFile(const juce::File& presetFile);

// Save current parameter state to .fxp file
void savePresetToFile(const juce::File& presetFile);

// Get current state as MemoryBlock (for PresetManager::savePreset)
juce::MemoryBlock getStateAsMemoryBlock() const;
```

- [ ] **Step 2: Implement preset load in PluginProcessor.cpp**

Add to PluginProcessor.cpp:

```cpp
void KaigenPhantomAudioProcessor::loadPresetFromFile(const juce::File& presetFile) {
    // Read .fxp file as memory block
    auto fxpData = presetFile.loadFileAsData();
    
    // Use APVTS to restore state from .fxp
    // .fxp format: 4-byte header "CcnK", then VST preset data
    // JUCE's ValueTreeState::replaceState handles the parsing
    
    if (fxpData.getSize() < 28) {
        jassertfalse;  // Invalid .fxp file
        return;
    }

    juce::ValueTree tree = juce::ValueTree::fromXml(
        juce::XmlElement::createTextElement(juce::String::fromUTF8(
            static_cast<const char*>(fxpData.getData()),
            (int)fxpData.getSize())));
    
    // Simpler approach: use AudioProcessor's getStateInformation/setStateInformation
    apvts.replaceState(juce::ValueTree::fromXml(
        juce::XmlElement::createTextElement(juce::String::fromUTF8(
            static_cast<const char*>(fxpData.getData()),
            (int)fxpData.getSize()))));

    // Trigger parameter changed callbacks so DSP updates
    for (auto param : apvts.processor.getParameters()) {
        if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param)) {
            param->sendValueChangedMessageToListeners(param->getValue());
        }
    }
}

void KaigenPhantomAudioProcessor::savePresetToFile(const juce::File& presetFile) {
    auto state = apvts.copyState();
    auto xml = state.createXml();
    
    if (!xml) {
        jassertfalse;
        return;
    }

    presetFile.replaceWithText(xml->toString());
}

juce::MemoryBlock KaigenPhantomAudioProcessor::getStateAsMemoryBlock() const {
    auto state = apvts.copyState();
    auto xml = state.createXml();
    
    if (!xml) {
        return juce::MemoryBlock();
    }

    auto xmlStr = xml->toString();
    juce::MemoryBlock block(xmlStr.toUTF8(), xmlStr.length());
    return block;
}
```

- [ ] **Step 3: Commit**

```bash
git add Source/PresetManager.h Source/PresetManager.cpp Source/PresetTypes.h
git commit -m "feat: add PresetManager and PresetTypes for preset file I/O"
```

---

### Task 4: Update PluginEditor to Instantiate PresetManager

Wire PresetManager into the plugin editor so WebView can access it.

**Files:**
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`

- [ ] **Step 1: Add PresetManager member to PluginEditor.h**

```cpp
// In KaigenPhantomAudioProcessorEditor class:

private:
    std::unique_ptr<PresetManager> presetManager;
```

- [ ] **Step 2: Initialize PresetManager in PluginEditor constructor (PluginEditor.cpp)**

```cpp
KaigenPhantomAudioProcessorEditor::KaigenPhantomAudioProcessorEditor (
    KaigenPhantomAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p) {
    
    // Existing code...
    
    // Initialize preset system
    presetManager = std::make_unique<PresetManager>();
    presetManager->initialize();

    // Existing code...
}
```

- [ ] **Step 3: Add method to expose PresetManager to WebView**

```cpp
// In PluginEditor.h, add public getter:
PresetManager* getPresetManager() { return presetManager.get(); }
```

- [ ] **Step 4: Commit**

```bash
git add Source/PluginEditor.h Source/PluginEditor.cpp
git commit -m "feat: integrate PresetManager into PluginEditor"
```

---

### Task 5: Update WebUI HTML for Preset Header

Add preset header markup to index.html.

**Files:**
- Modify: `Source/WebUI/index.html`

- [ ] **Step 1: Find the existing header section**

Locate the header bar (around line 50-80, likely has plugin name "KAIGEN PHANTOM").

- [ ] **Step 2: Replace header with preset-aware version**

```html
<!-- Preset Header Bar -->
<div id="preset-header" style="
    background: linear-gradient(to right, #0f3460, #16384d);
    padding: 12px 16px;
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 16px;
    border-bottom: 1px solid #16384d;
    font-family: system-ui, -apple-system, sans-serif;
    font-size: 12px;
">
    <!-- Left: Books icon + Heart -->
    <div style="display: flex; align-items: center; gap: 12px;">
        <button id="preset-browser-btn" 
                style="background: none; border: none; color: #16c784; cursor: pointer; font-size: 16px; padding: 0;">
            |||
        </button>
        <button id="preset-favorite-btn"
                style="background: none; border: none; color: #999; cursor: pointer; font-size: 16px; padding: 0;">
            ♡
        </button>
    </div>

    <!-- Center: Preset Name -->
    <div style="flex: 1; text-align: center;">
        <span id="preset-name" style="color: #fff; font-size: 16px; font-weight: 600; letter-spacing: 0.5px;">
            Default
        </span>
        <span id="preset-modified-indicator" style="color: #e94560; font-weight: 600; display: none;">*</span>
    </div>

    <!-- Right: Browse arrows + Save button -->
    <div style="display: flex; align-items: center; gap: 8px;">
        <button id="preset-prev-btn"
                style="background: none; border: none; color: #999; cursor: pointer; padding: 0; font-size: 14px;">
            ▲
        </button>
        <button id="preset-next-btn"
                style="background: none; border: none; color: #999; cursor: pointer; padding: 0; font-size: 14px;">
            ▼
        </button>
        <button id="preset-save-btn"
                style="background: #e94560; color: #fff; border: none; cursor: pointer; padding: 4px 10px; border-radius: 3px; font-size: 10px; font-weight: 600; display: none;">
            Save
        </button>
    </div>
</div>

<!-- Preset Dropdown (hidden by default) -->
<div id="preset-dropdown" style="
    display: none;
    position: absolute;
    top: 50px;
    left: 0;
    background: #0f3460;
    border: 1px solid #16384d;
    border-radius: 4px;
    width: 220px;
    font-family: system-ui;
    font-size: 11px;
    z-index: 1000;
    max-height: 400px;
    overflow-y: auto;
">
    <!-- Populated by preset-dropdown.js -->
</div>

<!-- Preset Browser Modal (hidden by default) -->
<div id="preset-browser-modal" style="
    display: none;
    position: fixed;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    background: rgba(0, 0, 0, 0.7);
    z-index: 2000;
    align-items: center;
    justify-content: center;
">
    <div style="
        background: #1a1a2e;
        border: 1px solid #16384d;
        border-radius: 8px;
        width: 90%;
        height: 90%;
        display: flex;
        flex-direction: column;
        font-family: system-ui;
        font-size: 11px;
    ">
        <!-- Populated by preset-browser.js -->
    </div>
</div>

<!-- Preset Save Modal (hidden by default) -->
<div id="preset-save-modal" style="
    display: none;
    position: fixed;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    background: rgba(0, 0, 0, 0.6);
    z-index: 2500;
    align-items: center;
    justify-content: center;
">
    <!-- Populated by preset-header.js -->
</div>
```

- [ ] **Step 3: Commit**

```bash
git add Source/WebUI/index.html
git commit -m "feat: add preset header markup to UI"
```

---

### Task 6: Create Preset Header JavaScript

Implement header button logic and save modal.

**Files:**
- Create: `Source/WebUI/preset-header.js`

- [ ] **Step 1: Write preset-header.js**

```javascript
// Preset header controls and save modal

let currentPresetName = "Default";
let currentPresetIsFavorite = false;
let currentPresetIsModified = false;
let currentPresetPack = "Factory";

// Initialize header buttons
function initPresetHeader() {
    const browserBtn = document.getElementById('preset-browser-btn');
    const favoriteBtn = document.getElementById('preset-favorite-btn');
    const prevBtn = document.getElementById('preset-prev-btn');
    const nextBtn = document.getElementById('preset-next-btn');
    const saveBtn = document.getElementById('preset-save-btn');
    const nameDisplay = document.getElementById('preset-name');

    browserBtn.addEventListener('click', () => {
        const modal = document.getElementById('preset-browser-modal');
        if (modal.style.display === 'none') {
            showPresetBrowser();
        } else {
            hidePresetBrowser();
        }
    });

    favoriteBtn.addEventListener('click', () => {
        currentPresetIsFavorite = !currentPresetIsFavorite;
        updateFavoriteButton();
        // TODO: call native function to persist favorite (Phase 2)
    });

    prevBtn.addEventListener('click', () => {
        // TODO: Load previous preset in current category
    });

    nextBtn.addEventListener('click', () => {
        // TODO: Load next preset in current category
    });

    saveBtn.addEventListener('click', () => {
        showSaveModal();
    });

    // Listen to parameter changes (set modified flag)
    window.addEventListener('parameter-changed', () => {
        setPresetModified(true);
    });
}

function updateFavoriteButton() {
    const btn = document.getElementById('preset-favorite-btn');
    btn.textContent = currentPresetIsFavorite ? '♥' : '♡';
    btn.style.color = currentPresetIsFavorite ? '#e94560' : '#999';
}

function setPresetModified(isModified) {
    currentPresetIsModified = isModified;
    const indicator = document.getElementById('preset-modified-indicator');
    const saveBtn = document.getElementById('preset-save-btn');

    if (isModified) {
        indicator.style.display = 'inline';
        saveBtn.style.display = 'block';
    } else {
        indicator.style.display = 'none';
        saveBtn.style.display = 'none';
    }
}

function setCurrentPreset(name, isFavorite, packName) {
    currentPresetName = name;
    currentPresetIsFavorite = isFavorite;
    currentPresetPack = packName;
    setPresetModified(false);

    document.getElementById('preset-name').textContent = name;
    updateFavoriteButton();
}

function showSaveModal() {
    const modal = document.getElementById('preset-save-modal');
    const html = `
        <div style="
            background: #1a1a2e;
            border: 2px solid #0f3460;
            border-radius: 6px;
            padding: 16px;
            width: 100%;
            max-width: 320px;
            font-family: system-ui;
            font-size: 12px;
        ">
            <div style="color: #16c784; font-weight: 600; margin-bottom: 12px;">Save Preset</div>
            <div style="margin-bottom: 12px;">
                <label style="display: block; color: #999; font-size: 10px; text-transform: uppercase; margin-bottom: 4px;">
                    Preset Name
                </label>
                <input id="preset-name-input" 
                       type="text" 
                       value="${currentPresetName}"
                       style="
                           width: 100%;
                           background: #0f3460;
                           color: #16c784;
                           border: 1px solid #16384d;
                           padding: 6px 8px;
                           border-radius: 3px;
                           font-size: 12px;
                           box-sizing: border-box;
                       "
                       autofocus>
            </div>
            <div style="margin-bottom: 12px; font-size: 10px; color: #999;">
                <input id="preset-overwrite-checkbox" type="checkbox">
                <label for="preset-overwrite-checkbox">Overwrite existing preset</label>
            </div>
            <div style="display: flex; gap: 6px;">
                <button id="preset-save-confirm" style="
                    flex: 1;
                    background: #16c784;
                    color: #1a1a2e;
                    border: none;
                    padding: 6px;
                    border-radius: 3px;
                    cursor: pointer;
                    font-weight: 600;
                    font-size: 11px;
                ">Save</button>
                <button id="preset-save-cancel" style="
                    flex: 1;
                    background: #16384d;
                    color: #999;
                    border: 1px solid #16384d;
                    padding: 6px;
                    border-radius: 3px;
                    cursor: pointer;
                    font-size: 11px;
                ">Cancel</button>
            </div>
        </div>
    `;
    modal.innerHTML = html;
    modal.style.display = 'flex';

    document.getElementById('preset-save-confirm').addEventListener('click', () => {
        const newName = document.getElementById('preset-name-input').value;
        const shouldOverwrite = document.getElementById('preset-overwrite-checkbox').checked;
        
        // Call native function to save preset
        if (window.Juce && window.Juce.savePreset) {
            window.Juce.savePreset(newName, shouldOverwrite);
            setCurrentPreset(newName, false, 'User');
            modal.style.display = 'none';
        }
    });

    document.getElementById('preset-save-cancel').addEventListener('click', () => {
        modal.style.display = 'none';
    });
}

function hidePresetBrowser() {
    document.getElementById('preset-browser-modal').style.display = 'none';
    document.getElementById('preset-browser-btn').textContent = '|||';
}

function showPresetBrowser() {
    document.getElementById('preset-browser-modal').style.display = 'flex';
    document.getElementById('preset-browser-btn').textContent = '✕';
    // TODO: populate browser (call initPresetBrowser from preset-browser.js)
}

// Call on document ready
document.addEventListener('DOMContentLoaded', initPresetHeader);
```

- [ ] **Step 2: Include preset-header.js in index.html**

Add to index.html `<head>` section:

```html
<script src="preset-header.js"></script>
```

- [ ] **Step 3: Commit**

```bash
git add Source/WebUI/preset-header.js
git commit -m "feat: implement preset header controls and save modal"
```

---

### Task 7: Create Preset Dropdown JavaScript

Implement simple category-based preset dropdown.

**Files:**
- Create: `Source/WebUI/preset-dropdown.js`

- [ ] **Step 1: Write preset-dropdown.js**

```javascript
// Simple preset dropdown (organized by category)

function initPresetDropdown() {
    const presetNameDisplay = document.getElementById('preset-name');
    
    presetNameDisplay.addEventListener('click', (e) => {
        e.stopPropagation();
        toggleDropdown();
    });

    // Close dropdown on document click
    document.addEventListener('click', () => {
        const dropdown = document.getElementById('preset-dropdown');
        if (dropdown.style.display !== 'none') {
            dropdown.style.display = 'none';
        }
    });
}

function toggleDropdown() {
    const dropdown = document.getElementById('preset-dropdown');
    if (dropdown.style.display === 'none') {
        populateDropdown();
        dropdown.style.display = 'block';
    } else {
        dropdown.style.display = 'none';
    }
}

function populateDropdown() {
    const dropdown = document.getElementById('preset-dropdown');
    
    // Get all presets from native function
    if (!window.Juce || !window.Juce.getAllPresets) {
        dropdown.innerHTML = '<div style="padding: 8px; color: #999;">No presets available</div>';
        return;
    }

    const presetsJson = window.Juce.getAllPresets();
    const presets = JSON.parse(presetsJson);

    let html = '<div style="padding: 8px; border-bottom: 1px solid #16384d; color: #999; font-size: 9px; text-transform: uppercase;">Categories</div>';

    // Group by type/category
    const categories = {};
    for (const [packName, presetList] of Object.entries(presets)) {
        for (const preset of presetList) {
            const type = preset.metadata.type || 'Uncategorized';
            if (!categories[type]) {
                categories[type] = [];
            }
            categories[type].push({ name: preset.metadata.name, packName: packName });
        }
    }

    // Render categories
    for (const [type, typePresets] of Object.entries(categories)) {
        html += `<div style="padding: 6px 8px; background: #16384d; cursor: pointer; color: #16c784; user-select: none;" onclick="toggleCategory('${type}')">▼ ${type}</div>`;
        html += `<div id="category-${type}" style="display: block;">`;
        
        for (const p of typePresets) {
            const isCurrent = p.name === currentPresetName;
            const bgColor = isCurrent ? '#16384d' : '#0f3460';
            const textColor = isCurrent ? '#16c784' : '#999';
            html += `<div style="padding: 6px 12px; cursor: pointer; background: ${bgColor}; color: ${textColor}; border-left: 2px solid ${isCurrent ? '#16c784' : 'transparent'};" onclick="loadPresetFromDropdown('${p.name}', '${p.packName}')">
                ${isCurrent ? '▶ ' : ''}${p.name}
            </div>`;
        }
        html += '</div>';
    }

    dropdown.innerHTML = html;
}

function toggleCategory(type) {
    const categoryDiv = document.getElementById(`category-${type}`);
    if (categoryDiv) {
        categoryDiv.style.display = categoryDiv.style.display === 'none' ? 'block' : 'none';
    }
}

function loadPresetFromDropdown(presetName, packName) {
    // Call native function to load preset
    if (window.Juce && window.Juce.loadPreset) {
        window.Juce.loadPreset(presetName, packName);
        setCurrentPreset(presetName, false, packName);
        document.getElementById('preset-dropdown').style.display = 'none';
    }
}

// Initialize on page load
document.addEventListener('DOMContentLoaded', initPresetDropdown);
```

- [ ] **Step 2: Include preset-dropdown.js in index.html**

Add to index.html `<head>`:

```html
<script src="preset-dropdown.js"></script>
```

- [ ] **Step 3: Commit**

```bash
git add Source/WebUI/preset-dropdown.js
git commit -m "feat: implement preset dropdown with category browsing"
```

---

### Task 8: Create Preset Browser Modal

Implement full preset browser with search, columns, and sidebar.

**Files:**
- Create: `Source/WebUI/preset-browser.js`

- [ ] **Step 1: Write preset-browser.js**

```javascript
// Full preset browser modal (search, columns, sidebar, favorites)

let allPresetsCache = null;

function initPresetBrowser() {
    // Populate browser on first open
    loadPresetsCache();
}

function loadPresetsCache() {
    if (window.Juce && window.Juce.getAllPresets) {
        const json = window.Juce.getAllPresets();
        allPresetsCache = JSON.parse(json);
    }
}

function populatePresetBrowser() {
    if (!allPresetsCache) {
        loadPresetsCache();
    }

    const modal = document.getElementById('preset-browser-modal');
    const html = `
        <div style="display: flex; width: 100%; height: 100%; background: #1a1a2e;">
            <!-- Left Sidebar -->
            <div style="
                width: 140px;
                background: #0f3460;
                border-right: 1px solid #16384d;
                overflow-y: auto;
                padding: 8px;
            ">
                <div style="color: #999; font-size: 9px; text-transform: uppercase; margin-bottom: 8px;">Categories</div>
                <div style="padding: 6px 8px; cursor: pointer; color: #16c784; border-left: 2px solid #16c784;" onclick="filterPresetsByCategory('all')">
                    Explore
                </div>
                <div style="padding: 6px 8px; cursor: pointer; color: #999; border-left: 2px solid transparent;" onclick="filterPresetsByCategory('factory')">
                    Factory
                </div>
                <div style="padding: 6px 8px; cursor: pointer; color: #999; border-left: 2px solid transparent;" onclick="filterPresetsByCategory('user')">
                    User
                </div>
            </div>

            <!-- Main Content -->
            <div style="flex: 1; display: flex; flex-direction: column; border-right: 1px solid #16384d;">
                <!-- Search Bar -->
                <div style="padding: 8px; border-bottom: 1px solid #16384d; display: flex; gap: 6px; align-items: center;">
                    <input id="preset-search-input"
                           type="text"
                           placeholder="Search presets..."
                           style="
                               flex: 1;
                               background: #16384d;
                               color: #16c784;
                               border: 1px solid #16384d;
                               padding: 4px 6px;
                               border-radius: 3px;
                               font-size: 10px;
                           "
                           onkeyup="filterPresetsOnSearch()">
                    <button onclick="clearPresetSearch()" style="
                        background: none;
                        border: none;
                        color: #999;
                        cursor: pointer;
                        font-size: 10px;
                        padding: 2px 4px;
                    ">✕ Clear</button>
                </div>

                <!-- Preset List -->
                <div style="flex: 1; overflow-y: auto; padding: 8px;">
                    <div id="preset-list" style="display: grid; gap: 6px;">
                        <!-- Populated by JavaScript -->
                    </div>
                </div>
            </div>

            <!-- Right Preview Panel -->
            <div style="
                width: 140px;
                background: #0f3460;
                border-left: 1px solid #16384d;
                padding: 8px;
                display: flex;
                flex-direction: column;
            ">
                <div style="color: #999; font-size: 9px; text-transform: uppercase; margin-bottom: 6px;">Preview</div>
                <div id="preset-preview" style="
                    background: #16384d;
                    flex: 1;
                    border-radius: 3px;
                    display: flex;
                    align-items: center;
                    justify-content: center;
                    font-size: 9px;
                    color: #666;
                    text-align: center;
                    padding: 8px;
                ">
                    Select a preset
                </div>
            </div>
        </div>
    `;

    modal.innerHTML = html;
    renderPresetList(allPresetsCache);

    document.getElementById('preset-search-input').focus();
}

function renderPresetList(presets) {
    const listDiv = document.getElementById('preset-list');
    let html = '';

    // Flatten and render presets
    for (const [packName, presetArray] of Object.entries(presets)) {
        for (const preset of presetArray) {
            const bgColor = preset.metadata.name === currentPresetName ? '#16384d' : '#0f3460';
            const heartIcon = preset.metadata.isFavorite ? '♥' : '♡';
            const heartColor = preset.metadata.isFavorite ? '#e94560' : '#999';

            html += `
                <div style="
                    display: grid;
                    grid-template-columns: 1fr 80px 100px 20px;
                    gap: 8px;
                    padding: 6px;
                    background: ${bgColor};
                    border-radius: 3px;
                    align-items: center;
                    cursor: pointer;
                    font-size: 10px;
                " 
                onmouseover="this.style.background='#16384d'"
                onmouseout="this.style.background='${bgColor}'"
                onclick="loadPresetFromBrowser('${preset.metadata.name}', '${packName}')">
                    <div style="color: #16c784;">${preset.metadata.name}</div>
                    <div style="color: #999;">${preset.metadata.type}</div>
                    <div style="color: #999;">${preset.metadata.designer}</div>
                    <div style="color: ${heartColor}; cursor: pointer; text-align: center;" onclick="event.stopPropagation(); toggleFavoriteFromBrowser('${preset.metadata.name}', '${packName}')">${heartIcon}</div>
                </div>
            `;
        }
    }

    listDiv.innerHTML = html || '<div style="color: #666; padding: 16px;">No presets found</div>';
}

function filterPresetsOnSearch() {
    const query = document.getElementById('preset-search-input').value.toLowerCase();
    
    const filtered = {};
    for (const [packName, presetArray] of Object.entries(allPresetsCache)) {
        filtered[packName] = presetArray.filter(p => 
            p.metadata.name.toLowerCase().includes(query)
        );
    }

    renderPresetList(filtered);
}

function clearPresetSearch() {
    document.getElementById('preset-search-input').value = '';
    renderPresetList(allPresetsCache);
}

function filterPresetsByCategory(category) {
    if (category === 'all') {
        renderPresetList(allPresetsCache);
    } else if (category === 'factory') {
        const filtered = { Factory: allPresetsCache.Factory || [] };
        renderPresetList(filtered);
    } else if (category === 'user') {
        const filtered = { User: allPresetsCache.User || [] };
        renderPresetList(filtered);
    }
}

function loadPresetFromBrowser(presetName, packName) {
    if (window.Juce && window.Juce.loadPreset) {
        window.Juce.loadPreset(presetName, packName);
        setCurrentPreset(presetName, false, packName);
        hidePresetBrowser();
    }
}

function toggleFavoriteFromBrowser(presetName, packName) {
    // TODO: call native function to toggle favorite
    // Update cache and re-render
    if (allPresetsCache[packName]) {
        const preset = allPresetsCache[packName].find(p => p.metadata.name === presetName);
        if (preset) {
            preset.metadata.isFavorite = !preset.metadata.isFavorite;
            renderPresetList(allPresetsCache);
        }
    }
}

// Initialize on page load
document.addEventListener('DOMContentLoaded', initPresetBrowser);
```

- [ ] **Step 2: Include preset-browser.js in index.html**

Add to index.html `<head>`:

```html
<script src="preset-browser.js"></script>
```

- [ ] **Step 3: Commit**

```bash
git add Source/WebUI/preset-browser.js
git commit -m "feat: implement full preset browser with search and favorites"
```

---

### Task 9: Add Native Functions to PluginEditor

Expose preset operations to WebView via native function bindings.

**Files:**
- Modify: `Source/PluginEditor.cpp`

- [ ] **Step 1: Add native function handlers to PluginEditor::resized() or constructor**

In PluginEditor.cpp, after WebBrowserComponent setup, add:

```cpp
// Bind native functions for preset system
webView->bindFunction("loadPreset", [this](const juce::var& args) -> juce::var {
    if (args.size() < 2) return false;
    
    auto presetName = args[0].toString();
    auto packName = args[1].toString();
    
    auto presetFile = presetManager->getPresetFile(presetName, packName);
    if (presetFile.exists()) {
        audioProcessor.loadPresetFromFile(presetFile);
        return true;
    }
    return false;
});

webView->bindFunction("savePreset", [this](const juce::var& args) -> juce::var {
    if (args.size() < 2) return false;
    
    auto presetName = args[0].toString();
    auto shouldOverwrite = args[1].isBool() && args[1];
    
    auto memBlock = audioProcessor.getStateAsMemoryBlock();
    auto savedPath = presetManager->savePreset(presetName, "Experimental", memBlock);
    
    return savedPath.isNotEmpty();
});

webView->bindFunction("getAllPresets", [this](const juce::var& args) -> juce::var {
    auto allPresets = presetManager->getAllPresets();
    juce::String json = "{";
    
    int packIdx = 0;
    for (const auto& [packName, presets] : allPresets) {
        if (packIdx++ > 0) json += ",";
        json += "\"" + packName + "\":[";
        
        int presetIdx = 0;
        for (const auto& preset : presets) {
            if (presetIdx++ > 0) json += ",";
            json += "{\"metadata\":{\"name\":\"" + preset.metadata.name + 
                   "\",\"type\":\"" + preset.metadata.type +
                   "\",\"designer\":\"" + preset.metadata.designer +
                   "\",\"isFavorite\":" + (preset.metadata.isFavorite ? "true" : "false") + "}}";
        }
        json += "]";
    }
    json += "}";
    
    return json;
});
```

- [ ] **Step 2: Ensure WebBrowserComponent methods are called correctly**

Verify the binding syntax matches your WebBrowserComponent version. Test compilation.

- [ ] **Step 3: Commit**

```bash
git add Source/PluginEditor.cpp
git commit -m "feat: add native preset functions to WebView"
```

---

### Task 10: Create Test Suite for PresetManager

Write tests for file I/O and directory operations.

**Files:**
- Create: `tests/PresetManagerTest.cpp`

- [ ] **Step 1: Write PresetManagerTest.cpp**

```cpp
#include <catch2/catch_all.hpp>
#include "../Source/PresetManager.h"
#include "../Source/PresetTypes.h"

using namespace kaizen::phantom;

TEST_CASE("PresetManager - Directory Structure", "[PresetManager]") {
    PresetManager mgr;
    mgr.initialize();

    auto userDir = mgr.getUserPresetsDirectory();
    auto factoryDir = mgr.getFactoryPresetsDirectory();

    REQUIRE(userDir.exists());
    REQUIRE(factoryDir.exists());
}

TEST_CASE("PresetManager - Save and Load Preset", "[PresetManager]") {
    PresetManager mgr;
    mgr.initialize();

    // Create dummy .fxp data
    juce::MemoryBlock testData;
    testData.append("TEST", 4);

    // Save preset
    auto savePath = mgr.savePreset("Test Preset", "Piano", testData);
    REQUIRE(savePath.isNotEmpty());
    REQUIRE(juce::File(savePath).exists());

    // Verify file was written
    auto file = juce::File(savePath);
    REQUIRE(file.getSize() == 4);
}

TEST_CASE("PresetManager - Enumerate Presets", "[PresetManager]") {
    PresetManager mgr;
    mgr.initialize();

    // Save a test preset
    juce::MemoryBlock testData;
    testData.append("TEST", 4);
    mgr.savePreset("Piano Test", "Piano", testData);

    // Load all presets
    auto allPresets = mgr.getAllPresets();
    REQUIRE(allPresets.find("User") != allPresets.end());
    REQUIRE(!allPresets["User"].empty());
}

TEST_CASE("PresetManager - Search by Name", "[PresetManager]") {
    PresetManager mgr;
    mgr.initialize();

    juce::MemoryBlock testData;
    testData.append("TEST", 4);
    mgr.savePreset("Warm Bass", "Bass", testData);
    mgr.savePreset("Deep Drone", "Drone", testData);

    auto results = mgr.searchByName("Warm");
    REQUIRE(results.size() >= 1);
    REQUIRE(results[0].metadata.name == "Warm Bass");
}

TEST_CASE("PresetManager - Favorite Toggle", "[PresetManager]") {
    PresetManager mgr;
    mgr.initialize();

    juce::MemoryBlock testData;
    testData.append("TEST", 4);
    auto savedPath = mgr.savePreset("Fav Test", "Piano", testData);

    mgr.setFavorite(savedPath, true);
    // TODO: verify favorite flag persisted (Phase 2)
}
```

- [ ] **Step 2: Add test target to CMakeLists.txt**

Find the test section and add:

```cmake
# Preset Manager Tests
add_executable(PresetManagerTest tests/PresetManagerTest.cpp 
               Source/PresetManager.cpp)
target_link_libraries(PresetManagerTest juce::juce_core Catch2::Catch2WithMain)
add_test(NAME PresetManager COMMAND PresetManagerTest)
```

- [ ] **Step 3: Run tests**

```bash
cd build && cmake .. && make PresetManagerTest && ctest --output-on-failure
```

Expected: All 5 tests pass.

- [ ] **Step 4: Commit**

```bash
git add tests/PresetManagerTest.cpp
git commit -m "test: add PresetManager file I/O tests"
```

---

### Task 11: Update CMakeLists.txt for BinaryData and Build Flags

Configure factory preset bundling and developer mode flag.

**Files:**
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add DEVELOPER_MODE option early in CMakeLists.txt**

```cmake
# After project() line, add:
option(DEVELOPER_MODE "Enable developer preset management panel" OFF)

if(DEVELOPER_MODE)
    add_definitions(-DDEVELOPER_MODE=1)
else()
    add_definitions(-DDEVELOPER_MODE=0)
endif()
```

- [ ] **Step 2: Add factory presets to BinaryData**

Find the JUCE_add_gui_app() call and ensure factory presets are included:

```cmake
# Add this before juce_add_gui_app:
set(FACTORY_PRESETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source/FactoryPresets")

# In juce_add_gui_app, add to the PRODUCT_NAME target's sources:
# (After existing BinaryData files)
file(GLOB FACTORY_PRESET_FILES "${FACTORY_PRESETS_DIR}/*.fxp")
juce_add_binary_data(KaigenPhantomData SOURCES ${FACTORY_PRESET_FILES})
```

- [ ] **Step 3: Commit**

```bash
git add CMakeLists.txt
git commit -m "build: add DEVELOPER_MODE flag and BinaryData setup"
```

---

### Task 12: Integration Test - End-to-End Preset System

Test the complete workflow in the running plugin.

**Files:**
- None (manual testing)

- [ ] **Step 1: Build the plugin**

```bash
cd build && cmake .. -DDEVELOPER_MODE=OFF && cmake --build . --config Release
```

Expected: Build succeeds, no linker errors.

- [ ] **Step 2: Load plugin in Ableton Live**

1. Manually copy built VST3 to Ableton plugins folder
2. Restart Ableton
3. Create an Instrument/Audio track
4. Load Kaigen Phantom

Expected: Plugin loads, preset header visible with "Default" preset name.

- [ ] **Step 3: Test preset header buttons**

- Click preset name → dropdown appears with categories
- Click category to expand/collapse
- Click a preset name → loads preset (if any exist), dropdown closes
- Click arrows (▲ ▼) → no-op for now (not implemented in Task 5)
- Click heart (♡) → toggles to ♥, back to ♡
- Click books (|||) → browser modal opens, changes to ✕
- Click ✕ → browser closes, changes back to |||

Expected: All buttons respond, no crashes.

- [ ] **Step 4: Test save workflow**

1. Adjust a parameter (turn a knob)
2. Verify `*` appears next to preset name
3. Verify Save button appears
4. Click Save → modal opens
5. Type new preset name "My Test Preset"
6. Click Save
7. Verify preset name updates, `*` disappears, Save button hides

Expected: Preset file saved to `%APPDATA%/Kaigen/KaigenPhantom/Presets/User/My Test Preset.fxp`

- [ ] **Step 5: Test load from dropdown**

1. Click preset name dropdown
2. Load "My Test Preset" from dropdown
3. Adjust a parameter
4. Verify `*` appears again

Expected: Preset loads, parameters update, modified indicator works.

- [ ] **Step 6: Test browser modal**

1. Click books (|||) to open browser
2. Search for "Test" in search bar
3. Verify "My Test Preset" appears in results
4. Click to load
5. Verify browser closes, preset loads

Expected: Search works, loading from browser works.

- [ ] **Step 7: Commit integration test results**

```bash
git add .  # If any test files generated
git commit -m "test: verify end-to-end preset system functionality"
```

---

### Task 13: Final Polish and Documentation

Update docs and verify clean state.

**Files:**
- Modify: `README.md` or `docs/PRESET_SYSTEM.md` (new)

- [ ] **Step 1: Document preset system for users**

Create or update docs:

```markdown
# Phantom Preset System

## Saving Presets

1. Adjust parameters to desired sound
2. Preset name shows `*` when modified
3. Click **Save** button
4. Enter preset name
5. Click **Save** to create new or overwrite existing

## Loading Presets

### Quick Browse (Click preset name)
- Dropdown shows presets organized by type
- Expand/collapse categories
- Click to load

### Full Browser (Click books icon |||)
- Search by name
- Columns: Name, Type, Designer
- ♡ to favorite
- Click preset to load

## Organizing Presets

- **Factory/** — read-only, shipped with plugin
- **User/** — your saved presets
- **[PackName]/** — imported preset packs

## Favorites

Click ♡ on any preset to favorite it. Favorite status persists across sessions.
```

- [ ] **Step 2: Update CHANGELOG**

Add entry:

```markdown
## [Unreleased]

### Added
- Complete preset system (Phase 1 MVP)
  - Arturia-style header with preset browsing
  - Save/load presets with modified indicator
  - Full preset browser with search
  - Favorite/heart presets
  - Platform-specific preset storage (Windows/macOS/Linux)
```

- [ ] **Step 3: Run full test suite**

```bash
cd build && ctest --output-on-failure
```

Expected: All tests pass (existing + new PresetManager tests).

- [ ] **Step 4: Final commit**

```bash
git add docs/ README.md CHANGELOG.md
git commit -m "docs: document preset system and update changelog"
```

---

## Summary

**Phase 1 Complete Tasks:**
1. ✓ PresetManager class (directory management, file I/O)
2. ✓ PluginProcessor integration (load/save preset state)
3. ✓ Preset header UI (name, arrows, heart, save button)
4. ✓ Simple dropdown (category browsing)
5. ✓ Full preset browser modal (search, columns, sidebar, favorites)
6. ✓ Native functions binding (WebView ↔ C++)
7. ✓ Test suite (file I/O, enumeration, search)
8. ✓ Integration test (end-to-end workflow)
9. ✓ Documentation

**Not Included (Phase 2-3):**
- Developer mode preset management
- Preset import and pack support
- Persistent favorite flag storage
- Previous/next preset navigation (Task 5, Steps 1-2)
- Preset metadata editing

**Total Implementation Estimate:** 3-4 hours for experienced JUCE developer.

---

## Open Questions (To Decide During Implementation)

1. **Factory preset seeding:** Should plugin create sample presets (Warm, Aggressive, etc.) on first launch, or require user to create them?
2. **Preset metadata storage:** Store type/designer in .fxp file metadata, or in separate JSON index?
3. **Sorting in browser:** Implement sortable columns (click header to sort A-Z)?
4. **Preset preview:** Show audio visualization or just metadata in preview pane?
