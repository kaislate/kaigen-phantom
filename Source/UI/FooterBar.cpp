#include "FooterBar.h"
#include "PhantomColours.h"

FooterBar::FooterBar()
{
    // All six buttons toggle independently
    for (auto* btn : { &bypassBtn, &recipeBtn, &ghostBtn,
                       &binauralBtn, &deconflictBtn, &settingsBtn })
    {
        btn->setClickingTogglesState(true);
        addAndMakeVisible(btn);
    }

    // Recipe starts in the active (on) state
    recipeBtn.setToggleState(true, juce::dontSendNotification);
}

// ── Helpers ──────────────────────────────────────────────────────────────

void FooterBar::drawStippleTexture(juce::Graphics& g)
{
    auto b = getLocalBounds();
    g.setColour(juce::Colour(0x05ffffff));

    for (int y = 0; y < b.getHeight(); y += 3)
        for (int x = 0; x < b.getWidth(); x += 3)
            g.fillEllipse(static_cast<float>(x),
                          static_cast<float>(y),
                          1.0f, 1.0f);
}

void FooterBar::drawTopGlowSeam(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    float seamY = b.getY();

    // Radial glow — brighter at horizontal centre
    juce::ColourGradient glow(juce::Colour(0x30ffffff),
                               b.getCentreX(), seamY,
                               juce::Colours::transparentBlack,
                               b.getX(), seamY,
                               /* isRadial */ true);
    g.setGradientFill(glow);
    g.fillRect(b.getX(), seamY, b.getWidth(), 6.0f);

    // Bright 1-px seam line
    g.setColour(PhantomColours::seamGlow);
    g.drawHorizontalLine(static_cast<int>(seamY),
                         b.getX() + b.getWidth() * 0.04f,
                         b.getRight() - b.getWidth() * 0.04f);
}

// ── Component overrides ──────────────────────────────────────────────────

void FooterBar::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // Base fill
    g.setColour(PhantomColours::panelDark);
    g.fillRect(b);

    // Subtle bottom-edge darkening gradient
    {
        juce::ColourGradient btmFade(juce::Colours::transparentBlack, b.getX(), b.getBottom() - 8.0f,
                                      juce::Colour(0x28000000), b.getX(), b.getBottom(),
                                      false);
        g.setGradientFill(btmFade);
        g.fillRect(b);
    }

    drawStippleTexture(g);
    drawTopGlowSeam(g);

    // ── Version label — very dim, right-aligned ──────────────────────────
    {
        juce::Font versionFont(juce::FontOptions()
            .withName("Helvetica Neue")
            .withStyle("Light")
            .withHeight(10.0f));

        g.setFont(versionFont);
        g.setColour(juce::Colour(0xffffffff).withAlpha(0.07f));

        float rightMargin = 10.0f;
        g.drawText("KAIGEN PHANTOM \xc2\xb7 v0.1.0",
                   juce::Rectangle<float>(b.getX(),
                                          b.getY(),
                                          b.getWidth() - rightMargin,
                                          b.getHeight()),
                   juce::Justification::centredRight,
                   false);
    }
}

void FooterBar::resized()
{
    constexpr int btnW   = 68;
    constexpr int btnH   = 22;
    constexpr int gap    = 4;
    constexpr int leftX  = 8;

    auto b = getLocalBounds();
    int btnY = b.getCentreY() - btnH / 2;

    juce::TextButton* buttons[] = {
        &bypassBtn, &recipeBtn, &ghostBtn,
        &binauralBtn, &deconflictBtn, &settingsBtn
    };

    int x = leftX;
    for (auto* btn : buttons)
    {
        btn->setBounds(x, btnY, btnW, btnH);
        x += btnW + gap;
    }
}
