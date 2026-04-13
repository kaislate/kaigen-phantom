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
    // Active = lit = plugin is ON (not bypassed). Invert the bypass value.
    bypassBtn.classList.toggle("active", !bypassState.getValue());
  }
  bypassState.valueChangedEvent.addListener(updateBypassUI);
  updateBypassUI();
}

// ── Punch toggle ──────────────────────────────────────────────────────────────
const punchEnabledState = getToggleState?.("punch_enabled");
const punchBtn = document.getElementById("punch-btn");
if (punchBtn && punchEnabledState) {
  punchBtn.addEventListener("click", () => {
    punchEnabledState.setValue(!punchEnabledState.getValue());
  });
  function updatePunchUI() {
    punchBtn.classList.toggle("active", punchEnabledState.getValue());
    const amountKnob = document.querySelector('phantom-knob[data-param="punch_amount"]');
    if (amountKnob) amountKnob.classList.toggle('mode-inactive', !punchEnabledState.getValue());
  }
  punchEnabledState.valueChangedEvent.addListener(updatePunchUI);
  updatePunchUI();
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
// 3. Wire H2-H8 knobs → recipe wheel (live amplitude update)
// =============================================================================

const harmonicParamIds = [
  'recipe_h2','recipe_h3','recipe_h4','recipe_h5','recipe_h6','recipe_h7','recipe_h8'
];

function updateWheelAmplitudes() {
  if (!window.PhantomRecipeWheel) return;
  const amps = harmonicParamIds.map(id => {
    const state = getSliderState(id);
    return state ? state.getNormalisedValue() : 0;
  });
  window.PhantomRecipeWheel.setAmplitudes(amps);
}

harmonicParamIds.forEach(id => {
  const state = getSliderState(id);
  if (state) state.valueChangedEvent.addListener(updateWheelAmplitudes);
});
updateWheelAmplitudes();

// Route spoke drag events back to JUCE parameters
document.addEventListener('spoke-change', e => {
  const { index, value } = e.detail;
  const id = harmonicParamIds[index];
  if (!id) return;
  const state = getSliderState(id);
  if (!state) return;
  // Snap to Custom preset (index 6) when user manually drags a spoke
  presetState.setChoiceIndex(6);
  state.sliderDragStarted();
  state.setNormalisedValue(value);
  state.sliderDragEnded();
});

// =============================================================================
// 4. Wire mode toggle (Effect / Instrument)
// =============================================================================

const modeState = getComboBoxState("mode");

function applyMode(idx) {
  document
    .querySelectorAll(".mb[data-mode]")
    .forEach((b) =>
      b.classList.toggle("active", parseInt(b.dataset.mode) === idx)
    );
  // Dim RESYN-only controls when in Effect mode (idx=0)
  const resynOnly = ['synth_h1', 'synth_wavelet_length', 'synth_gate_threshold'];
  const isEffect  = idx === 0;
  resynOnly.forEach(param => {
    const el = document.querySelector(`phantom-knob[data-param="${param}"]`);
    if (el) el.classList.toggle('mode-inactive', isEffect);
  });
}

document.querySelectorAll(".mb[data-mode]").forEach((btn) => {
  btn.addEventListener("click", () => {
    const idx = parseInt(btn.dataset.mode);
    applyMode(idx);
    modeState.setChoiceIndex(idx);
  });
});

modeState.valueChangedEvent.addListener(() => {
  applyMode(modeState.getChoiceIndex());
});
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

// =============================================================================
// 10. Wire envelope source select
// =============================================================================

const envSourceSelect = document.getElementById("env-source-select");
if (envSourceSelect) {
  const envSourceState = getComboBoxState?.("env_source");
  if (envSourceState) {
    envSourceSelect.addEventListener("change", () => {
      envSourceState.setChoiceIndex(parseInt(envSourceSelect.value));
    });
    envSourceState.valueChangedEvent?.addListener(() => {
      envSourceSelect.value = String(envSourceState.getChoiceIndex());
    });
    envSourceSelect.value = String(envSourceState.getChoiceIndex());
  }
}

})();
