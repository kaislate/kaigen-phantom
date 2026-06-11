# Native UI: settings overlay, meter relocation, osc Auto, collapse reflow — implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Six related native-UI improvements: settings overlay, top-bar gear wiring, binaural quick toggle in Advanced, meter relocation to a fixed bottom-right column, oscilloscope AUTO overlay, and a layout reflow that anchors the bottom row so only the spectrum resizes when Advanced collapses.

**Architecture:** All editor-side. No new APVTS params. Two new widget classes (`ChoiceToggle`, `SettingsOverlay`), modifications to `Oscilloscope`, `RightPanel`, `TopBar`, and `NativePluginEditor`. The bottom row of the visualizer area is anchored to the panel bottom; the spectrum becomes the only variable-height element.

**Tech Stack:** JUCE 8 / C++ / `juce::Component`, `juce::Button`, `juce::ParameterAttachment`.

**Spec:** `docs/superpowers/specs/2026-05-22-native-ui-settings-meters-osc-design.md`

---

### Task 1: New `ChoiceToggle` widget (2-state choice param as button)

**Files:**
- Create: `Source/UI/widgets/ChoiceToggle.h`
- Create: `Source/UI/widgets/ChoiceToggle.cpp`
- Modify: `CMakeLists.txt` (register the new source)

`EtchedToggle` only handles `AudioParameterBool`. `binaural_mode` is `AudioParameterChoice` (3 values; we only use indices 0 and 1). `ChoiceToggle` is a sibling widget that toggles a 2-value choice param.

- [ ] **Step 1: Create the header**

Write `Source/UI/widgets/ChoiceToggle.h`:

```cpp
// Source/UI/widgets/ChoiceToggle.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace kaigen::phantom
{

/** Single-word toggle painted in the EtchedToggle style, but bound to a
 *  juce::AudioParameterChoice. Used when a 2-value choice needs the same
 *  visual affordance as the bool EtchedToggle (off = etched dark; on =
 *  backlit glow). Clicking cycles the param between the two configured
 *  choice indices.
 */
class ChoiceToggle : public juce::Button
{
public:
    /** @param apvts          The plugin's APVTS.
     *  @param choiceParamId  Choice param ID. Must resolve to an
     *                        AudioParameterChoice with at least two values.
     *  @param label          Single-word label drawn on the button.
     *  @param onChoiceIndex  The choice index that counts as "on" (lit).
     *                        The "off" state is always index 0.
     */
    ChoiceToggle(juce::AudioProcessorValueTreeState& apvts,
                 const juce::String& choiceParamId,
                 juce::String label,
                 int onChoiceIndex = 1);
    ~ChoiceToggle() override;

    void paintButton(juce::Graphics& g,
                     bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override;

    /** True iff this toggle was constructed with an "a_" or "b_" prefixed
     *  param ID — i.e., a per-engine parameter that can be retargeted. */
    bool isPerEngine() const noexcept { return enginePrefix.isNotEmpty(); }

    /** Rebind to `<activePrefix><leaf>`. No mirror support (LINK is handled
     *  upstream by the per-engine knob widgets). */
    void setEnginePrefix(const juce::String& activePrefix);

private:
    void clicked() override;

    juce::AudioProcessorValueTreeState* apvtsRef { nullptr };
    juce::RangedAudioParameter*         param    { nullptr };
    juce::String                        labelText;
    juce::String                        enginePrefix;
    juce::String                        leafName;
    int                                 onIndex { 1 };
    std::unique_ptr<juce::ParameterAttachment> attachment;

    void rebuildAttachment(const juce::String& fullId);
    void onParamValueChanged(float newValue);

    bool isActiveState { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChoiceToggle)
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Create the .cpp**

Write `Source/UI/widgets/ChoiceToggle.cpp`:

```cpp
// Source/UI/widgets/ChoiceToggle.cpp
#include "ChoiceToggle.h"

namespace kaigen::phantom
{

namespace
{
    constexpr float kFontSize = 11.0f;
    constexpr float kKerning  = 0.18f;
}

ChoiceToggle::ChoiceToggle(juce::AudioProcessorValueTreeState& apvts,
                            const juce::String& choiceParamId,
                            juce::String label,
                            int onChoiceIndex)
    : juce::Button(label),
      apvtsRef(&apvts),
      labelText(std::move(label)),
      onIndex(onChoiceIndex)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);

    // Split "a_binaural_mode" → enginePrefix "a_" + leafName "binaural_mode".
    if (choiceParamId.startsWith("a_") || choiceParamId.startsWith("b_"))
    {
        enginePrefix = choiceParamId.substring(0, 2);
        leafName     = choiceParamId.substring(2);
    }
    else
    {
        leafName = choiceParamId;
    }

    rebuildAttachment(choiceParamId);
}

ChoiceToggle::~ChoiceToggle() = default;

void ChoiceToggle::setEnginePrefix(const juce::String& activePrefix)
{
    if (! isPerEngine()) return;
    if (activePrefix == enginePrefix) return;
    enginePrefix = activePrefix;
    rebuildAttachment(enginePrefix + leafName);
}

void ChoiceToggle::rebuildAttachment(const juce::String& fullId)
{
    attachment.reset();
    param = apvtsRef->getParameter(fullId);
    if (param == nullptr) return;

    attachment = std::make_unique<juce::ParameterAttachment>(
        *param,
        [this](float v) { onParamValueChanged(v); },
        nullptr);
    attachment->sendInitialUpdate();
}

void ChoiceToggle::onParamValueChanged(float newValue)
{
    // ParameterAttachment passes the un-normalised parameter value for
    // choice params (the int index as a float).
    const int idx = (int) std::round(newValue);
    const bool nowActive = (idx == onIndex);
    if (nowActive != isActiveState)
    {
        isActiveState = nowActive;
        repaint();
    }
}

void ChoiceToggle::clicked()
{
    if (! attachment) return;
    const int target = isActiveState ? 0 : onIndex;
    attachment->setValueAsCompleteGesture((float) target);
}

