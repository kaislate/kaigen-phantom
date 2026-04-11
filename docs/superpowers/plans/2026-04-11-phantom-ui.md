# Phantom UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the complete Kaigen Phantom plugin GUI — white phosphor neumorphic style, recipe wheel with harmonic tanks, spectrum analyzer, and all 35 APVTS parameters bound to controls.

**Architecture:** Pure JUCE 2D Graphics rendering via a custom LookAndFeel. The editor owns a Timer for real-time data (pitch, spectrum, levels). Each control panel is a self-contained Component that creates its own APVTS attachments. Mode switching (Effect/Instrument) toggles panel visibility.

**Tech Stack:** JUCE 7.0.9, C++17, juce_audio_utils, juce_dsp (FFT)

**Spec:** `docs/superpowers/specs/2026-04-11-phantom-ui-design.md`

---

## File Map

| File | Responsibility |
|---|---|
| `Source/UI/PhantomColours.h` | Colour constants namespace |
| `Source/UI/PhantomLookAndFeel.h/cpp` | All custom rendering (knobs, buttons, panels) |
| `Source/UI/PhantomKnob.h/cpp` | Slider subclass with size enum and double-click reset |
| `Source/UI/NeumorphicPanel.h/cpp` | Base class for panel background drawing |
| `Source/UI/GlowSeam.h/cpp` | 3px glowing divider component |
| `Source/UI/HeaderBar.h/cpp` | Header with logo, labels, mode toggle, status |
| `Source/UI/FooterBar.h/cpp` | Footer with nav buttons and version label |
| `Source/UI/HarmonicEnginePanel.h/cpp` | Saturation (lg) + Strength (md) |
| `Source/UI/GhostPanel.h/cpp` | Amount (lg) + Threshold (md) + Mode toggle |
| `Source/UI/OutputPanel.h/cpp` | Gain (lg) |
| `Source/UI/PitchTrackerPanel.h/cpp` | Sensitivity (md) + Glide (md) |
| `Source/UI/SidechainPanel.h/cpp` | Amount + Attack + Release (all md) |
| `Source/UI/StereoPanel.h/cpp` | Width (lg) |
| `Source/UI/DeconflictionPanel.h/cpp` | Mode selector + MaxVoices + StaggerDelay |
| `Source/UI/RecipeWheel.h/cpp` | Custom painted wheel (rings, tanks, OLED center) |
| `Source/UI/RecipeWheelPanel.h/cpp` | Wheel + preset strip container |
| `Source/UI/SpectrumAnalyzer.h/cpp` | Bar + line modes, 80 bins |
| `Source/UI/LevelMeter.h/cpp` | Thin vertical I/O meter |
| `Source/PluginEditor.h/cpp` | Modified: layout, Timer, mode switching |
| `Source/PluginProcessor.h/cpp` | Modified: add atomics, spectrum FIFO |
| `CMakeLists.txt` | Modified: add new source files |

---

## Phase 1 — Foundation

### Task 1: PhantomColours.h

**Files:**
- Create: `Source/UI/PhantomColours.h`

- [ ] **Step 1: Create the UI directory and colour constants**

```cpp
// Source/UI/PhantomColours.h
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
```

- [ ] **Step 2: Verify it compiles**

Run: `cmake --build build --target KaigenPhantom 2>&1 | head -5`

This will fail because it's not in CMakeLists yet — that's expected. We just need a clean header with no syntax errors. We'll add it to CMake in Task 7.

- [ ] **Step 3: Commit**

```bash
git add Source/UI/PhantomColours.h
git commit -m "feat(ui): add PhantomColours constants header"
```

---

### Task 2: PhantomKnob

**Files:**
- Create: `Source/UI/PhantomKnob.h`
- Create: `Source/UI/PhantomKnob.cpp`

- [ ] **Step 1: Create PhantomKnob header**

```cpp
// Source/UI/PhantomKnob.h
#pragma once
#include <JuceHeader.h>

class PhantomKnob : public juce::Slider
{
public:
    enum class Size { Large, Medium };

    PhantomKnob(const juce::String& label, Size size, float defaultValue);

    Size  getKnobSize() const noexcept { return knobSize; }
    float getDefaultValue() const noexcept { return defaultVal; }
    int   getDiameter() const noexcept { return knobSize == Size::Large ? 100 : 72; }

    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    Size  knobSize;
    float defaultVal;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomKnob)
};
```

- [ ] **Step 2: Create PhantomKnob implementation**

```cpp
// Source/UI/PhantomKnob.cpp
#include "PhantomKnob.h"

PhantomKnob::PhantomKnob(const juce::String& label, Size size, float defaultValue)
    : knobSize(size), defaultVal(defaultValue)
{
    setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    setName(label);
}

void PhantomKnob::mouseDoubleClick(const juce::MouseEvent&)
{
    setValue(defaultVal, juce::sendNotificationSync);
}
```

- [ ] **Step 3: Commit**

```bash
git add Source/UI/PhantomKnob.h Source/UI/PhantomKnob.cpp
git commit -m "feat(ui): add PhantomKnob slider subclass with double-click reset"
```

---

### Task 3: NeumorphicPanel

**Files:**
- Create: `Source/UI/NeumorphicPanel.h`
- Create: `Source/UI/NeumorphicPanel.cpp`

- [ ] **Step 1: Create NeumorphicPanel header**

```cpp
// Source/UI/NeumorphicPanel.h
#pragma once
#include <JuceHeader.h>

class NeumorphicPanel : public juce::Component
{
public:
    NeumorphicPanel() = default;
    void paint(juce::Graphics& g) override;

protected:
    // Subclasses override this to paint their controls on top of the panel background
    virtual void paintContent(juce::Graphics& g) { juce::ignoreUnused(g); }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NeumorphicPanel)
};
```

- [ ] **Step 2: Create NeumorphicPanel implementation**

```cpp
// Source/UI/NeumorphicPanel.cpp
#include "NeumorphicPanel.h"
#include "PhantomColours.h"

void NeumorphicPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    float corner = 8.0f;

    // Outer neumorphic shadows
    {
        // Top-left highlight
        juce::DropShadow hlShadow(PhantomColours::panelShadowLight, 14, { -5, -5 });
        hlShadow.drawForRectangle(g, getLocalBounds().reduced(2));

        // Bottom-right shadow
        juce::DropShadow dkShadow(PhantomColours::panelShadowDark, 18, { 5, 5 });
        dkShadow.drawForRectangle(g, getLocalBounds().reduced(2));
    }

    // Panel fill
    g.setColour(PhantomColours::panelDark);
    g.fillRoundedRectangle(bounds, corner);

    // Inset shadow — bottom-right dark edge (simulated with gradient overlay)
    {
        juce::ColourGradient insetDark(juce::Colours::transparentBlack, bounds.getX(), bounds.getY(),
                                        juce::Colour(0x40000000), bounds.getRight(), bounds.getBottom(), false);
        g.setGradientFill(insetDark);
        g.fillRoundedRectangle(bounds, corner);
    }

    // Top-left light spill
    {
        float cx = bounds.getX() - bounds.getWidth() * 0.25f;
        float cy = bounds.getY() - bounds.getHeight() * 0.35f;
        float radius = bounds.getWidth() * 0.55f;
        juce::ColourGradient spill(PhantomColours::panelHighlight, cx, cy,
                                    juce::Colours::transparentBlack, cx + radius, cy + radius, true);
        g.setGradientFill(spill);
        g.fillRoundedRectangle(bounds, corner);
    }

    paintContent(g);
}
```

- [ ] **Step 3: Commit**

```bash
git add Source/UI/NeumorphicPanel.h Source/UI/NeumorphicPanel.cpp
git commit -m "feat(ui): add NeumorphicPanel base class with directional lighting"
```

---

### Task 4: GlowSeam

**Files:**
- Create: `Source/UI/GlowSeam.h`
- Create: `Source/UI/GlowSeam.cpp`

- [ ] **Step 1: Create GlowSeam**

```cpp
// Source/UI/GlowSeam.h
#pragma once
#include <JuceHeader.h>

class GlowSeam : public juce::Component
{
public:
    enum class Orientation { Horizontal, Vertical };
    explicit GlowSeam(Orientation o) : orientation(o) {}
    void paint(juce::Graphics& g) override;
private:
    Orientation orientation;
};
```

```cpp
// Source/UI/GlowSeam.cpp
#include "GlowSeam.h"
#include "PhantomColours.h"

void GlowSeam::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    bool horiz = (orientation == Orientation::Horizontal);

    // Soft bloom background
    if (horiz)
    {
        juce::ColourGradient bloom(juce::Colours::transparentBlack, b.getX(), b.getCentreY(),
                                    juce::Colour(0x0affffff), b.getCentreX(), b.getCentreY(), false);
        bloom.addColour(0.15, juce::Colours::transparentBlack);
        bloom.addColour(0.5, juce::Colour(0x0affffff));
        bloom.addColour(0.85, juce::Colours::transparentBlack);
        g.setGradientFill(bloom);
        g.fillRect(b.reduced(0.0f, 0.0f).withHeight(b.getHeight()));
    }
    else
    {
        juce::ColourGradient bloom(juce::Colours::transparentBlack, b.getCentreX(), b.getY(),
                                    juce::Colour(0x0affffff), b.getCentreX(), b.getCentreY(), false);
        bloom.addColour(0.15, juce::Colours::transparentBlack);
        bloom.addColour(0.5, juce::Colour(0x0affffff));
        bloom.addColour(0.85, juce::Colours::transparentBlack);
        g.setGradientFill(bloom);
        g.fillRect(b);
    }

    // Center bright line
    g.setColour(PhantomColours::seamGlow);
    if (horiz)
    {
        float y = b.getCentreY();
        float margin = b.getWidth() * 0.04f;
        g.drawHorizontalLine((int)y, b.getX() + margin, b.getRight() - margin);
    }
    else
    {
        float x = b.getCentreX();
        float margin = b.getHeight() * 0.04f;
        g.drawVerticalLine((int)x, b.getY() + margin, b.getBottom() - margin);
    }
}
```

- [ ] **Step 2: Commit**

```bash
git add Source/UI/GlowSeam.h Source/UI/GlowSeam.cpp
git commit -m "feat(ui): add GlowSeam divider component"
```

---

### Task 5: PhantomLookAndFeel

**Files:**
- Create: `Source/UI/PhantomLookAndFeel.h`
- Create: `Source/UI/PhantomLookAndFeel.cpp`

- [ ] **Step 1: Create PhantomLookAndFeel header**

```cpp
// Source/UI/PhantomLookAndFeel.h
#pragma once
#include <JuceHeader.h>

class PhantomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PhantomLookAndFeel();

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

    // Utility: draw text with multi-pass OLED glow effect
    static void drawGlowText(juce::Graphics& g, const juce::String& text,
                              juce::Rectangle<float> area, float fontSize,
                              juce::Justification justification = juce::Justification::centred);

    // Utility: draw the triple-ring OLED lip/ridge
    static void drawOledRidge(juce::Graphics& g, float cx, float cy, float radius);

private:
    juce::Font monoFont;
};
```

- [ ] **Step 2: Create PhantomLookAndFeel implementation**

