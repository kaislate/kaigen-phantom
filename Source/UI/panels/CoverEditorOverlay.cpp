// Source/UI/panels/CoverEditorOverlay.cpp
#include "CoverEditorOverlay.h"

namespace kaigen::phantom
{

namespace
{
    constexpr int kCardW         = 520;
    constexpr int kCardH         = 620;
    constexpr int kFrameSize     = 360;   // square crop frame
    constexpr int kCardPad       = 24;
    constexpr int kCloseSize     = 28;
    constexpr int kBottomBarH    = 56;
    constexpr int kTopBarH       = 56;

    constexpr juce::uint32 kBackdropColour = 0xb3000000;   // 70% black dim
    constexpr juce::uint32 kCardSurface    = 0xff1b1d24;
    constexpr juce::uint32 kCardBorder     = 0xff2c3038;
    constexpr juce::uint32 kFrameStroke    = 0xffFFFFFF;
}

CoverEditorOverlay::CoverEditorOverlay()
{
    setInterceptsMouseClicks(true, true);

    chooseImageButton.getProperties().set("phantom-style", "header-raised");
    chooseImageButton.onClick = [this] { pickSourceImage(); };
    addAndMakeVisible(chooseImageButton);

    saveButton.getProperties().set("phantom-style", "header-raised");
    saveButton.onClick = [this]
    {
        if (! sourceFile.existsAsFile() || ! onSave || isSaving || isLoading) return;

        // Run the heavy save work (image decode + crop render + PNG
        // encode, or GIF file copy) on a background thread so a large
        // source file doesn't freeze the editor + DAW. Capture the
        // overlay safely so a mid-save dismiss doesn't crash.
        isSaving = true;
        setBusyButtons(true);
        repaint();

        juce::Component::SafePointer<CoverEditorOverlay> self(this);
        const auto cb       = onSave;
        const auto packName = activePackName;
        const auto file     = sourceFile;
        const auto isGif    = isGifSource;
        const auto sc       = scale;
        const auto ox       = offsetX;
        const auto oy       = offsetY;

        juce::Thread::launch([self, cb, packName, file, isGif, sc, ox, oy]
        {
            // setPackCoverWithCrop is thread-safe: file I/O + off-screen
            // juce::Graphics + sendChangeMessage (which is documented
            // thread-safe). The callback target also does no UI work.
            cb(packName, file, isGif, sc, ox, oy);

            juce::MessageManager::callAsync([self]
            {
                if (auto* p = self.getComponent())
                {
                    p->isSaving = false;
                    p->setBusyButtons(false);
                    if (p->onDismiss) p->onDismiss();
                }
            });
        });
    };
    addAndMakeVisible(saveButton);

    cancelButton.getProperties().set("phantom-style", "header-raised");
    cancelButton.onClick = [this] { if (onDismiss) onDismiss(); };
    addAndMakeVisible(cancelButton);

    closeButton.setButtonText(juce::String(juce::CharPointer_UTF8("\xE2\x9C\x95"))); // ✕
    closeButton.getProperties().set("phantom-style", "header-glyph");
    closeButton.onClick = [this] { if (onDismiss) onDismiss(); };
    addAndMakeVisible(closeButton);

    previewCache.setEnabled(true);
    previewCache.setRepaintCallback([this] { repaint(); });
}

CoverEditorOverlay::~CoverEditorOverlay() = default;

juce::Rectangle<int> CoverEditorOverlay::cardBounds() const
{
    auto area = getLocalBounds();
    return area.withSizeKeepingCentre(juce::jmin(area.getWidth() - 40, kCardW),
                                       juce::jmin(area.getHeight() - 40, kCardH));
}

juce::Rectangle<int> CoverEditorOverlay::frameRect() const
{
    auto card = cardBounds().reduced(kCardPad);
    card.removeFromTop(kTopBarH);
    card.removeFromBottom(kBottomBarH);
    return card.withSizeKeepingCentre(kFrameSize, kFrameSize);
}

void CoverEditorOverlay::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(kBackdropColour));

    const auto card = cardBounds();
    g.setColour(juce::Colour(kCardSurface));
    g.fillRoundedRectangle(card.toFloat(), 8.0f);
    g.setColour(juce::Colour(kCardBorder));
    g.drawRoundedRectangle(card.toFloat().reduced(0.5f), 8.0f, 1.0f);

    // Header label.
    g.setColour(juce::Colour(0xffd0d2d4));
    g.setFont(juce::FontOptions(juce::Font::getDefaultSansSerifFontName(), 16.0f, juce::Font::bold));
    g.drawText("Set Pack Cover",
               card.reduced(kCardPad, 0).withHeight(kTopBarH),
               juce::Justification::centredLeft, false);

    // Crop frame area.
    const auto frame = frameRect();
    g.setColour(juce::Colour(0xff0e0f13));
    g.fillRect(frame.expanded(2));

    if (sourceImage.isValid())
    {
        // Compute the on-screen rectangle the source image occupies.
        const float drawnW = sourceImage.getWidth()  * scale;
        const float drawnH = sourceImage.getHeight() * scale;
        const juce::Rectangle<float> drawn {
            (float) frame.getX() + offsetX,
            (float) frame.getY() + offsetY,
            drawnW, drawnH
        };

        if (isGifSource)
        {
            // Show animated GIF preview behind the frame mask. Clip to
            // the frame so the cropped region matches what the user will
            // see in the browser after save.
            juce::Graphics::ScopedSaveState ss(g);
            g.reduceClipRegion(frame);
            const auto current = previewCache.getCurrentFrame(activePackName, sourceFile);
            if (current.isValid())
                g.drawImage(current, drawn,
                            juce::RectanglePlacement::stretchToFit);
            else
                g.drawImage(sourceImage, drawn,
                            juce::RectanglePlacement::stretchToFit);
        }
        else
        {
            juce::Graphics::ScopedSaveState ss(g);
            g.reduceClipRegion(frame);
            g.drawImage(sourceImage, drawn,
                        juce::RectanglePlacement::stretchToFit);
        }
    }
    else
    {
        g.setColour(juce::Colour(0x66ffffff));
        g.setFont(juce::FontOptions(13.0f));
        const char* msg = isLoading ? "Loading..."
                       : isSaving  ? "Saving..."
                                    : "Pick an image to begin";
        g.drawText(msg, frame, juce::Justification::centred, false);
    }

    if ((isLoading || isSaving) && sourceImage.isValid())
    {
        // Overlay a faint busy indicator on top of the existing preview.
        g.setColour(juce::Colour(0x99000000));
        g.fillRect(frame);
        g.setColour(juce::Colour(0xffd0d2d4));
        g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        g.drawText(isSaving ? "Saving..." : "Loading...",
                   frame, juce::Justification::centred, false);
    }

    // Frame outline drawn on top so it stays visible over the image.
    g.setColour(juce::Colour(kFrameStroke));
    g.drawRect(frame, 2);

    // Instruction text below the frame.
    if (sourceImage.isValid())
    {
        auto info = juce::Rectangle<int>(frame.getX(),
                                          frame.getBottom() + 6,
                                          frame.getWidth(), 16);
        g.setColour(juce::Colour(0xff7a8088));
        g.setFont(juce::FontOptions(11.0f));
        g.drawText("Drag to pan \xC2\xB7 Scroll to zoom",
                   info, juce::Justification::centred, false);
    }
}

