// Source/UI/panels/TopBar.cpp
#include "TopBar.h"
#include "../Theme.h"
#include "../../PluginProcessor.h"
#include "../../Parameters.h"

namespace kaigen::phantom
{

TopBar::TopBar(PhantomProcessor& p, juce::AudioProcessorValueTreeState& a)
    : presetSelector(p, a), engineTabs(p), modeToggle(a), processor(p)
{
    addAndMakeVisible(presetSelector);
    addAndMakeVisible(engineTabs);
    addAndMakeVisible(modeToggle);

    bypassBtn   = std::make_unique<HeaderButton>(a, ParamID::BYPASS,
                                                  HeaderButton::Icon::Bypass);
    settingsBtn = std::make_unique<HeaderButton>(HeaderButton::Icon::Settings);
    advancedBtn = std::make_unique<HeaderButton>(a, ParamID::ADVANCED_OPEN,
                                                  HeaderButton::Icon::AdvancedChevron);
    addAndMakeVisible(*bypassBtn);
    addAndMakeVisible(*settingsBtn);
    addAndMakeVisible(*advancedBtn);
}

TopBar::~TopBar() = default;

void TopBar::paint(juce::Graphics& g)
{
    Theme::paintHeaderStrip(g, getLocalBounds());

    // CSS spec for the two logos (Source/WebUI/styles.css):
    //   .logo-phantom: 22 px, weight 300, letter-spacing 10 px, color #656769,
    //                   text-shadow 0 1px 0 rgba(255,255,255,0.72),
    //                                 0 -0.5px 0 rgba(0,0,0,0.10);
    //   .logo-kaigen:  13 px, weight 400, letter-spacing 6 px,  color #737577,
    //                   text-shadow 0 1px 0 rgba(255,255,255,0.60),
    //                                 0 -0.5px 0 rgba(0,0,0,0.08);
    auto paintEtchedLogo = [&](const juce::String& label, juce::Rectangle<int> bounds,
                                float fontPx, float kerning, juce::Colour fg,
                                juce::Colour shadowBelow, juce::Colour shadowAbove,
                                juce::Justification just)
    {
        // No SG Light in the fallback stack — Segoe UI's regular is what the
        // webview falls back to in practice. Plain weight matches.
        const auto font = juce::Font(juce::FontOptions(Theme::uiFontFamily(), fontPx,
                                                        juce::Font::plain))
                                .withExtraKerningFactor(kerning);
        g.setFont(font);

        // Soft dark shadow ABOVE the glyph (CSS 0 -0.5px 0).
        g.setColour(shadowAbove);
        g.drawText(label, bounds.translated(0, -1), just, false);

        // Bright white shadow BELOW the glyph (CSS 0 1px 0).
        g.setColour(shadowBelow);
        g.drawText(label, bounds.translated(0, 1), just, false);

        g.setColour(fg);
        g.drawText(label, bounds, just, false);
    };

    paintEtchedLogo("PHANTOM",
                     juce::Rectangle<int>(16, 0, 240, getHeight()),
                     22.0f, 0.45f, Theme::logoPhantom,
                     juce::Colour(0xb8FFFFFF),   // 72% white
                     juce::Colour(0x1a000000),   // 10% black
                     juce::Justification::centredLeft);

    paintEtchedLogo("KAIGEN",
                     juce::Rectangle<int>(getWidth() - 120, 0, 104, getHeight()),
                     13.0f, 0.46f, Theme::logoKaigen,
                     juce::Colour(0x99FFFFFF),   // 60% white
                     juce::Colour(0x14000000),   // 8% black
                     juce::Justification::centredRight);
}

void TopBar::resized()
{
    auto area = getLocalBounds();
    const int rowY    = (area.getHeight() - EngineTabsWidget::kNaturalHeight) / 2;
    const int btnSize = HeaderButton::kNaturalSize;
    const int btnY    = (area.getHeight() - btnSize) / 2;

    // ── Right-side chrome packs against the KAIGEN logo at
    //    getWidth() - 120 (matching paint() above).
    constexpr int kKaigenReserve = 120;
    int rx = area.getRight() - kKaigenReserve - 8;

    if (advancedBtn) { rx -= btnSize; advancedBtn->setBounds(rx, btnY, btnSize, btnSize); rx -= 6; }
    if (settingsBtn) { rx -= btnSize; settingsBtn->setBounds(rx, btnY, btnSize, btnSize); rx -= 6; }
    if (bypassBtn)   { rx -= btnSize; bypassBtn  ->setBounds(rx, btnY, btnSize, btnSize); rx -= 12; }

    rx -= ModeTogglePill::kNaturalWidth;
    modeToggle.setBounds(rx, rowY,
                          ModeTogglePill::kNaturalWidth,
                          ModeTogglePill::kNaturalHeight);
    rx -= 10;

    rx -= EngineTabsWidget::kNaturalWidth;
    engineTabs.setBounds(rx, rowY,
                          EngineTabsWidget::kNaturalWidth,
                          EngineTabsWidget::kNaturalHeight);

    // ── Preset selector fills the middle space between the PHANTOM logo
    //    (left, paints at x=16, w=240) and the right-side chrome cluster.
    constexpr int kPhantomReserve = 240 + 16 + 12;   // logo bounds + 12 px gap
    const int selectorLeft  = kPhantomReserve;
    const int selectorRight = rx - 16;                // 16 px gap before engine tabs
    const int selectorW     = juce::jmax(380, selectorRight - selectorLeft);
    presetSelector.setBounds(selectorLeft, 0, selectorW, area.getHeight());
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