```cpp
// Source/UI/PhantomLookAndFeel.cpp
#include "PhantomLookAndFeel.h"
#include "PhantomColours.h"
#include "PhantomKnob.h"

PhantomLookAndFeel::PhantomLookAndFeel()
{
    monoFont = juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::bold);

    setColour(juce::Slider::rotarySliderFillColourId, PhantomColours::phosphorWhite);
    setColour(juce::Slider::rotarySliderOutlineColourId, PhantomColours::trackDim);
}

void PhantomLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                           juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
    float diameter = juce::jmin(bounds.getWidth(), bounds.getHeight());
    float cx = bounds.getCentreX();
    float cy = bounds.getCentreY();
    float outerR = diameter * 0.5f;

    // Determine inset based on knob size
    float inset = 13.0f;
    float valueFontSize = 12.0f;
    if (auto* pk = dynamic_cast<PhantomKnob*>(&slider))
    {
        if (pk->getKnobSize() == PhantomKnob::Size::Medium)
        {
            inset = 9.0f;
            valueFontSize = 10.0f;
        }
    }
    float oledR = outerR - inset;

    // 1. Neumorphic outer shadows
    {
        juce::DropShadow hlShadow(PhantomColours::panelShadowLight, 12, { -5, -5 });
        hlShadow.drawForPath(g, juce::Path().addEllipse(cx - outerR, cy - outerR, diameter, diameter));

        juce::DropShadow dkShadow(PhantomColours::panelShadowDark, 16, { 6, 6 });
        dkShadow.drawForPath(g, juce::Path().addEllipse(cx - outerR, cy - outerR, diameter, diameter));
    }

    // 2. Volcano surface — radial gradient with directional light
    {
        juce::ColourGradient volcano(
            juce::Colour(0x21ffffff), cx - outerR * 0.35f, cy - outerR * 0.4f,  // bright top-left
            juce::Colour(0x99000000), cx + outerR * 0.5f, cy + outerR * 0.5f,   // dark bottom-right
            true);
        volcano.addColour(0.15, juce::Colour(0x0fffffff));
        volcano.addColour(0.4, juce::Colour(0x06ffffff));
        volcano.addColour(0.7, juce::Colour(0x80060610));
        g.setGradientFill(volcano);
        g.fillEllipse(cx - outerR, cy - outerR, diameter, diameter);
    }

    // 3. OLED well
    g.setColour(PhantomColours::oledBlack);
    g.fillEllipse(cx - oledR, cy - oledR, oledR * 2.0f, oledR * 2.0f);

    // 4. Lip/ridge
    drawOledRidge(g, cx, cy, oledR);

    // 5-7. Arc track + value + glow
    float arcR = (outerR + oledR) * 0.5f;
    float arcAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    {
        // Track (dim full sweep)
        juce::Path track;
        track.addArc(cx - arcR, cy - arcR, arcR * 2.0f, arcR * 2.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(PhantomColours::trackDim);
        g.strokePath(track, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Glow halo (wide, low opacity)
        if (sliderPos > 0.01f)
        {
            juce::Path glow;
            glow.addArc(cx - arcR, cy - arcR, arcR * 2.0f, arcR * 2.0f, rotaryStartAngle, arcAngle, true);
            g.setColour(PhantomColours::phosphorWhite.withAlpha(0.3f));
            g.strokePath(glow, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Value arc (bright, sharp)
        if (sliderPos > 0.01f)
        {
            juce::Path value;
            value.addArc(cx - arcR, cy - arcR, arcR * 2.0f, arcR * 2.0f, rotaryStartAngle, arcAngle, true);
            g.setColour(PhantomColours::phosphorWhite);
            g.strokePath(value, juce::PathStrokeType(2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // 8. Value text with OLED glow
    {
        juce::String valueText;
        double val = slider.getValue();
        // Format based on the parameter range
        if (val == (int)val && val >= -100 && val <= 200)
            valueText = juce::String((int)val) + (slider.getTextValueSuffix().isEmpty() ? "%" : slider.getTextValueSuffix());
        else
            valueText = slider.getTextFromValue(val);

        auto textArea = juce::Rectangle<float>(cx - oledR, cy - oledR * 0.4f, oledR * 2.0f, oledR * 0.6f);
        drawGlowText(g, valueText, textArea, valueFontSize);
    }

    // 9. Label text
    {
        g.setColour(PhantomColours::textDim);
        g.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 5.0f, juce::Font::bold));
        auto labelArea = juce::Rectangle<float>(cx - oledR, cy + oledR * 0.15f, oledR * 2.0f, oledR * 0.4f);
        g.drawText(slider.getName().toUpperCase(), labelArea, juce::Justification::centred, false);
    }
}

void PhantomLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                const juce::Colour&, bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    float corner = 9.0f;
    bool toggled = button.getToggleState();

    if (toggled || isDown)
    {
        // Raised neumorphic
        juce::DropShadow hl(PhantomColours::panelShadowLight, 6, { -3, -3 });
        hl.drawForRectangle(g, button.getLocalBounds());
        juce::DropShadow dk(PhantomColours::panelShadowDark, 8, { 3, 3 });
        dk.drawForRectangle(g, button.getLocalBounds());
        g.setColour(juce::Colour(0x08ffffff));
        g.fillRoundedRectangle(bounds, corner);
    }
    else
    {
        // Inset neumorphic
        g.setColour(juce::Colour(0x59000000));
        g.fillRoundedRectangle(bounds, corner);
        // Inset shadow simulation
        juce::ColourGradient inset(juce::Colour(0x30000000), bounds.getX(), bounds.getY(),
                                    juce::Colours::transparentBlack, bounds.getRight(), bounds.getBottom(), false);
        g.setGradientFill(inset);
        g.fillRoundedRectangle(bounds, corner);
    }

    if (isHighlighted && !isDown)
    {
        g.setColour(juce::Colour(0x04ffffff));
        g.fillRoundedRectangle(bounds, corner);
    }
}

void PhantomLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    auto bounds = button.getLocalBounds().toFloat();
    float alpha = button.getToggleState() ? 0.85f : 0.1f;
    g.setColour(PhantomColours::phosphorWhite.withAlpha(alpha));
    g.setFont(juce::Font(7.0f).withExtraKerningFactor(0.15f));
    g.drawText(button.getButtonText().toUpperCase(), bounds, juce::Justification::centred, false);

    if (button.getToggleState())
    {
        // Text glow
        g.setColour(PhantomColours::phosphorWhite.withAlpha(0.18f));
        g.drawText(button.getButtonText().toUpperCase(), bounds.translated(0, 0.5f), juce::Justification::centred, false);
    }
}

void PhantomLookAndFeel::drawGlowText(juce::Graphics& g, const juce::String& text,
                                        juce::Rectangle<float> area, float fontSize,
                                        juce::Justification justification)
{
    auto font = juce::Font(juce::Font::getDefaultMonospacedFontName(), fontSize, juce::Font::bold);
    g.setFont(font);

    // Pass 1: wide glow
    g.setColour(PhantomColours::phosphorWhite.withAlpha(0.3f));
    g.drawText(text, area.expanded(2.0f), justification, false);

    // Pass 2: medium glow
    g.setColour(PhantomColours::phosphorWhite.withAlpha(0.6f));
    g.drawText(text, area.expanded(0.5f), justification, false);

    // Pass 3: sharp text
    g.setColour(PhantomColours::phosphorWhite);
    g.drawText(text, area, justification, false);
}

void PhantomLookAndFeel::drawOledRidge(juce::Graphics& g, float cx, float cy, float radius)
{
    // Inner bright ring
    g.setColour(PhantomColours::ridgeBright);
    g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.5f);

    // Dark gap ring
    float gapR = radius + 1.5f;
    g.setColour(PhantomColours::ridgeDark);
    g.drawEllipse(cx - gapR, cy - gapR, gapR * 2.0f, gapR * 2.0f, 1.5f);

    // Outer subtle ring
    float outerR = gapR + 1.5f;
    g.setColour(PhantomColours::ridgeOuter);
    g.drawEllipse(cx - outerR, cy - outerR, outerR * 2.0f, outerR * 2.0f, 1.0f);
}
```

- [ ] **Step 3: Commit**

```bash
git add Source/UI/PhantomLookAndFeel.h Source/UI/PhantomLookAndFeel.cpp
git commit -m "feat(ui): add PhantomLookAndFeel with neumorphic knob and button rendering"
```

---

### Task 6: HeaderBar and FooterBar

**Files:**
- Create: `Source/UI/HeaderBar.h`
- Create: `Source/UI/HeaderBar.cpp`
- Create: `Source/UI/FooterBar.h`
- Create: `Source/UI/FooterBar.cpp`

- [ ] **Step 1: Create HeaderBar**

```cpp
// Source/UI/HeaderBar.h
#pragma once
#include <JuceHeader.h>

class PhantomProcessor;

class HeaderBar : public juce::Component
{
public:
    explicit HeaderBar(juce::AudioProcessorValueTreeState& apvts);
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::TextButton effectBtn { "Effect" }, instrBtn { "Instrument" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> modeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeaderBar)
};
```

```cpp
// Source/UI/HeaderBar.cpp
#include "HeaderBar.h"
#include "PhantomColours.h"
#include "../Parameters.h"

HeaderBar::HeaderBar(juce::AudioProcessorValueTreeState& apvts)
{
    effectBtn.setClickingTogglesState(true);
    instrBtn.setClickingTogglesState(true);

    effectBtn.setRadioGroupId(1);
    instrBtn.setRadioGroupId(1);
    effectBtn.setToggleState(true, juce::dontSendNotification);

    addAndMakeVisible(effectBtn);
    addAndMakeVisible(instrBtn);

    // Bind to MODE parameter via a lambda listener (ComboBox attachment needs adapter)
    // For now, buttons trigger parameter changes manually
    effectBtn.onClick = [&apvts]()
    {
        if (auto* param = apvts.getParameter(ParamID::MODE))
            param->setValueNotifyingHost(0.0f);
    };
    instrBtn.onClick = [&apvts]()
    {
        if (auto* param = apvts.getParameter(ParamID::MODE))
            param->setValueNotifyingHost(1.0f);
    };
}

void HeaderBar::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background gradient
    juce::ColourGradient bg(juce::Colour(0x05ffffff), 0, 0,
                             juce::Colour(0x59000000), 0, bounds.getHeight(), false);
    g.setGradientFill(bg);
    g.fillRect(bounds);

    // Stipple texture
    g.setColour(juce::Colour(0x05ffffff));
    for (float y = 0; y < bounds.getHeight(); y += 3.0f)
        for (float x = 0; x < bounds.getWidth(); x += 3.0f)
            g.fillEllipse(x, y, 1.0f, 1.0f);

    // Bottom glow seam
    juce::ColourGradient seam(juce::Colours::transparentWhite, bounds.getWidth() * 0.08f, bounds.getBottom(),
                               PhantomColours::seamGlow, bounds.getCentreX(), bounds.getBottom(), false);
    seam.addColour(0.5, PhantomColours::seamGlow);
    seam.addColour(1.0, juce::Colours::transparentWhite);
    g.setGradientFill(seam);
    g.fillRect(bounds.getX(), bounds.getBottom() - 1.0f, bounds.getWidth(), 1.0f);

    // PHANTOM text (etched metallic)
    {
        juce::Font phantomFont(22.0f, juce::Font::plain);
        g.setFont(phantomFont);
        // Etch shadow (dark above)
        g.setColour(PhantomColours::etchDark);
        g.drawText("PHANTOM", 54, 12, 200, 30, juce::Justification::centredLeft, false);
        // Etch catch (light below)
        g.setColour(PhantomColours::etchLight);
        g.drawText("PHANTOM", 54, 14, 200, 30, juce::Justification::centredLeft, false);
        // Metallic fill
        g.setColour(PhantomColours::metalC);
        g.drawText("PHANTOM", 54, 13, 200, 30, juce::Justification::centredLeft, false);
    }

    // KAIGEN text (etched metallic, right-aligned)
    {
        g.setFont(juce::Font(13.0f));
        g.setColour(PhantomColours::etchDark);
        g.drawText("KAIGEN", getWidth() - 120, 16, 100, 20, juce::Justification::centredRight, false);
        g.setColour(PhantomColours::etchLight);
        g.drawText("KAIGEN", getWidth() - 120, 18, 100, 20, juce::Justification::centredRight, false);
        g.setColour(PhantomColours::metalC);
        g.drawText("KAIGEN", getWidth() - 120, 17, 100, 20, juce::Justification::centredRight, false);
    }
}

void HeaderBar::resized()
{
    auto area = getLocalBounds().reduced(0, 8);
    // Position mode toggle buttons in the center-right area
    auto toggleArea = area.removeFromRight(220).removeFromRight(180);
    effectBtn.setBounds(toggleArea.removeFromLeft(85).reduced(2));
    instrBtn.setBounds(toggleArea.removeFromLeft(85).reduced(2));
}
```

