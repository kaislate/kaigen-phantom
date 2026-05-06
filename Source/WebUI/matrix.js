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

  let addRouting = null;
  if (window.Juce && typeof window.Juce.getNativeFunction === 'function') {
    try { addRouting = window.Juce.getNativeFunction('modulationAddRouting'); }
    catch (e) { console.warn('[matrix] addRouting unavailable', e); }
  }

  let setRoutingDepth = null;
  if (window.Juce && typeof window.Juce.getNativeFunction === 'function') {
    try { setRoutingDepth = window.Juce.getNativeFunction('modulationSetRoutingDepth'); }
    catch (e) { console.warn('[matrix] setRoutingDepth unavailable', e); }
  }

  let removeRouting = null;
  if (window.Juce && typeof window.Juce.getNativeFunction === 'function') {
    try { removeRouting = window.Juce.getNativeFunction('modulationRemoveRouting'); }
    catch (e) { console.warn('[matrix] removeRouting unavailable', e); }
  }

  let setMacroName = null;
  if (window.Juce && typeof window.Juce.getNativeFunction === 'function') {
    try { setMacroName = window.Juce.getNativeFunction('modulationSetMacroName'); }
    catch (e) { console.warn('[matrix] setMacroName unavailable', e); }
  }

  // Default depth applied when adding a routing via click-to-add (Task 7) and
  // the right-click "Add at +50%" popover entry (Task 9). Centralized so the
  // two callsites stay in sync.
  const DEFAULT_ADD_DEPTH = 0.5;

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

    // Determine which engine this modulator lives in. macro1/2 + lfo1/2 + randomA → A, others → B.
    const engineForMod = (() => {
      for (const eng of ['A', 'B']) {
        if (MODULATORS[eng].some(m => m.id === mod.id)) return eng;
      }
      return 'A';
    })();

    // Read current name from state if available (modulators[].name); fall back to mod.label.
    const stateMods = (engineForMod === 'A' ? lastState.engineA : lastState.engineB).modulators || [];
    const stateMod = stateMods.find(m => m.id === mod.id);
    const currentName = (stateMod && typeof stateMod.name === 'string' && stateMod.name) ? stateMod.name : mod.label;

    const name = document.createElement('div');
    name.className = 'mtx-mod-name';
    name.textContent = currentName;

    if (mod.type === 'macro' && setMacroName) {
      name.style.cursor = 'text';
      name.addEventListener('click', (ev) => {
        ev.stopPropagation();   // don't trigger the matrix's own delegated handlers
        const input = document.createElement('input');
        input.type = 'text';
        input.value = name.textContent;
        input.className = 'mtx-mod-name-input';
        input.maxLength = 16;
        let committed = false;
        const commit = () => {
          if (committed) return;
          committed = true;
          const v = input.value.trim() || mod.label;
          try {
            const result = setMacroName({ source: mod.id, name: v });
            if (result && typeof result.then === 'function') {
              result
                .then(ok => { if (ok === false) console.warn('[matrix] setMacroName returned false', { source: mod.id, name: v }); })
                .catch(e => console.warn('[matrix] setMacroName rejected', e));
            }
          } catch (e) { console.warn('[matrix] setMacroName failed', e); }
          // The C++ binding doesn't currently emit modulationStateChanged for
          // name changes. Force a refresh so the matrix re-renders with the
          // new name. (If a future C++ change adds the emit, this becomes
          // a no-op double-call — refreshFromState is idempotent.)
          refreshFromState();
        };
        const cancel = () => {
          if (committed) return;
          committed = true;
          render();   // re-render restores the original label
        };
        input.addEventListener('keydown', (kev) => {
          if (kev.key === 'Enter')  { kev.preventDefault(); commit(); }
          if (kev.key === 'Escape') { kev.preventDefault(); cancel(); }
        });
        input.addEventListener('blur', () => commit());
        name.replaceWith(input);
        input.focus();
        input.select();
      });
    }

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

    // Click handling for cells is delegated on document.body — see the
    // delegated 'click' listener near the bottom of the IIFE. Adding a
    // per-cell listener here would scale poorly: ~280 cells × every render
    // = hundreds of attachments per state mutation. Cells in the routed
    // state are owned by the pointerdown drag handler (Task 8) and the
    // right-click popover (Task 9).
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

  // Delegated click handler for empty matrix cells. Catches clicks anywhere
  // in the matrix tree, filters to .mtx-cell elements, and adds a routing
  // when the cell is empty. Cells in the routed state are owned by the
  // pointerdown drag handler (Task 8) and right-click popover (Task 9).
  // Delegation here means we attach ONE listener instead of one per cell;
  // re-renders don't re-bind anything.
  document.body.addEventListener('click', (ev) => {
    const cell = ev.target.closest('.mtx-cell');
    if (!cell) return;
    if (!cell.closest('#modulation-matrix')) return;  // only matrix cells
    if (cell.classList.contains('is-routed')) return;
    if (!addRouting) return;
    try {
      const result = addRouting({
        source: cell.dataset.modId,
        param: cell.dataset.paramId,
        depth: DEFAULT_ADD_DEPTH,
      });
      if (result && typeof result.then === 'function') {
        result
          .then(ok => {
            if (ok === false) {
              console.warn('[matrix] addRouting returned false', {
                modId: cell.dataset.modId,
                paramId: cell.dataset.paramId,
              });
            }
          })
          .catch(e => console.warn('[matrix] addRouting rejected', e));
      }
    } catch (e) { console.warn('[matrix] addRouting failed', e); }
    // Re-render is triggered by the modulationStateChanged event from C++.
  });

  // Delegated pointerdown for drag-to-adjust on active matrix cells.
  // Vertical drag: 100 px = full -1..+1 depth range. Dead-zone snap to 0
  // when |depth| < 0.03 (avoids fingertip jitter). pointermove/pointerup
  // attach to window so the drag continues even if the cursor leaves the
  // cell bounds. setRoutingDepth is only called on pointerup — never during
  // the live drag — to avoid spamming the C++ side at pointer-event rate.
  document.body.addEventListener('pointerdown', (downEv) => {
    const cell = downEv.target.closest('.mtx-cell');
    if (!cell) return;
    if (!cell.closest('#modulation-matrix')) return;
    if (!cell.classList.contains('is-routed')) return;   // empty cells handled by click
    if (downEv.button !== 0) return;                      // only left button (right = popover, Task 9)

    downEv.preventDefault();   // suppress default browser drag/text-selection
    const startY = downEv.clientY;
    const routing = findRouting(cell.dataset.engine, cell.dataset.modId, cell.dataset.paramId);
    if (!routing) return;       // race: cell visually routed but state stale
    const startDepth = Math.max(-1, Math.min(1, routing.depth));
    let dragged = false;

    const onMove = (moveEv) => {
      const dy = startY - moveEv.clientY;     // up = positive
      if (Math.abs(dy) > 3) dragged = true;
      let next = startDepth + dy / 100;
      if (Math.abs(next) < 0.03) next = 0;    // dead-zone snap
      next = Math.max(-1, Math.min(1, next));
      // Live visual feedback during drag:
      cell.style.setProperty('--depth-pct', String(Math.round(next * 100)));
      cell.style.setProperty('--depth-abs', String(Math.abs(next)));
      cell.classList.toggle('is-negative', next < 0);
      const num = cell.querySelector('.mtx-cell-depth');
      if (num) num.textContent = (next > 0 ? '+' : '') + Math.round(next * 100);
    };

    const onUp = () => {
      window.removeEventListener('pointermove', onMove);
      window.removeEventListener('pointerup', onUp);
      if (!dragged) return;                    // no commit on tap-without-move
      if (!setRoutingDepth) return;
      const finalDepth = parseInt(cell.style.getPropertyValue('--depth-pct'), 10) / 100;
      try {
        const result = setRoutingDepth({
          source: cell.dataset.modId,
          param: cell.dataset.paramId,
          depth: finalDepth,
        });
        if (result && typeof result.then === 'function') {
          result
            .then(ok => {
              if (ok === false) {
                console.warn('[matrix] setRoutingDepth returned false', {
                  modId: cell.dataset.modId,
                  paramId: cell.dataset.paramId,
                  depth: finalDepth,
                });
              }
            })
            .catch(e => console.warn('[matrix] setRoutingDepth rejected', e));
        }
      } catch (e) { console.warn('[matrix] setRoutingDepth failed', e); }
      // Re-render comes via modulationStateChanged.
    };

    window.addEventListener('pointermove', onMove);
    window.addEventListener('pointerup', onUp);
  });

  // ── Popover (right-click cell quick-actions) ──────────────────────────
  let activePopover = null;
  function closePopover() {
    if (activePopover && activePopover.parentNode) activePopover.remove();
    activePopover = null;
    document.removeEventListener('click', onDocClickOutside, true);
  }
  function onDocClickOutside(ev) {
    if (activePopover && !activePopover.contains(ev.target)) closePopover();
  }
  function openPopover(x, y, items) {
    closePopover();
    const pop = document.createElement('div');
    pop.className = 'mtx-popover';
    pop.style.left = x + 'px';
    pop.style.top  = y + 'px';
    for (const item of items) {
      const btn = document.createElement('button');
      btn.className = 'mtx-popover-btn' + (item.danger ? ' is-danger' : '');
      btn.textContent = item.label;
      btn.addEventListener('click', (ev) => {
        ev.stopPropagation();
        try { item.onClick(); } finally { closePopover(); }
      });
      pop.appendChild(btn);
    }
    document.body.appendChild(pop);
    activePopover = pop;
    // Defer outside-click registration so the contextmenu's own bubbling
    // click doesn't immediately close the popover we just opened.
    setTimeout(() => document.addEventListener('click', onDocClickOutside, true), 0);
  }

  // Delegated contextmenu for cell quick-actions popover.
  document.body.addEventListener('contextmenu', (ev) => {
    const cell = ev.target.closest('.mtx-cell');
    if (!cell) return;
    if (!cell.closest('#modulation-matrix')) return;
    ev.preventDefault();   // suppress default browser context menu

    const source = cell.dataset.modId;
    const param  = cell.dataset.paramId;
    const isRouted = cell.classList.contains('is-routed');

    // Wrapper: invoke a binding with .then(ok) diagnostic — same shape
    // as the click and drag handlers.
    function invoke(fn, name, payload) {
      if (!fn) return;
      try {
        const result = fn(payload);
        if (result && typeof result.then === 'function') {
          result
            .then(ok => {
              if (ok === false) console.warn(`[matrix] ${name} returned false`, payload);
            })
            .catch(e => console.warn(`[matrix] ${name} rejected`, e));
        }
      } catch (e) { console.warn(`[matrix] ${name} failed`, e); }
    }

    const items = isRouted
      ? [
          { label: '−100', onClick: () => invoke(setRoutingDepth, 'setRoutingDepth', { source, param, depth: -1 }) },
          { label: '0',         onClick: () => invoke(setRoutingDepth, 'setRoutingDepth', { source, param, depth: 0 }) },
          { label: '+50',       onClick: () => invoke(setRoutingDepth, 'setRoutingDepth', { source, param, depth: 0.5 }) },
          { label: '+100',      onClick: () => invoke(setRoutingDepth, 'setRoutingDepth', { source, param, depth: 1 }) },
          { label: 'Remove', danger: true, onClick: () => invoke(removeRouting, 'removeRouting', { source, param }) },
        ]
      : [
          { label: 'Add at +50',      onClick: () => invoke(addRouting, 'addRouting', { source, param, depth: 0.5 }) },
          { label: 'Add at −50', onClick: () => invoke(addRouting, 'addRouting', { source, param, depth: -0.5 }) },
        ];

    openPopover(ev.pageX, ev.pageY, items);
  });

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

  // Expose refreshFromState so the SLOTS→MATRIX toggle gets a fresh state pull
  // every time it opens — protects against staleness when routing changes
  // happened while the matrix was hidden (emitEventIfBrowserIsVisible is gated
  // on visibility, so events can be missed).
  window.kaigenRenderMatrix = refreshFromState;
})();
