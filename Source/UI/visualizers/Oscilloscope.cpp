// Source/UI/visualizers/Oscilloscope.cpp
//
// Full canvas port of Source/WebUI/oscilloscope.js — faithful translation,
// not approximation.  All colour values, trigger logic, scale factors, and
// layer order are taken from the JS source of truth.
//
#include "Oscilloscope.h"
#include "../Theme.h"
#include "../../PluginProcessor.h"
#include "../../Engines/PhantomEngine.h"
#include "../../Parameters.h"
#include "../../EngineFocus.h"
#include <cmath>

namespace kaigen::phantom
{

// ─── Constructor / Destructor ────────────────────────────────────────────────

Oscilloscope::Oscilloscope(PhantomProcessor& p)
    : processor(p)
{
    setSize(800, 120);
    startTimerHz(30);
}

Oscilloscope::~Oscilloscope()
{
    stopTimer();
}

// ─── Timer callback ──────────────────────────────────────────────────────────

void Oscilloscope::timerCallback()
{
    // Snapshot each ring buffer into a linearized (oldest-first) array.
    // wrPos points to the slot that will be written next, so it is the oldest
    // sample — identical to the JS linearize() function.
    // Relaxed loads are fine: visualization tolerates eventual consistency.

    const int inWrPos  = processor.oscInputWrPos .load(std::memory_order_relaxed);
    const int outWrPos = processor.oscOutputWrPos.load(std::memory_order_relaxed);
    const int synWrPos = processor.getActiveEngine().oscSynthWrPos
                                   .load(std::memory_order_relaxed);

    for (int i = 0; i < kBufSize; ++i)
    {
        linearIn [(size_t) i] = processor.oscInputBuf [(size_t)((inWrPos  + i) & (kBufSize - 1))]
                                    .load(std::memory_order_relaxed);
        linearSyn[(size_t) i] = processor.getActiveEngine().oscSynthBuf
                                    [(size_t)((synWrPos + i) & (kBufSize - 1))]
                                    .load(std::memory_order_relaxed);
        linearOut[(size_t) i] = processor.oscOutputBuf[(size_t)((outWrPos + i) & (kBufSize - 1))]
                                    .load(std::memory_order_relaxed);
    }

    // Auto-scale: find peak across in + out over the display window.
    // Trigger is needed to determine the window start; compute it now.
    if (autoScale)
    {
        const int trig = findTriggerIndex(linearIn);
        float peak = 1e-6f;
        for (int i = trig; i < trig + kDisplaySamples && i < kBufSize; ++i)
        {
            peak = juce::jmax(peak,
                              std::abs(linearIn [(size_t) i]),
                              std::abs(linearOut[(size_t) i]));
        }
        const float target = peak > 0.001f ? juce::jmin(8.0f, 1.0f / peak) : 1.0f;
        // Smooth the transition so auto-scale doesn't pop (JS uses instant;
        // we add light exponential smoothing for the native repaint cadence).
        currentNormScale += (target - currentNormScale) * 0.10f;
    }
    else
    {
        currentNormScale = 1.0f;
    }

    repaint();
}

// ─── Trigger detection ────────────────────────────────────────────────────────

int Oscilloscope::findTriggerIndex(const std::array<float, 2048>& linear) const noexcept
{
    // Mirrors JS findTrigger(): search i=8 to searchEnd for first positive-slope
    // zero crossing.  searchEnd = OSC_BUF_SIZE - OSC_DISPLAY_SAMPLES - 8.
    const int searchEnd = kBufSize - kDisplaySamples - 8;
    for (int i = 8; i < searchEnd; ++i)
    {
        if (linear[(size_t)(i - 1)] <= 0.0f && linear[(size_t) i] > 0.0f)
            return i;
    }
    return 8; // free-run fallback — no crossing found
}

// ─── Trace path builder ──────────────────────────────────────────────────────

juce::Path Oscilloscope::buildTracePath(const std::array<float, 2048>& linear,
                                         int trigStart, float w, float h,
                                         float scaleY) const
{
    juce::Path p;
    if constexpr (kDisplaySamples < 2) return p;

    const float mid = h * 0.5f;

    for (int px = 0; px < (int) w; ++px)
    {
        // Map pixel → sample index, matching JS:
        //   idx = trigStart + round(px * OSC_DISPLAY_SAMPLES / w)
        const int idx = trigStart + (int) std::round(
                            (float) px * (float) kDisplaySamples / w);
        const int clamped = juce::jmin(idx, kBufSize - 1);
        const float sample = juce::jlimit(-1.0f, 1.0f, linear[(size_t) clamped]);
        const float y = mid - sample * scaleY;
        const float x = (float) px + 0.5f;  // pixel-centre, matches JS (px + 0.5)

        if (px == 0)
            p.startNewSubPath(x, y);
        else
            p.lineTo(x, y);
    }
    return p;
}

// ─── paint() ─────────────────────────────────────────────────────────────────

void Oscilloscope::paint(juce::Graphics& g)
{
    // ── 1. Pitch-black background ────────────────────────────────────────────
    Theme::paintVisualizerInset(g, getLocalBounds(), 6.0f);

    const float w   = (float) getWidth();
    const float h   = (float) getHeight();
    const float mid = h * 0.5f;

    // ── 2. Zero line (h*0.5, rgba(255,255,255,0.06)) ────────────────────────
    g.setColour(juce::Colour(0x0fffffff)); // 0x0f ≈ 6% of 255
    g.drawHorizontalLine((int) mid, 0.0f, w);

    // ── 3. Vertical time-grid lines (7 lines at i*w/8, rgba(255,255,255,0.03)) ─
    g.setColour(juce::Colour(0x08ffffff)); // 0x08 = 8/255 ≈ 3%
    for (int i = 1; i < 8; ++i)
    {
        const float x = std::round(w * (float) i / 8.0f) + 0.5f;
        g.drawLine(x, 0.0f, x, h, 1.0f);
    }

    // Trigger on input channel (linearized snapshot updated in timerCallback).
    const int trig = findTriggerIndex(linearIn);

    // ── 4. Gate threshold lines ──────────────────────────────────────────────
    // Only in RESYN mode (mode == 1) and when gate threshold > 0.
    // Use the active engine's parameter prefix.
    {
        const bool isEngineB =
            (processor.getEngineFocus().activeTab == ActiveTab::B);
        const char* modeParamID  = isEngineB ? ParamID::B_MODE               : ParamID::A_MODE;
        const char* gateParamID  = isEngineB ? ParamID::B_SYNTH_GATE_THRESHOLD : ParamID::A_SYNTH_GATE_THRESHOLD;

        const auto* modeParam = processor.apvts.getRawParameterValue(modeParamID);
        const auto* gateParam = processor.apvts.getRawParameterValue(gateParamID);

        const bool isResyn  = modeParam && (modeParam->load() >= 0.5f);  // 0=Effect, 1=RESYN
        const float gateRaw = gateParam ? gateParam->load() : 0.0f;     // 0–100

        // JS uses getNormalisedValue() (0–1); APVTS range is 0–100, so divide by 100.
        const float gateThr = gateRaw / 100.0f;

        if (isResyn && gateThr > 0.0f)
        {
            // rawScaleY is NOT auto-scaled (gate is absolute amplitude).
            const float rawScaleY   = h * 0.38f;
            const float lineAbove   = juce::jlimit(0.0f, h, mid - gateThr * rawScaleY);
            const float lineBelow   = juce::jlimit(0.0f, h, mid + gateThr * rawScaleY);

            // rgba(80,142,215,0.50)
            g.setColour(juce::Colour(0x7f508ED7));

            const float dashLen  = 4.0f;
            const float gapLen   = 4.0f;
            float x = 0.0f;
            while (x < w)
            {
                const float segEnd = juce::jmin(x + dashLen, w);
                g.drawLine(x, lineAbove, segEnd, lineAbove, 1.0f);
                g.drawLine(x, lineBelow, segEnd, lineBelow, 1.0f);
                x += dashLen + gapLen;
            }
        }
    }

    // ── 5. Zero-crossing markers ─────────────────────────────────────────────
    // Scan the display window of the input linearized buffer for positive-slope
    // zero crossings. Valid if spaced >= minPeriod samples.
    {
        const bool isEngineB  =
            (processor.getEngineFocus().activeTab == ActiveTab::B);
        const char* minSmpID  = isEngineB ? ParamID::B_SYNTH_MIN_SAMPLES : ParamID::A_SYNTH_MIN_SAMPLES;
        const auto* minSmpParam = processor.apvts.getRawParameterValue(minSmpID);
        const float minPeriod   = minSmpParam ? minSmpParam->load() : 11.0f;

        float lastCross = -1e9f;

        for (int i = trig + 1; i < trig + kDisplaySamples - 1; ++i)
        {
            // Use linearized buffer with bounds check.
            const int idx = juce::jmin(i,     kBufSize - 1);
            const int prv = juce::jmin(i - 1, kBufSize - 1);

            if (linearIn[(size_t) prv] <= 0.0f && linearIn[(size_t) idx] > 0.0f)
            {
                const bool isValid = ((float)(i) - lastCross) >= minPeriod;
                const float px = std::round((float)(i - trig) * w / (float) kDisplaySamples) + 0.5f;

                if (isValid)
                {
                    // rgba(80,142,215,0.30)
                    g.setColour(juce::Colour(0x4d508ED7));
                    g.drawLine(px, 0.0f, px, h, 1.0f);
                }
                else
                {
                    // rgba(255,100,60,0.14) — dashed [2, 4]
                    g.setColour(juce::Colour(0x24FF643C));
                    float dy = 0.0f;
                    while (dy < h)
                    {
                        const float segEnd = juce::jmin(dy + 2.0f, h);
                        g.drawLine(px, dy, px, segEnd, 1.0f);
                        dy += 2.0f + 4.0f;
                    }
                }

                lastCross = (float) i;
            }
        }
    }

    // ── Shared scale for all three waveforms ─────────────────────────────────
    const float scaleY = h * 0.38f * currentNormScale;

    // ── 6. Input waveform (back): rgba(160,160,175,0.28) width 1 ────────────
    {
        juce::Path pIn = buildTracePath(linearIn, trig, w, h, scaleY);
        g.setColour(juce::Colour(0x47A0A0AF));
        g.strokePath(pIn, juce::PathStrokeType(1.0f));
    }

    // ── 7. Synth waveform (middle): rgba(80,142,215,0.82) width 1.5 + glow ──
    {
        juce::Path pSyn = buildTracePath(linearSyn, trig, w, h, scaleY);

        // Glow pass: paint the path once at lower alpha + wider stroke to
        // simulate the JS shadowBlur=5 effect on the synth trace.
        // Use a slightly wider stroke at ~20% alpha as a halo substitute.
        g.setColour(juce::Colour(0x33508ED7));  // ~20% blue glow
        g.strokePath(pSyn, juce::PathStrokeType(5.0f));

        // Main stroke: rgba(80,142,215,0.82)
        g.setColour(juce::Colour(0xd1508ED7));
        g.strokePath(pSyn, juce::PathStrokeType(1.5f));
    }

    // ── 8. Output waveform (front): rgba(255,255,255,0.62) width 1 ──────────
    {
        juce::Path pOut = buildTracePath(linearOut, trig, w, h, scaleY);
        g.setColour(juce::Colour(0x9effffff));
        g.strokePath(pOut, juce::PathStrokeType(1.0f));
    }

    // ── 9. Legend (top-left) ─────────────────────────────────────────────────
    // JS: fs = max(7, round(h * 0.10)); gap = fs * 3.4
    // Labels: "IN" (gray), "SYNTH" (blue), "OUT" (white)
    {
        const float fs = juce::jmax(7.0f, std::round(h * 0.10f));
        const float gap = fs * 3.4f;

        g.setFont(juce::Font(juce::FontOptions()
                                 .withName("Courier New")
                                 .withHeight(fs)
                                 .withStyle("Bold")));

        // "IN" — rgba(160,160,175,0.45)
        g.setColour(juce::Colour(0x72A0A0AF));
        g.drawText("IN",    4,                     3, (int)(gap - 2), (int)(fs + 4), juce::Justification::topLeft, false);

        // "SYNTH" — rgba(80,142,215,0.90)
        g.setColour(juce::Colour(0xe6508ED7));
        g.drawText("SYNTH", (int)(4 + gap),        3, (int)(gap * 2 - 2), (int)(fs + 4), juce::Justification::topLeft, false);

        // "OUT" — rgba(255,255,255,0.62)
        g.setColour(juce::Colour(0x9effffff));
        g.drawText("OUT",   (int)(4 + gap * 2.65f), 3, (int)(gap * 2 - 2), (int)(fs + 4), juce::Justification::topLeft, false);
    }
}

} // namespace kaigen::phantom