- [ ] **Step 2: Create FooterBar**

```cpp
// Source/UI/FooterBar.h
#pragma once
#include <JuceHeader.h>

class FooterBar : public juce::Component
{
public:
    FooterBar();
    void paint(juce::Graphics& g) override;
    void resized() override;

    juce::TextButton bypassBtn { "Bypass" }, recipeBtn { "Recipe" },
                     ghostBtn { "Ghost" }, binauralBtn { "Binaural" },
                     deconflictBtn { "Deconflict" }, settingsBtn { "Settings" };
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FooterBar)
};
```

```cpp
// Source/UI/FooterBar.cpp
#include "FooterBar.h"
#include "PhantomColours.h"

FooterBar::FooterBar()
{
    for (auto* btn : { &bypassBtn, &recipeBtn, &ghostBtn, &binauralBtn, &deconflictBtn, &settingsBtn })
    {
        btn->setClickingTogglesState(true);
        addAndMakeVisible(btn);
    }
    recipeBtn.setToggleState(true, juce::dontSendNotification);
}

void FooterBar::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background
    g.setColour(juce::Colour(0x30000000));
    g.fillRect(bounds);

    // Stipple
    g.setColour(juce::Colour(0x03ffffff));
    for (float y = 0; y < bounds.getHeight(); y += 3.0f)
        for (float x = 0; x < bounds.getWidth(); x += 3.0f)
            g.fillEllipse(x, y, 1.0f, 1.0f);

    // Top glow seam
    juce::ColourGradient seam(juce::Colours::transparentWhite, bounds.getWidth() * 0.06f, bounds.getY(),
                               PhantomColours::seamGlow.withAlpha(0.12f), bounds.getCentreX(), bounds.getY(), false);
    seam.addColour(0.5, PhantomColours::seamGlow.withAlpha(0.12f));
    seam.addColour(1.0, juce::Colours::transparentWhite);
    g.setGradientFill(seam);
    g.fillRect(bounds.getX(), bounds.getY(), bounds.getWidth(), 1.0f);

    // Version label
    g.setColour(PhantomColours::textDim.withAlpha(0.07f));
    g.setFont(juce::Font(6.5f).withExtraKerningFactor(0.15f));
    g.drawText("KAIGEN PHANTOM \xc2\xb7 v0.1.0", bounds.reduced(16, 0), juce::Justification::centredRight, false);
}

void FooterBar::resized()
{
    auto area = getLocalBounds().reduced(12, 6);
    int btnW = 68, gap = 4;
    for (auto* btn : { &bypassBtn, &recipeBtn, &ghostBtn, &binauralBtn, &deconflictBtn, &settingsBtn })
    {
        btn->setBounds(area.removeFromLeft(btnW));
        area.removeFromLeft(gap);
    }
}
```

- [ ] **Step 3: Commit**

```bash
git add Source/UI/HeaderBar.h Source/UI/HeaderBar.cpp Source/UI/FooterBar.h Source/UI/FooterBar.cpp
git commit -m "feat(ui): add HeaderBar and FooterBar with etched text and stipple texture"
```

---

### Task 7: Wire Foundation into PluginEditor and CMakeLists

**Files:**
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Update CMakeLists.txt to include all new UI source files**

Add after the existing `target_sources` block — replace the entire block:

```cmake
target_sources(KaigenPhantom PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Engines/PitchTracker.cpp
    Source/Engines/HarmonicGenerator.cpp
    Source/Engines/BinauralStage.cpp
    Source/Engines/PerceptualOptimizer.cpp
    Source/Engines/CrossoverBlend.cpp
    Source/Engines/Deconfliction/PartitionStrategy.cpp
    Source/Engines/Deconfliction/SpectralLaneStrategy.cpp
    Source/Engines/Deconfliction/StaggerStrategy.cpp
    Source/Engines/Deconfliction/OddEvenStrategy.cpp
    Source/Engines/Deconfliction/ResidueStrategy.cpp
    Source/Engines/Deconfliction/BinauralStrategy.cpp
    Source/UI/PhantomLookAndFeel.cpp
    Source/UI/PhantomKnob.cpp
    Source/UI/NeumorphicPanel.cpp
    Source/UI/GlowSeam.cpp
    Source/UI/HeaderBar.cpp
    Source/UI/FooterBar.cpp
)
```

- [ ] **Step 2: Rewrite PluginEditor.h**

```cpp
// Source/PluginEditor.h
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/PhantomLookAndFeel.h"
#include "UI/HeaderBar.h"
#include "UI/FooterBar.h"
#include "UI/GlowSeam.h"

class PhantomEditor : public juce::AudioProcessorEditor
{
public:
    explicit PhantomEditor(PhantomProcessor&);
    ~PhantomEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    PhantomProcessor& processor;
    PhantomLookAndFeel phantomLnf;

    HeaderBar  headerBar;
    FooterBar  footerBar;
    GlowSeam   headerSeam { GlowSeam::Orientation::Horizontal };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomEditor)
};
```

- [ ] **Step 3: Rewrite PluginEditor.cpp**

```cpp
// Source/PluginEditor.cpp
#include "PluginEditor.h"
#include "UI/PhantomColours.h"

PhantomEditor::PhantomEditor(PhantomProcessor& p)
    : AudioProcessorEditor(&p), processor(p),
      headerBar(p.apvts)
{
    setLookAndFeel(&phantomLnf);
    setSize(920, 620);

    addAndMakeVisible(headerBar);
    addAndMakeVisible(footerBar);
    addAndMakeVisible(headerSeam);
}

PhantomEditor::~PhantomEditor()
{
    setLookAndFeel(nullptr);
}

void PhantomEditor::paint(juce::Graphics& g)
{
    g.fillAll(PhantomColours::background);
}

void PhantomEditor::resized()
{
    auto area = getLocalBounds();

    headerBar.setBounds(area.removeFromTop(50));
    headerSeam.setBounds(area.removeFromTop(3));
    footerBar.setBounds(area.removeFromBottom(38));

    // Remaining area is for the body panel — will be filled in Phase 2+
}
```

- [ ] **Step 4: Build and verify**

Run: `cmake --build build --target KaigenPhantom 2>&1 | tail -5`

Expected: Build succeeds. Plugin loads in DAW showing dark background with header bar (PHANTOM/KAIGEN text, mode toggle buttons) and footer bar (nav buttons, version label), separated by a glow seam.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt Source/PluginEditor.h Source/PluginEditor.cpp
git commit -m "feat(ui): wire Phase 1 foundation — header, footer, LookAndFeel, 920x620 layout"
```

---

## Phase 2 — Knob Panels

### Task 8: HarmonicEnginePanel

**Files:**
- Create: `Source/UI/HarmonicEnginePanel.h`
- Create: `Source/UI/HarmonicEnginePanel.cpp`

- [ ] **Step 1: Create HarmonicEnginePanel**

```cpp
// Source/UI/HarmonicEnginePanel.h
#pragma once
#include "NeumorphicPanel.h"
#include "PhantomKnob.h"

class HarmonicEnginePanel : public NeumorphicPanel
{
public:
    explicit HarmonicEnginePanel(juce::AudioProcessorValueTreeState& apvts);
    void resized() override;
    void paintContent(juce::Graphics& g) override;

private:
    PhantomKnob saturationKnob { "Saturation", PhantomKnob::Size::Large, 0.0f };
    PhantomKnob strengthKnob   { "Strength",   PhantomKnob::Size::Medium, 80.0f };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> saturationAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> strengthAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HarmonicEnginePanel)
};
```

```cpp
// Source/UI/HarmonicEnginePanel.cpp
#include "HarmonicEnginePanel.h"
#include "PhantomColours.h"
#include "../Parameters.h"

HarmonicEnginePanel::HarmonicEnginePanel(juce::AudioProcessorValueTreeState& apvts)
{
    addAndMakeVisible(saturationKnob);
    addAndMakeVisible(strengthKnob);

    saturationAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParamID::HARMONIC_SATURATION, saturationKnob);
    strengthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParamID::PHANTOM_STRENGTH, strengthKnob);
}

void HarmonicEnginePanel::paintContent(juce::Graphics& g)
{
    g.setColour(PhantomColours::textDim);
    g.setFont(juce::Font(7.0f, juce::Font::bold).withExtraKerningFactor(0.2f));
    g.drawText("HARMONIC ENGINE", getLocalBounds().reduced(6, 4).removeFromTop(14),
               juce::Justification::topLeft, false);
}

void HarmonicEnginePanel::resized()
{
    auto area = getLocalBounds().reduced(6, 4);
    area.removeFromTop(16); // label space

    auto row = area;
    int lgD = saturationKnob.getDiameter();
    int mdD = strengthKnob.getDiameter();

    // Center the knobs horizontally
    int totalW = lgD + 10 + mdD;
    int startX = (row.getWidth() - totalW) / 2;

    saturationKnob.setBounds(row.getX() + startX, row.getY() + (row.getHeight() - lgD) / 2, lgD, lgD);
    strengthKnob.setBounds(row.getX() + startX + lgD + 10, row.getY() + (row.getHeight() - mdD) / 2, mdD, mdD);
}
```

- [ ] **Step 2: Commit**

```bash
git add Source/UI/HarmonicEnginePanel.h Source/UI/HarmonicEnginePanel.cpp
git commit -m "feat(ui): add HarmonicEnginePanel with Saturation and Strength knobs"
```

---

### Task 9: GhostPanel

**Files:**
- Create: `Source/UI/GhostPanel.h`
- Create: `Source/UI/GhostPanel.cpp`

- [ ] **Step 1: Create GhostPanel**

```cpp
// Source/UI/GhostPanel.h
#pragma once
#include "NeumorphicPanel.h"
#include "PhantomKnob.h"

