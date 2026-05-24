#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace kaigen::phantom
{

// GifPlayer — animated GIF playback as a JUCE Component.
//
// JUCE's juce::GIFImageFormat decodes only the first frame, so packs with
// animated cover art need a dedicated player. Decode happens once at
// load() time via stb_image's stbi_load_gif_from_memory; thereafter a
// juce::Timer advances frames using the per-frame delay table.
class GifPlayer : public juce::Component,
                  private juce::Timer
{
public:
    GifPlayer();
    ~GifPlayer() override;

    /** Load and decode a GIF from disk. Returns true if at least one
     *  frame was decoded. Stops + clears any previous animation. */
    bool load(const juce::File& gif);

    /** Drop frames + stop the timer. */
    void clear();

    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    std::vector<juce::Image> frames;
    std::vector<int>         delaysMs;
    int                      currentFrame { 0 };
};

} // namespace kaigen::phantom
