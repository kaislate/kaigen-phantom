#include "PresetManager.h"
#include "Parameters.h"
#include "PresetMigration.h"
#include "Pack/PackArchive.h"
#include <juce_core/juce_core.h>

#if KAIGEN_HAS_FACTORY_PACKS
 #include "KaigenFactoryPacks.h"
#endif

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

    void writePackManifest(const juce::File& packDir,
                            const juce::String& displayName,
                            const juce::String& description,
                            const juce::String& designer)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("name",        displayName);
        obj->setProperty("description", description);
        obj->setProperty("designer",    designer);

        auto json = juce::JSON::toString(juce::var(obj));
        packDir.getChildFile("pack.json").replaceWithText(json);
    }
}

juce::String presetKindToString(PresetKind k)
{
    switch (k)
    {
        case PresetKind::Single: return "single";
    }
    return "single";
}

PresetKind presetKindFromString(const juce::String& /*s*/)
{
    // PR1 retired the AB / ABMorph kinds. Anything we read off disk —
    // including legacy "ab" / "ab_morph" tags from pre-PR1 presets — is
    // mapped to Single; PresetMigration handles the on-disk shape.
    return PresetKind::Single;
}

PresetManager::PresetManager() = default;

PresetManager::~PresetManager()
{
    stopTimer();
}

void PresetManager::initialize()
{
    ensureDirectoryStructure();
    loadFavoritesIndex();
    scanPresetsFromDisk();
    refreshPackModTimes();

    // 2-second poll: cheap (a handful of stat() calls), and saving in another
    // instance becomes visible to this one within ~2 s without needing a
    // project reload.
    startTimer(2000);
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

    // PR1: preset preview shows engine A's harmonics. Per-engine PARAM ids are
    // prefixed (a_*); legacy un-prefixed ids are also accepted so old preset
    // files render correctly in the browser before being explicitly loaded
    // (which is the point at which migration formally rewrites them).
    static const juce::String paramIdsA[7] = {
        ParamID::A_RECIPE_H2, ParamID::A_RECIPE_H3, ParamID::A_RECIPE_H4,
        ParamID::A_RECIPE_H5, ParamID::A_RECIPE_H6, ParamID::A_RECIPE_H7,
        ParamID::A_RECIPE_H8,
    };
    static const juce::String legacyIds[7] = {
        ParamID::LEAF_RECIPE_H2, ParamID::LEAF_RECIPE_H3, ParamID::LEAF_RECIPE_H4,
        ParamID::LEAF_RECIPE_H5, ParamID::LEAF_RECIPE_H6, ParamID::LEAF_RECIPE_H7,
        ParamID::LEAF_RECIPE_H8,
    };

    // APVTS serializes each parameter as a <PARAM id="..." value="..."/> child.
    // Walk the tree and pick out the ones we care about.
    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto child = state.getChild(i);
        if (! child.hasProperty("id")) continue;

        const auto id = child.getProperty("id").toString();
        const auto value = (float) (double) child.getProperty("value", 0.0);

        if (id == ParamID::A_PHANTOM_THRESHOLD || id == ParamID::LEAF_PHANTOM_THRESHOLD)
        {
            data.crossover = value;
            continue;
        }

        if (id == ParamID::A_SYNTH_SKIP || id == ParamID::LEAF_SYNTH_SKIP)
        {
            // APVTS stores this as a stepped float; round and clamp to the valid 0..8 range
            // so corrupted or out-of-range presets still render safely.
            data.skip = juce::jlimit(0, 8, juce::roundToInt(value));
            continue;
        }

        for (int h = 0; h < 7; ++h)
        {
            if (id == paramIdsA[h] || id == legacyIds[h])
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
                        || packDir.getChildFile("cover.jpg").existsAsFile()
                        || packDir.getChildFile("cover.gif").existsAsFile();
        info.isReadOnly = (packName == kFactoryPackName);
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

    // Embedded factory packs supplement the on-disk packs. They get scanned
    // last so a disk pack of the same name would win — useful during designer
    // review iterations where they want a live copy to override the baked-in
    // one without rebuilding.
    loadFactoryPacksFromBinaryData();
}

void PresetManager::loadFactoryPacksFromBinaryData()
{
#if KAIGEN_HAS_FACTORY_PACKS
    // Group embedded resources by pack name; see the "<PackName>__<file>"
    // mangling note below for the exact originalFilenames[] path shape.
    // Routes metadata files to PackInfo and *.fxp files to PresetInfo
    // entries with embeddedData populated.
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

        // juce_add_binary_data only stores file basenames, so the CMake glue
        // mangles "<PackName>/<file>" into "<PackName>__<file>" before staging.
        // Split on the first "__" to recover (packName, fileName).
        const int sep = origPath.indexOf("__");
        if (sep < 1 || sep + 2 >= origPath.length()) continue;
        const auto packName = origPath.substring(0, sep);
        const auto fileName = origPath.substring(sep + 2);

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
              || fileName.equalsIgnoreCase("cover.jpg")
              || fileName.equalsIgnoreCase("cover.gif"))
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
    auto gif = packDir.getChildFile("cover.gif");
    if (gif.existsAsFile()) return gif;
    auto png = packDir.getChildFile("cover.png");
    if (png.existsAsFile()) return png;
    auto jpg = packDir.getChildFile("cover.jpg");
    if (jpg.existsAsFile()) return jpg;
    return {};
}

