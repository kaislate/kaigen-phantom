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
    const bool routed = (std::abs(depth) > 0.001f);

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

} // namespace kaigen::phantom
