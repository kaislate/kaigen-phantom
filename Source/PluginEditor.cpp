#include "PluginEditor.h"
#include "BinaryData.h"

#if JUCE_WINDOWS
#include <windows.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")
#endif

static const char* getMimeForExtension(const juce::String& extension)
{
    static const std::unordered_map<juce::String, const char*> mimeMap =
    {
        { "html", "text/html" },  { "htm",  "text/html" },
        { "css",  "text/css" },   { "js",   "text/javascript" },
        { "json", "application/json" },
        { "png",  "image/png" },  { "jpg",  "image/jpeg" },
        { "svg",  "image/svg+xml" }, { "woff2","font/woff2" },
    };
    if (const auto it = mimeMap.find(extension.toLowerCase()); it != mimeMap.end())
        return it->second;
    return "application/octet-stream";
}

#if JUCE_WINDOWS
namespace {

constexpr UINT_PTR kFocusRedirectId = 0x4B4750; // 'KGP'

// Set to true by JS whenever a text input / textarea / contenteditable has
// focus inside the WebView. While true, the keyboard subclass lets the
// WebView handle keys itself so the user can type. While false, keys are
// forwarded up to the top-level window (the DAW host) so spacebar plays,
// A–L triggers MIDI keyboard mode, etc.
std::atomic<bool> sWebViewInputFocused { false };

bool shouldForwardKey(UINT msg)
{
    return msg == WM_KEYDOWN  || msg == WM_KEYUP
        || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP
        || msg == WM_CHAR       || msg == WM_SYSCHAR;
}

LRESULT CALLBACK webViewFocusSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                       UINT_PTR id, DWORD_PTR)
{
    const bool inputFocused = sWebViewInputFocused.load(std::memory_order_relaxed);

    // When nothing text-y is focused, don't let a mouse click shift keyboard
    // focus onto the WebView. The click itself still goes through to JS so
    // knob drags work — we just refuse to become the active window.
    if (msg == WM_MOUSEACTIVATE && !inputFocused)
        return MA_NOACTIVATE;

    if (msg == WM_SETFOCUS)
    {
        // Redirect focus to the parent HWND so the DAW keeps keyboard MIDI
        if (HWND parent = GetParent(hwnd))
            SetFocus(parent);
        return 0;
    }

    // Forward keyboard input to the DAW's top-level window unless a text
    // field inside the WebView currently has focus.
    if (shouldForwardKey(msg) && !inputFocused)
    {
        if (HWND top = GetAncestor(hwnd, GA_ROOT))
        {
            if (top != hwnd)
            {
                PostMessage(top, msg, wParam, lParam);
                return 0;
            }
        }
    }

    if (msg == WM_NCDESTROY)
        RemoveWindowSubclass(hwnd, webViewFocusSubclass, id);
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

BOOL CALLBACK installOnChromeWindows(HWND hwnd, LPARAM)
{
    wchar_t cls[256] = {};
    GetClassNameW(hwnd, cls, 255);
    // Match any Chromium-owned child window that can hold keyboard focus.
    // "Chrome_WidgetWin"        — outer WebView HWND
    // "Chrome_RenderWidgetHostHWND" — inner renderer (where keys actually land)
    // "Intermediate D3D Window"      — compositing layer (seen in some versions)
    const bool isChromeWindow =
            wcsncmp(cls, L"Chrome_",            7)  == 0 ||
            wcsncmp(cls, L"Intermediate D3D",  16) == 0;

    if (isChromeWindow)
        SetWindowSubclass(hwnd, webViewFocusSubclass, kFocusRedirectId, 0);
    EnumChildWindows(hwnd, installOnChromeWindows, 0);
    return TRUE;
}

} // namespace

void PhantomEditor::FocusRescanTimer::timerCallback()
{
    if (owner == nullptr) return;
    if (auto* peer = owner->getPeer())
        installOnChromeWindows((HWND) peer->getNativeHandle(), 0);
}

void PhantomEditor::parentHierarchyChanged()
{
    // WebView2 creates its HWNDs asynchronously, and some Chromium helper
    // windows appear only after the user interacts with the UI (e.g., after
    // first mouse-down on a canvas). We rescan on a long-running timer so
    // those windows also get the focus subclass installed.
    focusRescanTimer.owner = this;
    if (!focusRescanTimer.isTimerRunning())
        focusRescanTimer.startTimer(1000);

    // Also do a few quick passes during startup to catch windows before
    // the first real scan tick.
    auto tryInstall = [this]()
    {
        if (auto* peer = getPeer())
            installOnChromeWindows((HWND) peer->getNativeHandle(), 0);
    };
    for (int delayMs : { 50, 200, 500 })
        juce::Timer::callAfterDelay(delayMs, tryInstall);
}
#endif

