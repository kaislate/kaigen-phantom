// Source/UI/widgets/PresetSelector.cpp
#include "PresetSelector.h"
#include "../Theme.h"
#include "../../PluginProcessor.h"
#include "../../PresetManager.h"

namespace kaigen::phantom
{

namespace
{
    /** Flatten all presets into an ordered list of (pack, name) pairs.
     *  Packs alphabetical (std::map iteration order); presets within each
     *  pack in PresetManager's order. */
    std::vector<std::pair<juce::String, juce::String>> flattenPresets(PhantomProcessor& processor)
    {
        std::vector<std::pair<juce::String, juce::String>> out;
        const auto all = processor.getPresetManager().getAllPresets();
        for (const auto& [packName, presets] : all)
            for (const auto& p : presets)
                out.emplace_back(packName, p.metadata.name);
        return out;
    }

    /** Index of (pack, name) in the flat list, or -1 if not found. */
    int findIndex(const std::vector<std::pair<juce::String, juce::String>>& flat,
                  const juce::String& pack, const juce::String& name)
    {
        for (size_t i = 0; i < flat.size(); ++i)
            if (flat[i].first == pack && flat[i].second == name)
                return (int) i;
        return -1;
    }

    // UTF-8 byte sequences for the glyph text. Defined here so the source
    // file's encoding doesn't matter.
    inline juce::String glyphHeart() { return juce::String(juce::CharPointer_UTF8("\xE2\x99\xA1")); }   // ♡
    inline juce::String glyphPrev () { return juce::String(juce::CharPointer_UTF8("\xE2\x96\xB2")); }   // ▲
    inline juce::String glyphNext () { return juce::String(juce::CharPointer_UTF8("\xE2\x96\xBC")); }   // ▼
    inline juce::String glyphSave () { return juce::String(juce::CharPointer_UTF8("\xF0\x9F\x92\xBE")); } // 💾
}

PresetSelector::PresetSelector(PhantomProcessor& p, juce::AudioProcessorValueTreeState& a)
    : processor(p), apvts(a)
{
    libraryButton.setButtonText("|||");
    heartButton  .setButtonText(glyphHeart());
    prevButton   .setButtonText(glyphPrev());
    nextButton   .setButtonText(glyphNext());
    saveButton   .setButtonText(glyphSave());

    libraryButton.onClick = [this] { if (onBrowseRequested) onBrowseRequested(); };
    heartButton  .onClick = [] { /* favorites — wired in a follow-up */ };
    prevButton   .onClick = [this] { prevPreset(); };
    nextButton   .onClick = [this] { nextPreset(); };
    saveButton   .onClick = [this] { saveDialog(); };

    for (auto* b : { &libraryButton, &heartButton, &prevButton, &nextButton, &saveButton })
    {
        b->getProperties().set("phantom-style", "header-glyph");
        addAndMakeVisible(b);
    }

    libraryButton.setTooltip("Open preset browser");
    heartButton  .setTooltip("Favorite (coming soon)");
    prevButton   .setTooltip("Previous preset");
    nextButton   .setTooltip("Next preset");
    saveButton   .setTooltip("Save preset");
}

PresetSelector::~PresetSelector() = default;

void PresetSelector::setCurrentPreset(const juce::String& name, const juce::String& pack)
{
    currentPresetName = name;
    currentPresetPack = pack;
    repaint();
}

void PresetSelector::prevPreset()
{
    auto flat = flattenPresets(processor);
    if (flat.empty()) return;
    int idx = findIndex(flat, currentPresetPack, currentPresetName);
    if (idx < 0) idx = 0;
    else idx = (idx == 0) ? (int) flat.size() - 1 : idx - 1;
    const auto& [pack, name] = flat[(size_t) idx];
    processor.getPresetManager().loadPreset(apvts, name, pack);
    setCurrentPreset(name, pack);
}

void PresetSelector::nextPreset()
{
    auto flat = flattenPresets(processor);
    if (flat.empty()) return;
    int idx = findIndex(flat, currentPresetPack, currentPresetName);
    if (idx < 0) idx = 0;
    else idx = (idx + 1) % (int) flat.size();
    const auto& [pack, name] = flat[(size_t) idx];
    processor.getPresetManager().loadPreset(apvts, name, pack);
    setCurrentPreset(name, pack);
}

void PresetSelector::saveDialog()
{
    auto* aw = new juce::AlertWindow("Save Preset",
                                      "Enter a name for the new preset:",
                                      juce::MessageBoxIconType::QuestionIcon);
    aw->addTextEditor("name", currentPresetName.isEmpty() ? "New Preset" : currentPresetName);
    aw->addButton("Save",   1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    aw->enterModalState(true,
        juce::ModalCallbackFunction::create([this, aw](int result)
        {
            std::unique_ptr<juce::AlertWindow> owned(aw);
            if (result != 1) return;
            const auto name = owned->getTextEditorContents("name").trim();
            if (name.isEmpty()) return;
            const auto saved = processor.getPresetManager().savePreset(
                apvts, name, "Experimental", "User", "", /*overwrite=*/false);
            if (saved.isNotEmpty())
                setCurrentPreset(saved, "User");
        }),
        false);
}

void PresetSelector::paint(juce::Graphics& g)
{
    Theme::paintGlassPill(g, pillBounds.toFloat());

    // Preset name centered inside the pill (between heart on left and the
    // modified asterisk on the right). Dark-on-light per CSS rgba(0,0,0,0.75).
    // Bumped 12 → 16 px to match the webview's rendered size at Live's
    // plugin-window scale.
    const auto pillTextArea = pillBounds.reduced(28, 0);
    g.setFont(juce::FontOptions(Theme::uiFontFamily(), 16.0f, juce::Font::plain));
    g.setColour(juce::Colour(0xbf000000));
    const auto display = currentPresetName.isEmpty() ? juce::String("Default") : currentPresetName;
    g.drawText(display, pillTextArea, juce::Justification::centred, true);

    // Modified-state asterisk — CSS spec hides this until the preset has
    // unsaved changes. Modified-state tracking isn't wired yet, so it stays
    // hidden for now. (Was previously always-shown as a placeholder.)
}

void PresetSelector::mouseDown(const juce::MouseEvent& e)
{
    // Click anywhere on the glass pill (outside the heart) opens the
    // Arturia-style quick picker. The full browser opens via the |||
    // library glyph (not via the pill).
    if (pillBounds.contains(e.getPosition()) && ! heartButton.getBounds().contains(e.getPosition()))
        if (onQuickPickRequested) onQuickPickRequested(pillBounds);
}

void PresetSelector::resized()
{
    // Layout left → right, all vertically centered in the strip:
    //   |||  [♡  Name  *]  ▲ ▼  💾
    // The whole group has a fixed compact width (no stretching to fill);
    // it's centered horizontally within the slot allocated by TopBar.
    const auto strip = getLocalBounds();

    // Scaled ~1.4× from CSS spec to match webview's rendered size.
    constexpr int pillH      = 36;     // CSS 26 → 36
    constexpr int glyphW     = 30;     // 22 → 30
    constexpr int gapWide    = 10;
    constexpr int gapTight   =  3;
    constexpr int heartW     = 22;     // 16 → 22
    constexpr int pillW      = 340;    // CSS 260 → 340

    const int totalW = glyphW + gapWide                    // ||| + gap
                     + pillW  + gapWide                    // pill + gap
                     + glyphW + gapTight + glyphW          // ▲ ▼
                     + gapWide + glyphW;                   // gap + 💾

    const int y      = (strip.getHeight() - pillH) / 2;
    int       x      = (strip.getWidth()  - totalW) / 2;

    libraryButton.setBounds(x, y, glyphW, pillH);
    x += glyphW + gapWide;

    pillBounds = juce::Rectangle<int>(x, y, pillW, pillH);
    x += pillW + gapWide;

    prevButton.setBounds(x, y, glyphW, pillH);
    x += glyphW + gapTight;
    nextButton.setBounds(x, y, glyphW, pillH);
    x += glyphW + gapWide;

    saveButton.setBounds(x, y, glyphW, pillH);

    // Heart sits centered inside the left edge of the pill.
    heartButton.setBounds(pillBounds.getX() + 6, pillBounds.getY(), heartW, pillBounds.getHeight());
}

} // namespace kaigen::phantom
