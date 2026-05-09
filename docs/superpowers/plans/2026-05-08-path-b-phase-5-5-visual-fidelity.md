# Path B — Phase 5.5: Visual Fidelity Polish

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bring the native editor's visual fidelity to pixel-close parity with the WebView2 reference. The structure (panels, widgets, layouts) is correct from Phase 0-5; the rendering (colors, gradients, shadows, typography, knob aesthetic) is wrong because Phase 1 invented a dark-theme palette instead of porting the actual silver-neumorphic CSS.

**Architecture:** Three-layer fidelity:
1. **Theme rebuild** — `Theme.h` + a new `Theme.cpp` (or inline helpers in the header) holds the actual CSS palette + reusable gradient/shadow paint helpers (e.g., `Theme::paintSilverPanel(g, bounds)`, `Theme::paintInsetCard(g, bounds)`, `Theme::paintEtchedText(g, text, bounds, font)`).
2. **Custom LookAndFeel** — a single `PhantomLookAndFeel : juce::LookAndFeel_V4` that overrides the JUCE-stock paint methods for knobs (`drawRotarySlider`), buttons (`drawButtonBackground`, `drawButtonText`), text editors, and popup menus. Set on the editor at construction; widgets pick it up via JUCE's parent-chain LookAndFeel resolution.
3. **Per-panel `paint()` updates** — each panel's `paint()` method calls `Theme::paintSilverPanel` (main panels) or `Theme::paintDarkSurface` (slot row, matrix, visualizers) instead of the current flat `g.fillAll(Theme::panelBg)`.

**Reference:** the WebView2 CSS at `Source/WebUI/styles.css` and `Source/WebUI/matrix.css` is the source of truth. Every gradient stop, shadow, color, and font weight in this plan comes from a specific line in those files.

**Tech Stack:** JUCE 8.0.4, C++20, MSVC 17.14. Existing `Source/UI/` widgets/panels stay; only their `paint()` and the LookAndFeel get rewritten.

**Branch:** continue on `feature/pr3b-macro-editor`.

---

## What ships at the end

A native editor that, opened next to the WebView2 in Live, is visually indistinguishable at a glance:
- Silver-plastic main body with neumorphic raised/inset surfaces (`#CACCCE → #BBBDBF → #AEAFB1` radial gradient)
- Engraved-looking dark text on light surfaces (`rgba(0,0,0,0.22)` + white text-shadow trick)
- Silver neumorphic knobs with proper convex body, indicator arc, type-color highlight
- Dark slot row at bottom (`rgba(14,17,22,0.95)`) with bright type-color labels
- Pitch-black inset visualizer surfaces (oscilloscope, spectrum) with hard inner shadows
- Dark `#0a0c10` matrix overlay with cell gradients matching CSS
- Recipe wheel with silver convex mount and OLED black hub

---

## Task ordering rationale

Tasks 1-2 are foundational (no visible change until widgets pick up the new LookAndFeel). Tasks 3-7 are visible per-task: each one flips a panel to its real visual treatment. Task 8 is final polish and smoke.

Order optimizes for **visible progress per task** so you can review each commit in Live and steer:

1. Theme rebuild + helpers (foundation, no visible change yet)
2. PhantomLookAndFeel + integration (knobs + buttons all change at once)
3. Top bar + section label etched look
4. LeftPanel + RightPanel silver-panel surfaces + sub-panel insets
5. Visualizers — pitch-black inset surfaces with hard inner shadows
6. ModulationPanel slot row dark surface + type-color labels + slot dots
7. MatrixView dark surface + cell gradients + popover styling
8. Recipe wheel silver mount + OLED hub + final pass smoke

---

## Task 1: Theme rebuild from CSS

**Files:**
- Replace: `Source/UI/Theme.h`
- Create: `Source/UI/Theme.cpp` (paint helpers — kept inline if size stays manageable)
- Modify: `CMakeLists.txt` (add `Theme.cpp` to `target_sources` if created)

Replace the dark-theme `Theme.h` with the real CSS-derived tokens. Add paint helpers for the most common compound surfaces (silver panel, inset card, dark surface, etched text) so per-panel `paint()` methods stay short.

The new `Theme.h` defines tokens grouped by purpose; the corresponding `Theme.cpp` defines paint helpers. Existing usage sites (`Theme::macroTeal`, `Theme::lfoBlue`, etc.) keep their existing names where the meaning is unchanged.

- [ ] **Step 1: Write the new Theme.h**

