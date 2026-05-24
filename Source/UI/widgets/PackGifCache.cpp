#include "PackGifCache.h"
#include "../../ThirdParty/stb_image.h"

namespace kaigen::phantom
{

namespace
{
    // Single shared tick interval. 33ms = ~30Hz which is fine for typical
    // GIFs (frames are 50-100ms apart). Lower would give smoother slow
    // GIFs at the cost of message-thread wakeups.
    constexpr int kTickMs = 33;
}

PackGifCache::PackGifCache() = default;

PackGifCache::~PackGifCache() { stopTimer(); }

void PackGifCache::setEnabled(bool e)
{
    if (enabled == e) return;
    enabled = e;
    if (! enabled)
        stopTimer();
    else if (! cache.empty())
        startTimer(kTickMs);
    if (repaintCb) repaintCb();
}

void PackGifCache::invalidate(const juce::String& packName)
{
    cache.erase(packName);
    if (cache.empty()) stopTimer();
}

void PackGifCache::clearAll()
{
    cache.clear();
    stopTimer();
}

bool PackGifCache::decodeInto(const juce::File& gifFile, Entry& outEntry)
{
    juce::MemoryBlock mb;
    if (! gifFile.loadFileAsData(mb)) return false;

    int width = 0, height = 0, layers = 0, comp = 0;
    int* delays = nullptr;
    stbi_uc* data = stbi_load_gif_from_memory(
        static_cast<const stbi_uc*>(mb.getData()),
        (int) mb.getSize(),
        &delays, &width, &height, &layers, &comp, 4);

    if (data == nullptr || layers < 1 || width <= 0 || height <= 0)
    {
        if (data != nullptr)   stbi_image_free(data);
        if (delays != nullptr) std::free(delays);
        return false;
    }

    const int frameBytes = width * height * 4;
    outEntry.frames.reserve((size_t) layers);
    outEntry.delaysMs.reserve((size_t) layers);

    for (int f = 0; f < layers; ++f)
    {
        juce::Image img(juce::Image::ARGB, width, height, /*clear*/ false);
        {
            juce::Image::BitmapData bd(img, juce::Image::BitmapData::writeOnly);
            const stbi_uc* src = data + f * frameBytes;
            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    const stbi_uc* p = src + (y * width + x) * 4;
                    bd.setPixelColour(x, y,
                        juce::Colour::fromRGBA(p[0], p[1], p[2], p[3]));
                }
            }
        }
        outEntry.frames.push_back(std::move(img));
        // stb reports delay in milliseconds. GIF spec allows zero (= "as
        // fast as possible") which we clamp to 50ms to keep CPU sane.
        const int d = delays[f];
        outEntry.delaysMs.push_back(d > 0 ? juce::jmax(20, d) : 50);
    }

    stbi_image_free(data);
    std::free(delays);
    return ! outEntry.frames.empty();
}

juce::Image PackGifCache::getCurrentFrame(const juce::String& packName, const juce::File& gifFile)
{
    auto it = cache.find(packName);
    if (it == cache.end())
    {
        Entry e;
        if (! decodeInto(gifFile, e)) return {};
        it = cache.emplace(packName, std::move(e)).first;
        if (enabled && it->second.frames.size() > 1 && ! isTimerRunning())
            startTimer(kTickMs);
    }

    const auto& entry = it->second;
    if (entry.frames.empty()) return {};
    if (! enabled) return entry.frames[0];
    return entry.frames[(size_t) entry.currentFrame];
}

void PackGifCache::timerCallback()
{
    if (! enabled || cache.empty()) { stopTimer(); return; }

    bool anyAdvanced = false;
    for (auto& [name, e] : cache)
    {
        if (e.frames.size() < 2) continue;
        e.msSinceFrameStart += kTickMs;
        const int needed = e.delaysMs[(size_t) e.currentFrame];
        if (e.msSinceFrameStart >= needed)
        {
            e.msSinceFrameStart = 0;
            e.currentFrame = (e.currentFrame + 1) % (int) e.frames.size();
            anyAdvanced = true;
        }
    }
    if (anyAdvanced && repaintCb) repaintCb();
}

} // namespace kaigen::phantom
