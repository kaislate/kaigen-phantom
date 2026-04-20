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
    void loadMetadataJson(PresetInfo& presetInfo);
    void saveMetadataJson(const juce::File& presetFile,
                         const juce::String& type,
                         const juce::String& designer,
                         bool isFavorite);

    // Helper to extract string value from JSON
    juce::String extractJsonString(const juce::String& jsonStr, const juce::String& key);

    std::map<juce::String, std::vector<PresetInfo>> allPresets;
    std::map<juce::String, bool> favoriteMap;  // path -> isFavorite
};

} // namespace kaigen::phantom
