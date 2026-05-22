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
