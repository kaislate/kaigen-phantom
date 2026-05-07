// Source/UI/widgets/PhantomKnob.cpp
#include "PhantomKnob.h"
#include "../Theme.h"
#include "PhantomNativeAssets.h"

namespace kaigen::phantom
{

namespace
{
    // Indicator angle range: -135 deg (min, 7-o'clock) to +135 deg (max, 5-o'clock).
    // Total sweep = 270 deg. Matches typical synth knob convention.
    constexpr float kAngleMinRadians = -2.356194f;  // -135 * PI / 180
    constexpr float kAngleMaxRadians =  2.356194f;  //  135 * PI / 180

    float normalizedToAngle(float n)
    {
        return kAngleMinRadians + (kAngleMaxRadians - kAngleMinRadians) * juce::jlimit(0.0f, 1.0f, n);
    }

    juce::Drawable* loadKnobSvg(PhantomKnob::Size size)
    {
        const char* data = nullptr;
        int dataSize = 0;
        switch (size)
        {
            case PhantomKnob::Size::Large:
                data = PhantomNativeAssets::knob_large_svg;
                dataSize = PhantomNativeAssets::knob_large_svgSize;
                break;
            case PhantomKnob::Size::Medium:
                data = PhantomNativeAssets::knob_medium_svg;
                dataSize = PhantomNativeAssets::knob_medium_svgSize;
                break;
            case PhantomKnob::Size::Small:
                data = PhantomNativeAssets::knob_small_svg;
                dataSize = PhantomNativeAssets::knob_small_svgSize;
                break;
        }
        if (data == nullptr) return nullptr;
        auto drawable = juce::Drawable::createFromImageData(data, (size_t) dataSize);
        return drawable.release();
    }

    int sizePixels(PhantomKnob::Size s)
    {
        switch (s)
        {
            case PhantomKnob::Size::Large:  return 66;
            case PhantomKnob::Size::Medium: return 50;
            case PhantomKnob::Size::Small:  return 36;
        }
        return 50;
    }
}

PhantomKnob::PhantomKnob(juce::AudioProcessorValueTreeState& apvts,
                         juce::StringRef paramID,
                         Size sz,
                         const juce::String& lbl)
    : size(sz), label(lbl)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    slider.addListener(this);
    addChildComponent(slider);  // hidden -- we forward events manually

    if (auto* param = apvts.getParameter(paramID))
        attachment = std::make_unique<juce::SliderParameterAttachment>(*param, slider);
    else
        jassertfalse;  // unknown paramID: typo or stale reference

    bodyDrawable.reset(loadKnobSvg(size));

    // Total widget height = body + label space underneath.
    const int body = sizePixels(size);
    const int labelHeight = label.isNotEmpty() ? 14 : 0;
    setSize(body + 8, body + labelHeight + 4);
}

PhantomKnob::~PhantomKnob()
{
    slider.removeListener(this);
}

void PhantomKnob::paint(juce::Graphics& g)
{
    const int body = sizePixels(size);
    auto bodyArea = juce::Rectangle<float>((float) (getWidth() - body) / 2.0f,
                                           4.0f,
                                           (float) body,
                                           (float) body);

    if (bodyDrawable != nullptr)
    {
        bodyDrawable->setTransformToFit(bodyArea, juce::RectanglePlacement::stretchToFit);
        bodyDrawable->draw(g, 1.0f);
    }
    else
    {
        // Fallback if SVG load failed.
        g.setColour(Theme::panelBg);
        g.fillEllipse(bodyArea);
    }

    // Indicator line -- drawn from center outward at the current value's angle.
    const auto centre = bodyArea.getCentre();
    const float radius = bodyArea.getWidth() * 0.42f;
    const float n = (float) slider.getNormalisableRange().convertTo0to1(slider.getValue());
    const float angle = normalizedToAngle(n);
    const float endX = centre.x + radius * std::sin(angle);
    const float endY = centre.y - radius * std::cos(angle);
    g.setColour(Theme::steelBlue);
    g.drawLine(centre.x, centre.y, endX, endY, 2.0f);

    // Label below body.
    if (label.isNotEmpty())
    {
        g.setColour(Theme::textSecondary);
        g.setFont(juce::FontOptions("Space Grotesk", 10.0f, juce::Font::plain));
        auto labelArea = juce::Rectangle<float>(0.0f,
                                                (float) (body + 4),
                                                (float) getWidth(),
                                                14.0f);
        g.drawText(label, labelArea, juce::Justification::centred, false);
    }
}

void PhantomKnob::resized()
{
    // Slider is hidden -- sized to match the full bounds for event hit-testing.
    slider.setBounds(getLocalBounds());
}

void PhantomKnob::mouseDown(const juce::MouseEvent& e)        { slider.mouseDown(e); }
void PhantomKnob::mouseDrag(const juce::MouseEvent& e)        { slider.mouseDrag(e); }
void PhantomKnob::mouseUp(const juce::MouseEvent& e)          { slider.mouseUp(e); }
void PhantomKnob::mouseDoubleClick(const juce::MouseEvent& e) { slider.mouseDoubleClick(e); }

void PhantomKnob::sliderValueChanged(juce::Slider*)
{
    repaint();
}

} // namespace kaigen::phantom
