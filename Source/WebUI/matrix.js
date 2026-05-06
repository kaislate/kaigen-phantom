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

  // Continuous-parameter destinations grouped by category. Choice/bool params
  // (mode, ghost_mode, env_source, midi_trigger_enabled, etc.) are excluded
  // because they're not modulation-eligible. The label is a 5–6 char abbreviation
  // shown in the column header; the tooltip shows the full prefixed param ID.
  const DEST_GROUPS = [
    { id: 'GHOST',    label: 'GHOST',    leaves: [
      ['ghost', 'GHOST'], ['phantom_threshold', 'PHTHR'],
      ['phantom_strength', 'PSTR'], ['output_gain', 'OUT'],
    ]},
    { id: 'RECIPE',   label: 'RECIPE',   leaves: [
      ['recipe_h2', 'H2'], ['recipe_h3', 'H3'], ['recipe_h4', 'H4'], ['recipe_h5', 'H5'],
      ['recipe_h6', 'H6'], ['recipe_h7', 'H7'], ['recipe_h8', 'H8'],
      ['harmonic_saturation', 'HSAT'],
    ]},
    { id: 'SHAPE',    label: 'SHAPE',    leaves: [
      ['synth_step', 'STEP'], ['synth_duty', 'DUTY'], ['synth_skip', 'SKIP'],
    ]},
    { id: 'ENVELOPE', label: 'ENV',      leaves: [
      ['env_attack_ms', 'ATK'], ['env_release_ms', 'REL'],
    ]},
    { id: 'FILTER',   label: 'FILTER',   leaves: [
      ['synth_lpf_hz', 'LPF'], ['synth_hpf_hz', 'HPF'],
    ]},
    { id: 'RESYN',    label: 'RESYN',    leaves: [
      ['synth_wavelet_length', 'WVLEN'], ['synth_gate_threshold', 'GATE'],
      ['synth_h1', 'H1'], ['synth_sub', 'SUB'],
    ]},
    { id: 'PITCH',    label: 'PITCH',    leaves: [
      ['synth_min_samples', 'MINSP'], ['synth_max_samples', 'MAXSP'],
      ['tracking_speed', 'TRACK'], ['punch_amount', 'PUNCH'],
      ['synth_boost_threshold', 'BTHR'], ['synth_boost_amount', 'BAMT'],
    ]},
    { id: 'STEREO',   label: 'STEREO',   leaves: [
      ['binaural_width', 'BIN'], ['stereo_width', 'WIDTH'],
    ]},
  ];

  // UI state: which categories are expanded per engine. Persisted via
  // matrixGetState/matrixSetState bindings (Task 12). Default-expanded:
  // GHOST and RECIPE only. Set ordering is irrelevant — rendering iterates
  // DEST_GROUPS in canonical order and queries set membership.
  const expandedCats = {
    A: new Set(['GHOST', 'RECIPE']),
    B: new Set(['GHOST', 'RECIPE']),
  };

  // Derived from MODULATORS so adding a 5th macro in the future doesn't
  // require updating a parallel hardcoded list. Computed once at IIFE init.
  const MACRO_IDS = [...MODULATORS.A, ...MODULATORS.B]
    .filter(m => m.type === 'macro')
    .map(m => m.id);

  // Cache the per-render live targets — populated by render() after each
  // DOM rebuild. The kaigen:live-state listener fires at 15 Hz; per-tick
  // querySelector is wasteful and contradicts the pattern established
  // in modulation-panel.js (see commit 8728003).
  let liveEls = { rings: {}, vals: {} };

  // Cache the modulation-state binding once. The full state is pulled lazily
  // and refreshed on every modulationStateChanged event from the C++ side.
  let getState = null;
  if (window.Juce && typeof window.Juce.getNativeFunction === 'function') {
    try { getState = window.Juce.getNativeFunction('modulationGetState'); }
    catch (e) { console.warn('[matrix] modulationGetState unavailable', e); }
  }

  let lastState = { engineA: { routings: [] }, engineB: { routings: [] } };

  async function pullState() {
    if (!getState) return;
    try {
      const s = await getState();
      if (s && typeof s === 'object') lastState = s;
    } catch (e) {
      console.warn('[matrix] getState failed', e);
    }
  }

  // Routings serialized by C++ use { source, param, depth, invert } — match
  // the property names exactly (see PluginEditor.cpp modulationGetState).
  function findRouting(engineId, modId, paramId) {
    const eng = engineId === 'A' ? lastState.engineA : lastState.engineB;
    if (!eng || !Array.isArray(eng.routings)) return null;
    return eng.routings.find(r => r.source === modId && r.param === paramId) || null;
  }

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

  function makeCell(engineId, modId, leaf) {
    const paramId = (engineId === 'A' ? 'a_' : 'b_') + leaf;
    const routing = findRouting(engineId, modId, paramId);
    const cell = document.createElement('div');

    // Determine cell type for color from MODULATORS table.
    const eng = MODULATORS[engineId] || [];
    const modDef = eng.find(m => m.id === modId);
    const modType = modDef ? modDef.type : 'macro';
    cell.className = `mtx-cell mtx-cell-${modType}`;

    cell.dataset.engine  = engineId;
    cell.dataset.modId   = modId;
    cell.dataset.paramId = paramId;
    cell.title = paramId;

    if (routing) {
      const d = Math.max(-1, Math.min(1, routing.depth));
      cell.classList.add('is-routed');
      if (d < 0) cell.classList.add('is-negative');
      cell.style.setProperty('--depth-pct', String(Math.round(d * 100)));
      cell.style.setProperty('--depth-abs', String(Math.abs(d)));
      const num = document.createElement('span');
      num.className = 'mtx-cell-depth';
      num.textContent = (d > 0 ? '+' : '') + Math.round(d * 100);
      cell.appendChild(num);
    }

    return cell;
  }

  function makeColumnHeaderRow(engineId) {
    const headerRow = document.createElement('div');
    headerRow.className = 'mtx-row mtx-header-row';
    const empty = document.createElement('div');
    empty.className = 'mtx-mod mtx-mod-headercell';
    headerRow.appendChild(empty);

    const headerCells = document.createElement('div');
    headerCells.className = 'mtx-cells mtx-header-cells';
    for (const group of DEST_GROUPS) {
      const isOpen = expandedCats[engineId].has(group.id);
      const groupHeader = document.createElement('div');
      groupHeader.className = 'mtx-cat-header' + (isOpen ? ' is-open' : '');
      groupHeader.dataset.engine = engineId;
      groupHeader.dataset.cat = group.id;
      groupHeader.textContent = group.label + (isOpen ? ' ▾' : ' ▸');
      groupHeader.title = isOpen ? `Collapse ${group.label}` : `Expand ${group.label}`;
      groupHeader.addEventListener('click', () => {
        if (expandedCats[engineId].has(group.id)) expandedCats[engineId].delete(group.id);
        else expandedCats[engineId].add(group.id);
        render();
        if (typeof window.kaigenSaveMatrixState === 'function') window.kaigenSaveMatrixState();
      });
      headerCells.appendChild(groupHeader);

      if (isOpen) {
        for (const [leaf, label] of group.leaves) {
          const colHeader = document.createElement('div');
          colHeader.className = 'mtx-col-header';
          colHeader.textContent = label;
          colHeader.title = (engineId === 'A' ? 'a_' : 'b_') + leaf;
          headerCells.appendChild(colHeader);
        }
      }
    }
    headerRow.appendChild(headerCells);
    return headerRow;
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

    grid.appendChild(makeColumnHeaderRow(engineId));

    for (const mod of MODULATORS[engineId]) {
      const row = document.createElement('div');
      row.className = 'mtx-row';
      row.appendChild(makeModRow(mod));

      const cells = document.createElement('div');
      cells.className = 'mtx-cells';
      cells.dataset.engine = engineId;
      cells.dataset.modId = mod.id;
      for (const group of DEST_GROUPS) {
        const isOpen = expandedCats[engineId].has(group.id);
        const groupSpacer = document.createElement('div');
        groupSpacer.className = 'mtx-cat-spacer';
        cells.appendChild(groupSpacer);
        if (isOpen) {
          for (const [leaf] of group.leaves) {
            cells.appendChild(makeCell(engineId, mod.id, leaf));
          }
        }
      }
      row.appendChild(cells);
      grid.appendChild(row);
    }
    return block;
  }

  function render() {
    const root = document.getElementById('modulation-matrix');
    if (!root) {
      console.warn('[matrix] #modulation-matrix not in DOM — render skipped');
      return;
    }
    // NOTE: replaceChildren() wipes the entire matrix subtree. Cell click
    // handlers added in Task 7+ MUST be re-attached on each render — either
    // by re-binding inside makeEngineBlock, or by switching to delegated
    // listeners on #modulation-matrix that survive re-renders.
    root.replaceChildren();
    root.appendChild(makeEngineBlock('A'));
    const divider = document.createElement('div');
    divider.className = 'mtx-engine-divider';
    root.appendChild(divider);
    root.appendChild(makeEngineBlock('B'));

    // Refresh the live-target cache after the DOM rebuild.
    liveEls = { rings: {}, vals: {} };
    for (const id of MACRO_IDS) {
      liveEls.rings[id] = root.querySelector(`.mtx-mod[data-mod-id="${id}"] .mtx-ring`);
      liveEls.vals[id]  = root.querySelector(`.mtx-mod[data-mod-id="${id}"] .mtx-mod-val`);
    }
  }

  // Pull state, then re-render. Used on init and on modulationStateChanged.
  async function refreshFromState() {
    await pullState();
    const root = document.getElementById('modulation-matrix');
    if (root && root.classList.contains('is-open')) {
      render();
    }
  }

  // The C++ bindings dispatch this CustomEvent after every Add/Remove/SetDepth
  // (via emitEventIfBrowserIsVisible → backend listener below). JS-side
  // mutations may also dispatch the document.body CustomEvent directly.
  document.body.addEventListener('modulationStateChanged', () => { refreshFromState(); });
  if (window.__JUCE__ && window.__JUCE__.backend
      && typeof window.__JUCE__.backend.addEventListener === 'function') {
    try {
      window.__JUCE__.backend.addEventListener('modulationStateChanged',
        () => { refreshFromState(); });
    } catch (e) {
      console.warn('[matrix] backend addEventListener failed', e);
    }
  }

  // Pull state once at startup so the first render() (triggered by SLOTS→MATRIX
  // toggle) has data even if the toggle happens before any routing mutation.
  refreshFromState();

  // Subscribe to live state — drives the macro rings and value readouts in
  // the matrix's modulator strip. Only updates the matrix when it's visible.
  document.body.addEventListener('kaigen:live-state', (ev) => {
    const root = document.getElementById('modulation-matrix');
    if (!root || !root.classList.contains('is-open')) return;
    const macros = (ev.detail && ev.detail.macros) || {};
    for (const id of MACRO_IDS) {
      const ring = liveEls.rings[id];
      const val  = liveEls.vals[id];
      const v = (macros[id] && typeof macros[id].value === 'number') ? macros[id].value : 0;
      if (ring) ring.style.setProperty('--v', String(Math.max(0, Math.min(1, v)) * 100));
      if (val)  val.textContent = v.toFixed(2);
    }
  });

  // Expose so modulation-panel.js can trigger a render on toggle-to-matrix.
  window.kaigenRenderMatrix = render;
})();
