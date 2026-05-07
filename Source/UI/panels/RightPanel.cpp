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
        g.setColour(Theme::textSecondary);
        g.setFont(juce::FontOptions("Space Grotesk", 11.0f, juce::Font::bold));
        g.drawText(title, bounds.toFloat(), juce::Justification::centredLeft, false);
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
}

RightPanel::~RightPanel() = default;

void RightPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::panelBg);

    drawSectionHeader(g, juce::Rectangle<int>(12,   8, 200, 16), "Harmonic Engine");
    drawSectionHeader(g, juce::Rectangle<int>(310,  8, 100, 16), "Stereo");
    drawSectionHeader(g, juce::Rectangle<int>(420,  8, 200, 16), "Levels");
    drawSectionHeader(g, juce::Rectangle<int>(12, 130, 200, 16), "Advanced");
}

void RightPanel::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(28);

    auto knobRow = area.removeFromTop(80).reduced(12, 0);
    const int knobWidth = 80;
    const int gap       = 8;
    const int sectionGap = 24;

    auto layoutKnob = [&](PhantomKnob& k) {
        k.setBounds(knobRow.removeFromLeft(knobWidth));
        knobRow.removeFromLeft(gap);
    };

    // Harmonic Engine
    layoutKnob(saturationKnob);
    layoutKnob(shapeKnob);
    layoutKnob(skipKnob);

    knobRow.removeFromLeft(sectionGap);

    // Stereo
    layoutKnob(widthKnob);

    knobRow.removeFromLeft(sectionGap);

    // Levels: meter, In knob, Out knob, meter
    inMeter.setBounds(knobRow.removeFromLeft(14));
    knobRow.removeFromLeft(6);
    layoutKnob(inGainKnob);
    layoutKnob(outGainKnob);
    outMeter.setBounds(knobRow.removeFromLeft(14));

    // Advanced mini-knob row
    area.removeFromTop(20);
    auto miniRow = area.removeFromTop(60).reduced(12, 0);
    const int miniWidth = 36;
    const int miniGap = 4;
    for (auto& mk : miniKnobs)
    {
        mk->setBounds(miniRow.removeFromLeft(miniWidth));
        miniRow.removeFromLeft(miniGap);
    }

    // Visualizers below Advanced row.
    area.removeFromTop(12);
    auto vizArea = area.reduced(12, 0);
    oscilloscope.setBounds(vizArea.removeFromTop(120));
    vizArea.removeFromTop(8);
    spectrum    .setBounds(vizArea.removeFromTop(280));

    // Auto button — positioned next to the "Levels" section header (which sits at x=420 in paint()).
    autoGainButton.setBounds(580, 6, 40, 18);
}

} // namespace kaigen::phantom
