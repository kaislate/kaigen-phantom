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
    WordSelector  warpToggle;       // 2-way choice: Off/Complex (warp mode)
    juce::TextButton folderButton;  // glyph: folder icon

    // Controls row.
    juce::ComboBox  rootNoteCombo;
    juce::ComboBox  quantizeCombo;
    EtchedToggle    loopToggle;
    EtchedToggle    sliceToggle;
    EtchedToggle    autoSliceToggle;
    EtchedToggle    reverseToggle;
    EtchedToggle    fixVelToggle;
    PhantomMiniKnob gainKnob;
    juce::Slider    attackSlider, decaySlider, sustainSlider, releaseSlider, xfadeSlider, velocitySlider;

    // APVTS attachments.
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>   rootAttach, quantizeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>     attackAttach, decayAttach, sustainAttach, releaseAttach, xfadeAttach, velocityAttach;
    // (sourceToggle, loopToggle, gainKnob already have their own
    // attachments via their WordSelector/EtchedToggle/PhantomMiniKnob ctors.)

    // Waveform thumbnail cached as an Image so paint is cheap.
    juce::Image waveformImage;

    // Start/end markers + slice handles drawn over the waveform.
    // Updated by mouse drag → APVTS or PhantomSampler.setSliceTable.
    enum class DragTarget { None, Start, End, Slice };
    DragTarget activeDrag { DragTarget::None };
    int        draggedSliceIdx { -1 };   // valid when activeDrag == Slice
    juce::Rectangle<float> waveformBoundsCache;   // last paint's waveform rect (for hit-tests)
    float fractionAtX(float xpx) const noexcept;  // px → 0..1 inside waveformBoundsCache

    // Sample-position helpers (depend on the loaded sample's length).
    int  totalSourceSamples() const noexcept;
    int  sampleAtX(float xpx) const noexcept;     // px → source-sample index

    // Hit-tests the slice handles (returns -1 if no hit). Slice 0 is
    // never grabbable (always at sample 0).
    int hitTestSliceHandle(juce::Point<int> p) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SamplerStrip)
};

} // namespace kaigen::phantom
