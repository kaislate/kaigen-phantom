#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <map>
#include <functional>

namespace kaigen::phantom
{

// PackGifCache — shared decoded-frame cache + single advance timer for
// every animated pack cover visible in the PresetBrowser. The browser
// owns one instance and calls getCurrentFrame(packName, file) during
// paint for tiles, the preview pane, and the drill-in banner. The
// cache decodes on first request (synchronously — typical pack covers
// are small enough that decode latency is sub-frame); a single 30Hz
// juce::Timer advances every active animation and fires the registered
// repaint callback when at least one frame index changed.
//
// Cache lifecycle:
//   - First getCurrentFrame for a pack: decode and store frames.
//   - invalidate(packName): drop the entry — call from rescan / cover
//     replacement so the next request reloads from disk.
//   - clearAll(): drop everything (browser hidden, settings disabled).
//
// Animation can be disabled globally via setEnabled(false) — paint sites
// then receive the first frame only, and the advance timer stops.
class PackGifCache : private juce::Timer
{
public:
    PackGifCache();
    ~PackGifCache() override;

    /** When false, getCurrentFrame returns frame 0 only and the advance
     *  timer is stopped. Cached frames are kept so re-enabling is cheap. */
    void setEnabled(bool enabled);
    bool isEnabled() const noexcept { return enabled; }

    /** PresetBrowser hooks its own repaint() here so the timer can trigger
     *  redraws when at least one tile's current frame changes. */
    void setRepaintCallback(std::function<void()> cb) { repaintCb = std::move(cb); }

    /** Returns the current frame for packName, decoding the file on first
     *  request. Returns an invalid juce::Image when the file is missing,
     *  not a GIF, or decoding failed. */
    juce::Image getCurrentFrame(const juce::String& packName, const juce::File& gifFile);

    /** Drop a single pack's cached entry — call after setPackCover
     *  replaces a cover or deletePack removes it. */
    void invalidate(const juce::String& packName);

    /** Drop the entire cache + stop the timer. */
    void clearAll();

private:
    struct Entry
    {
        std::vector<juce::Image> frames;
        std::vector<int>         delaysMs;
        int                      currentFrame      { 0 };
        int                      msSinceFrameStart { 0 };
    };

    void timerCallback() override;
    bool decodeInto(const juce::File& gifFile, Entry& outEntry);

    std::map<juce::String, Entry> cache;
    bool                          enabled { true };
    std::function<void()>         repaintCb;
};

} // namespace kaigen::phantom
