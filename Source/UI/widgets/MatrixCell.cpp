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
    if (std::abs(v - liveContribution) > 0.01f)
    {
        liveContribution = v;
        repaint();
    }
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

    // Background.
    g.setColour(routed ? Theme::matrixBg.brighter(0.10f) : Theme::matrixBg);
    g.fillRect(bounds);

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
        const float alpha = juce::jmin(1.0f, std::abs(depth));

        if (depth >= 0.0f)
        {
            auto grad = juce::ColourGradient(accent.withAlpha(alpha),
                                              bounds.getX(), bounds.getCentreY(),
                                              accent.withAlpha(0.0f),
                                              bounds.getRight(), bounds.getCentreY(),
                                              false);
            g.setGradientFill(grad);
        }
        else
        {
            auto grad = juce::ColourGradient(accent.withAlpha(0.0f),
                                              bounds.getX(), bounds.getCentreY(),
                                              accent.withAlpha(alpha),
                                              bounds.getRight(), bounds.getCentreY(),
                                              false);
            g.setGradientFill(grad);
        }
        g.fillRect(bounds.reduced(1.0f));

        g.setColour(Theme::textPrimary);
        g.setFont(juce::FontOptions("Space Grotesk", 7.0f, juce::Font::bold));
        const int pct = juce::roundToInt(depth * 100.0f);
        const auto txt = (pct > 0 ? juce::String("+") + juce::String(pct) : juce::String(pct));
        g.drawText(txt, bounds, juce::Justification::centred, false);

        if (liveContribution > 0.05f)
        {
            g.setColour(accent.withAlpha(0.4f * juce::jmin(1.0f, liveContribution)));
            g.drawRoundedRectangle(bounds.reduced(1.0f), 2.0f, 1.5f);
        }
    }

    g.setColour(Theme::panelBorder);
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
        // Empty cell — add routing at +0.5.
        engine.addRouting({sourceId, paramId, 0.5f, false});
        repaint();
    }
    else
    {
        // Existing cell — arm drag.
        dragStartDepth = existing;
        dragStartY     = e.y;
        dragArmed      = true;
    }
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