class GhostPanel : public NeumorphicPanel
{
public:
    explicit GhostPanel(juce::AudioProcessorValueTreeState& apvts);
    void resized() override;
    void paintContent(juce::Graphics& g) override;

private:
    PhantomKnob amountKnob    { "Amount",    PhantomKnob::Size::Large, 100.0f };
    PhantomKnob thresholdKnob { "Threshold", PhantomKnob::Size::Medium, 80.0f };

    juce::TextButton replaceBtn { "Rpl" }, addBtn { "Add" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> thresholdAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GhostPanel)
};
```

```cpp
// Source/UI/GhostPanel.cpp
#include "GhostPanel.h"
#include "PhantomColours.h"
#include "../Parameters.h"

GhostPanel::GhostPanel(juce::AudioProcessorValueTreeState& apvts)
{
    addAndMakeVisible(amountKnob);
    addAndMakeVisible(thresholdKnob);
    addAndMakeVisible(replaceBtn);
    addAndMakeVisible(addBtn);

    amountAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParamID::GHOST, amountKnob);
    thresholdAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParamID::PHANTOM_THRESHOLD, thresholdKnob);

    replaceBtn.setClickingTogglesState(true);
    addBtn.setClickingTogglesState(true);
    replaceBtn.setRadioGroupId(2);
    addBtn.setRadioGroupId(2);
    replaceBtn.setToggleState(true, juce::dontSendNotification);

    replaceBtn.onClick = [&apvts]() {
        if (auto* p = apvts.getParameter(ParamID::GHOST_MODE))
            p->setValueNotifyingHost(0.0f);
    };
    addBtn.onClick = [&apvts]() {
        if (auto* p = apvts.getParameter(ParamID::GHOST_MODE))
            p->setValueNotifyingHost(1.0f);
    };
}

void GhostPanel::paintContent(juce::Graphics& g)
{
    g.setColour(PhantomColours::textDim);
    g.setFont(juce::Font(7.0f, juce::Font::bold).withExtraKerningFactor(0.2f));
    g.drawText("GHOST", getLocalBounds().reduced(6, 4).removeFromTop(14),
               juce::Justification::topLeft, false);
}

void GhostPanel::resized()
{
    auto area = getLocalBounds().reduced(6, 4);
    auto labelRow = area.removeFromTop(16);

    // Mode toggle buttons — inline with label
    int toggleW = 28, toggleH = 14;
    replaceBtn.setBounds(labelRow.getRight() - toggleW * 2 - 4, labelRow.getY(), toggleW, toggleH);
    addBtn.setBounds(labelRow.getRight() - toggleW, labelRow.getY(), toggleW, toggleH);

    int lgD = amountKnob.getDiameter();
    int mdD = thresholdKnob.getDiameter();
    int totalW = lgD + 10 + mdD;
    int startX = (area.getWidth() - totalW) / 2;

    amountKnob.setBounds(area.getX() + startX, area.getY() + (area.getHeight() - lgD) / 2, lgD, lgD);
    thresholdKnob.setBounds(area.getX() + startX + lgD + 10, area.getY() + (area.getHeight() - mdD) / 2, mdD, mdD);
}
```

- [ ] **Step 2: Commit**

```bash
git add Source/UI/GhostPanel.h Source/UI/GhostPanel.cpp
git commit -m "feat(ui): add GhostPanel with Amount, Threshold, and Replace/Add toggle"
```

---

### Task 10: OutputPanel

**Files:**
- Create: `Source/UI/OutputPanel.h`
- Create: `Source/UI/OutputPanel.cpp`

- [ ] **Step 1: Create OutputPanel**

```cpp
// Source/UI/OutputPanel.h
#pragma once
#include "NeumorphicPanel.h"
#include "PhantomKnob.h"

class OutputPanel : public NeumorphicPanel
{
public:
    explicit OutputPanel(juce::AudioProcessorValueTreeState& apvts);
    void resized() override;
    void paintContent(juce::Graphics& g) override;

private:
    PhantomKnob gainKnob { "Gain", PhantomKnob::Size::Large, 0.0f };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutputPanel)
};
```

```cpp
// Source/UI/OutputPanel.cpp
#include "OutputPanel.h"
#include "PhantomColours.h"
#include "../Parameters.h"

OutputPanel::OutputPanel(juce::AudioProcessorValueTreeState& apvts)
{
    addAndMakeVisible(gainKnob);
    gainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParamID::OUTPUT_GAIN, gainKnob);
}

void OutputPanel::paintContent(juce::Graphics& g)
{
    g.setColour(PhantomColours::textDim);
    g.setFont(juce::Font(7.0f, juce::Font::bold).withExtraKerningFactor(0.2f));
    g.drawText("OUTPUT", getLocalBounds().reduced(6, 4).removeFromTop(14),
               juce::Justification::centred, false);
}

void OutputPanel::resized()
{
    auto area = getLocalBounds().reduced(6, 4);
    area.removeFromTop(16);
    int d = gainKnob.getDiameter();
    gainKnob.setBounds((area.getWidth() - d) / 2 + area.getX(),
                        area.getY() + (area.getHeight() - d) / 2, d, d);
}
```

- [ ] **Step 2: Commit**

```bash
git add Source/UI/OutputPanel.h Source/UI/OutputPanel.cpp
git commit -m "feat(ui): add OutputPanel with Gain knob"
```

---

### Task 11: Wire Phase 2 panels into PluginEditor

**Files:**
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add new .cpp files to CMakeLists.txt**

Append to the `target_sources` block:

```cmake
    Source/UI/HarmonicEnginePanel.cpp
    Source/UI/GhostPanel.cpp
    Source/UI/OutputPanel.cpp
```

- [ ] **Step 2: Update PluginEditor.h — add panel includes and members**

Add includes:

```cpp
#include "UI/HarmonicEnginePanel.h"
#include "UI/GhostPanel.h"
#include "UI/OutputPanel.h"
```

Add members after `headerSeam`:

```cpp
    GlowSeam   bodyVSeam { GlowSeam::Orientation::Vertical };
    GlowSeam   row1Seam  { GlowSeam::Orientation::Horizontal };

    HarmonicEnginePanel harmonicPanel;
    GhostPanel          ghostPanel;
    OutputPanel         outputPanel;
```

- [ ] **Step 3: Update PluginEditor.cpp constructor — create panels and add to editor**

Update constructor initialization list and body:

```cpp
PhantomEditor::PhantomEditor(PhantomProcessor& p)
    : AudioProcessorEditor(&p), processor(p),
      headerBar(p.apvts),
      harmonicPanel(p.apvts),
      ghostPanel(p.apvts),
      outputPanel(p.apvts)
{
    setLookAndFeel(&phantomLnf);
    setSize(920, 620);

    addAndMakeVisible(headerBar);
    addAndMakeVisible(footerBar);
    addAndMakeVisible(headerSeam);
    addAndMakeVisible(bodyVSeam);
    addAndMakeVisible(row1Seam);
    addAndMakeVisible(harmonicPanel);
    addAndMakeVisible(ghostPanel);
    addAndMakeVisible(outputPanel);
}
```

- [ ] **Step 4: Update PluginEditor::resized() — layout body panels**

```cpp
void PhantomEditor::resized()
{
    auto area = getLocalBounds();

    headerBar.setBounds(area.removeFromTop(50));
    headerSeam.setBounds(area.removeFromTop(3));
    footerBar.setBounds(area.removeFromBottom(38));

    // Body: left recipe wheel area (316px) | seam | right panel
    auto body = area;
    auto leftArea = body.removeFromLeft(316);  // Reserved for RecipeWheelPanel (Phase 4)
    bodyVSeam.setBounds(body.removeFromLeft(3));
    auto rightArea = body;

    // Right panel: Row 1 (knob panels) | seam | rest
    auto row1 = rightArea.removeFromTop(130);
    row1Seam.setBounds(rightArea.removeFromTop(3));

    // Row 1: HarmonicEngine | Ghost | Output
    int heW = (int)(row1.getWidth() * 0.42f);
    int ghW = (int)(row1.getWidth() * 0.35f);
    int ouW = row1.getWidth() - heW - ghW;

    harmonicPanel.setBounds(row1.removeFromLeft(heW));
    ghostPanel.setBounds(row1.removeFromLeft(ghW));
    outputPanel.setBounds(row1);
}
```

- [ ] **Step 5: Build and verify**

Run: `cmake --build build --target KaigenPhantom 2>&1 | tail -5`

Expected: Build succeeds. Plugin shows header, footer, and three neumorphic panels in the top row of the right panel with functional knobs (Saturation, Strength, Ghost Amount, Threshold, Gain).

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt Source/PluginEditor.h Source/PluginEditor.cpp
git commit -m "feat(ui): wire Phase 2 — top row knob panels with APVTS bindings"
```

---

## Phase 3 — Mode-Switched Row

### Task 12: PitchTrackerPanel, SidechainPanel, StereoPanel

**Files:**
- Create: `Source/UI/PitchTrackerPanel.h`
- Create: `Source/UI/PitchTrackerPanel.cpp`
- Create: `Source/UI/SidechainPanel.h`
- Create: `Source/UI/SidechainPanel.cpp`
- Create: `Source/UI/StereoPanel.h`
- Create: `Source/UI/StereoPanel.cpp`

- [ ] **Step 1: Create PitchTrackerPanel**

```cpp
// Source/UI/PitchTrackerPanel.h
#pragma once
#include "NeumorphicPanel.h"
#include "PhantomKnob.h"

class PitchTrackerPanel : public NeumorphicPanel
{
public:
    explicit PitchTrackerPanel(juce::AudioProcessorValueTreeState& apvts);
    void resized() override;
    void paintContent(juce::Graphics& g) override;
private:
    PhantomKnob sensitivityKnob { "Sensitivity", PhantomKnob::Size::Medium, 70.0f };
    PhantomKnob glideKnob       { "Glide",       PhantomKnob::Size::Medium, 20.0f };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sensAttach, glideAttach;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchTrackerPanel)
};
```

```cpp
// Source/UI/PitchTrackerPanel.cpp
#include "PitchTrackerPanel.h"
#include "PhantomColours.h"
#include "../Parameters.h"

PitchTrackerPanel::PitchTrackerPanel(juce::AudioProcessorValueTreeState& apvts)
{
    addAndMakeVisible(sensitivityKnob);
    addAndMakeVisible(glideKnob);
    sensAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, ParamID::TRACKING_SENSITIVITY, sensitivityKnob);
    glideAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, ParamID::TRACKING_GLIDE, glideKnob);
}

void PitchTrackerPanel::paintContent(juce::Graphics& g)
{
    g.setColour(PhantomColours::textDim);
    g.setFont(juce::Font(7.0f, juce::Font::bold).withExtraKerningFactor(0.2f));
    g.drawText("PITCH TRACKER", getLocalBounds().reduced(6, 4).removeFromTop(14), juce::Justification::topLeft, false);
}

void PitchTrackerPanel::resized()
{
    auto area = getLocalBounds().reduced(6, 4);
    area.removeFromTop(16);
    int d = sensitivityKnob.getDiameter();
    int totalW = d * 2 + 10;
    int startX = (area.getWidth() - totalW) / 2 + area.getX();
    int cy = area.getY() + (area.getHeight() - d) / 2;
    sensitivityKnob.setBounds(startX, cy, d, d);
    glideKnob.setBounds(startX + d + 10, cy, d, d);
}
```

