// Source/UI/widgets/BuildTagPill.cpp
#include "BuildTagPill.h"
#include "../Theme.h"

namespace kaigen::phantom
{

namespace
{
    constexpr juce::uint32 kPillBg       = 0xff4a8dd5;
    constexpr juce::uint32 kPillTopHigh  = 0x33ffffff;   // ~20% white
    constexpr juce::uint32 kPillShadow   = 0x4d000000;   // ~30% black
    constexpr juce::uint32 kTextShadow   = 0x40000000;   // ~25% black
    constexpr float        kCorner       = 3.0f;
    constexpr int          kPadX         = 7;
    constexpr int          kPadY         = 2;

    juce::Font tagFont()
    {
        return juce::Font(juce::FontOptions(Theme::uiFontFamily(), 9.0f,
                                             juce::Font::bold))
                  .withExtraKerningFactor(0.10f);
    }
}

BuildTagPill::BuildTagPill(juce::String t) : text(std::move(t)) {}

juce::Rectangle<int> BuildTagPill::getNaturalBounds() const
{
    const auto f = tagFont();
    const int w = juce::roundToInt(f.getStringWidthFloat(text)) + kPadX * 2;
    return { 0, 0, w, kNaturalHeight };
}

void BuildTagPill::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(0.0f, 1.0f);

    // Outer drop shadow.
    {
        juce::DropShadow ds(juce::Colour(kPillShadow), 3, { 0, 1 });
        juce::Path p; p.addRoundedRectangle(bounds, kCorner);
        ds.drawForPath(g, p);
    }

    // Blue body.
    g.setColour(juce::Colour(kPillBg));
    g.fillRoundedRectangle(bounds, kCorner);

    // Inset top highlight — 1 px white line just inside the top edge.
    g.setColour(juce::Colour(kPillTopHigh));
    g.drawLine(bounds.getX() + kCorner, bounds.getY() + 0.7f,
                bounds.getRight() - kCorner, bounds.getY() + 0.7f, 0.6f);

    // Text — white bold with a soft 1 px shadow below for depth.
    g.setFont(tagFont());
    g.setColour(juce::Colour(kTextShadow));
    g.drawText(text, getLocalBounds().translated(0, 1).reduced(kPadX, kPadY),
               juce::Justification::centred, false);
    g.setColour(juce::Colours::white);
    g.drawText(text, getLocalBounds().reduced(kPadX, kPadY),
               juce::Justification::centred, false);
}

} // namespace kaigen::phantom
