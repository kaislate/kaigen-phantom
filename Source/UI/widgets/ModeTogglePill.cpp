// Source/UI/widgets/ModeTogglePill.cpp
#include "ModeTogglePill.h"
#include "../Theme.h"
#include "../../Parameters.h"

namespace kaigen::phantom
{

namespace
{
    // CSS .mt container is depressed dark-tinted track; .mb.active is a
    // white raised pill with subtle shadow.
    constexpr juce::uint32 kTrackBg          = 0x14000000;   // rgba(0,0,0,0.08)
    constexpr juce::uint32 kInsetShadowTop   = 0x24000000;   // ~14% black, top inset
    constexpr juce::uint32 kInsetHighlightBot = 0x80FFFFFF;  // ~50% white, bottom inset
    constexpr juce::uint32 kSegIdleText      = 0x38000000;   // ~22% black
    constexpr juce::uint32 kSegHoverText     = 0x80000000;   // ~50% black
    constexpr juce::uint32 kSegActiveText    = 0x94000000;   // ~58% black
    constexpr juce::uint32 kSegActiveBg      = 0x8cFFFFFF;   // 55% white
    constexpr juce::uint32 kSegActiveShadow  = 0x1f000000;   // 12% black

    constexpr int kTrackRadius = 13;   // half of natural height
    constexpr int kSegRadius   = 11;
    constexpr int kPad         = 3;
}

ModeTogglePill::ModeTogglePill(juce::AudioProcessorValueTreeState& a)
    : apvts(a)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    apvts.addParameterListener(activeParamId, this);
}

ModeTogglePill::~ModeTogglePill()
{
    apvts.removeParameterListener(activeParamId, this);
}

void ModeTogglePill::setEnginePrefix(const juce::String& activePrefix,
                                       const juce::String& mirrorPrefix)
{
    const auto newActive = activePrefix + "mode";
    const auto newMirror = mirrorPrefix.isNotEmpty() ? (mirrorPrefix + "mode")
                                                     : juce::String{};
    if (newActive == activeParamId && newMirror == mirrorParamId) return;

    apvts.removeParameterListener(activeParamId, this);
    activeParamId = newActive;
    mirrorParamId = newMirror;
    apvts.addParameterListener(activeParamId, this);
    repaint();
}

void ModeTogglePill::parameterChanged(const juce::String&, float)
{
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<ModeTogglePill>(this)]
    {
        if (safe != nullptr) safe->repaint();
    });
}

juce::Rectangle<int> ModeTogglePill::segmentBounds(int idx) const
{
    const auto inner = getLocalBounds().reduced(kPad, kPad);
    const int segW = inner.getWidth() / 2;
    return idx == 0
        ? juce::Rectangle<int>(inner.getX(),          inner.getY(), segW, inner.getHeight())
        : juce::Rectangle<int>(inner.getX() + segW,   inner.getY(),
                                inner.getWidth() - segW, inner.getHeight());
}

int ModeTogglePill::hitTest(juce::Point<int> p) const
{
    if (segmentBounds(0).contains(p)) return 0;
    if (segmentBounds(1).contains(p)) return 1;
    return -1;
}

