#include "HeaderBar.h"
#include "PhantomColours.h"
#include "../Parameters.h"

HeaderBar::HeaderBar(juce::AudioProcessorValueTreeState& apvtsRef)
    : apvts(apvtsRef)
{
    // ── Effect button (radio group, index 0) ────────────────────────────
    effectBtn.setClickingTogglesState(true);
    effectBtn.setRadioGroupId(1);
    effectBtn.setToggleState(true, juce::dontSendNotification);
    effectBtn.onClick = [this]
    {
        if (auto* param = apvts.getParameter(ParamID::MODE))
            param->setValueNotifyingHost(0.0f);
    };
    addAndMakeVisible(effectBtn);

    // ── Instrument button (radio group, index 1) ────────────────────────
    instrumentBtn.setClickingTogglesState(true);
    instrumentBtn.setRadioGroupId(1);
    instrumentBtn.onClick = [this]
    {
        if (auto* param = apvts.getParameter(ParamID::MODE))
            param->setValueNotifyingHost(1.0f);
    };
    addAndMakeVisible(instrumentBtn);
}

// ── Helpers ──────────────────────────────────────────────────────────────

void HeaderBar::drawStippleTexture(juce::Graphics& g)
{
    auto b = getLocalBounds();
    g.setColour(juce::Colour(0x05ffffff));

    for (int y = 0; y < b.getHeight(); y += 3)
        for (int x = 0; x < b.getWidth(); x += 3)
            g.fillEllipse(static_cast<float>(x),
                          static_cast<float>(y),
                          1.0f, 1.0f);
}

void HeaderBar::drawBottomGlowSeam(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    float seamY = b.getBottom() - 1.0f;

    // Radial glow — brighter at horizontal centre, fading to the edges
    juce::ColourGradient glow(juce::Colour(0x30ffffff),
                               b.getCentreX(), seamY,
                               juce::Colours::transparentBlack,
                               b.getX(), seamY,
                               /* isRadial */ true);
    g.setGradientFill(glow);
    g.fillRect(b.getX(), seamY - 4.0f, b.getWidth(), 6.0f);

    // Bright 1-px seam line
    g.setColour(PhantomColours::seamGlow);
    g.drawHorizontalLine(static_cast<int>(seamY),
                         b.getX() + b.getWidth() * 0.04f,
                         b.getRight() - b.getWidth() * 0.04f);
}

void HeaderBar::drawEtchedText(juce::Graphics& g,
                                const juce::String& text,
                                juce::Rectangle<float> area,
                                const juce::Font& font,
                                juce::Justification justification)
{
    g.setFont(font);

    // Shadow pass — offset 1 px up (carved recess)
    g.setColour(PhantomColours::etchDark);
    g.drawText(text, area.translated(0.0f, -1.0f), justification, false);

    // Highlight pass — offset 1 px down (raised edge illusion)
    g.setColour(PhantomColours::etchLight);
    g.drawText(text, area.translated(0.0f, 1.0f), justification, false);

    // Primary metallic pass — true position
    g.setColour(PhantomColours::metalC);
    g.drawText(text, area, justification, false);
}

// ── Component overrides ──────────────────────────────────────────────────

void HeaderBar::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // Base fill
    g.setColour(PhantomColours::panelDark);
    g.fillRect(b);

    // Subtle top-edge darkening gradient
    {
        juce::ColourGradient topFade(juce::Colour(0x28000000), b.getX(), b.getY(),
                                      juce::Colours::transparentBlack, b.getX(), b.getY() + 8.0f,
                                      false);
        g.setGradientFill(topFade);
        g.fillRect(b);
    }

    drawStippleTexture(g);
    drawBottomGlowSeam(g);

    // ── "PHANTOM" — large etched title ──────────────────────────────────
    {
        juce::Font phantomFont("Helvetica Neue", 22.0f, juce::Font::plain);

        // Wide letter-spacing via GlyphArrangement
        juce::GlyphArrangement ga;
        ga.addFittedText(phantomFont, "PHANTOM",
                         0.0f, 0.0f, 999.0f, 40.0f,
                         juce::Justification::left, 1);
        ga.justifyGlyphs(0, ga.getNumGlyphs(),
                         b.getX() + 12.0f, b.getCentreY() - 14.0f,
                         400.0f, 28.0f,
                         juce::Justification::centredLeft);

        auto textArea = juce::Rectangle<float>(b.getX() + 12.0f,
                                                b.getCentreY() - 14.0f,
                                                400.0f, 28.0f);

        g.setFont(phantomFont);

        // Etch dark — 1 px up
        g.setColour(PhantomColours::etchDark);
        ga.draw(g, juce::AffineTransform::translation(0.0f, -1.0f));

        // Etch light — 1 px down
        g.setColour(PhantomColours::etchLight);
        ga.draw(g, juce::AffineTransform::translation(0.0f, 1.0f));

        // Metallic — true position
        g.setColour(PhantomColours::metalC);
        ga.draw(g);
    }

    // ── "KAIGEN" — smaller right-aligned sub-label ───────────────────────
    {
        juce::Font kaiFont("Helvetica Neue", 13.0f, juce::Font::plain);

        // Reserve right side area above mode buttons
        float rightEdge = b.getRight() - 12.0f;
        float btnLeft   = b.getRight() - 200.0f;  // approximate buttons region left
        juce::Rectangle<float> kaiArea(btnLeft, b.getY() + 6.0f,
                                        rightEdge - btnLeft, 16.0f);

        drawEtchedText(g, "KAIGEN", kaiArea, kaiFont,
                       juce::Justification::centredRight);
    }
}

void HeaderBar::resized()
{
    auto b = getLocalBounds();

    // Two mode-toggle buttons, right-aligned, centred vertically
    constexpr int btnW  = 80;
    constexpr int btnH  = 22;
    constexpr int gap   = 4;
    constexpr int rightMargin = 12;

    int totalW = btnW * 2 + gap;
    int btnX   = b.getRight()  - rightMargin - totalW;
    int btnY   = b.getCentreY() - btnH / 2;

    effectBtn    .setBounds(btnX,          btnY, btnW, btnH);
    instrumentBtn.setBounds(btnX + btnW + gap, btnY, btnW, btnH);
}
