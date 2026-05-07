// Source/UI/widgets/LinkButton.cpp
#include "LinkButton.h"
#include "../Theme.h"
#include "PhantomNativeAssets.h"

namespace kaigen::phantom
{

LinkButton::LinkButton()
{
    icon = juce::Drawable::createFromImageData(PhantomNativeAssets::link_icon_svg,
                                                PhantomNativeAssets::link_icon_svgSize);
    if (icon != nullptr)
    {
        icon->replaceColour(juce::Colours::black, Theme::textSecondary);
    }
    setSize(28, 28);
}

LinkButton::~LinkButton() = default;

void LinkButton::setLinked(bool shouldBeLinked)
{
    if (linked != shouldBeLinked)
    {
        linked = shouldBeLinked;
        // Re-load + re-tint the icon for the new state.
        icon = juce::Drawable::createFromImageData(PhantomNativeAssets::link_icon_svg,
                                                    PhantomNativeAssets::link_icon_svgSize);
        if (icon != nullptr)
        {
            icon->replaceColour(juce::Colours::black,
                                linked ? Theme::steelBlue : Theme::textSecondary);
        }
        if (onLinkChanged) onLinkChanged(linked);
        repaint();
    }
}

void LinkButton::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);

    // Background -- tinted when active.
    g.setColour(linked ? Theme::activeGlow : Theme::panelBg.withAlpha(0.4f));
    g.fillRoundedRectangle(bounds, 4.0f);

    if (icon != nullptr)
    {
        icon->setTransformToFit(bounds.reduced(4.0f), juce::RectanglePlacement::centred);
        icon->draw(g, 1.0f);
    }
}

void LinkButton::resized() {}

void LinkButton::mouseDown(const juce::MouseEvent&)
{
    setLinked(!linked);
}

} // namespace kaigen::phantom
