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
}

PresetSelector::PresetSelector(PhantomProcessor& p, juce::AudioProcessorValueTreeState& a)
    : processor(p), apvts(a)
{
    prevButton .onClick = [this] { prevPreset(); };
    nextButton .onClick = [this] { nextPreset(); };
    browseButton.onClick = [this] { if (onBrowseRequested) onBrowseRequested(); };
    saveButton .onClick = [this] { saveDialog(); };
    addAndMakeVisible(prevButton);
    addAndMakeVisible(nextButton);
    addAndMakeVisible(browseButton);
    addAndMakeVisible(saveButton);
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
        false);  // unique_ptr in callback owns the deletion
}

void PresetSelector::paint(juce::Graphics& g)
{
    g.fillAll(Theme::panelBg);

    // Current preset name in center.
    g.setColour(Theme::textPrimary);
    g.setFont(juce::FontOptions("Space Grotesk", 13.0f, juce::Font::bold));
    const auto display = currentPresetName.isEmpty() ? juce::String("Default") : currentPresetName;
    auto labelArea = getLocalBounds().reduced(120, 0);
    g.drawText(display, labelArea.toFloat(), juce::Justification::centred, true);
}

void PresetSelector::resized()
{
    auto area = getLocalBounds().reduced(8, 4);

    // Left: prev / next
    prevButton.setBounds(area.removeFromLeft(28));
    area.removeFromLeft(2);
    nextButton.setBounds(area.removeFromLeft(28));

    // Right: save / browse (right-to-left layout)
    saveButton  .setBounds(area.removeFromRight(60));
    area.removeFromRight(4);
    browseButton.setBounds(area.removeFromRight(60));
}

} // namespace kaigen::phantom
