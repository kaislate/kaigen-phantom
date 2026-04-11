// phantom.js — central JS bridge connecting Phantom WebView UI to C++ backend
// Regular script (not a module) — depends on window.Juce from juce-frontend.js

(function(){
if (!window.Juce) {
  console.error("phantom.js: window.Juce not defined");
  return;
}

const getSliderState   = window.Juce.getSliderState;
const getComboBoxState = window.Juce.getComboBoxState;
const getToggleState   = window.Juce.getToggleState;
const getNativeFunction = window.Juce.getNativeFunction;

// ── Bypass toggle ─────────────────────────────────────────────────────────
const bypassState = getToggleState?.("bypass");
const bypassBtn = document.getElementById("bypass-btn");
if (bypassBtn && bypassState) {
  bypassBtn.addEventListener("click", () => {
    bypassState.setValue(!bypassState.getValue());
  });
  function updateBypassUI() {
    bypassBtn.classList.toggle("active", !!bypassState.getValue());
  }
  bypassState.valueChangedEvent.addListener(updateBypassUI);
  updateBypassUI();
}

// =============================================================================
// 1. Display value formatting
// =============================================================================

function formatDisplayValue(state) {
  const val = state.getScaledValue();
  const props = state.properties;
  const label = (props.label || "").trim();

  // Integer parameters (e.g., max_voices)
  if (props.interval >= 1) {
    return `${Math.round(val)}${label ? " " + label : ""}`;
  }

  // Format based on label suffix
  if (label === "%") {
    return `${Math.round(val)}%`;
  }
  if (label === "dB") {
    return val >= 0 ? `+${val.toFixed(1)} dB` : `${val.toFixed(1)} dB`;
  }
  if (label === "Hz") {
    return val >= 1000 ? `${(val / 1000).toFixed(1)}kHz` : `${Math.round(val)}Hz`;
  }
  if (label === "ms") {
    return val >= 1000 ? `${(val / 1000).toFixed(2)}s` : `${Math.round(val)}ms`;
  }
  if (label === "st") {
    return `${val.toFixed(1)} st`;
  }

  // Fallback: 0-1 normalised range shown as percentage
  if (props.start === 0 && props.end === 1 && !label) {
    return `${Math.round(val * 100)}%`;
  }

  // Generic fallback
  const decimals = Math.abs(val) < 10 ? 2 : Math.abs(val) < 100 ? 1 : 0;
  return `${val.toFixed(decimals)}${label ? " " + label : ""}`;
}

// =============================================================================
// 2. Wire all phantom-knob elements to their slider relays
// =============================================================================

document.querySelectorAll("phantom-knob[data-param]").forEach((el) => {
  const paramName = el.dataset.param;
  const state = getSliderState(paramName);

  function updateKnob() {
    el.value = state.getNormalisedValue();
    el.displayValue = formatDisplayValue(state);
  }

  // Listen for backend changes
  state.valueChangedEvent.addListener(updateKnob);
  state.propertiesChangedEvent.addListener(updateKnob);

  // Listen for user interaction on the knob
  el.addEventListener("knob-change", (e) => {
    state.sliderDragStarted();
    state.setNormalisedValue(e.detail.value);
    state.sliderDragEnded();
  });

  // Initialize with current value
  updateKnob();
});

// =============================================================================
// 3. Wire mode toggle (Effect / Instrument)
// =============================================================================

const modeState = getComboBoxState("mode");

function applyMode(idx) {
  document
    .querySelectorAll(".mb[data-mode]")
    .forEach((b) =>
      b.classList.toggle("active", parseInt(b.dataset.mode) === idx)
    );
  document.getElementById("pitchPanel")?.classList.toggle("hidden", idx !== 0);
  document.getElementById("deconPanel")?.classList.toggle("hidden", idx === 0);
}

document.querySelectorAll(".mb[data-mode]").forEach((btn) => {
  btn.addEventListener("click", () => {
    const idx = parseInt(btn.dataset.mode);
    applyMode(idx);                     // update UI immediately
    modeState.setChoiceIndex(idx);      // send to backend
  });
});

modeState.valueChangedEvent.addListener(() => {
  applyMode(modeState.getChoiceIndex());
});
// Initialize visibility
applyMode(modeState.getChoiceIndex() || 0);

// =============================================================================
// 4. Wire preset strip
// =============================================================================

const presetState = getComboBoxState("recipe_preset");

document.querySelectorAll(".lw[data-preset]").forEach((btn) => {
  btn.addEventListener("click", () => {
    presetState.setChoiceIndex(parseInt(btn.dataset.preset));
  });
});

function updatePresetUI() {
  const idx = presetState.getChoiceIndex();
  document
    .querySelectorAll(".lw[data-preset]")
    .forEach((b) =>
      b.classList.toggle("active", parseInt(b.dataset.preset) === idx)
    );
}

presetState.valueChangedEvent.addListener(updatePresetUI);
updatePresetUI();

// =============================================================================
// 5. Wire ghost mode toggle (Replace / Additive)
// =============================================================================

const ghostModeState = getComboBoxState("ghost_mode");

document.querySelectorAll(".tog[data-gmode]").forEach((btn) => {
  btn.addEventListener("click", () => {
    ghostModeState.setChoiceIndex(parseInt(btn.dataset.gmode));
  });
});

function updateGhostModeUI() {
  const idx = ghostModeState.getChoiceIndex();
  document
    .querySelectorAll(".tog[data-gmode]")
    .forEach((b) =>
      b.classList.toggle("active", parseInt(b.dataset.gmode) === idx)
    );
}

ghostModeState.valueChangedEvent.addListener(updateGhostModeUI);
updateGhostModeUI();

// =============================================================================
// 6. Wire deconfliction mode select
// =============================================================================

const deconModeState = getComboBoxState("deconfliction_mode");
const deconSelect = document.getElementById("decon-mode");

if (deconSelect) {
  deconSelect.addEventListener("change", () => {
    deconModeState.setChoiceIndex(deconSelect.selectedIndex);
  });

  function updateDeconUI() {
    deconSelect.selectedIndex = deconModeState.getChoiceIndex();
  }

  deconModeState.valueChangedEvent.addListener(updateDeconUI);
  updateDeconUI();
}

// =============================================================================
// 7. Real-time data polling loop
// =============================================================================

const getSpectrum = getNativeFunction("getSpectrumData");
const getPeaks = getNativeFunction("getPeakLevels");
const getPitch = getNativeFunction("getPitchInfo");

function pollData() {
  requestAnimationFrame(pollData);

  getSpectrum().then((bins) => {
    document.dispatchEvent(
      new CustomEvent("spectrum-data", { detail: bins })
    );
  });

  getPeaks().then((p) => {
    document.dispatchEvent(new CustomEvent("peak-data", { detail: p }));
  });

  getPitch().then((p) => {
    const pitchDisplay = document.getElementById("pitchDisplay");
    const presetDisplay = document.getElementById("presetDisplay");

    if (pitchDisplay) {
      pitchDisplay.textContent =
        p && p.hz > 0 ? `${p.note} \u00B7 ${Math.round(p.hz)}Hz` : "---";
    }
    if (presetDisplay && p && p.preset) {
      presetDisplay.textContent = p.preset;
    }

    document.dispatchEvent(new CustomEvent("pitch-data", { detail: p }));
  });
}

pollData();

// =============================================================================
// 8. Settings overlay toggle
// =============================================================================

document.getElementById("settings-btn")?.addEventListener("click", () => {
  document.getElementById("settings-overlay")?.classList.toggle("hidden");
});
document.getElementById("settings-close")?.addEventListener("click", () => {
  document.getElementById("settings-overlay")?.classList.add("hidden");
});
document.querySelector(".settings-backdrop")?.addEventListener("click", () => {
  document.getElementById("settings-overlay")?.classList.add("hidden");
});

// =============================================================================
// 9. Wire binaural mode select
// =============================================================================

const binauralSelect = document.getElementById("binaural-mode-select");
if (binauralSelect) {
  const binauralState = getComboBoxState?.("binaural_mode");
  if (binauralState) {
    binauralSelect.addEventListener("change", () => {
      binauralState.setChoiceIndex(parseInt(binauralSelect.value));
    });
    binauralState.valueChangedEvent?.addListener(() => {
      binauralSelect.value = String(binauralState.getChoiceIndex());
    });
  }
}

})();
