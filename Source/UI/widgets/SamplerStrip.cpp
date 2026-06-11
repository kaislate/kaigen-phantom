// Source/UI/widgets/SamplerStrip.cpp
#include "SamplerStrip.h"
#include "../../PluginProcessor.h"
#include "../../Parameters.h"

namespace kaigen::phantom
{

namespace
{
    constexpr int kHeaderH       = 24;
    constexpr int kMarkerStripH  = 16;   // hosts slice MIDI-note labels + triangle handles in slice mode
    constexpr int kWaveformH     = 60;
    constexpr int kControlsH     = 90;   // tall enough for full PhantomMiniKnob + labeled ADSR sliders

    juce::String midiNoteName(int n)
    {
        static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        const int octave = (n / 12) - 2;     // MIDI 0 = C-2 convention
        return juce::String(names[n % 12]) + juce::String(octave);
    }
}

SamplerStrip::SamplerStrip(PhantomProcessor& p, juce::AudioProcessorValueTreeState& a)
    : processor(p), apvts(a),
      sourceToggle(a, ParamID::INPUT_SOURCE,
                    juce::StringArray{ "Input", "Sidechain", "Sampler" }, 1),
      warpToggle(a, ParamID::SAMPLER_WARP_MODE,
                  juce::StringArray{ "Off", "Complex" }, 1),
      loopToggle(a, ParamID::SAMPLER_LOOP, "Loop"),
      sliceToggle(a, ParamID::SAMPLER_SLICE_MODE, "Slice"),
      autoSliceToggle(a, ParamID::SAMPLER_AUTO_SLICE, "Auto"),
      reverseToggle(a, ParamID::SAMPLER_REVERSE, "Rev"),
      fixVelToggle(a, ParamID::SAMPLER_VEL_FIXED, "FixVel"),
      gainKnob(a, ParamID::SAMPLER_GAIN, "Gain", /*darkBackground=*/true)
{
    addAndMakeVisible(sourceToggle);
    addAndMakeVisible(warpToggle);

    folderButton.setButtonText("...");
    folderButton.getProperties().set("phantom-style", "header-glyph");
    folderButton.onClick = [this] { pickAndLoadFile(); };
    addAndMakeVisible(folderButton);

    for (int n = 0; n <= 127; ++n)
        rootNoteCombo.addItem(midiNoteName(n), n + 1);   // ComboBox ids must be >= 1
    rootAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, ParamID::SAMPLER_ROOT_NOTE, rootNoteCombo);
    addAndMakeVisible(rootNoteCombo);

    // Quantize combo — text mirrors the APVTS choice labels.
    for (auto& label : juce::StringArray{ "Off", "1/4", "1/8", "1/16", "1/32" })
        quantizeCombo.addItem(label, quantizeCombo.getNumItems() + 1);
    quantizeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, ParamID::SAMPLER_QUANTIZE, quantizeCombo);
    addAndMakeVisible(quantizeCombo);

    addAndMakeVisible(loopToggle);
    addAndMakeVisible(sliceToggle);
    addAndMakeVisible(autoSliceToggle);
    addAndMakeVisible(reverseToggle);
    addAndMakeVisible(fixVelToggle);
    addAndMakeVisible(gainKnob);

    auto setupSlider = [&](juce::Slider& s, const juce::String& paramId,
                            std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attach)
    {
        s.setSliderStyle(juce::Slider::LinearVertical);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, paramId, s);
        addAndMakeVisible(s);
    };
    setupSlider(attackSlider,  ParamID::SAMPLER_A, attackAttach);
    setupSlider(decaySlider,   ParamID::SAMPLER_D, decayAttach);
    setupSlider(sustainSlider, ParamID::SAMPLER_S, sustainAttach);
    setupSlider(releaseSlider, ParamID::SAMPLER_R, releaseAttach);
    setupSlider(xfadeSlider,    ParamID::SAMPLER_LOOP_XFADE, xfadeAttach);
    setupSlider(velocitySlider, ParamID::SAMPLER_VEL_VALUE,  velocityAttach);

    // If the plugin opened with a sample already loaded (restored from
    // plugin state by PluginProcessor::setStateInformation before the
    // editor exists), the thumbnail wasn't built — build it now from
    // the processor's AudioThumbnail which setSampleFromBytes seeded.
    if (processor.getSampleFilename().isNotEmpty()
        && processor.getPhantomSampler().hasSample())
    {
        rebuildWaveformThumbnail();
    }

    // 30 Hz playhead refresh while voices are active.
    startTimerHz(30);
}

