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
    setLookAndFeel(&lookAndFeel);
    setSize(editorWidth, editorHeight);

    addAndMakeVisible(rightPanel);
    addAndMakeVisible(leftPanel);
    addAndMakeVisible(topBar);

    addAndMakeVisible(modulationPanel);

    addAndMakeVisible(matrixView);
    matrixView.setVisible(false);
    matrixView.toFront(false);

    // Single source of truth for persisting matrix mode + re-laying out.
    auto persistMatrixMode = [this](bool active) {
        auto state = processor.getMatrixView();
        state.mode = active ? kaigen::phantom::MatrixMode::Matrix
                            : kaigen::phantom::MatrixMode::Slots;
        processor.setMatrixView(state);
        processor.updateHostDisplay();
    };

    // Wire ModulationPanel's MATRIX toggle to show/hide matrixView.
    // Mutual exclusion: opening matrix dismisses the preset browser if open.
    modulationPanel.onMatrixToggle = [this, persistMatrixMode](bool active) {
        if (active && presetBrowser.isVisible())
            presetBrowser.setVisible(false);
        matrixView.setVisible(active);
        if (active) matrixView.toFront(false);
        resized();  // Slot row collapses → bottom panel shrinks → re-layout left/right panels.
        persistMatrixMode(active);
    };

    // Click-outside-the-card dismiss → persist Slots mode.
    matrixView.onDismissed = [this, persistMatrixMode] {
        resized();
        persistMatrixMode(false);
    };

    // Modulations toggle → relayout (panel grows/shrinks for the slot row).
    modulationPanel.onSlotsExpandedChanged = [this](bool) { resized(); };

    // Browser is added but starts hidden; clicked-to-show by Browse button.
    addAndMakeVisible(presetBrowser);
    presetBrowser.setVisible(false);
    presetBrowser.toFront(false);  // ensure it's painted on top of other panels

    // Wire PresetSelector callbacks via TopBar.
    topBar.getPresetSelector().onBrowseRequested = [this, persistMatrixMode]
    {
        // Mutual exclusion: opening the browser dismisses matrix if open.
        if (matrixView.isVisible())
        {
            matrixView.setVisible(false);
            resized();
            persistMatrixMode(false);
        }
        presetBrowser.setVisible(true);
        presetBrowser.toFront(false);
    };
    presetBrowser.onPresetSelected = [this](juce::String name, juce::String pack)
    {
        topBar.getPresetSelector().setCurrentPreset(name, pack);
    };

    // (Slot-click → matrix handoff removed — macros are always visible in
    // the footer now, so a click should drag the value, not open matrix.
    // Use the MATRIX button to toggle the overlay.)

    // Restore persisted matrix mode.
    const auto persisted = processor.getMatrixView();
    if (persisted.mode == kaigen::phantom::MatrixMode::Matrix)
    {
        matrixView.setVisible(true);
        matrixView.toFront(false);
        resized();
    }
}

NativePluginEditor::~NativePluginEditor()
{
    setLookAndFeel(nullptr);
}

void NativePluginEditor::paint(juce::Graphics& g)
{
    // Outer body — dark behind the modulation panel and behind any overlay edges.
    // TopBar, LeftPanel, RightPanel, and ModulationPanel all paint their own surfaces.
    g.fillAll(Theme::editorBg);
}

void NativePluginEditor::resized()
{
    auto area = getLocalBounds();

    auto topBarArea = area.removeFromTop(topBarHeight);
    topBar.setBounds(topBarArea);

    // Mod panel is its collapsed mode-bar-only height unless the user has
    // explicitly expanded the slot row via the MODULATIONS button.
    const bool modExpanded = modulationPanel.isSlotsExpanded() && ! modulationPanel.isMatrixActive();
    const int currentModPanelHeight = modExpanded
        ? modPanelHeight
        : ModulationPanel::kCollapsedHeight;
    auto modPanelArea = area.removeFromBottom(currentModPanelHeight);
    modulationPanel.setBounds(modPanelArea);

    auto leftBounds = area.removeFromLeft(leftPanelWidth);
    leftPanel.setBounds(leftBounds);
    rightPanel.setBounds(area);

    // PresetBrowser overlays the entire editor when visible.
    presetBrowser.setBounds(getLocalBounds());
    matrixView.setBounds(getLocalBounds());
}

} // namespace kaigen::phantom
