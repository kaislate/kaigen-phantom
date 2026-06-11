// Source/UI/widgets/EyeButton.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace kaigen::phantom
{

/** Minimal show/hide toggle painted as an eye icon. Open eye = "visible
 *  / expanded"; closed eye (a single curved eyelid line) = "hidden /
 *  collapsed". Etched body matches the rest of the silver-panel UI
 *  language: white 1 px shadow + dark line glyph. No background fill,
 *  no border — the icon itself is the affordance.
 *
 *  Click toggles the internal `eyeOpen` state and fires onToggle(bool).
 *  Caller is responsible for wiring layout changes (e.g. hide /show
 *  child widgets) in the callback. */
class EyeButton : public juce::Button
{
public:
    EyeButton();
    ~EyeButton() override = default;

    /** Whether the eye is currently rendered as "open" (visible). */
    bool isOpen() const noexcept { return eyeOpen; }

    /** Imperatively set the state — does NOT fire onToggle. Used to
     *  initialise the button to match an existing model bool. */
    void setIsOpen(bool open);

    /** Fires after a click toggles the state. The bool arg is the NEW
     *  open/closed state. */
    std::function<void(bool)> onToggle;

    void paintButton(juce::Graphics& g,
                     bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override;
    void clicked() override;

private:
    bool eyeOpen { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EyeButton)
};

} // namespace kaigen::phantom
