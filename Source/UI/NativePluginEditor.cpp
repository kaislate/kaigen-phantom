// Source/UI/NativePluginEditor.cpp
#include "NativePluginEditor.h"
#include "../PluginProcessor.h"
#include "Theme.h"

namespace kaigen::phantom
{

namespace
{
    // Wireframe panel positions. Phase 1+ replaces these with real child Components.
    constexpr int editorWidth         = 1300;
    constexpr int editorHeight        = 970;
    constexpr int topBarHeight        = 50;
    constexpr int modPanelHeight      = 150;
    constexpr int leftPanelWidth      = 420;

    void drawLabeledPanel(juce::Graphics& g,
                          juce::Rectangle<int> bounds,
                          const juce::String& label)
    {
        g.setColour(Theme::panelBg);
        g.fillRect(bounds);
        g.setColour(Theme::panelBorder);
        g.drawRect(bounds, 1);
        g.setColour(Theme::textSecondary);
        g.setFont(juce::Font("Space Grotesk", 14.0f, juce::Font::bold));
        g.drawText(label, bounds, juce::Justification::centred, false);
    }
}

NativePluginEditor::NativePluginEditor(PhantomProcessor& p,
                                       juce::AudioProcessorValueTreeState& a)
    : juce::AudioProcessorEditor(&p), processor(p), apvts(a), rightPanel(a, p), leftPanel(a)
{
    setSize(editorWidth, editorHeight);

    backToWebViewButton.addListener(this);
    addAndMakeVisible(backToWebViewButton);
    addAndMakeVisible(rightPanel);
    addAndMakeVisible(leftPanel);
}

NativePluginEditor::~NativePluginEditor()
{
    backToWebViewButton.removeListener(this);
}

void NativePluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(Theme::deepBg);

    auto area = getLocalBounds();

    // TopBar across the very top.
    auto topBar = area.removeFromTop(topBarHeight);
    drawLabeledPanel(g, topBar, "TopBar (preset selector + advanced toggle)");

    // ModulationPanel across the very bottom.
    auto modPanel = area.removeFromBottom(modPanelHeight);
    drawLabeledPanel(g, modPanel, "ModulationPanel (mode bar + slot row + engine labels)");

    // LeftPanel and RightPanel are real Components now -- no wireframe rectangles.
}

void NativePluginEditor::resized()
{
    // Corner escape-hatch button -- dev-only, removed in Phase 6.
    backToWebViewButton.setBounds(getWidth() - 110, 10, 100, 26);

    auto area = getLocalBounds();
    area.removeFromTop(topBarHeight);
    area.removeFromBottom(modPanelHeight);

    auto leftBounds = area.removeFromLeft(leftPanelWidth);
    leftPanel.setBounds(leftBounds);
    rightPanel.setBounds(area);
}

void NativePluginEditor::buttonClicked(juce::Button* b)
{
    if (b == &backToWebViewButton)
    {
        auto state = processor.getEditorView();
        state.useNativeEditor = false;
        processor.setEditorView(state);
        processor.updateHostDisplay();
        // Note: change takes effect on next editor reopen. The current
        // editor stays as-is until the host destroys + recreates it.
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "Switched to WebView2",
            "Close and reopen the plugin window to load the WebView2 UI.");
    }
}

} // namespace kaigen::phantom
