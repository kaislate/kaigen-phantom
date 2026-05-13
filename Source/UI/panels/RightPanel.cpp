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
      trimKnob      (apvts, "a_synth_trim",          PhantomKnob::Size::Medium, "Trim"),
      widthKnob     (apvts, "a_stereo_width",        PhantomKnob::Size::Medium, "Width"),
      inGainKnob    (apvts, "input_gain",            PhantomKnob::Size::Medium, "In"),
      outGainKnob   (apvts, "a_output_gain",         PhantomKnob::Size::Medium, "Out"),
      inMeter       (p.peakInL),
      outMeter      (p.peakOutL),
      oscilloscope  (p),
      spectrum      (p, a)
{
    addAndMakeVisible(saturationKnob);
    addAndMakeVisible(shapeKnob);
    addAndMakeVisible(skipKnob);
    addAndMakeVisible(trimKnob);

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

    autoGainButton.setClickingTogglesState(true);
    autoGainButton.getProperties().set("phantom-style", "header-raised");
    addAndMakeVisible(autoGainButton);
    if (auto* param = apvts.getParameter("input_gain_auto"))
        autoGainAttachment = std::make_unique<juce::ButtonParameterAttachment>(*param, autoGainButton);
    else
        jassertfalse;  // unknown paramID: typo or stale reference

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
    constexpr int knobOverlap   = 24;   // adjacent knobs overlap their shadow halos
    constexpr int sectionGap    = 16;
    constexpr int cardPadY      = 4;
    const int cardTop = knobRowTop - 24;   // card top above the header label
    const int cardHeight = knobRowHeight + 24 + cardPadY * 2;

    int x = 12;

    // --- Harmonic Engine: 4 medium knobs (Saturation / Shape / Skip / Trim) ---
    const int harmonicCardX = x - 4;
    saturationKnob.setBounds(x, knobRowTop, kMedium, kMedium);
    x += kMedium - knobOverlap;
    shapeKnob     .setBounds(x, knobRowTop, kMedium, kMedium);
    x += kMedium - knobOverlap;
    skipKnob      .setBounds(x, knobRowTop, kMedium, kMedium);
    x += kMedium - knobOverlap;
    trimKnob      .setBounds(x, knobRowTop, kMedium, kMedium);
    x += kMedium;
    const int harmonicCardRight = x + 4;
    harmonicCardBounds = juce::Rectangle<int>(harmonicCardX, cardTop,
                                              harmonicCardRight - harmonicCardX, cardHeight);

    x += sectionGap;

    // --- Stereo: 1 medium knob ---
    const int stereoCardX = x - 4;
    widthKnob.setBounds(x, knobRowTop, kMedium, kMedium);
    x += kMedium;
    const int stereoCardRight = x + 4;
    stereoCardBounds = juce::Rectangle<int>(stereoCardX, cardTop,
                                             stereoCardRight - stereoCardX, cardHeight);

    x += sectionGap;

    // --- Levels: meter + 2 medium knobs + meter ───────────────────────
    // Meters sit inside their own 8-wide depressed slots, moved inward
    // by 2 px each (closer to the centre of the section) and tightened
    // against the In/Out knobs.
    const int levelsCardX = x - 4;
    constexpr int kMeterW = 12;          // wider (was 8)
    constexpr int kMeterInset = 8;       // meters sit further inward toward the knobs
    inMeter.setBounds(x + kMeterInset, knobRowTop + (kMedium - 90) / 2, kMeterW, 90);
    x += kMeterInset + kMeterW + 2;
    inGainKnob .setBounds(x, knobRowTop, kMedium, kMedium);
    x += kMedium - knobOverlap;
    outGainKnob.setBounds(x, knobRowTop, kMedium, kMedium);
    x += kMedium + 2;
    outMeter.setBounds(x, knobRowTop + (kMedium - 90) / 2, kMeterW, 90);
    x += kMeterW + kMeterInset;
    const int levelsCardRight = x + 4;
    levelsCardBounds = juce::Rectangle<int>(levelsCardX, cardTop,
                                             levelsCardRight - levelsCardX, cardHeight);

    // Reserve area below the top knob row for advanced + visualizers.
    area.removeFromTop(knobRowTop + knobRowHeight + cardPadY);

    // --- Advanced section: toggle ABOVE the mini knob row ---
    // Add ~32 px of breathing room below the top row before the Advanced
    // section starts, matching the photo reference.
    constexpr int advancedToggleY = 220;          // was 182
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

    // Auto button — positioned next to the "Levels" section header.
    autoGainButton.setBounds(580, 6, 40, 18);
}

} // namespace kaigen::phantom