- [ ] **Step 2: Create SidechainPanel**

```cpp
// Source/UI/SidechainPanel.h
#pragma once
#include "NeumorphicPanel.h"
#include "PhantomKnob.h"

class SidechainPanel : public NeumorphicPanel
{
public:
    explicit SidechainPanel(juce::AudioProcessorValueTreeState& apvts);
    void resized() override;
    void paintContent(juce::Graphics& g) override;
private:
    PhantomKnob amountKnob  { "Amount",  PhantomKnob::Size::Medium, 0.0f };
    PhantomKnob attackKnob  { "Attack",  PhantomKnob::Size::Medium, 5.0f };
    PhantomKnob releaseKnob { "Release", PhantomKnob::Size::Medium, 80.0f };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amtAttach, atkAttach, relAttach;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SidechainPanel)
};
```

```cpp
// Source/UI/SidechainPanel.cpp
#include "SidechainPanel.h"
#include "PhantomColours.h"
#include "../Parameters.h"

SidechainPanel::SidechainPanel(juce::AudioProcessorValueTreeState& apvts)
{
    addAndMakeVisible(amountKnob);
    addAndMakeVisible(attackKnob);
    addAndMakeVisible(releaseKnob);
    amtAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, ParamID::SIDECHAIN_DUCK_AMOUNT, amountKnob);
    atkAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, ParamID::SIDECHAIN_DUCK_ATTACK, attackKnob);
    relAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, ParamID::SIDECHAIN_DUCK_RELEASE, releaseKnob);
}

void SidechainPanel::paintContent(juce::Graphics& g)
{
    g.setColour(PhantomColours::textDim);
    g.setFont(juce::Font(7.0f, juce::Font::bold).withExtraKerningFactor(0.2f));
    g.drawText("SIDECHAIN DUCK", getLocalBounds().reduced(6, 4).removeFromTop(14), juce::Justification::topLeft, false);
}

void SidechainPanel::resized()
{
    auto area = getLocalBounds().reduced(6, 4);
    area.removeFromTop(16);
    int d = amountKnob.getDiameter();
    int totalW = d * 3 + 16;
    int startX = (area.getWidth() - totalW) / 2 + area.getX();
    int cy = area.getY() + (area.getHeight() - d) / 2;
    amountKnob.setBounds(startX, cy, d, d);
    attackKnob.setBounds(startX + d + 8, cy, d, d);
    releaseKnob.setBounds(startX + d * 2 + 16, cy, d, d);
}
```

- [ ] **Step 3: Create StereoPanel**

```cpp
// Source/UI/StereoPanel.h
#pragma once
#include "NeumorphicPanel.h"
#include "PhantomKnob.h"

class StereoPanel : public NeumorphicPanel
{
public:
    explicit StereoPanel(juce::AudioProcessorValueTreeState& apvts);
    void resized() override;
    void paintContent(juce::Graphics& g) override;
private:
    PhantomKnob widthKnob { "Width", PhantomKnob::Size::Large, 100.0f };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> widthAttach;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoPanel)
};
```

```cpp
// Source/UI/StereoPanel.cpp
#include "StereoPanel.h"
#include "PhantomColours.h"
#include "../Parameters.h"

StereoPanel::StereoPanel(juce::AudioProcessorValueTreeState& apvts)
{
    addAndMakeVisible(widthKnob);
    widthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, ParamID::STEREO_WIDTH, widthKnob);
}

void StereoPanel::paintContent(juce::Graphics& g)
{
    g.setColour(PhantomColours::textDim);
    g.setFont(juce::Font(7.0f, juce::Font::bold).withExtraKerningFactor(0.2f));
    g.drawText("STEREO", getLocalBounds().reduced(6, 4).removeFromTop(14), juce::Justification::centred, false);
}

void StereoPanel::resized()
{
    auto area = getLocalBounds().reduced(6, 4);
    area.removeFromTop(16);
    int d = widthKnob.getDiameter();
    widthKnob.setBounds((area.getWidth() - d) / 2 + area.getX(), area.getY() + (area.getHeight() - d) / 2, d, d);
}
```

- [ ] **Step 4: Commit**

```bash
git add Source/UI/PitchTrackerPanel.h Source/UI/PitchTrackerPanel.cpp Source/UI/SidechainPanel.h Source/UI/SidechainPanel.cpp Source/UI/StereoPanel.h Source/UI/StereoPanel.cpp
git commit -m "feat(ui): add PitchTracker, Sidechain, and Stereo panels"
```

---

### Task 13: DeconflictionPanel

**Files:**
- Create: `Source/UI/DeconflictionPanel.h`
- Create: `Source/UI/DeconflictionPanel.cpp`

- [ ] **Step 1: Create DeconflictionPanel**

```cpp
// Source/UI/DeconflictionPanel.h
#pragma once
#include "NeumorphicPanel.h"
#include "PhantomKnob.h"

class DeconflictionPanel : public NeumorphicPanel
{
public:
    explicit DeconflictionPanel(juce::AudioProcessorValueTreeState& apvts);
    void resized() override;
    void paintContent(juce::Graphics& g) override;
private:
    juce::ComboBox modeSelector;
    PhantomKnob voicesKnob  { "Voices",  PhantomKnob::Size::Medium, 4.0f };
    PhantomKnob staggerKnob { "Stagger", PhantomKnob::Size::Medium, 8.0f };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> voicesAttach, staggerAttach;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeconflictionPanel)
};
```

```cpp
// Source/UI/DeconflictionPanel.cpp
#include "DeconflictionPanel.h"
#include "PhantomColours.h"
#include "../Parameters.h"

DeconflictionPanel::DeconflictionPanel(juce::AudioProcessorValueTreeState& apvts)
{
    modeSelector.addItemList({ "Partition", "Lane", "Stagger", "Odd-Even", "Residue", "Binaural" }, 1);
    modeSelector.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(modeSelector);
    addAndMakeVisible(voicesKnob);
    addAndMakeVisible(staggerKnob);

    modeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, ParamID::DECONFLICTION_MODE, modeSelector);
    voicesAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, ParamID::MAX_VOICES, voicesKnob);
    staggerAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, ParamID::STAGGER_DELAY, staggerKnob);
}

void DeconflictionPanel::paintContent(juce::Graphics& g)
{
    g.setColour(PhantomColours::textDim);
    g.setFont(juce::Font(7.0f, juce::Font::bold).withExtraKerningFactor(0.2f));
    g.drawText("DECONFLICTION", getLocalBounds().reduced(6, 4).removeFromTop(14), juce::Justification::topLeft, false);
}

void DeconflictionPanel::resized()
{
    auto area = getLocalBounds().reduced(6, 4);
    area.removeFromTop(16);

    // Mode selector at top
    modeSelector.setBounds(area.removeFromTop(22).reduced(4, 0));
    area.removeFromTop(4);

    // Knobs side by side
    int d = voicesKnob.getDiameter();
    int totalW = d * 2 + 10;
    int startX = (area.getWidth() - totalW) / 2 + area.getX();
    int cy = area.getY() + (area.getHeight() - d) / 2;
    voicesKnob.setBounds(startX, cy, d, d);
    staggerKnob.setBounds(startX + d + 10, cy, d, d);
}
```

- [ ] **Step 2: Commit**

```bash
git add Source/UI/DeconflictionPanel.h Source/UI/DeconflictionPanel.cpp
git commit -m "feat(ui): add DeconflictionPanel with mode selector, voices, stagger knobs"
```

---

### Task 14: Wire Phase 3 — mode-switched row in PluginEditor

**Files:**
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add new .cpp files to CMakeLists.txt**

Append:
```cmake
    Source/UI/PitchTrackerPanel.cpp
    Source/UI/SidechainPanel.cpp
    Source/UI/StereoPanel.cpp
    Source/UI/DeconflictionPanel.cpp
```

- [ ] **Step 2: Update PluginEditor.h**

Add includes:
```cpp
#include "UI/PitchTrackerPanel.h"
#include "UI/SidechainPanel.h"
#include "UI/StereoPanel.h"
#include "UI/DeconflictionPanel.h"
```

Add to class, make editor a parameter listener:
```cpp
class PhantomEditor : public juce::AudioProcessorEditor,
                      private juce::AudioProcessorValueTreeState::Listener
```

Add members:
```cpp
    GlowSeam row2Seam { GlowSeam::Orientation::Horizontal };

    PitchTrackerPanel  pitchTrackerPanel;
    SidechainPanel     sidechainPanel;
    StereoPanel        stereoPanel;
    DeconflictionPanel deconflictionPanel;
```

Add override:
```cpp
    void parameterChanged(const juce::String& parameterID, float newValue) override;
```

- [ ] **Step 3: Update PluginEditor.cpp**

Constructor — add to init list and body:
```cpp
    pitchTrackerPanel(p.apvts),
    sidechainPanel(p.apvts),
    stereoPanel(p.apvts),
    deconflictionPanel(p.apvts)
```

In constructor body:
```cpp
    addAndMakeVisible(row2Seam);
    addAndMakeVisible(pitchTrackerPanel);
    addAndMakeVisible(sidechainPanel);
    addAndMakeVisible(stereoPanel);
    addAndMakeVisible(deconflictionPanel);

    // Default: Effect mode — show pitch tracker, hide deconfliction
    deconflictionPanel.setVisible(false);

    processor.apvts.addParameterListener(ParamID::MODE, this);
```

Destructor — unregister:
```cpp
PhantomEditor::~PhantomEditor()
{
    processor.apvts.removeParameterListener(ParamID::MODE, this);
    setLookAndFeel(nullptr);
}
```

Mode switch callback:
```cpp
void PhantomEditor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == ParamID::MODE)
    {
        bool isEffect = (newValue < 0.5f);
        juce::MessageManager::callAsync([this, isEffect]()
        {
            pitchTrackerPanel.setVisible(isEffect);
            deconflictionPanel.setVisible(!isEffect);
            resized();
        });
    }
}
```

Update `resized()` — add Row 2 after row1Seam:
```cpp
    // Row 2 (mode-switched)
    auto row2 = rightArea.removeFromTop(130);
    row2Seam.setBounds(rightArea.removeFromTop(3));

    // First panel: PitchTracker (Effect) or Deconfliction (Instrument)
    auto row2FirstPanel = row2.removeFromLeft((int)(row2.getWidth() * 0.30f));
    pitchTrackerPanel.setBounds(row2FirstPanel);
    deconflictionPanel.setBounds(row2FirstPanel);

    // Sidechain
    auto row2Sidechain = row2.removeFromLeft((int)(row2.getWidth() * 0.58f));
    sidechainPanel.setBounds(row2Sidechain);

    // Stereo
    stereoPanel.setBounds(row2);
```

- [ ] **Step 4: Build and verify**

Run: `cmake --build build --target KaigenPhantom 2>&1 | tail -5`

