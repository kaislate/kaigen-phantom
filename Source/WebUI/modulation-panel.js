// Source/WebUI/modulation-panel.js
//
// Bottom modulation panel controller. Owns the SLOTS/MATRIX mode toggle,
// matrix view persistence, header counter, and slot live-state rings.
// Macro slot clicks switch into MATRIX mode and highlight the row;
// LFO/Random/Morph slots are inert until later PRs.

(function () {
  'use strict';

  if (typeof window.Juce === 'undefined' || typeof window.Juce.getNativeFunction !== 'function') {
    document.addEventListener('DOMContentLoaded', init, { once: true });
    return;
  }
  init();

  function init() {
    const panel  = document.getElementById('modulation-panel');
    if (!panel) return;

    // ── SLOTS / MATRIX mode toggle ────────────────────────────────────────
    const matrix = document.getElementById('modulation-matrix');
    const modeBar = panel.querySelector('.modulation-mode-toggle');
    const BASE_HEIGHT     = 820;
    const MATRIX_EXPANDED = BASE_HEIGHT + 320;   // matrix needs more vertical room than the slot row alone
    let setEditorHeight = null;
    if (window.Juce && typeof window.Juce.getNativeFunction === 'function') {
      try { setEditorHeight = window.Juce.getNativeFunction('setEditorHeight'); }
      catch (e) { console.warn('[modulation-panel] setEditorHeight unavailable', e); }
    }

    // ── Persist matrix view state across sessions (Task 12) ───────────────
    // The C++ bindings hand back a Promise (Juce.getNativeFunction wrapper),
    // so the load is async; the save side fires-and-forgets. Both bindings
    // are no-ops if unavailable (e.g. running under a host where the
    // WebView bridge hasn't initialised yet).
    let matrixGetStateBinding = null;
    let matrixSetStateBinding = null;
    if (window.Juce && typeof window.Juce.getNativeFunction === 'function') {
      try { matrixGetStateBinding = window.Juce.getNativeFunction('matrixGetState'); }
      catch (e) { console.warn('[modulation-panel] matrixGetState unavailable', e); }
      try { matrixSetStateBinding = window.Juce.getNativeFunction('matrixSetState'); }
      catch (e) { console.warn('[modulation-panel] matrixSetState unavailable', e); }
    }

    function saveMatrixState() {
      if (!matrixSetStateBinding) return;
      try {
        const expA = (typeof window.kaigenGetMatrixExpanded === 'function')
                     ? window.kaigenGetMatrixExpanded('A') : '';
        const expB = (typeof window.kaigenGetMatrixExpanded === 'function')
                     ? window.kaigenGetMatrixExpanded('B') : '';
        matrixSetStateBinding({
          mode: currentMode === 'matrix' ? 'Matrix' : 'Slots',
          expandedA: expA,
          expandedB: expB,
        });
      } catch (e) { console.warn('[modulation-panel] matrixSetState failed', e); }
    }
    // Exposed so matrix.js's category-header click handler can call it
    // directly when the user toggles a category open/closed.
    window.kaigenSaveMatrixState = saveMatrixState;

    let currentMode = 'slots';
    function applyMode(mode) {
      currentMode = mode;
      panel.classList.toggle('is-matrix-mode', mode === 'matrix');
      if (matrix) {
        matrix.classList.toggle('is-open', mode === 'matrix');
        matrix.setAttribute('aria-hidden', mode === 'matrix' ? 'false' : 'true');
      }
      if (modeBar) {
        modeBar.querySelectorAll('.mode-btn').forEach(b => {
          const active = b.getAttribute('data-mode') === mode;
          b.classList.toggle('is-active', active);
          b.setAttribute('aria-selected', active ? 'true' : 'false');
        });
      }
      if (setEditorHeight) {
        try { setEditorHeight(mode === 'matrix' ? MATRIX_EXPANDED : BASE_HEIGHT); }
        catch (e) {}
      }
      // Tell the matrix to (re)render now that it's visible.
      if (mode === 'matrix' && typeof window.kaigenRenderMatrix === 'function') {
        window.kaigenRenderMatrix();
      }
      // Persist on every mode change. Category toggles call saveMatrixState
      // directly via window.kaigenSaveMatrixState (see matrix.js).
      saveMatrixState();
    }
    if (modeBar) {
      modeBar.querySelectorAll('.mode-btn').forEach(btn => {
        btn.addEventListener('click', () => {
          const m = btn.getAttribute('data-mode');
          if (m === 'slots' || m === 'matrix') applyMode(m);
        });
      });
    }
    window.kaigenSetModulationMode = applyMode;

    // Load persisted state on init: restore the per-engine expanded categories
    // BEFORE switching mode, so the first render in Matrix mode (triggered
    // by applyMode → kaigenRenderMatrix) reflects the saved expansion.
    if (matrixGetStateBinding) {
      try {
        const result = matrixGetStateBinding();
        if (result && typeof result.then === 'function') {
          result.then(s => {
            if (!s) return;
            if (typeof window.kaigenSetMatrixExpanded === 'function') {
              if (s.expandedA != null) window.kaigenSetMatrixExpanded('A', s.expandedA);
              if (s.expandedB != null) window.kaigenSetMatrixExpanded('B', s.expandedB);
            }
            if (s.mode === 'Matrix') applyMode('matrix');
          }).catch(e => console.warn('[modulation-panel] matrixGetState rejected', e));
        }
      } catch (e) { console.warn('[modulation-panel] matrixGetState failed', e); }
    }

    // Wire slot click handlers. For macros, switch to MATRIX mode and
    // briefly highlight the matching row (Task 13). LFO/Random/Morph
    // slots remain inert in this PR.
    panel.querySelectorAll('.mod-slot').forEach(btn => {
      if (btn.disabled) return;
      btn.addEventListener('click', () => {
        const id   = btn.getAttribute('data-slot-id');
        const type = btn.getAttribute('data-slot-type');

        if (type === 'macro') {
          // Engine-focus auto-switch. Macros 1+2 are engine A; macros 3+4
          // are engine B. The user editing macro 3's routings sees engine
          // B's eligible params, so the surrounding plugin UI should also
          // be on engine B.
          const wantTab = (id === 'macro3' || id === 'macro4') ? 'B' : 'A';
          if (window.__kaigenActiveTab !== wantTab) {
            window.__kaigenActiveTab = wantTab;
            let engineSetFocus = null;
            if (window.Juce && typeof window.Juce.getNativeFunction === 'function') {
              try { engineSetFocus = window.Juce.getNativeFunction('engineSetFocus'); }
              catch (e) { /* binding unavailable; only update JS state */ }
            }
            if (engineSetFocus) {
              try {
                engineSetFocus({ activeTab: wantTab, linkOn: !!window.__kaigenLinkOn });
              } catch (e) { console.warn('[modulation-panel] engineSetFocus failed', e); }
            }
            const tabA = document.getElementById('engine-tab-a');
            const tabB = document.getElementById('engine-tab-b');
            if (tabA && tabB) {
              tabA.classList.toggle('is-active', wantTab === 'A');
              tabA.setAttribute('aria-selected', wantTab === 'A' ? 'true' : 'false');
              tabB.classList.toggle('is-active', wantTab === 'B');
              tabB.setAttribute('aria-selected', wantTab === 'B' ? 'true' : 'false');
            }
            if (window.Juce && typeof window.Juce.broadcastKaigenTabChanged === 'function') {
              window.Juce.broadcastKaigenTabChanged();
            }
          }
          // Switch to MATRIX mode and highlight this row.
          applyMode('matrix');
          if (typeof window.kaigenHighlightMatrixRow === 'function') {
            window.kaigenHighlightMatrixRow(id);
          }
        }
        // LFO/Random/Morph slots: inert in this PR (PR4/PR5 wire LFO/Random).
      });
    });

    // Live ring updates for macro + morph slots. Each event carries
    // { macros: { macro1: { value }, ... }, morph_amount: <number> } from the
    // C++ modulationGetLiveState binding. Per the spec, only macro and morph
    // get live rings in this PR; LFO + Random rings are PR4/PR5.

    // Cache ring elements once. The slot DOM is built at HTML parse time and
    // never recreated, so per-tick querySelectors are wasteful — also
    // establishes the cache-at-init pattern for matrix-view's many-cell case.
    const ringEls = {
      macro1: panel.querySelector('.mod-slot[data-slot-id="macro1"] .mod-slot-ring'),
      macro2: panel.querySelector('.mod-slot[data-slot-id="macro2"] .mod-slot-ring'),
      macro3: panel.querySelector('.mod-slot[data-slot-id="macro3"] .mod-slot-ring'),
      macro4: panel.querySelector('.mod-slot[data-slot-id="macro4"] .mod-slot-ring'),
      morph:  panel.querySelector('.mod-slot[data-slot-id="morph"]  .mod-slot-ring'),
    };

    document.body.addEventListener('kaigen:live-state', (ev) => {
      const detail = ev.detail || {};
      const macros = detail.macros || {};
      for (const id of ['macro1', 'macro2', 'macro3', 'macro4']) {
        const ring = ringEls[id];
        if (!ring) continue;
        const v = (macros[id] && typeof macros[id].value === 'number') ? macros[id].value : 0;
        ring.style.setProperty('--v', String(Math.max(0, Math.min(1, v)) * 100));
      }
      if (ringEls.morph) {
        const m = (typeof detail.morph_amount === 'number') ? detail.morph_amount : 0;
        ringEls.morph.style.setProperty('--v', String(Math.max(0, Math.min(1, m)) * 100));
      }
    });

    // ── Header counter (routings + active macros) ─────────────────────────
    let modulationGetStateBinding = null;
    if (window.Juce && typeof window.Juce.getNativeFunction === 'function') {
      try { modulationGetStateBinding = window.Juce.getNativeFunction('modulationGetState'); }
      catch (e) { console.warn('[modulation-panel] modulationGetState unavailable for counter', e); }
    }

    async function updateCounter() {
      const counter = document.getElementById('modulation-counter');
      if (!counter) return;
      if (!modulationGetStateBinding) {
        counter.textContent = '— ROUTINGS';
        return;
      }
      let totalRoutings = 0;
      let activeMacros = 0;
      try {
        const s = await modulationGetStateBinding();
        if (s) {
          for (const engId of ['engineA', 'engineB']) {
            const eng = s[engId];
            if (eng && Array.isArray(eng.routings)) {
              totalRoutings += eng.routings.length;
              const seenMacros = new Set();
              for (const r of eng.routings) {
                if (r.source && r.source.startsWith('macro') && !seenMacros.has(r.source)) {
                  activeMacros++;
                  seenMacros.add(r.source);
                }
              }
            }
          }
        }
      } catch (e) { console.warn('[modulation-panel] updateCounter failed', e); }
      counter.textContent = `${totalRoutings} ROUTING${totalRoutings === 1 ? '' : 'S'}` +
                            (activeMacros > 0 ? ` · ${activeMacros} MACRO${activeMacros === 1 ? '' : 'S'} ACTIVE` : '');
    }
    document.body.addEventListener('modulationStateChanged', updateCounter);
    // Some platforms route C++→JS events through __JUCE__.backend instead of
    // CustomEvent on document.body; subscribe to both for parity with matrix.js.
    if (window.__JUCE__ && window.__JUCE__.backend && typeof window.__JUCE__.backend.addEventListener === 'function') {
      try { window.__JUCE__.backend.addEventListener('modulationStateChanged', updateCounter); }
      catch (e) {}
    }
    updateCounter();
  }
})();
