// Source/UI/panels/RightPanel.h
#pragma once
#include <array>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../widgets/PhantomKnob.h"
#include "../widgets/PhantomMiniKnob.h"
#include "../widgets/IOMeter.h"
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
    PhantomKnob trimKnob;

    // Stereo section
    PhantomKnob widthKnob;

    // Levels section
    PhantomKnob inGainKnob;
    PhantomKnob outGainKnob;

    // Levels section auto-gain toggle
    juce::TextButton autoGainButton { "Auto" };
    std::unique_ptr<juce::ButtonParameterAttachment> autoGainAttachment;

    // Levels section meters (flanking In/Out knobs)
    IOMeter inMeter;
    IOMeter outMeter;

    // Advanced panel — 14 mini knobs, all per-engine 'a_' prefix.
    std::array<std::unique_ptr<PhantomMiniKnob>, 14> miniKnobs;

    // Advanced section collapse toggle (instant, no animation).
    juce::TextButton advancedToggle { "Advanced (-)" };
    bool advancedExpanded { true };

    // Visualizers (below Advanced row)
    Oscilloscope oscilloscope;
    Spectrum spectrum;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RightPanel)
};

} // namespace kaigen::phantom
