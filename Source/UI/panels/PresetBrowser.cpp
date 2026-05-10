// Source/UI/panels/PresetBrowser.cpp
#include "PresetBrowser.h"
#include "../Theme.h"
#include "../../PluginProcessor.h"
#include "../../PresetManager.h"

namespace kaigen::phantom
{

namespace
{
    // Webview palette — silver gradient card with dark-on-light text.
    inline juce::ColourGradient cardGradient(juce::Rectangle<float> b)
    {
        // CSS: linear-gradient(135deg, #BBBDBF 0%, #AEAFB1 100%)
        return { juce::Colour(0xffBBBDBF), b.getX(),     b.getY(),
                 juce::Colour(0xffAEAFB1), b.getRight(), b.getBottom(), false };
    }

    inline juce::ColourGradient sidebarGradient(juce::Rectangle<float> b)
    {
        // Slightly brighter than the card so the sidebar reads as inset.
        return { juce::Colour(0xffC5C7C9), b.getX(),     b.getY(),
                 juce::Colour(0xffBBBDBF), b.getRight(), b.getBottom(), false };
    }

    constexpr juce::uint32 kTextStrong  = 0xd9000000;   // ~85% — title, preset name
    constexpr juce::uint32 kTextBody    = 0xb3000000;   // ~70% — metadata values
    constexpr juce::uint32 kTextLabel   = 0x80000000;   // ~50% — labels, count
    constexpr juce::uint32 kTextDim     = 0x66000000;   // ~40% — placeholders
    constexpr juce::uint32 kRowHover    = 0x0f000000;   // ~6% black
    constexpr juce::uint32 kRowActive   = 0x1a000000;   // ~10% black
    constexpr juce::uint32 kBorderSoft  = 0x1f000000;   // ~12% black
}

PresetBrowser::PresetBrowser(PhantomProcessor& p, juce::AudioProcessorValueTreeState& a)
    : processor(p), apvts(a)
{
    closeButton.setButtonText(juce::String(juce::CharPointer_UTF8("\xE2\x9C\x95"))); // ✕
    closeButton.getProperties().set("phantom-style", "header-glyph");
    closeButton.onClick = [this] { setVisible(false); };
    addAndMakeVisible(closeButton);
}

PresetBrowser::~PresetBrowser() = default;

void PresetBrowser::visibilityChanged()
{
    if (! isVisible())
    {
        rows.clear();
        hoverRow = -1;
        return;
    }

    rows.clear();
    const auto all = processor.getPresetManager().getAllPresets();
    for (const auto& [packName, presets] : all)
    {
        rows.push_back({ {}, packName, true });
        for (const auto& p : presets)
            rows.push_back({ p.metadata.name, packName, false });
    }
    repaint();
}

juce::Rectangle<int> PresetBrowser::cardBounds() const
{
    const auto bounds = getLocalBounds();
    // 90% × 90% of the editor, capped so the card doesn't get unwieldy.
    const int w = juce::jmin((int) (bounds.getWidth()  * 0.90f), 920);
    const int h = juce::jmin((int) (bounds.getHeight() * 0.90f), 640);
    return { (bounds.getWidth()  - w) / 2,
             (bounds.getHeight() - h) / 2,
             w, h };
}

void PresetBrowser::loadPresetAt(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= (int) rows.size()) return;
    const auto& r = rows[(size_t) rowIndex];
    if (r.isHeader) return;
    if (processor.getPresetManager().loadPreset(apvts, r.name, r.pack))
    {
        if (onPresetSelected) onPresetSelected(r.name, r.pack);
        setVisible(false);
    }
}

