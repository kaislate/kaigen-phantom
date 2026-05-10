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
    constexpr float kArcStartRad     = 4.101524f;          // 235° — symmetric with end at 125°
    constexpr float kArcSweepRad     = 4.363323f;          // 250° sweep, both ends 125° from top
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

    // Shadow padding so the DropShadow halo isn't clipped at component bounds.
    constexpr int kShadowPad = 17;
    const int labelHeight = label.isNotEmpty() ? 11 : 0;
    setSize(kBodySize + kShadowPad * 2, kBodySize + kShadowPad * 2 + labelHeight);
}

bool PhantomMiniKnob::hitTest(int x, int y)
{
    // Body region only; shadow halo is decorative.
    const auto bodyCentreX = (float) getWidth() * 0.5f;
    const auto bodyCentreY = (float) (kBodySize / 2 + 17);  // pad above body
    const float dx = (float) x - bodyCentreX;
    const float dy = (float) y - bodyCentreY;
    const float r = (float) kBodySize * 0.5f;
    return (dx * dx + dy * dy) <= (r * r);
}

PhantomMiniKnob::~PhantomMiniKnob() = default;

juce::String PhantomMiniKnob::formatValue()
{
    return formatKnobValue(param, slider.getValue());
}

void PhantomMiniKnob::paint(juce::Graphics& g)
{
    // Body sits in the upper portion of the bounds (above the label).
    constexpr int kShadowPad = 17;
    auto bodyArea = juce::Rectangle<float>((float) (getWidth() - kBodySize) * 0.5f,
                                            (float) kShadowPad,
                                            (float) kBodySize,
                                            (float) kBodySize);

    const auto centre = bodyArea.getCentre();
    const float radius = bodyArea.getWidth() * 0.5f;
    const float oledR  = radius - kInset;
    const float arcR   = oledR - 3.0f;

    // ── Static layers (body + shadows + OLED bezel + arc track) ────────
    // Cached image, blitted at the body's top-left so it lines up with the
    // bodyArea. The cache is one-time-built shared across all instances.
    const auto& cached = getCachedStaticLayers();
    const int cacheX = (getWidth() - cached.getWidth()) / 2;
    g.drawImageAt(cached, cacheX, 0);

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

        // Bumped from 8.5/11 → 11/13 so advanced row values are legible.
        float fontPx = isDragging ? 13.0f : 11.0f;
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
        // Label sits BELOW the body + shadow padding.
        constexpr int kShadowPadLabel = 17;
        auto labelArea = juce::Rectangle<float>(0.0f,
                                                  (float) (kShadowPadLabel + kBodySize + 2),
                                                  (float) getWidth(),
                                                  11.0f);
        g.drawText(label, labelArea, juce::Justification::centred, false);
    }
}

void PhantomMiniKnob::resized()
{
    slider.setBounds(0, 0, 0, 0);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Static-layer cache (single-size, shared across all instances).
//  Mirrors the body+shadow+OLED+arc-track paint code the per-frame paint()
//  used to do; rendered ONCE at first use.
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
    juce::Image gMiniStaticCache;
}

const juce::Image& PhantomMiniKnob::getCachedStaticLayers()
{
    if (gMiniStaticCache.isValid()) return gMiniStaticCache;

    constexpr int kShadowPad = 17;
    constexpr int imageW = kBodySize + kShadowPad * 2;   // square; height matches.
    gMiniStaticCache = juce::Image(juce::Image::ARGB, imageW, imageW + kShadowPad, true);
    juce::Graphics g(gMiniStaticCache);

    const auto centre = juce::Point<float>((float) imageW * 0.5f,
                                             (float) (kShadowPad + kBodySize / 2));
    const float radius = (float) kBodySize * 0.5f;
    const float oledR  = radius - kInset;
    const float arcR   = oledR - 3.0f;

    // Body — DropShadows + transparent radial gradient.
    {
        juce::Path circle;
        circle.addEllipse(juce::Rectangle<float>(radius * 2, radius * 2).withCentre(centre));

        juce::DropShadow brB { juce::Colour(0x24000000), 17, juce::Point<int>(3, 5) };
        brB.drawForPath(g, circle);
        juce::DropShadow brA { juce::Colour(0x4D000000), 10, juce::Point<int>(2, 3) };
        brA.drawForPath(g, circle);
        juce::DropShadow tlB { juce::Colour(0x4DFFFFFF), 17, juce::Point<int>(-3, -5) };
        tlB.drawForPath(g, circle);
        juce::DropShadow tlA { juce::Colour(0xa8FFFFFF), 10, juce::Point<int>(-2, -3) };
        tlA.drawForPath(g, circle);

        const juce::Point<float> gradOrigin {
            centre.x - radius * 0.30f,
            centre.y - radius * 0.40f
        };
        juce::ColourGradient body(juce::Colour(0x3DFFFFFF), gradOrigin,
                                    juce::Colour(0x12000000),
                                    { centre.x + radius, centre.y + radius },
                                    true);
        body.addColour(0.22, juce::Colour(0x1FFFFFFF));
        body.addColour(0.60, juce::Colour(0x05000000));
        g.setGradientFill(body);
        g.fillEllipse(juce::Rectangle<float>(radius * 2, radius * 2).withCentre(centre));
    }

    // OLED bezel.
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

    // Arc track.
    {
        juce::Path track;
        track.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f,
                             kArcStartRad, kArcStartRad + kArcSweepRad, true);
        g.setColour(juce::Colour(0x1AFFFFFF));
        g.strokePath(track, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::butt));
    }

    return gMiniStaticCache;
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
