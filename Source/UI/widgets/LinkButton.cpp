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
    setSize(28, 28);
}

LinkButton::~LinkButton() = default;

void LinkButton::setLinked(bool shouldBeLinked)
{
    if (linked != shouldBeLinked)
    {
        linked = shouldBeLinked;
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
        // Icon tint follows linked state.
        // The SVG uses stroke="currentColor" -- replaceColour swaps any explicit black.
        // For the stroke="currentColor" case, JUCE's Drawable resolves currentColor to
        // the foreground; we set that with g.setColour() before drawing.
        g.setColour(linked ? Theme::steelBlue : Theme::textSecondary);
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
