#include "PresetManager.h"
#include "ABSlotManager.h"
#include "Parameters.h"
#include <juce_core/juce_core.h>

namespace kaigen::phantom
{

namespace
{
    constexpr const char* kMetadataNodeId = "Metadata";
    constexpr const char* kFactoryPackName = "Factory";
    constexpr const char* kUserPackName = "User";

    const juce::StringArray kValidTypes { "Piano", "Drone", "Synth", "Bass", "Experimental" };

    juce::String sanitizeName(const juce::String& name)
    {
        // Strip path separators and oddball characters to keep preset names
        // as safe filenames. Spaces, parens, hyphens, underscores are fine.
        return name.trim().retainCharacters(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-()[]");
    }
}

juce::String presetKindToString(PresetKind k)
{
    switch (k)
    {
        case PresetKind::Single:  return "single";
        case PresetKind::AB:      return "ab";
        case PresetKind::ABMorph: return "ab_morph";
    }
    return "single";
}

PresetKind presetKindFromString(const juce::String& s)
{
    if (s == "ab")       return PresetKind::AB;
    if (s == "ab_morph") return PresetKind::ABMorph;
    return PresetKind::Single;
}

PresetManager::PresetManager() = default;

void PresetManager::initialize()
{
    ensureDirectoryStructure();
    loadFavoritesIndex();
    scanPresetsFromDisk();
}

// ── Directory structure ────────────────────────────────────────────────

juce::File PresetManager::getPresetsRootDirectory() const
{
    // Per-platform convention; all three map to the same JUCE enum.
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    return appData.getChildFile("Kaigen")
                  .getChildFile("KaigenPhantom")
                  .getChildFile("Presets");
}

juce::File PresetManager::getFactoryPresetsDirectory() const
{
    return getPresetsRootDirectory().getChildFile(kFactoryPackName);
}

juce::File PresetManager::getUserPresetsDirectory() const
{
    return getPresetsRootDirectory().getChildFile(kUserPackName);
}

void PresetManager::ensureDirectoryStructure()
{
    getPresetsRootDirectory().createDirectory();
    getFactoryPresetsDirectory().createDirectory();
    getUserPresetsDirectory().createDirectory();
}

// ── Preset file format ─────────────────────────────────────────────────
//
// A preset is an XML file containing the APVTS state tree. We attach a
// <Metadata> child node with name/type/designer. replaceState() restores
// both atomically on load.

juce::ValueTree PresetManager::buildMetadataTree(const juce::String& name,
                                                 const juce::String& type,
                                                 const juce::String& designer,
                                                 const juce::String& description)
{
    juce::ValueTree meta(kMetadataNodeId);
    meta.setProperty("name", name, nullptr);
    meta.setProperty("type", type, nullptr);
    meta.setProperty("designer", designer, nullptr);
    meta.setProperty("description", description, nullptr);
    return meta;
}

PresetMetadata PresetManager::readMetadataFromFile(const juce::File& file)
{
    PresetMetadata result;
    result.name = file.getFileNameWithoutExtension();

    auto xml = juce::parseXML(file);
    if (xml == nullptr) return result;

    auto tree = juce::ValueTree::fromXml(*xml);
    if (!tree.isValid()) return result;

    auto meta = tree.getChildWithName(kMetadataNodeId);
    if (meta.isValid())
    {
        result.name        = meta.getProperty("name", result.name).toString();
        result.type        = meta.getProperty("type", "Experimental").toString();
        result.designer    = meta.getProperty("designer", "").toString();
        result.description = meta.getProperty("description", "").toString();

        const auto kindStr = meta.getProperty("presetKind", juce::var("single")).toString();
        result.presetKind = presetKindFromString(kindStr);
    }
    else
    {
        // Legacy / externally-authored preset: supply sensible defaults.
        result.type = "Experimental";
    }

    return result;
}

PreviewData PresetManager::readPreviewFromState(const juce::ValueTree& state)
{
    PreviewData data;

    static const juce::String paramIds[7] = {
        ParamID::RECIPE_H2, ParamID::RECIPE_H3, ParamID::RECIPE_H4,
        ParamID::RECIPE_H5, ParamID::RECIPE_H6, ParamID::RECIPE_H7,
        ParamID::RECIPE_H8,
    };

    // APVTS serializes each parameter as a <PARAM id="..." value="..."/> child.
    // Walk the tree and pick out the ones we care about.
    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto child = state.getChild(i);
        if (! child.hasProperty("id")) continue;

        const auto id = child.getProperty("id").toString();
        const auto value = (float) (double) child.getProperty("value", 0.0);

        if (id == ParamID::PHANTOM_THRESHOLD)
        {
            data.crossover = value;
            continue;
        }

        if (id == ParamID::SYNTH_SKIP)
        {
            // APVTS stores this as a stepped float; round and clamp to the valid 0..8 range
            // so corrupted or out-of-range presets still render safely.
            data.skip = juce::jlimit(0, 8, juce::roundToInt(value));
            continue;
        }

        for (int h = 0; h < 7; ++h)
        {
            if (id == paramIds[h])
            {
                // APVTS stores recipe_h* as 0..100 (percent); normalize to 0..1
                // so consumers treat PreviewData.h[] uniformly on a 0..1 scale.
                data.h[h] = value * 0.01f;
                break;
            }
        }
    }

