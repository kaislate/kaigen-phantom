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
        else if (! slot.filled)
        {
            // Empty + current → blinking red (calling for save).
            const juce::Colour body = blinkPhase
                ? juce::Colour::fromFloatRGBA(0.95f, 0.20f, 0.18f, 1.00f)
                : juce::Colour::fromFloatRGBA(0.95f, 0.20f, 0.18f, 0.35f);
            drawGlowing(g, "SAVE", saveRect,
                         juce::Colour::fromFloatRGBA(1.0f, 0.30f, 0.25f, 1.0f),
                         body);
        }
        else
        {
            // Filled + current → DIRTY check.
            bool dirty = false;
            for (int h = 0; h < 7; ++h)
                if (std::abs(slot.savedH[(size_t) h] - liveH[(size_t) h]) > kDirtyEpsilon)
                { dirty = true; break; }

            if (dirty)
            {
                const juce::Colour body = blinkPhase
                    ? juce::Colour::fromFloatRGBA(0.95f, 0.20f, 0.18f, 1.00f)
                    : juce::Colour::fromFloatRGBA(0.95f, 0.20f, 0.18f, 0.35f);
                drawGlowing(g, "SAVE", saveRect,
                             juce::Colour::fromFloatRGBA(1.0f, 0.30f, 0.25f, 1.0f),
                             body);
            }
            else
            {
                // Solid green glow.
                drawGlowing(g, "SAVE", saveRect,
                             juce::Colour::fromFloatRGBA(0.35f, 0.95f, 0.45f, 1.0f),
                             juce::Colour::fromFloatRGBA(0.30f, 0.95f, 0.40f, 1.0f));
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
            // Filled → standard etched with a subtle red tint.
            drawEtched(g, "DEL", delRect,
                        juce::Colour::fromFloatRGBA(0.70f, 0.15f, 0.13f, 0.85f));
        }
    }
}

} // namespace kaigen::phantom
