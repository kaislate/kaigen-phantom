// Source/UI/widgets/SamplerStrip.h
#pragma once
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "WordSelector.h"
#include "PhantomMiniKnob.h"
#include "EtchedToggle.h"

class PhantomProcessor;

namespace kaigen::phantom
{

// SamplerStrip - the 3-row sampler widget sitting between the spectrum
// graph (above) and the oscilloscope (below) in RightPanel.
//
//   +-------------------------------------------------------------+
//   |  Source: [Input][Sidechain][Sampler]   mysample.wav   [...] |  24 px
//   |  ......waveform + playhead..............................   |  60 px
//   |  ROOT [C3 v]  LOOP [o]  GAIN [o]  A[-] D[-] S[-] R[-]       |  40 px
//   +-------------------------------------------------------------+
class SamplerStrip : public juce::Component,
                     public juce::FileDragAndDropTarget,
                     private juce::Timer
{
public:
    SamplerStrip(PhantomProcessor& proc, juce::AudioProcessorValueTreeState& apvts);
    ~SamplerStrip() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    void timerCallback() override;     // ~30 Hz playhead refresh
    void pickAndLoadFile();
    void loadSampleAsync(const juce::File& file);
    void rebuildWaveformThumbnail();   // call after a successful load

    enum class LoadState { Idle, Loading, Error };
    LoadState loadState { LoadState::Idle };
    juce::String errorMessage;

    // Monotonic counter bumped on every load kick - each launched
    // background thread captures the value at launch and checks it
    // matches the current value before applying its result, so a
    // second load started while the first is still decoding wins
    // (the slower first one's callAsync becomes a no-op).
    std::atomic<int> loadGeneration { 0 };

    PhantomProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    // Header row.
    WordSelector  sourceToggle;     // 3-way choice: Input/Sidechain/Sampler
    juce::TextButton folderButton;  // glyph: folder icon

    // Controls row.
    juce::ComboBox  rootNoteCombo;
    EtchedToggle    loopToggle;
    PhantomMiniKnob gainKnob;
    juce::Slider    attackSlider, decaySlider, sustainSlider, releaseSlider;

    // APVTS attachments.
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>   rootAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>     attackAttach, decayAttach, sustainAttach, releaseAttach;
    // (sourceToggle, loopToggle, gainKnob already have their own
    // attachments via their WordSelector/EtchedToggle/PhantomMiniKnob ctors.)

    // Waveform thumbnail cached as an Image so paint is cheap.
    juce::Image waveformImage;

    // Start/end markers drawn as triangle handles over the waveform.
    // Updated by mouse drag → APVTS::setValueNotifyingHost. Hit-testing
    // tracks which marker (if any) is currently being dragged.
    enum class DragTarget { None, Start, End };
    DragTarget activeDrag { DragTarget::None };
    juce::Rectangle<float> waveformBoundsCache;   // last paint's waveform rect (for hit-tests)
    float fractionAtX(float xpx) const noexcept;  // px → 0..1 inside waveformBoundsCache

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SamplerStrip)
};

} // namespace kaigen::phantom
