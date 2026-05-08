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

}

NativePluginEditor::NativePluginEditor(PhantomProcessor& p,
                                       juce::AudioProcessorValueTreeState& a)
    : juce::AudioProcessorEditor(&p), processor(p), apvts(a),
      rightPanel(a, p), leftPanel(a), topBar(p, a), presetBrowser(p, a),
      modulationPanel(p, a), matrixView(p, a)
{
    setSize(editorWidth, editorHeight);

    backToWebViewButton.addListener(this);
    addAndMakeVisible(backToWebViewButton);
    addAndMakeVisible(rightPanel);
    addAndMakeVisible(leftPanel);
    addAndMakeVisible(topBar);

    addAndMakeVisible(modulationPanel);

    addAndMakeVisible(matrixView);
    matrixView.setVisible(false);
    matrixView.toFront(false);

    // Wire ModulationPanel's MATRIX toggle to show/hide matrixView.
    // Mutual exclusion: opening matrix dismisses the preset browser if open.
    modulationPanel.onMatrixToggle = [this](bool active) {
        if (active && presetBrowser.isVisible())
            presetBrowser.setVisible(false);
        matrixView.setVisible(active);
        if (active) matrixView.toFront(false);
    };

    // Browser is added but starts hidden; clicked-to-show by Browse button.
    addAndMakeVisible(presetBrowser);
    presetBrowser.setVisible(false);
    presetBrowser.toFront(false);  // ensure it's painted on top of other panels

    // Wire PresetSelector callbacks via TopBar.
    topBar.getPresetSelector().onBrowseRequested = [this]
    {
        // Mutual exclusion: opening the browser dismisses matrix if open.
        // (ModulationPanel mode-bar state catches up the next time the user
        // clicks SLOTS — Task 7 will add a proper setMatrixActive(bool) hook.)
        if (matrixView.isVisible())
            matrixView.setVisible(false);
        presetBrowser.setVisible(true);
        presetBrowser.toFront(false);
    };
    presetBrowser.onPresetSelected = [this](juce::String name, juce::String pack)
    {
        topBar.getPresetSelector().setCurrentPreset(name, pack);
    };
}

NativePluginEditor::~NativePluginEditor()
{
    backToWebViewButton.removeListener(this);
}

void NativePluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(Theme::deepBg);
    auto area = getLocalBounds();
    area.removeFromTop(topBarHeight);
    area.removeFromBottom(modPanelHeight);
    // LeftPanel and RightPanel are real Components — no wireframes.
    // ModulationPanel is now real too.
}

void NativePluginEditor::resized()
{
    // Corner escape-hatch button -- dev-only, removed in Phase 6.
    backToWebViewButton.setBounds(getWidth() - 110, 10, 100, 26);

    auto area = getLocalBounds();

    auto topBarArea = area.removeFromTop(topBarHeight);
    topBar.setBounds(topBarArea);

    auto modPanelArea = area.removeFromBottom(modPanelHeight);
    modulationPanel.setBounds(modPanelArea);

    auto leftBounds = area.removeFromLeft(leftPanelWidth);
    leftPanel.setBounds(leftBounds);
    rightPanel.setBounds(area);

    // PresetBrowser overlays the entire editor when visible.
    presetBrowser.setBounds(getLocalBounds());
    matrixView.setBounds(getLocalBounds());
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