SamplerStrip::~SamplerStrip() = default;

void SamplerStrip::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff121419));

    auto area = getLocalBounds();
    auto headerArea  = area.removeFromTop(kHeaderH);
    auto markerStrip = area.removeFromTop(kMarkerStripH).reduced(6, 0);
    auto waveformArea = area.removeFromTop(kWaveformH).reduced(6, 4);
    // controlsArea is the remainder; positioned in resized().

    // Header - filename label drawn between the warp selector and folder button.
    auto filenameRect = headerArea.reduced(8, 0).withTrimmedLeft(220 + 100);   // skip source + warp
    filenameRect.removeFromRight(40);   // skip the folderButton width
    g.setColour(juce::Colour(processor.getSampleFilename().isEmpty()
        ? 0x66ffffff : 0xffd0d2d4));
    g.setFont(juce::FontOptions(12.0f));
    const auto fn = processor.getSampleFilename().isEmpty()
                        ? juce::String("(no sample loaded)")
                        : processor.getSampleFilename();
    g.drawText(fn, filenameRect, juce::Justification::centredRight, true);

    // Waveform area - bg + image + playhead + placeholder text.
    g.setColour(juce::Colour(0xff0a0c12));
    g.fillRect(waveformArea);

    // Cache the float-form bounds for mouse hit-testing in mouseDown/Drag.
    const_cast<SamplerStrip*>(this)->waveformBoundsCache = waveformArea.toFloat();

    if (waveformImage.isValid())
    {
        g.drawImage(waveformImage, waveformArea.toFloat(),
                    juce::RectanglePlacement::stretchToFit);

        // ── Start/end markers + dim overlay outside the active region ──
        const float startFrac = juce::jlimit(0.0f, 1.0f,
            apvts.getRawParameterValue(ParamID::SAMPLER_START)->load());
        const float endFrac   = juce::jlimit(0.0f, 1.0f,
            apvts.getRawParameterValue(ParamID::SAMPLER_END)->load());
        const auto wf = waveformArea.toFloat();
        const float startX = wf.getX() + startFrac * wf.getWidth();
        const float endX   = wf.getX() + endFrac   * wf.getWidth();

        const bool sliceMode = apvts.getRawParameterValue(ParamID::SAMPLER_SLICE_MODE)->load() > 0.5f;
        const juce::Colour amber(0xffe6b04a);

        // Start/end markers + dim overlay only apply in pitched mode. In
        // slice mode each slice has its own region [slices[i], slices[i+1])
        // so start/end are meaningless and would just confuse the user.
        if (! sliceMode)
        {
            // Dim regions outside [start, end] so the active region pops.
            g.setColour(juce::Colour(0xa0000000));
            if (startX > wf.getX())
                g.fillRect(juce::Rectangle<float>(wf.getX(), wf.getY(),
                                                    startX - wf.getX(), wf.getHeight()));
            if (endX < wf.getRight())
                g.fillRect(juce::Rectangle<float>(endX, wf.getY(),
                                                    wf.getRight() - endX, wf.getHeight()));

            // Vertical lines at start/end so the boundaries read at a glance.
            g.setColour(amber);   // distinct from blue playhead
            g.drawLine(startX, wf.getY(), startX, wf.getBottom(), 1.0f);
            g.drawLine(endX,   wf.getY(), endX,   wf.getBottom(), 1.0f);

            // Triangle handles pointing INWARD (into the active region) so
            // the user sees the grab area as an arrow.
            constexpr float kTriH = 7.0f;
            const float triY = wf.getY();
            juce::Path startTri;
            startTri.addTriangle(startX,         triY,
                                 startX + kTriH, triY + kTriH * 0.5f,
                                 startX,         triY + kTriH);
            juce::Path endTri;
            endTri.addTriangle(endX,         triY,
                               endX - kTriH, triY + kTriH * 0.5f,
                               endX,         triY + kTriH);
            g.setColour(amber);
            g.fillPath(startTri);
            g.fillPath(endTri);
        }
        else
        {
            // Slice mode: blue (SYNTH) vertical line per slice running
            // through the waveform, with a draggable handle (note label +
            // triangle) sitting above the waveform in the marker strip.
            // Color matches the spectrum/oscilloscope SYNTH stroke so the
            // sampler reads as a synth source at a glance. Slice 0 is
            // fixed at sample 0 and shown faded with no handle.
            const auto& slices = processor.getPhantomSampler().getSliceTable();
            const int totalSamples = totalSourceSamples();
            if (totalSamples > 0)
            {
                constexpr float kTriH = 5.0f;
                constexpr float kTriW = 9.0f;
                const auto strip = markerStrip.toFloat();   // marker strip rect for labels/triangles
                const juce::Colour synthBlue(0xff508ED7);   // matches SYNTH stroke in spectrum/scope

                g.setFont(juce::FontOptions(10.0f, juce::Font::bold));

                for (int idx = 0; idx < (int) slices.size(); ++idx)
                {
                    const float frac = (float) slices[idx] / (float) totalSamples;
                    if (frac > 1.0f) break;
                    const float x = wf.getX() + frac * wf.getWidth();

                    // Slice line through the waveform. Faded for the fixed
                    // slice 0, solid for the rest so movable ones pop.
                    g.setColour(synthBlue.withAlpha(idx == 0 ? 0.45f : 1.0f));
                    g.drawLine(x, wf.getY(), x, wf.getBottom(), idx == 0 ? 1.0f : 1.5f);

                    // Marker-strip area: label above, triangle pointing down
                    // toward the waveform line, both forming the drag handle.
                    const auto label = midiNoteName(36 + idx);
                    const float labelW = 28.0f;
                    juce::Rectangle<int> labelBox = (idx == 0)
                        ? juce::Rectangle<int>((int) (strip.getX()),       (int) strip.getY(),
                                               (int) labelW,               (int) (strip.getHeight() - kTriH))
                        : juce::Rectangle<int>((int) (x - labelW * 0.5f),  (int) strip.getY(),
                                               (int) labelW,               (int) (strip.getHeight() - kTriH));
                    g.setColour(synthBlue);
                    g.drawText(label, labelBox,
                               idx == 0 ? juce::Justification::centredLeft
                                        : juce::Justification::centred,
                               false);

                    // Downward-pointing triangle sits just above the
                    // waveform line — only for movable slices.
                    if (idx > 0)
                    {
                        const float triY = strip.getBottom() - kTriH;
                        juce::Path tri;
                        tri.addTriangle(x - kTriW * 0.5f, triY,
                                        x + kTriW * 0.5f, triY,
                                        x,                triY + kTriH);
                        g.setColour(synthBlue);
                        g.fillPath(tri);
                    }
                }
            }
        }

        // Playhead overlay (atomic int from PhantomSampler). Maps the
        // source-sample index to a fraction of the thumbnail's total
        // length, then to the waveform-area's width.
        const int playhead = processor.getPhantomSampler().getPlayheadPosition();
        auto& thumb         = processor.getSampleThumbnail();
        const double durSec = thumb.getTotalLength();
        if (playhead >= 0 && durSec > 0.0 && processor.getPhantomSampler().hasSample())
        {
            // Source-sample index -> fraction of total sample length. We
            // query the sampler for the real source sample rate of the
            // loaded sound, which avoids the ~8.8% drift the playhead used
            // to exhibit on 48k-rate samples. Defensive 44100 fallback if
            // no sample is loaded (shouldn't happen — guarded by hasSample
            // above — but keeps the math defined).
            const double srcRate = processor.getPhantomSampler().getLoadedSourceSampleRate();
            const double rate    = srcRate > 0.0 ? srcRate : 44100.0;
            const int totalSamples = (int) (durSec * rate);
            const float frac = juce::jlimit(0.0f, 1.0f,
                (float) playhead / juce::jmax(1.0f, (float) totalSamples));
            const int xpx = waveformArea.getX() + (int) (frac * waveformArea.getWidth());
            g.setColour(juce::Colour(0xe6ffffff));   // OUTPUT white — matches spectrum/scope legend
            g.drawLine((float) xpx, (float) waveformArea.getY(),
                        (float) xpx, (float) waveformArea.getBottom(), 1.0f);
        }
    }
    else
    {
        g.setColour(juce::Colour(0x66ffffff));
        g.setFont(juce::FontOptions(12.0f));
        const char* msg =
              loadState == LoadState::Loading ? "Loading..."
            : loadState == LoadState::Error   ? errorMessage.toRawUTF8()
                                                : "Click or drop a sample";
        g.drawText(msg, waveformArea, juce::Justification::centred, false);
    }

    // A/D/S/R letter labels under each envelope slider. The sliders
    // themselves are juce::Slider children that paint their own track +
    // thumb; we add the letter underneath since LinearVertical sliders
    // don't ship with built-in labels.
    g.setColour(juce::Colour(0x99ffffff));
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    struct { juce::Slider* s; const char* label; } adsr[] = {
        { &attackSlider,   "A"  },
        { &decaySlider,    "D"  },
        { &sustainSlider,  "S"  },
        { &releaseSlider,  "R"  },
        { &xfadeSlider,    "XF" },
        { &velocitySlider, "V"  }
    };
    for (const auto& entry : adsr)
    {
        const auto b = entry.s->getBounds();
        const juce::Rectangle<int> labelRect(b.getX(), b.getBottom() + 1,
                                              b.getWidth(), 10);
        g.drawText(entry.label, labelRect, juce::Justification::centred, false);
    }
}

