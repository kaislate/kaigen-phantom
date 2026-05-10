// Source/UI/widgets/WordSelector.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace kaigen::phantom
{

/** Underlined-text selector matching the WebView2 `.lw` preset-word style.
 *  Renders each option as bold uppercase text with an underline; the active
 *  option uses darker text and a darker underline. Click to select.
 *  Bound to an APVTS choice parameter via `juce::ComboBoxParameterAttachment`. */
class WordSelector : public juce::Component, private juce::ComboBox::Listener
{
public:
    WordSelector(juce::AudioProcessorValueTreeState& apvts,
                 juce::StringRef paramID,
                 const juce::StringArray& labels);
    ~WordSelector() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

private:
    void comboBoxChanged(juce::ComboBox* c) override;
    int  hitWord(juce::Point<int> p) const;

    juce::ComboBox combo;
    std::unique_ptr<juce::ComboBoxParameterAttachment> attachment;
    juce::StringArray labels;
    juce::Array<juce::Rectangle<int>> wordBounds;   // computed in resized()
    int hoverIndex { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WordSelector)
};

} // namespace kaigen::phantom
