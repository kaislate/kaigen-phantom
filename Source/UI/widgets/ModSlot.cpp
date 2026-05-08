// Source/UI/widgets/ModSlot.cpp
#include "ModSlot.h"
#include "../Theme.h"

namespace kaigen::phantom
{

ModSlot::ModSlot(juce::AudioProcessorValueTreeState& apvts,
                 Type t,
                 const juce::String& sId,
                 const juce::String& paramID,
                 const juce::String& lbl,
                 const juce::String& ph)
    : type(t), slotId(sId), label(lbl), placeholder(ph)
{
    if (type == Type::Macro || type == Type::Morph)
    {
        knob = std::make_unique<PhantomKnob>(apvts, paramID, PhantomKnob::Size::Small,
                                              juce::String{});  // no label on knob; we draw our own
        addAndMakeVisible(*knob);
    }
}

ModSlot::~ModSlot() = default;

void ModSlot::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Type-color accent used for label tint and placeholder dot.
    // (Macro/Morph slots paint via the embedded PhantomKnob child.)
    juce::Colour accent;
    switch (type)
    {
        case Type::Macro:  accent = Theme::macroTeal;    break;
        case Type::Morph:  accent = Theme::morphWhite;   break;
        case Type::Lfo:    accent = Theme::lfoBlue;      break;
        case Type::Random: accent = Theme::randomPurple; break;
    }

    // Label below the slot's interactive area.
    auto labelArea = bounds.removeFromBottom(14);
    g.setColour(accent.withAlpha(type == Type::Lfo || type == Type::Random ? 0.5f : 0.85f));
    g.setFont(juce::FontOptions("Space Grotesk", 9.0f, juce::Font::bold));
    g.drawText(label, labelArea.toFloat(), juce::Justification::centred, false);

    // For LFO/Random placeholders: draw a greyed "PR4" / "PR5" stub.
    if (type == Type::Lfo || type == Type::Random)
    {
        g.setColour(Theme::textDim);
        g.setFont(juce::FontOptions("Space Grotesk", 8.0f, juce::Font::plain));
        const auto stub = placeholder.isEmpty() ? juce::String("--") : placeholder;
        g.drawText(stub, bounds.toFloat(), juce::Justification::centred, false);

        // Type-color dot indicator.
        g.setColour(accent.withAlpha(0.4f));
        const float dotR = 4.0f;
        const auto centre = bounds.getCentre().toFloat();
        g.fillEllipse(centre.x - dotR, centre.y + 8.0f, dotR * 2.0f, dotR * 2.0f);
    }
}

void ModSlot::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromBottom(14);  // reserve label area

    if (knob != nullptr)
        knob->setBounds(bounds);
}

void ModSlot::mouseDown(const juce::MouseEvent&)
{
    if (onSlotClicked) onSlotClicked(slotId);
}

} // namespace kaigen::phantom