void SamplerStrip::resized()
{
    auto area = getLocalBounds();
    auto headerArea = area.removeFromTop(kHeaderH);
    area.removeFromTop(kMarkerStripH);   // slice handle strip — painted directly
    auto waveformArea = area.removeFromTop(kWaveformH);
    juce::ignoreUnused(waveformArea);    // painted directly, no children
    auto controlsArea = area.removeFromTop(kControlsH).reduced(8, 4);

    // Header - source selector left, warp selector next to it, folder
    // button right, filename painted between (in paint()).
    sourceToggle.setBounds(headerArea.removeFromLeft(220).reduced(6, 2));
    warpToggle.setBounds(headerArea.removeFromLeft(100).reduced(6, 2));
    folderButton.setBounds(headerArea.removeFromRight(36).reduced(4));

    // Controls row — 90 px tall. Widget heights vary; vertical-centre
    // the small fixed-height widgets in the row so they share a
    // baseline with the taller PhantomMiniKnob.
    constexpr int kRootW    = 76;
    constexpr int kRootH    = 28;
    constexpr int kLoopW    = 50;
    constexpr int kLoopH    = 22;
    constexpr int kGainW    = 82;   // PhantomMiniKnob natural width
    constexpr int kGainH    = 88;   // close to natural 93; slight squeeze fits the row
    constexpr int kEnvW     = 30;   // each ADSR slider
    constexpr int kEnvSpace = 10;   // reserved at bottom of slider for A/D/S/R letter labels
    constexpr int kColGap   = 10;
    constexpr int kAdsrGap  = 4;

    const int rowH = controlsArea.getHeight();

    auto placeCentered = [&](juce::Component& c, int w, int h)
    {
        auto slot = controlsArea.removeFromLeft(w);
        c.setBounds(slot.getX(), slot.getY() + (rowH - h) / 2, w, h);
        controlsArea.removeFromLeft(kColGap);
    };

    placeCentered(rootNoteCombo,  kRootW, kRootH);
    placeCentered(quantizeCombo,  kRootW, kRootH);
    placeCentered(loopToggle,     kLoopW, kLoopH);
    placeCentered(sliceToggle,    kLoopW, kLoopH);
    placeCentered(autoSliceToggle,kLoopW, kLoopH);
    placeCentered(reverseToggle,  kLoopW, kLoopH);
    placeCentered(fixVelToggle,   kLoopW + 10, kLoopH);   // slightly wider for "FixVel" text
    placeCentered(gainKnob,       kGainW, kGainH);

    // ADSR sliders share a row, each with a letter label painted below.
    // Slider takes rowH - kEnvSpace; the label area is drawn in paint().
    // XF (loop crossfade) sits last so it visually groups with the time-
    // domain envelope params even though it's a sample-loop control.
    const int sliderH = juce::jmax(40, rowH - kEnvSpace);
    auto placeSlider = [&](juce::Slider& s)
    {
        auto slot = controlsArea.removeFromLeft(kEnvW);
        s.setBounds(slot.getX(), slot.getY(), kEnvW, sliderH);
        controlsArea.removeFromLeft(kAdsrGap);
    };
    placeSlider(attackSlider);
    placeSlider(decaySlider);
    placeSlider(sustainSlider);
    placeSlider(releaseSlider);
    placeSlider(xfadeSlider);
    placeSlider(velocitySlider);
}

