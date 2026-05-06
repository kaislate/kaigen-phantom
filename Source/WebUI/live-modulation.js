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
//
// Externally gated: callers must invoke kaigenStartLiveModulationPoll() to
// arm the timer. The poll is dormant until then. modulation-panel.js
// arms it whenever MATRIX mode opens and disarms it when MATRIX closes,
// because SLOTS mode no longer needs the poll — phantom-knob macros drive
// their display via native APVTS attachment, not via the kaigen:live-state
// event. Eliminating the SLOTS-mode poll removes a per-tick allocation
// burst on the JUCE message thread that was contending with WebSliderRelay
// drag messages and causing visible knob jitter / value skipping.

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
    let enabled = false;
    let visible = (document.visibilityState !== 'hidden');

    async function tick() {
      try {
        const state = await getLiveState();
        document.body.dispatchEvent(new CustomEvent('kaigen:live-state', { detail: state }));
      } catch (e) {
        // Binding gone (editor teardown). Stop polling silently.
        if (timer) { clearInterval(timer); timer = null; }
        dead = true;
      }
    }

    function maybeStart() {
      if (timer || dead || !enabled || !visible) return;
      timer = setInterval(tick, POLL_MS);
    }
    function maybeStop() {
      if (!timer) return;
      clearInterval(timer);
      timer = null;
    }

    document.addEventListener('visibilitychange', () => {
      visible = (document.visibilityState !== 'hidden');
      if (visible) maybeStart();
      else maybeStop();
    });

    // External gates. modulation-panel.js drives these from MATRIX mode.
    window.kaigenStartLiveModulationPoll = () => { enabled = true;  maybeStart(); };
    window.kaigenStopLiveModulationPoll  = () => { enabled = false; maybeStop();  };
  }
})();