void PresetBrowser::paint(juce::Graphics& g)
{
    // Backdrop scrim — CSS: rgba(0,0,0,0.4).
    g.fillAll(juce::Colour(0x66000000));

    const auto card = cardBounds();
    const auto cardF = card.toFloat();
    constexpr float corner = 8.0f;

    // Card body — silver gradient (CSS 135deg #BBBDBF → #AEAFB1).
    g.setGradientFill(cardGradient(cardF));
    g.fillRoundedRectangle(cardF, corner);

    // 1px outline (CSS rgba(0,0,0,0.15)).
    g.setColour(juce::Colour(0x26000000));
    g.drawRoundedRectangle(cardF.reduced(0.5f), corner, 1.0f);

    // Inset top highlight for the lifted-card feel.
    g.setColour(juce::Colour(0x99FFFFFF));
    g.drawLine(cardF.getX() + corner, cardF.getY() + 1.0f,
                cardF.getRight() - corner, cardF.getY() + 1.0f, 1.0f);

    // Layout slots inside the card.
    auto inner = card;
    auto sidebarBounds = inner.removeFromLeft(kSidebarW);
    auto previewBounds = inner.removeFromRight(kPreviewW);
    auto middleBounds  = inner;

    // ── Sidebar ─────────────────────────────────────────────────────────
    g.setGradientFill(sidebarGradient(sidebarBounds.toFloat()));
    g.fillRect(sidebarBounds);
    g.setColour(juce::Colour(kBorderSoft));
    g.drawLine((float) sidebarBounds.getRight(), (float) sidebarBounds.getY(),
                (float) sidebarBounds.getRight(), (float) sidebarBounds.getBottom(), 1.0f);

    // Sidebar label.
    g.setFont(juce::FontOptions(Theme::uiFontFamily(), 9.0f, juce::Font::bold));
    g.setColour(juce::Colour(kTextLabel));
    g.drawText("CATEGORIES",
               sidebarBounds.reduced(10, 8).removeFromTop(12),
               juce::Justification::topLeft, false);

    // Placeholder "All" entry — wired in commit 3.
    {
        auto allRow = sidebarBounds.reduced(6, 0)
                                    .withY(sidebarBounds.getY() + 28)
                                    .withHeight(24);
        // Active state (border-left + background) — for now "All" is permanent.
        g.setColour(juce::Colour(kRowActive));
        g.fillRoundedRectangle(allRow.toFloat(), 3.0f);
        g.setColour(juce::Colour(0x4d000000));
        g.fillRect(juce::Rectangle<float>((float) allRow.getX(), (float) allRow.getY(),
                                           3.0f, (float) allRow.getHeight()));
        g.setColour(juce::Colour(kTextStrong));
        g.setFont(juce::FontOptions(Theme::uiFontFamily(), 11.0f, juce::Font::bold));
        g.drawText("All", allRow.reduced(10, 0), juce::Justification::centredLeft, false);
    }

    // ── Preview pane (right) ────────────────────────────────────────────
    g.setGradientFill(sidebarGradient(previewBounds.toFloat()));
    g.fillRect(previewBounds);
    g.setColour(juce::Colour(kBorderSoft));
    g.drawLine((float) previewBounds.getX(), (float) previewBounds.getY(),
                (float) previewBounds.getX(), (float) previewBounds.getBottom(), 1.0f);

    g.setFont(juce::FontOptions(Theme::uiFontFamily(), 9.0f, juce::Font::bold));
    g.setColour(juce::Colour(kTextLabel));
    g.drawText("PREVIEW",
               previewBounds.reduced(12, 10).removeFromTop(12),
               juce::Justification::topLeft, false);

    {
        auto previewBox = previewBounds.reduced(10, 0)
                                        .withTrimmedTop(28)
                                        .withTrimmedBottom(10);
        g.setColour(juce::Colour(0x4dFFFFFF));
        g.fillRoundedRectangle(previewBox.toFloat(), 3.0f);
        g.setColour(juce::Colour(kBorderSoft));
        g.drawRoundedRectangle(previewBox.toFloat().reduced(0.5f), 3.0f, 1.0f);

        g.setColour(juce::Colour(kTextDim));
        g.setFont(juce::FontOptions(Theme::uiFontFamily(), 11.0f, juce::Font::plain));
        g.drawText("Select a preset", previewBox.reduced(12),
                    juce::Justification::centredTop, true);
    }

    // ── Middle: header + list ───────────────────────────────────────────
    auto headerBar = middleBounds.removeFromTop(kHeaderBarH);
    auto searchBar = middleBounds.removeFromTop(kSearchBarH);
    auto listArea  = middleBounds;

    // Header — title + count.
    {
        const auto totalPresets = (int) std::count_if(rows.begin(), rows.end(),
                                    [](const Row& r) { return ! r.isHeader; });
        auto h = headerBar.reduced(14, 0);
        g.setColour(juce::Colour(kTextStrong));
        g.setFont(juce::FontOptions(Theme::uiFontFamily(), 18.0f, juce::Font::bold));
        g.drawText("All Presets", h.toFloat(), juce::Justification::centredLeft, false);

        const auto countText = juce::String(totalPresets) + " preset" + (totalPresets == 1 ? "" : "s");
        g.setColour(juce::Colour(kTextLabel));
        g.setFont(juce::FontOptions(Theme::uiFontFamily(), 11.0f, juce::Font::plain));
        // Reserve 80px on the right for the close button.
        g.drawText(countText, h.withTrimmedRight(36).toFloat(),
                    juce::Justification::centredRight, false);
    }

    // Header bottom border.
    g.setColour(juce::Colour(kBorderSoft));
    g.drawLine((float) headerBar.getX(), (float) headerBar.getBottom(),
                (float) headerBar.getRight(), (float) headerBar.getBottom(), 1.0f);

    // Search bar — placeholder input field, wired in commit 2.
    {
        auto field = searchBar.reduced(12, 6);
        g.setColour(juce::Colour(0x99FFFFFF));
        g.fillRoundedRectangle(field.toFloat(), 3.0f);
        g.setColour(juce::Colour(kBorderSoft));
        g.drawRoundedRectangle(field.toFloat().reduced(0.5f), 3.0f, 1.0f);

        g.setColour(juce::Colour(kTextDim));
        g.setFont(juce::FontOptions(Theme::uiFontFamily(), 11.0f, juce::Font::plain));
        g.drawText("Search presets…", field.reduced(8, 0),
                    juce::Justification::centredLeft, false);
    }

    g.setColour(juce::Colour(kBorderSoft));
    g.drawLine((float) searchBar.getX(), (float) searchBar.getBottom(),
                (float) searchBar.getRight(), (float) searchBar.getBottom(), 1.0f);

    // List rows.
    {
        const auto rowsArea = listArea.reduced(8, 4);
        int y = rowsArea.getY();
        for (size_t i = 0; i < rows.size(); ++i)
        {
            const auto& r = rows[i];
            const int h = r.isHeader ? kHeaderHeight : kRowHeight;
            if (y + h > rowsArea.getBottom()) break;     // commit 5 adds scroll

            const auto rowBounds = juce::Rectangle<int>(rowsArea.getX(), y,
                                                         rowsArea.getWidth(), h);

            if (r.isHeader)
            {
                g.setColour(juce::Colour(kTextLabel));
                g.setFont(juce::FontOptions(Theme::uiFontFamily(), 10.0f, juce::Font::bold));
                g.drawText(r.pack.toUpperCase(),
                           rowBounds.reduced(8, 0),
                           juce::Justification::centredLeft, false);
            }
            else
            {
                if ((int) i == hoverRow)
                {
                    g.setColour(juce::Colour(kRowHover));
                    g.fillRoundedRectangle(rowBounds.toFloat(), 3.0f);
                }
                g.setColour(juce::Colour(kTextStrong));
                g.setFont(juce::FontOptions(Theme::uiFontFamily(), 11.0f, juce::Font::plain));
                g.drawText(r.name,
                           rowBounds.reduced(12, 0),
                           juce::Justification::centredLeft, false);
            }
            y += h;
        }
    }
}

