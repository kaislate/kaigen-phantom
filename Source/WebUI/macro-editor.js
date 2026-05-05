// Source/WebUI/macro-editor.js
//
// Renders the macro editor inside the modulation drawer. Layout:
//   left column: big macro knob (APVTS-backed) + numeric value + name field
//   right column: destinations list (each with depth slider + remove)
//                 + "+ Add destination" picker
//
// State syncs from native modulationGetState; mutations go through
// modulationAddRouting / modulationRemoveRouting / modulationSetRoutingDepth /
// modulationSetMacroName. After each mutation, re-render from a fresh
// modulationGetState fetch.

(function () {
  'use strict';

  if (typeof window.Juce === 'undefined' || typeof window.Juce.getNativeFunction !== 'function') {
    document.addEventListener('DOMContentLoaded', init, { once: true });
    return;
  }
  init();

  function init() {
    const getState   = window.Juce.getNativeFunction('modulationGetState');
    const addRouting = window.Juce.getNativeFunction('modulationAddRouting');
    const rmRouting  = window.Juce.getNativeFunction('modulationRemoveRouting');
    const setDepth   = window.Juce.getNativeFunction('modulationSetRoutingDepth');
    const setName    = window.Juce.getNativeFunction('modulationSetMacroName');
    if (!getState || !addRouting || !rmRouting || !setDepth || !setName) {
      console.warn('[macro-editor] native bindings missing'); return;
    }

    // Determine which engine owns a macro id.
    function engineForMacro(macroId) {
      return (macroId === 'macro1' || macroId === 'macro2') ? 'engineA' : 'engineB';
    }

    // Mirror of phantom.js's formatDisplayValue, scoped down to what macro
    // params actually need (0-1 normalised, no label). Kept inline rather
    // than pulled out of phantom.js to avoid expanding that module's API
    // surface for one caller.
    function formatMacroDisplay(state) {
      if (typeof state.getScaledValue !== 'function') return '';
      const val = state.getScaledValue();
      const props = state.properties || {};
      const label = (props.label || '').trim();
      if (props.start === 0 && props.end === 1 && !label) {
        return `${Math.round(val * 100)}%`;
      }
      return `${val.toFixed(2)}${label ? ' ' + label : ''}`;
    }

    // Tracks slider-state listener IDs per macro so re-renders (which
    // recreate the knob element) can detach the old listeners. Without
    // this, every routing add/remove leaks an updateKnob closure that
    // keeps the detached knob alive and keeps firing on host changes.
    const _knobListeners = new Map();  // paramId → { vId, pId, state }

    // Wire a runtime-created phantom-knob to its APVTS slider relay.
    // Mirrors the auto-bind block in phantom.js (search "phantom-knob[data-param]").
    function bindKnobToParam(knob, paramId) {
      const getter = window.Juce && window.Juce.getSliderStateLogical;
      if (typeof getter !== 'function') {
        console.warn('[macro-editor] getSliderStateLogical unavailable');
        return;
      }
      const state = getter(paramId);
      if (!state) {
        console.warn('[macro-editor] no slider state for', paramId);
        return;
      }

      // Detach any previous binding for this macro (re-render path).
      const prev = _knobListeners.get(paramId);
      if (prev) {
        if (prev.state.valueChangedEvent && typeof prev.vId === 'number') {
          prev.state.valueChangedEvent.removeListener(prev.vId);
        }
        if (prev.state.propertiesChangedEvent && typeof prev.pId === 'number') {
          prev.state.propertiesChangedEvent.removeListener(prev.pId);
        }
      }

      function updateKnob() {
        knob.value = state.getNormalisedValue();
        knob.displayValue = formatMacroDisplay(state);
      }

      // Host → UI
      const vId = state.valueChangedEvent.addListener(updateKnob);
      let pId;
      if (state.propertiesChangedEvent && state.propertiesChangedEvent.addListener) {
        pId = state.propertiesChangedEvent.addListener(updateKnob);
      }
      _knobListeners.set(paramId, { vId, pId, state });

      // UI → host (drag)
      knob.addEventListener('knob-change', (e) => {
        state.sliderDragStarted();
        state.setNormalisedValue(e.detail.value);
        state.sliderDragEnded();
      });

      updateKnob();
    }

    async function render(host, macroId) {
      const state = await getState();
      const eng = state[engineForMacro(macroId)];
      const macro = eng.modulators.find(m => m.id === macroId);
      if (!macro) {
        host.textContent = '[modulator missing]';
        return;
      }
      const macroRoutings = (eng.routings || []).filter(r => r.source === macroId);

      host.replaceChildren();

      // ── Left: knob + name field ──
      const left = document.createElement('div');
      left.className = 'macro-editor-left';

      const knob = document.createElement('phantom-knob');
      knob.setAttribute('data-param', macroId);   // global APVTS param; getSliderState short-circuits
      knob.classList.add('macro-editor-knob');
      left.appendChild(knob);

      // phantom.js's auto-binder runs only at page-boot via querySelectorAll,
      // so dynamically-created knobs miss it. Bind explicitly here using the
      // same slider-state pattern.
      bindKnobToParam(knob, macroId);

      const nameInput = document.createElement('input');
      nameInput.type = 'text';
      nameInput.className = 'macro-editor-name';
      nameInput.value = macro.name || macroId;
      nameInput.placeholder = 'Name';
      nameInput.addEventListener('change', async () => {
        await setName({ source: macroId, name: nameInput.value });
        // No need to re-render; only the name changed locally.
      });
      left.appendChild(nameInput);

      host.appendChild(left);

      // ── Right: destinations list + add picker ──
      const right = document.createElement('div');
      right.className = 'macro-editor-right';

      const listHdr = document.createElement('div');
      listHdr.className = 'macro-editor-list-hdr';
      listHdr.textContent = 'DESTINATIONS';
      right.appendChild(listHdr);

      if (macroRoutings.length === 0) {
        const empty = document.createElement('div');
        empty.className = 'macro-editor-empty';
        empty.textContent = 'No destinations. Click + Add destination below.';
        right.appendChild(empty);
      } else {
        macroRoutings.forEach(r => {
          const eligibleEntry = (eng.eligible || []).find(e => e.id === r.param);
          const displayName = eligibleEntry ? eligibleEntry.name : r.param;

          const row = document.createElement('div');
          row.className = 'macro-editor-row';

          const name = document.createElement('span');
          name.className = 'macro-editor-row-name';
          name.textContent = displayName;
          row.appendChild(name);

          const slider = document.createElement('input');
          slider.type = 'range';
          slider.min = '-1';
          slider.max = '1';
          slider.step = '0.01';
          slider.value = String(r.depth);
          slider.className = 'macro-editor-row-depth';
          slider.addEventListener('input', async () => {
            await setDepth({ source: macroId, param: r.param, depth: parseFloat(slider.value) });
          });
          row.appendChild(slider);

          const valLabel = document.createElement('span');
          valLabel.className = 'macro-editor-row-value';
          valLabel.textContent = (r.depth > 0 ? '+' : '') + Math.round(r.depth * 100) + '%';
          row.appendChild(valLabel);
          slider.addEventListener('input', () => {
            const v = parseFloat(slider.value);
            valLabel.textContent = (v > 0 ? '+' : '') + Math.round(v * 100) + '%';
          });

          const removeBtn = document.createElement('button');
          removeBtn.className = 'macro-editor-row-remove';
          removeBtn.textContent = '×';
          removeBtn.title = 'Remove this destination';
          removeBtn.addEventListener('click', async () => {
            await rmRouting({ source: macroId, param: r.param });
            await render(host, macroId);
          });
          row.appendChild(removeBtn);

          right.appendChild(row);
        });
      }

      // ── Add picker ──
      const picker = document.createElement('div');
      picker.className = 'macro-editor-picker';
      const addBtn = document.createElement('button');
      addBtn.className = 'macro-editor-add';
      addBtn.textContent = '+ Add destination';
      picker.appendChild(addBtn);
      const select = document.createElement('select');
      select.className = 'macro-editor-picker-select';
      select.style.display = 'none';
      const placeholder = document.createElement('option');
      placeholder.value = '';
      placeholder.textContent = '-- choose param --';
      select.appendChild(placeholder);
      // Filter eligible: exclude params already routed from this macro.
      const usedParams = new Set(macroRoutings.map(r => r.param));
      (eng.eligible || []).forEach(e => {
        if (usedParams.has(e.id)) return;
        const opt = document.createElement('option');
        opt.value = e.id;
        opt.textContent = e.name;
        select.appendChild(opt);
      });
      picker.appendChild(select);

      addBtn.addEventListener('click', () => {
        addBtn.style.display = 'none';
        select.style.display = '';
        select.focus();
      });
      select.addEventListener('change', async () => {
        if (!select.value) return;
        await addRouting({ source: macroId, param: select.value, depth: 0.5 });
        await render(host, macroId);
      });

      right.appendChild(picker);

      // ── Close button ──
      const close = document.createElement('button');
      close.className = 'macro-editor-close';
      close.title = 'Close';
      close.textContent = '▼';
      close.addEventListener('click', () => {
        if (typeof window.kaigenCloseModulationDrawer === 'function')
          window.kaigenCloseModulationDrawer();
      });
      right.appendChild(close);

      host.appendChild(right);
    }

    // Expose for modulation-panel.js to call.
    window.kaigenRenderMacroEditor = render;
  }
})();
