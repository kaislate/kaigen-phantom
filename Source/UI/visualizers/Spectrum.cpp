// Source/UI/visualizers/Spectrum.cpp
//
// Full canvas port of Source/WebUI/spectrum.js.
// Visual layers, smoothing constants, Bezier construction and frequency
// mapping are all translated line-by-line from the JS source of truth.
//
#include "Spectrum.h"
#include "../Theme.h"
#include "../../PluginProcessor.h"
#include "../../Parameters.h"
#include <cmath>

namespace kaigen::phantom
{

// ─── Constructor / destructor ────────────────────────────────────────────────

Spectrum::Spectrum(PhantomProcessor& p,
                   juce::AudioProcessorValueTreeState& vts)
    : processor(p), apvts(vts)
{
    startTimerHz(20);   // dropped from 30 — visualizers stay smooth at 20 Hz
}

Spectrum::~Spectrum()
{
    stopTimer();
}

// ─── Timer callback ──────────────────────────────────────────────────────────

void Spectrum::timerCallback()
{
    // Skip work entirely when the editor isn't on screen.
    if (! isShowing()) return;

    // Pull raw bins (pre-smoothed on the audio thread as plain float arrays;
    // single-float reads are effectively atomic on x86/ARM at this visualization rate).
    smoothBins(processor.spectrumData.data(),       smoothedIn);
    smoothBins(processor.spectrumOutputData.data(), smoothedOut);

    // Peak hold: updated alongside smoothedOut (mirrors JS data-ingest logic).
    for (int i = 0; i < kBinCount; ++i)
    {
        if (smoothedOut[(size_t) i] > peakOut[(size_t) i])
            peakOut[(size_t) i] = smoothedOut[(size_t) i];
        else
            peakOut[(size_t) i] = juce::jmax(0.0f, peakOut[(size_t) i] - kPeakDecay);
    }

    // Per-engine bins for Split mode.
    smoothBinsAtomic(processor.engineASpectrum, smoothedEngA);
    smoothBinsAtomic(processor.engineBSpectrum, smoothedEngB);

    repaint();
}

// ─── Smoothing helpers ───────────────────────────────────────────────────────

void Spectrum::smoothBins(const float* raw,
                           std::array<float, kBinCount>& smoothed) noexcept
{
    for (int i = 0; i < kBinCount; ++i)
    {
        const float v = raw[(size_t) i];
        const float coef = (v > smoothed[(size_t) i]) ? kSmoothUp : kSmoothDown;
        smoothed[(size_t) i] += (v - smoothed[(size_t) i]) * coef;
    }
}

void Spectrum::smoothBinsAtomic(
    const std::array<std::atomic<float>, kBinCount>& raw,
    std::array<float, kBinCount>& smoothed) noexcept
{
    for (int i = 0; i < kBinCount; ++i)
    {
        const float v = raw[(size_t) i].load(std::memory_order_relaxed);
        const float coef = (v > smoothed[(size_t) i]) ? kSmoothUp : kSmoothDown;
        smoothed[(size_t) i] += (v - smoothed[(size_t) i]) * coef;
    }
}

// ─── Frequency / bin mapping ─────────────────────────────────────────────────

float Spectrum::binToFreq(int bin) noexcept
{
    // Matches spectrum.js: Math.pow(10, logLo + (logHi - logLo) * (bin + 0.5) / BIN_COUNT)
    const float logLo = std::log10(kBinFreqLow);
    const float logHi = std::log10(kBinFreqHigh);
    return std::pow(10.0f, logLo + (logHi - logLo) * ((float) bin + 0.5f) / (float) kBinCount);
}

float Spectrum::freqToX(float hz, float w) noexcept
{
    // Matches spectrum.js freqToX — log10 scale.
    const float logLo = std::log10(kDisplayFreqLow);
    const float logHi = std::log10(kDisplayFreqHigh);
    const float t = (std::log10(juce::jmax(hz, kDisplayFreqLow)) - logLo)
                  / (logHi - logLo);
    return juce::jlimit(0.0f, w, t * w);
}

// ─── Curve path builders ─────────────────────────────────────────────────────

std::vector<Spectrum::Pt> Spectrum::buildCurvePoints(
    const std::array<float, kBinCount>& bins,
    float w, float h) const
{
    std::vector<Pt> pts;
    pts.reserve((size_t) kBinCount);
    for (int i = 0; i < kBinCount; ++i)
    {
        const float freq = binToFreq(i);
        if (freq < kDisplayFreqLow || freq > kDisplayFreqHigh) continue;
        const float x = freqToX(freq, w);
        const float y = h * (1.0f - bins[(size_t) i]);
        pts.push_back({ x, y });
    }
    return pts;
}

juce::Path Spectrum::makeStrokePath(const std::vector<Pt>& pts)
{
    // Mirrors spectrum.js strokeCurve:
    //   moveTo pts[0], then for i=1..n-2: quadraticCurveTo(pts[i], midpoint(pts[i],pts[i+1]))
    //   lineTo last point.
    juce::Path p;
    if (pts.size() < 2) return p;
    p.startNewSubPath(pts[0].x, pts[0].y);
    for (size_t i = 1; i + 1 < pts.size(); ++i)
    {
        const float xc = (pts[i].x + pts[i + 1].x) * 0.5f;
        const float yc = (pts[i].y + pts[i + 1].y) * 0.5f;
        p.quadraticTo(pts[i].x, pts[i].y, xc, yc);
    }
    p.lineTo(pts.back().x, pts.back().y);
    return p;
}

juce::Path Spectrum::makeFillPath(const std::vector<Pt>& pts, float h)
{
    // Mirrors spectrum.js fillCurve:
    //   moveTo (pts[0].x, h), lineTo each point, lineTo (last.x, h), closePath.
    // NOTE: straight lines, NOT bezier — JS fillCurve uses lineTo for all pts.
    juce::Path p;
    if (pts.size() < 2) return p;
    p.startNewSubPath(pts[0].x, h);
    for (const auto& pt : pts)
        p.lineTo(pt.x, pt.y);
    p.lineTo(pts.back().x, h);
    p.closeSubPath();
    return p;
}

// ─── Grid + labels ───────────────────────────────────────────────────────────

void Spectrum::drawGrid(juce::Graphics& g, float w, float h)
{
    // dB horizontal lines — JS: y = h * (-db) / 60 (60 dB full range)
    {
        const int dbLines[] = { -12, -24, -36, -48 };
        g.setColour(juce::Colour(0x0affffff)); // rgba(255,255,255,0.04)
        for (int db : dbLines)
        {
            const float y = std::round(h * (float)(-db) / 60.0f) + 0.5f;
            g.drawLine(0.0f, y, w, y, 1.0f);
        }
    }

    // Frequency vertical lines
    {
        const float majors[] = { 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f };
        g.setColour(juce::Colour(0x09ffffff)); // rgba(255,255,255,0.035) → 0x09 ≈ 9/255 ≈ 0.035
        for (float f : majors)
        {
            const float x = std::round(freqToX(f, w)) + 0.5f;
            g.drawLine(x, 0.0f, x, h, 1.0f);
        }
    }

    // Frequency labels (bottom)
    {
        struct FLabel { float hz; const char* label; };
        const FLabel freqLabels[] = {
            { 30.f,    "30"  },
            { 100.f,   "100" },
            { 300.f,   "300" },
            { 1000.f,  "1k"  },
            { 3000.f,  "3k"  },
            { 10000.f, "10k" },
        };
        const float labelFontPx = juce::jmax(9.0f, std::round(h * 0.04f));
        g.setFont(juce::Font(juce::FontOptions()
                                 .withName("Courier New")
                                 .withHeight(labelFontPx)));
        g.setColour(juce::Colour(0x38ffffff)); // rgba(255,255,255,0.22) → 0x38 ≈ 56/255

        for (const auto& fl : freqLabels)
        {
            const float x = freqToX(fl.hz, w);
            if (x < 4.0f || x > w - 4.0f) continue;
            // textAlign='center', textBaseline='bottom', fillText(label, x, h-2)
            const float textW = 36.0f; // generous width for "100"
            g.drawText(juce::String(fl.label),
                       juce::Rectangle<float>(x - textW * 0.5f, h - 2.0f - labelFontPx,
                                              textW, labelFontPx),
                       juce::Justification::centredBottom, false);
        }
    }

    // dB labels (right edge)
    {
        const int dbLabels[] = { 0, -12, -24, -36, -48 };
        const float labelFontPx = juce::jmax(9.0f, std::round(h * 0.04f));
        g.setFont(juce::Font(juce::FontOptions()
                                 .withName("Courier New")
                                 .withHeight(labelFontPx)));
        g.setColour(juce::Colour(0x38ffffff)); // same color as freq labels

        for (int db : dbLabels)
        {
            const float y = h * (float)(-db) / 60.0f;
            // textAlign='right', textBaseline='middle', fillText(db+'dB', w-3, y)
            const juce::String text = (db == 0) ? "0dB" : (juce::String(db) + "dB");
            const float textW = 40.0f;
            g.drawText(text,
                       juce::Rectangle<float>(w - 3.0f - textW, y - labelFontPx * 0.5f,
                                              textW, labelFontPx),
                       juce::Justification::centredRight, false);
        }
    }
}

// ─── Single pane renderer ────────────────────────────────────────────────────

void Spectrum::drawPane(juce::Graphics& g,
                        float paneW, float paneH,
                        const std::array<float, kBinCount>& inBins,
                        const std::array<float, kBinCount>& outBins,
                        const std::array<float, kBinCount>* peakBins,
                        float xoverHz) const
{
    drawGrid(g, paneW, paneH);

    const auto inPts  = buildCurvePoints(inBins,  paneW, paneH);
    const auto outPts = buildCurvePoints(outBins, paneW, paneH);

    // ── Layer 1: input signal — gray gradient fill + subtle stroke ────────
    if (inPts.size() >= 2)
    {
        // Fill with straight lines (matches JS fillCurve)
        {
            const auto fillPath = makeFillPath(inPts, paneH);
            juce::ColourGradient grad(juce::Colour(0x33a0a0af),  // rgba(160,160,175,0.20)
                                      0.0f, 0.0f,
                                      juce::Colour(0x0aa0a0af),  // rgba(160,160,175,0.04)
                                      0.0f, paneH,
                                      false);
            g.setGradientFill(grad);
            g.fillPath(fillPath);
        }
        // Stroke with Bezier (matches JS strokeCurve)
        {
            const auto strokePath = makeStrokePath(inPts);
            const float sw = juce::jmax(1.0f, paneH * 0.004f);
            g.setColour(juce::Colour(0x80a0a0af)); // rgba(160,160,175,0.50)
            g.strokePath(strokePath,
                         juce::PathStrokeType(sw,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
        }
    }

    // ── Layer 2: output signal — bright white fill + glowing stroke ───────
    if (outPts.size() >= 2)
    {
        // Fill
        {
            const auto fillPath = makeFillPath(outPts, paneH);
            juce::ColourGradient grad(juce::Colour(0x47ffffff),  // rgba(255,255,255,0.28)
                                      0.0f, 0.0f,
                                      juce::Colour(0x05ffffff),  // rgba(255,255,255,0.02) at bottom
                                      0.0f, paneH,
                                      false);
            // JS adds a stop at 0.5: rgba(255,255,255,0.10)
            grad.addColour(0.5, juce::Colour(0x1affffff));       // rgba(255,255,255,0.10)
            g.setGradientFill(grad);
            g.fillPath(fillPath);
        }
        // Stroke with glow
        {
            const auto strokePath = makeStrokePath(outPts);
            const float sw = juce::jmax(1.2f, paneH * 0.006f);

            // Glow pass: draw a slightly wider blurred-alpha path underneath.
            // JUCE doesn't have native shadowBlur on paths; we fake the "4px
            // white glow" with a wider semi-transparent stroke (same technique
            // used in the knob port).
            g.setColour(juce::Colour(0x26ffffff)); // ~15% white — soft glow
            g.strokePath(strokePath,
                         juce::PathStrokeType(sw + 4.0f,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

            // Sharp primary stroke
            g.setColour(juce::Colour(0xe6ffffff)); // rgba(255,255,255,0.90)
            g.strokePath(strokePath,
                         juce::PathStrokeType(sw,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
        }
    }

    // ── Layer 3: output peak-hold line ────────────────────────────────────
    if (peakBins != nullptr)
    {
        const auto peakPts = buildCurvePoints(*peakBins, paneW, paneH);
        if (peakPts.size() >= 2)
        {
            const auto strokePath = makeStrokePath(peakPts);
            g.setColour(juce::Colour(0x40ffffff)); // rgba(255,255,255,0.25)
            g.strokePath(strokePath,
                         juce::PathStrokeType(1.0f,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
        }
    }

    // ── Crossover frequency line ──────────────────────────────────────────
    if (xoverHz > 20.0f && xoverHz < 20000.0f)
    {
        const float xPos = std::round(freqToX(xoverHz, paneW));

        // Dashed line: [3, 4] pattern
        juce::Path dashPath;
        dashPath.startNewSubPath(xPos + 0.5f, 0.0f);
        dashPath.lineTo(xPos + 0.5f, paneH);

        const float dashLengths[] = { 3.0f, 4.0f };
        juce::Path dashedPath;
        juce::PathStrokeType(1.0f).createDashedStroke(
            dashedPath, dashPath, dashLengths, 2);

        // rgba(80,142,215,0.38) → alpha 0x61 = round(0.38*255), RGB 0x508ED7
        g.setColour(juce::Colour(0x61508ed7));
        g.fillPath(dashedPath);

        // Crossover label: "NNNHz" top-left of the line
        const float labelFontPx = juce::jmax(8.0f, std::round(paneH * 0.10f));
        g.setFont(juce::Font(juce::FontOptions()
                                 .withName("Courier New")
                                 .withHeight(labelFontPx)));
        g.setColour(juce::Colour(0x99508ed7)); // rgba(80,142,215,0.60) → 0x99
        const juce::String labelText = juce::String(juce::roundToInt(xoverHz)) + "Hz";
        g.drawText(labelText,
                   juce::Rectangle<float>(xPos + 3.0f, 2.0f, 80.0f, labelFontPx),
                   juce::Justification::centredLeft, false);
    }
}

// ─── paint() ────────────────────────────────────────────────────────────────

void Spectrum::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds();
    const float w = (float) bounds.getWidth();
    const float h = (float) bounds.getHeight();

    // Background: pitch-black inset surface (same as the existing bar renderer).
    Theme::paintVisualizerInset(g, bounds, 6.0f);

    // Clip to inner area so curves don't bleed over the rounded-corner border.
    g.reduceClipRegion(bounds.reduced(1));

    const auto viewMode = processor.getSpectrumViewMode();

    if (viewMode == SpectrumViewMode::Combined)
    {
        // ── Combined: full-width post-crossfader spectrum ─────────────────
        // Read active engine's phantom_threshold for the crossover line.
        float xoverHz = 0.0f;
        if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(
                apvts.getParameter(ParamID::A_PHANTOM_THRESHOLD)))
            xoverHz = p->get();

        drawPane(g, w, h, smoothedIn, smoothedOut, &peakOut, xoverHz);
    }
    else
    {
        // ── Split: side-by-side Engine A | Engine B ───────────────────────
        const float halfW = std::floor(w * 0.5f);

        // Engine A — left pane
        {
            juce::Graphics::ScopedSaveState ss(g);
            g.reduceClipRegion(juce::Rectangle<float>(0.0f, 0.0f, halfW, h).toNearestInt());
            g.addTransform(juce::AffineTransform::translation(0.0f, 0.0f));

            float xoverHz = 0.0f;
            if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(
                    apvts.getParameter(ParamID::A_PHANTOM_THRESHOLD)))
                xoverHz = p->get();

            drawPane(g, halfW, h, smoothedIn, smoothedEngA, nullptr, xoverHz);
        }

        // Engine B — right pane
        {
            juce::Graphics::ScopedSaveState ss(g);
            g.reduceClipRegion(juce::Rectangle<float>(halfW, 0.0f, w - halfW, h).toNearestInt());

            float xoverHz = 0.0f;
            if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(
                    apvts.getParameter(ParamID::B_PHANTOM_THRESHOLD)))
                xoverHz = p->get();

            // Translate so drawPane draws relative to (0,0) inside the right pane.
            g.addTransform(juce::AffineTransform::translation(halfW, 0.0f));
            drawPane(g, w - halfW, h, smoothedIn, smoothedEngB, nullptr, xoverHz);
        }

        // Center divider
        g.setColour(juce::Colour(0x1affffff)); // rgba(255,255,255,0.10)
        g.drawLine(halfW + 0.5f, 0.0f, halfW + 0.5f, h, 1.0f);

        // A / B labels (top-left / top-right of their respective panes)
        const float labelFontPx = juce::jmax(9.0f, std::round(h * 0.10f));
        g.setFont(juce::Font(juce::FontOptions()
                                 .withName("Courier New")
                                 .withHeight(labelFontPx)));
        g.setColour(juce::Colour(0xb24a90e2)); // rgba(74,144,226,0.70) → 0xb2
        g.drawText("A",
                   juce::Rectangle<float>(6.0f, 4.0f, labelFontPx * 2.0f, labelFontPx),
                   juce::Justification::centredLeft, false);
        g.drawText("B",
                   juce::Rectangle<float>(w - 6.0f - labelFontPx * 2.0f, 4.0f, labelFontPx * 2.0f, labelFontPx),
                   juce::Justification::centredRight, false);
    }
}

} // namespace kaigen::phantom