Expected: Full right panel with two rows of knobs. Clicking Effect/Instrument in header swaps PitchTracker and Deconfliction panels.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt Source/PluginEditor.h Source/PluginEditor.cpp
git commit -m "feat(ui): wire Phase 3 — mode-switched row with all control panels"
```

---

## Phase 4 — Recipe Wheel

### Task 15: RecipeWheel (custom painted)

**Files:**
- Create: `Source/UI/RecipeWheel.h`
- Create: `Source/UI/RecipeWheel.cpp`

- [ ] **Step 1: Create RecipeWheel header**

```cpp
// Source/UI/RecipeWheel.h
#pragma once
#include <JuceHeader.h>

class RecipeWheel : public juce::Component, private juce::Timer
{
public:
    RecipeWheel();

    void paint(juce::Graphics& g) override;
    void resized() override {}

    // Called from editor Timer to update detected pitch display
    void updatePitch(float pitchHz);

    // Called to update harmonic amplitudes (from APVTS values)
    void setHarmonicAmplitudes(const std::array<float, 7>& amps);

    // Set the current preset name displayed in center
    void setPresetName(const juce::String& name);

private:
    void timerCallback() override;

    // Animation state
    float ringRotations[6] = {};
    float scanAngle = 0.0f;
    float shimmerPhase = 0.0f;

    // Data
    std::array<float, 7> harmonicAmps = { 0.8f, 0.7f, 0.5f, 0.35f, 0.2f, 0.12f, 0.07f };
    float currentPitchHz = -1.0f;
    juce::String presetName = "Warm";

    // Paint helpers
    void paintNeumorphicMount(juce::Graphics& g, float cx, float cy, float radius);
    void paintHolographicRings(juce::Graphics& g, float cx, float cy);
    void paintTankPathways(juce::Graphics& g, float cx, float cy, float innerR, float outerR);
    void paintCenterOled(juce::Graphics& g, float cx, float cy, float radius);
    void paintHarmonicLabels(juce::Graphics& g, float cx, float cy, float radius);

    static juce::String pitchToNoteName(float hz);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecipeWheel)
};
```

- [ ] **Step 2: Create RecipeWheel implementation**

```cpp
// Source/UI/RecipeWheel.cpp
#include "RecipeWheel.h"
#include "PhantomColours.h"
#include "PhantomLookAndFeel.h"
#include <cmath>

static constexpr float kTwoPi = juce::MathConstants<float>::twoPi;
static constexpr int kNumHarmonics = 7;

RecipeWheel::RecipeWheel()
{
    startTimerHz(15); // 15fps for animation
}

void RecipeWheel::updatePitch(float pitchHz)
{
    if (std::abs(pitchHz - currentPitchHz) > 0.1f)
    {
        currentPitchHz = pitchHz;
        repaint();
    }
}

void RecipeWheel::setHarmonicAmplitudes(const std::array<float, 7>& amps)
{
    harmonicAmps = amps;
    repaint();
}

void RecipeWheel::setPresetName(const juce::String& name)
{
    presetName = name;
    repaint();
}

void RecipeWheel::timerCallback()
{
    // Advance ring rotations at different speeds (v7 speeds)
    float speeds[] = { 0.015f, -0.02f, 0.01f, -0.025f, 0.012f, -0.008f };
    for (int i = 0; i < 6; ++i)
        ringRotations[i] += speeds[i];

    scanAngle += 0.18f;
    shimmerPhase += 0.05f;

    repaint();
}

void RecipeWheel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    float cx = bounds.getCentreX();
    float cy = bounds.getCentreY();
    float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;

    paintNeumorphicMount(g, cx, cy, radius);
    paintHolographicRings(g, cx, cy);
    paintTankPathways(g, cx, cy, 50.0f, radius - 8.0f);
    paintCenterOled(g, cx, cy, 48.0f);
    paintHarmonicLabels(g, cx, cy, radius - 4.0f);
}

void RecipeWheel::paintNeumorphicMount(juce::Graphics& g, float cx, float cy, float radius)
{
    float d = radius * 2.0f;

    // Outer neumorphic shadows
    juce::DropShadow hlShadow(PhantomColours::panelShadowLight, 16, { -6, -6 });
    juce::Path circlePath;
    circlePath.addEllipse(cx - radius, cy - radius, d, d);
    hlShadow.drawForPath(g, circlePath);

    juce::DropShadow dkShadow(PhantomColours::panelShadowDark, 20, { 6, 6 });
    dkShadow.drawForPath(g, circlePath);

    // Directional gradient fill
    juce::ColourGradient volcano(
        juce::Colour(0x14ffffff), cx - radius * 0.35f, cy - radius * 0.4f,
        juce::Colour(0x80000000), cx + radius * 0.5f, cy + radius * 0.5f, true);
    volcano.addColour(0.25, juce::Colour(0x08ffffff));
    volcano.addColour(0.6, juce::Colour(0x66060610));
    g.setGradientFill(volcano);
    g.fillEllipse(cx - radius, cy - radius, d, d);

    // Border
    g.setColour(juce::Colour(0x0dffffff));
    g.drawEllipse(cx - radius, cy - radius, d, d, 2.0f);
}

void RecipeWheel::paintHolographicRings(juce::Graphics& g, float cx, float cy)
{
    // v7-style ethereal rings — thin, low opacity
    struct RingDef { float radius; float width; float opacity; };
    RingDef rings[] = {
        { 120.0f, 0.8f, 0.10f }, { 100.0f, 0.6f, 0.07f },
        { 78.0f, 0.5f, 0.05f },  { 52.0f, 0.6f, 0.08f },
        { 28.0f, 0.8f, 0.13f },  { 126.0f, 0.3f, 0.035f }
    };

    for (int i = 0; i < 6; ++i)
    {
        auto& r = rings[i];
        g.setColour(PhantomColours::phosphorWhite.withAlpha(r.opacity));

        juce::Path ring;
        float startAngle = ringRotations[i];
        ring.addArc(cx - r.radius, cy - r.radius, r.radius * 2.0f, r.radius * 2.0f,
                    startAngle, startAngle + kTwoPi, true);
        g.strokePath(ring, juce::PathStrokeType(r.width));
    }

    // Scan line
    g.setColour(PhantomColours::phosphorWhite.withAlpha(0.05f));
    float scanR = 122.0f;
    g.drawLine(cx, cy, cx + scanR * std::cos(scanAngle), cy + scanR * std::sin(scanAngle), 1.0f);
}

void RecipeWheel::paintTankPathways(juce::Graphics& g, float cx, float cy, float innerR, float outerR)
{
    for (int i = 0; i < kNumHarmonics; ++i)
    {
        float angle = i * (kTwoPi / kNumHarmonics) - juce::MathConstants<float>::halfPi;
        float cosA = std::cos(angle);
        float sinA = std::sin(angle);

        // Dim track (full path)
        g.setColour(PhantomColours::trackDim);
        g.drawLine(cx + innerR * cosA, cy + innerR * sinA,
                   cx + outerR * cosA, cy + outerR * sinA, 5.0f);

        // Bright fill (proportional to amplitude)
        float amp = harmonicAmps[(size_t)i];
        if (amp > 0.01f)
        {
            float fillStart = innerR;
            float fillEnd = innerR + (outerR - innerR) * amp;

            // Glow halo
            g.setColour(PhantomColours::phosphorWhite.withAlpha(amp * 0.2f));
            g.drawLine(cx + fillStart * cosA, cy + fillStart * sinA,
                       cx + fillEnd * cosA, cy + fillEnd * sinA, 6.0f);

            // Sharp fill
            g.setColour(PhantomColours::phosphorWhite.withAlpha(0.15f + amp * 0.4f));
            g.drawLine(cx + fillStart * cosA, cy + fillStart * sinA,
                       cx + fillEnd * cosA, cy + fillEnd * sinA, 3.5f);

            // Cap dot
            g.setColour(PhantomColours::phosphorWhite.withAlpha(amp * 0.65f));
            g.fillEllipse(cx + fillEnd * cosA - 3.0f, cy + fillEnd * sinA - 3.0f, 6.0f, 6.0f);

            // Node glow halo at endpoint
            g.setColour(PhantomColours::phosphorWhite.withAlpha(amp * 0.12f));
            float glowR = 6.0f + amp * 12.0f;
            g.fillEllipse(cx + outerR * cosA - glowR, cy + outerR * sinA - glowR, glowR * 2.0f, glowR * 2.0f);
        }
    }
}

void RecipeWheel::paintCenterOled(juce::Graphics& g, float cx, float cy, float radius)
{
    // Black OLED circle
    g.setColour(PhantomColours::oledBlack);
    g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    // Lip/ridge
    PhantomLookAndFeel::drawOledRidge(g, cx, cy, radius);

    // "FUND" label
    g.setColour(PhantomColours::textDim.withAlpha(0.3f));
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 6.0f, juce::Font::plain));
    g.drawText("FUND", juce::Rectangle<float>(cx - 40, cy - 22, 80, 12), juce::Justification::centred, false);

    // Frequency display
    juce::String freqText;
    if (currentPitchHz > 0.0f)
        freqText = pitchToNoteName(currentPitchHz) + " \xc2\xb7 " + juce::String((int)currentPitchHz) + "Hz";
    else
        freqText = "---";

    PhantomLookAndFeel::drawGlowText(g, freqText,
        juce::Rectangle<float>(cx - 44, cy - 10, 88, 18), 14.0f);

    // Preset name
    g.setColour(PhantomColours::phosphorWhite.withAlpha(0.7f));
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 7.0f, juce::Font::plain));
    g.drawText(presetName, juce::Rectangle<float>(cx - 40, cy + 10, 80, 12), juce::Justification::centred, false);
}

void RecipeWheel::paintHarmonicLabels(juce::Graphics& g, float cx, float cy, float radius)
{
    g.setColour(PhantomColours::textDim);
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 8.0f, juce::Font::bold));

    for (int i = 0; i < kNumHarmonics; ++i)
    {
        float angle = i * (kTwoPi / kNumHarmonics) - juce::MathConstants<float>::halfPi;
        float lx = cx + radius * std::cos(angle);
        float ly = cy + radius * std::sin(angle);
        juce::String label = "H" + juce::String(i + 2);
        g.drawText(label, juce::Rectangle<float>(lx - 12, ly - 6, 24, 12), juce::Justification::centred, false);
    }
}

juce::String RecipeWheel::pitchToNoteName(float hz)
{
    if (hz <= 0.0f) return "---";
    static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    float semitone = 12.0f * std::log2(hz / 440.0f) + 69.0f;
    int midi = juce::roundToInt(semitone);
    int note = ((midi % 12) + 12) % 12;
    int octave = (midi / 12) - 1;
    return juce::String(noteNames[note]) + juce::String(octave);
}
```

- [ ] **Step 3: Commit**

```bash
git add Source/UI/RecipeWheel.h Source/UI/RecipeWheel.cpp
git commit -m "feat(ui): add RecipeWheel with holographic rings, harmonic tanks, center OLED"
```

---

### Task 16: RecipeWheelPanel + wire into editor

**Files:**
- Create: `Source/UI/RecipeWheelPanel.h`
- Create: `Source/UI/RecipeWheelPanel.cpp`
- Modify: `Source/PluginProcessor.h` (add currentPitch atomic)
- Modify: `Source/PluginProcessor.cpp` (set pitch in processBlock)
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create RecipeWheelPanel**

```cpp
// Source/UI/RecipeWheelPanel.h
#pragma once
#include <JuceHeader.h>
#include "RecipeWheel.h"

