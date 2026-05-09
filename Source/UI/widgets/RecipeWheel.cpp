// Source/UI/widgets/RecipeWheel.cpp
#include "RecipeWheel.h"
#include "../Theme.h"

namespace kaigen::phantom
{

namespace
{
    /** Spoke angle for harmonic index 0..6.
     *  Angle 0 = straight up. Spokes evenly distributed clockwise. */
    float spokeAngleRadians(int i)
    {
        return (float) i * juce::MathConstants<float>::twoPi / (float) RecipeWheel::kSpokes;
    }

    /** Outer radius of the wheel as a fraction of the smaller bounds dimension. */
    constexpr float kOuterRadiusFrac = 0.42f;
    constexpr float kInnerRadiusFrac = 0.10f;  // dead zone at center
    constexpr float kSpokeHeadRadius = 6.0f;
    constexpr float kHitTestRadiusPx = 14.0f;  // generous hit area for spoke heads
}

RecipeWheel::RecipeWheel(juce::AudioProcessorValueTreeState& apvts,
                         const std::array<juce::String, kSpokes>& paramIDs)
{
    for (int i = 0; i < kSpokes; ++i)
    {
        auto& s = sliders[(size_t) i];
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        s.addListener(this);
        addChildComponent(s);  // hidden -- we forward via mouse handlers

        if (auto* param = apvts.getParameter(paramIDs[(size_t) i]))
        {
            params[(size_t) i] = param;
            attachments[(size_t) i] = std::make_unique<juce::SliderParameterAttachment>(*param, s);
        }
        else
        {
            jassertfalse;  // unknown paramID: typo or stale reference
        }
    }
}

RecipeWheel::~RecipeWheel()
{
    for (auto& s : sliders) s.removeListener(this);
}

void RecipeWheel::sliderValueChanged(juce::Slider*)
{
    repaint();
}

int RecipeWheel::hitTestSpoke(juce::Point<float> p) const
{
    const auto bounds = getLocalBounds().toFloat();
    const auto centre = bounds.getCentre();
    const float r = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float outerR = r * kOuterRadiusFrac;
    const float innerR = r * kInnerRadiusFrac;

    const float dx = p.x - centre.x;
    const float dy = p.y - centre.y;
    const float dist = std::sqrt(dx * dx + dy * dy);

    // Click must be in the donut between inner and outer radii (with a small
    // margin past the outer ring to make the hit area generous).
    constexpr float kHitMarginPx = 14.0f;
    if (dist < innerR) return -1;
    if (dist > outerR + kHitMarginPx) return -1;

    // Angle from center, with 0 = up (matching spokeAngleRadians).
    // atan2(dx, -dy) gives angle clockwise from straight up: 0 at top,
    // PI/2 at right, PI at bottom, -PI/2 at left.
    float clickAngle = std::atan2(dx, -dy);
    if (clickAngle < 0.0f) clickAngle += juce::MathConstants<float>::twoPi;

    // Each spoke owns an angular slice of width (2*PI / kSpokes). Find the
    // spoke whose angle is closest.
    const float spokeStep = juce::MathConstants<float>::twoPi / (float) kSpokes;
    int best = -1;
    float bestDelta = spokeStep * 0.5f;  // half a slice — angular tolerance
    for (int i = 0; i < kSpokes; ++i)
    {
        const float a = spokeAngleRadians(i);  // 0..2PI
        // Angular distance, wrapped to [0, PI]
        float delta = std::abs(clickAngle - a);
        if (delta > juce::MathConstants<float>::pi)
            delta = juce::MathConstants<float>::twoPi - delta;
        if (delta < bestDelta)
        {
            bestDelta = delta;
            best = i;
        }
    }
    return best;
}

float RecipeWheel::pointToValue(juce::Point<float> p) const
{
    const auto bounds = getLocalBounds().toFloat();
    const auto centre = bounds.getCentre();
    const float r = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float outerR = r * kOuterRadiusFrac;
    const float innerR = r * kInnerRadiusFrac;

    const float dx = p.x - centre.x;
    const float dy = p.y - centre.y;
    const float dist = std::sqrt(dx * dx + dy * dy);

    if (dist <= innerR) return 0.0f;
    if (dist >= outerR) return 1.0f;
    return (dist - innerR) / (outerR - innerR);
}

void RecipeWheel::mouseDown(const juce::MouseEvent& e)
{
    activeSpoke = hitTestSpoke(e.position);
    if (activeSpoke < 0) return;

    auto& s = sliders[(size_t) activeSpoke];
    if (auto* p = params[(size_t) activeSpoke])
        p->beginChangeGesture();
    const float n = pointToValue(e.position);
    const auto value = s.getNormalisableRange().convertFrom0to1(n);
    s.setValue(value, juce::sendNotificationSync);
}

void RecipeWheel::mouseDrag(const juce::MouseEvent& e)
{
    if (activeSpoke < 0) return;
    auto& s = sliders[(size_t) activeSpoke];
    const float n = pointToValue(e.position);
    const auto value = s.getNormalisableRange().convertFrom0to1(n);
    s.setValue(value, juce::sendNotificationSync);
}

void RecipeWheel::mouseUp(const juce::MouseEvent&)
{
    if (activeSpoke >= 0)
    {
        if (auto* p = params[(size_t) activeSpoke])
            p->endChangeGesture();
        activeSpoke = -1;
        repaint();
    }
}

void RecipeWheel::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto centre = bounds.getCentre();
    const float r = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float outerR = r * kOuterRadiusFrac;
    const float innerR = r * kInnerRadiusFrac;