void ChoiceToggle::paintButton(juce::Graphics& g,
                                bool shouldDrawButtonAsHighlighted,
                                bool /*shouldDrawButtonAsDown*/)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto upper  = labelText.toUpperCase();
    const bool isHover = shouldDrawButtonAsHighlighted;

    juce::Font font(juce::FontOptions("Space Grotesk", kFontSize, juce::Font::bold));
    font.setExtraKerningFactor(kKerning);
    g.setFont(font);

    if (isActiveState)
    {
        for (int r = 4; r >= 1; --r)
        {
            g.setColour(juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 0.06f));
            g.drawText(upper, bounds.translated(-(float) r, 0.0f), juce::Justification::centred, false);
            g.drawText(upper, bounds.translated( (float) r, 0.0f), juce::Justification::centred, false);
            g.drawText(upper, bounds.translated(0.0f, -(float) r), juce::Justification::centred, false);
            g.drawText(upper, bounds.translated(0.0f,  (float) r), juce::Justification::centred, false);
        }
        g.setColour(juce::Colour(0xfff5f8fb));
        g.drawText(upper, bounds, juce::Justification::centred, false);
    }
    else
    {
        const auto textColour = isHover ? juce::Colour(0x80000000)
                                          : juce::Colour(0x38000000);
        g.setColour(juce::Colour(0x80FFFFFF));
        g.drawText(upper, bounds.translated(0.0f, 1.0f), juce::Justification::centred, false);
        g.setColour(textColour);
        g.drawText(upper, bounds, juce::Justification::centred, false);
    }
}

} // namespace kaigen::phantom
```

- [ ] **Step 3: Register in CMakeLists.txt**

Open `CMakeLists.txt`. Locate the existing `EtchedToggle.cpp` reference in the `target_sources(...)` block. Add `Source/UI/widgets/ChoiceToggle.cpp` right after it (alphabetical order within the widgets directory).

If the source list is glob-based and picks up new files automatically, no change is needed — check the existing entries for `EtchedToggle.cpp`. If listed explicitly, add the new file in the same style.

- [ ] **Step 4: Verify build**

```
cd "C:/Documents/NEw project/Kaigen Phantom/build" && cmake . && "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/amd64/MSBuild.exe" KaigenPhantom.sln //p:Configuration=Release //p:Platform=x64 //m //nologo //v:minimal 2>&1 | tail -8
```

Expected: 0 errors. ChoiceToggle compiles and links into the plugin; nothing uses it yet.

- [ ] **Step 5: Commit**

```
cd "C:/Documents/NEw project/Kaigen Phantom" && git add Source/UI/widgets/ChoiceToggle.h Source/UI/widgets/ChoiceToggle.cpp CMakeLists.txt && git commit -m "feat(ui-native): add ChoiceToggle widget — 2-value choice param as etched button"
```

---

### Task 2: Binaural quick toggle above the Width mini-knob

**Files:**
- Modify: `Source/UI/panels/RightPanel.h`
- Modify: `Source/UI/panels/RightPanel.cpp`

- [ ] **Step 1: Include + add member**

Open `Source/UI/panels/RightPanel.h`. Find the `#include "../widgets/EtchedToggle.h"` line (existing). Add right after:

```cpp
#include "../widgets/ChoiceToggle.h"
```

Find the `std::array<std::unique_ptr<PhantomMiniKnob>, 14> miniKnobs;` line. Add right after:

```cpp
    // Binaural quick toggle — sits above the Width mini-knob in the
    // Advanced row. Off (unlit) = binaural_mode = 0 (off). On (lit) =
    // binaural_mode = 1 (Spread). Skip choice index 2 (Voice-Split is
    // stubbed in BinauralStage).
    ChoiceToggle binauralToggle;
```

- [ ] **Step 2: Initialise + add to visible children**

Open `Source/UI/panels/RightPanel.cpp`. Find the constructor initializer list and the autoGainToggle line:

```cpp
      autoGainToggle(apvts, "input_gain_auto", "Auto"),
```

Add right after (before `oscilloscope`):

```cpp
      binauralToggle(apvts, "a_binaural_mode", "BIN", 1),
```

Find `addAndMakeVisible(autoGainToggle);` (later in the constructor). Add right after:

```cpp
    addAndMakeVisible(binauralToggle);
```

- [ ] **Step 3: Engine-prefix retargeting**

In `RightPanel::setEnginePrefix`, find the existing loop:

```cpp
    for (auto* k : { &saturationKnob, &shapeKnob, &skipKnob, &trimKnob,
                     &widthKnob, &outGainKnob })
        k->setEnginePrefix(activePrefix, mirrorPrefix);
```

Add right after (separate call because ChoiceToggle's signature differs from PhantomKnob's):

```cpp
    // ChoiceToggle retarget is single-arg (no mirror — choice mirroring is
    // handled by the WordSelector in the settings overlay for LINK mode).
    binauralToggle.setEnginePrefix(activePrefix);
```

- [ ] **Step 4: Layout in `resized()`**

In `RightPanel::resized()`, find the Advanced mini-knob row block:

```cpp
    if (advancedExpanded)
    {
        // Mini knob row spans the FULL panel width (toggle is above it now).
        ...
        constexpr int miniRowY = advancedToggleY + advancedToggleH + 14;
        constexpr int miniW    = 82;
        constexpr int miniH    = 93;
        const int rowLeft  = 12;
        const int rowRight = getWidth() - 12;
        const int rowAvail = rowRight - rowLeft;
        const int n = (int) miniKnobs.size();

        const int slotW = (n > 0) ? (rowAvail - miniW) / juce::jmax(1, n - 1) : 0;
        int mx = rowLeft;
        for (auto& mk : miniKnobs)
        {
            mk->setBounds(mx, miniRowY, miniW, miniH);
            mx += slotW;
        }
        advancedCardBottom = miniRowY + miniH + 6;
    }
```

Replace with (adds the binaural toggle layout pass after the knob row is positioned):

