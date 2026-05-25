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
                                  const juce::String& lbl,
                                  bool darkBackground)
    : apvtsRef(&apvts), label(lbl), darkStyle(darkBackground)
{
    const auto idStr = juce::String(paramID);
    if (idStr.startsWith("a_") || idStr.startsWith("b_"))
    {
        enginePrefix = idStr.substring(0, 2);
        leafName     = idStr.substring(2);
    }
    else
    {
        leafName = idStr;
    }

    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    slider.onValueChange = [this] { repaint(); };
    addChildComponent(slider);  // hidden — we paint everything ourselves
    slider.setBounds(0, 0, 0, 0);
    slider.addListener(this);   // LINK mirror

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

PhantomMiniKnob::~PhantomMiniKnob()
{
    slider.removeListener(this);
}

void PhantomMiniKnob::setEnginePrefix(const juce::String& activePrefix,
                                        const juce::String& newMirrorPrefix)
{
    if (! isPerEngine() || apvtsRef == nullptr) return;
    if (activePrefix == enginePrefix && newMirrorPrefix == mirrorPrefix) return;

    enginePrefix = activePrefix;
    mirrorPrefix = newMirrorPrefix;

    attachment.reset();
    const auto fullId = enginePrefix + leafName;
    if (auto* p = apvtsRef->getParameter(fullId))
    {
        param = p;
        attachment = std::make_unique<juce::SliderParameterAttachment>(*p, slider);
        defaultNorm = p->getDefaultValue();
    }
    else
    {
        jassertfalse;
    }
    repaint();
}

void PhantomMiniKnob::sliderValueChanged(juce::Slider*)
{
    if (mirrorPrefix.isEmpty() || isMirroring || apvtsRef == nullptr) return;
    auto* otherParam = apvtsRef->getParameter(mirrorPrefix + leafName);
    if (otherParam == nullptr || param == nullptr) return;

    juce::ScopedValueSetter<bool> guard(isMirroring, true);
    const auto& srcRange = param->getNormalisableRange();
    const float norm = srcRange.convertTo0to1((float) slider.getValue());
    otherParam->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, norm));
}

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
    // bodyArea. Two caches (light + dark) shared across all instances.
    const auto& cached = getCachedStaticLayers(darkStyle);
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
        // Light text on dark backgrounds (sampler section), dark text on
        // light backgrounds (main phantom panel).
        g.setColour(darkStyle ? juce::Colour(0xb0d0d2d4) : Theme::textOnLightLabel);
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
    juce::Image gMiniStaticCache;       // light-bg variant (default)
    juce::Image gMiniStaticCacheDark;   // dark-bg variant (sampler strip etc.)
}

const juce::Image& PhantomMiniKnob::getCachedStaticLayers(bool darkBackground)
{
    juce::Image& cache = darkBackground ? gMiniStaticCacheDark : gMiniStaticCache;
    if (cache.isValid()) return cache;

    constexpr int kShadowPad = 17;
    constexpr int imageW = kBodySize + kShadowPad * 2;   // square; height matches.
    cache = juce::Image(juce::Image::ARGB, imageW, imageW + kShadowPad, true);
    juce::Graphics g(cache);

    const auto centre = juce::Point<float>((float) imageW * 0.5f,
                                             (float) (kShadowPad + kBodySize / 2));
    const float radius = (float) kBodySize * 0.5f;
    const float oledR  = radius - kInset;
    const float arcR   = oledR - 3.0f;

    // Body — DropShadows + radial gradient. The light variant uses bright
    // white halos and a white-tinted body for the silver-on-grey phantom
    // look; the dark variant drops the halos and uses a subtle dark
    // gradient so the knob recedes into a dark surface.
    {
        juce::Path circle;
        circle.addEllipse(juce::Rectangle<float>(radius * 2, radius * 2).withCentre(centre));

        // Black drop shadow for depth — softer on dark bg.
        juce::DropShadow brB { juce::Colour(darkBackground ? 0x14000000 : 0x24000000),
                                17, juce::Point<int>(3, 5) };
        brB.drawForPath(g, circle);
        juce::DropShadow brA { juce::Colour(darkBackground ? 0x2D000000 : 0x4D000000),
                                10, juce::Point<int>(2, 3) };
        brA.drawForPath(g, circle);

        // White top-left halo: phantom-light look. Skipped entirely on
        // dark bg (it's the source of the "phantom white" glow the user
        // wanted gone from the sampler section).
        if (! darkBackground)
        {
            juce::DropShadow tlB { juce::Colour(0x4DFFFFFF), 17, juce::Point<int>(-3, -5) };
            tlB.drawForPath(g, circle);
            juce::DropShadow tlA { juce::Colour(0xa8FFFFFF), 10, juce::Point<int>(-2, -3) };
            tlA.drawForPath(g, circle);
        }

        // Body fill — light variant uses a bright-to-shadow radial; dark
        // variant uses a near-flat dark grey with a subtle inset feel.
        if (darkBackground)
        {
            const juce::Point<float> gradOrigin {
                centre.x - radius * 0.20f,
                centre.y - radius * 0.30f
            };
            juce::ColourGradient body(juce::Colour(0xff2a2e36), gradOrigin,
                                        juce::Colour(0xff14171c),
                                        { centre.x + radius, centre.y + radius },
                                        true);
            body.addColour(0.55, juce::Colour(0xff1d2128));
            g.setGradientFill(body);
            g.fillEllipse(juce::Rectangle<float>(radius * 2, radius * 2).withCentre(centre));

            // Hint of an inner rim so the body has a defined edge against
            // the dark backdrop.
            g.setColour(juce::Colour(0x33000000));
            g.drawEllipse(juce::Rectangle<float>(radius * 2, radius * 2).withCentre(centre).reduced(0.5f), 1.0f);
        }
        else
        {
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
    }

    // OLED bezel — same on both variants (always reads as a black inset).
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

    // Arc track — same on both variants (lit white arc reads on either bg).
    {
        juce::Path track;
        track.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f,
                             kArcStartRad, kArcStartRad + kArcSweepRad, true);
        g.setColour(juce::Colour(0x1AFFFFFF));
        g.strokePath(track, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::butt));
    }

    return cache;
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
