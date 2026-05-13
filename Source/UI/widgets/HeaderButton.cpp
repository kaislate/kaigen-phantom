// Source/UI/widgets/HeaderButton.cpp
#include "HeaderButton.h"
#include "../Theme.h"

namespace kaigen::phantom
{

namespace
{
    constexpr juce::uint32 kBodyBg          = 0x1f000000;   // ~12% black — slightly deeper than CSS so the well reads
    constexpr juce::uint32 kIconIdle        = 0x47000000;   // rgba(0,0,0,0.28)
    constexpr juce::uint32 kIconHover       = 0x8c000000;   // rgba(0,0,0,0.55)
    constexpr juce::uint32 kIconActive      = 0xe03773c3;   // rgba(55,115,195,0.88)
    constexpr juce::uint32 kInsetShadowTop  = 0x4d000000;   // ~30% black — deeper inset shadow
    constexpr juce::uint32 kInsetHighlight  = 0xb3ffffff;   // ~70% white — brighter inset highlight
    constexpr juce::uint32 kOuterRimLight   = 0x99ffffff;   // ~60% white — light "lip" above the recess
    constexpr juce::uint32 kActiveGlow      = 0x474682d2;   // rgba(70,130,210,0.28)

    void paintIcon(juce::Graphics& g, juce::Rectangle<float> bounds,
                   HeaderButton::Icon icon, juce::Colour stroke)
    {
        const auto cx = bounds.getCentreX();
        const auto cy = bounds.getCentreY();
        const auto r  = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f * 0.6f;

        juce::Path p;
        const juce::PathStrokeType strokeT(1.7f,
                                            juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded);

        switch (icon)
        {
            case HeaderButton::Icon::Bypass:
            {
                // Power symbol — open circle with a vertical notch at top.
                const float gap = r * 0.35f;
                const float startAngle = juce::MathConstants<float>::pi * 1.5f + gap;
                const float endAngle   = juce::MathConstants<float>::pi * 1.5f
                                          - gap + juce::MathConstants<float>::twoPi;
                p.addCentredArc(cx, cy, r * 0.85f, r * 0.85f, 0.0f,
                                startAngle, endAngle, true);
                // Vertical bar from centre up through the notch.
                p.startNewSubPath(cx, cy - r * 0.95f);
                p.lineTo(cx, cy - r * 0.15f);
                break;
            }
            case HeaderButton::Icon::Settings:
            {
                // Gear — outer circle + small spokes around the perimeter.
                p.addEllipse(cx - r * 0.55f, cy - r * 0.55f, r * 1.1f, r * 1.1f);
                p.addEllipse(cx - r * 0.18f, cy - r * 0.18f, r * 0.36f, r * 0.36f);
                for (int i = 0; i < 6; ++i)
                {
                    const float a = (float) i * juce::MathConstants<float>::twoPi / 6.0f;
                    const float ix0 = cx + std::cos(a) * r * 0.65f;
                    const float iy0 = cy + std::sin(a) * r * 0.65f;
                    const float ix1 = cx + std::cos(a) * r * 0.95f;
                    const float iy1 = cy + std::sin(a) * r * 0.95f;
                    p.startNewSubPath(ix0, iy0);
                    p.lineTo(ix1, iy1);
                }
                break;
            }
            case HeaderButton::Icon::AdvancedChevron:
            {
                // Downward chevron — wide V.
                p.startNewSubPath(cx - r * 0.85f, cy - r * 0.30f);
                p.lineTo(cx,                       cy + r * 0.50f);
                p.lineTo(cx + r * 0.85f,           cy - r * 0.30f);
                break;
            }
        }

        g.setColour(stroke);
        g.strokePath(p, strokeT);
    }
}

HeaderButton::HeaderButton(juce::AudioProcessorValueTreeState& a,
                            const juce::String& paramId, Icon ic)
    : apvts(&a), boundParam(paramId), icon(ic)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    apvts->addParameterListener(boundParam, this);
}

HeaderButton::HeaderButton(Icon ic)
    : icon(ic)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

HeaderButton::~HeaderButton()
{
    if (apvts != nullptr && boundParam.isNotEmpty())
        apvts->removeParameterListener(boundParam, this);
}

