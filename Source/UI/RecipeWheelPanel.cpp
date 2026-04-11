#include "RecipeWheelPanel.h"
#include "PhantomColours.h"
#include "../Parameters.h"

static const char* kAmpIDs[7] = {
    ParamID::RECIPE_H2, ParamID::RECIPE_H3, ParamID::RECIPE_H4,
    ParamID::RECIPE_H5, ParamID::RECIPE_H6, ParamID::RECIPE_H7, ParamID::RECIPE_H8
};

static const char* kPresetNames[5] = { "Warm", "Aggr", "Hollow", "Dense", "Custom" };

static const float* kPresetAmps[4] = {
    kWarmAmps, kAggressiveAmps, kHollowAmps, kDenseAmps
};

//==============================================================================
RecipeWheelPanel::RecipeWheelPanel(juce::AudioProcessorValueTreeState& a)
    : apvts(a)
{
    addAndMakeVisible(wheel);

    juce::TextButton* btns[5] = { &warmBtn, &aggrBtn, &hollowBtn, &denseBtn, &customBtn };
    for (int i = 0; i < 5; ++i)
    {
        btns[i]->setRadioGroupId(100);
        btns[i]->setClickingTogglesState(true);

        const int idx = i;
        btns[i]->onClick = [this, idx]()
        {
            if (auto* param = apvts.getParameter(ParamID::RECIPE_PRESET))
                param->setValueNotifyingHost((float)idx / 4.0f);
        };

        addAndMakeVisible(*btns[i]);
    }

    // Warm starts toggled on
    warmBtn.setToggleState(true, juce::dontSendNotification);

    apvts.addParameterListener(ParamID::RECIPE_PRESET, this);

    // Initialise wheel to Warm preset
    applyPreset(0);
}

RecipeWheelPanel::~RecipeWheelPanel()
{
    apvts.removeParameterListener(ParamID::RECIPE_PRESET, this);
}

//==============================================================================
void RecipeWheelPanel::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == ParamID::RECIPE_PRESET)
    {
        const int idx = juce::roundToInt(newValue * 4.0f);
        juce::MessageManager::callAsync([this, idx]() { applyPreset(idx); });
    }
}

void RecipeWheelPanel::applyPreset(int presetIndex)
{
    presetIndex = juce::jlimit(0, 4, presetIndex);

    // Sync toggle buttons
    juce::TextButton* btns[5] = { &warmBtn, &aggrBtn, &hollowBtn, &denseBtn, &customBtn };
    for (int i = 0; i < 5; ++i)
        btns[i]->setToggleState(i == presetIndex, juce::dontSendNotification);

    std::array<float, 7> amps;

    if (presetIndex < 4)
    {
        // Hardcoded preset — read from constant arrays
        const float* src = kPresetAmps[presetIndex];
        for (int i = 0; i < 7; ++i)
            amps[i] = src[i];

        // Write to APVTS so audio engine picks them up (0-1 -> 0-100 range)
        for (int i = 0; i < 7; ++i)
        {
            if (auto* param = apvts.getParameter(kAmpIDs[i]))
                param->setValueNotifyingHost(
                    param->getNormalisableRange().convertTo0to1(amps[i] * 100.0f));
        }
    }
    else
    {
        // Custom — read current APVTS values
        for (int i = 0; i < 7; ++i)
            amps[i] = apvts.getRawParameterValue(kAmpIDs[i])->load() / 100.0f;
    }

    wheel.setHarmonicAmplitudes(amps);
    wheel.setPresetName(kPresetNames[presetIndex]);
}

//==============================================================================
void RecipeWheelPanel::updatePitch(float hz)
{
    wheel.updatePitch(hz);
}

//==============================================================================
void RecipeWheelPanel::paint(juce::Graphics& g)
{
    // Stamp label at top
    g.setFont(juce::Font(5.5f, juce::Font::bold));
    g.setColour(PhantomColours::textDim.withAlpha(0.12f));
    g.drawText("RECIPE ENGINE \xc2\xb7 H2-H8",
               getLocalBounds().removeFromTop(14),
               juce::Justification::centred, false);
}

void RecipeWheelPanel::resized()
{
    auto area = getLocalBounds();

    // 14px top for stamp label
    area.removeFromTop(14);

    // 20px preset strip at bottom
    auto presetStrip = area.removeFromBottom(20);

    // RecipeWheel fills remaining space, kept square, centred
    const int side = juce::jmin(area.getWidth(), area.getHeight());
    const int cx   = area.getX() + area.getWidth()  / 2;
    const int cy   = area.getY() + area.getHeight()  / 2;
    wheel.setBounds(cx - side / 2, cy - side / 2, side, side);

    // 5 buttons evenly divided across preset strip
    const int btnW = presetStrip.getWidth() / 5;
    juce::TextButton* btns[5] = { &warmBtn, &aggrBtn, &hollowBtn, &denseBtn, &customBtn };
    for (int i = 0; i < 5; ++i)
        btns[i]->setBounds(presetStrip.removeFromLeft(btnW));
}
