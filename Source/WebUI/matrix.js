// Source/WebUI/matrix.js — matrix view controller. See plan Task 4+.
(function () {
  'use strict';

  function render() {
    const root = document.getElementById('modulation-matrix');
    if (!root) return;
    root.replaceChildren();
    const ph = document.createElement('div');
    ph.className = 'matrix-placeholder';
    ph.textContent = 'matrix view (skeleton — rows/cells in next tasks)';
    ph.style.cssText = 'padding: 40px; text-align: center; color: rgba(255,255,255,0.35); font: 600 11px/1 sans-serif; letter-spacing: 1.5px;';
    root.appendChild(ph);
  }

  window.kaigenRenderMatrix = render;
})();
