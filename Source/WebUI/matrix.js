// Source/WebUI/matrix.js — matrix view controller.
(function () {
  'use strict';

  // Modulator definitions. Order within each engine: macros → LFOs → random.
  // LFO + Random rows are placeholders (no live data) until PR4/PR5.
  const MODULATORS = {
    A: [
      { id: 'macro1',  type: 'macro',  label: 'MAC 1' },
      { id: 'macro2',  type: 'macro',  label: 'MAC 2' },
      { id: 'lfo1',    type: 'lfo',    label: 'LFO 1', placeholder: 'PR4' },
      { id: 'lfo2',    type: 'lfo',    label: 'LFO 2', placeholder: 'PR4' },
      { id: 'randomA', type: 'random', label: 'RAND',  placeholder: 'PR5' },
    ],
    B: [
      { id: 'macro3',  type: 'macro',  label: 'MAC 3' },
      { id: 'macro4',  type: 'macro',  label: 'MAC 4' },
      { id: 'lfo3',    type: 'lfo',    label: 'LFO 3', placeholder: 'PR4' },
      { id: 'lfo4',    type: 'lfo',    label: 'LFO 4', placeholder: 'PR4' },
      { id: 'randomB', type: 'random', label: 'RAND',  placeholder: 'PR5' },
    ],
  };

  function makeModRow(mod) {
    const row = document.createElement('div');
    row.className = `mtx-mod mtx-mod-${mod.type}`;
    if (mod.placeholder) row.classList.add('is-placeholder');
    row.dataset.modId = mod.id;

    const anim = document.createElement('div');
    anim.className = 'mtx-mod-anim';
    if (mod.type === 'macro') {
      anim.innerHTML = '<span class="mtx-ring" style="--v: 0;"></span>';
    } else if (mod.type === 'lfo') {
      anim.innerHTML = '<svg viewBox="0 0 24 14" preserveAspectRatio="none"><path d="M0,7 Q3,0 6,7 T12,7 T18,7 T24,7" fill="none" stroke="currentColor" stroke-width="1.2"/></svg>';
    } else if (mod.type === 'random') {
      anim.innerHTML = '<span class="mtx-scatter"><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i></span>';
    }

    const name = document.createElement('div');
    name.className = 'mtx-mod-name';
    name.textContent = mod.label;

    const val = document.createElement('div');
    val.className = 'mtx-mod-val';
    val.textContent = mod.placeholder ? mod.placeholder : '0.00';

    row.appendChild(anim);
    row.appendChild(name);
    row.appendChild(val);
    return row;
  }

  function makeEngineBlock(engineId) {
    const block = document.createElement('section');
    block.className = `mtx-engine mtx-engine-${engineId.toLowerCase()}`;

    const header = document.createElement('div');
    header.className = 'mtx-engine-label';
    header.textContent = `ENGINE ${engineId}`;
    block.appendChild(header);

    const grid = document.createElement('div');
    grid.className = 'mtx-grid';
    block.appendChild(grid);

    for (const mod of MODULATORS[engineId]) {
      const row = document.createElement('div');
      row.className = 'mtx-row';
      row.appendChild(makeModRow(mod));
      // Cell area placeholder — populated in later tasks.
      const cells = document.createElement('div');
      cells.className = 'mtx-cells';
      cells.dataset.engine = engineId;
      cells.dataset.modId = mod.id;
      row.appendChild(cells);
      grid.appendChild(row);
    }
    return block;
  }

  function render() {
    const root = document.getElementById('modulation-matrix');
    if (!root) return;
    root.replaceChildren();
    root.appendChild(makeEngineBlock('A'));
    const divider = document.createElement('div');
    divider.className = 'mtx-engine-divider';
    root.appendChild(divider);
    root.appendChild(makeEngineBlock('B'));
  }

  // Subscribe to live state — drives the macro rings and value readouts in
  // the matrix's modulator strip. Only updates the matrix when it's visible.
  document.body.addEventListener('kaigen:live-state', (ev) => {
    const root = document.getElementById('modulation-matrix');
    if (!root || !root.classList.contains('is-open')) return;
    const macros = (ev.detail && ev.detail.macros) || {};
    for (const id of ['macro1', 'macro2', 'macro3', 'macro4']) {
      const ring = root.querySelector(`.mtx-mod[data-mod-id="${id}"] .mtx-ring`);
      const val  = root.querySelector(`.mtx-mod[data-mod-id="${id}"] .mtx-mod-val`);
      const v = (macros[id] && typeof macros[id].value === 'number') ? macros[id].value : 0;
      if (ring) ring.style.setProperty('--v', String(Math.max(0, Math.min(1, v)) * 100));
      if (val)  val.textContent = v.toFixed(2);
    }
  });

  // Expose so modulation-panel.js can trigger a render on toggle-to-matrix.
  window.kaigenRenderMatrix = render;
})();
