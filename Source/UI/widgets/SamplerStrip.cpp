// Source/UI/widgets/SamplerStrip.cpp
#include "SamplerStrip.h"
#include "../../PluginProcessor.h"
#include "../../Parameters.h"

namespace kaigen::phantom
{

namespace
{
    constexpr int kHeaderH   = 24;
    constexpr int kWaveformH = 60;
    constexpr int kControlsH = 40;

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
      loopToggle(a, ParamID::SAMPLER_LOOP, "Loop"),
      gainKnob(a, ParamID::SAMPLER_GAIN, "Gain")
{
    addAndMakeVisible(sourceToggle);

    folderButton.setButtonText("...");
    folderButton.getProperties().set("phantom-style", "header-glyph");
    folderButton.onClick = [this] { pickAndLoadFile(); };
    addAndMakeVisible(folderButton);

    for (int n = 0; n <= 127; ++n)
        rootNoteCombo.addItem(midiNoteName(n), n + 1);   // ComboBox ids must be >= 1
    rootAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, ParamID::SAMPLER_ROOT_NOTE, rootNoteCombo);
    addAndMakeVisible(rootNoteCombo);

    addAndMakeVisible(loopToggle);
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

    // 30 Hz playhead refresh while voices are active.
    startTimerHz(30);
}

SamplerStrip::~SamplerStrip() = default;

void SamplerStrip::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff121419));

    auto area = getLocalBounds();
    auto headerArea = area.removeFromTop(kHeaderH);
    auto waveformArea = area.removeFromTop(kWaveformH).reduced(6, 4);
    // controlsArea is the remainder; positioned in resized().

    // Header - filename label drawn between the source selector and folder button.
    auto filenameRect = headerArea.reduced(8, 0).withTrimmedLeft(220);   // skip the sourceToggle width
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

    if (waveformImage.isValid())
    {
        g.drawImage(waveformImage, waveformArea.toFloat(),
                    juce::RectanglePlacement::stretchToFit);

        // Playhead overlay (atomic int from PhantomSampler). Maps the
        // source-sample index to a fraction of the thumbnail's total
        // length, then to the waveform-area's width.
        const int playhead = processor.getPhantomSampler().getPlayheadPosition();
        auto& thumb         = processor.getSampleThumbnail();
        const double durSec = thumb.getTotalLength();
        if (playhead >= 0 && durSec > 0.0 && processor.getPhantomSampler().hasSample())
        {
            // Source-sample index -> fraction of total sample length. Use the
            // thumbnail's source sample rate (approximated at host rate 44.1k
            // since AudioThumbnail doesn't expose its source sample rate
            // directly via the public API — close enough for visual feedback).
            const int totalSamples = (int) (durSec * 44100.0);
            const float frac = juce::jlimit(0.0f, 1.0f,
                (float) playhead / juce::jmax(1.0f, (float) totalSamples));
            const int xpx = waveformArea.getX() + (int) (frac * waveformArea.getWidth());
            g.setColour(juce::Colour(0xff77ddff));
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
}

void SamplerStrip::resized()
{
    auto area = getLocalBounds();
    auto headerArea = area.removeFromTop(kHeaderH);
    auto waveformArea = area.removeFromTop(kWaveformH);
    juce::ignoreUnused(waveformArea);   // painted directly, no children
    auto controlsArea = area.removeFromTop(kControlsH).reduced(6, 4);

    // Header - source selector left, folder button right, filename
    // painted between (in paint()).
    sourceToggle.setBounds(headerArea.removeFromLeft(220).reduced(6, 2));
    folderButton.setBounds(headerArea.removeFromRight(36).reduced(4));

    // Controls row.
    constexpr int kRootW = 64, kLoopW = 36, kGainW = 36, kEnvW = 18, kGap = 6;
    rootNoteCombo.setBounds(controlsArea.removeFromLeft(kRootW));
    controlsArea.removeFromLeft(kGap);
    loopToggle.setBounds(controlsArea.removeFromLeft(kLoopW));
    controlsArea.removeFromLeft(kGap);
    gainKnob.setBounds(controlsArea.removeFromLeft(kGainW));
    controlsArea.removeFromLeft(kGap * 2);
    attackSlider.setBounds(controlsArea.removeFromLeft(kEnvW));
    controlsArea.removeFromLeft(kGap);
    decaySlider.setBounds(controlsArea.removeFromLeft(kEnvW));
    controlsArea.removeFromLeft(kGap);
    sustainSlider.setBounds(controlsArea.removeFromLeft(kEnvW));
    controlsArea.removeFromLeft(kGap);
    releaseSlider.setBounds(controlsArea.removeFromLeft(kEnvW));
}

void SamplerStrip::mouseDown(const juce::MouseEvent& e)
{
    // Click on the waveform area opens the file picker.
    auto waveformArea = juce::Rectangle<int>(0, kHeaderH, getWidth(), kWaveformH);
    if (waveformArea.contains(e.getPosition()))
        pickAndLoadFile();
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
        // into memory just to reject afterwards.
        if (file.getSize() > 5 * 1024 * 1024)
        {
            juce::MessageManager::callAsync([self, myGen] {
                if (auto* pp = self.getComponent()) {
                    if (pp->loadGeneration.load(std::memory_order_relaxed) != myGen) return;
                    pp->loadState = LoadState::Error;
                    pp->errorMessage = "Sample too large (max 5MB)";
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
    g.setColour(juce::Colour(0xff77ddff));
    juce::Rectangle<int> rect(0, 0, kImgW, kImgH);
    thumb.drawChannels(g, rect, 0.0, thumb.getTotalLength(), 1.0f);
}

} // namespace kaigen::phantom
