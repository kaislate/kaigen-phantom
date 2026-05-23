#include <catch2/catch_test_macros.hpp>
#include "../Source/Pack/PackArchive.h"

using namespace kaigen::phantom;

namespace
{
    // Build a temporary pack folder with pack.json + one .fxp + cover.png.
    juce::File makeFixturePack(const juce::File& parent)
    {
        auto packDir = parent.getChildFile("FixturePack");
        packDir.deleteRecursively();
        packDir.createDirectory();

        packDir.getChildFile("pack.json").replaceWithText(
            R"({"name":"FixturePack","description":"d","designer":"me"})");
        packDir.getChildFile("Preset One.fxp").replaceWithText(
            "<KaigenPhantomState/>");
        packDir.getChildFile("cover.png").replaceWithData(
            "\x89PNG\r\n\x1a\n", 8);
        return packDir;
    }
}

TEST_CASE("PackArchive: exportPack writes a valid .kaipack containing the pack folder", "[pack-archive]")
{
    auto tmp = juce::File::createTempFile("kaipack-test");
    tmp.deleteFile();
    tmp.createDirectory();

    const auto packDir = makeFixturePack(tmp);
    const auto destZip = tmp.getChildFile("FixturePack.kaipack");

    REQUIRE(PackArchive::exportPack(packDir, destZip));
    REQUIRE(destZip.existsAsFile());
    REQUIRE(destZip.getSize() > 0);
    CHECK(PackArchive::peekPackName(destZip) == "FixturePack");

    tmp.deleteRecursively();
}

TEST_CASE("PackArchive: roundtrip preserves files and metadata", "[pack-archive]")
{
    auto tmp = juce::File::createTempFile("kaipack-rt");
    tmp.deleteFile();
    tmp.createDirectory();

    const auto packDir = makeFixturePack(tmp);
    const auto destZip = tmp.getChildFile("Roundtrip.kaipack");
    REQUIRE(PackArchive::exportPack(packDir, destZip));

    // Import into a fresh sibling folder.
    auto importRoot = tmp.getChildFile("imported");
    importRoot.createDirectory();
    const auto packName = PackArchive::importPack(destZip, importRoot, /*overwrite*/ true);
    REQUIRE(packName == "FixturePack");

    const auto extracted = importRoot.getChildFile("FixturePack");
    CHECK(extracted.getChildFile("pack.json").existsAsFile());
    CHECK(extracted.getChildFile("Preset One.fxp").existsAsFile());
    CHECK(extracted.getChildFile("cover.png").existsAsFile());

    tmp.deleteRecursively();
}

TEST_CASE("PackArchive: importPack refuses to overwrite when flag is false", "[pack-archive]")
{
    auto tmp = juce::File::createTempFile("kaipack-noov");
    tmp.deleteFile();
    tmp.createDirectory();

    const auto packDir = makeFixturePack(tmp);
    const auto destZip = tmp.getChildFile("Pack.kaipack");
    REQUIRE(PackArchive::exportPack(packDir, destZip));

    auto importRoot = tmp.getChildFile("imported");
    importRoot.createDirectory();
    REQUIRE(PackArchive::importPack(destZip, importRoot, /*overwrite*/ true) == "FixturePack");
    // Second import with overwrite=false must fail.
    CHECK(PackArchive::importPack(destZip, importRoot, /*overwrite*/ false).isEmpty());

    tmp.deleteRecursively();
}

TEST_CASE("PackArchive: malformed archive returns empty pack name", "[pack-archive]")
{
    auto tmp = juce::File::createTempFile("kaipack-bad");
    tmp.deleteFile();
    tmp.replaceWithText("not a zip");

    CHECK(PackArchive::peekPackName(tmp).isEmpty());
    CHECK(PackArchive::importPack(tmp, tmp.getParentDirectory(), true).isEmpty());

    tmp.deleteFile();
}

TEST_CASE("PackArchive: peekPackName skips __MACOSX entries", "[pack-archive]")
{
    auto tmp = juce::File::createTempFile("kaipack-macosx");
    tmp.deleteFile();
    tmp.createDirectory();

    // Build a zip whose first entry is __MACOSX metadata, second is the
    // real pack folder. peekPackName must return "RealPack", not "__MACOSX".
    auto metaSrc = tmp.getChildFile("metadata_blob");
    metaSrc.replaceWithText("macos resource fork");
    auto realSrc = tmp.getChildFile("pack.json");
    realSrc.replaceWithText(R"({"name":"RealPack"})");

    juce::ZipFile::Builder builder;
    builder.addFile(metaSrc, 9, "__MACOSX/somefile");
    builder.addFile(realSrc, 9, "RealPack/pack.json");

    const auto destZip = tmp.getChildFile("MixedTopLevel.kaipack");
    {
        juce::FileOutputStream out(destZip);
        REQUIRE(out.openedOk());
        REQUIRE(builder.writeToStream(out, nullptr));
    }

    CHECK(PackArchive::peekPackName(destZip) == "RealPack");

    tmp.deleteRecursively();
}
