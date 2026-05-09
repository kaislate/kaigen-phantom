// Source/UI/Theme.h
#pragma once
#include <juce_graphics/juce_graphics.h>

namespace kaigen::phantom::Theme
{
    // ── Outer body / dark backgrounds ────────────────────────────────────
    inline const juce::Colour editorBg          { 0xff0a0c10 };  // body bg
    inline const juce::Colour matrixBg          { 0xff0a0c10 };  // matrix overlay
    inline const juce::Colour modPanelBg        { 0xf20E1116 };  // 95% — mod panel (slot row)
    inline const juce::Colour modPanelBorder    { 0x0dffffff };  // 5% white top border
    inline const juce::Colour advancedBg        { 0xff0A0B0C };  // advanced collapse panel

    // ── Silver panel gradient stops (radial-gradient ellipse at 28%/18%) ─
    inline const juce::Colour panelRadialA      { 0xffCACCCE };  // 0%
    inline const juce::Colour panelRadialB      { 0xffBBBDBF };  // 48%
    inline const juce::Colour panelRadialC      { 0xffAEAFB1 };  // 100%

    // ── Header / top bar gradient (linear 180deg) ────────────────────────
    inline const juce::Colour headerHi          { 0x61ffffff };  // 38% white top
    inline const juce::Colour headerLo          { 0x0affffff };  // 4% white bottom
    inline const juce::Colour headerSeparator   { 0x12000000 };  // 7% black hairline

    // ── Inset card surface (sub-panels inside silver) ────────────────────
    inline const juce::Colour insetSurfaceTint  { 0x05000000 };  // 2% black wash
    inline const juce::Colour insetShadowDark   { 0x1f000000 };  // ~12% black
    inline const juce::Colour insetShadowLight  { 0xb8DCDEE2 };  // ~72% silver

    // ── Pitch-black inset (visualizers) ──────────────────────────────────
    inline const juce::Colour vizSurface        { 0xff000000 };
    inline const juce::Colour vizShadowInner    { 0xff000000 };  // inset 3px 3px 12px
    inline const juce::Colour vizHighlightOuter { 0x4dffffff };  // -2px -2px 6px white
    inline const juce::Colour vizShadowOuter    { 0x38000000 };  // 3px 3px 8px

    // ── Engraved (dark on light) text ────────────────────────────────────
    inline const juce::Colour textOnLightBody     { 0xb3000000 };  // 70% black
    inline const juce::Colour textOnLightLabel    { 0x38000000 };  // 22% black — section labels
    inline const juce::Colour textOnLightActive   { 0x9e000000 };  // 62% black — active toggle
    inline const juce::Colour textOnLightInactive { 0x47000000 };  // 28% black
    inline const juce::Colour textShadowEtch      { 0x80ffffff };  // 50% white — etched

    // ── Logos ────────────────────────────────────────────────────────────
    inline const juce::Colour logoPhantom       { 0xff656769 };
    inline const juce::Colour logoKaigen        { 0xff737577 };

    // ── Modulator identity colors ────────────────────────────────────────
    inline const juce::Colour macroTeal         { 0xff5DD3E0 };
    inline const juce::Colour lfoBlue           { 0xff4A90E2 };
    inline const juce::Colour randomPurple      { 0xff6B5DC9 };  // dot border
    inline const juce::Colour randomLabel       { 0xff9990E0 };  // name label
    inline const juce::Colour morphWhite        { 0xffEFEFF2 };

    // ── Accent blue (engine tabs, mode-btn active, build tag) ────────────
    inline const juce::Colour accentBlue        { 0xff4A8DD5 };
    inline const juce::Colour accentBlueBg      { 0x2e4A90E2 };
    inline const juce::Colour accentBlueBorder  { 0x8c4A90E2 };
    inline const juce::Colour accentBlueGlow    { 0x404A90E2 };
    inline const juce::Colour accentBlueMode    { 0xff4A90E2 };

    // ── Round-button (header, filter-link) ───────────────────────────────
    inline const juce::Colour btnActiveColor    { 0xe03773C3 };
    inline const juce::Colour btnActiveGlow     { 0x474682D2 };

    // ── Visualizer dark-surface text/buttons ─────────────────────────────
    inline const juce::Colour vizText           { 0xb3ffffff };
    inline const juce::Colour vizBtnBg          { 0x14ffffff };
    inline const juce::Colour vizBtnHoverBg     { 0x29ffffff };

    // ── Slot dot inner / matrix track / mask ─────────────────────────────
    inline const juce::Colour slotDotInner      { 0xff1f242c };
    inline const juce::Colour ringTrack         { 0x14ffffff };
    inline const juce::Colour ringMask          { 0xff0a0c10 };

    // ── Matrix-specific ──────────────────────────────────────────────────
    inline const juce::Colour mtxCellEmpty      { 0x9914181E };
    inline const juce::Colour mtxCellHover      { 0xb328303D };
    inline const juce::Colour mtxModStrip       { 0x8014181E };
    inline const juce::Colour mtxGridGap        { 0x66000000 };
    inline const juce::Colour mtxPopoverBg      { 0xff15181d };
    inline const juce::Colour mtxPopoverBorder  { 0x4d5DD3E0 };
    inline const juce::Colour mtxDanger         { 0xd9e86464 };

    // ── Misc ─────────────────────────────────────────────────────────────
    inline const juce::Colour clipRed           { 0xffe85050 };

    // ── Backwards-compat aliases (Task 8: deepBg removed — zero usages.
    //     Remaining 7 still have call sites; kept until a dedicated cleanup pass.)
    inline const juce::Colour& panelBg        = panelRadialB;
    inline const juce::Colour& panelBorder    = headerSeparator;
    inline const juce::Colour& textPrimary    = textOnLightBody;
    inline const juce::Colour& textSecondary  = textOnLightLabel;
    inline const juce::Colour& textDim        = textOnLightInactive;
    inline const juce::Colour& activeGlow     = accentBlueGlow;
    inline const juce::Colour& steelBlue      = accentBlue;

    // ── Compound paint helpers (defined in Theme.cpp) ────────────────────
    /** Paints a silver-plastic raised panel with neumorphic inner highlights. */
    void paintSilverPanel(juce::Graphics& g, juce::Rectangle<int> bounds);

    /** Paints a sub-panel "candy-inner" inset card (rounded, neumorphic dish). */
    void paintInsetCard(juce::Graphics& g, juce::Rectangle<int> bounds, float cornerRadius = 14.0f);

    /** Paints the editor's top header strip (linear gradient + bottom hairline). */
    void paintHeaderStrip(juce::Graphics& g, juce::Rectangle<int> bounds);

    /** Paints a pitch-black inset surface for visualizers (hard inner shadow). */
    void paintVisualizerInset(juce::Graphics& g, juce::Rectangle<int> bounds, float cornerRadius = 6.0f);

    /** Draws engraved-style text with 50% white text-shadow below. */
    void drawEtchedText(juce::Graphics& g, juce::String text, juce::Rectangle<int> bounds,
                        juce::Justification just, juce::Font font, juce::Colour textColour);
}