    // Background — fill component area with editor bg.
    g.setColour(Theme::matrixBg);
    g.fillRect(bounds);

    // ── Silver convex mount disc ──────────────────────────────────────────
    // CSS: radial-gradient(ellipse at 35% 30%, rgba(255,255,255,0.24)→...→rgba(0,0,0,0.07))
    // wheelRadius here maps to the outer spoke radius so the silver disc
    // frames the full spoke area with some breathing room.
    const float wheelRadius = outerR * (1.0f / kOuterRadiusFrac) * 0.45f;
    const juce::Point<float> gradOrigin {
        centre.x - wheelRadius * 0.30f,
        centre.y - wheelRadius * 0.40f
    };
    juce::ColourGradient mount(juce::Colour(0x3DFFFFFF), gradOrigin,
                                juce::Colour(0x12000000),
                                { centre.x + wheelRadius, centre.y + wheelRadius },
                                true);
    mount.addColour(0.20, juce::Colour(0x1EFFFFFF));
    mount.addColour(0.55, juce::Colour(0x05000000));
    g.setGradientFill(mount);
    g.fillEllipse(juce::Rectangle<float>(wheelRadius * 2.0f, wheelRadius * 2.0f).withCentre(centre));

    // Outer offset shadows — bottom-right shadow + top-left highlight.
    {
        const auto sb = juce::Rectangle<float>(wheelRadius * 2.0f, wheelRadius * 2.0f)
                             .withCentre({ centre.x + 2.0f, centre.y + 2.0f });
        juce::DropShadow shadow { juce::Colour(0x38000000), 14, juce::Point<int>(2, 2) };
        juce::Path circlePath; circlePath.addEllipse(sb);
        shadow.drawForPath(g, circlePath);
    }
    {
        const auto hb = juce::Rectangle<float>(wheelRadius * 2.0f, wheelRadius * 2.0f)
                             .withCentre({ centre.x - 2.0f, centre.y - 2.0f });
        juce::DropShadow hl { juce::Colour(0x80FFFFFF), 10, juce::Point<int>(-2, -2) };
        juce::Path circlePath; circlePath.addEllipse(hb);
        hl.drawForPath(g, circlePath);
    }

    // Inset rim — thin dark ring at the disc edge.
    g.setColour(juce::Colour(0x12000000));
    g.drawEllipse(juce::Rectangle<float>(wheelRadius * 2.0f, wheelRadius * 2.0f)
                       .withCentre(centre).reduced(0.5f), 1.5f);

    // ── Spoke area gridlines (on top of silver mount) ─────────────────────
    // Outer ring (faint).
    g.setColour(Theme::panelBorder);
    g.drawEllipse(centre.x - outerR, centre.y - outerR, outerR * 2.0f, outerR * 2.0f, 1.0f);

