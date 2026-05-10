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

    // Pick type-colors.
    juce::Colour fillColour;
    bool isPlaceholder = (type == Type::Lfo || type == Type::Random) && hiddenSlider == nullptr;
    switch (type)
    {
        case Type::Macro:  fillColour = Theme::accentBlue;    break;   // steel blue
        case Type::Morph:  fillColour = Theme::morphWhite;    break;
        case Type::Lfo:    fillColour = Theme::lfoBlue;       break;
        case Type::Random: fillColour = Theme::randomPurple;  break;
    }

    // ── Macros + Morph: arc-only with center (or beside) label ──────────
    if ((type == Type::Macro || type == Type::Morph) && hiddenSlider != nullptr)
    {
        // Morph: arc on the LEFT, label "Morph" to the right of the arc.
        // Macro: arc fills the slot, label "m1".."m4" centered inside the arc.
        juce::Rectangle<int> arcArea;
        if (type == Type::Morph)
            arcArea = bounds.removeFromLeft(juce::jmin(bounds.getHeight(), 36));
        else
            arcArea = bounds;

        const float arcDiameter = (float) juce::jmin(arcArea.getWidth(), arcArea.getHeight()) - 4.0f;
        const auto  arcCentre   = arcArea.getCentre().toFloat();
        const float arcR        = arcDiameter * 0.5f;
        const float arcThick    = 2.5f;

        // Arc track — visible even at value 0 so the slot reads as a control.
        // Uses a tinted version of the fill colour at low alpha against the
        // silver background.
        const float startA = juce::degreesToRadians(235.0f);   // matches PhantomKnob
        const float sweep  = juce::degreesToRadians(250.0f);
        juce::Path bgArc;
        bgArc.addCentredArc(arcCentre.x, arcCentre.y, arcR, arcR, 0.0f,
                             startA, startA + sweep, true);
        g.setColour(fillColour.withAlpha(0.22f));   // tinted track, always visible
        g.strokePath(bgArc, juce::PathStrokeType(arcThick + 1.0f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Active arc — type-color, scaled by slider value.
        const float val = (float) hiddenSlider->getValue();
        if (val > 0.001f)
        {
            juce::Path fgArc;
            fgArc.addCentredArc(arcCentre.x, arcCentre.y, arcR, arcR, 0.0f,
                                 startA, startA + sweep * val, true);
            g.setColour(fillColour);
            g.strokePath(fgArc, juce::PathStrokeType(arcThick,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Label — etched (dark text + 1 px white shadow below) for the
        // engraved-into-silver look matching the section headings.
        if (type == Type::Macro)
        {
            const auto font = juce::Font(juce::FontOptions("Space Grotesk", 12.0f, juce::Font::bold));
            // White shadow below.
            g.setFont(font);
            g.setColour(juce::Colour(0x80FFFFFF));
            g.drawText(label, arcArea.translated(0, 1), juce::Justification::centred, false);
            // Foreground — slightly brighter than 70% black per request.
            g.setColour(juce::Colour(0xc8000000));   // ~78% black
            g.drawText(label, arcArea, juce::Justification::centred, false);
        }
        else   // Morph
        {
            const auto font = juce::Font(juce::FontOptions("Space Grotesk", 11.0f, juce::Font::italic));
            g.setFont(font);
            g.setColour(juce::Colour(0x80FFFFFF));
            g.drawText(label, bounds.translated(0, 1), juce::Justification::centredLeft, false);
            g.setColour(juce::Colour(0xc8000000));
            g.drawText(label, bounds, juce::Justification::centredLeft, false);
        }
        return;
    }

    // ── LFO/Random placeholders (only in expanded mode) ────────────────
    auto labelArea = bounds.toFloat().removeFromBottom(14.0f);
    auto dotArea   = bounds.reduced(2);
    const float dotDiameter = juce::jmin(28.0f, (float) juce::jmin(dotArea.getWidth(), dotArea.getHeight() - 2));
    const auto dotCentre = juce::Point<float> { (float) dotArea.getCentreX(), (float) dotArea.getCentreY() };
    const auto dotRect = juce::Rectangle<float>(dotDiameter, dotDiameter).withCentre(dotCentre);

    g.setColour(fillColour.withAlpha(0.40f));
    g.fillEllipse(dotRect);
    g.setColour(fillColour.withAlpha(0.70f));
    g.drawEllipse(dotRect, 2.0f);
    if (placeholder.isNotEmpty())
    {
        g.setColour(juce::Colour(0x66ffffff));
        g.setFont(juce::FontOptions("Space Grotesk", 7.0f, juce::Font::bold));
        g.drawText(placeholder, dotRect, juce::Justification::centred, false);
    }
    g.setFont(juce::FontOptions("Space Grotesk", 9.0f, juce::Font::bold));
    g.setColour(fillColour.withAlpha(0.65f));
    g.drawText(label, labelArea.toNearestInt(), juce::Justification::centred, false);
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
