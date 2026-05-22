// Source/UI/widgets/RecipeSlotPills.cpp
#include "RecipeSlotPills.h"
#include "../../PluginProcessor.h"
#include "../../EngineFocus.h"
#include "../../Parameters.h"
#include <cmath>

namespace kaigen::phantom
{

namespace
{
    constexpr int kDotSize     = 6;
    constexpr int kDotGap      = 4;
    constexpr int kRowH        = 12;
    constexpr int kRowGap      = 2;

    // Slot DIRTY check tolerance — H param values round-trip through
    // 0..100 floats; treat anything closer than 0.005 (0.5 %) as equal.
    constexpr float kDirtyEpsilon = 0.005f;
}

RecipeSlotPills::RecipeSlotPills(PhantomProcessor& p)
    : processor(p)
{
    startTimerHz(4);
}

RecipeSlotPills::~RecipeSlotPills()
{
    stopTimer();
}

void RecipeSlotPills::timerCallback()
{
    blinkPhase = ! blinkPhase;
    repaint();
}

void RecipeSlotPills::resized()
{
    // Three rows stacked vertically; pill pair centred horizontally in
    // each row. Width of the component should match the bottom row of
    // the WordSelector (the LeftPanel sets that).
    const int cx = getWidth() / 2;
    const int pairW = 2 * kDotSize + kDotGap;

    for (int row = 0; row < 3; ++row)
    {
        const int y    = row * (kRowH + kRowGap);
        const int rowY = y + (kRowH - kDotSize) / 2;
        const int saveX = cx - pairW / 2;
        const int delX  = saveX + kDotSize + kDotGap;
        saveDots  [(size_t) row] = { saveX, rowY, kDotSize, kDotSize };
        deleteDots[(size_t) row] = { delX,  rowY, kDotSize, kDotSize };
    }
}

int RecipeSlotPills::rowAtY(int y) const noexcept
{
    if (y < 0) return -1;
    const int row = y / (kRowH + kRowGap);
    return (row < 0 || row > 2) ? -1 : row;
}

void RecipeSlotPills::mouseDown(const juce::MouseEvent& e)
{
    const int row = rowAtY(e.y);
    if (row < 0) return;

    const int engineIdx = (processor.getEngineFocus().activeTab
                            == kaigen::phantom::ActiveTab::B) ? 1 : 0;

    if (saveDots[(size_t) row].contains(e.getPosition()))
    {
        // Save only meaningful when this slot is the active preset.
        const char* presetParamId = (engineIdx == 1)
            ? ParamID::B_RECIPE_PRESET : ParamID::A_RECIPE_PRESET;
        const int currentPreset = juce::roundToInt(
            processor.apvts.getRawParameterValue(presetParamId)->load());
        if (currentPreset == 6 + row)
            processor.saveRecipeSlot(engineIdx, row);
    }
    else if (deleteDots[(size_t) row].contains(e.getPosition()))
    {
        if (processor.getRecipeSlot(engineIdx, row).filled)
            processor.clearRecipeSlot(engineIdx, row);
    }
}

void RecipeSlotPills::paint(juce::Graphics& g)
{
    const int engineIdx = (processor.getEngineFocus().activeTab
                            == kaigen::phantom::ActiveTab::B) ? 1 : 0;

    const char* presetParamId = (engineIdx == 1)
        ? ParamID::B_RECIPE_PRESET : ParamID::A_RECIPE_PRESET;
    const int currentPreset = juce::roundToInt(
        processor.apvts.getRawParameterValue(presetParamId)->load());

    // Read live H values once per paint for the DIRTY comparison.
    std::array<float, 7> liveH {};
    static const char* aHIds[7] = {
        ParamID::A_RECIPE_H2, ParamID::A_RECIPE_H3, ParamID::A_RECIPE_H4,
        ParamID::A_RECIPE_H5, ParamID::A_RECIPE_H6, ParamID::A_RECIPE_H7,
        ParamID::A_RECIPE_H8
    };
    static const char* bHIds[7] = {
        ParamID::B_RECIPE_H2, ParamID::B_RECIPE_H3, ParamID::B_RECIPE_H4,
        ParamID::B_RECIPE_H5, ParamID::B_RECIPE_H6, ParamID::B_RECIPE_H7,
        ParamID::B_RECIPE_H8
    };
    const char* const* hIds = (engineIdx == 1) ? bHIds : aHIds;
    for (int h = 0; h < 7; ++h)
    {
        const auto* raw = processor.apvts.getRawParameterValue(hIds[h]);
        liveH[(size_t) h] = (raw != nullptr) ? raw->load() * 0.01f : 0.0f;
    }

    for (int row = 0; row < 3; ++row)
    {
        const auto& slot = processor.getRecipeSlot(engineIdx, row);
        const bool isCurrent = (currentPreset == 6 + row);

        // Save dot colour.
        juce::Colour saveColour;
        if (! isCurrent)
        {
            saveColour = juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 0.15f);  // grey
        }
        else if (! slot.filled)
        {
            // Empty + current → blinking red (calling for save).
            saveColour = blinkPhase
                ? juce::Colour::fromFloatRGBA(1.0f, 0.30f, 0.25f, 0.95f)
                : juce::Colour::fromFloatRGBA(1.0f, 0.30f, 0.25f, 0.25f);
        }
        else
        {
            // Filled + current → compare savedH to liveH for DIRTY.
            bool dirty = false;
            for (int h = 0; h < 7; ++h)
                if (std::abs(slot.savedH[(size_t) h] - liveH[(size_t) h]) > kDirtyEpsilon)
                { dirty = true; break; }

            if (dirty)
            {
                saveColour = blinkPhase
                    ? juce::Colour::fromFloatRGBA(1.0f, 0.30f, 0.25f, 0.95f)
                    : juce::Colour::fromFloatRGBA(1.0f, 0.30f, 0.25f, 0.25f);
            }
            else
            {
                saveColour = juce::Colour::fromFloatRGBA(0.30f, 0.95f, 0.40f, 0.90f);   // green
            }
        }

        g.setColour(saveColour);
        g.fillEllipse(saveDots[(size_t) row].toFloat());

        // Delete dot colour.
        const juce::Colour delColour = slot.filled
            ? juce::Colour::fromFloatRGBA(1.0f, 0.30f, 0.25f, 0.85f)   // red
            : juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 0.12f);    // grey
        g.setColour(delColour);
        g.fillEllipse(deleteDots[(size_t) row].toFloat());
    }
}

} // namespace kaigen::phantom