void CoverEditorOverlay::resized()
{
    auto card = cardBounds();
    closeButton.setBounds(card.getRight() - kCloseSize - 8,
                          card.getY() + 8, kCloseSize, 24);

    auto top = card.reduced(kCardPad, 0).withHeight(kTopBarH);
    chooseImageButton.setBounds(top.removeFromRight(140).reduced(0, 14));

    auto bottom = card.removeFromBottom(kBottomBarH).reduced(kCardPad, 14);
    cancelButton.setBounds(bottom.removeFromLeft(96));
    saveButton  .setBounds(bottom.removeFromRight(96));
}

void CoverEditorOverlay::mouseDown(const juce::MouseEvent& e)
{
    // Backdrop click outside the card dismisses (matches SettingsOverlay).
    if (! cardBounds().contains(e.getPosition()))
    {
        if (onDismiss) onDismiss();
        return;
    }

    if (! sourceImage.isValid() || ! frameRect().contains(e.getPosition()))
        return;

    dragAnchor       = e.position;
    dragStartOffsetX = offsetX;
    dragStartOffsetY = offsetY;
}

void CoverEditorOverlay::mouseDrag(const juce::MouseEvent& e)
{
    if (! sourceImage.isValid()) return;

    offsetX = dragStartOffsetX + (e.position.x - dragAnchor.x);
    offsetY = dragStartOffsetY + (e.position.y - dragAnchor.y);
    repaint();
}

