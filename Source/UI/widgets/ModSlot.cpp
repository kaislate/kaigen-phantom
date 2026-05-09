// Source/UI/widgets/ModSlot.cpp
#include "ModSlot.h"
#include "../Theme.h"

namespace kaigen::phantom
{

ModSlot::ModSlot(juce::AudioProcessorValueTreeState& apvts,
                 Type t,
                 const juce::String& sId,
                 const juce::String& paramID,
                 const juce::String& lbl,
                 const juce::String& ph)
    : type(t), slotId(sId), label(lbl), placeholder(ph)
{
    if ((type == Type::Macro || type == Type::Morph) && paramID.isNotEmpty())
    {
        if (auto* p = apvts.getParameter(paramID))
        {
            hiddenSlider = std::make_unique<juce::Slider>();
            hiddenSlider->setRange(0.0, 1.0);
            attachment = std::make_unique<juce::SliderParameterAttachment>(*p, *hiddenSlider, nullptr);
            hiddenSlider->onValueChange = [this] { repaint(); };
        }
    }
}

ModSlot::~ModSlot() = default;

void ModSlot::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    auto labelArea = bounds.removeFromBottom(14);
    auto dotArea   = bounds.reduced(2);

    // Pick type-colors.
    juce::Colour fillColour, borderColour, glowColour;
    bool hasValueRing = false;
    bool isPlaceholder = (type == Type::Lfo || type == Type::Random) && hiddenSlider == nullptr;
    switch (type)
    {
        case Type::Macro:
            fillColour   = Theme::macroTeal;
            borderColour = Theme::macroTeal;
            glowColour   = Theme::macroTeal.withAlpha(0.5f);
            hasValueRing = true;
            break;
        case Type::Morph:
            fillColour   = Theme::morphWhite;
            borderColour = Theme::morphWhite;
            glowColour   = Theme::morphWhite.withAlpha(0.5f);
            hasValueRing = true;
            break;
        case Type::Lfo:
            fillColour   = Theme::lfoBlue;
            borderColour = Theme::lfoBlue;
            glowColour   = Theme::lfoBlue.withAlpha(0.5f);
            break;
        case Type::Random:
            fillColour   = Theme::randomPurple;
            borderColour = Theme::randomPurple;
            glowColour   = Theme::randomPurple.withAlpha(0.5f);
            break;
    }

    // Fit a 36x36 dot centered in the dot area (or smaller if bounds are tight).
    const float dotDiameter = juce::jmin(36.0f, (float) juce::jmin(dotArea.getWidth(), dotArea.getHeight() - 2));
    const auto dotCentre = juce::Point<float> { (float) dotArea.getCentreX(), (float) dotArea.getCentreY() };
    const auto dotRect = juce::Rectangle<float>(dotDiameter, dotDiameter).withCentre(dotCentre);

    // Optional value ring (macro/morph) — draw BEHIND the dot.
    if (hasValueRing && hiddenSlider != nullptr)
    {
        const float ringRadius    = dotDiameter * 0.5f + 4.0f;
        const float ringThickness = 2.0f;

        // Background track (faint white).
        const float ringStartAngle = -juce::MathConstants<float>::pi * 0.75f;   // ~ -135°
        const float ringEndAngle   =  juce::MathConstants<float>::pi * 0.75f;   // ~ +135°  (270° sweep)
        juce::Path bgArc;
        bgArc.addCentredArc(dotCentre.x, dotCentre.y, ringRadius, ringRadius, 0.0f,
                             ringStartAngle, ringEndAngle, true);
        g.setColour(Theme::ringTrack);
        g.strokePath(bgArc, juce::PathStrokeType(ringThickness));

        // Foreground active arc.
        const float val       = (float) hiddenSlider->getValue();   // 0..1
        const float fgEndAngle = ringStartAngle + val * (ringEndAngle - ringStartAngle);
        juce::Path fgArc;
        fgArc.addCentredArc(dotCentre.x, dotCentre.y, ringRadius, ringRadius, 0.0f,
                             ringStartAngle, fgEndAngle, true);
        g.setColour(fillColour);
        g.strokePath(fgArc, juce::PathStrokeType(ringThickness));
    }

    if (isPlaceholder)
    {
        // Faint dot for LFO/Random placeholders (PR4/PR5).
        g.setColour(fillColour.withAlpha(0.40f));
        g.fillEllipse(dotRect);
        g.setColour(borderColour.withAlpha(0.70f));
        g.drawEllipse(dotRect, 2.0f);
        // Stub label inside the dot.
        if (placeholder.isNotEmpty())
        {
            g.setColour(juce::Colour(0x66ffffff));
            g.setFont(juce::FontOptions("Space Grotesk", 7.0f, juce::Font::bold));
            g.drawText(placeholder, dotRect, juce::Justification::centred, false);
        }
    }
    else
    {
        // Active radial gradient body — type-color at center, dark slot-dot inner at edge.
        juce::ColourGradient body(fillColour, dotCentre,
                                   Theme::slotDotInner,
                                   juce::Point<float>(dotCentre.x + dotDiameter * 0.5f,
                                                       dotCentre.y + dotDiameter * 0.5f),
                                   true);
        body.addColour(0.30, fillColour);       // sharp edge at 30%
        body.addColour(0.70, Theme::slotDotInner);
        g.setGradientFill(body);
        g.fillEllipse(dotRect);

        // Border ring + glow.
        g.setColour(glowColour);
        g.drawEllipse(dotRect.expanded(1.0f), 2.0f);  // outer glow
        g.setColour(borderColour);
        g.drawEllipse(dotRect, 2.0f);
    }

    // Label below the dot.
    g.setFont(juce::FontOptions("Space Grotesk", 9.0f, juce::Font::bold));
    g.setColour(fillColour.withAlpha(isPlaceholder ? 0.65f : 1.0f));
    g.drawText(label, labelArea, juce::Justification::centred, false);
}

void ModSlot::resized()
{
    // No children to lay out — hiddenSlider is not added as a visible component.
}

void ModSlot::mouseDown(const juce::MouseEvent& e)
{
    if (onSlotClicked) onSlotClicked(slotId);   // existing callback — preserve it

    if (hiddenSlider != nullptr)
    {
        dragArmed      = true;
        dragStartValue = (float) hiddenSlider->getValue();
        dragStartY     = e.y;
    }
}

void ModSlot::mouseDrag(const juce::MouseEvent& e)
{
    if (! dragArmed || hiddenSlider == nullptr) return;
    // 100 px vertical = full 0..1 range.
    const int   dy       = dragStartY - e.y;   // up = positive
    const float newValue = juce::jlimit(0.0f, 1.0f, dragStartValue + (float) dy / 100.0f);
    hiddenSlider->setValue(newValue, juce::sendNotificationSync);
}

} // namespace kaigen::phantom
