// Source/UI/widgets/PhantomKnob.cpp
// Faithful canvas port of Source/WebUI/knob.js _render() / _ensureScaffold().
// Source of truth: knob.js (SVG, not Canvas2D).
#include "PhantomKnob.h"
#include "KnobValueFormat.h"

namespace kaigen::phantom
{

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor / destructor
// ─────────────────────────────────────────────────────────────────────────────

PhantomKnob::PhantomKnob(juce::AudioProcessorValueTreeState& apvts,
                          juce::StringRef paramID,
                          Size sz,
                          const juce::String& lbl)
    : sizeVariant(sz), labelText(lbl)
{
    // Hidden slider — attached to APVTS; drives repaint via listener.
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    slider.onValueChange = [this] { repaint(); };
    addChildComponent(slider);   // hidden — zero-sized below via resized()

    if (auto* p = apvts.getParameter(paramID))
    {
        param = p;
        attachment = std::make_unique<juce::SliderParameterAttachment>(*p, slider);
        // Cache the default normalized value for double-click reset.
        defaultNorm = p->getDefaultValue();
    }
    else
    {
        jassertfalse;   // unknown paramID
    }

    // Widget size: diameter + small vertical padding for text clearance.
    const int d = diameter();
    setSize(d + 8, d + 8);
}

PhantomKnob::~PhantomKnob() = default;

// ─────────────────────────────────────────────────────────────────────────────
//  Geometry helpers
//  Values match getSizeTier() in knob.js:
//    large  → sz=114, inset=6
//    medium → sz=88,  inset=5   (default)
//    small  → sz=56,  inset=3
// ─────────────────────────────────────────────────────────────────────────────

int PhantomKnob::diameter() const
{
    switch (sizeVariant)
    {
        case Size::Large:  return 114;
        case Size::Small:  return  56;
        default:           return  88;
    }
}

int PhantomKnob::inset() const
{
    switch (sizeVariant)
    {
        case Size::Large:  return 6;
        case Size::Small:  return 3;
        default:           return 5;
    }
}

// textPx — matches the CSS font-size rules and drag overrides in knob.js.
float PhantomKnob::textPx(bool dragging) const
{
    switch (sizeVariant)
    {
        case Size::Large:  return dragging ? 22.0f : 14.0f;
        case Size::Small:  return dragging ? 13.0f :  9.0f;
        default:           return dragging ? 18.0f : 12.0f;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  paint() — orchestrates all layers
// ─────────────────────────────────────────────────────────────────────────────

void PhantomKnob::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty()) return;

    const float sz    = (float) diameter();
    const float in    = (float) inset();
    const auto  centre = bounds.getCentre();
    const float oledR  = sz * 0.5f - in;
    const float arcR   = oledR - 4.0f;

    // Arc constants — matching knob.js: ARC_START=135°, ARC_SWEEP=270°.
    // Note: JUCE addCentredArc uses radians measured clockwise from 12-o'clock.
    // knob.js polarToXY uses standard math angles (0° = 3-o'clock, CCW).
    // We replicate the visual, so arcStart/arcSweep are in JUCE's convention
    // (radians from 12-o'clock, CW) which matches juce::degreesToRadians of the
    // CSS/SVG interpretation:  135° from 12-o'clock CW = 7-o'clock start.
    const float arcStart = juce::degreesToRadians(135.0f);
    const float arcSweep = juce::degreesToRadians(270.0f);

    // Normalized value [0,1].
    const auto& range    = slider.getNormalisableRange();
    const float normVal  = (float) range.convertTo0to1(slider.getValue());

    paintBody          (g, centre, sz * 0.5f);
    paintOLED          (g, centre, oledR);
    paintArcTrack      (g, centre, arcR, arcStart, arcSweep);
    paintIndicatorArc  (g, centre, arcR, arcStart, arcSweep, normVal);
    paintValueText     (g, centre, formatValue(), oledR);
}

void PhantomKnob::resized()
{
    // Slider is hidden — zero-sized so it never clips or intercepts mouse events
    // in an unexpected area. We handle all mouse interaction ourselves.
    slider.setBounds(0, 0, 0, 0);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Layer 1 — Neumorphic raised body
//
//  CSS source (knob.js :host {}):
//    background: radial-gradient(circle at 35% 30%,
//      rgba(255,255,255,0.24) 0%,
//      rgba(255,255,255,0.12) 22%,
//      rgba(0,0,0,0.02)       60%,
//      rgba(0,0,0,0.07)       100%);
//    box-shadow (medium):
//      -3px -3px 12px rgba(255,255,255,0.70)
//      -5px -6px 22px rgba(255,255,255,0.34)
//       3px  4px 14px rgba(0,0,0,0.34)
//       5px  7px 24px rgba(0,0,0,0.16)
//    box-shadow (large):
//      -4px -4px 16px rgba(255,255,255,0.70)
//      -7px -8px 30px rgba(255,255,255,0.36)
//       4px  5px 18px rgba(0,0,0,0.36)
//       7px  9px 32px rgba(0,0,0,0.18)
//    box-shadow (small):
//      -2px -2px  9px rgba(255,255,255,0.66)
//      -3px -4px 15px rgba(255,255,255,0.30)
//       2px  3px 10px rgba(0,0,0,0.30)
//       3px  5px 17px rgba(0,0,0,0.14)
// ─────────────────────────────────────────────────────────────────────────────

void PhantomKnob::paintBody(juce::Graphics& g, juce::Point<float> centre, float radius)
{
    juce::Path circle;
    circle.addEllipse(juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre));

    // CSS box-shadow recipe per knob.js — drives the volcano-slope illusion.
    // A faint cyan tint can appear from JUCE's software-blur on Windows; that
    // is the LANDED visual per docs/thoughts-2026-04-17.md ("the user-validated
    // look") and we DO NOT iterate on it. See docs/native-knob-recipe.md.
    struct ShadowParams { int ox, oy, blurA, ox2, oy2, blurB; };
    const ShadowParams sp = [&]() -> ShadowParams {
        switch (sizeVariant)
        {
            case Size::Large:  return {  4,  5, 18,  7,  9, 32 };
            case Size::Small:  return {  2,  3, 10,  3,  5, 17 };
            default:           return {  3,  4, 14,  5,  7, 24 };
        }
    }();

    // BR shadows (drawn first, behind TL highlights). Black blurs render cleanly.
    {
        juce::DropShadow brB { juce::Colour(0x29000000), sp.blurB, { sp.ox2, sp.oy2 } };
        brB.drawForPath(g, circle);
        juce::DropShadow brA { juce::Colour(0x57000000), sp.blurA, { sp.ox, sp.oy } };
        brA.drawForPath(g, circle);
    }
    // TL highlights — alpha values from CSS (rgba(255,255,255,0.34) and 0.70).
    {
        juce::DropShadow tlB { juce::Colour(0x57FFFFFF), sp.blurB, { -sp.ox2, -sp.oy2 } };
        tlB.drawForPath(g, circle);
        juce::DropShadow tlA { juce::Colour(0xb3FFFFFF), sp.blurA, { -sp.ox, -sp.oy } };
        tlA.drawForPath(g, circle);
    }

    // ── Knob body — TRANSPARENT-stop radial gradient ─────────────────────
    // KEY TRICK from the icy-redesign breakthrough (thoughts-2026-04-17.md):
    // the gradient stops are RGBA with low alpha so the panel SHOWS THROUGH.
    // At the outer edge we're 7% black (= 93% panel visible) — no hard
    // contrast edge means no visible knob boundary.
    const juce::Point<float> gradOrigin {
        centre.x - radius * 0.30f,    // 35% from left
        centre.y - radius * 0.40f     // 30% from top
    };
    juce::ColourGradient body(
        juce::Colour(0x3DFFFFFF), gradOrigin,       // rgba(255,255,255,0.24) at 0%
        juce::Colour(0x12000000),                   // rgba(0,0,0,0.07) at 100% — bezel shows through
        { centre.x + radius, centre.y + radius },
        true);
    body.addColour(0.22, juce::Colour(0x1FFFFFFF)); // rgba(255,255,255,0.12) at 22%
    body.addColour(0.60, juce::Colour(0x05000000)); // rgba(0,0,0,0.02) at 60%
    g.setGradientFill(body);
    g.fillEllipse(juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Layer 2 — OLED well + bezel rings
//
//  knob.js _ensureScaffold():
//    mkCircle(oledR, '#000')                                    — black fill
//    mkCircle(oledR,        null, 'rgba(255,255,255,0.28)', 0.75)  — inner bright
//    mkCircle(oledR + 0.75, null, 'rgba(0,0,0,0.92)',       0.75)  — dark bevel
//    mkCircle(oledR + 1.5,  null, 'rgba(255,255,255,0.10)', 0.5 )  — outer soft
// ─────────────────────────────────────────────────────────────────────────────

void PhantomKnob::paintOLED(juce::Graphics& g, juce::Point<float> centre, float oledR)
{
    const auto rect = juce::Rectangle<float>(oledR * 2.0f, oledR * 2.0f).withCentre(centre);

    g.setColour(juce::Colours::black);
    g.fillEllipse(rect);

    // Inner bright ring (radius = oledR, stroke 0.75 px).
    g.setColour(juce::Colour(0x47FFFFFF));   // rgba(255,255,255,0.28) ≈ 0x47
    g.drawEllipse(rect, 0.75f);

    // Dark bevel (radius = oledR + 0.75, stroke 0.75 px).
    g.setColour(juce::Colour(0xEB000000));   // rgba(0,0,0,0.92)
    g.drawEllipse(rect.expanded(0.75f), 0.75f);

    // Outer soft halo (radius = oledR + 1.5, stroke 0.5 px).
    g.setColour(juce::Colour(0x1AFFFFFF));   // rgba(255,255,255,0.10)
    g.drawEllipse(rect.expanded(1.5f), 0.5f);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Layer 3 — Arc track (faint background 270° arc)
//
//  knob.js: mkPath('rgba(255,255,255,0.06)', 3.5)
//           trackPath.d = describeArc(cx, cy, arcR, ARC_START, ARC_END - 0.01)
// ─────────────────────────────────────────────────────────────────────────────

void PhantomKnob::paintArcTrack(juce::Graphics& g, juce::Point<float> centre, float arcR,
                                  float arcStart, float arcSweep)
{
    juce::Path track;
    // Subtract tiny epsilon to avoid the path closing at the full 270° (matching
    // the ARC_END - 0.01 in knob.js).
    track.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f,
                        arcStart, arcStart + arcSweep - 0.0002f, true);
    g.setColour(juce::Colour(0x0FFFFFFF));   // rgba(255,255,255,0.06) ≈ 0x0F
    g.strokePath(track, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::butt));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Layers 4 & 5 — Indicator arc (glow halo + sharp value line)
//
//  knob.js: glowPath — stroke 'rgba(255,255,255,0.45)' width 6, filter glow-{sz}
//           valPath  — stroke '#fff' width 2.8
//           Only drawn when value > 0.001.
//
//  The SVG filter is feGaussianBlur stdDeviation=2.  We approximate this in
//  JUCE by drawing the glow arc as two stacked strokes at decreasing alpha
//  (simulates the Gaussian falloff without a real blur pass).
// ─────────────────────────────────────────────────────────────────────────────

void PhantomKnob::paintIndicatorArc(juce::Graphics& g, juce::Point<float> centre, float arcR,
                                     float arcStart, float arcSweep, float value01)
{
    if (value01 <= 0.001f) return;

    juce::Path arc;
    arc.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f,
                      arcStart, arcStart + arcSweep * value01, true);

    // Layer 4 — Glow halo (single soft stroke). Stacking multiple wide white
    // strokes caused heavy ClearType color fringing (cyan halos) on Windows;
    // CSS used a single 6 px stroke with feGaussianBlur — keep just one stroke.
    g.setColour(juce::Colour(0x40FFFFFF));   // 25% white — softer than the 45% CSS spec
    g.strokePath(arc, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));

    // Layer 5 — Sharp value arc (#fff, 2.8 px).
    g.setColour(juce::Colours::white);
    g.strokePath(arc, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Layer 6 — Value text (3-pass layered shadow)
//
//  knob.js _ensureScaffold() — for non-waveform:
//    valueY = cy - (isLarge ? 4 : 3)    (slightly above OLED centre)
//    Three <text> elements at opacity 0.3, 0.6, 1.0
//    font-family: 'Courier New', monospace
//    font-weight: 700
//    font-size: via CSS (.value-text)  9/12/14 px rest, 13/18/22 px drag
//
//  The spec also calls for a +1 Y offset on the faint pass — replicated here.
// ─────────────────────────────────────────────────────────────────────────────

void PhantomKnob::paintValueText(juce::Graphics& g, juce::Point<float> centre,
                                  const juce::String& text, float oledR)
{
    // Inner OLED width available for text — keep ~85% of diameter so glyphs
    // don't bleed against the bezel.
    const float maxW = oledR * 1.7f;

    // Start with the size-tier font. If the text is too wide, scale font down
    // until it fits (down to 6 px minimum so it stays readable).
    float fontPx = textPx(isDragging);
    juce::Font font(juce::FontOptions("Courier New", fontPx, juce::Font::bold));
    while (fontPx > 6.0f && (float) juce::GlyphArrangement::getStringWidthInt(font, text) > maxW)
    {
        fontPx -= 1.0f;
        font = juce::Font(juce::FontOptions("Courier New", fontPx, juce::Font::bold));
    }
    g.setFont(font);

    // Y position: centre shifted up slightly (knob.js cy - (isLarge ? 4 : 3)).
    const float yOffset = (sizeVariant == Size::Large) ? -4.0f : -3.0f;
    const float textH   = fontPx * 1.4f;
    const juce::Rectangle<float> rect(centre.x - maxW * 0.5f,
                                      centre.y + yOffset - textH * 0.5f,
                                      maxW, textH);

    // Pass 1: alpha 0.30, +1 px Y offset.
    g.setColour(juce::Colour(0x4DFFFFFF));
    g.drawText(text, rect.translated(0.0f, 1.0f), juce::Justification::centred, false);

    // Pass 2: alpha 0.60.
    g.setColour(juce::Colour(0x99FFFFFF));
    g.drawText(text, rect, juce::Justification::centred, false);

    // Pass 3: alpha 1.00 (fully opaque white).
    g.setColour(juce::Colours::white);
    g.drawText(text, rect, juce::Justification::centred, false);
}

// ─────────────────────────────────────────────────────────────────────────────
//  formatValue — uses the slider's built-in text formatter (set by APVTS) if
//  available; otherwise falls back to the raw value with 2 decimal places.
//  This matches knob.js: displayText = this._displayValue || this._value.toFixed(2)
// ─────────────────────────────────────────────────────────────────────────────

juce::String PhantomKnob::formatValue()
{
    return formatKnobValue(param, slider.getValue());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Mouse interaction
//  Drag: vertical pixel distance → normalized delta, 200 px = full range.
//  Double-click: reset to the parameter's stored default normalized value.
// ─────────────────────────────────────────────────────────────────────────────

void PhantomKnob::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown()) return;
    isDragging    = true;
    dragStartY    = e.y;
    dragStartNorm = (float) slider.getNormalisableRange()
                        .convertTo0to1(slider.getValue());
    // Notify SliderParameterAttachment that a gesture is starting (it calls
    // attachment.beginGesture() internally via the Slider::Listener callbacks).
    slider.startedDragging();
    repaint();
}

void PhantomKnob::mouseDrag(const juce::MouseEvent& e)
{
    if (!isDragging) return;

    // 200 px for full range — matches knob.js dy * -0.005 per-px sensitivity.
    const float dy       = (float)(dragStartY - e.y);
    const float newNorm  = juce::jlimit(0.0f, 1.0f, dragStartNorm + dy / 200.0f);
    const float newValue = (float) slider.getNormalisableRange().convertFrom0to1(newNorm);
    slider.setValue(newValue, juce::sendNotificationSync);
}

void PhantomKnob::mouseUp(const juce::MouseEvent&)
{
    if (!isDragging) return;
    isDragging = false;
    // Notify SliderParameterAttachment that the gesture has ended.
    slider.stoppedDragging();
    repaint();
}

void PhantomKnob::mouseDoubleClick(const juce::MouseEvent&)
{
    // Reset to the parameter's default normalized value (captured in constructor).
    const float defaultValue = (float) slider.getNormalisableRange()
                                   .convertFrom0to1(defaultNorm);
    // Wrap in a begin/end gesture so the undo manager sees a clean transaction.
    slider.startedDragging();
    slider.setValue(defaultValue, juce::sendNotificationSync);
    slider.stoppedDragging();
}

} // namespace kaigen::phantom
