// Source/UI/widgets/LinkButton.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace kaigen::phantom
{

class LinkButton : public juce::Component
{
public:
    LinkButton();
    ~LinkButton() override;

    /** Whether the link is currently active. Defaults false. */
    bool isLinked() const { return linked; }
    void setLinked(bool shouldBeLinked);

    /** Called on click toggle. */
    std::function<void(bool)> onLinkChanged;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    bool linked { false };
    std::unique_ptr<juce::Drawable> icon;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LinkButton)
};

} // namespace kaigen::phantom
