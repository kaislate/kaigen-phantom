# Native Knob Visual Recipe

> Anti-thrash doc. The WebView2 knob visual went through ~25 commits and 6 distinct rendering pipelines before landing. The native port repeated this thrash on 2026-05-09 (5+ commits chasing cyan halos and lost depth) before the user said *"there should be a well-documented procedure on how this was done — don't repeat that."* This doc captures the landed recipe + the dead ends so future iterations don't relitigate them.

## TL;DR — the landed recipe

Both `PhantomKnob` and `PhantomMiniKnob` paint the body as:

1. **`juce::DropShadow` × 4** for the box-shadow (TL bright + TL secondary + BR primary + BR secondary). Black blurs render cleanly; white blurs may show faint cyan tint on Windows software rendering — that is **acceptable per the WebView2 reference and explicit user direction**.
2. **`juce::ColourGradient` radial body** with **transparent RGBA stops**. The outer edge is `rgba(0,0,0,0.07)` — only 7% opacity, so the bezel/panel **shows through 93%**. There is no hard knob boundary because there is no opaque pixel where the boundary would be.

This combination produces the volcano-slope look. The DropShadows give the offset depth; the transparent gradient blends the knob seamlessly into the panel.

## The breakthrough

From `docs/thoughts-2026-04-17.md` (line 7):

> "The breakthrough came from the user's own observation that the recipe wheel's `.wheel-mount` didn't have this problem — and reading its CSS revealed the trick: **transparent rgba stops in the radial-gradient**. The outer edge isn't 'gray color blending into different gray bezel'; it's the bezel itself, alpha-blended through. **No contrast edge means nothing to terminate.**"

This is **the load-bearing insight**. Every "fix" that uses opaque colors at the outer gradient stop reintroduces a hard edge.

## Source-of-truth color values

From `Source/WebUI/knob.js` (the actual landed CSS on `:host`):

```css
background: radial-gradient(circle at 35% 30%,
  rgba(255,255,255,0.24) 0%,
  rgba(255,255,255,0.12) 22%,
  rgba(0,0,0,0.02)       60%,
  rgba(0,0,0,0.07)       100%);
```

Translated to JUCE `juce::ColourGradient`:

```cpp
const juce::Point<float> gradOrigin {
    centre.x - radius * 0.30f,    // 35% from left
    centre.y - radius * 0.40f     // 30% from top
};
juce::ColourGradient body(
    juce::Colour(0x3DFFFFFF), gradOrigin,       // 0% — 24% white
    juce::Colour(0x12000000),                   // 100% — 7% black (bezel shows through)
    { centre.x + radius, centre.y + radius },
    true);
body.addColour(0.22, juce::Colour(0x1FFFFFFF)); // 22% — 12% white
body.addColour(0.60, juce::Colour(0x05000000)); // 60% — 2% black
```

## Box-shadow recipe (per size tier, from knob.js CSS)

| Size | TL₁ offset×blur (white α) | TL₂ offset×blur (white α) | BR₁ offset×blur (black α) | BR₂ offset×blur (black α) |
|------|---------------------------|---------------------------|---------------------------|---------------------------|
| Large (114) | `-4 -4 16 (0.70)` | `-7 -8 30 (0.36)` | `4 5 18 (0.36)` | `7 9 32 (0.18)` |
| Medium (88) | `-3 -3 12 (0.70)` | `-5 -6 22 (0.34)` | `3 4 14 (0.34)` | `5 7 24 (0.16)` |
| Small (56) | `-2 -2 9 (0.66)` | `-3 -4 15 (0.30)` | `2 3 10 (0.30)` | `3 5 17 (0.14)` |

In native:

```cpp
juce::DropShadow brB { juce::Colour(0x29000000), 24, { 5, 7 } };  // 16% black
brB.drawForPath(g, circle);
juce::DropShadow brA { juce::Colour(0x57000000), 14, { 3, 4 } };  // 34% black
brA.drawForPath(g, circle);
juce::DropShadow tlB { juce::Colour(0x57FFFFFF), 24, { -5, -6 } }; // 34% white
tlB.drawForPath(g, circle);
juce::DropShadow tlA { juce::Colour(0xb3FFFFFF), 14, { -3, -3 } }; // 70% white
tlA.drawForPath(g, circle);
```

## Dead ends (do NOT repeat)

- **`filter: drop-shadow` on `:host`** (CSS) — produces a visible 1-px ring at the knob perimeter. Filter region terminates the blur as a sharp edge.
- **SVG `<feDiffuseLighting>` with blurred SourceAlpha heightmap** (CSS) — same termination problem, just in the SVG viewport instead of the CSS layer.
- **Opaque outer gradient stops** (e.g. `rgba(0,0,0,1.0)` or `#A9AAAC`) — produces a visible knob boundary because the panel can't show through. Tried 2026-05-09; reverted after one round-trip.
- **Removing DropShadow entirely** to eliminate the cyan tint — kills the volcano slope. Tried 2026-05-09; reverted in the same iteration after the user noted the depth was gone.
- **Stacked alpha-falloff strokes to approximate Gaussian blur** on indicator arcs — produces visible "tree-rings" effect on the OLED. The WebView2 used a single 6 px stroke + Gaussian filter; using one stroke at lower alpha is closer.
- **Shrinking DropShadow alpha (e.g., 70% → 35% white)** to reduce cyan tint — reduces the 3D depth proportionally. Cyan is from the software blur engine, not the alpha.
- **Halving the bezel ring stroke widths** to "fix" the cyan tint — bezel rings aren't the source.
- **Adding a separate `radial-gradient` halo behind the knob inside the component bounds** — clipped at component edges; doesn't deliver the WebView2 effect. The CSS halo is on the panel `::before` element, sized to 160% of the knob diameter; in JUCE the equivalent would need to be painted by the parent panel BEFORE drawing the knob. Currently the DropShadow + transparent gradient is sufficient; a separate halo is a polish enhancement only.

## When to deviate

You probably shouldn't. The visual was validated by the user across multiple iterations of WebView2 (~25 commits). Before changing the recipe, get explicit user buy-in on the new direction.

## Cyan tint on Windows

`juce::DropShadow` uses software Gaussian blur. On a silver gradient backdrop, white blurs can show a faint cyan/blue tint due to ClearType subpixel anti-aliasing. **Per the user (2026-05-09): this is the LANDED visual.** Don't try to "fix" it by reducing alpha or removing DropShadow — that was the dead-end loop. Address it in a future polish pass via a non-blur shadow approach (e.g., pre-baked PNG halo or `juce::Image`-based premultiplied-alpha blur), but only as an explicit task, not as a side iteration.

## Files implementing this recipe

- `Source/UI/widgets/PhantomKnob.cpp` — `paintBody()`
- `Source/UI/widgets/PhantomMiniKnob.cpp` — `paint()` body section

## Reference

- `docs/thoughts-2026-04-17.md` — full session narrative of the WebView2 breakthrough
- `docs/superpowers/specs/2026-04-16-knob-icy-redesign-design.md` — design spec
- `docs/superpowers/plans/2026-04-16-knob-icy-redesign.md` — implementation plan
- `Source/WebUI/knob.js` — source of truth for the landed visual
