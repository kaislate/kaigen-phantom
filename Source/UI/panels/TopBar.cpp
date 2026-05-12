// Source/UI/panels/TopBar.cpp
#include "TopBar.h"
#include "../Theme.h"
#include "../../PluginProcessor.h"

namespace kaigen::phantom
{

TopBar::TopBar(PhantomProcessor& p, juce::AudioProcessorValueTreeState& a)
    : presetSelector(p, a), engineTabs(p), processor(p)
{
    addAndMakeVisible(presetSelector);
    addAndMakeVisible(engineTabs);
}

TopBar::~TopBar() = default;

void TopBar::paint(juce::Graphics& g)
{
    Theme::paintHeaderStrip(g, getLocalBounds());

    // PHANTOM logo — left side, vertically centred.
    {
        const auto logoBounds = juce::Rectangle<int>(16, 0, 240, getHeight());
        const auto font = juce::Font(juce::FontOptions(Theme::uiFontFamily(), 22.0f, juce::Font::plain))
                              .withExtraKerningFactor(0.45f);
        // Bright white shadow below (72% white, CSS: 0 1px 0 rgba(255,255,255,0.72))
        g.setFont(font);
        g.setColour(juce::Colour(0xb8FFFFFF));
        g.drawText("PHANTOM", logoBounds.translated(0, 1), juce::Justification::centredLeft, false);
        // Foreground
        g.setColour(Theme::logoPhantom);
        g.drawText("PHANTOM", logoBounds, juce::Justification::centredLeft, false);
    }

    // KAIGEN logo — right side. CSS spec is font-weight: 400 (Regular).
    {
        const auto logoBounds = juce::Rectangle<int>(getWidth() - 120, 0, 104, getHeight());
        const auto font = juce::Font(juce::FontOptions(Theme::uiFontFamily(), 13.0f, juce::Font::plain))
                              .withExtraKerningFactor(0.46f);
        // White shadow below (60% white, CSS: 0 1px 0 rgba(255,255,255,0.60))
        g.setFont(font);
        g.setColour(juce::Colour(0x99FFFFFF));
        g.drawText("KAIGEN", logoBounds.translated(0, 1), juce::Justification::centredRight, false);
        // Foreground
        g.setColour(Theme::logoKaigen);
        g.drawText("KAIGEN", logoBounds, juce::Justification::centredRight, false);
    }
}

void TopBar::resized()
{
    auto area = getLocalBounds();

    // Center the preset selector horizontally; ~600px wide.
    const int selectorW = juce::jmin(600, area.getWidth() - 200);
    const int selectorX = (area.getWidth() - selectorW) / 2;
    presetSelector.setBounds(selectorX, 0, selectorW, area.getHeight());

    // Engine tabs sit to the right of the preset selector, vertically
    // centered in the strip.
    const int tabsX = selectorX + selectorW + 16;
    const int tabsY = (area.getHeight() - EngineTabsWidget::kNaturalHeight) / 2;
    engineTabs.setBounds(tabsX, tabsY,
                          EngineTabsWidget::kNaturalWidth,
                          EngineTabsWidget::kNaturalHeight);
}

void TopBar::mouseDown(const juce::MouseEvent& e)
{
    // Dev toggle: shift+click PHANTOM logo switches back to the webview
    // editor. Mirrors the webview-side shift+click that switches TO native
    // (Source/WebUI/phantom.js setupNativeUIToggle). Removed in Phase 6.
    const auto phantomLogoBounds = juce::Rectangle<int>(16, 0, 240, getHeight());
    if (! e.mods.isShiftDown()) return;
    if (! phantomLogoBounds.contains(e.getPosition())) return;

    auto state = processor.getEditorView();
    state.useNativeEditor = false;
    processor.setEditorView(state);
    processor.updateHostDisplay();

    juce::AlertWindow::showMessageBoxAsync(
        juce::MessageBoxIconType::InfoIcon,
        "WebView UI enabled",
        "Close and reopen the plugin window to see the WebView UI.");
}

} // namespace kaigen::phantom
