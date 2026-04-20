#include "PresetManager.h"
#include <juce_core/juce_core.h>
#include <set>

namespace kaigen::phantom {

PresetManager::PresetManager() {
}

void PresetManager::initialize() {
    ensureDirectoryStructure();
    loadPresetsFromDisk();
}

juce::String PresetManager::extractJsonString(const juce::String& jsonStr, const juce::String& key) {
    auto searchKey = "\"" + key + "\":\"";
    if (!jsonStr.contains(searchKey)) {
        return "";
    }

    auto start = jsonStr.indexOf(searchKey);
    if (start < 0) return "";

    start += searchKey.length();
    auto end = jsonStr.indexOf("\"", start);

    if (end > start) {
        return jsonStr.substring(start, end);
    }
    return "";
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

void PresetManager::loadMetadataJson(PresetInfo& presetInfo) {
    auto jsonFile = presetInfo.file.withFileExtension(".json");

    if (!jsonFile.exists()) {
        // Fallback to defaults if no JSON file
        presetInfo.metadata.type = "Experimental";
        presetInfo.metadata.designer = "Kai Slate";
        presetInfo.metadata.isFavorite = false;
        return;
    }

    auto jsonStr = jsonFile.loadFileAsString();

    auto type = extractJsonString(jsonStr, "type");
    if (!type.isEmpty()) {
        presetInfo.metadata.type = type;
    }

    auto designer = extractJsonString(jsonStr, "designer");
    if (!designer.isEmpty()) {
        presetInfo.metadata.designer = designer;
    }

    presetInfo.metadata.isFavorite = jsonStr.contains("\"isFavorite\":true");
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

            // Load metadata from .json if it exists, otherwise use defaults
            loadMetadataJson(info);

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
    std::set<juce::String> typeSet;

    for (const auto& [packName, presets] : allPresets) {
        for (const auto& preset : presets) {
            typeSet.insert(preset.metadata.type);
        }
    }

    return std::vector<juce::String>(typeSet.begin(), typeSet.end());
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

    auto presetFile = juce::File(presetPath);
    if (!presetFile.exists() || presetFile.getFileExtension() != ".fxp") {
        return;
    }

    // Validate path is within presets directory
    auto presetsRoot = getPresetsRootDirectory();
    if (!presetFile.getFullPathName().startsWith(presetsRoot.getFullPathName())) {
        jassertfalse;  // Path outside preset directory
        return;
    }

    auto jsonFile = presetFile.withFileExtension(".json");
    juce::String type = "Experimental";
    juce::String designer = "Kai Slate";

    if (jsonFile.exists()) {
        auto jsonStr = jsonFile.loadFileAsString();
        auto jsonType = extractJsonString(jsonStr, "type");
        if (!jsonType.isEmpty()) type = jsonType;

        auto jsonDesigner = extractJsonString(jsonStr, "designer");
        if (!jsonDesigner.isEmpty()) designer = jsonDesigner;
    }

    saveMetadataJson(presetFile, type, designer, isFavorite);
}

void PresetManager::saveMetadataJson(const juce::File& presetFile,
                                     const juce::String& type,
                                     const juce::String& designer,
                                     bool isFavorite) {
    auto jsonFile = presetFile.withFileExtension(".json");

    // Escape special characters
    auto escapedType = type.replace("\"", "\\\"").replace("\\", "\\\\");
    auto escapedDesigner = designer.replace("\"", "\\\"").replace("\\", "\\\\");

    auto jsonStr = juce::String("{\"type\":\"") + escapedType +
                   "\",\"designer\":\"" + escapedDesigner +
                   "\",\"isFavorite\":" + (isFavorite ? "true" : "false") + "}";

    jsonFile.replaceWithText(jsonStr);
}

juce::String PresetManager::savePreset(const juce::String& presetName,
                                       const juce::String& type,
                                       const juce::MemoryBlock& fxpData) {
    // Validate type
    const auto validTypes = std::vector<juce::String>{"Piano", "Drone", "Synth", "Bass", "Experimental"};
    auto validType = std::find(validTypes.begin(), validTypes.end(), type) != validTypes.end() ? type : "Experimental";

    auto userDir = getUserPresetsDirectory();
    auto presetFile = userDir.getChildFile(presetName + ".fxp");

    // Save .fxp file
    if (!presetFile.replaceWithData(fxpData.getData(), fxpData.getSize())) {
        jassertfalse;  // File write failed
        return "";
    }

    // Save metadata to .json file using validated type
    saveMetadataJson(presetFile, validType, "User", false);

    // Update in-memory cache
    PresetInfo newPreset;
    newPreset.file = presetFile;
    newPreset.metadata.name = presetName;
    newPreset.metadata.type = validType;
    newPreset.metadata.designer = "User";
    newPreset.metadata.packName = "User";
    newPreset.metadata.isFactory = false;
    newPreset.metadata.isFavorite = false;

    allPresets["User"].push_back(newPreset);

    return presetFile.getFullPathName();
}

juce::File PresetManager::getPresetFile(const juce::String& presetName,
                                        const juce::String& packName) const {
    return getPresetsRootDirectory()
        .getChildFile(packName)
        .getChildFile(presetName + ".fxp");
}

} // namespace kaigen::phantom