void PresetBrowser::resized()
{
    const auto card = cardBounds();
    closeButton.setBounds(card.getRight() - 38, card.getY() + 8, 28, 24);
}

void PresetBrowser::mouseMove(const juce::MouseEvent& e)
{
    const auto card = cardBounds();
    auto inner = card;
    inner.removeFromLeft(kSidebarW);
    inner.removeFromRight(kPreviewW);
    auto listArea = inner.withTrimmedTop(kHeaderBarH + kSearchBarH).reduced(8, 4);

    int newHover = -1;
    int y = listArea.getY();
    for (size_t i = 0; i < rows.size(); ++i)
    {
        const auto& r = rows[i];
        const int h = r.isHeader ? kHeaderHeight : kRowHeight;
        if (y + h > listArea.getBottom()) break;
        if (! r.isHeader
            && e.x >= listArea.getX() && e.x < listArea.getRight()
            && e.y >= y               && e.y < y + h)
        {
            newHover = (int) i;
            break;
        }
        y += h;
    }
    if (newHover != hoverRow)
    {
        hoverRow = newHover;
        repaint();
    }
}

void PresetBrowser::mouseExit(const juce::MouseEvent&)
{
    if (hoverRow != -1)
    {
        hoverRow = -1;
        repaint();
    }
}

void PresetBrowser::mouseDown(const juce::MouseEvent& e)
{
    const auto card = cardBounds();

    // Click outside the card → dismiss.
    if (! card.contains(e.getPosition()))
    {
        setVisible(false);
        return;
    }

    // Hit-test: list area only (sidebar/preview/header inert in commit 1).
    auto inner = card;
    inner.removeFromLeft(kSidebarW);
    inner.removeFromRight(kPreviewW);
    const auto listArea = inner.withTrimmedTop(kHeaderBarH + kSearchBarH).reduced(8, 4);
    if (! listArea.contains(e.getPosition())) return;

    int y = listArea.getY();
    for (size_t i = 0; i < rows.size(); ++i)
    {
        const auto& r = rows[i];
        const int h = r.isHeader ? kHeaderHeight : kRowHeight;
        if (y + h > listArea.getBottom()) break;
        if (e.y >= y && e.y < y + h)
        {
            loadPresetAt((int) i);
            return;
        }
        y += h;
    }
}

} // namespace kaigen::phantom
