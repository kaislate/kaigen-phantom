// Source/UI/widgets/RecipeWheel.cpp
//
// Full canvas port of Source/WebUI/recipe-wheel.js.
// Read every line of that file before touching this one.
#include "RecipeWheel.h"

#include <cmath>
#include <random>

namespace kaigen::phantom
{

// ─── Constants mirrored verbatim from recipe-wheel.js ─────────────────────────

static constexpr float kRingRadii[6]  = { 0.92f, 0.77f, 0.60f, 0.40f, 0.22f, 0.96f };
static constexpr float kRingWidths[6] = { 0.8f,  0.6f,  0.5f,  0.6f,  0.8f,  0.3f  };
static constexpr float kRingAlphas[6] = { 0.18f, 0.12f, 0.08f, 0.13f, 0.22f, 0.06f };
static constexpr float kRingSpeeds[6] = { 0.015f,-0.020f, 0.010f,-0.025f, 0.012f,-0.008f };

// ─── Helpers ──────────────────────────────────────────────────────────────────

/** Spoke angle: (i/7)*2π - π/2.  Spoke 0 points straight up. */
float RecipeWheel::spokeAngle(int i) noexcept
{
    return ((float) i / (float) kSpokes) * juce::MathConstants<float>::twoPi
           - juce::MathConstants<float>::halfPi;
}

/** Get normalised [0,1] amplitude for spoke i from its hidden slider. */
float RecipeWheel::getSpokeAmp(int i) const noexcept
{
    const auto& s = sliders[(size_t) i];
    return (float) s.getNormalisableRange().convertTo0to1(s.getValue());
}

/** Write a new amplitude via the hidden slider (triggers APVTS). */
void RecipeWheel::setSpokeAmp(int spokeIdx, float amp01)
{
    auto& s = sliders[(size_t) spokeIdx];
    const auto value = s.getNormalisableRange().convertFrom0to1((double) juce::jlimit(0.0f, 1.0f, amp01));
    s.setValue(value, juce::sendNotificationSync);
}

// ─── Constructor / Destructor ─────────────────────────────────────────────────

RecipeWheel::RecipeWheel(juce::AudioProcessorValueTreeState& apvts,
                         const std::array<juce::String, kSpokes>& paramIDs)
{
    // Initialise 140 particles with random progress so they start spread out
    // across the spoke lengths (steady-state like the JS Math.random() init).
    {
        std::mt19937 rng(42u);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        for (auto& p : particleProgress)
            p = dist(rng);
    }

    for (int i = 0; i < kSpokes; ++i)
    {
        auto& s = sliders[(size_t) i];
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        // Hidden — never shown, only used as APVTS attachment surface.
        addChildComponent(s);

        if (auto* param = apvts.getParameter(paramIDs[(size_t) i]))
        {
            params[(size_t) i] = param;
            attachments[(size_t) i] =
                std::make_unique<juce::SliderParameterAttachment>(*param, s);
        }
        else
        {
            jassertfalse; // unknown paramID — typo or stale reference
        }
    }

    // Timer at 60 Hz; repaint throttled to 30 fps inside timerCallback().
    startTimerHz(60);
}

RecipeWheel::~RecipeWheel()
{
    stopTimer();
}

// ─── Timer ────────────────────────────────────────────────────────────────────

void RecipeWheel::timerCallback()
{
    // Animation state always advances every tick (60 Hz).
    // Ring rotation: JS does ringRot[i] += ringSpeed[i] * 0.5 per draw frame
    //   at 30 fps.  At 60 Hz we run the physics every tick, so we multiply by
    //   0.5 here AND only advance on even ticks to keep the same net rate.
    //   Equivalently: advance ringSpeed[i]*0.5 every OTHER tick = ringSpeed[i]*0.25
    //   per tick.  But the simplest faithful port is: advance ONLY on even ticks
    //   (same as draw frames) with the full *0.5 factor.
    const bool isDrawFrame = ((timerTick & 1) == 0);

    if (isDrawFrame)
    {
        // Ring rotation (JS: ringRot[i] += ringSpeed[i] * 0.5 each draw frame)
        for (int i = 0; i < kNumRings; ++i)
            ringRot[(size_t) i] += kRingSpeeds[i] * 0.5f;

        // Particles (JS: p.progress += spd each draw frame, wraps to 0 when >1)
        for (int s = 0; s < kSpokes; ++s)
        {
            const float amp = getSpokeAmp(s);
            const float spd = 0.004f + amp * 0.020f;
            for (int p = 0; p < kParticlesPerSpoke; ++p)
            {
                auto& prog = particleProgress[(size_t)(s * kParticlesPerSpoke + p)];
                prog += spd;
                if (prog > 1.0f) prog = 0.0f;
            }
        }

        // Scan line (JS: scanAngle += 0.018 each draw frame)
        scanAngle += 0.018f;

        // Shimmer (JS: shimmerT += 0.05 each draw frame)
        shimmerT += 0.05f;

        repaint();
    }

    ++timerTick;
}

// ─── Hit testing ──────────────────────────────────────────────────────────────

/** Matches JS hitSpoke() exactly: perpendicular-distance approach. */
int RecipeWheel::hitSpoke(juce::Point<float> p) const
{
    const auto  bounds = getLocalBounds().toFloat();
    const auto  centre = bounds.getCentre();
    const float r      = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f - 4.0f;
    const float innerR = r * 0.22f;
    const float outerR = r * 0.90f;

    for (int i = 0; i < kSpokes; ++i)
    {
        const float a   = spokeAngle(i);
        const float c   = std::cos(a);
        const float s_  = std::sin(a);
        const float dx  = p.x - centre.x;
        const float dy  = p.y - centre.y;
        const float proj = dx * c + dy * s_;
        const float perp = std::abs(-dx * s_ + dy * c);
        const float hitW = juce::jmax(12.0f, r * 0.06f);

        // JS: proj >= innerR * 0.8 && proj <= outerR * 1.1
        if (proj >= innerR * 0.8f && proj <= outerR * 1.1f && perp < hitW)
            return i;
    }
    return -1;
}

/** Matches JS pointerToAmp(): project onto spoke direction, normalise to [0,1]. */
float RecipeWheel::pointerToAmp(juce::Point<float> p, int spokeIdx) const
{
    const auto  bounds = getLocalBounds().toFloat();
    const auto  centre = bounds.getCentre();
    const float r      = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f - 4.0f;
    const float innerR = r * 0.22f;
    const float outerR = r * 0.90f;

    const float a    = spokeAngle(spokeIdx);
    const float proj = (p.x - centre.x) * std::cos(a) + (p.y - centre.y) * std::sin(a);
    return juce::jlimit(0.0f, 1.0f, (proj - innerR) / (outerR - innerR));
}

// ─── Mouse interaction ────────────────────────────────────────────────────────

void RecipeWheel::mouseDown(const juce::MouseEvent& e)
{
    const int s = hitSpoke(e.position);
    if (s < 0) return;
    dragSpoke  = s;
    hoverSpoke = -1;
    if (auto* param = params[(size_t) s])
        param->beginChangeGesture();
    setSpokeAmp(s, pointerToAmp(e.position, s));
}

void RecipeWheel::mouseDrag(const juce::MouseEvent& e)
{
    if (dragSpoke < 0) return;
    setSpokeAmp(dragSpoke, pointerToAmp(e.position, dragSpoke));
}

void RecipeWheel::mouseUp(const juce::MouseEvent&)
{
    if (dragSpoke >= 0)
    {
        if (auto* param = params[(size_t) dragSpoke])
            param->endChangeGesture();
        dragSpoke = -1;
        repaint();
    }
}

void RecipeWheel::mouseMove(const juce::MouseEvent& e)
{
    const int h = hitSpoke(e.position);
    if (h != hoverSpoke)
    {
        hoverSpoke = h;
        setMouseCursor(h >= 0 ? juce::MouseCursor::LeftRightResizeCursor
                               : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void RecipeWheel::mouseExit(const juce::MouseEvent&)
{
    if (hoverSpoke != -1)
    {
        hoverSpoke = -1;
        repaint();
    }
}

// ─── resized ──────────────────────────────────────────────────────────────────

void RecipeWheel::resized()
{
    // Sliders are hidden; their pixel bounds don't matter.
    for (auto& s : sliders)
        s.setBounds(0, 0, 0, 0);
}

// ─── paint ────────────────────────────────────────────────────────────────────

void RecipeWheel::paint(juce::Graphics& g)
{
    const auto  bounds = getLocalBounds().toFloat();
    const float w      = bounds.getWidth();
    const float h      = bounds.getHeight();
    const float cx     = w * 0.5f;
    const float cy     = h * 0.5f;
    // JS: R = min(w,h)*0.5 - 4   (in canvas pixel coords, which == JUCE logical coords)
    const float R      = juce::jmin(w, h) * 0.5f - 4.0f;
    const float innerR = R * 0.22f;
    const float outerR = R * 0.90f;

    // ── 1. Background radial gradient ────────────────────────────────────
    // JS: createRadialGradient(cx,cy,0, cx,cy,R)
    //   stop 0:   rgba(10,10,20,0.4)
    //   stop 0.6: rgba(3,3,8,0.6)
    //   stop 1:   rgba(0,0,0,0.8)
    {
        juce::ColourGradient bg(
            juce::Colour(0x660a0a14),   // rgba(10,10,20,0.4)  — 0x66 = 102 ≈ 40%
            cx, cy,
            juce::Colour(0xCC000000),   // rgba(0,0,0,0.8)     — 0xCC = 204 ≈ 80%
            cx, cy + R,
            true);                      // radial
        bg.addColour(0.6, juce::Colour(0x99030308)); // rgba(3,3,8,0.6) — 0x99 = 153 ≈ 60%
        g.setGradientFill(bg);
        g.fillEllipse(cx - R, cy - R, R * 2.0f, R * 2.0f);
    }

    // ── 2. Holographic rings ─────────────────────────────────────────────
    // JS: full arc (0 → 2π) starting at ringRot[i].  The offset IS the rotation.
    //     lineWidth = ringWidths[i]  (no DPR scaling in JUCE)
    for (int i = 0; i < kNumRings; ++i)
    {
        const float rr = R * kRingRadii[i];
        // Create a path for the full circle (arc from 0 to 2π, offset doesn't
        // change the arc shape — it's a full circle regardless — but mirroring
        // the JS draw call faithfully).
        juce::Path ring;
        ring.addEllipse(cx - rr, cy - rr, rr * 2.0f, rr * 2.0f);

        const uint8_t alpha = (uint8_t) juce::roundToInt(kRingAlphas[i] * 255.0f);
        g.setColour(juce::Colour(255, 255, 255).withAlpha(alpha));
        g.strokePath(ring, juce::PathStrokeType(kRingWidths[i]));
    }

    // ── 3+4+5+6+7+8. Spokes ─────────────────────────────────────────────
    for (int i = 0; i < kSpokes; ++i)
    {
        const float a   = spokeAngle(i);
        const float c   = std::cos(a);
        const float s_  = std::sin(a);
        const float amp = getSpokeAmp(i);
        const bool  hot = (i == dragSpoke || i == hoverSpoke);

        // Spoke endpoint coordinates
        const float ixStart = cx + innerR * c;
        const float iyStart = cy + innerR * s_;
        const float oxEnd   = cx + outerR * c;
        const float oyEnd   = cy + outerR * s_;

        // ── 3. Dim track (full spoke) ──────────────────────────────────
        // JS: strokeStyle = hot ? rgba(255,255,255,0.12) : rgba(255,255,255,0.05)
        //     lineWidth = 5, lineCap = round
        {
            const float trackAlpha = hot ? 0.12f : 0.05f;
            g.setColour(juce::Colours::white.withAlpha(trackAlpha));
            juce::Path track;
            track.startNewSubPath(ixStart, iyStart);
            track.lineTo(oxEnd, oyEnd);
            g.strokePath(track, juce::PathStrokeType(5.0f,
                juce::PathStrokeType::curved,
                juce::PathStrokeType::rounded));
        }

        // Fill portion proportional to amplitude
        const float fillEnd = innerR + (outerR - innerR) * amp;
        const float fx1 = cx + innerR  * c,  fy1 = cy + innerR  * s_;
        const float fx2 = cx + fillEnd * c,  fy2 = cy + fillEnd * s_;

        // ── 4. Glow halo (drawn first, behind fill line) ──────────────
        // JS: strokeStyle = rgba(255,255,255, 0.25*amp + (hot?0.15:0))
        //     lineWidth = hot ? 12 : 9
        {
            const float glowAlpha = 0.25f * amp + (hot ? 0.15f : 0.0f);
            if (glowAlpha > 0.001f)
            {
                g.setColour(juce::Colours::white.withAlpha(glowAlpha));
                juce::Path glow;
                glow.startNewSubPath(fx1, fy1);
                glow.lineTo(fx2, fy2);
                g.strokePath(glow, juce::PathStrokeType(hot ? 12.0f : 9.0f,
                    juce::PathStrokeType::curved,
                    juce::PathStrokeType::rounded));
            }
        }

        // ── 5. Sharp fill line with linear gradient ────────────────────
        // JS: linear gradient along spoke direction
        //   stop 0: rgba(255,255,255, 0.55*amp + (hot?0.2:0))
        //   stop 1: rgba(255,255,255, 0.08*amp + (hot?0.1:0))
        //   lineWidth = hot ? 5 : 3.5
        if (amp > 0.001f || hot)
        {
            const float a0 = 0.55f * amp + (hot ? 0.2f : 0.0f);
            const float a1 = 0.08f * amp + (hot ? 0.1f : 0.0f);

            juce::ColourGradient fillGrad(
                juce::Colours::white.withAlpha(juce::jlimit(0.0f,1.0f,a0)), fx1, fy1,
                juce::Colours::white.withAlpha(juce::jlimit(0.0f,1.0f,a1)), fx2, fy2,
                false);

            juce::Path fill;
            fill.startNewSubPath(fx1, fy1);
            fill.lineTo(fx2, fy2);
            g.setGradientFill(fillGrad);
            g.strokePath(fill, juce::PathStrokeType(hot ? 5.0f : 3.5f,
                juce::PathStrokeType::curved,
                juce::PathStrokeType::rounded));
        }

        // ── 6. Cap dot at fill endpoint ────────────────────────────────
        // JS: fillStyle = rgba(255,255,255, 0.8*amp + (hot?0.2:0))
        //     radius = hot ? 5 : 3
        {
            const float capAlpha  = 0.8f * amp + (hot ? 0.2f : 0.0f);
            const float capRadius = hot ? 5.0f : 3.0f;
            if (capAlpha > 0.001f)
            {
                g.setColour(juce::Colours::white.withAlpha(juce::jlimit(0.0f,1.0f,capAlpha)));
                g.fillEllipse(fx2 - capRadius, fy2 - capRadius,
                              capRadius * 2.0f, capRadius * 2.0f);
            }
        }

        // ── 7. Outer node circle (at outerR endpoint) ──────────────────
        // JS: nodeSize = (3 + amp*10)
        //   halo: radial gradient, rgba(255,255,255,0.4*amp)→transparent, radius nodeSize*3
        //   inner solid: radius nodeSize*0.5, rgba(255,255,255,0.5+amp*0.45)
        {
            const float nx       = cx + outerR * c;
            const float ny       = cy + outerR * s_;
            const float nodeSize = 3.0f + amp * 10.0f;
            const float haloR    = nodeSize * 3.0f;

            // Halo radial gradient
            const float haloAlpha = 0.4f * amp;
            if (haloAlpha > 0.001f)
            {
                juce::ColourGradient haloGrad(
                    juce::Colours::white.withAlpha(haloAlpha), nx, ny,
                    juce::Colours::transparentWhite,            nx + haloR, ny,
                    true);
                g.setGradientFill(haloGrad);
                g.fillEllipse(nx - haloR, ny - haloR, haloR * 2.0f, haloR * 2.0f);
            }

            // Inner solid dot
            const float innerAlpha = juce::jlimit(0.0f, 1.0f, 0.5f + amp * 0.45f);
            const float innerR2    = nodeSize * 0.5f;
            g.setColour(juce::Colours::white.withAlpha(innerAlpha));
            g.fillEllipse(nx - innerR2, ny - innerR2, innerR2 * 2.0f, innerR2 * 2.0f);
        }

        // ── 8. Spoke label (hot && amp > 0) ───────────────────────────
        // JS: position = (fillEnd + 16) along spoke direction from centre
        //     font: bold 9px monospace, rgba(255,255,255,0.9)
        //     text: round(amp*100) + '%'
        if (hot && amp > 0.001f)
        {
            const float lx = cx + (fillEnd + 16.0f) * c;
            const float ly = cy + (fillEnd + 16.0f) * s_;
            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.setFont(juce::Font(juce::FontOptions("monospace", 9.0f, juce::Font::bold)));
            const juce::String pctText = juce::String(juce::roundToInt(amp * 100)) + "%";
            g.drawText(pctText,
                       (int) lx - 24, (int) ly - 8, 48, 16,
                       juce::Justification::centred, false);
        }

        // (H2..H8 labels are painted by LeftPanel in the silver area around
        // the wheel — etched dark-on-light, matching GHOST/FILTER section
        // headings. See LeftPanel::paint().)
    }

    // ── 9. Particles ─────────────────────────────────────────────────────
    // JS: radius 1.4 px, rgba(255,255,255, amp*(1-progress)*0.85)
    for (int s = 0; s < kSpokes; ++s)
    {
        const float a   = spokeAngle(s);
        const float c   = std::cos(a);
        const float s_  = std::sin(a);
        const float amp = getSpokeAmp(s);

        for (int p = 0; p < kParticlesPerSpoke; ++p)
        {
            const float progress = particleProgress[(size_t)(s * kParticlesPerSpoke + p)];
            const float al       = amp * (1.0f - progress) * 0.85f;
            if (al < 0.01f) continue;

            const float r  = innerR + (outerR - innerR) * progress;
            const float px = cx + r * c;
            const float py = cy + r * s_;

            g.setColour(juce::Colours::white.withAlpha(al));
            g.fillEllipse(px - 1.4f, py - 1.4f, 2.8f, 2.8f);
        }
    }

    // ── 10. Scan line ────────────────────────────────────────────────────
    // JS: radial line from centre to outerR*0.95, linear gradient
    //   stop 0:   rgba(255,255,255,0)
    //   stop 0.7: rgba(255,255,255,0.04)
    //   stop 1:   rgba(255,255,255,0.12)
    //   lineWidth: 1.5
    {
        const float scanDist = outerR * 0.95f;
        const float sx = cx + std::cos(scanAngle) * scanDist;
        const float sy = cy + std::sin(scanAngle) * scanDist;

        juce::ColourGradient scanGrad(
            juce::Colours::transparentWhite,           cx, cy,
            juce::Colours::white.withAlpha(0.12f),     sx, sy,
            false);
        scanGrad.addColour(0.7, juce::Colours::white.withAlpha(0.04f));

        juce::Path scanLine;
        scanLine.startNewSubPath(cx, cy);
        scanLine.lineTo(sx, sy);
        g.setGradientFill(scanGrad);
        g.strokePath(scanLine, juce::PathStrokeType(1.5f));
    }

    // ── 11. Centre glow (pulsing) ─────────────────────────────────────────
    // JS: shimmerT += 0.05, pulse = 0.7 + 0.3*sin(shimmerT)
    //   radial gradient from centre to innerR:
    //   stop 0:   rgba(255,255,255, 0.25*pulse)
    //   stop 0.5: rgba(255,255,255, 0.08*pulse)
    //   stop 1:   rgba(255,255,255, 0)
    {
        const float pulse = 0.7f + 0.3f * std::sin(shimmerT);

        juce::ColourGradient glowGrad(
            juce::Colours::white.withAlpha(0.25f * pulse), cx, cy,
            juce::Colours::transparentWhite,                cx, cy + innerR,
            true);
        glowGrad.addColour(0.5, juce::Colours::white.withAlpha(0.08f * pulse));

        g.setGradientFill(glowGrad);
        g.fillEllipse(cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f);
    }
}

} // namespace kaigen::phantom
