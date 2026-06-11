// Source/UI/widgets/MatrixCell.cpp
#include "MatrixCell.h"
#include "../Theme.h"
#include "../../PluginProcessor.h"
#include "../../Modulation/ModulationEngine.h"
#include "../../Modulation/Routing.h"

namespace kaigen::phantom
{

MatrixCell::MatrixCell(PhantomProcessor& p, const juce::String& src,
                        const juce::String& param, ModSlot::Type t)
    : processor(p), sourceId(src), paramId(param), type(t)
{
}

MatrixCell::~MatrixCell() = default;

void MatrixCell::setLiveContribution(float v)
{
    const bool wasActive = liveContribution > 0.05f;
    const bool nowActive = v > 0.05f;
    const bool valueMoved = std::abs(v - liveContribution) > 0.01f;
    liveContribution = v;
    // Repaint on any meaningful change OR while modulating, so the breathing
    // pulse animation in paint() actually advances frame-by-frame.
    if (valueMoved || wasActive || nowActive)
        repaint();
}

float MatrixCell::currentDepth() const
{
    // Hot path (310 cells × paint cycles). Use the lock-free snapshot accessor
    // — atomic shared_ptr load instead of a vector copy per call.
    auto& engine = paramId.startsWith("a_")
        ? processor.getModulationEngineA()
        : processor.getModulationEngineB();
    if (auto snap = engine.getRoutingsSnapshot())
        for (const auto& r : *snap)
            if (r.sourceId == sourceId && r.paramId == paramId)
                return r.depth;
    return 0.0f;
}

void MatrixCell::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float depth = currentDepth();
    const bool routed = (std::abs(depth) > kRoutedThreshold);
    const float absDep = juce::jmin(1.0f, std::abs(depth));

    // ── Background ────────────────────────────────────────────────────────
    // Empty: rgba(20,24,30,0.6) == Theme::mtxCellEmpty
    // Routed: base fill uses type-color at CSS-spec alpha
    if (routed)
    {
        juce::Colour accent;
        switch (type)
        {
            case ModSlot::Type::Macro:  accent = Theme::macroTeal;    break;
            case ModSlot::Type::Lfo:    accent = Theme::lfoBlue;      break;
            case ModSlot::Type::Random: accent = Theme::randomPurple; break;
            default:                    accent = Theme::macroTeal;    break;
        }

        // Base fill: rgba(<type-color>, 0.18 + |depth|*0.20)
        const float baseFillAlpha = 0.18f + 0.20f * absDep;
        g.setColour(accent.withAlpha(baseFillAlpha));
        g.fillRect(bounds);

        // Bipolar gradient (CSS: linear-gradient 90deg for positive, 270deg for negative).
        // Positive: left=transparent → right=accent at 0.50*|depth|
        // Negative: left=accent at 0.50*|depth| → right=transparent
        const float gradAlpha = 0.50f * absDep;
        juce::ColourGradient grad;
        if (depth >= 0.0f)
        {
            grad = juce::ColourGradient(accent.withAlpha(0.0f),
                                        bounds.getX(),     bounds.getCentreY(),
                                        accent.withAlpha(gradAlpha),
                                        bounds.getRight(), bounds.getCentreY(),
                                        false);
        }
        else
        {
            grad = juce::ColourGradient(accent.withAlpha(gradAlpha),
                                        bounds.getX(),     bounds.getCentreY(),
                                        accent.withAlpha(0.0f),
                                        bounds.getRight(), bounds.getCentreY(),
                                        false);
        }
        g.setGradientFill(grad);
        g.fillRect(bounds);

        // 1 px inset ring: rgba(<type-color>, 0.55)
        g.setColour(accent.withAlpha(0.55f));
        g.drawRect(bounds.reduced(0.5f), 1.0f);

        // Depth percentage label — white text on dark surface.
        g.setColour(Theme::vizText.withAlpha(0.85f));
        g.setFont(juce::FontOptions("Space Grotesk", 7.0f, juce::Font::bold));
        const int pct = juce::roundToInt(depth * 100.0f);
        const auto txt = (pct > 0 ? juce::String("+") + juce::String(pct) : juce::String(pct));
        g.drawText(txt, bounds, juce::Justification::centred, false);

        // ── Live-pulse breathing animation (preserved from Phase 5) ───────
        if (liveContribution > 0.05f)
        {
            // 700ms cycle, matching the WebView2 mtx-cell-pulse animation.
            const double now    = juce::Time::getMillisecondCounterHiRes();
            const float  phase  = (float) std::fmod(now, 700.0) / 700.0f;  // 0..1
            const float  breath = 0.5f + 0.5f * std::sin(phase * juce::MathConstants<float>::twoPi);
            const float  contrib = juce::jmin(1.0f, liveContribution);
            const float  pulse  = contrib * (0.45f + 0.55f * breath);

            // Brighten the cell fill so the eye catches the activity on small cells.
            g.setColour(accent.brighter(0.3f).withAlpha(pulse * 0.4f));
            g.fillRect(bounds.reduced(1.0f));

            // Inner ring + outer halo for the visible glow.
            g.setColour(accent.brighter(0.6f).withAlpha(juce::jmin(1.0f, pulse * 1.4f)));
            g.drawRoundedRectangle(bounds.reduced(1.5f), 2.0f, 2.0f);
        }
    }
    else
    {
        // Empty cell: rgba(20,24,30,0.6)
        g.setColour(Theme::mtxCellEmpty);
        g.fillRect(bounds);

        // Faint "+" hint text so the cell is clearly interactive.
        g.setColour(juce::Colour(0x40ffffff));  // rgba(255,255,255,0.25)
        g.setFont(juce::FontOptions("Space Grotesk", 7.0f, juce::Font::plain));
        g.drawText("+", bounds, juce::Justification::centred, false);
    }

