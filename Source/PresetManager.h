#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <map>
#include <set>

namespace kaigen::phantom
{

struct PresetMetadata
{
    juce::String name;         // "Warm Bass Boost"
    juce::String type;         // "Piano" | "Drone" | "Synth" | "Bass" | "Experimental"
    juce::String designer;     // "Kai Slate" | user name
    juce::String description;  // Optional free-form notes from the designer
    juce::String packName;     // "Factory" | "User" | pack folder name
    bool isFactory = false;
    bool isFavorite = false;
};

struct PresetInfo
{
    PresetMetadata metadata;
    juce::File file;
};

// A preset pack = any directory under Presets/. Factory and User are the
// built-in packs; third-party packs live in their own sibling folders.
// Optional `pack.json` in the folder provides display metadata; optional
// `cover.png` provides album art. Both are optional.
struct PackInfo
{
    juce::String name;          // Folder name (also the pack's key)
    juce::String displayName;   // From pack.json, or name if absent
    juce::String description;
    juce::String designer;
    bool         hasCoverArt = false;  // Whether cover.png exists in the pack folder
    int          presetCount = 0;
};

// Preset library manager. Owned by the AudioProcessor (not the Editor) so
// its lifetime matches the plugin's, not the UI's.
//
// On-disk format: each preset is a single .fxp file containing XML from a
// juce::ValueTree. The tree's root is the APVTS state; a <Metadata> child
// node holds name/type/designer. Favorites live in a separate
// `favorites.index` JSON file at the presets root so favorites work even
// when the preset lives in a read-only pack directory.
class PresetManager
{
public:
    PresetManager();
    ~PresetManager() = default;

    // Create directories, scan the disk, load favorites index.
    void initialize();

    // All presets, grouped by pack. Cheap copy (cached in memory).
    std::map<juce::String, std::vector<PresetInfo>> getAllPresets() const;

    // Returns the File for a preset, or a non-existent File if not found.
    juce::File getPresetFile(const juce::String& presetName,
                             const juce::String& packName) const;

    // Load a preset into the given APVTS. Must be called on the message thread.
    // Returns true on success, false if file missing / parse error.
    bool loadPreset(juce::AudioProcessorValueTreeState& apvts,
                    const juce::String& presetName,
                    const juce::String& packName);

    // Save APVTS state as a new preset in User/. If overwrite=false and a
    // preset with this name exists, a numeric suffix is appended.
    // Returns the saved preset's name (possibly disambiguated), or empty on failure.
    juce::String savePreset(juce::AudioProcessorValueTreeState& apvts,
                            const juce::String& presetName,
                            const juce::String& type,
                            const juce::String& designer,
                            const juce::String& description,
                            bool overwrite);

    // Delete a user preset (factory/pack presets cannot be deleted).
    bool deletePreset(const juce::String& presetName,
                      const juce::String& packName);

    // Favorites (persisted in favorites.index).
    void setFavorite(const juce::String& presetName,
                     const juce::String& packName,
                     bool isFavorite);
    bool isFavorite(const juce::String& presetName,
                    const juce::String& packName) const;

    // Rescan disk (call after external file changes).
    void rescan();

    // Packs (including Factory and User, plus any third-party pack dirs).
    std::vector<PackInfo> getAllPacks() const;

    // Cover art file for a pack (returns non-existent File if none present).
    juce::File getPackCoverFile(const juce::String& packName) const;

    // Path accessors.
    juce::File getPresetsRootDirectory() const;
    juce::File getUserPresetsDirectory() const;
    juce::File getFactoryPresetsDirectory() const;

private:
    void ensureDirectoryStructure();
    void scanPresetsFromDisk();

    void loadFavoritesIndex();
    void saveFavoritesIndex();
    juce::String favoriteKey(const juce::String& presetName,
                             const juce::String& packName) const;

    // Build the Metadata child ValueTree.
    static juce::ValueTree buildMetadataTree(const juce::String& name,
                                             const juce::String& type,
                                             const juce::String& designer,
                                             const juce::String& description);

    // Extract metadata from a preset file, if present.
    static PresetMetadata readMetadataFromFile(const juce::File& file);

    std::map<juce::String, std::vector<PresetInfo>> allPresets;
    std::map<juce::String, PackInfo> packs;  // keyed by folder name
    std::set<juce::String> favorites;        // keys: "packName/presetName"
};

} // namespace kaigen::phantom
