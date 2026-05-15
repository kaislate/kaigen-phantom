// Source/UI/panels/RightPanel.cpp
#include "RightPanel.h"
#include "../Theme.h"
#include "../../PluginProcessor.h"

namespace kaigen::phantom
{

namespace
{
    void drawSectionHeader(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title)
    {
        const auto labelFont = juce::Font(juce::FontOptions("Space Grotesk", 10.0f, juce::Font::bold))
                                   .withExtraKerningFactor(0.25f);
        Theme::drawEtchedText(g, title.toUpperCase(), bounds, juce::Justification::centred,
                              labelFont, Theme::textOnLightLabel);
    }
}

void RightPanel::setEnginePrefix(const juce::String& activePrefix,
                                    const juce::String& mirrorPrefix)
{
    // Skip inGainKnob — input_gain is global, not per-engine; its
    // PhantomKnob.isPerEngine() returns false and setEnginePrefix is a no-op.
    for (auto* k : { &saturationKnob, &shapeKnob, &skipKnob, &trimKnob,
                     &widthKnob, &outGainKnob })
        k->setEnginePrefix(activePrefix, mirrorPrefix);

    // Advanced-row mini knobs — all per-engine.
    for (auto& mk : miniKnobs)
        if (mk) mk->setEnginePrefix(activePrefix, mirrorPrefix);

    // WordSelector / ToggleGroup choice-param widgets still bound to a_*;
    // their setEnginePrefix lands in the next sub-commit.
}

RightPanel::RightPanel(juce::AudioProcessorValueTreeState& a, PhantomProcessor& p)
    : apvts(a),
      saturationKnob(apvts, "a_harmonic_saturation", PhantomKnob::Size::Medium, "Saturation"),
      shapeKnob     (apvts, "a_synth_step",          PhantomKnob::Size::Medium, "Shape"),
      skipKnob      (apvts, "a_synth_skip",          PhantomKnob::Size::Medium, "Skip"),
      trimKnob      (apvts, "a_synth_trim",          PhantomKnob::Size::Small,  "Trim"),
      widthKnob     (apvts, "a_stereo_width",        PhantomKnob::Size::Medium, "Width"),
      inGainKnob    (apvts, "input_gain",            PhantomKnob::Size::Medium, "PKE"),
      outGainKnob   (apvts, "a_output_gain",         PhantomKnob::Size::Medium, "Out"),
      inMeter       (p.peakInL,  p.peakInR),
      outMeter      (p.peakOutL, p.peakOutR),
      autoGainToggle(apvts, "input_gain_auto", { "Manual", "Auto" }),
      oscilloscope  (p),
      spectrum      (p, a)
{
    addAndMakeVisible(saturationKnob);
    addAndMakeVisible(shapeKnob);
    addAndMakeVisible(skipKnob);
    addAndMakeVisible(trimKnob);

    // PKE = "Psycho-Kinetic Energy" detector — Ghostbusters Easter egg.
    // Functionally it's the synth-detection gain: scales the signal that
    // the WaveletSynth's pitch / gate / boost detectors see, without
    // affecting the audio path's output level. Higher values = engine
    // tracks quieter material; doesn't make anything louder.

    // Shape knob OLED: small waveform in the upper portion (sine→square
    // morph), numeric value in the lower portion. The OLED itself is a
    // CIRCLE, so all content stays inside an inscribed-square safe area
    // (~65 % of the OLED diameter) to avoid clipping at the edges.
    shapeKnob.paintOLEDContents = [](juce::Graphics& g,
                                      juce::Rectangle<float> oled,
                                      float v01,
                                      const juce::String& valueText)
    {
        const float diameter = juce::jmin(oled.getWidth(), oled.getHeight());
        const float safeW    = diameter * 0.55f;
        const float cx       = oled.getCentreX();
        const float cy       = oled.getCentreY();

        // Upper portion — waveform centred above the OLED midline.
        {
            const float waveW    = safeW;
            const float waveAmp  = diameter * 0.075f;   // peak Y excursion
            const float waveCY   = cy - diameter * 0.16f;
            const float xL       = cx - waveW * 0.5f;
            const float blend    = juce::jlimit(0.0f, 1.0f, v01);

            constexpr int kSamples = 40;
            juce::Path wave;
            for (int i = 0; i < kSamples; ++i)
            {
                const float t     = (float) i / (float) (kSamples - 1);
                const float phase = t * juce::MathConstants<float>::twoPi;
                const float s     = std::sin(phase);
                const float sq    = (s > 0.0f) ? 1.0f : (s < 0.0f) ? -1.0f : 0.0f;
                const float y     = s * (1.0f - blend) + sq * blend;
                const float px    = xL + t * waveW;
                const float py    = waveCY - y * waveAmp;
                if (i == 0) wave.startNewSubPath(px, py);
                else        wave.lineTo(px, py);
            }
            g.setColour(juce::Colour(0xe6FFFFFF));
            g.strokePath(wave, juce::PathStrokeType(1.0f,
                                                      juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }

        // Lower portion — formatted value at the default OLED text size
        // for this knob variant (Medium = 12 px). Sized off the diameter
        // so it stays proportional if the knob's size changes.
        {
            const float textPx = diameter * 0.16f;      // ~12 px on a ~75 px OLED
            const auto textRect = juce::Rectangle<float>(
                cx - safeW * 0.5f, cy + diameter * 0.04f,
                safeW,             diameter * 0.32f);
            g.setFont(juce::FontOptions("Courier New", textPx, juce::Font::bold));
            g.setColour(juce::Colour(0xe6FFFFFF));
            g.drawText(valueText, textRect, juce::Justification::centred, false);
        }
    };

    addAndMakeVisible(widthKnob);
    addAndMakeVisible(inGainKnob);
    addAndMakeVisible(outGainKnob);

    addAndMakeVisible(inMeter);
    addAndMakeVisible(outMeter);

    addAndMakeVisible(oscilloscope);
    addAndMakeVisible(spectrum);

    addAndMakeVisible(autoGainToggle);

    {
        static constexpr struct { const char* paramID; const char* label; } miniDefs[] = {
            { "a_synth_duty",            "Push"      },
            { "a_synth_h1",              "H1"        },
            { "a_synth_sub",             "Sub"       },
            { "a_synth_wavelet_length",  "Length"    },
            { "a_synth_gate_threshold",  "Gate"      },
            { "a_synth_min_samples",     "Min"       },
            { "a_synth_max_samples",     "Max"       },
            { "a_tracking_speed",        "Track"     },
            { "a_punch_amount",          "Amount"    },
            { "a_synth_boost_threshold", "Threshold" },
            { "a_synth_boost_amount",    "Boost"     },
            { "a_env_attack_ms",         "Attack"    },
            { "a_env_release_ms",        "Release"   },
            { "a_binaural_width",        "Width"     },
        };
        static_assert(sizeof(miniDefs) / sizeof(miniDefs[0]) == 14, "14 mini knobs expected");

        for (size_t i = 0; i < miniKnobs.size(); ++i)
        {
            miniKnobs[i] = std::make_unique<PhantomMiniKnob>(apvts, miniDefs[i].paramID, miniDefs[i].label);
            addAndMakeVisible(*miniKnobs[i]);
        }
    }

    advancedToggle.setClickingTogglesState(false);
    advancedToggle.onClick = [this] {
        advancedExpanded = !advancedExpanded;
        advancedToggle.setButtonText(advancedExpanded ? "Advanced (-)" : "Advanced (+)");
        for (auto& mk : miniKnobs)
            mk->setVisible(advancedExpanded);
        resized();  // recompute visualizer bounds based on new state
        repaint();
    };
    addAndMakeVisible(advancedToggle);
}

RightPanel::~RightPanel() = default;

void RightPanel::paint(juce::Graphics& g)
{
    // Silver panel surface — replaces the flat panelBg fill.
    Theme::paintSilverPanel(g, getLocalBounds());

    // Sub-section inset cards with title-notch at top center.
    // Notch widths sized to fully encompass each title with breathing room.
    if (! harmonicCardBounds.isEmpty())
        Theme::paintInsetCardWithNotch(g, harmonicCardBounds, 14.0f, 180.0f, 16.0f);
    if (! stereoCardBounds.isEmpty())
        Theme::paintInsetCardWithNotch(g, stereoCardBounds, 14.0f, 130.0f, 16.0f);
    if (! levelsCardBounds.isEmpty())
        Theme::paintInsetCardWithNotch(g, levelsCardBounds, 14.0f, 130.0f, 16.0f);
    if (! advancedCardBounds.isEmpty())
        Theme::paintInsetCardWithNotch(g, advancedCardBounds, 14.0f, 130.0f, 16.0f);

    // Section titles — centered IN the flat-bottomed notch.
    drawSectionHeader(g, juce::Rectangle<int>(harmonicCardBounds.getX(), harmonicCardBounds.getY() + 1, harmonicCardBounds.getWidth(), 14), "Harmonic Engine");
    drawSectionHeader(g, juce::Rectangle<int>(stereoCardBounds.getX(),   stereoCardBounds.getY()   + 1, stereoCardBounds.getWidth(),   14), "Stereo");
    drawSectionHeader(g, juce::Rectangle<int>(levelsCardBounds.getX(),   levelsCardBounds.getY()   + 1, levelsCardBounds.getWidth(),   14), "Levels");
    drawSectionHeader(g, juce::Rectangle<int>(advancedCardBounds.getX(), advancedCardBounds.getY() + 1, advancedCardBounds.getWidth(), 14), "Advanced");
}

void RightPanel::resized()
{
    auto area = getLocalBounds();

    // Knob component natural size (body 88 + shadow padding 24*2 = 136).
    constexpr int kMedium = 136;

    // Top row Y: pushed down so the knob's 32-px shadow halo isn't clipped
    // by the panel's top edge.
    constexpr int knobRowTop    = 36;
    constexpr int knobRowHeight = kMedium;
    constexpr int sectionGap    = 8;    // gap between HE/Stereo and Stereo/Levels (was 16)
    constexpr int cardPadY      = 4;
    const int cardTop = knobRowTop - 24;   // card top above the header label
    const int cardHeight = knobRowHeight + 24 + cardPadY * 2;

    // ── Layout strategy ────────────────────────────────────────────────
    // Stereo is centred horizontally with the panel (so it aligns with
    // the Advanced card's centre). HE and Levels become symmetric
    // mirror partners with equal widths and equal 16 px gaps to Stereo.
    constexpr int kAdvancedSidePad = 8;   // Advanced card sits at x=8 .. getWidth()-8
    const int panelCentre   = getWidth() / 2;
    const int stereoCardW   = kMedium + 8;             // 1 medium + 4 pad each side
    const int stereoCardX   = panelCentre - stereoCardW / 2;
    const int stereoCardRight = stereoCardX + stereoCardW;

    const int harmonicCardX     = kAdvancedSidePad;
    const int harmonicCardRight = stereoCardX - sectionGap;
    const int harmonicCardW     = harmonicCardRight - harmonicCardX;

    const int levelsCardX     = stereoCardRight + sectionGap;
    const int levelsCardRight = getWidth() - kAdvancedSidePad;
    const int levelsCardW     = levelsCardRight - levelsCardX;

    constexpr int kMeterW          = 28;   // wider bars — each L/R bar is ~13 px
    constexpr int kMedSmallOverlap = 30;
    constexpr int kSmallSide       = 90;
    constexpr int kKnobShadowPad   = 24;
    constexpr int kMeterEdgeMargin = 6;    // breathing room from the section card edge
    constexpr int kMeterKnobGap    = -8;   // negative → meter shifts further into the shadow halo

    // --- Harmonic Engine: 3 medium knobs, fit into harmonicCardW ──────
    // Adaptive overlap so 3 mediums fit the card's content area exactly.
    // contentW = harmonicCardW - 8 (4 px pad each side).
    // contentW = kMedium + 2 * (kMedium - heOverlap)
    // => heOverlap = (3 * kMedium - contentW) / 2
    {
        const int contentW = harmonicCardW - 8;
        const int heOverlap = juce::jmax(24, (3 * kMedium - contentW) / 2);
        int hx = harmonicCardX + 4;
        saturationKnob.setBounds(hx, knobRowTop, kMedium, kMedium);
        hx += kMedium - heOverlap;
        shapeKnob     .setBounds(hx, knobRowTop, kMedium, kMedium);
        hx += kMedium - heOverlap;
        skipKnob      .setBounds(hx, knobRowTop, kMedium, kMedium);
    }
    harmonicCardBounds = juce::Rectangle<int>(harmonicCardX, cardTop,
                                              harmonicCardW, cardHeight);

    // --- Stereo: 1 medium knob centred in the card ────────────────────
    widthKnob.setBounds(stereoCardX + 4, knobRowTop, kMedium, kMedium);
    stereoCardBounds = juce::Rectangle<int>(stereoCardX, cardTop,
                                             stereoCardW, cardHeight);

    // --- Levels: In + Trim + Out, content centred in the card ─────────
    // Meters tuck inside the In/Out shadow halos so the section can
    // remain narrow without losing visible body sizes.
    // The In/Out/Trim trio sits in the centre of the card; the meters sit
    // OUTSIDE the knobs' shadow halos with a small margin from the card edge.
    const int levelsContentW = kMedium
                              + (kSmallSide - kMedSmallOverlap)
                              + (kMedium    - kMedSmallOverlap);
    const int inGainX = levelsCardX + (levelsCardW - levelsContentW) / 2;
    inGainKnob.setBounds(inGainX, knobRowTop, kMedium, kMedium);
    // Left meter — clear of In's left shadow halo + clear of card edge.
    inMeter.setBounds(juce::jmax(levelsCardX + kMeterEdgeMargin,
                                  inGainX - kMeterKnobGap - kMeterW),
                       knobRowTop + (kMedium - 90) / 2, kMeterW, 90);

    const int trimX = inGainX + kMedium - kMedSmallOverlap;
    const int trimY = knobRowTop + (kMedium - kSmallSide) / 2 + 1;
    trimKnob.setBounds(trimX, trimY, kSmallSide, kSmallSide);

    const int outGainX = trimX + kSmallSide - kMedSmallOverlap;
    outGainKnob.setBounds(outGainX, knobRowTop, kMedium, kMedium);
    // Right meter — mirror of the left, outside Out's right shadow halo.
    outMeter.setBounds(juce::jmin(levelsCardRight - kMeterEdgeMargin - kMeterW,
                                   outGainX + kMedium + kMeterKnobGap),
                        knobRowTop + (kMedium - 90) / 2, kMeterW, 90);

    levelsCardBounds = juce::Rectangle<int>(levelsCardX, cardTop,
                                             levelsCardW, cardHeight);

    // Reserve area below the top knob row for advanced + visualizers.
    area.removeFromTop(knobRowTop + knobRowHeight + cardPadY);

    // --- Advanced section: toggle ABOVE the mini knob row ---
    constexpr int advancedToggleY = 220;
    constexpr int advancedToggleH = 20;
    advancedToggle.setBounds(12, advancedToggleY, 100, advancedToggleH);

    const int advancedCardTop = advancedToggleY - 4;
    int advancedCardBottom    = 0;

    if (advancedExpanded)
    {
        // Mini knob row spans the FULL panel width (toggle is above it now).
        // Component natural size: 48 body + 17 shadow pad × 2 + 11 label
        //   = 82 wide × 93 tall.
        // 14-px gap below toggle so the mini knob's TOP shadow halo (17 px)
        // clears the title-notch dip area (16 px deep) by ~7 px.
        constexpr int miniRowY = advancedToggleY + advancedToggleH + 14;
        constexpr int miniW    = 82;
        constexpr int miniH    = 93;
        const int rowLeft  = 12;
        const int rowRight = getWidth() - 12;
        const int rowAvail = rowRight - rowLeft;
        const int n = (int) miniKnobs.size();

        // Evenly distribute knobs across the available width. Allow modest
        // shadow overlap (slotW < miniW is fine — bodies stay separate).
        const int slotW = (n > 0) ? (rowAvail - miniW) / juce::jmax(1, n - 1) : 0;
        int mx = rowLeft;
        for (auto& mk : miniKnobs)
        {
            mk->setBounds(mx, miniRowY, miniW, miniH);
            mx += slotW;
        }
        advancedCardBottom = miniRowY + miniH + 6;
    }
    else
    {
        advancedCardBottom = advancedToggleY + advancedToggleH + 8;
    }
    area.removeFromTop(advancedCardBottom - knobRowTop - knobRowHeight - cardPadY);

    advancedCardBounds = juce::Rectangle<int>(8, advancedCardTop,
                                               getWidth() - 16,
                                               advancedCardBottom - advancedCardTop);

    // Visualizers below Advanced row — excluded from inset cards (Task 5).
    area.removeFromTop(12);
    auto vizArea = area.reduced(12, 0);
    oscilloscope.setBounds(vizArea.removeFromTop(120));
    vizArea.removeFromTop(8);
    spectrum    .setBounds(vizArea.removeFromTop(280));

    // Auto/Manual toggle — sits just above the PKE (In) knob, centred on
    // the knob's body. Same WordSelector visual style as the Ghost-Mode
    // (Replace / Combine / Phantom Only) toggle in LeftPanel.
    {
        const int  inBodyCx = inGainKnob.getX() + kMedium / 2;
        constexpr int kAutoW = 96;
        constexpr int kAutoH = 16;
        const int autoY = inGainKnob.getY() - kAutoH - 2;
        autoGainToggle.setBounds(inBodyCx - kAutoW / 2, autoY, kAutoW, kAutoH);
    }
}

} // namespace kaigen::phantom