    // Grid gap line (subtle, matches mtxGridGap).
    g.setColour(Theme::mtxGridGap);
    g.drawRect(bounds, 0.5f);
}

void MatrixCell::resized()
{
}

void MatrixCell::mouseDown(const juce::MouseEvent& e)
{
    dragArmed = false;

    if (e.mods.isPopupMenu())
    {
        showPopover();
        return;
    }

    const float existing = currentDepth();
    auto& engine = paramId.startsWith("a_")
        ? processor.getModulationEngineA()
        : processor.getModulationEngineB();

    if (std::abs(existing) < kRoutedThreshold)
    {
        // Empty cell — add routing at +0.5 AND arm drag so the same gesture
        // can sweep the depth without releasing.
        engine.addRouting({sourceId, paramId, 0.5f, false});
        dragStartDepth = 0.5f;
        repaint();
    }
    else
    {
        dragStartDepth = existing;
    }
    dragStartY = e.y;
    dragArmed  = true;
}

void MatrixCell::mouseDrag(const juce::MouseEvent& e)
{
    // Only react to drags that started on a routed cell with the left button.
    // Without dragArmed, a right-click followed by a drag would slam the cell
    // using uninitialised dragStart* state.
    if (! dragArmed) return;

    // Vertical drag: kDragPxPerUnit logical pixels = full -1..+1 sweep.
    const int   dy       = dragStartY - e.y;  // up = positive
    const float newDepth = juce::jlimit(-1.0f, 1.0f,
                                         dragStartDepth + (float) dy / kDragPxPerUnit);

    auto& engine = paramId.startsWith("a_")
        ? processor.getModulationEngineA()
        : processor.getModulationEngineB();
    if (! engine.setRoutingDepth(sourceId, paramId, newDepth))
        dragArmed = false;  // routing was removed concurrently — stop dragging
    repaint();
}

void MatrixCell::showPopover()
{
    juce::PopupMenu menu;

    const float existing = currentDepth();
    const bool routed = (std::abs(existing) > kRoutedThreshold);

    if (routed)
    {
        menu.addItem(1, "Set -100%");
        menu.addItem(2, "Set 0%");
        menu.addItem(3, "Set +50%");
        menu.addItem(4, "Set +100%");
        menu.addSeparator();
        menu.addItem(5, "Remove");
    }
    else
    {
        menu.addItem(10, "Add at +50%");
        menu.addItem(11, "Add at -50%");
    }

    // SafePointer guards against the cell being destroyed while the menu is open.
    juce::Component::SafePointer<MatrixCell> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(this),
        [safeThis](int result) {
            auto* self = safeThis.getComponent();
            if (self == nullptr) return;

            auto& engine = self->paramId.startsWith("a_")
                ? self->processor.getModulationEngineA()
                : self->processor.getModulationEngineB();

            switch (result)
            {
                case 1:  engine.setRoutingDepth(self->sourceId, self->paramId, -1.0f); break;
                case 2:  engine.setRoutingDepth(self->sourceId, self->paramId,  0.0f); break;
                case 3:  engine.setRoutingDepth(self->sourceId, self->paramId,  0.5f); break;
                case 4:  engine.setRoutingDepth(self->sourceId, self->paramId,  1.0f); break;
                case 5:  engine.removeRouting (self->sourceId, self->paramId);         break;
                case 10: engine.addRouting({self->sourceId, self->paramId,  0.5f, false}); break;
                case 11: engine.addRouting({self->sourceId, self->paramId, -0.5f, false}); break;
                default: break;
            }
            self->repaint();
        });
}

} // namespace kaigen::phantom
