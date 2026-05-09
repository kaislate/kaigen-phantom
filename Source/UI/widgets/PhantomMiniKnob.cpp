// Source/UI/widgets/PhantomMiniKnob.cpp
#include "PhantomMiniKnob.h"
#include "KnobValueFormat.h"
#include "../Theme.h"

namespace kaigen::phantom
{

namespace
{
    constexpr int   kBodySize        = 48;   // diameter of the knob body (was 36 — too small)
    constexpr float kInset           = 3.0f;
    constexpr float kArcStartRad     = 2.356194f;          // 135°
    constexpr float kArcSweepRad     = 4.712389f;          // 270°
}

PhantomMiniKnob::PhantomMiniKnob(juce::AudioProcessorValueTreeState& apvts,
                                  juce::StringRef paramID,
                                  const juce::String& lbl)
    : label(lbl)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    slider.onValueChange = [this] { repaint(); };
    addChildComponent(slider);  // hidden — we paint everything ourselves
    slider.setBounds(0, 0, 0, 0);

    if (auto* p = apvts.getParameter(paramID))
    {
        param = p;
        attachment = std::make_unique<juce::SliderParameterAttachment>(*p, slider);
        defaultNorm = p->getDefaultValue();
    }
    else
    {
        jassertfalse;  // unknown paramID
    }

    const int labelHeight = label.isNotEmpty() ? 11 : 0;
    setSize(kBodySize + 6, kBodySize + labelHeight + 2);
}

PhantomMiniKnob::~PhantomMiniKnob() = default;

juce::String PhantomMiniKnob::formatValue()
{
    return formatKnobValue(param, slider.getValue());
}

