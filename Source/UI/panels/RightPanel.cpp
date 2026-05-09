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
        Theme::drawEtchedText(g, title.toUpperCase(), bounds, juce::Justification::centredLeft,
                              labelFont, Theme::textOnLightLabel);
    }
}

RightPanel::RightPanel(juce::AudioProcessorValueTreeState& a, PhantomProcessor& p)
    : apvts(a),
      saturationKnob(apvts, "a_harmonic_saturation", PhantomKnob::Size::Medium, "Saturation"),
      shapeKnob     (apvts, "a_synth_step",          PhantomKnob::Size::Medium, "Shape"),
      skipKnob      (apvts, "a_synth_skip",          PhantomKnob::Size::Medium, "Skip"),
      widthKnob     (apvts, "a_stereo_width",        PhantomKnob::Size::Medium, "Width"),
      inGainKnob    (apvts, "input_gain",            PhantomKnob::Size::Medium, "In"),
      outGainKnob   (apvts, "a_output_gain",         PhantomKnob::Size::Medium, "Out"),
      inMeter       (p.peakInL),
      outMeter      (p.peakOutL),
      oscilloscope  (p),
      spectrum      (p)
{
    addAndMakeVisible(saturationKnob);
    addAndMakeVisible(shapeKnob);
    addAndMakeVisible(skipKnob);
    addAndMakeVisible(widthKnob);
    addAndMakeVisible(inGainKnob);
    addAndMakeVisible(outGainKnob);

    addAndMakeVisible(inMeter);
    addAndMakeVisible(outMeter);

    addAndMakeVisible(oscilloscope);
    addAndMakeVisible(spectrum);

    autoGainButton.setClickingTogglesState(true);
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

    // Sub-section inset cards (neumorphic dish beneath each section's controls).
    // Visualizer area is excluded — it gets the pitch-black inset in Task 5.
    if (! harmonicCardBounds.isEmpty())
        Theme::paintInsetCard(g, harmonicCardBounds, 14.0f);
    if (! stereoCardBounds.isEmpty())
        Theme::paintInsetCard(g, stereoCardBounds, 14.0f);
    if (! levelsCardBounds.isEmpty())
        Theme::paintInsetCard(g, levelsCardBounds, 14.0f);
    if (! advancedCardBounds.isEmpty())
        Theme::paintInsetCard(g, advancedCardBounds, 14.0f);

    // Section header labels (etched text, established in Task 3).
    // x-positions match the section columns laid out in resized().
    drawSectionHeader(g, juce::Rectangle<int>(harmonicCardBounds.getX() + 4, 8, 200, 16), "Harmonic Engine");
    drawSectionHeader(g, juce::Rectangle<int>(stereoCardBounds.getX()  + 4, 8, 100, 16), "Stereo");
    drawSectionHeader(g, juce::Rectangle<int>(levelsCardBounds.getX()  + 4, 8, 200, 16), "Levels");
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

    // --- Harmonic Engine: 3 medium knobs ---
    const int harmonicCardX = x - 4;
    saturationKnob.setBounds(x, knobRowTop, kMedium, kMedium);
    x += kMedium - knobOverlap;
    shapeKnob     .setBounds(x, knobRowTop, kMedium, kMedium);
    x += kMedium - knobOverlap;
    skipKnob      .setBounds(x, knobRowTop, kMedium, kMedium);
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

    // --- Levels: meter + 2 medium knobs + meter ---
    const int levelsCardX = x - 4;
    inMeter.setBounds(x, knobRowTop + (kMedium - 90) / 2, 14, 90);
    x += 14 + 6;
    inGainKnob .setBounds(x, knobRowTop, kMedium, kMedium);
    x += kMedium - knobOverlap;
    outGainKnob.setBounds(x, knobRowTop, kMedium, kMedium);
    x += kMedium + 6;
    outMeter.setBounds(x, knobRowTop + (kMedium - 90) / 2, 14, 90);
    x += 14;
    const int levelsCardRight = x + 4;
    levelsCardBounds = juce::Rectangle<int>(levelsCardX, cardTop,
                                             levelsCardRight - levelsCardX, cardHeight);

    // Reserve area below the top knob row for advanced + visualizers.
    area.removeFromTop(knobRowTop + knobRowHeight + cardPadY);

    // --- Advanced section: toggle button + optional mini knob row ---
    // Top row ends at knobRowTop + knobRowHeight (= 36 + 136 = 172). Advanced
    // toggle sits a short gap below.
    constexpr int advancedToggleY = 188;
    constexpr int advancedToggleH = 22;
    advancedToggle.setBounds(12, advancedToggleY, 110, advancedToggleH);

    const int advancedCardTop = advancedToggleY - 4;
    int advancedCardBottom    = 0;

    if (advancedExpanded)
    {
        // Mini knobs spread evenly across the available width to the right of
        // the toggle. Component natural size: 48 body + 17 px shadow pad each
        // side + 11 label = 82 wide × 93 tall.
        constexpr int miniRowY = advancedToggleY + advancedToggleH + 8;
        constexpr int miniW    = 82;
        constexpr int miniH    = 93;
        const int rowLeft  = 12 + 110 + 12;   // toggle right edge + gap
        const int rowRight = getWidth() - 12;
        const int rowAvail = rowRight - rowLeft;
        const int n = (int) miniKnobs.size();
        // Evenly distribute: give each knob `slotW` of horizontal space, allow
        // overlap if total > rowAvail.
        const int slotW = (n > 0) ? rowAvail / n : miniW;
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