    // 25%, 50%, 75% radius gridlines (very faint).
    for (float frac : { 0.25f, 0.5f, 0.75f })
    {
        const float gr = innerR + (outerR - innerR) * frac;
        g.setColour(Theme::panelBorder.withAlpha(0.4f));
        g.drawEllipse(centre.x - gr, centre.y - gr, gr * 2.0f, gr * 2.0f, 0.6f);
    }

    // Hub radius shared by spoke origin clipping and OLED hub chrome.
    const float hubRadius = wheelRadius * 0.40f;

    // ── Spokes ────────────────────────────────────────────────────────────
    for (int i = 0; i < kSpokes; ++i)
    {
        const auto& s = sliders[(size_t) i];
        const float n = (float) s.getNormalisableRange().convertTo0to1(s.getValue());
        const float a = spokeAngleRadians(i);
        const float spokeLength = innerR + (outerR - innerR) * n;

        const float endX = centre.x + spokeLength * std::sin(a);
        const float endY = centre.y - spokeLength * std::cos(a);

        // Spoke line starts at hub edge so it doesn't cross the OLED body.
        const float startX = centre.x + hubRadius * std::sin(a);
        const float startY = centre.y - hubRadius * std::cos(a);
        g.setColour(Theme::steelBlue);
        g.drawLine(startX, startY, endX, endY, 1.5f);

        // Spoke head circle (filled at value's tip).
        g.setColour((i == activeSpoke) ? Theme::macroTeal : Theme::steelBlue);
        g.fillEllipse(endX - kSpokeHeadRadius, endY - kSpokeHeadRadius,
                      kSpokeHeadRadius * 2.0f, kSpokeHeadRadius * 2.0f);

        // Hint marker at outer ring (for hit-test reference).
        const float hintX = centre.x + outerR * std::sin(a);
        const float hintY = centre.y - outerR * std::cos(a);
        g.setColour(Theme::textDim);
        g.fillEllipse(hintX - 2.0f, hintY - 2.0f, 4.0f, 4.0f);
    }

    // ── OLED center hub ───────────────────────────────────────────────────
    const auto hubBounds = juce::Rectangle<float>(hubRadius * 2.0f, hubRadius * 2.0f)
                                .withCentre(centre);

    // Outer subtle drop-shadow ring (separates the hub from the silver mount).
    {
        juce::DropShadow hubShadow { juce::Colour(0x66000000), 6, juce::Point<int>(0, 1) };
        juce::Path circlePath; circlePath.addEllipse(hubBounds.expanded(2.0f));
        hubShadow.drawForPath(g, circlePath);
    }

    // Concentric bezel layers — outer dark gap, silver bezel, inner dark gap, hub body.
    g.setColour(juce::Colour(0x33000000));                   // outer dark gap (CSS: 0 0 0 5px rgba(0,0,0,0.18))
    g.fillEllipse(hubBounds.expanded(5.0f));

    // Silver bezel ring — full opaque so it reads clearly.
    juce::ColourGradient bezelGrad(juce::Colour(0xffD8DADC),
                                     hubBounds.getCentreX() - hubRadius,
                                     hubBounds.getCentreY() - hubRadius,
                                     juce::Colour(0xff848688),
                                     hubBounds.getCentreX() + hubRadius,
                                     hubBounds.getCentreY() + hubRadius,
                                     false);
    g.setGradientFill(bezelGrad);
    g.fillEllipse(hubBounds.expanded(3.5f));

    // Inner dark gap (CSS: 0 0 0 2px rgba(0,0,0,0.55)).
    g.setColour(juce::Colour(0x8C000000));
    g.fillEllipse(hubBounds.expanded(2.0f));

    // Black hub body.
    g.setColour(juce::Colour(0xff000000));
    g.fillEllipse(hubBounds);

    // Hard inset shadow on top + left of hub body for the OLED depth illusion.
    g.setColour(juce::Colour(0xCC000000));
    g.drawEllipse(hubBounds.reduced(1.0f), 2.5f);
}

void RecipeWheel::resized()
{
    // Sliders are hidden; their bounds don't matter.
    for (auto& s : sliders)
        s.setBounds(0, 0, 0, 0);
}

} // namespace kaigen::phantom
