#include "GifPlayer.h"
#include "../../ThirdParty/stb_image.h"

namespace kaigen::phantom
{

GifPlayer::GifPlayer() = default;

GifPlayer::~GifPlayer() { stopTimer(); }

void GifPlayer::clear()
{
    stopTimer();
    frames.clear();
    delaysMs.clear();
    currentFrame = 0;
}

bool GifPlayer::load(const juce::File& gif)
{
    clear();

    juce::MemoryBlock mb;
    if (! gif.loadFileAsData(mb)) return false;

    int width = 0, height = 0, layers = 0, comp = 0;
    int* delays = nullptr;
    stbi_uc* data = stbi_load_gif_from_memory(
        static_cast<const stbi_uc*>(mb.getData()),
        (int) mb.getSize(),
        &delays, &width, &height, &layers, &comp, 4);  // force RGBA

    if (data == nullptr || layers < 1 || width <= 0 || height <= 0)
    {
        if (data != nullptr)    stbi_image_free(data);
        if (delays != nullptr)  std::free(delays);
        return false;
    }

    const int frameBytes = width * height * 4;
    frames.reserve((size_t) layers);
    delaysMs.reserve((size_t) layers);

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
        frames.push_back(std::move(img));
        // stb reports delays in centiseconds * 10 (i.e. milliseconds);
        // a zero delay is the GIF spec's "as fast as possible" — clamp
        // to 50 ms to keep CPU usage sane.
        const int d = delays[f];
        delaysMs.push_back(d > 0 ? juce::jmax(20, d) : 50);
    }

    stbi_image_free(data);
    std::free(delays);

    if (frames.size() > 1)
        startTimer(delaysMs[0]);

    repaint();
    return true;
}

void GifPlayer::timerCallback()
{
    if (frames.size() < 2) { stopTimer(); return; }
    currentFrame = (currentFrame + 1) % (int) frames.size();
    startTimer(delaysMs[(size_t) currentFrame]);
    repaint();
}

void GifPlayer::paint(juce::Graphics& g)
{
    if (frames.empty()) return;
    g.drawImage(frames[(size_t) currentFrame], getLocalBounds().toFloat(),
                juce::RectanglePlacement::centred);
}

} // namespace kaigen::phantom