void PhantomMiniKnob::paint(juce::Graphics& g)
{
    auto bodyArea = juce::Rectangle<float>((float) (getWidth() - kBodySize) * 0.5f,
                                            2.0f,
                                            (float) kBodySize,
                                            (float) kBodySize);

    const auto centre = bodyArea.getCentre();
    const float radius = bodyArea.getWidth() * 0.5f;
    const float oledR  = radius - kInset;
    const float arcR   = oledR - 3.0f;

    // ── Layer 1: neumorphic raised body — radial gradient only (no blur shadow) ─
    {
        // No DropShadow — JUCE's software blur fringes cyan on Windows.
        // Radial gradient alone provides the convex depth illusion.

        // Soft halo just outside the body (clipped at component bounds).
        {
            const float haloR = radius * 1.15f;
            juce::ColourGradient halo(
                juce::Colour(0x66FFFFFF),
                centre.x - radius * 0.20f, centre.y - radius * 0.30f,
                juce::Colour(0x00FFFFFF),
                centre.x + haloR, centre.y + haloR,
                true);
            g.setGradientFill(halo);
            g.fillEllipse(juce::Rectangle<float>(haloR * 2, haloR * 2).withCentre(centre));
        }

        // Radial gradient body — peak off-centre top-left, fades to panel-tone
        // at the boundary. Same recipe as PhantomKnob.
        const juce::Point<float> gradOrigin {
            centre.x - radius * 0.30f,
            centre.y - radius * 0.40f
        };
        juce::ColourGradient body(juce::Colour(0xffE2E3E5), gradOrigin,
                                    juce::Colour(0xffA9AAAC),
                                    { centre.x + radius, centre.y + radius },
                                    true);
        body.addColour(0.30, juce::Colour(0xffCFD0D2));
        body.addColour(0.65, juce::Colour(0xffB1B2B4));
        g.setGradientFill(body);
        g.fillEllipse(juce::Rectangle<float>(radius * 2, radius * 2).withCentre(centre));
    }

    // ── Layer 2: OLED well + bezel rings ───────────────────────────────────
    {
        const auto rect = juce::Rectangle<float>(oledR * 2, oledR * 2).withCentre(centre);
        g.setColour(juce::Colours::black);
        g.fillEllipse(rect);

        g.setColour(juce::Colour(0x47FFFFFF));
        g.drawEllipse(rect.reduced(0.5f), 0.75f);

        g.setColour(juce::Colour(0xeb000000));
        g.drawEllipse(rect.expanded(0.75f), 0.75f);

        g.setColour(juce::Colour(0x1aFFFFFF));
        g.drawEllipse(rect.expanded(1.5f), 0.5f);
    }

    // ── Layer 3: arc track ─────────────────────────────────────────────────
    {
        juce::Path track;
        track.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f,
                             kArcStartRad, kArcStartRad + kArcSweepRad, true);
        g.setColour(juce::Colour(0x0fFFFFFF));
        g.strokePath(track, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::butt));
    }

    // ── Layers 4-5: indicator arc (glow halo + sharp white) ────────────────
    {
        const float normVal = (float) slider.getNormalisableRange().convertTo0to1(slider.getValue());
        if (normVal > 0.001f)
        {
            juce::Path arc;
            arc.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f,
                               kArcStartRad, kArcStartRad + kArcSweepRad * normVal, true);

            // Glow — softer alpha to avoid ClearType cyan fringing on Windows.
            g.setColour(juce::Colour(0x40FFFFFF));
            g.strokePath(arc, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));

            // Sharp.
            g.setColour(juce::Colours::white);
            g.strokePath(arc, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }
    }

    // ── Layer 6: value text (3-pass, shrink to fit) ────────────────────────
    {
        const auto text = formatValue();
        const float maxW = oledR * 1.7f;

        float fontPx = isDragging ? 11.0f : 8.5f;
        juce::Font font(juce::FontOptions("Courier New", fontPx, juce::Font::bold));
        while (fontPx > 5.0f && (float) juce::GlyphArrangement::getStringWidthInt(font, text) > maxW)
        {
            fontPx -= 0.5f;
            font = juce::Font(juce::FontOptions("Courier New", fontPx, juce::Font::bold));
        }
        g.setFont(font);

        const float textH = fontPx * 1.4f;
        const juce::Rectangle<float> rect(centre.x - maxW * 0.5f,
                                           centre.y - 2.0f - textH * 0.5f,
                                           maxW, textH);

        g.setColour(juce::Colour(0x4DFFFFFF));
        g.drawText(text, rect.translated(0.0f, 1.0f), juce::Justification::centred, false);
        g.setColour(juce::Colour(0x99FFFFFF));
        g.drawText(text, rect, juce::Justification::centred, false);
        g.setColour(juce::Colours::white);
        g.drawText(text, rect, juce::Justification::centred, false);
    }

    // ── External label below the knob body ─────────────────────────────────
    if (label.isNotEmpty())
    {
        g.setColour(Theme::textOnLightLabel);
        g.setFont(juce::FontOptions("Space Grotesk", 8.0f, juce::Font::plain));
        auto labelArea = juce::Rectangle<float>(0.0f,
                                                  (float) (kBodySize + 2),
                                                  (float) getWidth(),
                                                  11.0f);
        g.drawText(label, labelArea, juce::Justification::centred, false);
    }
}

void PhantomMiniKnob::resized()
{
    slider.setBounds(0, 0, 0, 0);
}

void PhantomMiniKnob::mouseDown(const juce::MouseEvent& e)
{
    isDragging     = true;
    dragStartY     = e.y;
    dragStartNorm  = (float) slider.getNormalisableRange().convertTo0to1(slider.getValue());
    slider.startedDragging();
    repaint();
}

void PhantomMiniKnob::mouseDrag(const juce::MouseEvent& e)
{
    if (! isDragging) return;
    const float pxPerUnit = 200.0f;
    const float dy = (float)(dragStartY - e.y);
    const float newNorm = juce::jlimit(0.0f, 1.0f, dragStartNorm + dy / pxPerUnit);
    const float newVal  = (float) slider.getNormalisableRange().convertFrom0to1(newNorm);
    slider.setValue(newVal, juce::sendNotificationSync);
}

void PhantomMiniKnob::mouseUp(const juce::MouseEvent&)
{
    if (isDragging) slider.stoppedDragging();
    isDragging = false;
    repaint();
}

void PhantomMiniKnob::mouseDoubleClick(const juce::MouseEvent&)
{
    const float defaultVal = (float) slider.getNormalisableRange().convertFrom0to1(defaultNorm);
    slider.setValue(defaultVal, juce::sendNotificationSync);
}

} // namespace kaigen::phantom
