// Source/WebUI/modulation-panel.js
//
// Bottom modulation panel controller. Tracks which slot (if any) is
// currently expanded in the drawer. Click handlers on macro slots
// toggle the drawer + load the macro editor. Other slot types are
// inert until PR4 (LFO) and PR5 (Random) wire them.

(function () {
  'use strict';

  if (typeof window.Juce === 'undefined' || typeof window.Juce.getNativeFunction !== 'function') {
    document.addEventListener('DOMContentLoaded', init, { once: true });
    return;
  }
  init();

  function init() {
    const panel  = document.getElementById('modulation-panel');
    const drawer = document.getElementById('modulation-drawer');
    if (!panel || !drawer) return;

    let activeSlot = null;   // string id or null

    // .wrap is overflow:hidden with a fixed 820px height. The drawer's
    // 160px expansion gets clipped without growing the host window. Use
    // the existing setEditorHeight native binding (PluginEditor.cpp) to
    // resize the editor to match. Same pattern as advanced-mode in
    // phantom.js (which uses 820 → 1020 for its 200px panel).
    const BASE_HEIGHT     = 820;
    const DRAWER_EXPANDED = BASE_HEIGHT + 160;  // matches .modulation-drawer.is-open max-height
    const DRAWER_TRANSITION_MS = 220;
    let setEditorHeight = null;
    if (window.Juce && typeof window.Juce.getNativeFunction === 'function') {
      try { setEditorHeight = window.Juce.getNativeFunction('setEditorHeight'); }
      catch (e) { console.warn('[modulation-panel] setEditorHeight unavailable', e); }
    }
    function growEditor()   { if (setEditorHeight) try { setEditorHeight(DRAWER_EXPANDED); } catch (e) {} }
    function shrinkEditor() {
      // Shrink only after the CSS transition finishes — otherwise the
      // window snaps small while the drawer is still mid-collapse and
      // the bottom row gets clipped.
      if (!setEditorHeight) return;
      setTimeout(() => { try { setEditorHeight(BASE_HEIGHT); } catch (e) {} }, DRAWER_TRANSITION_MS + 20);
    }

    function closeDrawer() {
      drawer.classList.remove('is-open');
      drawer.setAttribute('aria-hidden', 'true');
      drawer.replaceChildren();
      if (activeSlot) {
        const prev = panel.querySelector(`.mod-slot[data-slot-id="${activeSlot}"]`);
        if (prev) prev.classList.remove('is-active');
      }
      activeSlot = null;
      shrinkEditor();
    }

    function openSlot(slotId, slotType) {
      // Only macros work in PR3b.
      if (slotType !== 'macro') return;

      // Toggle: clicking the active slot closes the drawer.
      if (activeSlot === slotId) {
        closeDrawer();
        return;
      }

      // Auto-switch engine focus to match the macro's scope. Macros 1+2 are
      // engine A; macros 3+4 are engine B. The user editing macro 3's
      // routings sees engine B's eligible params, so the surrounding plugin
      // UI should also be on engine B — otherwise the visible knobs are
      // from a different engine than the picker is offering.
      const wantTab = (slotId === 'macro3' || slotId === 'macro4') ? 'B' : 'A';
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
        if (window.Juce && typeof window.Juce.broadcastKaigenTabChanged === 'function') {
          window.Juce.broadcastKaigenTabChanged();
        }
        // Mirror phantom.js's applyToUI for the visible A | B | LINK buttons
        // — phantom.js only updates them on its own click handlers, so we
        // have to keep them in sync ourselves here.
        const tabA = document.getElementById('engine-tab-a');
        const tabB = document.getElementById('engine-tab-b');
        if (tabA && tabB) {
          tabA.classList.toggle('is-active', wantTab === 'A');
          tabA.setAttribute('aria-selected', wantTab === 'A' ? 'true' : 'false');
          tabB.classList.toggle('is-active', wantTab === 'B');
          tabB.setAttribute('aria-selected', wantTab === 'B' ? 'true' : 'false');
        }
      }

      // First-time open (no active slot yet): grow the editor before the
      // CSS transition starts so the new vertical space is already there
      // when max-height animates up. Re-opens (different slot) skip the
      // grow because the editor is already tall enough.
      const wasClosed = (activeSlot === null);

      // Reassign active highlighting.
      if (activeSlot) {
        const prev = panel.querySelector(`.mod-slot[data-slot-id="${activeSlot}"]`);
        if (prev) prev.classList.remove('is-active');
      }
      const next = panel.querySelector(`.mod-slot[data-slot-id="${slotId}"]`);
      if (next) next.classList.add('is-active');
      activeSlot = slotId;

      // Render the macro editor into the drawer.
      drawer.replaceChildren();
      if (typeof window.kaigenRenderMacroEditor === 'function') {
        window.kaigenRenderMacroEditor(drawer, slotId);
      }
      drawer.classList.add('is-open');
      drawer.setAttribute('aria-hidden', 'false');

      if (wasClosed) growEditor();
    }

    // Wire slot click handlers.
    panel.querySelectorAll('.mod-slot').forEach(btn => {
      if (btn.disabled) return;
      btn.addEventListener('click', () => {
        const id   = btn.getAttribute('data-slot-id');
        const type = btn.getAttribute('data-slot-type');
        openSlot(id, type);
      });
    });

    // Expose for macro editor's "close" button.
    window.kaigenCloseModulationDrawer = closeDrawer;

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
  }
})();
