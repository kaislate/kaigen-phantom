#include "PresetManager.h"
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
    }
    else
    {
        // Legacy / externally-authored preset: supply sensible defaults.
        result.type = "Experimental";
    }

    return result;
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
                                       const juce::String& presetName,
                                       const juce::String& type,
                                       const juce::String& designer,
                                       const juce::String& description,
                                       bool overwrite)
{
    auto sanitized = sanitizeName(presetName);
    if (sanitized.isEmpty()) return {};

    const auto validType = kValidTypes.contains(type) ? type : juce::String("Experimental");
    const auto effectiveDesigner = designer.isEmpty() ? juce::String("User") : designer;

    auto userDir = getUserPresetsDirectory();
    auto target = userDir.getChildFile(sanitized + ".fxp");

    // Disambiguate the name if needed.
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

    // Build a fresh state tree: params + metadata child.
    auto state = apvts.copyState();

    // Remove any existing metadata node (from a previously-loaded preset)
    // before adding our own.
    auto existingMeta = state.getChildWithName(kMetadataNodeId);
    if (existingMeta.isValid())
        state.removeChild(existingMeta, nullptr);

    state.appendChild(buildMetadataTree(sanitized, validType, effectiveDesigner, description), nullptr);

    auto xml = state.createXml();
    if (xml == nullptr) return {};

    if (!target.replaceWithText(xml->toString()))
        return {};

    // Update in-memory cache.
    PresetInfo info;
    info.file = target;
    info.metadata.name = sanitized;
    info.metadata.type = validType;
    info.metadata.designer = effectiveDesigner;
    info.metadata.description = description;
    info.metadata.packName = kUserPackName;
    info.metadata.isFactory = false;
    info.metadata.isFavorite = isFavorite(sanitized, kUserPackName);

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