void PresetManager::rescan()
{
    scanPresetsFromDisk();
    refreshPackModTimes();
    sendChangeMessage();
}

void PresetManager::refreshPackModTimes()
{
    packModTimes.clear();
    const auto root = getPresetsRootDirectory();
    if (! root.isDirectory()) return;

    for (auto& entry : juce::RangedDirectoryIterator(
            root, /*recursive*/ false, "*", juce::File::findDirectories))
    {
        const auto dir = entry.getFile();
        packModTimes[dir.getFileName()] = dir.getLastModificationTime();
    }
}

void PresetManager::timerCallback()
{
    const auto root = getPresetsRootDirectory();
    if (! root.isDirectory()) return;

    // Build the current snapshot of pack-dir mod times and compare to the
    // last one. Any add / remove / mod-time bump triggers a rescan.
    std::map<juce::String, juce::Time> current;
    for (auto& entry : juce::RangedDirectoryIterator(
            root, /*recursive*/ false, "*", juce::File::findDirectories))
    {
        const auto dir = entry.getFile();
        current[dir.getFileName()] = dir.getLastModificationTime();
    }

    if (current != packModTimes)
    {
        packModTimes = std::move(current);
        scanPresetsFromDisk();
        sendChangeMessage();
    }
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

    // Run preset migration. Pre-PR1 presets had un-prefixed per-engine PARAM
    // ids and may carry <SlotB> / <MorphConfig> children — migration rewrites
    // them in place to the new dual-engine format. Idempotent on already-new
    // presets, so safe to run unconditionally.
    PresetMigration::migrateInPlace(tree);

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

    // PR1: every save is Single — the snapshot is just the live APVTS state.
    // The legacy <SlotB> / <MorphConfig> emission paths are gone.
    juce::ValueTree state = apvts.copyState();

    // Remove any pre-existing children that we're about to re-emit (or that
    // shouldn't survive into a fresh save).
    if (auto existingMeta = state.getChildWithName(kMetadataNodeId); existingMeta.isValid())
        state.removeChild(existingMeta, nullptr);
    if (auto existingSlotB = state.getChildWithName("SlotB"); existingSlotB.isValid())
        state.removeChild(existingSlotB, nullptr);
    if (auto existingMorph = state.getChildWithName("MorphConfig"); existingMorph.isValid())
        state.removeChild(existingMorph, nullptr);

    auto metadataTree = buildMetadataTree(sanitized, validType, effectiveDesigner, description);
    metadataTree.setProperty("presetKind", presetKindToString(PresetKind::Single), nullptr);
    state.appendChild(metadataTree, nullptr);

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
    info.metadata.presetKind = PresetKind::Single;
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
    {
        saveFavoritesIndex();
        sendChangeMessage();   // refresh any UI showing favorite state
    }
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

PresetManager::CoverCrop PresetManager::getPackCoverCrop(const juce::String& packName) const
{
    CoverCrop c;
    auto packDir = getPresetsRootDirectory().getChildFile(packName);
    auto sidecar = packDir.getChildFile("cover.crop.json");
    if (! sidecar.existsAsFile()) return c;

    auto parsed = juce::JSON::parse(sidecar);
    if (auto* obj = parsed.getDynamicObject())
    {
        c.valid   = true;
        c.scale   = (float) (double) obj->getProperty("scale");
        c.offsetX = (float) (double) obj->getProperty("offsetX");
        c.offsetY = (float) (double) obj->getProperty("offsetY");
        if (c.scale <= 0.0f) c.scale = 1.0f;   // guard against malformed
    }
    return c;
}

bool PresetManager::setPackCoverWithCrop(const juce::String& packName,
                                          const juce::File& sourceImage,
                                          float scale, float offsetX, float offsetY)
{
    auto packIt = packs.find(packName);
    if (packIt == packs.end() || packIt->second.isReadOnly) return false;
    if (! sourceImage.existsAsFile()) return false;

    auto packDir = getPresetsRootDirectory().getChildFile(packName);
    if (! packDir.isDirectory()) return false;

    const auto ext = sourceImage.getFileExtension().toLowerCase();
    const bool isPng  = ext == ".png";
    const bool isJpg  = ext == ".jpg" || ext == ".jpeg";
    const bool isGif  = ext == ".gif";
    if (! (isPng || isJpg || isGif)) return false;

    // Wipe any prior cover.* and sidecar — only one cover stays.
    packDir.getChildFile("cover.png").deleteFile();
    packDir.getChildFile("cover.jpg").deleteFile();
    packDir.getChildFile("cover.gif").deleteFile();
    packDir.getChildFile("cover.crop.json").deleteFile();

    if (isGif)
    {
        // Copy the original GIF + write a crop sidecar so the render
        // path can apply the mask without re-encoding any frames.
        auto destGif = packDir.getChildFile("cover.gif");
        if (! sourceImage.copyFileTo(destGif)) return false;

        auto* obj = new juce::DynamicObject();
        obj->setProperty("scale",   (double) scale);
        obj->setProperty("offsetX", (double) offsetX);
        obj->setProperty("offsetY", (double) offsetY);
        packDir.getChildFile("cover.crop.json")
              .replaceWithText(juce::JSON::toString(juce::var(obj)));

        rescan();
        return true;
    }

    // Static path: load source, render the cropped square region to a
    // new juce::Image, write as PNG. JPEG sources lose their original
    // extension here (output is always PNG); colour fidelity is
    // preserved by the rescale + PNG encode round-trip.
    auto src = juce::ImageFileFormat::loadFrom(sourceImage);
    if (! src.isValid()) return false;

    // Compute the source-image rect that maps to the unit square frame.
    // Editor coords: scaledSrcW = src.w * scale, drawn at (offsetX,offsetY).
    // To find the source rect we invert: srcX = -offsetX / scale, etc.
    // Frame size in editor is irrelevant for output dimensions; we
    // produce a fixed-size square output so all covers are uniform.
    constexpr int kOutputSize = 720;   // square PNG output

    // The user picked a 1:1 frame in the editor (kFrameSize). In source
    // pixels, that frame is kFrameSize / scale wide and tall, anchored
    // at (-offsetX/scale, -offsetY/scale). The CoverEditorOverlay used
    // kFrameSize=360, but that value isn't visible here — we re-derive
    // the equivalent crop in source pixels using only scale + offsets.
    //
    // editor frame side length = 360 px on screen
    // source pixels per editor px = 1/scale
    // source rect = 360/scale square at editor (0,0) -> source
    //               (-offsetX/scale, -offsetY/scale)
    constexpr float kEditorFrameSize = 360.0f;
    const float srcRectSize = kEditorFrameSize / scale;
    const float srcX = -offsetX / scale;
    const float srcY = -offsetY / scale;

    juce::Image out(juce::Image::ARGB, kOutputSize, kOutputSize, true);
    {
        juce::Graphics g(out);
        // Background — matches drawPackCover's transparency backdrop so
        // any out-of-source area composites cleanly.
        g.fillAll(juce::Colour(0xff1f2128));

        // Draw the source image scaled so that srcRectSize source px
        // map to kOutputSize output px. Position so srcX/srcY land at
        // output origin.
        const float outScale = (float) kOutputSize / srcRectSize;
        const auto t = juce::AffineTransform::translation(-srcX, -srcY)
                            .scaled(outScale, outScale);
        g.drawImageTransformed(src, t, false);
    }

    auto destPng = packDir.getChildFile("cover.png");
    juce::FileOutputStream os(destPng);
    if (! os.openedOk()) return false;
    juce::PNGImageFormat fmt;
    if (! fmt.writeImageToStream(out, os)) return false;
    os.flush();

    rescan();
    return true;
}

bool PresetManager::setPackCover(const juce::String& packName, const juce::File& sourceImage)
{
    auto packIt = packs.find(packName);
    if (packIt == packs.end() || packIt->second.isReadOnly) return false;
    if (! sourceImage.existsAsFile()) return false;

    auto packDir = getPresetsRootDirectory().getChildFile(packName);
    if (! packDir.isDirectory()) return false;

    // Accept PNG / JPG / JPEG / GIF. Anything else: refuse.
    const auto ext = sourceImage.getFileExtension().toLowerCase();
    const bool isPng  = ext == ".png";
    const bool isJpg  = ext == ".jpg" || ext == ".jpeg";
    const bool isGif  = ext == ".gif";
    if (! (isPng || isJpg || isGif)) return false;

    // For PNG/JPG, confirm the source actually decodes before we destroy
    // any existing cover (catches renamed extensions / corrupt files).
    // For GIFs the byte-copy is enough; JUCE's first-frame decoder would
    // run later in the animation path.
    if (! isGif)
    {
        auto probe = juce::ImageFileFormat::loadFrom(sourceImage);
        if (! probe.isValid()) return false;
    }

    // Always wipe any prior cover.* — only one stays.
    packDir.getChildFile("cover.png").deleteFile();
    packDir.getChildFile("cover.jpg").deleteFile();
    packDir.getChildFile("cover.gif").deleteFile();

    // Copy as-is. Preserves PNG alpha, JPG colour fidelity, and GIF
    // animation timing without re-encoding artefacts. Dropped the prior
    // 512px resize: factory bake-in size is a developer concern at build
    // time; on-disk user packs aren't space-constrained.
    const auto destExt = isJpg ? ".jpg" : ext;
    auto dest = packDir.getChildFile("cover" + destExt);
    if (! sourceImage.copyFileTo(dest)) return false;

    rescan();
    return true;
}

// Designer-mode equivalent of savePreset, with three intentional differences:
//   - target directory is the caller-specified packDir (not the User folder)
//   - skips the defensive SlotB/MorphConfig strip from savePreset because
//     the live APVTS state has been free of those children since PR1
//     (2026-05-04) — re-add if we ever load a pre-PR1 preset into the live
//     editor and then re-save it
//   - calls rescan() instead of the incremental cache update savePreset
//     does. Designer-mode saves are user-initiated menu actions, not a hot
//     UI loop, so the simpler full rescan is fine here. If this ever
//     becomes hot, mirror the savePreset cache-mutation pattern.
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
#endif

} // namespace kaigen::phantom