juce::WebBrowserComponent::Options PhantomEditor::buildWebViewOptions(PhantomEditor& self)
{
   #if JUCE_WINDOWS
    // Force WebView2 to render at devicePixelRatio = 1 regardless of monitor
    // DPR. On HiDPI displays this quarters the GPU texture memory per surface
    // (a 2× DPR → 1× drop cuts texture area by 4), dramatically reducing DWM
    // compositor work when multiple plugin UIs are visible at once. Visual
    // tradeoff: canvas content renders slightly softer on HiDPI, but the
    // perf ceiling with 2+ visible instances rises substantially.
    std::wstring webViewArgs = L"--force-device-scale-factor=1";

    // Debug port is opt-in (see prior note on TCP-bind stalls). Set
    // KAIGEN_PHANTOM_DEVTOOLS=1 before launching the host to re-enable
    // DevTools on port 9222.
    if (juce::SystemStats::getEnvironmentVariable("KAIGEN_PHANTOM_DEVTOOLS", {}).isNotEmpty())
        webViewArgs += L" --remote-debugging-port=9222 --remote-allow-origins=*";

    SetEnvironmentVariableW(L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS", webViewArgs.c_str());
   #endif

    // Shared WebView2 User Data Folder across all Phantom instances in the same
    // process. WebView2 reuses a single browser process for WebViews that share
    // a UDF, which is cheaper at open-time than spawning a fresh Chromium per
    // instance. The UDF lives under our app's roaming-data dir rather than the
    // OS temp dir so it doesn't collide with other apps' temp files.
    auto udf = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("Kaigen")
                   .getChildFile("KaigenPhantom")
                   .getChildFile("WebView2UserData");
    udf.createDirectory();

    auto options = juce::WebBrowserComponent::Options{}
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options(
            juce::WebBrowserComponent::Options::WinWebView2{}
                .withUserDataFolder(udf))
        .withNativeIntegrationEnabled();

    // ── Slider relays ─────────────────────────────────────────────────
    juce::WebSliderRelay* sliderRelays[] = {
        &self.inputGainRelay,
        &self.ghostRelay, &self.phantomThresholdRelay, &self.phantomStrengthRelay,
        &self.outputGainRelay,
        &self.recipeH2Relay, &self.recipeH3Relay, &self.recipeH4Relay,
        &self.recipeH5Relay, &self.recipeH6Relay, &self.recipeH7Relay, &self.recipeH8Relay,
        &self.harmonicSaturationRelay,
        &self.synthStepRelay, &self.synthDutyRelay, &self.synthSkipRelay,
        &self.envAttackRelay, &self.envReleaseRelay,
        &self.binauralWidthRelay, &self.stereoWidthRelay,
        &self.synthLPFRelay, &self.synthHPFRelay,
        &self.synthWaveletLengthRelay, &self.synthGateThresholdRelay,
        &self.synthH1Relay, &self.synthSubRelay, &self.synthMinSamplesRelay, &self.synthMaxSamplesRelay, &self.trackingSpeedRelay,
        &self.punchAmountRelay,
        &self.synthBoostThresholdRelay, &self.synthBoostAmountRelay
    };
    for (auto* r : sliderRelays)
        options = options.withOptionsFrom(*r);

    // ── Combo-box relays ──────────────────────────────────────────────
    juce::WebComboBoxRelay* comboRelays[] = {
        &self.modeRelay, &self.ghostModeRelay,
        &self.recipePresetRelay, &self.binauralModeRelay,
        &self.filterSlopeRelay
    };
    for (auto* r : comboRelays)
        options = options.withOptionsFrom(*r);

    // ── Toggle relays ────────────────────────────────────────────────
    options = options.withOptionsFrom(self.bypassRelay);
    options = options.withOptionsFrom(self.punchEnabledRelay);
    options = options.withOptionsFrom(self.inputGainAutoRelay);
    options = options.withOptionsFrom(self.midiTriggerRelay);
    options = options.withOptionsFrom(self.midiGateReleaseRelay);

    // ── Native functions for real-time data ──────────────────────────
    options = options
        .withNativeFunction("getSpectrumData",
            [&self](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                juce::Array<juce::var> inputBins, outputBins;
                for (int i = 0; i < PhantomProcessor::kSpectrumBins; ++i)
                {
                    inputBins .add(self.processor.spectrumData      [(size_t) i]);
                    outputBins.add(self.processor.spectrumOutputData[(size_t) i]);
                }
                auto* obj = new juce::DynamicObject();
                obj->setProperty("input",  inputBins);
                obj->setProperty("output", outputBins);
                complete(juce::var(obj));
            })
        .withNativeFunction("getPeakLevels",
            [&self](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                auto* obj = new juce::DynamicObject();
                obj->setProperty("inL",  (double) self.processor.peakInL .load(std::memory_order_relaxed));
                obj->setProperty("inR",  (double) self.processor.peakInR .load(std::memory_order_relaxed));
                obj->setProperty("outL", (double) self.processor.peakOutL.load(std::memory_order_relaxed));
                obj->setProperty("outR", (double) self.processor.peakOutR.load(std::memory_order_relaxed));
                complete(juce::var(obj));
            })
        .withNativeFunction("getPitchInfo",
            [&self](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                auto* obj = new juce::DynamicObject();
                const float hz = self.processor.currentPitch.load(std::memory_order_relaxed);
                obj->setProperty("hz", (double) hz);
                if (hz > 0.0f)
                {
                    static const char* noteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
                    const int midi = juce::roundToInt(12.0f * std::log2(hz / 440.0f) + 69.0f);
                    const int note = ((midi % 12) + 12) % 12;
                    const int octave = (midi / 12) - 1;
                    obj->setProperty("note", juce::String(noteNames[note]) + juce::String(octave));
                }
                else
                {
                    obj->setProperty("note", "---");
                }
                const int presetIdx = (int) self.processor.apvts.getRawParameterValue(ParamID::RECIPE_PRESET)->load();
                static const char* presetNames[] = { "Warm","Aggressive","Hollow","Dense","Stable","Weird","Custom" };
                obj->setProperty("preset", juce::String(presetNames[juce::jlimit(0, 6, presetIdx)]));
                complete(juce::var(obj));
            })
        .withNativeFunction("getOscilloscopeData",
            [&self](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                juce::Array<juce::var> inArr, synthArr, outArr;
                for (int i = 0; i < PhantomEngine::kOscBufSize; ++i)
                {
                    inArr  .add((double) self.processor.oscInputBuf [(size_t) i]);
                    synthArr.add((double) self.processor.engine.oscSynthBuf[(size_t) i]);
                    outArr .add((double) self.processor.oscOutputBuf[(size_t) i]);
                }
                auto* obj = new juce::DynamicObject();
                obj->setProperty("input",       inArr);
                obj->setProperty("synth",       synthArr);
                obj->setProperty("output",      outArr);
                obj->setProperty("inputWrPos",  (int) self.processor.oscInputWrPos .load(std::memory_order_relaxed));
                obj->setProperty("synthWrPos",  (int) self.processor.engine.oscSynthWrPos.load(std::memory_order_relaxed));
                obj->setProperty("outputWrPos", (int) self.processor.oscOutputWrPos.load(std::memory_order_relaxed));
                obj->setProperty("sampleRate",   (double) self.processor.getSampleRate());
                obj->setProperty("synthPeak",    (double) self.processor.engine.getSynthInputPeak());
                complete(juce::var(obj));
            })
        .withNativeFunction("setEditorHeight",
            [&self](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                const int height = args.size() > 0 ? (int) args[0] : 820;
                const int clamped = juce::jlimit(400, 2000, height);
                juce::MessageManager::callAsync([weakSelf = juce::Component::SafePointer<PhantomEditor>(&self), clamped]
                {
                    if (auto* p = weakSelf.getComponent())
                        p->setSize(1300, clamped);
                });
                complete(juce::var(true));
            })
        .withNativeFunction("setInputFocused",
            // Tells the Win32 subclass whether a text field inside the WebView
            // has focus — determines whether keyboard events are forwarded to
            // the DAW (false) or handled by the WebView (true).
            []([[maybe_unused]] const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
               #if JUCE_WINDOWS
                const bool focused = args.size() > 0 && args[0].isBool() && (bool) args[0];
                sWebViewInputFocused.store(focused, std::memory_order_relaxed);
               #endif
                complete(juce::var(true));
            })
        .withNativeFunction("returnFocusToHost",
            // Called from JS after any mouseup on a non-text element. WebView2
            // grabs keyboard focus internally during interaction (through its
            // own focus routing that bypasses Windows focus APIs), so we have
            // to explicitly hand focus back to the DAW's top-level window.
            [&self]([[maybe_unused]] const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
               #if JUCE_WINDOWS
                juce::MessageManager::callAsync([weakSelf = juce::Component::SafePointer<PhantomEditor>(&self)]
                {
                    if (auto* ed = weakSelf.getComponent())
                    {
                        if (auto* peer = ed->getPeer())
                        {
                            HWND pluginHwnd = (HWND) peer->getNativeHandle();
                            HWND top = GetAncestor(pluginHwnd, GA_ROOT);
                            if (top != nullptr) SetFocus(top);
                        }
                    }
                });
               #endif
                complete(juce::var(true));
            })
        .withNativeFunction("forwardKeyToHost",
            // JS calls this for every keydown/keyup that lands on a non-input
            // DOM element. We PostMessage the key to the DAW's top-level window
            // so transport controls (spacebar) and MIDI keyboard (A-L) work
            // regardless of whether the WebView stole keyboard focus.
            //
            // Args: [virtualKeyCode: int, isDown: bool]
            [&self](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
               #if JUCE_WINDOWS
                if (args.size() >= 2)
                {
                    const WPARAM vk = (WPARAM) (int) args[0];
                    const bool isDown = args[1].isBool() && (bool) args[1];

                    if (auto* peer = self.getPeer())
                    {
                        HWND pluginHwnd = (HWND) peer->getNativeHandle();
                        HWND top = GetAncestor(pluginHwnd, GA_ROOT);
                        if (top != nullptr && top != pluginHwnd)
                        {
                            // Minimal, well-formed lParam:
                            //   bits 0-15  = repeat count (1)
                            //   bit  30    = previous key state (1 on release)
                            //   bit  31    = transition state  (1 on release)
                            const LPARAM lp = isDown ? LPARAM(1)
                                                     : LPARAM(0xC0000001u);
                            const UINT msg = isDown ? WM_KEYDOWN : WM_KEYUP;
                            PostMessage(top, msg, vk, lp);
                        }
                    }
                }
               #endif
                complete(juce::var(true));
            })
        // ── Preset system ──────────────────────────────────────────────
        .withNativeFunction("getAllPresets",
            [&self](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                auto& pm = self.processor.getPresetManager();
                auto all = pm.getAllPresets();

                auto* root = new juce::DynamicObject();
                for (const auto& [packName, presets] : all)
                {
                    juce::Array<juce::var> arr;
                    for (const auto& p : presets)
                    {
                        auto* meta = new juce::DynamicObject();
                        meta->setProperty("name",        p.metadata.name);
                        meta->setProperty("type",        p.metadata.type);
                        meta->setProperty("designer",    p.metadata.designer);
                        meta->setProperty("description", p.metadata.description);
                        meta->setProperty("isFavorite",  p.metadata.isFavorite);
                        meta->setProperty("isFactory",   p.metadata.isFactory);
                        meta->setProperty("presetKind",
                            kaigen::phantom::presetKindToString(p.metadata.presetKind));

                        // Preview: 7 harmonic weights + crossover Hz + skip count
                        auto* preview = new juce::DynamicObject();
                        juce::Array<juce::var> hArr;
                        for (int i = 0; i < 7; ++i)
                            hArr.add(juce::var(p.preview.h[i]));
                        preview->setProperty("h",         juce::var(hArr));
                        preview->setProperty("crossover", juce::var(p.preview.crossover));
                        preview->setProperty("skip",      juce::var(p.preview.skip));

                        auto* item = new juce::DynamicObject();
                        item->setProperty("metadata", juce::var(meta));
                        item->setProperty("preview",  juce::var(preview));
                        arr.add(juce::var(item));
                    }
                    root->setProperty(packName, juce::var(arr));
                }

                complete(juce::JSON::toString(juce::var(root)));
            })
        .withNativeFunction("loadPreset",
            [&self](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                if (args.size() < 2)
                {
                    complete(juce::var(false));
                    return;
                }

                const auto presetName = args[0].toString();
                const auto packName   = args[1].toString();

                // APVTS mutation must run on the message thread.
                juce::MessageManager::callAsync(
                    [weakSelf = juce::Component::SafePointer<PhantomEditor>(&self), presetName, packName]
                    {
                        if (auto* ed = weakSelf.getComponent())
                            ed->processor.getPresetManager().loadPresetInto(
                                ed->processor.getABSlotManager(), presetName, packName);
                    });

                complete(juce::var(true));
            })
        .withNativeFunction("savePreset",
            [&self](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                if (args.size() < 1)
                {
                    complete(juce::var(juce::String{}));
                    return;
                }

                const auto name        = args[0].toString();
                const auto type        = args.size() > 1 ? args[1].toString() : juce::String("Experimental");
                const auto designer    = args.size() > 2 ? args[2].toString() : juce::String("User");
                const auto description = args.size() > 3 ? args[3].toString() : juce::String();
                const bool overwrite   = args.size() > 4 && args[4].isBool() && (bool) args[4];
                const auto kindStr     = args.size() > 5 ? args[5].toString() : juce::String("single");

                const auto kind = kaigen::phantom::presetKindFromString(kindStr);

                auto savedName = self.processor.getPresetManager().savePreset(
                    self.processor.apvts,
                    &self.processor.getABSlotManager(),
                    name, type, designer, description, kind, overwrite);
                complete(juce::var(savedName));
            })
        .withNativeFunction("setFavorite",
            [&self](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                if (args.size() < 3)
                {
                    complete(juce::var(false));
                    return;
                }

                const auto name    = args[0].toString();
                const auto pack    = args[1].toString();
                const bool isFav   = args[2].isBool() && (bool) args[2];

                self.processor.getPresetManager().setFavorite(name, pack, isFav);
                complete(juce::var(true));
            })
        .withNativeFunction("deletePreset",
            [&self](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                if (args.size() < 2)
                {
                    complete(juce::var(false));
                    return;
                }

                const auto name = args[0].toString();
                const auto pack = args[1].toString();

                const bool ok = self.processor.getPresetManager().deletePreset(name, pack);
                complete(juce::var(ok));
            })
        .withNativeFunction("getAllPacks",
            [&self](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                auto packs = self.processor.getPresetManager().getAllPacks();
                juce::Array<juce::var> arr;
                for (const auto& p : packs)
                {
                    auto* obj = new juce::DynamicObject();
                    obj->setProperty("name",         p.name);
                    obj->setProperty("displayName",  p.displayName);
                    obj->setProperty("description",  p.description);
                    obj->setProperty("designer",     p.designer);
                    obj->setProperty("hasCoverArt",  p.hasCoverArt);
                    obj->setProperty("presetCount",  p.presetCount);
                    arr.add(juce::var(obj));
                }
                complete(juce::JSON::toString(juce::var(arr)));
            })
        .withNativeFunction("abGetState",
            [&self](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                auto& ab = self.processor.getABSlotManager();

                juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                obj->setProperty("active",
                    ab.getActive() == kaigen::phantom::ABSlotManager::Slot::A ? "A" : "B");
                obj->setProperty("modifiedA",
                    ab.isModified(kaigen::phantom::ABSlotManager::Slot::A));
                obj->setProperty("modifiedB",
                    ab.isModified(kaigen::phantom::ABSlotManager::Slot::B));

                const bool identical =
                    ab.getSlot(kaigen::phantom::ABSlotManager::Slot::A).toXmlString() ==
                    ab.getSlot(kaigen::phantom::ABSlotManager::Slot::B).toXmlString();
                obj->setProperty("slotsIdentical", identical);
                obj->setProperty("includeDiscrete", ab.getIncludeDiscreteInSnap());

                complete(juce::var(obj.get()));
            })
        .withNativeFunction("abSnapTo",
            [&self](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                if (args.size() < 1) { complete(juce::var(false)); return; }
                const auto slotStr = args[0].toString();
                const auto target = (slotStr == "B") ? kaigen::phantom::ABSlotManager::Slot::B
                                                     : kaigen::phantom::ABSlotManager::Slot::A;

                juce::MessageManager::callAsync(
                    [weakSelf = juce::Component::SafePointer<PhantomEditor>(&self), target]
                    {
                        if (auto* ed = weakSelf.getComponent())
                            ed->processor.getABSlotManager().snapTo(target);
                    });
                complete(juce::var(true));
            })
        .withNativeFunction("abCopy",
            [&self](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                juce::MessageManager::callAsync(
                    [weakSelf = juce::Component::SafePointer<PhantomEditor>(&self)]
                    {
                        if (auto* ed = weakSelf.getComponent())
                        {
                            auto& ab = ed->processor.getABSlotManager();
                            const auto src  = ab.getActive();
                            const auto dest = (src == kaigen::phantom::ABSlotManager::Slot::A)
                                              ? kaigen::phantom::ABSlotManager::Slot::B
                                              : kaigen::phantom::ABSlotManager::Slot::A;
                            ab.copy(src, dest);
                        }
                    });
                complete(juce::var(true));
            })
        .withNativeFunction("abSetIncludeDiscrete",
            [&self](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                if (args.size() < 1) { complete(juce::var(false)); return; }
                const bool on = args[0].isBool() ? (bool) args[0] : (((int) args[0]) != 0);
                self.processor.getABSlotManager().setIncludeDiscreteInSnap(on);
                complete(juce::var(true));
            })
        .withResourceProvider([&self](const auto& url) { return self.getResource(url); });

    return options;
}

PhantomEditor::PhantomEditor(PhantomProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      webView(buildWebViewOptions(*this))
{
    // Plugin never wants keyboard focus by default; clicks on JUCE components
    // (the editor, the WebBrowserComponent wrapper) must not steal keyboard
    // focus from the DAW. The WebView's internal focus model is handled
    // separately in preset-system.js (mousedown preventDefault on non-inputs)
    // and the Win32 subclass in webViewFocusSubclass.
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
    webView.setWantsKeyboardFocus(false);
    webView.setMouseClickGrabsKeyboardFocus(false);
    setSize(1300, 820);
    addAndMakeVisible(webView);

    juce::MessageManager::callAsync([this]()
    {
        webView.goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
    });

    // ── Slider attachments ────────────────────────────────────────────
    struct SliderBinding { const char* paramId; juce::WebSliderRelay& relay; };
    SliderBinding sliderBindings[] = {
        { ParamID::INPUT_GAIN,          inputGainRelay },
        { ParamID::GHOST,               ghostRelay },
        { ParamID::PHANTOM_THRESHOLD,   phantomThresholdRelay },
        { ParamID::PHANTOM_STRENGTH,    phantomStrengthRelay },
        { ParamID::OUTPUT_GAIN,         outputGainRelay },
        { ParamID::RECIPE_H2,           recipeH2Relay },
        { ParamID::RECIPE_H3,           recipeH3Relay },
        { ParamID::RECIPE_H4,           recipeH4Relay },
        { ParamID::RECIPE_H5,           recipeH5Relay },
        { ParamID::RECIPE_H6,           recipeH6Relay },
        { ParamID::RECIPE_H7,           recipeH7Relay },
        { ParamID::RECIPE_H8,           recipeH8Relay },
        { ParamID::HARMONIC_SATURATION, harmonicSaturationRelay },
        { ParamID::SYNTH_STEP,          synthStepRelay },
        { ParamID::SYNTH_DUTY,          synthDutyRelay },
        { ParamID::SYNTH_SKIP,          synthSkipRelay },
        { ParamID::ENV_ATTACK_MS,       envAttackRelay },
        { ParamID::ENV_RELEASE_MS,      envReleaseRelay },
        { ParamID::BINAURAL_WIDTH,      binauralWidthRelay },
        { ParamID::STEREO_WIDTH,        stereoWidthRelay },
        { ParamID::SYNTH_LPF_HZ,            synthLPFRelay },
        { ParamID::SYNTH_HPF_HZ,            synthHPFRelay },
        { ParamID::SYNTH_WAVELET_LENGTH,    synthWaveletLengthRelay },
        { ParamID::SYNTH_GATE_THRESHOLD,    synthGateThresholdRelay },
        { ParamID::SYNTH_H1,                synthH1Relay },
        { ParamID::SYNTH_SUB,               synthSubRelay },
        { ParamID::SYNTH_MIN_SAMPLES,       synthMinSamplesRelay },
        { ParamID::SYNTH_MAX_SAMPLES,       synthMaxSamplesRelay },
        { ParamID::TRACKING_SPEED,          trackingSpeedRelay },
        { ParamID::PUNCH_AMOUNT,            punchAmountRelay },
        { ParamID::SYNTH_BOOST_THRESHOLD,   synthBoostThresholdRelay },
        { ParamID::SYNTH_BOOST_AMOUNT,      synthBoostAmountRelay },
    };
    for (auto& b : sliderBindings)
        sliderAttachments.push_back(std::make_unique<juce::WebSliderParameterAttachment>(
            *processor.apvts.getParameter(b.paramId), b.relay, nullptr));

    // ── Combo attachments ─────────────────────────────────────────────
    struct ComboBinding { const char* paramId; juce::WebComboBoxRelay& relay; };
    ComboBinding comboBindings[] = {
        { ParamID::MODE,          modeRelay },
        { ParamID::GHOST_MODE,    ghostModeRelay },
        { ParamID::RECIPE_PRESET, recipePresetRelay },
        { ParamID::BINAURAL_MODE,       binauralModeRelay },
        { ParamID::SYNTH_FILTER_SLOPE,  filterSlopeRelay  },
    };
    for (auto& b : comboBindings)
        comboAttachments.push_back(std::make_unique<juce::WebComboBoxParameterAttachment>(
            *processor.apvts.getParameter(b.paramId), b.relay, nullptr));

    // ── Toggle attachments ────────────────────────────────────────────
    bypassAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *processor.apvts.getParameter(ParamID::BYPASS), bypassRelay, nullptr);
    punchEnabledAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *processor.apvts.getParameter(ParamID::PUNCH_ENABLED), punchEnabledRelay, nullptr);
    inputGainAutoAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *processor.apvts.getParameter(ParamID::INPUT_GAIN_AUTO), inputGainAutoRelay, nullptr);
    midiTriggerAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *processor.apvts.getParameter(ParamID::MIDI_TRIGGER_ENABLED), midiTriggerRelay, nullptr);
    midiGateReleaseAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *processor.apvts.getParameter(ParamID::MIDI_GATE_RELEASE), midiGateReleaseRelay, nullptr);
}

