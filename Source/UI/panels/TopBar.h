// Source/UI/panels/TopBar.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../widgets/PresetSelector.h"
#include "../widgets/EngineTabsWidget.h"
#include "../widgets/ModeTogglePill.h"
#include "../widgets/HeaderButton.h"
#include <memory>

class PhantomProcessor;

namespace kaigen::phantom
{

class TopBar : public juce::Component
{
public:
    TopBar(PhantomProcessor& processor, juce::AudioProcessorValueTreeState& apvts);
    ~TopBar() override;

    PresetSelector& getPresetSelector() noexcept { return presetSelector; }
    ModeTogglePill& getModeToggle()    noexcept { return modeToggle; }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    PresetSelector   presetSelector;
    EngineTabsWidget engineTabs;
    ModeTogglePill   modeToggle;
    std::unique_ptr<HeaderButton> bypassBtn;
    std::unique_ptr<HeaderButton> settingsBtn;
    std::unique_ptr<HeaderButton> advancedBtn;
    PhantomProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TopBar)
};

} // namespace kaigen::phantom
