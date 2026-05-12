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
    apvts.addParameterListener(ParamID::A_MODE, this);
}

ModeTogglePill::~ModeTogglePill()
{
    apvts.removeParameterListener(ParamID::A_MODE, this);
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

    // Soft inset top shadow + inset bottom highlight (clipped to the
    // rounded pill so the bands don't bleed past the corners).
    {
        juce::Path clip; clip.addRoundedRectangle(bounds, (float) kTrackRadius);
        juce::Graphics::ScopedSaveState save(g);
        g.reduceClipRegion(clip);

        juce::ColourGradient topShadow(juce::Colour(kInsetShadowTop),
                                        0.0f, bounds.getY(),
                                        juce::Colour(0x00000000),
                                        0.0f, bounds.getY() + 4.0f, false);
        g.setGradientFill(topShadow);
        g.fillRect(juce::Rectangle<float>(bounds.getX(), bounds.getY(),
                                           bounds.getWidth(), 5.0f));

        juce::ColourGradient botHigh(juce::Colour(0x00FFFFFF),
                                      0.0f, bounds.getBottom() - 3.0f,
                                      juce::Colour(kInsetHighlightBot),
                                      0.0f, bounds.getBottom(), false);
        g.setGradientFill(botHigh);
        g.fillRect(juce::Rectangle<float>(bounds.getX(), bounds.getBottom() - 3.0f,
                                           bounds.getWidth(), 3.0f));
    }

    const auto modeIdx = (int) apvts.getRawParameterValue(ParamID::A_MODE)->load();

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
    if (auto* p = apvts.getParameter(ParamID::A_MODE))
    {
        const auto normalized = (float) h / (float) juce::jmax(1, p->getNumSteps() - 1);
        p->beginChangeGesture();
        p->setValueNotifyingHost(normalized);
        p->endChangeGesture();
    }
}

} // namespace kaigen::phantom