float SamplerStrip::fractionAtX(float xpx) const noexcept
{
    if (waveformBoundsCache.getWidth() <= 0.0f) return 0.0f;
    return juce::jlimit(0.0f, 1.0f,
        (xpx - waveformBoundsCache.getX()) / waveformBoundsCache.getWidth());
}

int SamplerStrip::totalSourceSamples() const noexcept
{
    const double durSec  = processor.getSampleThumbnail().getTotalLength();
    const double srcRate = processor.getPhantomSampler().getLoadedSourceSampleRate();
    const double rate    = srcRate > 0.0 ? srcRate : 44100.0;
    return (int) (durSec * rate);
}

int SamplerStrip::sampleAtX(float xpx) const noexcept
{
    const int total = totalSourceSamples();
    if (total <= 0) return 0;
    return juce::jlimit(0, total - 1, (int) (fractionAtX(xpx) * (float) total));
}

int SamplerStrip::hitTestSliceHandle(juce::Point<int> p) const noexcept
{
    // The handle is the label + triangle stacked in the marker strip just
    // above the waveform — together they form a generous drag target so
    // the user can grab a slice by its note name as easily as by its arrow.
    if (apvts.getRawParameterValue(ParamID::SAMPLER_SLICE_MODE)->load() <= 0.5f)
        return -1;

    const auto& slices = processor.getPhantomSampler().getSliceTable();
    const int total = totalSourceSamples();
    if (total <= 0) return -1;

    // Marker strip spans [waveformTop - kMarkerStripH, waveformTop].
    const int waveformTop = (int) waveformBoundsCache.getY();
    const int stripTop    = waveformTop - kMarkerStripH;
    if (p.y < stripTop || p.y > waveformTop) return -1;

    constexpr int kHandleHalfW = 14;   // matches the label box half-width

    // Slice 0 is fixed at sample 0 — not grabbable.
    for (int idx = 1; idx < (int) slices.size(); ++idx)
    {
        const float frac = (float) slices[idx] / (float) total;
        if (frac > 1.0f) break;
        const float x = waveformBoundsCache.getX() + frac * waveformBoundsCache.getWidth();
        if (std::abs((float) p.x - x) <= kHandleHalfW) return idx;
    }
    return -1;
}

