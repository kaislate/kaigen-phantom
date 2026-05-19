// Source/UI/widgets/EtchedToggle.cpp
#include "EtchedToggle.h"

namespace kaigen::phantom
{

namespace
{
    constexpr float kFontSize = 11.0f;
    constexpr float kKerning  = 0.18f;   // matches WordSelector spacing
}

EtchedToggle::EtchedToggle(juce::AudioProcessorValueTreeState& apvts,
                            const juce::String& boolParamId,
                            juce::String label)
    : juce::Button(label),
      labelText(std::move(label))
{
    setClickingTogglesState(true);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);

    if (auto* p = apvts.getParameter(boolParamId))
        attachment = std::make_unique<juce::ButtonParameterAttachment>(*p, *this, nullptr);
}

void EtchedToggle::paintButton(juce::Graphics& g,
                                bool shouldDrawButtonAsHighlighted,
                                bool /*shouldDrawButtonAsDown*/)
{
    const auto bounds = getLocalBounds().toFloat();
    const bool isActive = getToggleState();
    const bool isHover  = shouldDrawButtonAsHighlighted;
    const auto upper    = labelText.toUpperCase();

    juce::Font font(juce::FontOptions("Space Grotesk", kFontSize, juce::Font::bold));
    font.setExtraKerningFactor(kKerning);
    g.setFont(font);

    if (isActive)
    {
        // Backlit-glow effect — same recipe as WordSelector active word.
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
        // White shadow 1 px below glyph (etched effect).
        g.setColour(juce::Colour(0x80FFFFFF));
        g.drawText(upper, bounds.translated(0.0f, 1.0f), juce::Justification::centred, false);
        g.setColour(textColour);
        g.drawText(upper, bounds, juce::Justification::centred, false);
    }
}

} // namespace kaigen::phantom
