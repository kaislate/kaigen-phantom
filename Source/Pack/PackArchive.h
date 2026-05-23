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
