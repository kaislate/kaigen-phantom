// Source/UI/panels/CoverEditorOverlay.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../widgets/PackGifCache.h"

namespace kaigen::phantom
{

// CoverEditorOverlay — modal full-editor overlay that opens when the
// designer clicks the AUTHORING strip's "Set Cover" button. Picks an
// image (PNG/JPG/GIF), then lets the designer drag to pan and scroll
// to zoom inside a square crop frame. On save:
//   - Static images (PNG/JPG): render the cropped region to a new PNG
//     and call PresetManager::setPackCover with that file.
//   - GIFs: copy the original file as-is and write a cover.crop.json
//     sidecar with the scale + offset so the render path applies the
//     crop as a clip+transform mask (preserves frame data & timing).
class CoverEditorOverlay : public juce::Component
{
public:
    CoverEditorOverlay();
    ~CoverEditorOverlay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e,
                        const juce::MouseWheelDetails& wheel) override;

    /** Open the overlay; if the pack already has a cover, prefill with it. */
    void openForPack(const juce::String& packName, const juce::File& existingCover);

    /** Dismissed via Cancel button or Escape. */
    std::function<void()> onDismiss;

    /** Fired on Save. The pack name is whatever openForPack received.
     *  - sourceFile is the file the user picked (PNG/JPG/GIF).
     *  - isGif tells the caller which storage path to use.
     *  - scale/offsetX/offsetY are the crop transform in
     *    source-image coordinates: srcRect = frameRectInSrc(scale, offsets). */
    std::function<void(juce::String packName,
                       juce::File   sourceFile,
                       bool         isGif,
                       float        scale,
                       float        offsetX,
                       float        offsetY)> onSave;

private:
    void pickSourceImage();
    void loadSource(const juce::File& file);
    void resetTransformForFit();   // initial fit-to-frame on load

    juce::Rectangle<int> cardBounds() const;
    juce::Rectangle<int> frameRect() const;  // the square crop frame

    juce::String activePackName;
    juce::File   sourceFile;
    juce::Image  sourceImage;     // first frame for GIFs, full image otherwise
    bool         isGifSource { false };

    // Crop transform — where the source image sits relative to the
    // frame's top-left corner, in screen pixels.
    float scale   { 1.0f };
    float offsetX { 0.0f };
    float offsetY { 0.0f };

    // Drag tracking
    juce::Point<float> dragAnchor;
    float              dragStartOffsetX { 0.0f };
    float              dragStartOffsetY { 0.0f };

    // UI
    juce::TextButton chooseImageButton { "Choose Image\xE2\x80\xA6" };
    juce::TextButton saveButton        { "Save" };
    juce::TextButton cancelButton      { "Cancel" };
    juce::TextButton closeButton;

    // Optional GIF playback in the editor so the user previews motion
    // while cropping. Mirrors the frame cadence of the on-disk source.
    PackGifCache previewCache;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CoverEditorOverlay)
};

} // namespace kaigen::phantom
