#include "GlowSeam.h"
#include "PhantomColours.h"

void GlowSeam::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    bool horiz = (orientation == Orientation::Horizontal);

    // Soft bloom background
    if (horiz)
    {
        juce::ColourGradient bloom(juce::Colours::transparentBlack, b.getX(), b.getCentreY(),
                                    juce::Colour(0x0affffff), b.getCentreX(), b.getCentreY(), false);
        bloom.addColour(0.15, juce::Colours::transparentBlack);
        bloom.addColour(0.5, juce::Colour(0x0affffff));
        bloom.addColour(0.85, juce::Colours::transparentBlack);
        g.setGradientFill(bloom);
        g.fillRect(b);
    }
    else
    {
        juce::ColourGradient bloom(juce::Colours::transparentBlack, b.getCentreX(), b.getY(),
                                    juce::Colour(0x0affffff), b.getCentreX(), b.getCentreY(), false);
        bloom.addColour(0.15, juce::Colours::transparentBlack);
        bloom.addColour(0.5, juce::Colour(0x0affffff));
        bloom.addColour(0.85, juce::Colours::transparentBlack);
        g.setGradientFill(bloom);
        g.fillRect(b);
    }

    // Center bright line
    g.setColour(PhantomColours::seamGlow);
    if (horiz)
    {
        float y = b.getCentreY();
        float margin = b.getWidth() * 0.04f;
        g.drawHorizontalLine((int)y, b.getX() + margin, b.getRight() - margin);
    }
    else
    {
        float x = b.getCentreX();
        float margin = b.getHeight() * 0.04f;
        g.drawVerticalLine((int)x, b.getY() + margin, b.getBottom() - margin);
    }
}
