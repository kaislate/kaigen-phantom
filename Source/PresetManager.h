#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <map>
#include <set>

namespace kaigen::phantom
{

// PR1 (2026-05-04) unified the dual-SKU split into a single always-on
// dual-engine binary. The legacy AB / ABMorph save kinds — which depended
// on ABSlotManager and the per-slot APVTS clones it used to maintain —
// are gone; every save is a Single now. Old presets that still carry
// <SlotB> / <MorphConfig> children are still loadable: PresetMigration
// folds them back into the new a_/b_ prefixed APVTS layout on load.
enum class PresetKind
{
    Single
};

juce::String presetKindToString(PresetKind);
PresetKind   presetKindFromString(const juce::String&);

struct PresetMetadata
{
    juce::String name;         // "Warm Bass Boost"
    juce::String type;         // "Piano" | "Drone" | "Synth" | "Bass" | "Experimental"
    juce::String designer;     // "Kai Slate" | user name
    juce::String description;  // Optional free-form notes from the designer
    juce::String packName;     // "Factory" | "User" | pack folder name
    bool isFactory = false;
    bool isFavorite = false;
    PresetKind   presetKind = PresetKind::Single;
};

// Parameter values extracted from a preset for the browser preview spectrum.
// Populated once at scan time so hovering a preset triggers no disk I/O.
struct PreviewData
{
    float h[7]       {};       // recipe_h2 .. recipe_h8, normalized 0..1
    float crossover  = 120.0f; // phantom_threshold in Hz (matches APVTS default in Parameters.h)
    int   skip       = 0;      // synth_skip, 0..8 (matches APVTS default)
};

struct PresetInfo
{
    PresetMetadata metadata;
    PreviewData    preview;
    juce::File     file;
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
class PresetManager : public juce::Timer,
                       public juce::ChangeBroadcaster
{
public:
    PresetManager();
    ~PresetManager() override;

    // Create directories, scan the disk, load favorites index.
    void initialize();

    // All presets, grouped by pack. Cheap copy (cached in memory).
    std::map<juce::String, std::vector<PresetInfo>> getAllPresets() const;

    // Returns the File for a preset, or a non-existent File if not found.
    juce::File getPresetFile(const juce::String& presetName,
                             const juce::String& packName) const;

    // Load a preset into the given APVTS. Must be called on the message thread.
    // Returns true on success, false if file missing / parse error.
    //
    // Legacy <SlotB> / <MorphConfig> children in pre-PR1 presets are folded
    // back into the new a_/b_ prefixed APVTS layout by PresetMigration before
    // the state is applied. Idempotent on already-new presets.
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

    // Rescan disk (call after external file changes). Sends a change
    // notification if the preset list actually changed.
    void rescan();

    /** Timer callback (every 2 s) — checks the modification times of each
     *  pack directory and triggers a rescan + change notification when any
     *  diff is detected. Catches presets saved/deleted by another instance
     *  of the plugin without needing a project reload. */
    void timerCallback() override;

    // Packs (including Factory and User, plus any third-party pack dirs).
    std::vector<PackInfo> getAllPacks() const;

    // Cover art file for a pack (returns non-existent File if none present).
    juce::File getPackCoverFile(const juce::String& packName) const;

    // Path accessors.
    juce::File getPresetsRootDirectory() const;
    juce::File getUserPresetsDirectory() const;
    juce::File getFactoryPresetsDirectory() const;

    // Extract the preview parameter values from a preset's APVTS state tree.
    // Missing recipe params default to 0; missing crossover defaults to 120 Hz.
    static PreviewData readPreviewFromState(const juce::ValueTree& state);

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

    /** Snapshot of last-modified times per pack directory, used by the
     *  filesystem-watcher Timer to detect external changes. */
    std::map<juce::String, juce::Time> packModTimes;
    void refreshPackModTimes();
};

} // namespace kaigen::phantom
