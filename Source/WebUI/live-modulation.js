// Source/WebUI/live-modulation.js
//
// Polls modulationGetLiveState at 10 Hz and dispatches a 'kaigen:live-state'
// CustomEvent on document.body. Both the slots view (macro/morph rings) and
// the matrix view (per-cell live-modulating glow) subscribe to this single
// source rather than each running their own poll.
//
// 10 Hz (was 15 Hz) is still smooth enough for ring fill + cell pulse
// animations and saves ~33% of polling load — see commit message for
// "fix(matrix): macro knobs, slide-down layout, lower poll rate + cell cache".

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

    const POLL_MS = 1000 / 10;
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
