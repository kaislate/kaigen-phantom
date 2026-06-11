// Source/UI/widgets/ChoiceToggle.cpp
#include "ChoiceToggle.h"

namespace kaigen::phantom
{

namespace
{
    constexpr float kFontSize = 11.0f;
    constexpr float kKerning  = 0.18f;
}

ChoiceToggle::ChoiceToggle(juce::AudioProcessorValueTreeState& apvts,
                            const juce::String& choiceParamId,
                            juce::String label,
                            int onChoiceIndex)
    : juce::Button(label),
      apvtsRef(&apvts),
      labelText(std::move(label)),
      onIndex(onChoiceIndex)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);

    // Split "a_binaural_mode" → enginePrefix "a_" + leafName "binaural_mode".
    if (choiceParamId.startsWith("a_") || choiceParamId.startsWith("b_"))
    {
        enginePrefix = choiceParamId.substring(0, 2);
        leafName     = choiceParamId.substring(2);
    }
    else
    {
        leafName = choiceParamId;
    }

    rebuildAttachment(choiceParamId);
}

ChoiceToggle::~ChoiceToggle() = default;

void ChoiceToggle::setEnginePrefix(const juce::String& activePrefix)
{
    if (! isPerEngine()) return;
    if (activePrefix == enginePrefix) return;
    enginePrefix = activePrefix;
    rebuildAttachment(enginePrefix + leafName);
}

void ChoiceToggle::rebuildAttachment(const juce::String& fullId)
{
    attachment.reset();
    param = apvtsRef->getParameter(fullId);
    if (param == nullptr) return;

    attachment = std::make_unique<juce::ParameterAttachment>(
        *param,
        [this](float v) { onParamValueChanged(v); },
        nullptr);
    attachment->sendInitialUpdate();
}

void ChoiceToggle::onParamValueChanged(float newValue)
{
    // ParameterAttachment passes the un-normalised parameter value for
    // choice params (the int index as a float).
    const int idx = (int) std::round(newValue);
    const bool nowActive = (idx == onIndex);
    if (nowActive != isActiveState)
    {
        isActiveState = nowActive;
        repaint();
    }
}

void ChoiceToggle::clicked()
{
    if (! attachment) return;
    const int target = isActiveState ? 0 : onIndex;
    attachment->setValueAsCompleteGesture((float) target);
}

void ChoiceToggle::paintButton(juce::Graphics& g,
                                bool shouldDrawButtonAsHighlighted,
                                bool /*shouldDrawButtonAsDown*/)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto upper  = labelText.toUpperCase();
    const bool isHover = shouldDrawButtonAsHighlighted;

    juce::Font font(juce::FontOptions("Space Grotesk", kFontSize, juce::Font::bold));
    font.setExtraKerningFactor(kKerning);
    g.setFont(font);

    if (isActiveState)
    {
        for (int r = 4; r >= 1; --r)
        {
            g.setColour(juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 0.06f));
            g.drawText(upper, bounds.translated(-(float) r, 0.0f), juce::Justification::centred, false);
            g.drawText(upper, bounds.translated( (float) r, 0.0f), juce::Justification::centred, false);
            g.drawText(upper, bounds.translated(0.0f, -(float) r), juce::Justification::centred, false);
            g.drawText(upper, bounds.translated(0.0f,  (float) r), juce::Justification::centred, false);
        }
        g.setColour(juce::Colour(0xfff5f8fb));
        g.drawText(upper, bounds, juce::Justification::centred, false);
    }
    else
    {
        const auto textColour = isHover ? juce::Colour(0x80000000)
                                          : juce::Colour(0x38000000);
        g.setColour(juce::Colour(0x80FFFFFF));
        g.drawText(upper, bounds.translated(0.0f, 1.0f), juce::Justification::centred, false);
        g.setColour(textColour);
        g.drawText(upper, bounds, juce::Justification::centred, false);
    }
}

} // namespace kaigen::phantom