```cpp
    if (advancedExpanded)
    {
        // Mini knob row spans the FULL panel width (toggle is above it now).
        constexpr int miniRowY = advancedToggleY + advancedToggleH + 14;
        constexpr int miniW    = 82;
        constexpr int miniH    = 93;
        const int rowLeft  = 12;
        const int rowRight = getWidth() - 12;
        const int rowAvail = rowRight - rowLeft;
        const int n = (int) miniKnobs.size();

        const int slotW = (n > 0) ? (rowAvail - miniW) / juce::jmax(1, n - 1) : 0;
        int mx = rowLeft;
        int widthKnobX = rowLeft;   // captured for the binaural toggle below
        for (size_t i = 0; i < miniKnobs.size(); ++i)
        {
            miniKnobs[i]->setBounds(mx, miniRowY, miniW, miniH);
            // Width is the last mini-knob (index 13 — "binaural_width").
            if (i == miniKnobs.size() - 1)
                widthKnobX = mx;
            mx += slotW;
        }

        // Binaural quick toggle — sits in the 14 px gap directly above the
        // Width mini-knob. ~40 x 12, centred on the knob body (knob body is
        // 48 px wide inside the 82-px component bounds, so the body centre
        // is at widthKnobX + (miniW / 2)).
        constexpr int kBinW = 40;
        constexpr int kBinH = 12;
        const int binX = widthKnobX + (miniW - kBinW) / 2;
        const int binY = advancedToggleY + advancedToggleH + 2;
        binauralToggle.setBounds(binX, binY, kBinW, kBinH);

        advancedCardBottom = miniRowY + miniH + 6;
    }
    else
    {
        advancedCardBottom = advancedToggleY + advancedToggleH + 8;
    }
```

Note: when `advancedExpanded` is false, `binauralToggle` is still visible by default. Hide it when Advanced is collapsed by adding right at the start of the `else` branch above the assignment:

Replace:

```cpp
    else
    {
        advancedCardBottom = advancedToggleY + advancedToggleH + 8;
    }
```

with:

```cpp
    else
    {
        binauralToggle.setBounds(0, 0, 0, 0);  // hide while Advanced is collapsed
        advancedCardBottom = advancedToggleY + advancedToggleH + 8;
    }
```

The zero-size bounds keep it logically visible but unrenderable. (Alternative: call `binauralToggle.setVisible(advancedExpanded);` at the top of `resized()` — but JUCE then re-runs layout, so the zero-bounds approach is simpler.)

- [ ] **Step 5: Build**

```
cd "C:/Documents/NEw project/Kaigen Phantom/build" && "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/amd64/MSBuild.exe" KaigenPhantom.sln //p:Configuration=Release //p:Platform=x64 //m //nologo //v:minimal 2>&1 | tail -8
```

Expected: 0 errors. BIN toggle appears above the rightmost mini-knob (Width) when Advanced is expanded. Clicking it toggles `a_binaural_mode` between 0 and 1.

- [ ] **Step 6: Commit**

```
cd "C:/Documents/NEw project/Kaigen Phantom" && git add Source/UI/panels/RightPanel.h Source/UI/panels/RightPanel.cpp && git commit -m "feat(ui-native): binaural quick toggle above the Width mini-knob in Advanced"
```

---

### Task 3: Wire the settings button in TopBar

**Files:**
- Modify: `Source/UI/panels/TopBar.h`
- Modify: `Source/UI/panels/TopBar.cpp`

The `settingsBtn` (`HeaderButton::Icon::Settings`) already exists in TopBar; it just has no `onClick`. Expose an `onSettingsRequested` callback on TopBar so `NativePluginEditor` can wire it.

- [ ] **Step 1: Add the public callback to TopBar.h**

Open `Source/UI/panels/TopBar.h`. Find the public section with `getPresetSelector`/`getModeToggle`. Add right after `getModeToggle`:

```cpp
    /** Fired when the user clicks the gear button. The editor owns the
     *  settings overlay component and decides what to do. */
    std::function<void()> onSettingsRequested;
```

- [ ] **Step 2: Wire the button's onClick in the TopBar constructor**

Open `Source/UI/panels/TopBar.cpp`. Find the existing `settingsBtn = std::make_unique<HeaderButton>(HeaderButton::Icon::Settings);` line (around line 18) in the constructor. After the `addAndMakeVisible(*settingsBtn);` call (a few lines later), add:

```cpp
    settingsBtn->onClick = [this] {
        if (onSettingsRequested) onSettingsRequested();
    };
```

- [ ] **Step 3: Build**

```
cd "C:/Documents/NEw project/Kaigen Phantom/build" && "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/amd64/MSBuild.exe" KaigenPhantom.sln //p:Configuration=Release //p:Platform=x64 //m //nologo //v:minimal 2>&1 | tail -6
```

Expected: 0 errors. Clicking the gear button now calls `onSettingsRequested` if set; nothing happens visually yet because no listener is connected.

- [ ] **Step 4: Commit**

```
cd "C:/Documents/NEw project/Kaigen Phantom" && git add Source/UI/panels/TopBar.h Source/UI/panels/TopBar.cpp && git commit -m "feat(ui-native): expose onSettingsRequested callback on TopBar gear button"
```

---

### Task 4: SettingsOverlay component

**Files:**
- Create: `Source/UI/panels/SettingsOverlay.h`
- Create: `Source/UI/panels/SettingsOverlay.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the header**

Create `Source/UI/panels/SettingsOverlay.h`:

```cpp
// Source/UI/panels/SettingsOverlay.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../widgets/WordSelector.h"
#include "../widgets/PhantomKnob.h"
#include "../widgets/EtchedToggle.h"

namespace kaigen::phantom
{

/** Modal settings overlay — native port of the WebView's #settings-overlay
 *  card. Three sections: Binaural (mode + width), Envelope Source, MIDI
 *  Triggering (two toggles). Dismissed via click-on-backdrop or × button.
 *  Lifetime: owned by NativePluginEditor, added with setVisible(false) by
 *  default. */
class SettingsOverlay : public juce::Component
{
public:
    SettingsOverlay(juce::AudioProcessorValueTreeState& apvts);
    ~SettingsOverlay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

