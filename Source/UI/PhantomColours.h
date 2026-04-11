#pragma once
#include <JuceHeader.h>

namespace PhantomColours
{
    const juce::Colour background       { 0xff06060c };
    const juce::Colour panelDark        { 0xe8080810 };
    const juce::Colour panelHighlight   { 0x06ffffff };
    const juce::Colour panelShadowLight { 0x06ffffff };
    const juce::Colour panelShadowDark  { 0xc0000000 };
    const juce::Colour phosphorWhite    { 0xffffffff };
    const juce::Colour oledBlack        { 0xff000000 };
    const juce::Colour ridgeBright      { 0x28ffffff };
    const juce::Colour ridgeDark        { 0xb3000000 };
    const juce::Colour ridgeOuter       { 0x12ffffff };
    const juce::Colour trackDim         { 0x12ffffff };
    const juce::Colour textDim          { 0x48ffffff };
    const juce::Colour textGlow         { 0xffffffff };
    const juce::Colour seamGlow         { 0x29ffffff };
    const juce::Colour etchDark         { 0xf2000000 };
    const juce::Colour etchLight        { 0x12ffffff };

    // Metallic gradient stops for etched text
    const juce::Colour metalA { 0xffa0a8b8 };
    const juce::Colour metalB { 0xffdde0ec };
    const juce::Colour metalC { 0xff808898 };
    const juce::Colour metalD { 0xffc8ccd8 };
}