class RecipeWheelPanel : public juce::Component,
                         private juce::AudioProcessorValueTreeState::Listener
{
public:
    RecipeWheelPanel(juce::AudioProcessorValueTreeState& apvts);
    ~RecipeWheelPanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void updatePitch(float hz);

private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void updateWheelFromPreset(int presetIndex);

    juce::AudioProcessorValueTreeState& apvtsRef;
    RecipeWheel wheel;

    juce::TextButton presetBtns[5];
    static constexpr const char* presetNames[] = { "Warm", "Aggr", "Hollow", "Dense", "Custom" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecipeWheelPanel)
};
```

```cpp
// Source/UI/RecipeWheelPanel.cpp
#include "RecipeWheelPanel.h"
#include "PhantomColours.h"
#include "../Parameters.h"

constexpr const char* RecipeWheelPanel::presetNames[];

RecipeWheelPanel::RecipeWheelPanel(juce::AudioProcessorValueTreeState& apvts)
    : apvtsRef(apvts)
{
    addAndMakeVisible(wheel);

    for (int i = 0; i < 5; ++i)
    {
        presetBtns[i].setButtonText(presetNames[i]);
        presetBtns[i].setClickingTogglesState(true);
        presetBtns[i].setRadioGroupId(100);
        presetBtns[i].onClick = [this, i]()
        {
            if (auto* p = apvtsRef.getParameter(ParamID::RECIPE_PRESET))
                p->setValueNotifyingHost((float)i / 4.0f);
        };
        addAndMakeVisible(presetBtns[i]);
    }
    presetBtns[0].setToggleState(true, juce::dontSendNotification);

    apvtsRef.addParameterListener(ParamID::RECIPE_PRESET, this);

    // Initialize wheel with Warm preset
    updateWheelFromPreset(0);
}

RecipeWheelPanel::~RecipeWheelPanel()
{
    apvtsRef.removeParameterListener(ParamID::RECIPE_PRESET, this);
}

void RecipeWheelPanel::updatePitch(float hz) { wheel.updatePitch(hz); }

void RecipeWheelPanel::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == ParamID::RECIPE_PRESET)
    {
        int idx = juce::roundToInt(newValue * 4.0f);
        juce::MessageManager::callAsync([this, idx]()
        {
            updateWheelFromPreset(idx);
            for (int i = 0; i < 5; ++i)
                presetBtns[i].setToggleState(i == idx, juce::dontSendNotification);
        });
    }
}

void RecipeWheelPanel::updateWheelFromPreset(int presetIndex)
{
    // Preset amplitude tables from Parameters.h
    static const float* presetTables[] = { kWarmAmps, kAggressiveAmps, kHollowAmps, kDenseAmps, nullptr };

    std::array<float, 7> amps;
    if (presetIndex < 4 && presetTables[presetIndex])
    {
        for (int i = 0; i < 7; ++i)
            amps[(size_t)i] = presetTables[presetIndex][i];
        wheel.setPresetName(presetNames[presetIndex]);
    }
    else
    {
        // Custom — read current APVTS values
        static const char* hIds[] = { ParamID::RECIPE_H2, ParamID::RECIPE_H3, ParamID::RECIPE_H4,
                                       ParamID::RECIPE_H5, ParamID::RECIPE_H6, ParamID::RECIPE_H7, ParamID::RECIPE_H8 };
        for (int i = 0; i < 7; ++i)
            amps[(size_t)i] = apvtsRef.getRawParameterValue(hIds[i])->load() / 100.0f;
        wheel.setPresetName("Custom");
    }

    wheel.setHarmonicAmplitudes(amps);

    // Also write to APVTS so the audio engine picks up the preset values
    if (presetIndex < 4 && presetTables[presetIndex])
    {
        static const char* hIds[] = { ParamID::RECIPE_H2, ParamID::RECIPE_H3, ParamID::RECIPE_H4,
                                       ParamID::RECIPE_H5, ParamID::RECIPE_H6, ParamID::RECIPE_H7, ParamID::RECIPE_H8 };
        for (int i = 0; i < 7; ++i)
            if (auto* p = apvtsRef.getParameter(hIds[i]))
                p->setValueNotifyingHost(presetTables[presetIndex][i] / 100.0f);
    }
}

void RecipeWheelPanel::paint(juce::Graphics& g)
{
    // Stamp label
    g.setColour(PhantomColours::textDim.withAlpha(0.12f));
    g.setFont(juce::Font(5.5f).withExtraKerningFactor(0.25f));
    g.drawText("RECIPE ENGINE \xc2\xb7 H2-H8", getLocalBounds().reduced(8, 6).removeFromTop(12),
               juce::Justification::topLeft, false);
}

void RecipeWheelPanel::resized()
{
    auto area = getLocalBounds().reduced(4);
    area.removeFromTop(14); // stamp label space

    // Preset strip at bottom
    auto presetRow = area.removeFromBottom(20);
    int btnW = (presetRow.getWidth() - 16) / 5;
    for (int i = 0; i < 5; ++i)
        presetBtns[i].setBounds(presetRow.removeFromLeft(btnW + (i < 4 ? 4 : 0)).reduced(2, 0));

    // Wheel fills the rest — keep it square
    int wheelSize = juce::jmin(area.getWidth(), area.getHeight());
    wheel.setBounds(area.getCentreX() - wheelSize / 2, area.getY() + (area.getHeight() - wheelSize) / 2,
                    wheelSize, wheelSize);
}
```

- [ ] **Step 2: Add currentPitch atomic to PluginProcessor.h**

Add as a public member after `apvts`:

```cpp
    std::atomic<float> currentPitch { -1.0f };
```

- [ ] **Step 3: Set pitch in PluginProcessor::processBlock()**

Find where `pitchTracker.getSmoothedPitch()` is called in processBlock and add after it:

```cpp
    currentPitch.store(pitchTracker.getSmoothedPitch(), std::memory_order_relaxed);
```

- [ ] **Step 4: Wire into PluginEditor**

Add includes, members, CMakeLists entries, constructor init, and update `resized()` to place the RecipeWheelPanel in the left area. Add Timer to editor for pitch polling:

Add to class declaration:
```cpp
    , private juce::Timer  // add Timer inheritance
```

Add member:
```cpp
    RecipeWheelPanel recipeWheelPanel;
```

Constructor init list:
```cpp
    recipeWheelPanel(p.apvts),
```

Constructor body:
```cpp
    addAndMakeVisible(recipeWheelPanel);
    startTimerHz(30);
```

Timer callback:
```cpp
void PhantomEditor::timerCallback()
{
    recipeWheelPanel.updatePitch(processor.currentPitch.load(std::memory_order_relaxed));
}
```

In `resized()`, replace the left area comment:
```cpp
    recipeWheelPanel.setBounds(leftArea);
```

Add to CMakeLists:
```cmake
    Source/UI/RecipeWheel.cpp
    Source/UI/RecipeWheelPanel.cpp
```

- [ ] **Step 5: Build and verify**

Run: `cmake --build build --target KaigenPhantom 2>&1 | tail -5`

Expected: Recipe wheel visible with animated holographic rings, harmonic tank fills matching Warm preset, preset strip toggles at bottom, center OLED showing pitch.

- [ ] **Step 6: Commit**

```bash
git add Source/UI/RecipeWheel.h Source/UI/RecipeWheel.cpp Source/UI/RecipeWheelPanel.h Source/UI/RecipeWheelPanel.cpp Source/PluginProcessor.h Source/PluginProcessor.cpp Source/PluginEditor.h Source/PluginEditor.cpp CMakeLists.txt
git commit -m "feat(ui): wire Phase 4 — RecipeWheel with harmonic tanks, presets, pitch display"
```

---

## Phase 5 — Spectrum & Meters

### Task 17: Add spectrum FIFO and peak atomics to Processor

**Files:**
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`

- [ ] **Step 1: Add spectrum and peak members to PluginProcessor.h**

Add as public members:

```cpp
    // Peak levels for I/O meters
    std::atomic<float> peakInL  { 0.0f };
    std::atomic<float> peakInR  { 0.0f };
    std::atomic<float> peakOutL { 0.0f };
    std::atomic<float> peakOutR { 0.0f };

    // Spectrum data — double-buffered, 80 log-spaced bins
    static constexpr int kSpectrumBins = 80;
    std::array<float, kSpectrumBins> spectrumData {};
    std::atomic<bool> spectrumReady { false };
```

Add as private members:

```cpp
    // FFT for spectrum analysis
    juce::dsp::FFT spectrumFFT { 9 }; // 512-point
    std::array<float, 1024> fftBuffer {};
    int fftWritePos = 0;
```

- [ ] **Step 2: Add peak and FFT computation in processBlock()**

At the end of `processBlock()`, after all audio processing and before the function returns:

```cpp
    // --- UI data: peaks and spectrum ---
    {
        auto numSamples = buffer.getNumSamples();
        auto* inL  = dryBuf.getReadPointer(0);  // dryBuf has the original input
        auto* inR  = dryBuf.getNumChannels() > 1 ? dryBuf.getReadPointer(1) : inL;
        auto* outL = buffer.getReadPointer(0);
        auto* outR = buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : outL;

        float pInL = 0.0f, pInR = 0.0f, pOutL = 0.0f, pOutR = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            pInL  = juce::jmax(pInL,  std::abs(inL[i]));
            pInR  = juce::jmax(pInR,  std::abs(inR[i]));
            pOutL = juce::jmax(pOutL, std::abs(outL[i]));
            pOutR = juce::jmax(pOutR, std::abs(outR[i]));
        }
        peakInL.store(pInL, std::memory_order_relaxed);
        peakInR.store(pInR, std::memory_order_relaxed);
        peakOutL.store(pOutL, std::memory_order_relaxed);
        peakOutR.store(pOutR, std::memory_order_relaxed);

        // FFT accumulation
        for (int i = 0; i < numSamples; ++i)
        {
            fftBuffer[(size_t)fftWritePos] = outL[i];
            if (++fftWritePos >= 512)
            {
                fftWritePos = 0;
                // Apply Hann window
                for (int j = 0; j < 512; ++j)
                {
                    float w = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * j / 511.0f));
                    fftBuffer[(size_t)j] *= w;
                }
                // Zero-pad upper half
                std::fill(fftBuffer.begin() + 512, fftBuffer.end(), 0.0f);
                spectrumFFT.performFrequencyOnlyForwardTransform(fftBuffer.data());
                // Bin into 80 log-spaced bands
                for (int b = 0; b < kSpectrumBins; ++b)
                {
                    float fLo = 30.0f * std::pow(16000.0f / 30.0f, (float)b / kSpectrumBins);
                    float fHi = 30.0f * std::pow(16000.0f / 30.0f, (float)(b + 1) / kSpectrumBins);
                    int binLo = juce::jmax(1, (int)(fLo * 512.0f / (float)sampleRate));
                    int binHi = juce::jmin(255, (int)(fHi * 512.0f / (float)sampleRate));
                    float sum = 0.0f;
                    for (int j = binLo; j <= binHi; ++j)
                        sum = juce::jmax(sum, fftBuffer[(size_t)j]);
                    // Convert to dB-ish (0 to 1 range)
                    spectrumData[(size_t)b] = juce::jlimit(0.0f, 1.0f, 
                        (juce::Decibels::gainToDecibels(sum + 1e-6f) + 48.0f) / 48.0f);
                }
                spectrumReady.store(true, std::memory_order_relaxed);
            }
        }
    }
```

