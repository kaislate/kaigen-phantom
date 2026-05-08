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

    int best = -1;
    float bestDistSq = kHitTestRadiusPx * kHitTestRadiusPx;
    for (int i = 0; i < kSpokes; ++i)
    {
        const float a = spokeAngleRadians(i);
        // Spoke head sits at outerR (regardless of current value, for hit testing).
        const float hx = centre.x + outerR * std::sin(a);
        const float hy = centre.y - outerR * std::cos(a);
        const float dx = p.x - hx;
        const float dy = p.y - hy;
        const float d2 = dx * dx + dy * dy;
        if (d2 < bestDistSq)
        {
            bestDistSq = d2;
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

    // Background.
    g.setColour(Theme::matrixBg);
    g.fillRect(bounds);

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

    // Inner dead zone.
    g.setColour(Theme::panelBorder);
    g.drawEllipse(centre.x - innerR, centre.y - innerR, innerR * 2.0f, innerR * 2.0f, 1.0f);

    // Spokes.
    for (int i = 0; i < kSpokes; ++i)
    {
        const auto& s = sliders[(size_t) i];
        const float n = (float) s.getNormalisableRange().convertTo0to1(s.getValue());
        const float a = spokeAngleRadians(i);
        const float spokeLength = innerR + (outerR - innerR) * n;

        const float endX = centre.x + spokeLength * std::sin(a);
        const float endY = centre.y - spokeLength * std::cos(a);

        // Spoke line.
        g.setColour(Theme::steelBlue);
        g.drawLine(centre.x, centre.y, endX, endY, 1.5f);

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
}

void RecipeWheel::resized()
{
    // Sliders are hidden; their bounds don't matter.
    for (auto& s : sliders)
        s.setBounds(0, 0, 0, 0);
}

} // namespace kaigen::phantom
