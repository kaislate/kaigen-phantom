#include "PackArchive.h"

namespace kaigen::phantom
{

bool PackArchive::exportPack(const juce::File& packDir, const juce::File& destZipFile)
{
    if (! packDir.isDirectory()) return false;

    juce::ZipFile::Builder builder;

    // Find every regular file under packDir recursively.
    juce::Array<juce::File> files;
    packDir.findChildFiles(files, juce::File::findFiles, /*recursive*/ true);

    for (const auto& f : files)
    {
        // Store path relative to packDir's parent so the archive contains
        // <pack-name>/<rel-path> entries.
        const auto rel = f.getRelativePathFrom(packDir.getParentDirectory());
        // JUCE's ZipFile uses forward slashes regardless of host OS.
        const auto normalized = rel.replaceCharacter('\\', '/');
        builder.addFile(f, /*compression*/ 9, normalized);
    }

    if (! destZipFile.getParentDirectory().createDirectory().wasOk())
        return false;

    destZipFile.deleteFile();
    juce::FileOutputStream out(destZipFile);
    if (! out.openedOk()) return false;

    return builder.writeToStream(out, nullptr);
}

juce::String PackArchive::peekPackName(const juce::File& sourceZipFile)
{
    if (! sourceZipFile.existsAsFile()) return {};

    juce::ZipFile zip(sourceZipFile);
    const int n = zip.getNumEntries();
    if (n == 0) return {};

    // Scan entries, skipping macOS resource-fork metadata (__MACOSX/) and
    // similar tooling artefacts whose first path segment starts with "__".
    // Return the first segment of the first real entry.
    for (int i = 0; i < n; ++i)
    {
        const auto* entry = zip.getEntry(i);
        if (entry == nullptr) continue;

        const auto firstSlash = entry->filename.indexOfChar('/');
        if (firstSlash < 1) continue;

        const auto firstSegment = entry->filename.substring(0, firstSlash);
        if (firstSegment.startsWith("__")) continue;

        return firstSegment;
    }
    return {};
}

juce::String PackArchive::importPack(const juce::File& sourceZipFile,
                                     const juce::File& destRoot,
                                     bool overwriteExisting)
{
    if (! sourceZipFile.existsAsFile()) return {};
    if (! destRoot.isDirectory() && ! destRoot.createDirectory().wasOk()) return {};

    juce::ZipFile zip(sourceZipFile);
    if (zip.getNumEntries() == 0) return {};

    const auto packName = peekPackName(sourceZipFile);
    if (packName.isEmpty()) return {};

    auto packDir = destRoot.getChildFile(packName);
    if (packDir.exists())
    {
        if (! overwriteExisting) return {};
        if (! packDir.deleteRecursively()) return {};
    }

    const auto result = zip.uncompressTo(destRoot, /*shouldOverwriteFiles*/ true);
    if (result.failed()) return {};

    return packName;
}

} // namespace kaigen::phantom