bool HeaderButton::isActive() const
{
    if (apvts == nullptr || boundParam.isEmpty()) return false;
    if (auto* v = apvts->getRawParameterValue(boundParam))
        return v->load() > 0.5f;
    return false;
}

void HeaderButton::parameterChanged(const juce::String&, float)
{
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<HeaderButton>(this)]
    {
        if (safe != nullptr) safe->repaint();
    });
}

void HeaderButton::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    const auto active = isActive();

    // Body fill — slightly darker than the surrounding silver to suggest
    // a pocket cut into the surface.
    g.setColour(juce::Colour(kBodyBg));
    g.fillEllipse(bounds);

    // Inset shadow + highlight — stronger gradients than before so the
    // well reads as recessed. Top-left dark crescent (light "falls in"
    // from above), bottom-right light crescent (reflects off the back
    // wall). No outer drop shadow — that was making the button read as
    // raised instead of recessed.
    {
        juce::Path clip; clip.addEllipse(bounds);
        juce::Graphics::ScopedSaveState save(g);
        g.reduceClipRegion(clip);

        juce::ColourGradient topShadow(juce::Colour(kInsetShadowTop),
                                        bounds.getX(),     bounds.getY(),
                                        juce::Colour(0x00000000),
                                        bounds.getCentreX(), bounds.getCentreY(),
                                        false);
        g.setGradientFill(topShadow);
        g.fillRect(bounds);

        juce::ColourGradient btmHigh(juce::Colour(0x00FFFFFF),
                                      bounds.getCentreX(), bounds.getCentreY(),
                                      juce::Colour(kInsetHighlight),
                                      bounds.getRight(),   bounds.getBottom(),
                                      false);
        g.setGradientFill(btmHigh);
        g.fillRect(bounds);

        // Sharp dark rim along the top arc — the actual "edge" where the
        // surrounding silver lips over into the well.
        const auto rim = bounds.reduced(0.5f);
        g.setColour(juce::Colour(0x4d000000));
        juce::Path topRim;
        topRim.addCentredArc(rim.getCentreX(), rim.getCentreY(),
                              rim.getWidth() * 0.5f, rim.getHeight() * 0.5f,
                              0.0f,
                              juce::MathConstants<float>::pi * 1.15f,
                              juce::MathConstants<float>::pi * 1.85f, true);
        g.strokePath(topRim, juce::PathStrokeType(1.2f));
    }

    // Outer "lip" — a faint white crescent JUST OUTSIDE the bottom-right
    // of the well. The surrounding silver appears to catch light on its
    // raised edge, reinforcing the recess-into-the-surface illusion.
    {
        const auto outer = bounds.expanded(0.5f);
        juce::Path lip;
        lip.addCentredArc(outer.getCentreX(), outer.getCentreY(),
                           outer.getWidth() * 0.5f, outer.getHeight() * 0.5f,
                           0.0f,
                           juce::MathConstants<float>::pi * 0.10f,
                           juce::MathConstants<float>::pi * 0.85f, true);
        g.setColour(juce::Colour(kOuterRimLight));
        g.strokePath(lip, juce::PathStrokeType(0.8f));
    }

    // Active glow ring stays outside the well — visual marker that the
    // toggle is on (bypass, advanced).
    if (active)
    {
        g.setColour(juce::Colour(kActiveGlow));
        g.drawEllipse(bounds.expanded(1.0f), 1.4f);
    }

    // Icon stroke.
    const auto strokeColour = active ? juce::Colour(kIconActive)
                            : hover  ? juce::Colour(kIconHover)
                                      : juce::Colour(kIconIdle);
    paintIcon(g, bounds, icon, strokeColour);
}

void HeaderButton::mouseEnter(const juce::MouseEvent&)
{
    hover = true;
    repaint();
}

void HeaderButton::mouseExit(const juce::MouseEvent&)
{
    hover = false;
    repaint();
}

void HeaderButton::mouseDown(const juce::MouseEvent&)
{
    if (apvts != nullptr && boundParam.isNotEmpty())
    {
        if (auto* p = apvts->getParameter(boundParam))
        {
            const bool isOn = isActive();
            p->beginChangeGesture();
            p->setValueNotifyingHost(isOn ? 0.0f : 1.0f);
            p->endChangeGesture();
        }
    }
    if (onClick) onClick();
}

} // namespace kaigen::phantom