```cpp
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
    inline const juce::Colour textOnLightBody   { 0xb3000000 };  // 70% black
    inline const juce::Colour textOnLightLabel  { 0x38000000 };  // 22% black — section labels
    inline const juce::Colour textOnLightActive { 0x9e000000 };  // 62% black — active toggle
    inline const juce::Colour textOnLightInactive { 0x47000000 }; // 28% black
    inline const juce::Colour textShadowEtch    { 0x80ffffff };  // 50% white — etched

    // ── Logos ────────────────────────────────────────────────────────────
    inline const juce::Colour logoPhantom       { 0xff656769 };
    inline const juce::Colour logoKaigen        { 0xff737577 };

    // ── Modulator identity colors (unchanged from existing tokens) ───────
    inline const juce::Colour macroTeal         { 0xff5DD3E0 };
    inline const juce::Colour lfoBlue           { 0xff4A90E2 };
    inline const juce::Colour randomPurple      { 0xff6B5DC9 };  // dot border
    inline const juce::Colour randomLabel       { 0xff9990E0 };  // name label
    inline const juce::Colour morphWhite        { 0xffEFEFF2 };

    // ── Accent blue (engine tabs, mode-btn active, build tag) ────────────
    inline const juce::Colour accentBlue        { 0xff4A8DD5 };
    inline const juce::Colour accentBlueBg      { 0x2e4A90E2 };  // 18% — tab active bg
    inline const juce::Colour accentBlueBorder  { 0x8c4A90E2 };  // 55% — tab border
    inline const juce::Colour accentBlueGlow    { 0x404A90E2 };  // 25% — tab shadow
    inline const juce::Colour accentBlueMode    { 0xff4A90E2 };  // mode-btn active label

    // ── Round-button (header, filter-link) ───────────────────────────────
    inline const juce::Colour btnActiveColor    { 0xe03773C3 };  // ~88% — active state
    inline const juce::Colour btnActiveGlow     { 0x474682D2 };  // ~28% — glow

    // ── Visualizer dark-surface text/buttons ─────────────────────────────
    inline const juce::Colour vizText           { 0xb3ffffff };
    inline const juce::Colour vizBtnBg          { 0x14ffffff };
    inline const juce::Colour vizBtnHoverBg     { 0x29ffffff };

    // ── Slot dot inner / matrix track / mask ─────────────────────────────
    inline const juce::Colour slotDotInner      { 0xff1f242c };
    inline const juce::Colour ringTrack         { 0x14ffffff };  // 8% white empty arc
    inline const juce::Colour ringMask          { 0xff0a0c10 };  // matches editorBg

    // ── Matrix-specific ──────────────────────────────────────────────────
    inline const juce::Colour mtxCellEmpty      { 0x9914181E };  // rgba(20,24,30,0.6)
    inline const juce::Colour mtxCellHover      { 0xb328303D };  // ~70%
    inline const juce::Colour mtxModStrip       { 0x8014181E };  // rgba(20,24,30,0.5)
    inline const juce::Colour mtxGridGap        { 0x66000000 };  // 40% black
    inline const juce::Colour mtxPopoverBg      { 0xff15181d };
    inline const juce::Colour mtxPopoverBorder  { 0x4d5DD3E0 };  // 30% teal
    inline const juce::Colour mtxDanger         { 0xd9e86464 };  // 85%

    // ── Misc ─────────────────────────────────────────────────────────────
    inline const juce::Colour clipRed           { 0xffe85050 };

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
```

- [ ] **Step 2: Write Theme.cpp with the paint helpers**

```cpp
// Source/UI/Theme.cpp
#include "Theme.h"

namespace kaigen::phantom::Theme
{
    void paintSilverPanel(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        const auto fb = bounds.toFloat();

        // Radial gradient body — ellipse at 28%/18%, A → B → C
        const juce::Point<float> centre { fb.getX() + fb.getWidth() * 0.28f,
                                           fb.getY() + fb.getHeight() * 0.18f };
        const float r = std::max(fb.getWidth(), fb.getHeight()) * 1.1f;
        juce::ColourGradient grad(panelRadialA, centre,
                                   panelRadialC, { centre.x + r, centre.y + r }, true);
        grad.addColour(0.48, panelRadialB);
        g.setGradientFill(grad);
        g.fillRect(fb);

        // Inset highlights (top-left bright)
        g.setColour(juce::Colour(0x9eFFFFFF));  // 62% white
        for (int i = 0; i < 6; ++i)  // top gloss — 6px tall, 16px blur fake via overlay
            g.fillRect(fb.withHeight(1.0f).translated(0, (float) i).withTrimmedLeft(0));

        // (Real implementation will use a Drawable or layered fillRect with falling alpha.
        //  Keep this scaffold minimal in Step 2; refine in Step 3 once visual is on screen.)
    }

    void paintInsetCard(juce::Graphics& g, juce::Rectangle<int> bounds, float cornerRadius)
    {
        const auto fb = bounds.toFloat();
        // 2% black wash
        g.setColour(insetSurfaceTint);
        g.fillRoundedRectangle(fb, cornerRadius);

        // Top-left dark inset shadow + bottom-right silver inset (suggest depth).
        // Implemented as a 3-layer overlay; tune in step 3.
    }

    void paintHeaderStrip(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        const auto fb = bounds.toFloat();
        juce::ColourGradient grad(headerHi, fb.getX(), fb.getY(),
                                   headerLo, fb.getX(), fb.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRect(fb);

        // Bottom hairline.
        g.setColour(headerSeparator);
        g.drawHorizontalLine(bounds.getBottom() - 1, fb.getX(), fb.getRight());
    }

    void paintVisualizerInset(juce::Graphics& g, juce::Rectangle<int> bounds, float cornerRadius)
    {
        const auto fb = bounds.toFloat();
        g.setColour(vizSurface);
        g.fillRoundedRectangle(fb, cornerRadius);
        // Hard inset shadow approximated with stacked thin rectangles.
        // Refined in step 3.
    }

    void drawEtchedText(juce::Graphics& g, juce::String text, juce::Rectangle<int> bounds,
                        juce::Justification just, juce::Font font, juce::Colour textColour)
    {
        g.setFont(font);
        // Shadow (1 px below)
        g.setColour(textShadowEtch);
        g.drawText(text, bounds.translated(0, 1), just, false);
        // Foreground
        g.setColour(textColour);
        g.drawText(text, bounds, just, false);
    }
}
```

