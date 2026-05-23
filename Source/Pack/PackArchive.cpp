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
    if (zip.getNumEntries() == 0) return {};

    const auto* entry = zip.getEntry(0);
    if (entry == nullptr) return {};

    const auto firstSlash = entry->filename.indexOfChar('/');
    if (firstSlash < 1) return {};
    return entry->filename.substring(0, firstSlash);
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
    if (! packDir.createDirectory().wasOk()) return {};

    const auto result = zip.uncompressTo(destRoot, /*shouldOverwriteFiles*/ true);
    if (result.failed()) return {};

    return packName;
}

} // namespace kaigen::phantom