void CoverEditorOverlay::mouseWheelMove(const juce::MouseEvent& e,
                                         const juce::MouseWheelDetails& wheel)
{
    if (! sourceImage.isValid()) return;
    const auto frame = frameRect();
    if (! frame.contains(e.getPosition())) return;

    const float zoomStep = 1.0f + (wheel.deltaY > 0 ? 0.1f : -0.1f);
    const float newScale = juce::jlimit(0.1f, 10.0f, scale * zoomStep);
    if (newScale == scale) return;

    // Zoom about the cursor position so it stays anchored to the same
    // source-image pixel under the cursor.
    const float cursorXInFrame = (float) e.position.x - (float) frame.getX();
    const float cursorYInFrame = (float) e.position.y - (float) frame.getY();
    const float srcX = (cursorXInFrame - offsetX) / scale;
    const float srcY = (cursorYInFrame - offsetY) / scale;

    scale = newScale;
    offsetX = cursorXInFrame - srcX * scale;
    offsetY = cursorYInFrame - srcY * scale;
    repaint();
}

void CoverEditorOverlay::openForPack(const juce::String& packName,
                                      const juce::File& existingCover)
{
    activePackName = packName;
    if (existingCover.existsAsFile())
    {
        loadSource(existingCover);   // async; sets buttons + paints
    }
    else
    {
        sourceFile = {};
        sourceImage = {};
        isGifSource = false;
        isLoading = false;
        isSaving = false;
        setBusyButtons(false);   // disables Save (no source yet)
    }
    setVisible(true);
    toFront(false);
    grabKeyboardFocus();
    repaint();
}

void CoverEditorOverlay::pickSourceImage()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Choose cover art",
        juce::File::getSpecialLocation(juce::File::userPicturesDirectory),
        "*.png;*.jpg;*.jpeg;*.gif");
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                         | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
        {
            const auto picked = fc.getResult();
            if (picked.existsAsFile())
                loadSource(picked);
        });
}

void CoverEditorOverlay::loadSource(const juce::File& file)
{
    // Decoding a large image on the message thread freezes the whole
    // editor + DAW for multi-second images. Run the decode on a
    // background thread and post the resulting juce::Image back to the
    // message thread. SafePointer guards against the overlay being
    // dismissed mid-load.
    sourceFile = file;
    isGifSource = file.getFileExtension().equalsIgnoreCase(".gif");
    sourceImage = {};
    isLoading = true;
    setBusyButtons(true);
    repaint();

    juce::Component::SafePointer<CoverEditorOverlay> self(this);
    juce::Thread::launch([self, file]
    {
        auto decoded = juce::ImageFileFormat::loadFrom(file);

        juce::MessageManager::callAsync([self, decoded, file]
        {
            auto* p = self.getComponent();
            if (p == nullptr) return;
            // Drop the result if the user picked a different file in
            // the meantime (the SafePointer is alive but a fresh load
            // is now in flight).
            if (p->sourceFile != file) return;

            p->isLoading = false;
            p->setBusyButtons(false);
            if (decoded.isValid())
            {
                p->sourceImage = decoded;
                p->resetTransformForFit();
            }
            else
            {
                p->sourceFile = {};
            }
            p->repaint();
        });
    });
}

void CoverEditorOverlay::setBusyButtons(bool busy)
{
    chooseImageButton.setEnabled(! busy);
    saveButton       .setEnabled(! busy && sourceFile.existsAsFile());
    cancelButton     .setEnabled(! busy);
    closeButton      .setEnabled(! busy);
}

void CoverEditorOverlay::resetTransformForFit()
{
    if (! sourceImage.isValid()) return;

    // Fill the frame (cover-style — smaller edge fills, larger edge
    // crops). Matches the browser tile rendering default.
    const float w = (float) sourceImage.getWidth();
    const float h = (float) sourceImage.getHeight();
    const float fs = (float) kFrameSize;
    scale = juce::jmax(fs / w, fs / h);
    offsetX = (fs - w * scale) * 0.5f;
    offsetY = (fs - h * scale) * 0.5f;
}

} // namespace kaigen::phantom