void SamplerStrip::mouseDown(const juce::MouseEvent& e)
{
    // Click on the waveform area: hit-test the marker/slice handles first;
    // if anything is grabbed, start a drag instead of opening the picker.
    // Combined hit zone: marker strip (slice handles) + waveform (markers/picker).
    auto waveformArea = juce::Rectangle<int>(0, kHeaderH, getWidth(), kMarkerStripH + kWaveformH);
    if (! waveformArea.contains(e.getPosition()))
        return;

    // Open file picker if no sample loaded yet — no handles to grab.
    if (! processor.getPhantomSampler().hasSample())
    {
        pickAndLoadFile();
        return;
    }

    const bool sliceMode = apvts.getRawParameterValue(ParamID::SAMPLER_SLICE_MODE)->load() > 0.5f;

    if (sliceMode)
    {
        // Right-click (or ctrl-click on macOS) — manual slice authoring.
        //   on handle → delete that slice
        //   on empty  → insert a new slice at the click position
        if (e.mods.isPopupMenu())
        {
            const int hit = hitTestSliceHandle(e.getPosition());
            auto slices = processor.getPhantomSampler().getSliceTable();
            if (hit > 0 && hit < (int) slices.size())
            {
                slices.erase(slices.begin() + hit);
            }
            else
            {
                const int sampleIdx = sampleAtX((float) e.x);
                if (sampleIdx > 0) slices.push_back(sampleIdx);
            }
            processor.getPhantomSampler().setSliceTable(std::move(slices));
            repaint();
            return;
        }

        // Left-click: hit-test the handle, start dragging if grabbed.
        const int hit = hitTestSliceHandle(e.getPosition());
        if (hit > 0)
        {
            activeDrag       = DragTarget::Slice;
            draggedSliceIdx  = hit;
            return;
        }
        // Empty left-click in slice mode is a no-op (don't open picker —
        // the user is likely just trying to inspect the waveform).
        return;
    }

    const float startFrac = apvts.getRawParameterValue(ParamID::SAMPLER_START)->load();
    const float endFrac   = apvts.getRawParameterValue(ParamID::SAMPLER_END)->load();
    const float startX = waveformBoundsCache.getX() + startFrac * waveformBoundsCache.getWidth();
    const float endX   = waveformBoundsCache.getX() + endFrac   * waveformBoundsCache.getWidth();

    // ±8 px hit zone around each marker.
    constexpr float kHit = 8.0f;
    if (std::abs((float) e.x - startX) < kHit)
    {
        activeDrag = DragTarget::Start;
        return;
    }
    if (std::abs((float) e.x - endX) < kHit)
    {
        activeDrag = DragTarget::End;
        return;
    }

    // Empty waveform area → file picker.
    pickAndLoadFile();
}