NOTE: Step 2 is intentionally a scaffold — the silver-panel/inset-card/visualizer-inset shadow stacks are not pixel-precise yet. Step 3 of THIS task is to iterate the helpers against the WebView2 visual once we can see them on screen (via a temporary test page). However, since Tasks 3-7 will dogfood these helpers, **defer iteration to Task 3** when there's a real panel to compare against. Step 2's job is just to make the helpers compile and produce a recognizable approximation.

- [ ] **Step 3: Add Theme.cpp to CMakeLists.txt**

In `target_sources(KaigenPhantom PRIVATE`, add `Source/UI/Theme.cpp` near the existing `Source/UI/NativePluginEditor.cpp` line.

- [ ] **Step 4: Build**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expected: builds clean. The compiler will flag any token name renames I missed — a number of widgets currently use `Theme::panelBg`, `Theme::deepBg`, `Theme::textPrimary`, `Theme::textSecondary`, `Theme::textDim`, `Theme::activeGlow`, `Theme::steelBlue`. These tokens DO NOT exist in the new Theme.h. I need to map them:

| Old token | New token |
|-----------|-----------|
| `Theme::panelBg` | (depends on context — see below) |
| `Theme::deepBg` | `Theme::editorBg` |
| `Theme::textPrimary` | (silver context: `Theme::textOnLightBody`; dark context: `Theme::vizText`) |
| `Theme::textSecondary` | (silver: `Theme::textOnLightLabel`; dark: appropriate alpha white) |
| `Theme::textDim` | (case by case) |
| `Theme::activeGlow` | `Theme::accentBlueGlow` (or `Theme::btnActiveGlow`) |
| `Theme::steelBlue` | `Theme::accentBlue` or `Theme::accentBlueMode` (case by case) |
| `Theme::panelBorder` | `Theme::headerSeparator` (or context-specific) |

The `panelBg` migration is non-trivial because the SAME widget previously assumed dark + now needs to know whether it's in a silver panel or a dark surface. **Don't try to do all migrations in Task 1.** Add backwards-compat aliases in `Theme.h` so the build still compiles:

```cpp
// Temporary aliases — Tasks 3-7 will update each call site to the right new token,
// then these aliases get removed.
inline const juce::Colour& panelBg     = panelRadialB;
inline const juce::Colour& deepBg      = editorBg;
inline const juce::Colour& textPrimary = textOnLightBody;
inline const juce::Colour& textSecondary = textOnLightLabel;
inline const juce::Colour& textDim     = textOnLightInactive;
inline const juce::Colour& activeGlow  = accentBlueGlow;
inline const juce::Colour& steelBlue   = accentBlue;
inline const juce::Colour& panelBorder = headerSeparator;
```

This makes the existing widgets render with vaguely-silver-looking colors out of the box. Tasks 3-7 update each site to the precise new token and remove the aliases.

- [ ] **Step 5: Manual smoke**

Build and reload in Live. Expected: editor looks DIFFERENT from before — main panel area is light-silver instead of dark. Knobs, buttons, etc. will look slightly off because they were tuned for dark. This is fine; Task 2 fixes the knobs.

- [ ] **Step 6: Commit**

```bash
git add Source/UI/Theme.h Source/UI/Theme.cpp CMakeLists.txt
git commit -m "feat(path-b): rebuild Theme.h from WebView2 CSS palette + paint helpers"
```

---

## Task 2: PhantomLookAndFeel — silver neumorphic knobs + buttons

**Files:**
- Create: `Source/UI/PhantomLookAndFeel.h`
- Create: `Source/UI/PhantomLookAndFeel.cpp`
- Modify: `Source/UI/NativePluginEditor.h` and `.cpp` (instantiate + apply)
- Modify: `CMakeLists.txt`

A custom `juce::LookAndFeel_V4` subclass that overrides:
- `drawRotarySlider` — silver convex disc + inset rim + indicator arc + tick mark
- `drawButtonBackground` and `drawButtonText` — neumorphic toggle pill / round button / segmented control depending on button properties (use ColourId + button properties)
- `drawTextEditorOutline` — silver-edged text editor for the macro name editor
- `drawPopupMenuBackground` and `drawPopupMenuItem` — dark popup with teal accent (matrix popover)
- `drawComboBox` — silver-edged combo (used internally by `ToggleGroup`)

