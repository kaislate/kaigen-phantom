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
    // 220px expansion gets clipped without growing the host window. Use
    // the existing setEditorHeight native binding (PluginEditor.cpp) to
    // resize the editor to match. Same pattern as advanced-mode in
    // phantom.js (which uses 820 → 1020 for its 200px panel).
    const BASE_HEIGHT     = 820;
    const DRAWER_EXPANDED = BASE_HEIGHT + 220;  // matches .modulation-drawer.is-open max-height
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
  }
})();
