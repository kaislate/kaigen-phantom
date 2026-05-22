// Source/UI/panels/RightPanel.h
#pragma once
#include <array>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../widgets/PhantomKnob.h"
#include "../widgets/PhantomMiniKnob.h"
#include "../widgets/IOMeter.h"
#include "../widgets/WordSelector.h"
#include "../widgets/EtchedToggle.h"
#include "../widgets/ChoiceToggle.h"
#include "../widgets/PitchDisplay.h"
#include "../widgets/LevelReadout.h"
#include "../visualizers/Oscilloscope.h"
#include "../visualizers/Spectrum.h"

class PhantomProcessor;

namespace kaigen::phantom
{

class RightPanel : public juce::Component
{
public:
    RightPanel(juce::AudioProcessorValueTreeState& apvts, PhantomProcessor& processor);
    ~RightPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** Retarget per-engine knobs under this panel. See LeftPanel::setEnginePrefix. */
    void setEnginePrefix(const juce::String& activePrefix,
                          const juce::String& mirrorPrefix = {});

private:
    juce::AudioProcessorValueTreeState& apvts;

    // Sub-section inset card bounds — computed in resized(), used in paint().
    juce::Rectangle<int> harmonicCardBounds;
    juce::Rectangle<int> stereoCardBounds;
    juce::Rectangle<int> levelsCardBounds;
    juce::Rectangle<int> advancedCardBounds;

    // Harmonic Engine section
    PhantomKnob saturationKnob;
    PhantomKnob shapeKnob;
    PhantomKnob skipKnob;

    // Levels-section auxiliary: small knob sits in a 2nd row below the
    // In/Out medium knobs (post-envelope synth gain).
    PhantomKnob trimKnob;

    // Stereo section
    PhantomKnob widthKnob;

    // Levels section
    PhantomKnob inGainKnob;
    PhantomKnob outGainKnob;

    // Reverb send — global (single-knob algorithmic reverb). Sits between
    // Trim and Out in the Levels card to mirror the WebView layout.
    PhantomKnob reverbKnob;

    // Reverb-source toggle — global bool. Lit (active) = Phantom Only;
    // unlit = Post-Engine. Sits beneath the Reverb knob in the Levels card.
    EtchedToggle reverbSourceToggle;

    // Levels section auto-gain toggle — single etched word in the same
    // visual style as the Ghost-Mode WordSelector (off = etched dark,
    // on = backlit glow).
    EtchedToggle autoGainToggle;

    // Levels section meters (flanking In/Out knobs)
    IOMeter inMeter;
    IOMeter outMeter;

    // Advanced panel — 14 mini knobs, all per-engine 'a_' prefix.
    std::array<std::unique_ptr<PhantomMiniKnob>, 14> miniKnobs;

    // Binaural quick toggle — sits above the Width mini-knob in the
    // Advanced row. Off (unlit) = binaural_mode = 0 (off). On (lit) =
    // binaural_mode = 1 (Spread). Skip choice index 2 (Voice-Split is
    // stubbed in BinauralStage).
    ChoiceToggle binauralToggle;

    // Advanced section collapse toggle (instant, no animation).
    juce::TextButton advancedToggle { "Advanced (-)" };
    bool advancedExpanded { true };

    // Visualizers (below Advanced row)
    Oscilloscope oscilloscope;
    Spectrum spectrum;

    // Pitch / note OLED display — sits between the spectrum (above) and
    // the oscilloscope (below), constrained to the oscilloscope's width.
    PitchDisplay pitchDisplay;

    // IN / OUT level numeric readouts — sits above the meter column,
    // constrained to the meter column's width.
    LevelReadout levelReadout;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RightPanel)
};

} // namespace kaigen::phantom
