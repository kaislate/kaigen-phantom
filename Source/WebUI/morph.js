// morph.js — Morph crossfader slider (post-PR1)
// In PR1 this is a thin wrapper: slider value drives morph_amount which
// the host treats as an audio crossfade between Engine A and Engine B.
// PR3 introduces the new bottom modulation panel and supersedes this.
(function () {
  'use strict';

  if (typeof window.Juce === 'undefined' || typeof window.Juce.getNativeFunction !== 'function') {
    document.addEventListener('DOMContentLoaded', init, { once: true });
    return;
  }
  init();

  function init() {
    const slider = document.getElementById('mod-slider');
    const value  = document.getElementById('mod-value');
    if (!slider || !value) return;

    const relay = window.Juce.getSliderState('morph_amount');
    if (!relay) { console.warn('[morph] no morph_amount relay'); return; }

    const setFromClientX = (clientX) => {
      const rect = slider.getBoundingClientRect();
      if (rect.width <= 0) return;
      const norm = Math.max(0, Math.min(1, (clientX - rect.left) / rect.width));
      relay.setNormalisedValue(norm);
    };

    let dragging = false;
    slider.addEventListener('mousedown', (e) => {
      dragging = true;
      setFromClientX(e.clientX);
      e.preventDefault();
    });
    document.addEventListener('mousemove', (e) => { if (dragging) setFromClientX(e.clientX); });
    document.addEventListener('mouseup',   () => { dragging = false; });

    function render() {
      const v = relay.getNormalisedValue();
      const fillEl   = slider.querySelector('.mod-slider-fill');
      const handleEl = slider.querySelector('.mod-slider-handle');
      if (fillEl)   fillEl.style.width   = (v * 100) + '%';
      if (handleEl) handleEl.style.left  = (v * 100) + '%';
      value.textContent = v.toFixed(2);
      requestAnimationFrame(render);
    }
    render();
  }
})();
