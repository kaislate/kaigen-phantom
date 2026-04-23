// morph.js — Pro-only morph module
// In Standard builds, the Pro native functions are not registered; this file
// silently does nothing. In Pro builds, it wires up the modulation panel,
// arc ring rendering, and capture mode.
(function () {
  'use strict';

  // Early bail if the bridge isn't even loaded yet (runs again on later init).
  if (typeof window.Juce === 'undefined' || typeof window.Juce.getNativeFunction !== 'function') {
    return;
  }

  // Detect Pro build: morphGetState is only registered in Pro.
  const morphGetState = window.Juce.getNativeFunction('morphGetState');
  if (typeof morphGetState !== 'function') {
    // Standard build — do nothing.
    return;
  }

  // Pro build — scaffolding for subsequent tasks.
  console.log('[morph] Pro build detected, morph module initializing');
})();
