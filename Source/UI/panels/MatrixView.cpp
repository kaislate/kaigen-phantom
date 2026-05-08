// Source/UI/panels/MatrixView.cpp
#include "MatrixView.h"
#include "../Theme.h"

namespace kaigen::phantom
{

MatrixView::MatrixView(PhantomProcessor& p, juce::AudioProcessorValueTreeState& a)
    : processor(p), apvts(a)
{
}

MatrixView::~MatrixView() = default;

juce::Rectangle<int> MatrixView::getCardBounds() const noexcept
{
    // MatrixView card spans full editor width minus margin (unlike PresetBrowser's
    // fixed 600px card) — Tasks 4-5 need the horizontal real estate for the
    // destination grid columns.
    return getLocalBounds().reduced(kCardMargin);
}

void MatrixView::visibilityChanged()
{
    if (isVisible()) repaint();
}

void MatrixView::paint(juce::Graphics& g)
{
    // Backdrop scrim.
    g.fillAll(juce::Colour(0xc0000000));

    // Centered card.
    auto card = getCardBounds();

    g.setColour(Theme::matrixBg);
    g.fillRoundedRectangle(card.toFloat(), 6.0f);
    g.setColour(Theme::panelBorder);
    g.drawRoundedRectangle(card.toFloat(), 6.0f, 1.0f);

    // Placeholder title bar — Tasks 4-5 replace this with real content.
    auto titleBar = card.removeFromTop(40).reduced(16, 8);
    g.setColour(Theme::textPrimary);
    g.setFont(juce::FontOptions("Space Grotesk", 14.0f, juce::Font::bold));
    g.drawText("Modulation Matrix", titleBar.toFloat(), juce::Justification::centredLeft, false);

    g.setColour(Theme::textDim);
    g.setFont(juce::FontOptions("Space Grotesk", 11.0f, juce::Font::plain));
    g.drawText("Modulator strip + destination grid (Tasks 4-5)",
               card.toFloat(), juce::Justification::centred, false);
}

void MatrixView::resized()
{
    // Tasks 4-5 add child components.
}

void MatrixView::mouseDown(const juce::MouseEvent& e)
{
    // Click outside the card → dismiss.
    if (! getCardBounds().contains(e.getPosition()))
        setVisible(false);
}

} // namespace kaigen::phantom
