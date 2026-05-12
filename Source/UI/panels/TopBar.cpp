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

    // Build identifier — ops/QA verify the right DLL is loaded by reading
    // this tag. Bumped per release; this is the current dev tag.
    buildTag = std::make_unique<BuildTagPill>("MORPH-9");
    addAndMakeVisible(*buildTag);
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
    const int rowY    = (area.getHeight() - EngineTabsWidget::kNaturalHeight) / 2;
    const int btnSize = HeaderButton::kNaturalSize;
    const int btnY    = (area.getHeight() - btnSize) / 2;

    // ── Reserve the right-side chrome first, packed against the KAIGEN
    //    logo at x = getWidth() - 120. Everything is positioned right-to-
    //    left so the cluster stays glued to the KAIGEN edge as the editor
    //    resizes; the preset selector then takes whatever middle space
    //    remains.
    constexpr int kKaigenReserve = 120;
    int rx = area.getRight() - kKaigenReserve - 8;   // 8 px gap before KAIGEN

    if (buildTag)
    {
        const auto natural = buildTag->getNaturalBounds();
        rx -= natural.getWidth();
        buildTag->setBounds(rx, (area.getHeight() - natural.getHeight()) / 2,
                             natural.getWidth(), natural.getHeight());
        rx -= 10;
    }
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
