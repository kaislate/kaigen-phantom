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
