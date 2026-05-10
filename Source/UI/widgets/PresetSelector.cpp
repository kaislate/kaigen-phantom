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
    // The TopBar paints the strip background — we only paint the glass pill
    // and the preset name on top of it.
    Theme::paintGlassPill(g, pillBounds.toFloat());

    // Preset name centered inside the pill (between heart on left and any
    // future modified-asterisk on the right). Etched dark on the glass.
    const auto pillTextArea = pillBounds.reduced(28, 0);
    g.setFont(juce::FontOptions("Space Grotesk", 12.0f, juce::Font::plain));
    g.setColour(juce::Colour(0xbf000000));   // ~75% black, matches CSS rgba(0,0,0,0.75)
    const auto display = currentPresetName.isEmpty() ? juce::String("Default") : currentPresetName;
    g.drawText(display, pillTextArea, juce::Justification::centred, true);
}

void PresetSelector::mouseDown(const juce::MouseEvent& e)
{
    // Click anywhere on the glass pill (outside the heart) opens the browser.
    if (pillBounds.contains(e.getPosition()) && ! heartButton.getBounds().contains(e.getPosition()))
        if (onBrowseRequested) onBrowseRequested();
}

void PresetSelector::resized()
{
    // Layout left → right: ||| [♡  Name  *] ▲ ▼ 💾
    auto area = getLocalBounds().reduced(0, 4);

    constexpr int glyphW       = 26;
    constexpr int gapAroundPill =  6;
    constexpr int heartW       = 18;
    constexpr int pillMinW     = 220;

    libraryButton.setBounds(area.removeFromLeft(glyphW));
    area.removeFromLeft(gapAroundPill);

    // Save sits at the right end; prev/next sit to its left.
    saveButton.setBounds(area.removeFromRight(glyphW));
    area.removeFromRight(2);
    nextButton.setBounds(area.removeFromRight(glyphW));
    prevButton.setBounds(area.removeFromRight(glyphW));
    area.removeFromRight(gapAroundPill);

    // Whatever's left is the glass pill (with a sensible minimum).
    auto pill = area;
    if (pill.getWidth() < pillMinW)
        pill.setWidth(pillMinW);
    pillBounds = pill;

    // Heart sits inside the pill on the left.
    heartButton.setBounds(pill.getX() + 6, pill.getY(), heartW, pill.getHeight());
}

} // namespace kaigen::phantom
