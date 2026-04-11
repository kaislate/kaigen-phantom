#include "NeumorphicPanel.h"
#include "PhantomColours.h"

void NeumorphicPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    float corner = 8.0f;

    // Outer neumorphic shadows
    {
        juce::DropShadow hlShadow(PhantomColours::panelShadowLight, 14, { -5, -5 });
        hlShadow.drawForRectangle(g, getLocalBounds().reduced(2));

        juce::DropShadow dkShadow(PhantomColours::panelShadowDark, 18, { 5, 5 });
        dkShadow.drawForRectangle(g, getLocalBounds().reduced(2));
    }

    // Panel fill
    g.setColour(PhantomColours::panelDark);
    g.fillRoundedRectangle(bounds, corner);

    // Inset shadow — bottom-right dark edge
    {
        juce::ColourGradient insetDark(juce::Colours::transparentBlack, bounds.getX(), bounds.getY(),
                                        juce::Colour(0x40000000), bounds.getRight(), bounds.getBottom(), false);
        g.setGradientFill(insetDark);
        g.fillRoundedRectangle(bounds, corner);
    }

    // Top-left light spill
    {
        float cx = bounds.getX() - bounds.getWidth() * 0.25f;
        float cy = bounds.getY() - bounds.getHeight() * 0.35f;
        float radius = bounds.getWidth() * 0.55f;
        juce::ColourGradient spill(PhantomColours::panelHighlight, cx, cy,
                                    juce::Colours::transparentBlack, cx + radius, cy + radius, true);
        g.setGradientFill(spill);
        g.fillRoundedRectangle(bounds, corner);
    }

    paintContent(g);
}