    return data;
}

// ── Scanning ───────────────────────────────────────────────────────────

void PresetManager::scanPresetsFromDisk()
{
    allPresets.clear();
    packs.clear();
    auto root = getPresetsRootDirectory();
    if (!root.isDirectory()) return;

    // Always include Factory and User as packs, even when empty.
    auto registerPack = [this](const juce::File& packDir, const juce::String& packName)
    {
        PackInfo info;
        info.name = packName;
        info.displayName = packName;

        auto manifest = packDir.getChildFile("pack.json");
        if (manifest.existsAsFile())
        {
            auto parsed = juce::JSON::parse(manifest);
            if (auto* obj = parsed.getDynamicObject())
            {
                auto get = [obj](const char* key, const juce::String& fallback)
                {
                    auto v = obj->getProperty(key);
                    return v.toString().isNotEmpty() ? v.toString() : fallback;
                };
                info.displayName = get("name", packName);
                info.description = get("description", "");
                info.designer    = get("designer", "");
            }
        }

        info.hasCoverArt = packDir.getChildFile("cover.png").existsAsFile()
                        || packDir.getChildFile("cover.jpg").existsAsFile();
        packs[packName] = info;
    };

    registerPack(getFactoryPresetsDirectory(), kFactoryPackName);
    registerPack(getUserPresetsDirectory(),    kUserPackName);

    for (const auto& packDir : root.findChildFiles(juce::File::findDirectories, false))
    {
        const auto packName = packDir.getFileName();
        std::vector<PresetInfo> presets;

        for (const auto& presetFile : packDir.findChildFiles(juce::File::findFiles, false, "*.fxp"))
        {
            PresetInfo info;
            info.file = presetFile;
            info.metadata = readMetadataFromFile(presetFile);
            info.metadata.packName = packName;
            info.metadata.isFactory = (packName == kFactoryPackName);
            info.metadata.isFavorite = isFavorite(info.metadata.name, packName);

            // Parse the state tree once more to extract preview parameter values.
            // readMetadataFromFile already parses the file; we repeat here to avoid
            // a signature change. The cost is negligible (few dozen presets at load).
            if (auto xml = juce::parseXML(presetFile))
            {
                auto state = juce::ValueTree::fromXml(*xml);
                if (state.isValid())
                    info.preview = readPreviewFromState(state);
            }

            presets.push_back(info);
        }

        if (!presets.empty())
        {
            std::sort(presets.begin(), presets.end(),
                [](const PresetInfo& a, const PresetInfo& b)
                { return a.metadata.name.compareIgnoreCase(b.metadata.name) < 0; });
            allPresets[packName] = std::move(presets);
        }

        // Ensure every directory (including third-party packs) is registered.
        if (packs.find(packName) == packs.end())
            registerPack(packDir, packName);

        packs[packName].presetCount = (int) presets.size();
    }

    // Fill preset counts for Factory/User too.
    for (auto& [name, info] : packs)
    {
        auto it = allPresets.find(name);
        info.presetCount = (it != allPresets.end()) ? (int) it->second.size() : 0;
    }
}

std::vector<PackInfo> PresetManager::getAllPacks() const
{
    std::vector<PackInfo> result;
    result.reserve(packs.size());
    for (const auto& [_, p] : packs) result.push_back(p);

    // Stable ordering: Factory first, User second, then everything else A-Z.
    std::sort(result.begin(), result.end(), [](const PackInfo& a, const PackInfo& b)
    {
        auto weight = [](const juce::String& n) -> int
        {
            if (n == "Factory") return 0;
            if (n == "User")    return 1;
            return 2;
        };
        const int wa = weight(a.name), wb = weight(b.name);
        if (wa != wb) return wa < wb;
        return a.name.compareIgnoreCase(b.name) < 0;
    });
    return result;
}

juce::File PresetManager::getPackCoverFile(const juce::String& packName) const
{
    auto packDir = getPresetsRootDirectory().getChildFile(packName);
    auto png = packDir.getChildFile("cover.png");
    if (png.existsAsFile()) return png;
    auto jpg = packDir.getChildFile("cover.jpg");
    if (jpg.existsAsFile()) return jpg;
    return {};
}

void PresetManager::rescan()
{
    scanPresetsFromDisk();
}

std::map<juce::String, std::vector<PresetInfo>> PresetManager::getAllPresets() const
{
    return allPresets;
}

juce::File PresetManager::getPresetFile(const juce::String& presetName,
                                        const juce::String& packName) const
{
    return getPresetsRootDirectory()
        .getChildFile(packName)
        .getChildFile(presetName + ".fxp");
}

// ── Load / save / delete ───────────────────────────────────────────────

bool PresetManager::loadPreset(juce::AudioProcessorValueTreeState& apvts,
                               const juce::String& presetName,
                               const juce::String& packName)
{
    auto file = getPresetFile(presetName, packName);
    if (!file.existsAsFile()) return false;

    auto xml = juce::parseXML(file);
    if (xml == nullptr) return false;

    auto tree = juce::ValueTree::fromXml(*xml);
    if (!tree.isValid()) return false;

    // Accept trees tagged with the APVTS state type OR a plain state tree;
    // we replace wholesale, and the Metadata child is carried along.
    if (tree.getType() != apvts.state.getType())
        return false;

    apvts.replaceState(tree);
    return true;
}

juce::String PresetManager::savePreset(juce::AudioProcessorValueTreeState& apvts,
                                       ABSlotManager* abSlots,
                                       const juce::String& presetName,
                                       const juce::String& type,
                                       const juce::String& designer,
                                       const juce::String& description,
                                       PresetKind kind,
                                       bool overwrite)
{
    auto sanitized = sanitizeName(presetName);
    if (sanitized.isEmpty()) return {};

    const auto validType = kValidTypes.contains(type) ? type : juce::String("Experimental");
    const auto effectiveDesigner = designer.isEmpty() ? juce::String("User") : designer;

    // Reject AB / AB+Morph saves when slots are identical (safety net — UI
    // should disable the radio in that state).
    if ((kind == PresetKind::AB || kind == PresetKind::ABMorph) && abSlots != nullptr)
    {
        const auto slotA = abSlots->getSlot(ABSlotManager::Slot::A);
        const auto slotB = abSlots->getSlot(ABSlotManager::Slot::B);
        if (slotA.toXmlString() == slotB.toXmlString())
            return {};
    }

    auto userDir = getUserPresetsDirectory();
    auto target = userDir.getChildFile(sanitized + ".fxp");

    if (target.existsAsFile() && !overwrite)
    {
        int suffix = 2;
        while (true)
        {
            auto candidate = userDir.getChildFile(sanitized + " " + juce::String(suffix) + ".fxp");
            if (!candidate.existsAsFile())
            {
                target = candidate;
                sanitized = sanitized + " " + juce::String(suffix);
                break;
            }
            if (++suffix > 999) return {};
        }
    }

    // Build the root state: for Single, use live APVTS (current behavior).
    // For AB / AB+Morph, the root is user's SLOT A (regardless of active slot).
    juce::ValueTree state;
    if (kind == PresetKind::Single || abSlots == nullptr)
    {
        state = apvts.copyState();
    }
    else
    {
        state = abSlots->getSlot(ABSlotManager::Slot::A).createCopy();
    }

    // Remove any pre-existing children that we're about to re-emit.
    if (auto existingMeta = state.getChildWithName(kMetadataNodeId); existingMeta.isValid())
        state.removeChild(existingMeta, nullptr);
    if (auto existingSlotB = state.getChildWithName("SlotB"); existingSlotB.isValid())
        state.removeChild(existingSlotB, nullptr);
    if (auto existingMorph = state.getChildWithName("MorphConfig"); existingMorph.isValid())
        state.removeChild(existingMorph, nullptr);

    // Metadata child, with presetKind prop.
    auto metadataTree = buildMetadataTree(sanitized, validType, effectiveDesigner, description);
    metadataTree.setProperty("presetKind", presetKindToString(kind), nullptr);
    state.appendChild(metadataTree, nullptr);

    // Slot B for AB / AB+Morph saves.
    if ((kind == PresetKind::AB || kind == PresetKind::ABMorph) && abSlots != nullptr)
    {
        state.appendChild(abSlots->buildPresetSlotBChild(), nullptr);
    }

    // <MorphConfig> is Pro-build only — not emitted in this plan.

    auto xml = state.createXml();
    if (xml == nullptr) return {};

    if (!target.replaceWithText(xml->toString()))
        return {};

    // Update in-memory cache (same as before, plus presetKind).
    PresetInfo info;
    info.file = target;
    info.metadata.name = sanitized;
    info.metadata.type = validType;
    info.metadata.designer = effectiveDesigner;
    info.metadata.description = description;
    info.metadata.packName = kUserPackName;
    info.metadata.isFactory = false;
    info.metadata.isFavorite = isFavorite(sanitized, kUserPackName);
    info.metadata.presetKind = kind;
    info.preview = readPreviewFromState(state);

    auto& userList = allPresets[kUserPackName];
    auto it = std::find_if(userList.begin(), userList.end(),
        [&](const PresetInfo& p) { return p.metadata.name == sanitized; });
    if (it != userList.end())
        *it = info;
    else
        userList.push_back(info);

    std::sort(userList.begin(), userList.end(),
        [](const PresetInfo& a, const PresetInfo& b)
        { return a.metadata.name.compareIgnoreCase(b.metadata.name) < 0; });

    return sanitized;
}

bool PresetManager::deletePreset(const juce::String& presetName,
                                 const juce::String& packName)
{
    // Factory and pack presets are read-only; only User/ deletes are allowed.
    if (packName != kUserPackName) return false;

    auto file = getPresetFile(presetName, packName);
    if (!file.existsAsFile()) return false;
    if (!file.deleteFile()) return false;

    // Also remove from favorites if present.
    const auto key = favoriteKey(presetName, packName);
    if (favorites.erase(key) > 0)
        saveFavoritesIndex();

    // Update cache.
    auto packIt = allPresets.find(packName);
    if (packIt != allPresets.end())
    {
        auto& list = packIt->second;
        list.erase(std::remove_if(list.begin(), list.end(),
            [&](const PresetInfo& p) { return p.metadata.name == presetName; }),
            list.end());
        if (list.empty())
            allPresets.erase(packIt);
    }

    return true;
}

// ── Favorites ──────────────────────────────────────────────────────────

juce::String PresetManager::favoriteKey(const juce::String& presetName,
                                        const juce::String& packName) const
{
    return packName + "/" + presetName;
}

void PresetManager::setFavorite(const juce::String& presetName,
                                const juce::String& packName,
                                bool fav)
{
    const auto key = favoriteKey(presetName, packName);
    bool changed = false;

    if (fav)
        changed = favorites.insert(key).second;
    else
        changed = favorites.erase(key) > 0;

    // Update cached metadata so the next getAllPresets() reflects the change.
    auto packIt = allPresets.find(packName);
    if (packIt != allPresets.end())
    {
        for (auto& preset : packIt->second)
        {
            if (preset.metadata.name == presetName)
            {
                preset.metadata.isFavorite = fav;
                break;
            }
        }
    }

    if (changed)
        saveFavoritesIndex();
}

bool PresetManager::isFavorite(const juce::String& presetName,
                               const juce::String& packName) const
{
    return favorites.count(favoriteKey(presetName, packName)) > 0;
}

void PresetManager::loadFavoritesIndex()
{
    favorites.clear();
    auto indexFile = getPresetsRootDirectory().getChildFile("favorites.index");
    if (!indexFile.existsAsFile()) return;

    auto parsed = juce::JSON::parse(indexFile);
    if (auto* arr = parsed.getArray())
    {
        for (const auto& v : *arr)
            favorites.insert(v.toString());
    }
}

void PresetManager::saveFavoritesIndex()
{
    juce::Array<juce::var> arr;
    for (const auto& key : favorites)
        arr.add(key);

    auto json = juce::JSON::toString(juce::var(arr));
    auto indexFile = getPresetsRootDirectory().getChildFile("favorites.index");
    indexFile.replaceWithText(json);
}

} // namespace kaigen::phantom
