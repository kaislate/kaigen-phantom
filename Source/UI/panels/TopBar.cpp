// Source/UI/panels/TopBar.cpp
#include "TopBar.h"
#include "../Theme.h"

namespace kaigen::phantom
{

TopBar::TopBar(PhantomProcessor& p, juce::AudioProcessorValueTreeState& a)
    : presetSelector(p, a)
{
    addAndMakeVisible(presetSelector);
}

TopBar::~TopBar() = default;

void TopBar::paint(juce::Graphics& g)
{
    g.fillAll(Theme::panelBg);
    g.setColour(Theme::panelBorder);
    g.drawHorizontalLine(getHeight() - 1, 0.0f, (float) getWidth());
}

void TopBar::resized()
{
    auto area = getLocalBounds();

    // Center the preset selector horizontally; ~600px wide.
    const int selectorW = juce::jmin(600, area.getWidth() - 200);
    const int selectorX = (area.getWidth() - selectorW) / 2;
    presetSelector.setBounds(selectorX, 0, selectorW, area.getHeight());
}

} // namespace kaigen::phantom
