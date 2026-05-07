// Source/UI/widgets/PhantomMiniKnob.cpp
#include "PhantomMiniKnob.h"
#include "../Theme.h"
#include "PhantomNativeAssets.h"

namespace kaigen::phantom
{

namespace
{
    constexpr float kAngleMinRadians = -2.356194f;  // -135 deg
    constexpr float kAngleMaxRadians =  2.356194f;  // +135 deg
    constexpr int   kBodySize        = 28;

    float normalizedToAngle(float n)
    {
        return kAngleMinRadians + (kAngleMaxRadians - kAngleMinRadians) * juce::jlimit(0.0f, 1.0f, n);
    }
}

PhantomMiniKnob::PhantomMiniKnob(juce::AudioProcessorValueTreeState& apvts,
                                  juce::StringRef paramID,
                                  const juce::String& lbl)
    : label(lbl)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    slider.addListener(this);
    addChildComponent(slider);

    if (auto* param = apvts.getParameter(paramID))
        attachment = std::make_unique<juce::SliderParameterAttachment>(*param, slider);
    else
        jassertfalse;  // unknown paramID: typo or stale reference

    bodyDrawable = juce::Drawable::createFromImageData(PhantomNativeAssets::knob_mini_svg,
                                                       PhantomNativeAssets::knob_mini_svgSize);

    const int labelHeight = label.isNotEmpty() ? 11 : 0;
    setSize(kBodySize + 6, kBodySize + labelHeight + 2);
}

PhantomMiniKnob::~PhantomMiniKnob()
{
    slider.removeListener(this);
}

void PhantomMiniKnob::paint(juce::Graphics& g)
{
    auto bodyArea = juce::Rectangle<float>((float) (getWidth() - kBodySize) / 2.0f,
                                           2.0f,
                                           (float) kBodySize,
                                           (float) kBodySize);

    if (bodyDrawable != nullptr)
    {
        bodyDrawable->setTransformToFit(bodyArea, juce::RectanglePlacement::stretchToFit);
        bodyDrawable->draw(g, 1.0f);
    }

    const auto centre = bodyArea.getCentre();
    const float radius = bodyArea.getWidth() * 0.40f;
    const float n = (float) slider.getNormalisableRange().convertTo0to1(slider.getValue());
    const float angle = normalizedToAngle(n);
    const float endX = centre.x + radius * std::sin(angle);
    const float endY = centre.y - radius * std::cos(angle);
    g.setColour(Theme::steelBlue);
    g.drawLine(centre.x, centre.y, endX, endY, 1.5f);

    if (label.isNotEmpty())
    {
        g.setColour(Theme::textSecondary);
        g.setFont(juce::FontOptions("Space Grotesk", 8.0f, juce::Font::plain));
        auto labelArea = juce::Rectangle<float>(0.0f,
                                                (float) (kBodySize + 2),
                                                (float) getWidth(),
                                                11.0f);
        g.drawText(label, labelArea, juce::Justification::centred, false);
    }
}

void PhantomMiniKnob::resized()
{
    slider.setBounds(getLocalBounds());
}

void PhantomMiniKnob::mouseDown(const juce::MouseEvent& e)        { slider.mouseDown(e); }
void PhantomMiniKnob::mouseDrag(const juce::MouseEvent& e)        { slider.mouseDrag(e); }
void PhantomMiniKnob::mouseUp(const juce::MouseEvent& e)          { slider.mouseUp(e); }
void PhantomMiniKnob::mouseDoubleClick(const juce::MouseEvent& e) { slider.mouseDoubleClick(e); }

void PhantomMiniKnob::sliderValueChanged(juce::Slider*) { repaint(); }

} // namespace kaigen::phantom