void ModeTogglePill::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Track — depressed dark-tinted pill with inset shadow + highlight.
    g.setColour(juce::Colour(kTrackBg));
    g.fillRoundedRectangle(bounds, (float) kTrackRadius);

    // Soft recessed-well shadows — same family as HeaderButton so the
    // chrome reads as one consistent design language. Soft gradients,
    // not hard bands.
    {
        juce::Path clip; clip.addRoundedRectangle(bounds, (float) kTrackRadius);
        juce::Graphics::ScopedSaveState save(g);
        g.reduceClipRegion(clip);

        // Top inset shadow — 18% black at the top edge, fading over
        // ~60% of the pill height. Gives "light fell into the well" cue.
        juce::ColourGradient topShadow(juce::Colour(0x2e000000),
                                        0.0f, bounds.getY(),
                                        juce::Colour(0x00000000),
                                        0.0f, bounds.getY() + bounds.getHeight() * 0.6f,
                                        false);
        g.setGradientFill(topShadow);
        g.fillRect(bounds);

        // Bottom inset highlight — 50% white at the bottom edge, fading up.
        juce::ColourGradient botHigh(juce::Colour(0x00FFFFFF),
                                      0.0f, bounds.getY() + bounds.getHeight() * 0.6f,
                                      juce::Colour(0x80FFFFFF),
                                      0.0f, bounds.getBottom(),
                                      false);
        g.setGradientFill(botHigh);
        g.fillRect(bounds);

        // Faint dark line along the very top — the "lip" where the
        // surrounding surface curves down into the pill.
        g.setColour(juce::Colour(0x28000000));
        g.drawLine(bounds.getX() + (float) kTrackRadius * 0.6f, bounds.getY() + 0.6f,
                    bounds.getRight() - (float) kTrackRadius * 0.6f, bounds.getY() + 0.6f, 0.7f);
    }

    // Outer "lip" highlight just below the bottom of the pill — surface
    // catches light on its raised edge, reinforcing the recess illusion.
    {
        const float xL = bounds.getX() + (float) kTrackRadius * 0.5f;
        const float xR = bounds.getRight() - (float) kTrackRadius * 0.5f;
        g.setColour(juce::Colour(0x66ffffff));
        g.drawLine(xL, bounds.getBottom() + 0.5f, xR, bounds.getBottom() + 0.5f, 0.7f);
    }

    const auto modeIdx = (int) apvts.getRawParameterValue(activeParamId)->load();

    auto paintSegment = [&](int idx, const juce::String& label) {
        const auto seg  = segmentBounds(idx);
        const auto segF = seg.toFloat();
        const bool isActive = (modeIdx == idx);
        const bool isHover  = (hoverSegment == idx);

        if (isActive)
        {
            g.setColour(juce::Colour(kSegActiveBg));
            g.fillRoundedRectangle(segF, (float) kSegRadius);
            g.setColour(juce::Colour(kSegActiveShadow));
            g.drawRoundedRectangle(segF.reduced(0.5f), (float) kSegRadius, 0.7f);
        }

        const auto colour = isActive ? juce::Colour(kSegActiveText)
                          : isHover  ? juce::Colour(kSegHoverText)
                                      : juce::Colour(kSegIdleText);
        g.setColour(colour);
        auto font = juce::Font(juce::FontOptions(Theme::uiFontFamily(), 10.0f,
                                                  juce::Font::bold))
                       .withExtraKerningFactor(0.25f);
        g.setFont(font);
        g.drawText(label, seg, juce::Justification::centred, false);
    };

    paintSegment(0, "EFFECT");
    paintSegment(1, "RESYN");
}

void ModeTogglePill::mouseMove(const juce::MouseEvent& e)
{
    const int h = hitTest(e.getPosition());
    if (h != hoverSegment)
    {
        hoverSegment = h;
        repaint();
    }
}

void ModeTogglePill::mouseExit(const juce::MouseEvent&)
{
    if (hoverSegment != -1) { hoverSegment = -1; repaint(); }
}

void ModeTogglePill::mouseDown(const juce::MouseEvent& e)
{
    const int h = hitTest(e.getPosition());
    if (h < 0) return;
    auto write = [&](const juce::String& id) {
        if (auto* p = apvts.getParameter(id))
        {
            const auto normalized = (float) h / (float) juce::jmax(1, p->getNumSteps() - 1);
            p->beginChangeGesture();
            p->setValueNotifyingHost(normalized);
            p->endChangeGesture();
        }
    };
    write(activeParamId);
    if (mirrorParamId.isNotEmpty())   // LINK mode — also flip the other engine
        write(mirrorParamId);
}

} // namespace kaigen::phantom
