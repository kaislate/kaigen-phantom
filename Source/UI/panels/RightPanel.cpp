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
    area.removeFromTop(28);

    // knobRowFull is the full 80px-tall strip used by all top-row sections.
    const int knobRowTop    = 28;
    const int knobRowHeight = 80;
    auto knobRow = area.removeFromTop(knobRowHeight).reduced(12, 0);
    const int knobWidth  = 80;
    const int gap        = 8;
    const int sectionGap = 24;
    const int cardPadY   = 6;    // card extends slightly above/below the knob row
    const int cardTop    = knobRowTop - cardPadY;
    const int cardHeight = knobRowHeight + cardPadY * 2;

    auto layoutKnob = [&](PhantomKnob& k) {
        k.setBounds(knobRow.removeFromLeft(knobWidth));
        knobRow.removeFromLeft(gap);
    };

    // --- Harmonic Engine ---
    const int harmonicCardX = knobRow.getX() - 4;  // slight inward padding from reduced edge
    layoutKnob(saturationKnob);
    layoutKnob(shapeKnob);
    layoutKnob(skipKnob);
    // after 3 knobs: 3*(80+8)-8 = 232px consumed, knobRow.getX() is at harmonicCardX+232
    const int harmonicCardRight = knobRow.getX() - gap + 4;
    harmonicCardBounds = juce::Rectangle<int>(harmonicCardX, cardTop,
                                              harmonicCardRight - harmonicCardX, cardHeight);

    knobRow.removeFromLeft(sectionGap);

    // --- Stereo ---
    const int stereoCardX = knobRow.getX() - 4;
    layoutKnob(widthKnob);
    const int stereoCardRight = knobRow.getX() - gap + 4;
    stereoCardBounds = juce::Rectangle<int>(stereoCardX, cardTop,
                                             stereoCardRight - stereoCardX, cardHeight);

    knobRow.removeFromLeft(sectionGap);

    // --- Levels ---
    const int levelsCardX = knobRow.getX() - 4;
    inMeter.setBounds(knobRow.removeFromLeft(14));
    knobRow.removeFromLeft(6);
    layoutKnob(inGainKnob);
    layoutKnob(outGainKnob);
    outMeter.setBounds(knobRow.removeFromLeft(14));
    const int levelsCardRight = knobRow.getX() + 4;
    levelsCardBounds = juce::Rectangle<int>(levelsCardX, cardTop,
                                             levelsCardRight - levelsCardX, cardHeight);

    // --- Advanced section: toggle button + optional mini knob row ---
    area.removeFromTop(8);
    advancedToggle.setBounds(12, 130, 100, 18);

    const int advancedCardTop = 128;
    int advancedCardBottom    = 0;

    if (advancedExpanded)
    {
        area.removeFromTop(20);  // space below toggle
        auto miniRow = area.removeFromTop(60).reduced(12, 0);
        const int miniWidth = 36;
        const int miniGap = 4;
        for (auto& mk : miniKnobs)
        {
            mk->setBounds(miniRow.removeFromLeft(miniWidth));
            miniRow.removeFromLeft(miniGap);
        }
        advancedCardBottom = 130 + 18 + 20 + 60 + 6;  // toggle + space + miniRow + padding
    }
    else
    {
        // Collapsed: skip mini-row's vertical space; visualizers shift up.
        area.removeFromTop(8);
        advancedCardBottom = 130 + 18 + 8 + 4;  // toggle + collapsed space + padding
    }

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
