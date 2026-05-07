// Source/UI/Theme.h
#pragma once
#include <juce_graphics/juce_graphics.h>

namespace kaigen::phantom::Theme
{
    // ── Accent + modulator type colors (matches existing CSS values) ─────
    inline const juce::Colour steelBlue       { 0xff4A90E2 };  // active toggles, primary accent
    inline const juce::Colour macroTeal       { 0xff5DD3E0 };
    inline const juce::Colour lfoBlue         { 0xff4A90E2 };
    inline const juce::Colour randomPurple    { 0xff9990E0 };
    inline const juce::Colour morphWhite      { 0xffEFEFF2 };

    // ── Surfaces ─────────────────────────────────────────────────────────
    inline const juce::Colour panelBg         { 0xff0E1116 };  // standard panel surface
    inline const juce::Colour matrixBg        { 0xff0A0C10 };  // matrix view background
    inline const juce::Colour panelBorder     { 0x14ffffff };  // 8% white panel borders
    inline const juce::Colour deepBg          { 0xff05070A };  // editor outermost background

    // ── Text ─────────────────────────────────────────────────────────────
    inline const juce::Colour textPrimary     { 0xffc8d8ea };  // primary readable text
    inline const juce::Colour textSecondary   { 0x73ffffff };  // 45% white — secondary labels
    inline const juce::Colour textDim         { 0x40ffffff };  // 25% white — placeholder labels

    // ── Highlights / glow ────────────────────────────────────────────────
    inline const juce::Colour activeGlow      { 0x335DD3E0 };  // 20% teal — active state shadow
    inline const juce::Colour clipRed         { 0xffe85050 };  // meter clip indicator
}