- [ ] **Step 3: Commit**

```bash
git add Source/PluginProcessor.h Source/PluginProcessor.cpp
git commit -m "feat(ui): add spectrum FFT, peak levels, and currentPitch to Processor"
```

---

### Task 18: SpectrumAnalyzer and LevelMeter

**Files:**
- Create: `Source/UI/SpectrumAnalyzer.h`
- Create: `Source/UI/SpectrumAnalyzer.cpp`
- Create: `Source/UI/LevelMeter.h`
- Create: `Source/UI/LevelMeter.cpp`

- [ ] **Step 1: Create SpectrumAnalyzer**

```cpp
// Source/UI/SpectrumAnalyzer.h
#pragma once
#include <JuceHeader.h>

class PhantomProcessor;

class SpectrumAnalyzer : public juce::Component
{
public:
    enum class Mode { Bar, Line };

    SpectrumAnalyzer();
    void paint(juce::Graphics& g) override;

    void pullSpectrum(PhantomProcessor& processor);
    void toggleMode();

private:
    Mode mode = Mode::Bar;
    std::array<float, 80> bins {};
    std::array<float, 80> smoothed {};

    void paintBars(juce::Graphics& g, juce::Rectangle<float> area);
    void paintLine(juce::Graphics& g, juce::Rectangle<float> area);
    void paintGrid(juce::Graphics& g, juce::Rectangle<float> area);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};
```

```cpp
// Source/UI/SpectrumAnalyzer.cpp
#include "SpectrumAnalyzer.h"
#include "PhantomColours.h"
#include "../PluginProcessor.h"

SpectrumAnalyzer::SpectrumAnalyzer() { smoothed.fill(0.0f); }

void SpectrumAnalyzer::pullSpectrum(PhantomProcessor& processor)
{
    if (processor.spectrumReady.exchange(false, std::memory_order_relaxed))
    {
        bins = processor.spectrumData;
        for (size_t i = 0; i < 80; ++i)
            smoothed[i] += (bins[i] - smoothed[i]) * 0.3f;
        repaint();
    }
}

void SpectrumAnalyzer::toggleMode()
{
    mode = (mode == Mode::Bar) ? Mode::Line : Mode::Bar;
    repaint();
}

void SpectrumAnalyzer::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(PhantomColours::oledBlack);
    g.fillRoundedRectangle(bounds, 5.0f);

    auto area = bounds.reduced(5.0f, 3.0f);
    paintGrid(g, area);

    if (mode == Mode::Bar)
        paintBars(g, area);
    else
        paintLine(g, area);
}

void SpectrumAnalyzer::paintBars(juce::Graphics& g, juce::Rectangle<float> area)
{
    float barW = area.getWidth() / 80.0f;
    for (int i = 0; i < 80; ++i)
    {
        float h = smoothed[(size_t)i] * area.getHeight() * 0.88f;
        if (h < 1.0f) continue;
        float x = area.getX() + i * barW + 0.5f;
        float w = barW - 1.0f;

        juce::ColourGradient grad(PhantomColours::phosphorWhite.withAlpha(0.5f), x, area.getBottom(),
                                   PhantomColours::phosphorWhite.withAlpha(0.04f), x, area.getBottom() - h, false);
        g.setGradientFill(grad);
        g.fillRect(x, area.getBottom() - h, w, h);

        // Peak cap
        g.setColour(PhantomColours::phosphorWhite.withAlpha(0.65f));
        g.fillRect(x, area.getBottom() - h, w, 1.0f);
    }
}

void SpectrumAnalyzer::paintLine(juce::Graphics& g, juce::Rectangle<float> area)
{
    juce::Path curve;
    for (int i = 0; i < 80; ++i)
    {
        float x = area.getX() + (i / 79.0f) * area.getWidth();
        float y = area.getBottom() - smoothed[(size_t)i] * area.getHeight() * 0.88f;
        if (i == 0) curve.startNewSubPath(x, y);
        else curve.lineTo(x, y);
    }

    // Fill underneath
    juce::Path fill(curve);
    fill.lineTo(area.getRight(), area.getBottom());
    fill.lineTo(area.getX(), area.getBottom());
    fill.closeSubPath();
    juce::ColourGradient fillGrad(PhantomColours::phosphorWhite.withAlpha(0.2f), 0, area.getBottom(),
                                   juce::Colours::transparentBlack, 0, area.getY(), false);
    g.setGradientFill(fillGrad);
    g.fillPath(fill);

    // Stroke
    g.setColour(PhantomColours::phosphorWhite.withAlpha(0.6f));
    g.strokePath(curve, juce::PathStrokeType(1.5f));
}

void SpectrumAnalyzer::paintGrid(juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour(juce::Colour(0x05ffffff));
    // Horizontal dB lines
    for (int i = 0; i < 5; ++i)
    {
        float y = area.getY() + (i / 5.0f) * area.getHeight();
        g.drawHorizontalLine((int)y, area.getX(), area.getRight());
    }

    // Frequency labels
    g.setColour(juce::Colour(0x14ffffff));
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 5.0f, juce::Font::plain));
    const char* freqLabels[] = { "30", "60", "125", "250", "500", "1k", "2k", "4k", "8k", "16k" };
    for (int i = 0; i < 10; ++i)
    {
        float x = area.getX() + (i / 9.5f) * area.getWidth();
        g.drawText(freqLabels[i], juce::Rectangle<float>(x, area.getBottom() - 8, 20, 8), juce::Justification::centredLeft, false);
    }
}
```

- [ ] **Step 2: Create LevelMeter**

```cpp
// Source/UI/LevelMeter.h
#pragma once
#include <JuceHeader.h>

class LevelMeter : public juce::Component
{
public:
    LevelMeter(const juce::String& label);
    void paint(juce::Graphics& g) override;
    void setLevel(float leftPeak, float rightPeak);
private:
    juce::String label;
    float level = 0.0f;
    float smoothed = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeter)
};
```

```cpp
// Source/UI/LevelMeter.cpp
#include "LevelMeter.h"
#include "PhantomColours.h"

LevelMeter::LevelMeter(const juce::String& l) : label(l) {}

void LevelMeter::setLevel(float leftPeak, float rightPeak)
{
    float newLevel = juce::jmax(leftPeak, rightPeak);
    if (std::abs(newLevel - level) > 0.001f)
    {
        level = newLevel;
        smoothed += (level - smoothed) * 0.3f;
        repaint();
    }
}

void LevelMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Black background
    g.setColour(PhantomColours::oledBlack);
    g.fillRoundedRectangle(bounds, 3.0f);

    // Fill
    auto inner = bounds.reduced(1.0f);
    float h = smoothed * inner.getHeight() * 0.85f;
    if (h > 0.5f)
    {
        juce::ColourGradient grad(PhantomColours::phosphorWhite.withAlpha(0.35f), 0, inner.getBottom(),
                                   PhantomColours::phosphorWhite.withAlpha(0.05f), 0, inner.getBottom() - h, false);
        g.setGradientFill(grad);
        g.fillRect(inner.getX(), inner.getBottom() - h, inner.getWidth(), h);

        // Peak line
        g.setColour(PhantomColours::phosphorWhite.withAlpha(0.5f));
        g.fillRect(inner.getX(), inner.getBottom() - h, inner.getWidth(), 1.0f);
    }

    // Label
    g.setColour(PhantomColours::textDim.withAlpha(0.2f));
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 5.0f, juce::Font::plain));
    g.drawText(label, bounds.reduced(1, 2).removeFromBottom(14), juce::Justification::centredBottom, false);
}
```

- [ ] **Step 3: Commit**

```bash
git add Source/UI/SpectrumAnalyzer.h Source/UI/SpectrumAnalyzer.cpp Source/UI/LevelMeter.h Source/UI/LevelMeter.cpp
git commit -m "feat(ui): add SpectrumAnalyzer (bar+line) and LevelMeter components"
```

---

### Task 19: Wire Phase 5 — spectrum and meters into editor

**Files:**
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add new .cpp files to CMakeLists.txt**

Append:
```cmake
    Source/UI/SpectrumAnalyzer.cpp
    Source/UI/LevelMeter.cpp
```

- [ ] **Step 2: Update PluginEditor.h**

Add includes:
```cpp
#include "UI/SpectrumAnalyzer.h"
#include "UI/LevelMeter.h"
```

Add members:
```cpp
    SpectrumAnalyzer spectrumAnalyzer;
    LevelMeter inputMeter  { "IN" };
    LevelMeter outputMeter { "OUT" };
```

- [ ] **Step 3: Update PluginEditor.cpp constructor**

```cpp
    addAndMakeVisible(spectrumAnalyzer);
    addAndMakeVisible(inputMeter);
    addAndMakeVisible(outputMeter);
```

- [ ] **Step 4: Update timerCallback**

```cpp
void PhantomEditor::timerCallback()
{
    recipeWheelPanel.updatePitch(processor.currentPitch.load(std::memory_order_relaxed));
    inputMeter.setLevel(processor.peakInL.load(std::memory_order_relaxed),
                        processor.peakInR.load(std::memory_order_relaxed));
    outputMeter.setLevel(processor.peakOutL.load(std::memory_order_relaxed),
                         processor.peakOutR.load(std::memory_order_relaxed));
    spectrumAnalyzer.pullSpectrum(processor);
}
```

- [ ] **Step 5: Update resized() — add spectrum row at bottom of right panel**

After the row2Seam placement:

```cpp
    // Spectrum row fills remaining space
    auto specRow = rightArea;
    inputMeter.setBounds(specRow.removeFromLeft(14));
    outputMeter.setBounds(specRow.removeFromRight(14));
    spectrumAnalyzer.setBounds(specRow);
```

- [ ] **Step 6: Build and verify**

Run: `cmake --build build --target KaigenPhantom 2>&1 | tail -5`

Expected: Full UI visible — header, footer, recipe wheel with animated rings, three rows of knob panels, spectrum analyzer, I/O meters. All knobs change audio parameters. Mode switch toggles PitchTracker/Deconfliction panels.

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt Source/PluginEditor.h Source/PluginEditor.cpp
git commit -m "feat(ui): wire Phase 5 — spectrum analyzer and I/O meters, full UI complete"
```

---

## Summary

**19 tasks** across 5 phases. Each task produces a compilable, testable increment. After Task 19 the plugin has a fully functional UI with:
- All 20+ main-view parameters bound to APVTS
- Recipe wheel with animated holographic rings and harmonic tank fills
- Mode-switched panels (Effect: PitchTracker, Instrument: Deconfliction)
- Real-time spectrum analyzer (bar + line modes)
- I/O level meters
- White phosphor neumorphic visual language throughout

Phase 6 (Settings panel, polish) is deferred to a follow-up plan after the core UI is verified in a DAW.