void SamplerStrip::mouseDrag(const juce::MouseEvent& e)
{
    if (activeDrag == DragTarget::None) return;

    if (activeDrag == DragTarget::Slice)
    {
        auto slices = processor.getPhantomSampler().getSliceTable();
        if (draggedSliceIdx <= 0 || draggedSliceIdx >= (int) slices.size())
        {
            activeDrag      = DragTarget::None;
            draggedSliceIdx = -1;
            return;
        }
        const int total = totalSourceSamples();
        // Clamp between neighbours so the table stays ordered and the
        // dragged slice doesn't merge with its siblings mid-drag.
        const int leftBound  = slices[draggedSliceIdx - 1] + 1;
        const int rightBound = (draggedSliceIdx + 1 < (int) slices.size())
                                   ? slices[draggedSliceIdx + 1] - 1
                                   : juce::jmax(1, total - 1);
        slices[draggedSliceIdx] = juce::jlimit(leftBound, rightBound, sampleAtX((float) e.x));
        processor.getPhantomSampler().setSliceTable(std::move(slices));
        repaint();
        return;
    }

    const float frac = fractionAtX((float) e.x);
    const auto* paramId = (activeDrag == DragTarget::Start)
                              ? ParamID::SAMPLER_START : ParamID::SAMPLER_END;
    if (auto* p = apvts.getParameter(paramId))
        p->setValueNotifyingHost(frac);
    repaint();
}

void SamplerStrip::mouseUp(const juce::MouseEvent&)
{
    activeDrag      = DragTarget::None;
    draggedSliceIdx = -1;
}

void SamplerStrip::mouseDoubleClick(const juce::MouseEvent& e)
{
    // Combined hit zone: marker strip (slice handles) + waveform (markers/picker).
    auto waveformArea = juce::Rectangle<int>(0, kHeaderH, getWidth(), kMarkerStripH + kWaveformH);
    if (! waveformArea.contains(e.getPosition())) return;
    if (! processor.getPhantomSampler().hasSample()) return;

    const bool sliceMode = apvts.getRawParameterValue(ParamID::SAMPLER_SLICE_MODE)->load() > 0.5f;

    if (sliceMode)
    {
        // Double-click on a slice handle deletes that slice (slice 0 is
        // never deletable — it's the implicit start at sample 0).
        const int hit = hitTestSliceHandle(e.getPosition());
        if (hit > 0)
        {
            auto slices = processor.getPhantomSampler().getSliceTable();
            if (hit < (int) slices.size())
            {
                slices.erase(slices.begin() + hit);
                processor.getPhantomSampler().setSliceTable(std::move(slices));
                repaint();
            }
        }
        return;
    }

    // Pitched mode: double-click on a start/end marker resets it.
    const float startFrac = apvts.getRawParameterValue(ParamID::SAMPLER_START)->load();
    const float endFrac   = apvts.getRawParameterValue(ParamID::SAMPLER_END)->load();
    const float startX = waveformBoundsCache.getX() + startFrac * waveformBoundsCache.getWidth();
    const float endX   = waveformBoundsCache.getX() + endFrac   * waveformBoundsCache.getWidth();
    constexpr float kHit = 8.0f;

    if (std::abs((float) e.x - startX) < kHit)
        if (auto* p = apvts.getParameter(ParamID::SAMPLER_START)) p->setValueNotifyingHost(0.0f);
    if (std::abs((float) e.x - endX) < kHit)
        if (auto* p = apvts.getParameter(ParamID::SAMPLER_END))   p->setValueNotifyingHost(1.0f);
    repaint();
}

bool SamplerStrip::isInterestedInFileDrag(const juce::StringArray& files)
{
    if (files.size() != 1) return false;
    const auto ext = juce::File(files[0]).getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".aif" || ext == ".aiff"
        || ext == ".flac" || ext == ".ogg" || ext == ".mp3";
}

void SamplerStrip::filesDropped(const juce::StringArray& files, int /*x*/, int /*y*/)
{
    if (files.size() == 1)
        loadSampleAsync(juce::File(files[0]));
}

