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
    constexpr float kFontSize = 9.5f;
    constexpr float kKerning  = 0.20f;

    // DIRTY-check tolerance in normalised [0..1] H space.
    constexpr float kDirtyEpsilon = 0.005f;

    // Etched text helper — white 1 px shadow below + body text in the
    // requested colour. Matches EtchedToggle's inactive recipe and the
    // WordSelector inactive-word style so the pills feel engraved into
    // the silver surface alongside the preset names above.
    void drawEtched(juce::Graphics& g,
                    const juce::String& upper,
                    juce::Rectangle<float> bounds,
                    juce::Colour bodyColour)
    {
        g.setColour(juce::Colour(0x80FFFFFF));
        g.drawText(upper, bounds.translated(0.0f, 1.0f),
                    juce::Justification::centred, false);
        g.setColour(bodyColour);
        g.drawText(upper, bounds, juce::Justification::centred, false);
    }

    // Halo + bright body — used for the "active" SAVE states. Same recipe
    // as EtchedToggle's active branch but with a tinted halo so green
    // (clean) and red (dirty/empty) read distinct.
    void drawGlowing(juce::Graphics& g,
                     const juce::String& upper,
                     juce::Rectangle<float> bounds,
                     juce::Colour haloColour,
                     juce::Colour bodyColour)
    {
        for (int r = 4; r >= 1; --r)
        {
            g.setColour(haloColour.withAlpha(0.07f));
            g.drawText(upper, bounds.translated(-(float) r, 0.0f), juce::Justification::centred, false);
            g.drawText(upper, bounds.translated( (float) r, 0.0f), juce::Justification::centred, false);
            g.drawText(upper, bounds.translated(0.0f, -(float) r), juce::Justification::centred, false);
            g.drawText(upper, bounds.translated(0.0f,  (float) r), juce::Justification::centred, false);
        }
        g.setColour(bodyColour);
        g.drawText(upper, bounds, juce::Justification::centred, false);
    }
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
    // 3 columns × 2 pills (SAVE + DEL). Column widths match the
    // WordSelector grid above (which also divides into thirds).
    const int totalW = getWidth();
    const int totalH = getHeight();
    const int colW   = totalW / 3;

    for (int col = 0; col < 3; ++col)
    {
        const int colX  = col * colW;
        const int halfW = colW / 2;
        saveBounds  [(size_t) col] = { colX,           0, halfW, totalH };
        deleteBounds[(size_t) col] = { colX + halfW,   0, halfW, totalH };
    }
}

void RecipeSlotPills::mouseDown(const juce::MouseEvent& e)
{
    const int engineIdx = (processor.getEngineFocus().activeTab
                            == kaigen::phantom::ActiveTab::B) ? 1 : 0;
    const char* presetParamId = (engineIdx == 1)
        ? ParamID::B_RECIPE_PRESET : ParamID::A_RECIPE_PRESET;
    const int currentPreset = juce::roundToInt(
        processor.apvts.getRawParameterValue(presetParamId)->load());

    for (int col = 0; col < 3; ++col)
    {
        if (saveBounds[(size_t) col].contains(e.getPosition()))
        {
            if (currentPreset == 6 + col)
                processor.saveRecipeSlot(engineIdx, col);
            return;
        }
        if (deleteBounds[(size_t) col].contains(e.getPosition()))
        {
            if (processor.getRecipeSlot(engineIdx, col).filled)
                processor.clearRecipeSlot(engineIdx, col);
            return;
        }
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
    std::array<float, 7> liveH {};
    for (int h = 0; h < 7; ++h)
    {
        const auto* raw = processor.apvts.getRawParameterValue(hIds[h]);
        liveH[(size_t) h] = (raw != nullptr) ? raw->load() * 0.01f : 0.0f;
    }

    juce::Font font(juce::FontOptions("Space Grotesk", kFontSize, juce::Font::bold));
    font.setExtraKerningFactor(kKerning);
    g.setFont(font);

    // Highlight palette — matches EtchedToggle / WordSelector active-word
    // recipe (white halo + bright white body) so SAVE pills feel native
    // alongside the preset words above. DIRTY / empty-pending states blink
    // by alternating the body alpha; CLEAN state is solid bright. Colour
    // is never used — state distinction comes from glow vs etched and
    // solid vs blinking.
    const juce::Colour kWhiteHalo = juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 1.0f);
    const juce::Colour kBrightBody = juce::Colour(0xfff5f8fb);

    for (int col = 0; col < 3; ++col)
    {
        const auto& slot      = processor.getRecipeSlot(engineIdx, col);
        const bool  isCurrent = (currentPreset == 6 + col);

        // ── SAVE pill ────────────────────────────────────────────────────
        const auto saveRect = saveBounds[(size_t) col].toFloat();
        if (! isCurrent)
        {
            // Not the active slot — dim etched grey.
            drawEtched(g, "SAVE", saveRect, juce::Colour(0x38000000));
        }
        else
        {
            // Compute DIRTY: filled + current and live H differs from savedH.
            bool dirty = false;
            if (slot.filled)
            {
                for (int h = 0; h < 7; ++h)
                    if (std::abs(slot.savedH[(size_t) h] - liveH[(size_t) h]) > kDirtyEpsilon)
                    { dirty = true; break; }
            }

            const bool needsSave = ! slot.filled || dirty;
            if (needsSave)
            {
                // Blinking white — alternate bright body with dim body.
                // Halo stays on both phases so the pulse reads as throb,
                // not on/off flicker.
                const float alpha = blinkPhase ? 1.00f : 0.30f;
                drawGlowing(g, "SAVE", saveRect, kWhiteHalo,
                             juce::Colour::fromFloatRGBA(0.96f, 0.97f, 0.98f, alpha));
            }
            else
            {
                // Solid bright white — clean and saved.
                drawGlowing(g, "SAVE", saveRect, kWhiteHalo, kBrightBody);
            }
        }

        // ── DEL pill ────────────────────────────────────────────────────
        const auto delRect = deleteBounds[(size_t) col].toFloat();
        if (! slot.filled)
        {
            // Empty → very dim etched (disabled-look).
            drawEtched(g, "DEL", delRect, juce::Colour(0x20000000));
        }
        else
        {
            // Filled → standard etched (clickable), same body colour as
            // an inactive WordSelector word so it reads as a peer rather
            // than a coloured warning.
            drawEtched(g, "DEL", delRect, juce::Colour(0x80000000));
        }
    }
}

} // namespace kaigen::phantom