Set the LookAndFeel on `NativePluginEditor` at construction; child widgets inherit it via JUCE's parent-chain resolution.

- [ ] **Step 1: Create Source/UI/PhantomLookAndFeel.h**

```cpp
// Source/UI/PhantomLookAndFeel.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace kaigen::phantom
{

/** Custom LookAndFeel matching the WebView2 CSS:
 *  - Rotary sliders → silver neumorphic convex knob with inset rim + indicator arc.
 *  - Buttons → context-aware: round neumorphic (header-style) when small + round,
 *    segmented pill (toggle group) when wide, etc. Caller can opt into a specific
 *    style by setting a juce::Colour::black-coded ColourId before paint, or by
 *    setting a property via setProperties() — see the .cpp for the conventions.
 *  - Text editors → silver edge + dark text (macro name input).
 *  - Popup menu → dark `#15181d` body, teal-accent border, hover rows. */
class PhantomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PhantomLookAndFeel();
    ~PhantomLookAndFeel() override = default;

    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawButtonBackground(juce::Graphics&, juce::Button&,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;

    void drawTextEditorOutline(juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void fillTextEditorBackground(juce::Graphics&, int width, int height, juce::TextEditor&) override;

    void drawPopupMenuBackground(juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool hasSubMenu, const juce::String& text,
                           const juce::String& shortcutKeyText,
                           const juce::Drawable* icon, const juce::Colour* textColour) override;

private:
    /** Paint the neumorphic raised body of a button (silver gradient + edge highlight). */
    void paintNeumorphicRaised(juce::Graphics&, juce::Rectangle<float> bounds, float cornerRadius);

    /** Paint a segmented-toggle inactive/active state. */
    void paintToggleSegment(juce::Graphics&, juce::Rectangle<float> bounds,
                             bool active, float cornerRadius);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomLookAndFeel)
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Create Source/UI/PhantomLookAndFeel.cpp**

This implementation is the longest single piece of Phase 5.5. The full `drawRotarySlider` paints:

1. Convex disc body — radial gradient `ellipse at 35%/30%, rgba(255,255,255,0.24)→rgba(255,255,255,0.12)→rgba(0,0,0,0.02)→rgba(0,0,0,0.07)`. Use `juce::ColourGradient::isRadial = true`.
2. Outer offset shadows — `-4px -4px 10px rgba(255,255,255,0.50)` (top-left highlight) and `4px 4px 14px rgba(0,0,0,0.22)` (bottom-right shadow). JUCE doesn't have built-in box-shadow; emulate with two extra layers (or use a `juce::DropShadow` with offset).
3. Inset rim — a thin dark ring at the edge (`0 0 0 4px rgba(0,0,0,0.07)`).
4. Indicator arc — colored stroke from `rotaryStartAngle` to `rotaryEndAngle * sliderPos`. Color = type-color (look up via slider property or default to `Theme::accentBlue`).
5. Tick mark — a short white line from the indicator arc inward.

Because this is a non-trivial custom paint, the implementer will need to iterate against the WebView2 in a side-by-side window. Plan `drawRotarySlider` as a focused subagent task with screenshot-driven iteration.

(Code skeleton goes here — keep it short; the implementer will fill in the exact gradients/shadows during execution.)

```cpp
// Source/UI/PhantomLookAndFeel.cpp — skeleton
#include "PhantomLookAndFeel.h"
#include "Theme.h"

namespace kaigen::phantom
{

PhantomLookAndFeel::PhantomLookAndFeel()
{
    // Set default ColourId palette so widgets that don't get custom-painted
    // still pick up reasonable colors.
    setColour(juce::Slider::rotarySliderFillColourId,    Theme::accentBlue);
    setColour(juce::Slider::rotarySliderOutlineColourId, Theme::ringTrack);
    setColour(juce::Label::textColourId,                 Theme::textOnLightBody);
    setColour(juce::TextButton::buttonColourId,          Theme::panelRadialB);
    setColour(juce::TextButton::buttonOnColourId,        Theme::accentBlueBg);
    setColour(juce::TextButton::textColourOffId,         Theme::textOnLightInactive);
    setColour(juce::TextButton::textColourOnId,          Theme::textOnLightActive);
    setColour(juce::TextEditor::backgroundColourId,      Theme::mtxPopoverBg);
    setColour(juce::TextEditor::textColourId,            Theme::macroTeal);
    setColour(juce::TextEditor::outlineColourId,         Theme::macroTeal);
    setColour(juce::PopupMenu::backgroundColourId,       Theme::mtxPopoverBg);
    setColour(juce::PopupMenu::textColourId,             Theme::vizText);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0x2e5DD3E0));
    setColour(juce::PopupMenu::highlightedTextColourId,  Theme::macroTeal);
}

void PhantomLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                            float sliderPos,
                                            float rotaryStartAngle, float rotaryEndAngle,
                                            juce::Slider& slider)
{
    // 1. Compute centre + radius.
    // 2. Paint convex disc body (radial gradient at 35%/30%).
    // 3. Paint outer offset shadows (2 layers).
    // 4. Paint inset rim ring.
    // 5. Paint indicator arc (rotaryStart → rotaryStart + (rotaryEnd-rotaryStart)*sliderPos).
    // 6. Paint white tick mark.
    // (TBD by implementer with iteration against WebView2 reference.)
    juce::ignoreUnused(x, y, w, h, sliderPos, rotaryStartAngle, rotaryEndAngle, slider);
}

// ... other overrides as skeletons ...

} // namespace kaigen::phantom
```

NOTE: this scaffold is intentionally minimal. The execution-time agent gets ONE iteration loop per override:
- Implement override
- Build + screenshot in Live
- Compare to WebView2
- Adjust gradient stops, shadow blurs, alphas
- Commit when "looks right"

Budget: ~30-60 min of agent work + manual review per override.

- [ ] **Step 3: Update NativePluginEditor.h to own a PhantomLookAndFeel**

```cpp
#include "PhantomLookAndFeel.h"
// ...
private:
    PhantomLookAndFeel lookAndFeel;
```

- [ ] **Step 4: Update NativePluginEditor.cpp constructor**

```cpp
setLookAndFeel(&lookAndFeel);
```

And destructor:

```cpp
setLookAndFeel(nullptr);
```

- [ ] **Step 5: Build, smoke, iterate**

Open in Live. Knobs and buttons should pick up the new look automatically (JUCE walks the parent chain). Expect first-pass rendering to be approximate; iterate against the WebView2 side-by-side until pixels look right. Each iteration = one tweak + one rebuild.

- [ ] **Step 6: Commit**

```bash
git add Source/UI/PhantomLookAndFeel.h Source/UI/PhantomLookAndFeel.cpp Source/UI/NativePluginEditor.h Source/UI/NativePluginEditor.cpp CMakeLists.txt
git commit -m "feat(path-b): PhantomLookAndFeel — silver neumorphic knobs + buttons"
```

---

## Task 3: Top bar + section labels — etched look

**Files:**
- Modify: `Source/UI/panels/TopBar.cpp`
- Modify: `Source/UI/widgets/PresetSelector.cpp` (logo + arrow icons + preset display)
- Modify: any existing section-label paint sites in `LeftPanel.cpp` / `RightPanel.cpp`

The top bar uses the linear `headerHi → headerLo` gradient + a bottom hairline. PHANTOM and KAIGEN logos use the etched-text trick (white shadow below, slate-grey foreground). Section labels (`GHOST`, `RECIPE`, `HARMONIC ENGINE`, `STEREO`, `LEVELS`, `FILTER`, etc.) use the same etched style at 22% black + 50% white shadow.

- [ ] **Step 1: Update TopBar::paint() to call Theme::paintHeaderStrip**

Replace the current dark-fill paint with `Theme::paintHeaderStrip(g, getLocalBounds())`.

- [ ] **Step 2: Update PresetSelector::paint() to use etched logo + dark-on-light text**

Render the "PHANTOM" and "KAIGEN" logos using `Theme::drawEtchedText` with the right font weights (300 for PHANTOM at 22 px / letter-spacing 10, 400 for KAIGEN at 13 px / letter-spacing 6). Browse/Save buttons inherit the new LookAndFeel from Task 2 — no per-button paint here unless they need a specific style hint.

- [ ] **Step 3: Update all section labels to use Theme::drawEtchedText**

Audit `LeftPanel.cpp`, `RightPanel.cpp`, `ModulationPanel.cpp` for `g.setFont(...)` + `g.drawText(...)` patterns that render section headers (uppercase labels like "GHOST", "FILTER", "RECIPE", "HARMONIC ENGINE", "STEREO", "LEVELS", "ADVANCED"). Replace each with `Theme::drawEtchedText(g, "GHOST", labelBounds, juce::Justification::left, font, Theme::textOnLightLabel)`.

The font for section labels is `Space Grotesk` 10px, weight 600, letter-spacing 2.5px (use `juce::Font::FontStyleFlags` + `setExtraKerningFactor` if needed for tracking).

- [ ] **Step 4: Build, smoke, iterate**

Side-by-side with WebView2: do the logos look engraved? Do the section labels look carved into the silver?

- [ ] **Step 5: Commit**

```bash
git add Source/UI/panels/TopBar.cpp Source/UI/widgets/PresetSelector.cpp Source/UI/panels/LeftPanel.cpp Source/UI/panels/RightPanel.cpp
git commit -m "feat(path-b): top bar + section labels — etched look on silver surface"
```

---

## Task 4: LeftPanel + RightPanel silver surfaces + sub-panel insets

**Files:**
- Modify: `Source/UI/panels/LeftPanel.cpp`
- Modify: `Source/UI/panels/RightPanel.cpp`

Each main panel currently does `g.fillAll(Theme::panelBg)` (formerly dark, now via the alias falls back to `panelRadialB`). Replace with `Theme::paintSilverPanel(g, getLocalBounds())`. Inside each panel, the sub-panel cards (the "candy-inner" containers — Ghost section, Filter section, Harmonic Engine section, Stereo section, Levels section, Advanced collapse) use `Theme::paintInsetCard` with `cornerRadius = 14`.

- [ ] **Step 1: LeftPanel::paint() — silver surface + inset cards for Ghost + Filter**

```cpp
void LeftPanel::paint(juce::Graphics& g)
{
    Theme::paintSilverPanel(g, getLocalBounds());

    // Ghost sub-panel inset card.
    Theme::paintInsetCard(g, ghostBounds, 14.0f);

    // Filter sub-panel inset card.
    Theme::paintInsetCard(g, filterBounds, 14.0f);

    // Section labels via Theme::drawEtchedText (Task 3).
    Theme::drawEtchedText(g, "GHOST", ghostLabelBounds, juce::Justification::left,
                           Theme::sectionLabelFont(), Theme::textOnLightLabel);
    // ... other labels ...
}
```

(`ghostBounds`, `filterBounds`, etc. are computed in `resized()` and stored as members.)

- [ ] **Step 2: RightPanel::paint() — same pattern for Harmonic Engine + Stereo + Levels + Advanced**

Same approach. Visualizer regions (Oscilloscope + Spectrum) keep their pitch-black inset (Task 5). Stop drawing the silver panel inside the visualizer area — only draw it for the control sections.

- [ ] **Step 3: Build, smoke, iterate**

The light-silver surface should now visibly host the silver knobs from Task 2. Sub-panels should look like neumorphic dishes.

- [ ] **Step 4: Commit**

```bash
git add Source/UI/panels/LeftPanel.cpp Source/UI/panels/RightPanel.cpp
git commit -m "feat(path-b): LeftPanel + RightPanel silver surfaces with neumorphic insets"
```

---

## Task 5: Visualizers — pitch-black inset surfaces

**Files:**
- Modify: `Source/UI/visualizers/Oscilloscope.cpp`
- Modify: `Source/UI/visualizers/Spectrum.cpp`

Both visualizer panels currently fill with a dark color. Replace with `Theme::paintVisualizerInset(g, getLocalBounds(), 6.0f)` to get the pitch-black `#000` body + hard inner shadow + outer top-left highlight + outer bottom-right shadow.

The waveform/spectrum line color should be `rgba(74,141,213,0.85)` (the WebView2 used the accent blue for the trace).

- [ ] **Step 1: Oscilloscope::paint() — paintVisualizerInset background**
- [ ] **Step 2: Spectrum::paint() — paintVisualizerInset background**
- [ ] **Step 3: Verify zoom buttons + spectrum mode toggle inherit LookAndFeel** so they pick up the dark-surface accent (rather than silver). The LookAndFeel may need a button-style hint that's set per-button (e.g., set a property `phantom-style: viz-button` on the buttons and check it in `drawButtonBackground`).
- [ ] **Step 4: Build, smoke, iterate**
- [ ] **Step 5: Commit**

```bash
git add Source/UI/visualizers/Oscilloscope.cpp Source/UI/visualizers/Spectrum.cpp
git commit -m "feat(path-b): visualizer pitch-black inset surfaces"
```

---

## Task 6: ModulationPanel slot row dark surface + slot dot widget

**Files:**
- Modify: `Source/UI/panels/ModulationPanel.cpp`
- Modify: `Source/UI/widgets/ModSlot.h` and `.cpp` (rebuild the slot's visual)

The modulation panel's surface is `rgba(14,17,22,0.95)` with a 1 px white-5% top border. NOT silver. Each slot has a 36 × 36 px circle "slot dot" with a type-color radial gradient + 2 px type-color border + a soft glow shadow when active. The macro/morph slots also have a value ring around the dot (conic gradient style).

This task replaces the current `PhantomKnob`-based slot rendering with the slot-dot pattern from the CSS. The dot is a small custom Component drawn manually; the value ring sits behind/around it.

- [ ] **Step 1: ModulationPanel::paint() — dark surface + top border**

```cpp
void ModulationPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::modPanelBg);
    g.setColour(Theme::modPanelBorder);
    g.drawHorizontalLine(0, 0.0f, (float) getWidth());
}
```

- [ ] **Step 2: ModSlot rebuild — slot-dot widget**

Replace the current `PhantomKnob` member with a custom paint that renders the 36 × 36 dot + value ring + name label below. The dot's center color/border depends on `Type`:
- Macro: `radial-gradient(circle, #5DD3E0 30%, #1f242c 70%)` + 2px `#5DD3E0` border + `0 0 6px rgba(93,211,224,0.5)` glow
- LFO: same pattern with `#4A90E2`
- Random: same with `#6B5DC9`
- Morph: `radial-gradient(circle at 35% 35%, #fafaff 10%, #b8b8c4 50%, #4a4d54 80%)` + 2px `#EFEFF2` border

Active state (when this slot is "selected" — e.g., currently being dragged): scale to 1.05 + brighter glow (`0 0 14px rgba(<color>,0.7)`).

The value ring (the conic-gradient track around the dot) is for macro/morph only and shows the current value as a partial arc. CSS uses `conic-gradient(currentColor calc(var(--v,0)*1%), rgba(255,255,255,0.08) 0)` — translate to JUCE: draw a stroked arc from -45° to (-45° + 270°*value) with the type color, plus a faint 8% white fallback arc for the rest.

For drag interaction: the slot dot is the value control. Click+drag vertically to change the macro value. Internally bind to the macroN APVTS param via a `juce::SliderParameterAttachment` (use a hidden Slider as the model, like the old PhantomKnob did).

- [ ] **Step 3: Build, smoke, iterate**

Dragging a macro slot dot should change `macroN`. Live cell pulse in matrix view should still respond.

- [ ] **Step 4: Commit**

```bash
git add Source/UI/panels/ModulationPanel.cpp Source/UI/widgets/ModSlot.h Source/UI/widgets/ModSlot.cpp
git commit -m "feat(path-b): slot row dark surface + slot-dot widget per CSS"
```

---

## Task 7: MatrixView — dark surface + cell gradients + popover styling

**Files:**
- Modify: `Source/UI/panels/MatrixView.cpp`
- Modify: `Source/UI/widgets/MatrixCell.cpp`
- Modify: `Source/UI/widgets/MatrixModRow.cpp`

The matrix overlay is `#0a0c10` (matches editor body). The card is full-width with no border-radius or border (replaces the current rounded card). Modulator strip rows have `padding: 6px 10px`, `background: rgba(20,24,30,0.5)`, `border-left: 2px solid <type-color>`, `border-radius: 3px 0 0 3px`.

Cells:
- Empty: `background: rgba(20,24,30,0.6)`, `color: rgba(255,255,255,0.25)`
- Routed: `linear-gradient(90deg, transparent, <type-color> calc(0.5 * |depth|))` + a base `rgba(<type-color>, 0.18 + |depth|*0.20)` + `inset 0 0 0 1px rgba(<type-color>, 0.55)` ring
- Negative polarity flips the gradient direction to `270deg`
- Modulating: 700 ms breathing pulse — already implemented in Phase 5 (just needs color tuning)

Popover: `background: #15181d`, `border: 1px solid rgba(93,211,224,0.3)`, `border-radius: 4px`, `box-shadow: 0 4px 16px rgba(0,0,0,0.6)`.

- [ ] **Step 1: MatrixView::paint() — flat `#0a0c10` background, no rounded card**

Drop the current card/border/title-bar paint chrome. Replace with a flat fill + a thin engine-divider line + per-engine "ENGINE A"/"ENGINE B" colored labels.

- [ ] **Step 2: MatrixModRow::paint() — strip background + 2px left type-color accent**

```cpp
g.setColour(Theme::mtxModStrip);
g.fillRoundedRectangle(bounds.toFloat(), 3.0f);  // only top-left + bottom-left rounded
g.setColour(typeColor);
g.fillRect(bounds.getX(), bounds.getY(), 2, bounds.getHeight());  // 2px left accent
```

- [ ] **Step 3: MatrixCell::paint() — empty/routed gradients matching CSS exactly**

Update the existing paint to use the precise CSS-derived alphas:
- Empty: `Theme::mtxCellEmpty` fill, text in 25% white
- Routed positive: gradient from transparent → `accent.withAlpha(0.50f * abs(depth))` (left to right) + base fill `accent.withAlpha(0.18f + 0.20f * abs(depth))` + 1px inset `accent.withAlpha(0.55f)` ring
- Routed negative: gradient flipped (right to left)

Live pulse already implemented; just confirm the type-colors match the CSS.

- [ ] **Step 4: Popover via PhantomLookAndFeel** — Task 2's `drawPopupMenuBackground` and `drawPopupMenuItem` cover this. Verify by right-clicking a cell.

- [ ] **Step 5: Build, smoke, iterate**

- [ ] **Step 6: Commit**

```bash
git add Source/UI/panels/MatrixView.cpp Source/UI/widgets/MatrixCell.cpp Source/UI/widgets/MatrixModRow.cpp
git commit -m "feat(path-b): matrix dark surface + cell gradients matching CSS"
```

---

## Task 8: Recipe wheel + final pass

**Files:**
- Modify: `Source/UI/widgets/RecipeWheel.cpp`
- Modify: any final spacing/sizing fixes discovered during smoke

The recipe wheel has the most distinctive piece in the WebView2: the silver convex `wheel-mount` (280 × 280 px circle with the radial-gradient body + neumorphic outer shadows + dark thin ring) and the OLED hub at the center (112 × 112 px black circle with silver bezel rings + glowing white center text).

The harmonic spoke labels are 8 px monospace at 152 px radius from center. The 7 spokes connect from the hub edge outward to dots positioned by harmonic amplitude (existing logic).

- [ ] **Step 1: RecipeWheel::paint() — silver mount disc + dark inset container**

```cpp
// Outer convex disc (radial gradient at 35%/30%).
juce::ColourGradient bodyGrad(juce::Colour(0x3dffffff), centre.x - radius * 0.30f, centre.y - radius * 0.40f,
                                juce::Colour(0x12000000), centre.x + radius, centre.y + radius, true);
bodyGrad.addColour(0.20, juce::Colour(0x1eFFFFFF));
bodyGrad.addColour(0.55, juce::Colour(0x05000000));
g.setGradientFill(bodyGrad);
g.fillEllipse(...);

// Outer offset shadows (top-left highlight + bottom-right shadow).
// Inset rim (4 px dark ring).
```

- [ ] **Step 2: OLED center hub**

A 112 × 112 px black circle with concentric ring borders:
- Inner gap: 2 px `rgba(0,0,0,0.55)`
- Silver bezel: 1.5 px `rgba(180,182,186,0.7)`
- Outer gap: 1.5 px `rgba(0,0,0,0.18)`
- Inset glow: hard black inner shadow

OLED text: top label "FREQ" / "PRESET" in monospace 8 px white-30%, freq readout "440.0" in monospace 18 px white with multi-layer glow (white + 2px + 8px + 20px + 40px).

- [ ] **Step 3: Harmonic spoke labels**

Position H1, H2, ... H7 at 152 px from center, oriented with the spokes. Use 8 px monospace, weight 700, color black-38% with white text-shadow.

- [ ] **Step 4: Final pass — visual polish audit**

Side-by-side with WebView2:
- Spacing matches (panel margins, sub-panel gaps)
- Font sizes and letter-spacings exact
- Active state colors match (selected toggle, active button, modulating cell)
- Cursor states match (drag cursor on knobs, pointer on buttons)
- Popups + alert windows use the dark popover style

- [ ] **Step 5: Manual smoke in Live**

Walk through every feature once with WebView2 reference open in a separate Live track. Take side-by-side screenshots. Note any remaining visual gaps.

- [ ] **Step 6: Commit**

```bash
git add Source/UI/widgets/RecipeWheel.cpp
git commit -m "feat(path-b): recipe wheel silver mount + OLED hub + final polish"
```

---

## Self-review notes

### Spec coverage

Phase 5.5 produces a native editor visually indistinguishable at a glance from the WebView2:
- Silver-plastic main body ✓ (Tasks 1, 4)
- Neumorphic raised/inset surfaces ✓ (Task 1 helpers, Tasks 4, 6)
- Engraved-looking dark text ✓ (Tasks 1, 3)
- Silver neumorphic knobs ✓ (Task 2)
- Dark slot row with type-color labels ✓ (Task 6)
- Pitch-black visualizer insets ✓ (Task 5)
- Dark matrix overlay with cell gradients ✓ (Task 7)
- Silver recipe wheel + OLED hub ✓ (Task 8)
- Etched section labels ✓ (Task 3)

### Risk areas

- **Task 2 (LookAndFeel)** is the biggest visual swing. The `drawRotarySlider` override needs side-by-side iteration; expect 3-5 commits within the task as gradients are tuned.
- **Task 6 (slot-dot)** discards the existing `PhantomKnob` integration in slots and rebuilds from scratch. The macroN APVTS attachment must survive — verify by dragging post-rebuild and confirming the value persists.
- **Task 1 backwards-compat aliases** (`Theme::panelBg`, `Theme::deepBg`, etc.) are temporary. Tasks 3-7 update each call site to the precise new token; task 8 removes the aliases. Failure to remove them would leave the codebase with two ways to spell the same color.

### Placeholder scan

Several `// TBD by implementer with iteration against WebView2 reference` markers in Task 2 and Task 6 are intentional — those overrides need visual iteration that can't be specified in advance from CSS alone. The implementer is expected to:
1. Implement first-pass scaffold from the CSS values.
2. Build + screenshot + compare side-by-side.
3. Adjust gradient stops, blur radii, alphas.
4. Repeat 2-3 until visually correct.

### Effort estimate

| Task | Implementation | Iteration | Total |
|------|----------------|-----------|-------|
| 1 — Theme rebuild | 30 min | 0 (no visual yet) | 30 min |
| 2 — LookAndFeel | 60 min | 60-90 min | 2-2.5 h |
| 3 — Top bar + labels | 30 min | 30 min | 1 h |
| 4 — Panel surfaces | 30 min | 30 min | 1 h |
| 5 — Visualizers | 20 min | 20 min | 40 min |
| 6 — Slot row + dot | 60 min | 60 min | 2 h |
| 7 — Matrix | 45 min | 45 min | 1.5 h |
| 8 — Recipe wheel + final | 60 min | 60 min | 2 h |
| | | | **~10-11 h total** |

Spread across multiple sessions. Each task ends with a commit + manually-testable visual state.

### What doesn't ship in Phase 5.5

Animations and micro-interactions are deferred:
- Slot row collapse/expand animation (currently instant)
- Matrix overlay slide/fade in/out animation (currently instant)
- Advanced panel reveal animation
- Cell glow `is-modulating` state — already implemented in Phase 5
- Logo shift+click toggle to WebView2 — N/A (Phase 6 deletes WebView2)

Dirty-state asterisk (preset modified-from-saved indicator) is also deferred.

After Phase 5.5 lands, Phase 6 (WebView2 cutover) becomes appropriate.