void SamplerStrip::timerCallback()
{
    // Repaint only if a voice is active so we're not chewing cycles
    // when idle.
    if (processor.getPhantomSampler().getActiveVoiceCount() > 0)
        repaint();
}

void SamplerStrip::pickAndLoadFile()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Choose sample",
        juce::File::getSpecialLocation(juce::File::userMusicDirectory),
        "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                         | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file.existsAsFile())
                loadSampleAsync(file);
        });
}

void SamplerStrip::loadSampleAsync(const juce::File& file)
{
    loadState = LoadState::Loading;
    repaint();

    const int myGen = loadGeneration.fetch_add(1, std::memory_order_relaxed) + 1;

    juce::Component::SafePointer<SamplerStrip> self(this);
    juce::Thread::launch([self, file, myGen]
    {
        // Cheap up-front size check via stat() - avoids reading hundreds of MB
        // into memory just to reject afterwards. 50MB is enough for ~5 min of
        // 44.1k/16-bit stereo or several minutes of FLAC/MP3; preset state
        // grows correspondingly when the sample is embedded.
        if (file.getSize() > 50 * 1024 * 1024)
        {
            juce::MessageManager::callAsync([self, myGen] {
                if (auto* pp = self.getComponent()) {
                    if (pp->loadGeneration.load(std::memory_order_relaxed) != myGen) return;
                    pp->loadState = LoadState::Error;
                    pp->errorMessage = "Sample too large (max 50MB)";
                    pp->repaint();
                }
            });
            return;
        }

        juce::MemoryBlock bytes;
        if (! file.loadFileAsData(bytes))
        {
            juce::MessageManager::callAsync([self, myGen] {
                if (auto* pp = self.getComponent()) {
                    if (pp->loadGeneration.load(std::memory_order_relaxed) != myGen) return;
                    pp->loadState = LoadState::Error;
                    pp->errorMessage = "Couldn't read file";
                    pp->repaint();
                }
            });
            return;
        }
        // Decode via a local format manager (registered with basic
        // formats - matches the one in PluginProcessor).
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(
            fm.createReaderFor(std::make_unique<juce::MemoryInputStream>(bytes, false)));
        if (reader == nullptr)
        {
            juce::MessageManager::callAsync([self, myGen] {
                if (auto* pp = self.getComponent()) {
                    if (pp->loadGeneration.load(std::memory_order_relaxed) != myGen) return;
                    pp->loadState = LoadState::Error;
                    pp->errorMessage = "Couldn't decode file";
                    pp->repaint();
                }
            });
            return;
        }
        juce::AudioBuffer<float> decoded(
            (int) reader->numChannels,
            (int) reader->lengthInSamples);
        reader->read(&decoded, 0, decoded.getNumSamples(), 0, true, true);
        const double sr = reader->sampleRate;
        const auto filename = file.getFileName();

        juce::MessageManager::callAsync(
            [self, myGen, bytes = std::move(bytes), filename,
             decoded = std::move(decoded), sr]() mutable
            {
                if (auto* pp = self.getComponent())
                {
                    if (pp->loadGeneration.load(std::memory_order_relaxed) != myGen) return;
                    if (pp->processor.setSampleFromBytes(
                            std::move(bytes), filename, std::move(decoded), sr))
                    {
                        pp->loadState = LoadState::Idle;
                        pp->rebuildWaveformThumbnail();
                    }
                    else
                    {
                        pp->loadState = LoadState::Error;
                        pp->errorMessage = "Load failed";
                    }
                    pp->repaint();
                }
            });
    });
}

void SamplerStrip::rebuildWaveformThumbnail()
{
    constexpr int kImgW = 512;
    constexpr int kImgH = 60;
    waveformImage = juce::Image(juce::Image::ARGB, kImgW, kImgH, true);

    auto& thumb = processor.getSampleThumbnail();
    if (thumb.getTotalLength() <= 0.0) return;

    juce::Graphics g(waveformImage);
    g.setColour(juce::Colour(0xff1a2028));
    g.fillAll();
    g.setColour(juce::Colour(0xffa0a0af));   // INPUT gray — matches spectrum/scope legend
    juce::Rectangle<int> rect(0, 0, kImgW, kImgH);
    thumb.drawChannels(g, rect, 0.0, thumb.getTotalLength(), 1.0f);
}

} // namespace kaigen::phantom
