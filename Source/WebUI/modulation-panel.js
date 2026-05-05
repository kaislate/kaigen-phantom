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

    function closeDrawer() {
      drawer.classList.remove('is-open');
      drawer.setAttribute('aria-hidden', 'true');
      drawer.replaceChildren();
      if (activeSlot) {
        const prev = panel.querySelector(`.mod-slot[data-slot-id="${activeSlot}"]`);
        if (prev) prev.classList.remove('is-active');
      }
      activeSlot = null;
    }

    function openSlot(slotId, slotType) {
      // Only macros work in PR3b.
      if (slotType !== 'macro') return;

      // Toggle: clicking the active slot closes the drawer.
      if (activeSlot === slotId) {
        closeDrawer();
        return;
      }

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
