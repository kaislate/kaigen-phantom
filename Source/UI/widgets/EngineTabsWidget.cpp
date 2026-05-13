// Source/UI/widgets/EngineTabsWidget.cpp
#include "EngineTabsWidget.h"
#include "../Theme.h"
#include "../../PluginProcessor.h"

namespace kaigen::phantom
{

namespace
{
    // Match webview .engine-tabs container + .engine-tab CSS spec.
    constexpr juce::uint32 kContainerBg     = 0x99141820;   // rgba(20,24,30,0.6)
    constexpr juce::uint32 kContainerEdge   = 0x404a90e2;   // rgba(74,144,226,0.25)
    constexpr juce::uint32 kTabIdleText     = 0xff6a7a8a;
    constexpr juce::uint32 kTabHoverText    = 0xffc8d8ea;
    constexpr juce::uint32 kTabActiveText   = 0xfff5f8fb;
    constexpr juce::uint32 kTabActiveBg     = 0x2e4a90e2;   // rgba(74,144,226,0.18)
    constexpr juce::uint32 kTabActiveEdge   = 0x8c4a90e2;   // rgba(74,144,226,0.55)
    constexpr juce::uint32 kLinkActiveBg    = 0x387aa8d0;   // rgba(122,168,208,0.22)
    constexpr juce::uint32 kLinkActiveEdge  = 0x997aa8d0;   // rgba(122,168,208,0.60)
    constexpr juce::uint32 kLinkDivider     = 0x2e4a90e2;   // rgba(74,144,226,0.18)

    constexpr int kPadX      = 4;     // container padding
    constexpr int kPadY      = 3;
    constexpr int kTabGap    = 4;
    constexpr int kLinkGap   = 6;
    constexpr int kTabRadius = 4;
    constexpr int kContRad   = 6;
}

EngineTabsWidget::EngineTabsWidget(PhantomProcessor& p)
    : processor(p)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

EngineTabsWidget::~EngineTabsWidget() = default;

juce::Rectangle<int> EngineTabsWidget::tabRect(Hit which) const
{
    const auto inner = getLocalBounds().reduced(kPadX, kPadY);
    // Equal-width A / B / LINK with the link-divider gap before LINK.
    const int linkW = 50;
    const int abW   = (inner.getWidth() - linkW - kLinkGap - kTabGap) / 2;
    const int ax    = inner.getX();
    const int bx    = ax + abW + kTabGap;
    const int lx    = bx + abW + kLinkGap;

    switch (which)
    {
        case Hit::TabA: return { ax, inner.getY(), abW, inner.getHeight() };
        case Hit::TabB: return { bx, inner.getY(), abW, inner.getHeight() };
        case Hit::Link: return { lx, inner.getY(), linkW, inner.getHeight() };
        case Hit::None:
        default:        return {};
    }
}

EngineTabsWidget::Hit EngineTabsWidget::hitTest(juce::Point<int> p) const
{
    if (tabRect(Hit::TabA).contains(p)) return Hit::TabA;
    if (tabRect(Hit::TabB).contains(p)) return Hit::TabB;
    if (tabRect(Hit::Link).contains(p)) return Hit::Link;
    return Hit::None;
}

void EngineTabsWidget::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Container — dark blue-tinted pill with accent border.
    g.setColour(juce::Colour(kContainerBg));
    g.fillRoundedRectangle(bounds, (float) kContRad);
    g.setColour(juce::Colour(kContainerEdge));
    g.drawRoundedRectangle(bounds.reduced(0.5f), (float) kContRad, 1.0f);

    const auto focus    = processor.getEngineFocus();
    const bool aActive  = (focus.activeTab == ActiveTab::A);
    const bool bActive  = (focus.activeTab == ActiveTab::B);
    const bool linkOn   = focus.linkOn;

    auto paintTab = [&](Hit which, const juce::String& label,
                        bool isActive, bool isLink) {
        const auto r  = tabRect(which);
        const auto rf = r.toFloat();
        const bool isHover = (hoverHit == which);

        // Background tint when active.
        if (isActive)
        {
            g.setColour(juce::Colour(isLink ? kLinkActiveBg : kTabActiveBg));
            g.fillRoundedRectangle(rf, (float) kTabRadius);
            g.setColour(juce::Colour(isLink ? kLinkActiveEdge : kTabActiveEdge));
            g.drawRoundedRectangle(rf.reduced(0.5f), (float) kTabRadius, 1.0f);
        }

        // Label.
        const auto colour = isActive ? juce::Colour(kTabActiveText)
                          : isHover  ? juce::Colour(kTabHoverText)
                                      : juce::Colour(kTabIdleText);
        g.setColour(colour);
        auto font = juce::Font(juce::FontOptions(Theme::uiFontFamily(), 12.0f,
                                                  juce::Font::bold))
                       .withExtraKerningFactor(0.15f);
        g.setFont(font);
        g.drawText(label, r, juce::Justification::centred, false);
    };

    paintTab(Hit::TabA, "A",    aActive, false);
    paintTab(Hit::TabB, "B",    bActive, false);

    // Divider just left of LINK.
    {
        const auto linkR = tabRect(Hit::Link);
        const float xDiv = (float) (linkR.getX() - kLinkGap / 2);
        g.setColour(juce::Colour(kLinkDivider));
        g.drawLine(xDiv, bounds.getY() + 4.0f, xDiv, bounds.getBottom() - 4.0f, 1.0f);
    }

    paintTab(Hit::Link, "LINK", linkOn, true);
}

void EngineTabsWidget::mouseMove(const juce::MouseEvent& e)
{
    const auto h = hitTest(e.getPosition());
    if (h != hoverHit)
    {
        hoverHit = h;
        repaint();
    }
}

void EngineTabsWidget::mouseExit(const juce::MouseEvent&)
{
    if (hoverHit != Hit::None) { hoverHit = Hit::None; repaint(); }
}

void EngineTabsWidget::mouseDown(const juce::MouseEvent& e)
{
    const auto h = hitTest(e.getPosition());
    if (h == Hit::None) return;

    auto focus = processor.getEngineFocus();
    switch (h)
    {
        case Hit::TabA: focus.activeTab = ActiveTab::A; break;
        case Hit::TabB: focus.activeTab = ActiveTab::B; break;
        case Hit::Link: focus.linkOn    = ! focus.linkOn; break;
        case Hit::None: return;
    }
    processor.setEngineFocus(focus);
    repaint();
}

} // namespace kaigen::phantom