    /** Per-engine widgets retarget to the active engine's prefix. Called
     *  by NativePluginEditor::applyEngineFocus. */
    void setEnginePrefix(const juce::String& activePrefix,
                         const juce::String& mirrorPrefix = {});

    /** Called when the overlay should be dismissed (close button or
     *  backdrop click). Wires back to the editor's hide logic. */
    std::function<void()> onDismiss;

private:
    juce::AudioProcessorValueTreeState& apvts;

    // Card bounds computed in resized(); used by mouseDown to discriminate
    // backdrop clicks from clicks on the card content.
    juce::Rectangle<int> cardBounds;

    // Binaural section.
    WordSelector binauralModeSelector;
    PhantomKnob  binauralWidthKnob;

    // Envelope source section.
    WordSelector envSourceSelector;

    // MIDI triggering section.
    EtchedToggle midiTriggerToggle;
    EtchedToggle midiGateReleaseToggle;

    // Close button.
    juce::TextButton closeButton { "\xC3\x97" };   // UTF-8 ×

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsOverlay)
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Write the .cpp**

Create `Source/UI/panels/SettingsOverlay.cpp`:

```cpp
// Source/UI/panels/SettingsOverlay.cpp
#include "SettingsOverlay.h"
#include "../Theme.h"

namespace kaigen::phantom
{

namespace
{
    constexpr int kCardWidth  = 480;
    constexpr int kCardHeight = 380;

    void drawSectionLabel(juce::Graphics& g, juce::Rectangle<int> bounds,
                          const juce::String& text)
    {
        const auto font = juce::Font(juce::FontOptions("Space Grotesk", 10.0f, juce::Font::bold))
                              .withExtraKerningFactor(0.25f);
        Theme::drawEtchedText(g, text.toUpperCase(), bounds, juce::Justification::centredLeft,
                              font, Theme::textOnLightLabel);
    }
}

SettingsOverlay::SettingsOverlay(juce::AudioProcessorValueTreeState& a)
    : apvts(a),
      binauralModeSelector(apvts, "a_binaural_mode",
                            juce::StringArray{ "Off", "Spread" }),
      binauralWidthKnob   (apvts, "a_binaural_width",
                            PhantomKnob::Size::Medium, "Width"),
      envSourceSelector   (apvts, "a_env_source",
                            juce::StringArray{ "Input", "Sidechain" }),
      midiTriggerToggle   (apvts, "a_midi_trigger_enabled", "Trigger"),
      midiGateReleaseToggle(apvts, "a_midi_gate_release",   "Gate Release")
{
    addAndMakeVisible(binauralModeSelector);
    addAndMakeVisible(binauralWidthKnob);
    addAndMakeVisible(envSourceSelector);
    addAndMakeVisible(midiTriggerToggle);
    addAndMakeVisible(midiGateReleaseToggle);

    closeButton.setColour(juce::TextButton::buttonColourId,   juce::Colours::transparentBlack);
    closeButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    closeButton.setColour(juce::TextButton::textColourOffId,  juce::Colour(0xff656769));
    closeButton.setColour(juce::TextButton::textColourOnId,   juce::Colour(0xff656769));
    closeButton.onClick = [this] { if (onDismiss) onDismiss(); };
    addAndMakeVisible(closeButton);
}

SettingsOverlay::~SettingsOverlay() = default;

void SettingsOverlay::setEnginePrefix(const juce::String& activePrefix,
                                      const juce::String& mirrorPrefix)
{
    binauralModeSelector.setEnginePrefix(activePrefix, mirrorPrefix);
    binauralWidthKnob   .setEnginePrefix(activePrefix, mirrorPrefix);
    envSourceSelector   .setEnginePrefix(activePrefix, mirrorPrefix);
    // EtchedToggle has no setEnginePrefix — the param it binds to is per-
    // engine but rebinding requires reconstructing the attachment. For now
    // both MIDI toggles stay bound to a_*. (Same status quo as the rest of
    // the editor's toggle widgets — addressed in a future sub-commit.)
}

void SettingsOverlay::paint(juce::Graphics& g)
{
    // Backdrop — dimmed full-component.
    g.fillAll(juce::Colour::fromFloatRGBA(0.0f, 0.0f, 0.0f, 0.40f));

    // Card surface — neumorphic light plastic with subtle shadow.
    if (cardBounds.isEmpty()) return;

    juce::Path cardShadow;
    cardShadow.addRoundedRectangle(cardBounds.toFloat(), 8.0f);
    juce::DropShadow(juce::Colour::fromFloatRGBA(0.0f, 0.0f, 0.0f, 0.25f), 18, { 0, 6 })
        .drawForPath(g, cardShadow);

    juce::ColourGradient grad(juce::Colour(0xffBBBDBF), cardBounds.toFloat().getTopLeft(),
                              juce::Colour(0xffAEAFB1), cardBounds.toFloat().getBottomRight(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(cardBounds.toFloat(), 8.0f);
    g.setColour(juce::Colour::fromFloatRGBA(0.0f, 0.0f, 0.0f, 0.15f));
    g.drawRoundedRectangle(cardBounds.toFloat(), 8.0f, 1.0f);

    // Header — "SETTINGS" etched label.
    const auto headerBounds = cardBounds.withHeight(28).reduced(16, 4);
    {
        const auto headerFont = juce::Font(juce::FontOptions("Space Grotesk", 13.0f, juce::Font::bold))
                                    .withExtraKerningFactor(0.30f);
        Theme::drawEtchedText(g, "SETTINGS", headerBounds, juce::Justification::centredLeft,
                              headerFont, Theme::textOnLightLabel);
    }

    // Section labels above each row (computed positions match resized()).
    const int colX     = cardBounds.getX() + 24;
    const int colW     = cardBounds.getWidth() - 48;
    int       sectionY = cardBounds.getY() + 40;
    constexpr int kSectionGap = 12;
    constexpr int kLabelH     = 14;
    constexpr int kRowH       = 56;

    drawSectionLabel(g, { colX, sectionY, colW, kLabelH }, "BINAURAL");
    sectionY += kLabelH + kRowH + kSectionGap;

    drawSectionLabel(g, { colX, sectionY, colW, kLabelH }, "ENVELOPE SOURCE");
    sectionY += kLabelH + kRowH + kSectionGap;

    drawSectionLabel(g, { colX, sectionY, colW, kLabelH }, "MIDI TRIGGERING");
}

void SettingsOverlay::resized()
{
    // Centre the card in the overlay.
    const int x = (getWidth()  - kCardWidth)  / 2;
    const int y = (getHeight() - kCardHeight) / 2;
    cardBounds = { x, y, kCardWidth, kCardHeight };

    // Close button — top-right of card.
    constexpr int kCloseSize = 22;
    closeButton.setBounds(cardBounds.getRight() - kCloseSize - 8,
                          cardBounds.getY() + 6,
                          kCloseSize, kCloseSize);

    const int colX = cardBounds.getX() + 24;
    const int colW = cardBounds.getWidth() - 48;
    int       sectionY = cardBounds.getY() + 40;
    constexpr int kLabelH     = 14;
    constexpr int kRowH       = 56;
    constexpr int kSectionGap = 12;

    // Binaural row: mode selector on the left, width knob on the right.
    {
        sectionY += kLabelH + 2;
        const int knobW = 136;   // PhantomKnob Medium natural width
        const int knobH = 136;
        binauralModeSelector.setBounds(colX, sectionY + (kRowH - 18) / 2,
                                        colW - knobW - 16, 18);
        // Knob is taller than the row — let it overflow vertically; visually OK because
        // section spacing accounts for the shadow halo.
        binauralWidthKnob.setBounds(cardBounds.getRight() - 24 - knobW,
                                     sectionY + (kRowH - knobH) / 2,
                                     knobW, knobH);
        sectionY += kRowH + kSectionGap;
    }

    // Envelope source row: selector only.
    {
        sectionY += kLabelH + 2;
        envSourceSelector.setBounds(colX, sectionY + (kRowH - 18) / 2, colW, 18);
        sectionY += kRowH + kSectionGap;
    }

    // MIDI triggering row: two toggles side by side.
    {
        sectionY += kLabelH + 2;
        constexpr int kToggleW = 100;
        constexpr int kToggleH = 18;
        midiTriggerToggle    .setBounds(colX,
                                         sectionY + (kRowH - kToggleH) / 2,
                                         kToggleW, kToggleH);
        midiGateReleaseToggle.setBounds(colX + kToggleW + 24,
                                         sectionY + (kRowH - kToggleH) / 2,
                                         kToggleW + 20, kToggleH);
    }
}

void SettingsOverlay::mouseDown(const juce::MouseEvent& e)
{
    // Click outside the card = dismiss.
    if (! cardBounds.contains(e.getPosition()))
    {
        if (onDismiss) onDismiss();
    }
}

} // namespace kaigen::phantom
```

- [ ] **Step 3: Register in CMakeLists.txt**

Open `CMakeLists.txt`. Add `Source/UI/panels/SettingsOverlay.cpp` next to the other panel sources (e.g., after `Source/UI/panels/PresetBrowser.cpp` if the list is explicit).

- [ ] **Step 4: Build**

```
cd "C:/Documents/NEw project/Kaigen Phantom/build" && cmake . && "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/amd64/MSBuild.exe" KaigenPhantom.sln //p:Configuration=Release //p:Platform=x64 //m //nologo //v:minimal 2>&1 | tail -8
```

Expected: 0 errors. Overlay compiles; nothing instantiates it yet.

- [ ] **Step 5: Commit**

```
cd "C:/Documents/NEw project/Kaigen Phantom" && git add Source/UI/panels/SettingsOverlay.h Source/UI/panels/SettingsOverlay.cpp CMakeLists.txt && git commit -m "feat(ui-native): add SettingsOverlay component (Binaural / EnvSource / MIDI)"
```

---

### Task 5: Wire SettingsOverlay into NativePluginEditor

**Files:**
- Modify: `Source/UI/NativePluginEditor.h`
- Modify: `Source/UI/NativePluginEditor.cpp`

- [ ] **Step 1: Include + add member**

Open `Source/UI/NativePluginEditor.h`. Find:

```cpp
#include "panels/PresetBrowser.h"
```

Add right after:

```cpp
#include "panels/SettingsOverlay.h"
```

Find the private members. After `PresetBrowser presetBrowser;`:

```cpp
    SettingsOverlay settingsOverlay;
```

- [ ] **Step 2: Construct in initializer list + addAndMakeVisible + wire**

Open `Source/UI/NativePluginEditor.cpp`. Find the constructor initializer list:

```cpp
NativePluginEditor::NativePluginEditor(PhantomProcessor& p,
                                       juce::AudioProcessorValueTreeState& a)
    : juce::AudioProcessorEditor(&p), processor(p), apvts(a),
      rightPanel(a, p), leftPanel(a), topBar(p, a), presetBrowser(p, a),
      presetDropdown(p, a), modulationPanel(p, a), matrixView(p, a)
```

Replace with (add `settingsOverlay(a)` after `presetBrowser(p, a)`):

```cpp
NativePluginEditor::NativePluginEditor(PhantomProcessor& p,
                                       juce::AudioProcessorValueTreeState& a)
    : juce::AudioProcessorEditor(&p), processor(p), apvts(a),
      rightPanel(a, p), leftPanel(a), topBar(p, a), presetBrowser(p, a),
      settingsOverlay(a),
      presetDropdown(p, a), modulationPanel(p, a), matrixView(p, a)
```

Find the existing browser setup block in the constructor body:

```cpp
    addAndMakeVisible(presetBrowser);
    presetBrowser.setVisible(false);
    presetBrowser.toFront(false);  // ensure it's painted on top of other panels
```

Add right after:

```cpp
    addAndMakeVisible(settingsOverlay);
    settingsOverlay.setVisible(false);
    settingsOverlay.toFront(false);

    settingsOverlay.onDismiss = [this] {
        settingsOverlay.setVisible(false);
    };

    // TopBar's gear button → open the overlay with mutual exclusion against
    // the preset browser, dropdown, and matrix overlay.
    topBar.onSettingsRequested = [this, persistMatrixMode] {
        if (matrixView.isVisible())
        {
            matrixView.setVisible(false);
            resized();
            persistMatrixMode(false);
        }
        presetBrowser .setVisible(false);
        presetDropdown.setVisible(false);
        settingsOverlay.setBounds(getLocalBounds());
        settingsOverlay.setVisible(true);
        settingsOverlay.toFront(false);
    };
```

The lambda captures `persistMatrixMode` — that's the same matrix-persist helper used by the preset browser's `onBrowseRequested` (defined earlier in the constructor). The settings overlay just follows the same dismissal pattern.

- [ ] **Step 3: Hook into applyEngineFocus**

Find `NativePluginEditor::applyEngineFocus()`:

```cpp
void NativePluginEditor::applyEngineFocus()
{
    const auto focus = processor.getEngineFocus();
    const auto activePrefix = (focus.activeTab == kaigen::phantom::ActiveTab::B) ? "b_" : "a_";
    const auto mirrorPrefix = focus.linkOn
        ? juce::String((focus.activeTab == kaigen::phantom::ActiveTab::B) ? "a_" : "b_")
        : juce::String{};

    leftPanel .setEnginePrefix(activePrefix, mirrorPrefix);
    rightPanel.setEnginePrefix(activePrefix, mirrorPrefix);
    topBar.getModeToggle().setEnginePrefix(activePrefix, mirrorPrefix);
}
```

Replace with (one more call at the end):

```cpp
void NativePluginEditor::applyEngineFocus()
{
    const auto focus = processor.getEngineFocus();
    const auto activePrefix = (focus.activeTab == kaigen::phantom::ActiveTab::B) ? "b_" : "a_";
    const auto mirrorPrefix = focus.linkOn
        ? juce::String((focus.activeTab == kaigen::phantom::ActiveTab::B) ? "a_" : "b_")
        : juce::String{};

    leftPanel .setEnginePrefix(activePrefix, mirrorPrefix);
    rightPanel.setEnginePrefix(activePrefix, mirrorPrefix);
    topBar.getModeToggle().setEnginePrefix(activePrefix, mirrorPrefix);
    settingsOverlay.setEnginePrefix(activePrefix, mirrorPrefix);
}
```

- [ ] **Step 4: Size on `resized()`**

Find `NativePluginEditor::resized()`. Locate the line that sizes `presetBrowser`:

```cpp
    presetBrowser.setBounds(getLocalBounds());
    presetDropdown.setBounds(getLocalBounds());
    matrixView.setBounds(getLocalBounds());
```

Replace with:

```cpp
    presetBrowser  .setBounds(getLocalBounds());
    presetDropdown .setBounds(getLocalBounds());
    matrixView     .setBounds(getLocalBounds());
    settingsOverlay.setBounds(getLocalBounds());
```

- [ ] **Step 5: Build + test**

```
cd "C:/Documents/NEw project/Kaigen Phantom/build" && "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/amd64/MSBuild.exe" KaigenPhantom.sln //p:Configuration=Release //p:Platform=x64 //m //nologo //v:minimal 2>&1 | tail -6
```

Expected: 0 errors. Clicking the gear button opens the settings overlay; clicking outside the card or × button closes it; opening it dismisses preset browser / matrix if either was visible.

- [ ] **Step 6: Commit**

```
cd "C:/Documents/NEw project/Kaigen Phantom" && git add Source/UI/NativePluginEditor.h Source/UI/NativePluginEditor.cpp && git commit -m "feat(ui-native): wire SettingsOverlay to gear button with mutual exclusion"
```

---

### Task 6: Oscilloscope AUTO button

**Files:**
- Modify: `Source/UI/visualizers/Oscilloscope.h`
- Modify: `Source/UI/visualizers/Oscilloscope.cpp`

- [ ] **Step 1: Add the button + resized override declaration**

Open `Source/UI/visualizers/Oscilloscope.h`. Find:

```cpp
class Oscilloscope : public juce::Component, private juce::Timer
{
public:
    explicit Oscilloscope(PhantomProcessor& processor);
    ~Oscilloscope() override;

    void paint(juce::Graphics& g) override;
```

Replace with (adds `resized()` override and `autoButton` member):

```cpp
class Oscilloscope : public juce::Component, private juce::Timer
{
public:
    explicit Oscilloscope(PhantomProcessor& processor);
    ~Oscilloscope() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
```

Then find the private members section (after `bool autoScale { false };`). Add right after the `currentNormScale` member:

```cpp
    // Editor-local AUTO button — toggles autoScale. Matches WebView's
    // bottom-right corner pill. Not persisted to APVTS (matches WebView).
    juce::TextButton autoButton { "AUTO" };
```

- [ ] **Step 2: Construct + wire the button**

Open `Source/UI/visualizers/Oscilloscope.cpp`. Find the constructor:

```cpp
Oscilloscope::Oscilloscope(PhantomProcessor& p)
    : processor(p)
{
    setSize(800, 120);
    startTimerHz(20);   // dropped from 30 — still smooth, halves CPU
}
```

Replace with:

```cpp
Oscilloscope::Oscilloscope(PhantomProcessor& p)
    : processor(p)
{
    setSize(800, 120);
    startTimerHz(20);   // dropped from 30 — still smooth, halves CPU

    autoButton.setClickingTogglesState(true);
    autoButton.setToggleState(autoScale, juce::dontSendNotification);
    autoButton.setColour(juce::TextButton::buttonColourId,
                         juce::Colours::transparentBlack);
    autoButton.setColour(juce::TextButton::buttonOnColourId,
                         juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 0.10f));
    autoButton.setColour(juce::TextButton::textColourOffId,
                         juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 0.50f));
    autoButton.setColour(juce::TextButton::textColourOnId,
                         juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 0.90f));
    autoButton.setColour(juce::ComboBox::outlineColourId,
                         juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 0.15f));
    autoButton.onClick = [this] {
        autoScale = autoButton.getToggleState();
        repaint();
    };
    addAndMakeVisible(autoButton);
}
```

- [ ] **Step 3: Implement `resized()`**

In `Source/UI/visualizers/Oscilloscope.cpp`, find the end of the constructor `}` and add the `resized()` definition right after:

```cpp
void Oscilloscope::resized()
{
    constexpr int kBtnW   = 28;
    constexpr int kBtnH   = 12;
    constexpr int kBtnPad = 4;
    autoButton.setBounds(getWidth()  - kBtnW - kBtnPad,
                         getHeight() - kBtnH - kBtnPad,
                         kBtnW, kBtnH);
}
```

- [ ] **Step 4: Build + test**

```
cd "C:/Documents/NEw project/Kaigen Phantom/build" && "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/amd64/MSBuild.exe" KaigenPhantom.sln //p:Configuration=Release //p:Platform=x64 //m //nologo //v:minimal 2>&1 | tail -6
```

Expected: 0 errors. Oscilloscope shows a small AUTO pill at bottom-right; clicking toggles auto-scale behaviour.

- [ ] **Step 5: Commit**

```
cd "C:/Documents/NEw project/Kaigen Phantom" && git add Source/UI/visualizers/Oscilloscope.h Source/UI/visualizers/Oscilloscope.cpp && git commit -m "feat(ui-native): AUTO button overlay in the oscilloscope (toggles autoScale)"
```

---

### Task 7: Meter relocation + visualizer reflow

**Files:**
- Modify: `Source/UI/panels/RightPanel.cpp`

This task removes the meters from the Levels card, moves the oscilloscope below the spectrum, anchors the bottom row to the panel bottom, and lets the spectrum's height absorb Advanced collapse/expand.

- [ ] **Step 1: Remove the meters from the Levels card layout**

In `Source/UI/panels/RightPanel.cpp`, find the existing Levels-card meter placements (around lines 279–296):

```cpp
    // Left meter — clear of In's left shadow halo + clear of card edge.
    inMeter.setBounds(juce::jmax(levelsCardX + kMeterEdgeMargin,
                                  inGainX - kMeterKnobGap - kMeterW),
                       knobRowTop + (kMedium - 90) / 2, kMeterW, 90);
```

Delete those 3 lines for `inMeter.setBounds(...)`.

Find:

```cpp
    // Right meter — mirror of the left, outside Out's right shadow halo.
    outMeter.setBounds(juce::jmin(levelsCardRight - kMeterEdgeMargin - kMeterW,
                                   outGainX + kMedium + kMeterKnobGap),
                        knobRowTop + (kMedium - 90) / 2, kMeterW, 90);
```

Delete those 3 lines for `outMeter.setBounds(...)`.

Also delete the `kMeterEdgeMargin` and `kMeterKnobGap` and `kMeterW` constants if they're no longer referenced after removal. Search the file for those identifiers; if Levels was their only consumer, remove the declarations.

- [ ] **Step 2: Replace the bottom visualizer block with the anchored layout**

Find the bottom of `RightPanel::resized()`:

```cpp
    // Visualizers below Advanced row — excluded from inset cards (Task 5).
    area.removeFromTop(12);
    auto vizArea = area.reduced(12, 0);
    oscilloscope.setBounds(vizArea.removeFromTop(120));
    vizArea.removeFromTop(8);
    spectrum    .setBounds(vizArea.removeFromTop(280));
```

Replace with:

```cpp
    // Visualizers — bottom-anchored layout. Whole bottom row (oscilloscope
    // + meter column) is fixed in size and position; spectrum fills the
    // variable height between the Advanced row's bottom and the bottom
    // row's top. Collapsing Advanced only expands the spectrum upward.
    constexpr int kOscRowHeight     = 100;   // oscilloscope + meter column height
    constexpr int kVizSidePad       = 12;
    constexpr int kVizBottomPad     = 8;
    constexpr int kOscToSpecGap     = 6;
    constexpr int kMeterColWidth    = 56;
    constexpr int kMeterColPad      = 4;
    constexpr int kIndividualMeterW = 24;
    constexpr int kMeterHeight      = 88;     // fits within kOscRowHeight with 6 px margin
    constexpr int kSpectrumMinH     = 60;

    const int vizLeft   = kVizSidePad;
    const int vizRight  = getWidth() - kVizSidePad;
    const int vizBottom = getHeight() - kVizBottomPad;

    // Bottom row: oscilloscope on the left, meter column on the right.
    const int bottomRowTop = vizBottom - kOscRowHeight;
    const int meterColX    = vizRight - kMeterColWidth;
    const int oscRight     = meterColX - kMeterColPad;
    oscilloscope.setBounds(vizLeft, bottomRowTop,
                           oscRight - vizLeft, kOscRowHeight);

    // Meter column — two vertical IOMeters side-by-side, anchored bottom-right.
    constexpr int kMeterColGap = (kMeterColWidth - 2 * kIndividualMeterW) / 3;
    const int meterY      = bottomRowTop + (kOscRowHeight - kMeterHeight) / 2;
    const int inMeterXNew = meterColX + kMeterColGap;
    const int outMeterXNew = inMeterXNew + kIndividualMeterW + kMeterColGap;
    inMeter .setBounds(inMeterXNew,  meterY, kIndividualMeterW, kMeterHeight);
    outMeter.setBounds(outMeterXNew, meterY, kIndividualMeterW, kMeterHeight);

    // Spectrum — variable height. Bottom is fixed (just above bottom row);
    // top moves up or down based on advancedCardBottom.
    const int spectrumBottom = bottomRowTop - kOscToSpecGap;
    const int spectrumTop    = advancedCardBottom + kOscToSpecGap;
    const int spectrumHeight = juce::jmax(kSpectrumMinH, spectrumBottom - spectrumTop);
    spectrum.setBounds(vizLeft, spectrumBottom - spectrumHeight,
                       vizRight - vizLeft, spectrumHeight);
```

Note: this layout block reads `advancedCardBottom` which is already computed above it. It does NOT consume `area` (the existing top-down layout helper) — the bottom row is independent of `area`'s remaining-space tracking.

- [ ] **Step 3: Repaint the IN / OUT labels under the meters**

Find `RightPanel::paint(juce::Graphics& g)`. At the end of the function (after the existing `Theme::paintInsetCardWithNotch` and `drawSectionHeader` calls), add:

```cpp
    // Meter column labels — small etched "IN" / "OUT" under each meter.
    if (! inMeter.getBounds().isEmpty() && ! outMeter.getBounds().isEmpty())
    {
        const auto labelFont = juce::Font(juce::FontOptions("Space Grotesk", 8.0f, juce::Font::bold))
                                   .withExtraKerningFactor(0.25f);

        const auto inLabelBounds  = juce::Rectangle<int>(
            inMeter.getX() - 2, inMeter.getBottom() + 1,
            inMeter.getWidth() + 4, 9);
        const auto outLabelBounds = juce::Rectangle<int>(
            outMeter.getX() - 2, outMeter.getBottom() + 1,
            outMeter.getWidth() + 4, 9);

        Theme::drawEtchedText(g, "IN",  inLabelBounds.toFloat().toNearestInt(),
                               juce::Justification::centred,
                               labelFont, Theme::textOnLightLabel);
        Theme::drawEtchedText(g, "OUT", outLabelBounds.toFloat().toNearestInt(),
                               juce::Justification::centred,
                               labelFont, Theme::textOnLightLabel);
    }
```

- [ ] **Step 4: Remove the now-dead Levels content-width math (if it referenced the removed meter constants)**

Find the line in the Levels block:

```cpp
    const int levelsContentW = kMedium                                  // In
                              + (kSmallSide - kMedSmallOverlap)         // Trim
                              + (kSmallSide - kMedSmallOverlap)         // Reverb
                              + (kMedium    - kMedSmallOverlap);        // Out
```

This is still correct — the four-knob math is unchanged by meter removal. No edit needed here.

- [ ] **Step 5: Build**

```
cd "C:/Documents/NEw project/Kaigen Phantom/build" && "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/amd64/MSBuild.exe" KaigenPhantom.sln //p:Configuration=Release //p:Platform=x64 //m //nologo //v:minimal 2>&1 | tail -8
```

Expected: 0 errors. Levels card has no flanking meters. Oscilloscope sits in the bottom-left of the visualizer area; meter column in the bottom-right with IN/OUT labels. Spectrum sits above and resizes when Advanced expands/collapses; bottom row stays put.

- [ ] **Step 6: Commit**

```
cd "C:/Documents/NEw project/Kaigen Phantom" && git add Source/UI/panels/RightPanel.cpp && git commit -m "feat(ui-native): relocate meters to bottom-right, anchor visualizer bottom row, only spectrum resizes on Advanced toggle"
```

---

### Task 8: Final verification

- [ ] **Step 1: Confirm VST3 install timestamps**

```
stat -c '%y' "C:/Documents/NEw project/Kaigen Phantom/build/KaigenPhantom_artefacts/Release/VST3/Kaigen Phantom.vst3/Contents/x86_64-win/Kaigen Phantom.vst3"
stat -c '%y' "C:/Users/kaislate/AppData/Local/Programs/Common/VST3/Kaigen Phantom.vst3/Contents/x86_64-win/Kaigen Phantom.vst3"
```

Expected: both within the last few minutes.

- [ ] **Step 2: Branch commit log check**

```
cd "C:/Documents/NEw project/Kaigen Phantom" && git log --oneline integration/native-plus-reverb ^master | head -25
```

Expected: the seven feature commits from this plan plus the prior history.

- [ ] **Step 3: Manual user verification in Ableton**

- Restart Ableton / rescan plugins.
- Verify in the native UI:
  - Gear button opens the settings overlay. Card centred, dimmed backdrop.
  - Settings → Binaural: Off / Spread selector works; Width knob works.
  - Settings → Envelope Source: Input / Sidechain selector works.
  - Settings → MIDI Triggering: both toggles light when active.
  - × button and click-outside both dismiss.
  - Open the preset browser, then click gear — browser should dismiss and settings open.
- Advanced row: BIN toggle above the Width mini-knob; lit = Spread; reflects same `a_binaural_mode` value as the settings overlay's selector.
- Levels card: no flanking meters. Bottom-right of the visualizer area: two vertical meters with IN/OUT labels.
- Oscilloscope: AUTO button at bottom-right of its bounds; click toggles auto-scale.
- Toggle Advanced: spectrum grows / shrinks upward; oscilloscope and meter column do not move.

---

## Self-Review

**Spec coverage:** every spec section maps to a task.
- §1 SettingsOverlay component → Tasks 4 + 5
- §2 Top-bar gear button → Task 3 (wiring; button already exists)
- §3 Binaural quick toggle → Tasks 1 + 2 (widget + placement)
- §4 Meter relocation → Task 7
- §5 Oscilloscope AUTO button → Task 6
- §6 Collapse-Advanced layout reflow → Task 7
- Build verification → Task 8

**Placeholder scan:** none. All steps include exact paths and verbatim code. The two "if the list is glob-based, no change needed" notes in Tasks 1/4 step 3 are instructions to *check and follow the local pattern*, not placeholders.

**Type consistency:**
- `ChoiceToggle` constructor `(apvts, paramId, label, onChoiceIndex = 1)` consistent across Tasks 1 (declaration) and 2 (use).
- `SettingsOverlay` constructor `(apvts)` consistent across Tasks 4 (declaration) and 5 (use).
- `onSettingsRequested` callback declared in Task 3 (TopBar) used by Task 5 (NativePluginEditor).
- `IOMeter` member type (`inMeter` / `outMeter`) unchanged from existing — only its `setBounds` call moves.
- `autoScale` bool already exists on Oscilloscope; Task 6 adds the UI to drive it, doesn't redefine.

**Scope check:** one branch, one cohesive UI improvement pass. Suitable for a single execution session.
