// morph.js — Pro morph module
(function () {
  'use strict';

  if (typeof window.Juce === 'undefined' || typeof window.Juce.getNativeFunction !== 'function') {
    // JUCE bridge not yet ready — retry on DOMContentLoaded if not already.
    document.addEventListener('DOMContentLoaded', init, { once: true });
    return;
  }
  init();

  function init() {
    const native = {
      morphGetState:                window.Juce.getNativeFunction('morphGetState'),
      morphSetEnabled:              window.Juce.getNativeFunction('morphSetEnabled'),
      morphSetSceneEnabled:         window.Juce.getNativeFunction('morphSetSceneEnabled'),
      morphGetArcDepths:            window.Juce.getNativeFunction('morphGetArcDepths'),
      morphSetArcDepth:             window.Juce.getNativeFunction('morphSetArcDepth'),
      morphBeginCapture:            window.Juce.getNativeFunction('morphBeginCapture'),
      morphEndCapture:              window.Juce.getNativeFunction('morphEndCapture'),
      morphGetContinuousParamIDs:   window.Juce.getNativeFunction('morphGetContinuousParamIDs'),
    };

    if (typeof native.morphGetState !== 'function') {
      // Standard build — silently do nothing.
      return;
    }

    console.log('[morph] Pro build, initializing module');

    const el = {
      panel:       document.getElementById('mod-panel'),
      enable:      document.getElementById('mod-enable'),
      slider:      document.getElementById('mod-slider'),
      value:       document.getElementById('mod-value'),
      armed:       document.getElementById('mod-armed'),
      captureBtn:  document.getElementById('mod-capture-btn'),
      cancelBtn:   document.getElementById('mod-cancel-btn'),
      sceneRow:    document.getElementById('mod-row-scene'),
      sceneSlider: document.getElementById('mod-scene-slider'),
      sceneValue:  document.getElementById('mod-scene-value'),
    };

    if (!el.panel) { console.warn('[morph] modulation panel DOM not found'); return; }

    // Show the panel now that we've confirmed Pro.
    el.panel.style.display = '';

    // Signal Pro to other modules (e.g. save modal) and un-hide A/B + Morph option.
    window.__morphProEnabled = true;
    const abmWrap = document.getElementById('save-kind-abm-wrap');
    if (abmWrap) abmWrap.style.display = '';

    // Morph settings section — only shown in Pro
    const morphSettings = document.getElementById('settings-morph-section');
    if (morphSettings) {
      morphSettings.style.display = '';
    }
    const sceneToggle = document.getElementById('setting-morph-scene-enabled');
    if (sceneToggle) {
      sceneToggle.addEventListener('change', async (e) => {
        await native.morphSetSceneEnabled(e.target.checked);
        await refreshState();
      });
      // Sync initial state once state is loaded (below).
    }

    const state = {
      enabled: false,
      morphAmount: 0,
      sceneEnabled: false,
      scenePosition: 0,
      armedCount: 0,
      inCapture: false,
      arcDepths: {},
      continuousIDs: [],
    };

    // ── Rendering ──────────────────────────────────────────────────────────
    function render() {
      el.enable.classList.toggle('on', state.enabled);
      el.armed.textContent = `${state.armedCount} armed`;

      // Morph slider fill + handle position
      const morphPct = state.morphAmount * 100;
      el.slider.querySelector('.mod-slider-fill').style.width = morphPct + '%';
      el.slider.querySelector('.mod-slider-handle').style.left = morphPct + '%';
      el.value.textContent = state.morphAmount.toFixed(2);

      // Scene row
      if (state.sceneEnabled) {
        el.sceneRow.style.display = '';
        const scenePct = state.scenePosition * 100;
        el.sceneSlider.querySelector('.mod-slider-fill').style.width = scenePct + '%';
        el.sceneSlider.querySelector('.mod-slider-handle').style.left = scenePct + '%';
        el.sceneValue.textContent = state.scenePosition.toFixed(2);
      } else {
        el.sceneRow.style.display = 'none';
      }

      // Capture state
      el.captureBtn.classList.toggle('capture-active', state.inCapture);
      el.captureBtn.textContent = state.inCapture ? 'COMMIT' : 'CAPTURE';
      el.cancelBtn.style.display = state.inCapture ? '' : 'none';
      document.body.classList.toggle('mod-capture-active', state.inCapture);
    }

    async function refreshState() {
      try {
        const s = await native.morphGetState();
        Object.assign(state, s);
        state.arcDepths = await native.morphGetArcDepths();
        render();
        renderKnobRings();
      } catch (err) {
        console.error('[morph] refreshState failed', err);
      }
    }

    // ── Event wiring ───────────────────────────────────────────────────────
    el.enable.addEventListener('click', async () => {
      await native.morphSetEnabled(!state.enabled);
      await refreshState();
    });

    el.captureBtn.addEventListener('click', async () => {
      if (state.inCapture) {
        const modified = await native.morphEndCapture(true);
        console.log('[morph] committed arcs for', modified);
      } else {
        await native.morphBeginCapture();
      }
      await refreshState();
    });

    el.cancelBtn.addEventListener('click', async () => {
      await native.morphEndCapture(false);
      await refreshState();
    });

    // Escape key cancels capture
    document.addEventListener('keydown', async (e) => {
      if (e.key === 'Escape' && state.inCapture) {
        await native.morphEndCapture(false);
        await refreshState();
      }
    });

    // Slider interaction (drag to set morph amount via the APVTS param)
    function wireSlider(sliderEl, paramID) {
      let dragging = false;
      const setFromClientX = (clientX) => {
        const rect = sliderEl.getBoundingClientRect();
        const norm = Math.max(0, Math.min(1, (clientX - rect.left) / rect.width));
        const relay = window.Juce.getSliderState(paramID);
        if (relay) relay.setNormalisedValue(norm);
      };

      sliderEl.addEventListener('pointerdown', (e) => {
        dragging = true;
        sliderEl.setPointerCapture(e.pointerId);
        setFromClientX(e.clientX);
      });
      sliderEl.addEventListener('pointermove', (e) => { if (dragging) setFromClientX(e.clientX); });
      sliderEl.addEventListener('pointerup', (e) => {
        dragging = false;
        sliderEl.releasePointerCapture(e.pointerId);
      });
    }
    wireSlider(el.slider,      'morph_amount');
    wireSlider(el.sceneSlider, 'scene_position');

    // ── Knob ring rendering ────────────────────────────────────────────────
    function renderKnobRings() {
      document.querySelectorAll('phantom-knob').forEach(knob => {
        const paramID = knob.getAttribute('data-param');
        if (!paramID) return;

        const relay = window.Juce.getSliderState(paramID);
        const liveValue = relay ? relay.getNormalisedValue() : 0;
        const arcDepth = state.arcDepths[paramID] || 0;

        // For the ring, baseValue = (liveValue - arcDepth * morph), i.e. reverse-derive
        // the base so the visual is consistent with the live pointer animation.
        let baseValue = liveValue;
        if (Math.abs(arcDepth) > 1e-4 && state.morphAmount > 1e-4) {
          baseValue = Math.max(0, Math.min(1, liveValue - arcDepth * state.morphAmount));
        }

        knob.setMorphState({
          enabled: state.enabled,
          baseValue, arcDepth, liveValue,
          morph: state.morphAmount,
        });
      });
    }

    // Arc handle drag: listen on shadow roots of each phantom-knob.
    document.addEventListener('pointerdown', async (e) => {
      const target = e.composedPath()[0];
      if (!(target instanceof Element) || !target.classList || !target.classList.contains('morph-arc-handle')) return;

      const paramID = target.dataset.paramID;
      if (!paramID) return;

      e.preventDefault();
      e.stopPropagation();

      const knobEl = target.getRootNode().host;    // the phantom-knob element
      const rect = knobEl.getBoundingClientRect();
      const cx = rect.left + rect.width / 2;
      const cy = rect.top + rect.height / 2;

      function anglesToDepth(clientX, clientY) {
        const dx = clientX - cx;
        const dy = clientY - cy;
        const angleDeg = (Math.atan2(dy, dx) * 180 / Math.PI + 360) % 360;
        // Map angle ∈ [135, 405] (wrap past 360 into 45) to normalized 0..1.
        let rel = angleDeg;
        if (rel < 135) rel += 360;
        const norm = Math.max(0, Math.min(1, (rel - 135) / 270));
        return norm;
      }

      function onMove(ev) {
        const targetNorm = anglesToDepth(ev.clientX, ev.clientY);
        const relay = window.Juce.getSliderState(paramID);
        const liveNorm = relay ? relay.getNormalisedValue() : 0;
        const morphVal = state.morphAmount || 0;
        const currentDepth = state.arcDepths[paramID] || 0;
        const impliedBase = liveNorm - currentDepth * morphVal;
        const newDepth = Math.max(-1, Math.min(1, targetNorm - impliedBase));
        native.morphSetArcDepth(paramID, newDepth);
      }
      function onUp() {
        document.removeEventListener('pointermove', onMove);
        document.removeEventListener('pointerup',   onUp);
      }
      document.addEventListener('pointermove', onMove);
      document.addEventListener('pointerup',   onUp);
    });

    // ── Initial fetch ──────────────────────────────────────────────────────
    (async () => {
      try {
        state.continuousIDs = await native.morphGetContinuousParamIDs();
        await refreshState();

        // Sync settings checkbox with initial state
        const sceneCheckbox = document.getElementById('setting-morph-scene-enabled');
        if (sceneCheckbox) sceneCheckbox.checked = !!state.sceneEnabled;

        // 30fps polling with backpressure
        let inFlight = false;
        setInterval(async () => {
          if (inFlight) return;
          inFlight = true;
          try { await refreshState(); } finally { inFlight = false; }
        }, 33);
      } catch (err) {
        console.error('[morph] init failed', err);
      }
    })();
  }
})();