PhantomEditor::~PhantomEditor() = default;

void PhantomEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff06060c));
}

void PhantomEditor::resized()
{
    webView.setBounds(getLocalBounds());
}

std::optional<juce::WebBrowserComponent::Resource> PhantomEditor::getResource(const juce::String& url)
{
    // Pack cover art served from disk: /pack-cover/<urlencoded packName>
    if (url.startsWith("/pack-cover/"))
    {
        auto packNameEnc = url.fromFirstOccurrenceOf("/pack-cover/", false, false);
        auto packName    = juce::URL::removeEscapeChars(packNameEnc);
        auto coverFile   = processor.getPresetManager().getPackCoverFile(packName);
        if (coverFile.existsAsFile())
        {
            juce::MemoryBlock mb;
            if (coverFile.loadFileAsData(mb))
            {
                std::vector<std::byte> bytes(
                    reinterpret_cast<const std::byte*>(mb.getData()),
                    reinterpret_cast<const std::byte*>(mb.getData()) + mb.getSize());
                const auto ext = coverFile.getFileExtension().substring(1).toLowerCase();
                return juce::WebBrowserComponent::Resource{
                    std::move(bytes),
                    juce::String(getMimeForExtension(ext))
                };
            }
        }
        return std::nullopt;
    }

    const auto urlToRetrieve = url == "/" ? juce::String{ "index.html" }
                                          : url.fromFirstOccurrenceOf("/", false, false);

    // JUCE BinaryData naming: dots → underscores, dashes STRIPPED
    auto resourceName = urlToRetrieve.replace(".", "_").replace("-", "").replace("/", "_");

    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        if (juce::String(BinaryData::namedResourceList[i]) == resourceName)
        {
            int size = 0;
            const auto* data = BinaryData::getNamedResource(BinaryData::namedResourceList[i], size);
            if (data != nullptr && size > 0)
            {
                auto extension = urlToRetrieve.fromLastOccurrenceOf(".", false, false);
                std::vector<std::byte> bytes(
                    reinterpret_cast<const std::byte*>(data),
                    reinterpret_cast<const std::byte*>(data) + size);
                return juce::WebBrowserComponent::Resource{
                    std::move(bytes),
                    juce::String(getMimeForExtension(extension))
                };
            }
        }
    }
    return std::nullopt;
}
