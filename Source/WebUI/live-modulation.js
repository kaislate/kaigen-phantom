// Source/WebUI/live-modulation.js
//
// Polls modulationGetLiveState at 15 Hz and dispatches a 'kaigen:live-state'
// CustomEvent on document.body. Both the slots view (macro/morph rings) and
// the matrix view (per-cell live-modulating glow) subscribe to this single
// source rather than each running their own poll.

(function () {
  'use strict';

  if (typeof window.Juce === 'undefined' || typeof window.Juce.getNativeFunction !== 'function') {
    document.addEventListener('DOMContentLoaded', init, { once: true });
    return;
  }
  init();

  function init() {
    let getLiveState = null;
    try { getLiveState = window.Juce.getNativeFunction('modulationGetLiveState'); }
    catch (e) { console.warn('[live-modulation] binding unavailable', e); return; }
    if (!getLiveState) return;

    const POLL_MS = 1000 / 15;
    let timer = null;
    let dead = false;

    async function tick() {
      try {
        const state = await getLiveState();
        document.body.dispatchEvent(new CustomEvent('kaigen:live-state', { detail: state }));
      } catch (e) {
        // Binding gone (editor teardown). Stop polling silently.
        clearInterval(timer);
        timer = null;
        dead = true;
      }
    }

    function start() {
      if (timer || dead) return;
      timer = setInterval(tick, POLL_MS);
    }
    function stop() {
      if (!timer) return;
      clearInterval(timer);
      timer = null;
    }

    document.addEventListener('visibilitychange', () => {
      if (document.visibilityState === 'visible') start();
      else stop();
    });

    start();
  }
})();
