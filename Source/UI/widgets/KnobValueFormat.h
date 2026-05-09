// Source/UI/widgets/KnobValueFormat.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

namespace kaigen::phantom
{

/** Format a parameter's current value for display in a knob's OLED, using
 *  the parameter's unit label (Hz / dB / % / ms / etc.) to pick a sensible
 *  precision. Mirrors the WebView2 behavior in phantom.js where each knob
 *  has a custom display formatter. */
inline juce::String formatKnobValue(juce::RangedAudioParameter* param, double currentValue)
{
    if (param == nullptr)
        return juce::String(currentValue, 2);

    const float v     = (float) currentValue;
    const auto  unit  = param->getLabel();

    juce::String num;
    if (unit == "Hz")
    {
        if (std::abs(v) >= 1000.0f)
            num = juce::String(v / 1000.0f, 1) + "k";
        else
            num = juce::String((int) std::round(v));
    }
    else if (unit == "dB")
    {
        num = juce::String(v, 1);
    }
    else if (unit == "ms")
    {
        if (std::abs(v) >= 1000.0f)
            num = juce::String(v / 1000.0f, 2) + "s";
        else
            num = juce::String((int) std::round(v));
    }
    else if (unit == "%")
    {
        num = juce::String((int) std::round(v));
    }
    else if (unit.isEmpty())
    {
        // Bare float (macros, normalized knobs): 2 decimals, no unit.
        return juce::String(v, 2);
    }
    else
    {
        // Unknown unit — pass it through with 2 decimals.
        num = juce::String(v, 2);
    }

    return num + unit;
}

} // namespace kaigen::phantom
