// Source/UI/widgets/EyeButton.cpp
#include "EyeButton.h"

namespace kaigen::phantom
{

EyeButton::EyeButton()
    : juce::Button("eye")
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void EyeButton::setIsOpen(bool open)
{
    if (eyeOpen != open)
    {
        eyeOpen = open;
        repaint();
    }
}

void EyeButton::clicked()
{
    eyeOpen = ! eyeOpen;
    repaint();
    if (onToggle) onToggle(eyeOpen);
}

void EyeButton::paintButton(juce::Graphics& g,
                             bool shouldDrawButtonAsHighlighted,
                             bool /*shouldDrawButtonAsDown*/)
{
    const auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    const float cx    = bounds.getCentreX();
    const float cy    = bounds.getCentreY();
    const float halfW = bounds.getWidth()  * 0.40f;
    const float halfH = bounds.getHeight() * 0.28f;

    // Build the eye path — lens shape when open, single eyelid curve when closed.
    juce::Path eye;
    if (eyeOpen)
    {
        eye.startNewSubPath(cx - halfW, cy);
        eye.quadraticTo(cx, cy - halfH, cx + halfW, cy);
        eye.quadraticTo(cx, cy + halfH, cx - halfW, cy);
        eye.closeSubPath();
    }
    else
    {
        eye.startNewSubPath(cx - halfW, cy);
        eye.quadraticTo(cx, cy + halfH * 0.65f, cx + halfW, cy);
    }

    const juce::PathStrokeType stroke(1.4f,
                                       juce::PathStrokeType::curved,
                                       juce::PathStrokeType::rounded);

    // Etched white shadow (1 px below) — matches WordSelector / EtchedToggle.
    g.setColour(juce::Colour(0x80FFFFFF));
    g.strokePath(eye, stroke, juce::AffineTransform::translation(0.0f, 1.0f));

    // Body — slightly darker on hover.
    const juce::Colour body = shouldDrawButtonAsHighlighted
        ? juce::Colour(0xb0000000)
        : juce::Colour(0x90000000);
    g.setColour(body);
    g.strokePath(eye, stroke);

    // Pupil — only when the eye is open. Etched shadow + body.
    if (eyeOpen)
    {
        const float pr = halfH * 0.45f;
        g.setColour(juce::Colour(0x80FFFFFF));
        g.fillEllipse(cx - pr, cy - pr + 1.0f, pr * 2.0f, pr * 2.0f);
        g.setColour(body);
        g.fillEllipse(cx - pr, cy - pr, pr * 2.0f, pr * 2.0f);
    }
}

} // namespace kaigen::phantom
