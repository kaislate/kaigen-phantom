// ─────────────────────────────────────────────────────────────────────────
// Kaigen Phantom — Preset system (UI)
//
// One consolidated module. Uses the JUCE WebView bridge correctly:
//   const fn = window.Juce.getNativeFunction("name");
//   const result = await fn(arg1, arg2);  // returns a Promise
// ─────────────────────────────────────────────────────────────────────────

(function () {

// ── Native function bridge ───────────────────────────────────────────────

const native = {
    getAllPresets:     null,
    loadPreset:        null,
    savePreset:        null,
    setFavorite:       null,
    deletePreset:      null,
    getAllPacks:       null,
    setInputFocused:   null,
    returnFocusToHost: null,
    forwardKeyToHost:  null,
};

function initNativeBridge() {
    if (!window.Juce || typeof window.Juce.getNativeFunction !== 'function') {
        console.warn('Preset system: window.Juce.getNativeFunction unavailable');
        return false;
    }
    native.getAllPresets     = window.Juce.getNativeFunction('getAllPresets');
    native.loadPreset        = window.Juce.getNativeFunction('loadPreset');
    native.savePreset        = window.Juce.getNativeFunction('savePreset');
    native.setFavorite       = window.Juce.getNativeFunction('setFavorite');
    native.deletePreset      = window.Juce.getNativeFunction('deletePreset');
    native.getAllPacks       = window.Juce.getNativeFunction('getAllPacks');
    native.setInputFocused   = window.Juce.getNativeFunction('setInputFocused');
    native.returnFocusToHost = window.Juce.getNativeFunction('returnFocusToHost');
    native.forwardKeyToHost  = window.Juce.getNativeFunction('forwardKeyToHost');
    return true;
}

// ── Keyboard pass-through ─────────────────────────────────────────────
//
// Goal: the DAW always receives keyboard input (spacebar = play/pause,
// A–L = MIDI keyboard mode, etc.) *except* when the user is typing in a
// text field inside the plugin UI. Crucially, dragging a knob must not
// steal keyboard focus — holding spacebar while moving a parameter should
// keep playback toggling as expected.
//
// Two-part solution:
//
//  1. Block focus shift at its source — preventDefault on mousedown for
//     non-input targets. The click still dispatches to knob/button JS
//     listeners (they use bubble-phase mousedown and don't depend on
//     default behavior), so drag/click logic keeps working. What's
//     suppressed is Chromium's default focus transfer to the clicked
//     element — meaning the WebView never asks for keyboard focus in the
//     first place and the DAW keeps it throughout the interaction.
//
//  2. Track input focus — when user clicks into a text field, let the
//     default (focus transfer) happen by NOT calling preventDefault.
//     focusin/focusout update the native flag so the Win32 subclass
//     (which runs as a safety net) knows whether to forward keys.

function isTextTarget(target) {
    return !!target && typeof target.matches === 'function' &&
        target.matches('input, textarea, [contenteditable="true"]');
}

function wireFocusTracking() {
    if (!native.setInputFocused) return;

    // Block default focus transfer on clicks that aren't meant to type.
    // capture: true so we run before any element's own mousedown listener
    // can call stopPropagation.
    document.addEventListener('mousedown', (e) => {
        if (!isTextTarget(e.target)) e.preventDefault();
    }, true);

    document.addEventListener('focusin', (e) => {
        native.setInputFocused(isTextTarget(e.target));
    }, true);

    document.addEventListener('focusout', () => {
        native.setInputFocused(isTextTarget(document.activeElement));
    }, true);

    // Key forwarding — the core DAW pass-through. Any key pressed while the
    // focused element is NOT a text input gets intercepted and PostMessage'd
    // to the DAW's top-level window. This works even when WebView2 has stolen
    // Windows-level keyboard focus (the mousedown/focus-blocking fixes above
    // don't fully prevent that on every Chromium version).
    const forwardKey = (e, isDown) => {
        if (isTextTarget(e.target)) return;                  // let user type
        if (!native.forwardKeyToHost) return;
        e.preventDefault();
        e.stopPropagation();
        // e.keyCode is deprecated but maps directly to Windows VK_* codes
        // (VK_SPACE=32, A-Z=65-90, arrows=37-40, F1-F12=112-123, etc.) which
        // is exactly what we need for PostMessage(WM_KEYDOWN).
        native.forwardKeyToHost(e.keyCode, isDown);
    };
    document.addEventListener('keydown', (e) => forwardKey(e, true),  true);
    document.addEventListener('keyup',   (e) => forwardKey(e, false), true);

    // Initial state.
    native.setInputFocused(isTextTarget(document.activeElement));
}

// ── UI state ─────────────────────────────────────────────────────────────

const state = {
    currentName:     'Default',
    currentPack:     '',
    isFavorite:      false,
    isModified:      false,
    isLoadingPreset: false,
    presetsCache:    null,  // { packName: [ { metadata: {...} } ] }
    packsCache:      null,  // [ { name, displayName, description, designer, hasCoverArt, presetCount } ]
};

// Sort state for the preset browser list. Persists for the lifetime of the
// editor window; resets to defaults when the plugin is re-opened.
const browserSort = { column: 'name', dir: 'asc' };

// Weighted-mean peak frequency in Hz — used by the Shape column sort.
// Uses the same effective fundamental the renderer uses, so skip and
// crossover both feed in. Returns 0 for presets with no harmonic content
// (they sort to the bottom in ascending order).
function spectralCentroid(preview) {
    if (!preview || !Array.isArray(preview.h)) return 0;
    const fund = Math.max(0.01, (preview.crossover || 120) / Math.pow(2, preview.skip || 0));
    let num = 0, den = 0;
    for (let i = 0; i < 7; ++i) {
        const w = preview.h[i] || 0;
        num += w * (i + 2) * fund;
        den += w;
    }
    return den > 0 ? num / den : 0;
}

function compareRows(a, b) {
    const dir = browserSort.dir === 'asc' ? 1 : -1;
    const col = browserSort.column;
    const byName = a.meta.name.localeCompare(b.meta.name); // ascending tiebreaker

    if (col === 'name')     return dir * byName;
    if (col === 'type')     return (dir * (a.meta.type || '').localeCompare(b.meta.type || '')) || byName;
    if (col === 'designer') return (dir * (a.meta.designer || '').localeCompare(b.meta.designer || '')) || byName;
    if (col === 'skip')     return (dir * ((a.preview?.skip ?? 0) - (b.preview?.skip ?? 0))) || byName;
    if (col === 'shape')    return (dir * (spectralCentroid(a.preview) - spectralCentroid(b.preview))) || byName;
    if (col === 'heart') {
        // Ascending = favorites on top. (true should compare as "less than" false in asc order.)
        const av = a.meta.isFavorite ? 0 : 1;
        const bv = b.meta.isFavorite ? 0 : 1;
        return (dir * (av - bv)) || byName;
    }
    return byName;
}

// ── Element refs (filled in initUI) ──────────────────────────────────────

const el = {};

function cacheElements() {
    el.browserBtn       = document.getElementById('preset-browser-btn');
    el.favoriteBtn      = document.getElementById('preset-favorite-btn');
    el.nameContainer    = document.getElementById('preset-name-container');
    el.nameSpan         = document.getElementById('preset-name');
    el.modifiedMark     = document.getElementById('preset-modified-indicator');
    el.prevBtn          = document.getElementById('preset-prev-btn');
    el.nextBtn          = document.getElementById('preset-next-btn');
    el.saveBtn          = document.getElementById('preset-save-btn');
    el.dropdown         = document.getElementById('preset-dropdown');
    el.browserModal     = document.getElementById('preset-browser-modal');
    el.saveModal        = document.getElementById('preset-save-modal');
}

// ── Header UI updates ────────────────────────────────────────────────────

function refreshHeader() {
    if (el.nameSpan) el.nameSpan.textContent = state.currentName;
    if (el.modifiedMark) el.modifiedMark.style.display = state.isModified ? 'inline-block' : 'none';
    if (el.saveBtn) el.saveBtn.style.display = state.isModified ? 'inline-block' : 'none';
    if (el.favoriteBtn) {
        el.favoriteBtn.textContent = state.isFavorite ? '♥' : '♡';
        el.favoriteBtn.style.color = state.isFavorite ? '#c74a4a' : 'rgba(0,0,0,0.50)';
    }
}

function setModified(flag) {
    state.isModified = flag;
    refreshHeader();
}

function setCurrentPreset(name, pack, isFav) {
    state.currentName = name;
    state.currentPack = pack;
    state.isFavorite = !!isFav;
    state.isModified = false;
    refreshHeader();
}

// ── Presets cache ────────────────────────────────────────────────────────

async function refreshCache() {
    if (!native.getAllPresets) return;
    try {
        const json = await native.getAllPresets();
        state.presetsCache = json ? JSON.parse(json) : {};
    } catch (err) {
        console.error('getAllPresets failed:', err);
        state.presetsCache = {};
    }
}

async function refreshPacks() {
    if (!native.getAllPacks) return;
    try {
        const json = await native.getAllPacks();
        state.packsCache = json ? JSON.parse(json) : [];
    } catch (err) {
        console.error('getAllPacks failed:', err);
        state.packsCache = [];
    }
}

function flatPresetList() {
    const flat = [];
    if (!state.presetsCache) return flat;
    for (const [packName, list] of Object.entries(state.presetsCache)) {
        for (const p of list) flat.push({ name: p.metadata.name, pack: packName, meta: p.metadata, preview: p.preview });
    }
    return flat;
}

// ── Load / save / favorite / delete ──────────────────────────────────────

async function loadPreset(name, pack) {
    if (!native.loadPreset) return false;
    state.isLoadingPreset = true;
    try {
        await native.loadPreset(name, pack);
        // Refresh cache to pick up the favorite flag for the loaded preset.
        const entry = flatPresetList().find(p => p.name === name && p.pack === pack);
        setCurrentPreset(name, pack, entry ? entry.meta.isFavorite : false);
    } catch (err) {
        console.error('loadPreset failed:', err);
    } finally {
        // Parameter change events will fire as APVTS updates; wait briefly,
        // then clear the flag so subsequent user edits mark the preset modified.
        setTimeout(() => { state.isLoadingPreset = false; }, 200);
    }
    closeAllModals();
    return true;
}

async function savePreset(name, type, designer, description, overwrite) {
    if (!native.savePreset) return '';
    try {
        const savedName = await native.savePreset(name, type, designer, description, !!overwrite);
        if (savedName) {
            await refreshCache();
            setCurrentPreset(savedName, 'User', false);
        }
        return savedName || '';
    } catch (err) {
        console.error('savePreset failed:', err);
        return '';
    }
}

async function setFavorite(name, pack, isFav) {
    if (!native.setFavorite) return;
    try {
        await native.setFavorite(name, pack, !!isFav);
        // Update cache locally; no need to re-fetch everything.
        if (state.presetsCache && state.presetsCache[pack]) {
            const entry = state.presetsCache[pack].find(p => p.metadata.name === name);
            if (entry) entry.metadata.isFavorite = !!isFav;
        }
        if (name === state.currentName && pack === state.currentPack) {
            state.isFavorite = !!isFav;
            refreshHeader();
        }
    } catch (err) {
        console.error('setFavorite failed:', err);
    }
}

async function deletePreset(name, pack) {
    if (!native.deletePreset) return false;
    try {
        const ok = await native.deletePreset(name, pack);
        if (ok) await refreshCache();
        return !!ok;
    } catch (err) {
        console.error('deletePreset failed:', err);
        return false;
    }
}

async function navigatePreset(direction) {
    await refreshCache();
    const flat = flatPresetList();
    if (flat.length === 0) return;
    let idx = flat.findIndex(p => p.name === state.currentName && p.pack === state.currentPack);
    if (idx < 0) idx = 0;
    idx = (idx + direction + flat.length) % flat.length;
    await loadPreset(flat[idx].name, flat[idx].pack);
}

// ── Dropdown (click preset name) — Arturia 2-column layout ───────────────
//
// Left column: categories (All, Favorites, Piano, Drone, Synth, Bass,
// Experimental). Right column: alphabetical preset list filtered by the
// selected category.

let dropdownCategory = 'All';

async function toggleDropdown() {
    if (!el.dropdown) return;
    const isOpen = el.dropdown.style.display && el.dropdown.style.display !== 'none';
    if (isOpen) { el.dropdown.style.display = 'none'; return; }

    el.dropdown.innerHTML = '<div style="padding: 16px; color: rgba(0,0,0,0.50); font-size: 11px;">Loading…</div>';
    el.dropdown.style.display = 'block';

    await refreshCache();

    // Default to the current preset's type if any, else All.
    const flat = flatPresetList();
    const current = flat.find(p => p.name === state.currentName && p.pack === state.currentPack);
    dropdownCategory = current ? (current.meta.type || 'All') : 'All';

    renderDropdown();
}

function renderDropdown() {
    if (!el.dropdown) return;
    const flat = flatPresetList();

    if (flat.length === 0) {
        el.dropdown.innerHTML = '<div style="padding: 16px; color: rgba(0,0,0,0.50); text-align: center; font-size: 11px;">No presets saved yet.<br><span style="font-size: 10px;">Adjust a knob and click Save.</span></div>';
        return;
    }

    // Build the category list: always-show fixed categories + any extras found.
    const fixedCats = ['All', 'Favorites', 'Piano', 'Drone', 'Synth', 'Bass', 'Experimental'];
    const extraCats = new Set();
    for (const p of flat) {
        const t = (p.meta.type || '').trim();
        if (t && !fixedCats.includes(t)) extraCats.add(t);
    }
    const categories = [...fixedCats, ...[...extraCats].sort()];

    // Filter presets for the current category.
    const filtered = flat.filter(p => {
        if (dropdownCategory === 'All') return true;
        if (dropdownCategory === 'Favorites') return !!p.meta.isFavorite;
        return (p.meta.type || '') === dropdownCategory;
    }).sort((a, b) => a.name.localeCompare(b.name, undefined, { sensitivity: 'base' }));

    // Render.
    const leftCol = categories.map(cat => {
        const count = cat === 'All'
            ? flat.length
            : cat === 'Favorites'
                ? flat.filter(p => p.meta.isFavorite).length
                : flat.filter(p => p.meta.type === cat).length;
        const active = cat === dropdownCategory;
        const bg     = active ? 'rgba(0,0,0,0.10)' : 'transparent';
        const col    = active ? 'rgba(0,0,0,0.85)' : 'rgba(0,0,0,0.55)';
        const weight = active ? '500' : '400';
        const mark   = active ? '✓ ' : '';
        const countText = count > 0 ? ` <span style="color:rgba(0,0,0,0.35); font-size:10px;">(${count})</span>` : '';
        return `<div class="dd-cat" data-cat="${escapeAttr(cat)}" style="padding: 5px 10px; cursor: pointer; color: ${col}; background: ${bg}; font-weight: ${weight}; font-size: 11px; display: flex; align-items: center; gap: 4px;">
            <span style="color: #4a7a4a; width: 12px; display: inline-block;">${mark}</span><span>${escapeHtml(cat)}</span>${countText}
        </div>`;
    }).join('');

    const rightCol = filtered.length === 0
        ? `<div style="padding: 16px; color: rgba(0,0,0,0.40); font-size: 11px; text-align: center;">No presets in ${escapeHtml(dropdownCategory)}</div>`
        : filtered.map(p => {
            const isCurrent = p.name === state.currentName && p.pack === state.currentPack;
            const bg  = isCurrent ? 'rgba(0,0,0,0.10)' : 'transparent';
            const col = isCurrent ? 'rgba(0,0,0,0.85)' : 'rgba(0,0,0,0.65)';
            const fav = p.meta.isFavorite
                ? '<span style="color: #c74a4a; margin-right: 6px; flex-shrink: 0;">♥</span>'
                : '<span style="width: 12px; margin-right: 6px; display: inline-block; flex-shrink: 0;"></span>';
            return `<div class="dd-preset" data-name="${escapeAttr(p.name)}" data-pack="${escapeAttr(p.pack)}" style="padding: 5px 10px; cursor: pointer; background: ${bg}; color: ${col}; font-size: 11px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; display: flex; align-items: center;">${fav}<span style="overflow: hidden; text-overflow: ellipsis;">${escapeHtml(p.name)}</span></div>`;
        }).join('');

    el.dropdown.innerHTML = `
        <div style="display: flex; height: 100%;">
            <div style="width: 140px; border-right: 1px solid rgba(0,0,0,0.12); overflow-y: auto; padding: 4px 0;">
                ${leftCol}
            </div>
            <div style="flex: 1; overflow-y: auto; padding: 4px 0; min-width: 180px;">
                ${rightCol}
            </div>
        </div>
    `;

    // Category click handlers.
    el.dropdown.querySelectorAll('.dd-cat').forEach(c => {
        c.addEventListener('click', (e) => {
            e.stopPropagation();
            dropdownCategory = c.getAttribute('data-cat');
            renderDropdown();
        });
        c.addEventListener('mouseover', () => {
            if (c.getAttribute('data-cat') !== dropdownCategory) c.style.background = 'rgba(0,0,0,0.06)';
        });
        c.addEventListener('mouseout', () => {
            if (c.getAttribute('data-cat') !== dropdownCategory) c.style.background = 'transparent';
        });
    });

    // Preset click handlers.
    el.dropdown.querySelectorAll('.dd-preset').forEach(item => {
        item.addEventListener('click', (e) => {
            e.stopPropagation();
            const name = item.getAttribute('data-name');
            const pack = item.getAttribute('data-pack');
            loadPreset(name, pack);
        });
        item.addEventListener('mouseover', () => {
            const n = item.getAttribute('data-name');
            const p = item.getAttribute('data-pack');
            if (!(n === state.currentName && p === state.currentPack)) item.style.background = 'rgba(0,0,0,0.06)';
        });
        item.addEventListener('mouseout', () => {
            const n = item.getAttribute('data-name');
            const p = item.getAttribute('data-pack');
            item.style.background = (n === state.currentName && p === state.currentPack) ? 'rgba(0,0,0,0.10)' : 'transparent';
        });
    });
}

// ── Full browser modal ───────────────────────────────────────────────────

let browserFilter = 'all';  // 'all' | 'factory' | 'user' | 'favorites' | packName

async function toggleBrowser() {
    if (!el.browserModal) return;
    const isOpen = el.browserModal.style.display && el.browserModal.style.display !== 'none';
    if (isOpen) { closeBrowser(); return; }

    await Promise.all([refreshCache(), refreshPacks()]);
    browserFilter = 'all';
    renderBrowser();
    el.browserModal.style.display = 'flex';
    if (el.browserBtn) el.browserBtn.textContent = '✕';
}

function closeBrowser() {
    if (el.browserModal) el.browserModal.style.display = 'none';
    if (el.browserBtn) el.browserBtn.textContent = '|||';
}

function renderBrowser() {
    if (!el.browserModal) return;
    const packs = state.packsCache || [];

    // Sidebar: fixed categories + one entry per pack.
    const categoryItems = [
        { id: 'all',       label: 'Explore' },
        { id: 'favorites', label: 'Favorites' },
    ];
    for (const p of packs) categoryItems.push({ id: p.name, label: p.displayName || p.name });

    const sidebarHtml = categoryItems.map(c => {
        const active = (c.id === browserFilter);
        const bg    = active ? 'rgba(0,0,0,0.08)' : 'transparent';
        const col   = active ? 'rgba(0,0,0,0.85)' : 'rgba(0,0,0,0.60)';
        const border = active ? 'rgba(0,0,0,0.30)' : 'transparent';
        const weight = active ? '500' : '400';
        return `<div class="browser-category" data-filter="${escapeAttr(c.id)}" style="padding: 6px 8px; cursor: pointer; color: ${col}; background: ${bg}; border-left: 3px solid ${border}; font-weight: ${weight}; font-size: 11px;">${escapeHtml(c.label)}</div>`;
    }).join('');

    const isExplore = (browserFilter === 'all');

    // Compute total count for the current view (before any search filter).
    const browserTitle = browserTitleFor(browserFilter);
    const totalCount = countForFilter(browserFilter);

    const mainPane = isExplore
        ? `<div id="browser-explore" style="flex: 1; overflow-y: auto; padding: 16px;"></div>`
        : `<div style="padding: 4px 12px; font-size: 9px; color: rgba(0,0,0,0.40); text-transform: uppercase; border-bottom: 1px solid rgba(0,0,0,0.08); display: grid; grid-template-columns: 1fr 80px 100px 40px; gap: 8px;">
             <div>Name</div><div>Type</div><div>Designer</div><div style="text-align: center;">♡</div>
           </div>
           <div id="browser-list" style="flex: 1; overflow-y: auto; padding: 4px;"></div>`;

    el.browserModal.innerHTML = `
        <div style="display: flex; width: 100%; height: 100%; background: linear-gradient(135deg, #BBBDBF 0%, #AEAFB1 100%); font-family: 'Space Grotesk', system-ui;">
          <div style="width: 160px; background: linear-gradient(135deg, #C5C7C9 0%, #BBBDBF 100%); border-right: 1px solid rgba(0,0,0,0.12); overflow-y: auto; padding: 8px; flex-shrink: 0;">
            <div style="color: rgba(0,0,0,0.50); font-size: 9px; text-transform: uppercase; margin-bottom: 8px; font-weight: 600;">Categories</div>
            ${sidebarHtml}
          </div>
          <div style="flex: 1; display: flex; flex-direction: column; border-right: 1px solid rgba(0,0,0,0.12); min-width: 0;">
            <div style="padding: 12px 14px 6px 14px; display: flex; align-items: baseline; justify-content: space-between;">
              <div style="font-size: 18px; font-weight: 600; color: rgba(0,0,0,0.85); letter-spacing: -0.3px;">${escapeHtml(browserTitle)}</div>
              <div id="browser-count" style="font-size: 11px; color: rgba(0,0,0,0.55); font-variant-numeric: tabular-nums;">${totalCount} preset${totalCount === 1 ? '' : 's'}</div>
            </div>
            <div style="padding: 4px 12px 8px 12px; border-bottom: 1px solid rgba(0,0,0,0.12); display: flex; gap: 6px; align-items: center;">
              <input id="browser-search" type="text" placeholder="${isExplore ? 'Search packs…' : 'Search presets…'}" style="flex: 1; background: rgba(255,255,255,0.6); color: rgba(0,0,0,0.70); border: 1px solid rgba(0,0,0,0.12); padding: 4px 8px; border-radius: 3px; font-size: 11px; font-family: 'Space Grotesk', system-ui;">
              <button id="browser-close" style="background: none; border: none; color: rgba(0,0,0,0.70); cursor: pointer; font-size: 14px; padding: 2px 8px; font-weight: 600;">✕</button>
            </div>
            ${mainPane}
          </div>
          <div style="width: 180px; background: linear-gradient(135deg, #C5C7C9 0%, #BBBDBF 100%); border-left: 1px solid rgba(0,0,0,0.12); padding: 10px; display: flex; flex-direction: column; flex-shrink: 0;">
            <div style="color: rgba(0,0,0,0.50); font-size: 9px; text-transform: uppercase; margin-bottom: 6px; font-weight: 600;">Preview</div>
            <div id="browser-preview" style="background: rgba(255,255,255,0.4); flex: 1; border-radius: 3px; padding: 10px; font-size: 10px; color: rgba(0,0,0,0.70); border: 1px solid rgba(0,0,0,0.10); line-height: 1.6;">${isExplore ? 'Hover a pack for details' : 'Select a preset'}</div>
          </div>
        </div>
    `;

    // Wire handlers.
    el.browserModal.querySelector('#browser-close').addEventListener('click', closeBrowser);
    el.browserModal.querySelectorAll('.browser-category').forEach(c => {
        c.addEventListener('click', () => {
            browserFilter = c.getAttribute('data-filter');
            renderBrowser();
        });
    });
    el.browserModal.querySelector('#browser-search').addEventListener('input', (e) => {
        if (isExplore) renderExplore(e.target.value);
        else renderBrowserList(e.target.value);
    });

    if (isExplore) renderExplore('');
    else renderBrowserList('');
}

function browserTitleFor(filter) {
    if (filter === 'all')       return 'Explore';
    if (filter === 'favorites') return 'Favorites';
    const pack = (state.packsCache || []).find(p => p.name === filter);
    return pack ? (pack.displayName || pack.name) : filter;
}

function countForFilter(filter) {
    const flat = flatPresetList();
    if (filter === 'all')       return flat.length;
    if (filter === 'favorites') return flat.filter(p => p.meta.isFavorite).length;
    return flat.filter(p => p.pack === filter).length;
}

function updateBrowserCount(visibleCount) {
    const el = document.getElementById('browser-count');
    if (!el) return;
    const n = (visibleCount == null) ? countForFilter(browserFilter) : visibleCount;
    el.textContent = `${n} preset${n === 1 ? '' : 's'}`;
}

// Explore view: grid of pack cards with cover art (or gradient fallback).
function renderExplore(searchTerm) {
    const target = document.getElementById('browser-explore');
    if (!target) return;
    const packs = state.packsCache || [];
    const q = (searchTerm || '').toLowerCase().trim();

    const visible = packs.filter(p => {
        if (!q) return true;
        return (p.displayName || p.name).toLowerCase().includes(q) ||
               (p.description || '').toLowerCase().includes(q);
    });

    if (visible.length === 0) {
        target.innerHTML = '<div style="padding: 16px; color: rgba(0,0,0,0.50); text-align: center; font-size: 11px;">No packs found</div>';
        return;
    }

    target.innerHTML = `
        <div style="display: grid; grid-template-columns: repeat(auto-fill, minmax(140px, 1fr)); gap: 14px;">
            ${visible.map(p => packCardHtml(p)).join('')}
        </div>
    `;

    target.querySelectorAll('.pack-card').forEach(card => {
        const packName = card.getAttribute('data-pack');
        card.addEventListener('click', () => {
            browserFilter = packName;
            renderBrowser();
        });
        card.addEventListener('mouseover', () => {
            card.style.transform = 'translateY(-2px)';
            card.style.boxShadow = '0 4px 12px rgba(0,0,0,0.18)';
            updatePackPreview(packName);
        });
        card.addEventListener('mouseout', () => {
            card.style.transform = 'translateY(0)';
            card.style.boxShadow = '0 2px 6px rgba(0,0,0,0.10)';
        });
    });
}

function packCardHtml(pack) {
    const coverUrl = pack.hasCoverArt ? `/pack-cover/${encodeURIComponent(pack.name)}` : null;
    const gradient = packGradient(pack.name);
    const initial  = (pack.displayName || pack.name).charAt(0).toUpperCase();

    const coverInner = coverUrl
        ? `<img src="${escapeAttr(coverUrl)}" style="width: 100%; height: 100%; object-fit: cover; display: block;" onerror="this.replaceWith(Object.assign(document.createElement('div'),{style:'width:100%;height:100%;background:${escapeAttr(gradient)};display:flex;align-items:center;justify-content:center;color:rgba(255,255,255,0.85);font-size:48px;font-weight:300;',textContent:'${escapeAttr(initial)}'}))">`
        : `<div style="width: 100%; height: 100%; background: ${gradient}; display: flex; align-items: center; justify-content: center; color: rgba(255,255,255,0.85); font-size: 48px; font-weight: 300;">${escapeHtml(initial)}</div>`;

    return `
        <div class="pack-card" data-pack="${escapeAttr(pack.name)}" style="cursor: pointer; background: rgba(255,255,255,0.30); border: 1px solid rgba(0,0,0,0.10); border-radius: 6px; overflow: hidden; box-shadow: 0 2px 6px rgba(0,0,0,0.10); transition: transform 0.15s, box-shadow 0.15s;">
            <div style="aspect-ratio: 1 / 1; background: ${gradient};">${coverInner}</div>
            <div style="padding: 8px 10px;">
                <div style="font-size: 11px; font-weight: 600; color: rgba(0,0,0,0.85); margin-bottom: 2px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;">${escapeHtml(pack.displayName || pack.name)}</div>
                <div style="font-size: 9px; color: rgba(0,0,0,0.50);">${pack.presetCount} preset${pack.presetCount === 1 ? '' : 's'}</div>
            </div>
        </div>
    `;
}

// Deterministic gradient per pack name (no images required).
function packGradient(name) {
    let hash = 0;
    for (let i = 0; i < name.length; i++) hash = (hash * 31 + name.charCodeAt(i)) & 0xFFFFFFFF;
    const hue1 = Math.abs(hash) % 360;
    const hue2 = (hue1 + 40) % 360;
    return `linear-gradient(135deg, hsl(${hue1}, 35%, 55%) 0%, hsl(${hue2}, 40%, 42%) 100%)`;
}

function updatePackPreview(packName) {
    const preview = document.getElementById('browser-preview');
    if (!preview) return;
    const pack = (state.packsCache || []).find(p => p.name === packName);
    if (!pack) { preview.textContent = 'Hover a pack for details'; return; }
    preview.innerHTML = `
        <div style="font-size: 12px; font-weight: 600; color: rgba(0,0,0,0.85); margin-bottom: 8px;">${escapeHtml(pack.displayName || pack.name)}</div>
        ${pack.description ? `<div style="margin-bottom: 10px; color: rgba(0,0,0,0.65);">${escapeHtml(pack.description)}</div>` : ''}
        <div><span style="color: rgba(0,0,0,0.50);">Presets:</span> ${pack.presetCount}</div>
        ${pack.designer ? `<div><span style="color: rgba(0,0,0,0.50);">Designer:</span> ${escapeHtml(pack.designer)}</div>` : ''}
    `;
}

function renderBrowserList(searchTerm) {
    const listDiv = document.getElementById('browser-list');
    if (!listDiv) return;
    const cache = state.presetsCache || {};
    const q = (searchTerm || '').toLowerCase().trim();

    // Flatten + filter
    const rows = [];
    for (const [packName, list] of Object.entries(cache)) {
        for (const p of list) {
            if (browserFilter === 'favorites' && !p.metadata.isFavorite) continue;
            if (browserFilter !== 'all' && browserFilter !== 'favorites' && packName !== browserFilter) continue;
            if (q && !p.metadata.name.toLowerCase().includes(q)) continue;
            rows.push({ pack: packName, meta: p.metadata, preview: p.preview });
        }
    }

    updateBrowserCount(rows.length);

    rows.sort(compareRows);

    // Header row (always rendered, even when list is empty — keeps the chrome consistent)
    const headerCols = [
        { id: 'name',     label: 'Name' },
        { id: 'type',     label: 'Type' },
        { id: 'designer', label: 'Designer' },
        { id: 'shape',    label: 'Shape' },
        { id: 'skip',     label: 'Skip' },
        { id: 'heart',    label: '♥', title: 'Sort by favorites' },
    ];
    const arrow = browserSort.dir === 'asc' ? '↑' : '↓';
    const headerHtml = `
        <div class="browser-header">
            ${headerCols.map(c => {
                const active = (browserSort.column === c.id);
                const cls = 'sortable' + (active ? ' active' : '');
                const titleAttr = c.title ? ` title="${escapeAttr(c.title)}"` : '';
                const arrowSpan = active ? ` <span class="arrow">${arrow}</span>` : '';
                return `<span data-sort="${c.id}" class="${cls}"${titleAttr}>${c.label}${arrowSpan}</span>`;
            }).join('')}
        </div>
    `;

    if (rows.length === 0) {
        listDiv.innerHTML = headerHtml +
            '<div style="padding: 16px; color: rgba(0,0,0,0.50); text-align: center; font-size: 11px;">No presets found</div>';
        listDiv.querySelectorAll('.browser-header .sortable').forEach(span => {
            span.addEventListener('click', () => {
                const col = span.getAttribute('data-sort');
                if (!col) return;
                if (browserSort.column === col) {
                    browserSort.dir = browserSort.dir === 'asc' ? 'desc' : 'asc';
                } else {
                    browserSort.column = col;
                    browserSort.dir = 'asc';
                }
                renderBrowserList(document.getElementById('browser-search').value);
            });
        });
        return;
    }

    const rowsHtml = rows.map(r => {
        const isCurrent = r.meta.name === state.currentName && r.pack === state.currentPack;
        const bg = isCurrent ? 'rgba(0,0,0,0.10)' : 'transparent';
        const heart = r.meta.isFavorite ? '♥' : '♡';
        const heartCol = r.meta.isFavorite ? '#c74a4a' : 'rgba(0,0,0,0.40)';
        const skipVal = (r.preview && r.preview.skip) ? String(r.preview.skip) : '—';
        return `
            <div class="browser-row" data-name="${escapeAttr(r.meta.name)}" data-pack="${escapeAttr(r.pack)}"
                 style="display: grid; grid-template-columns: 1fr 72px 72px 140px 40px 30px; gap: 8px; padding: 6px 12px; background: ${bg}; border-radius: 3px; align-items: center; cursor: pointer; font-size: 11px; margin-bottom: 2px;">
                <div style="color: rgba(0,0,0,0.85); font-weight: 500;">${escapeHtml(r.meta.name)}</div>
                <div style="color: rgba(0,0,0,0.60);">${escapeHtml(r.meta.type || '')}</div>
                <div style="color: rgba(0,0,0,0.60);">${escapeHtml(r.meta.designer || '')}</div>
                <svg class="browser-shape" viewBox="0 0 170 26" preserveAspectRatio="none"></svg>
                <div style="color: rgba(0,0,0,0.60); font-variant-numeric: tabular-nums;">${skipVal}</div>
                <div class="browser-heart" style="color: ${heartCol}; text-align: center; cursor: pointer;">${heart}</div>
            </div>
        `;
    }).join('');

    listDiv.innerHTML = headerHtml + rowsHtml;

    // Populate each thumbnail SVG now that the DOM exists.
    listDiv.querySelectorAll('.browser-row').forEach((row, i) => {
        const svg = row.querySelector('.browser-shape');
        if (svg && rows[i].preview && window.PresetSpectrum) {
            window.PresetSpectrum.render(svg, rows[i].preview, { variant: 'thumbnail' });
        }

        const name = row.getAttribute('data-name');
        const pack = row.getAttribute('data-pack');
        row.addEventListener('click', () => loadPreset(name, pack));
        row.addEventListener('mouseover', () => {
            const rn = row.getAttribute('data-name');
            const rp = row.getAttribute('data-pack');
            const isCur = rn === state.currentName && rp === state.currentPack;
            if (!isCur) row.style.background = 'rgba(0,0,0,0.06)';
            updatePreview(rn, rp);
        });
        row.addEventListener('mouseout', () => {
            const rn = row.getAttribute('data-name');
            const rp = row.getAttribute('data-pack');
            const isCur = rn === state.currentName && rp === state.currentPack;
            row.style.background = isCur ? 'rgba(0,0,0,0.10)' : 'transparent';
        });

        const heart = row.querySelector('.browser-heart');
        heart.addEventListener('click', async (e) => {
            e.stopPropagation();
            const entry = flatPresetList().find(p => p.name === name && p.pack === pack);
            const newFav = entry ? !entry.meta.isFavorite : true;
            await setFavorite(name, pack, newFav);
            renderBrowserList(document.getElementById('browser-search').value);
        });
    });

    listDiv.querySelectorAll('.browser-header .sortable').forEach(span => {
        span.addEventListener('click', () => {
            const col = span.getAttribute('data-sort');
            if (!col) return;
            if (browserSort.column === col) {
                browserSort.dir = browserSort.dir === 'asc' ? 'desc' : 'asc';
            } else {
                browserSort.column = col;
                browserSort.dir = 'asc';
            }
            renderBrowserList(document.getElementById('browser-search').value);
        });
    });
}

function updatePreview(name, pack) {
    const preview = document.getElementById('browser-preview');
    if (!preview) return;
    const entry = flatPresetList().find(p => p.name === name && p.pack === pack);
    if (!entry) { preview.textContent = 'Select a preset'; return; }
    preview.innerHTML = `
        <svg class="preview-spectrum" viewBox="0 0 280 56" preserveAspectRatio="none"
             style="width:100%;height:56px;display:block;margin-bottom:10px;"></svg>
        <div style="font-size: 12px; font-weight: 600; color: rgba(0,0,0,0.85); margin-bottom: 8px;">${escapeHtml(entry.meta.name)}</div>
        <div><span style="color: rgba(0,0,0,0.50);">Type:</span> ${escapeHtml(entry.meta.type || '—')}</div>
        <div><span style="color: rgba(0,0,0,0.50);">Designer:</span> ${escapeHtml(entry.meta.designer || '—')}</div>
        <div><span style="color: rgba(0,0,0,0.50);">Pack:</span> ${escapeHtml(entry.pack)}</div>
        ${entry.meta.description ? `<div style="margin-top: 10px; padding-top: 10px; border-top: 1px solid rgba(0,0,0,0.10); color: rgba(0,0,0,0.65); white-space: pre-wrap;">${escapeHtml(entry.meta.description)}</div>` : ''}
        ${entry.pack === 'User' ? `<div style="margin-top: 10px;"><button class="preview-delete" style="background: linear-gradient(135deg, #BBBDBF 0%, #AEAFB1 100%); color: rgba(0,0,0,0.70); border: 1px solid rgba(0,0,0,0.12); padding: 4px 10px; border-radius: 3px; cursor: pointer; font-size: 10px; font-family: 'Space Grotesk', system-ui;">Delete</button></div>` : ''}
    `;

    const svg = preview.querySelector('.preview-spectrum');
    if (svg && entry.preview && window.PresetSpectrum) {
        window.PresetSpectrum.render(svg, entry.preview, { variant: 'preview' });
    }

    const btn = preview.querySelector('.preview-delete');
    if (btn) {
        btn.addEventListener('click', async () => {
            if (!confirm(`Delete "${entry.meta.name}"?`)) return;
            await deletePreset(entry.meta.name, entry.pack);
            renderBrowserList(document.getElementById('browser-search').value);
            preview.textContent = 'Select a preset';
        });
    }
}

// ── Save modal ───────────────────────────────────────────────────────────

function openSaveModal() {
    if (!el.saveModal) return;
    const isFactory = state.currentPack === 'Factory';

    el.saveModal.innerHTML = `
        <div style="background: linear-gradient(135deg, #C5C7C9 0%, #B8B9BB 100%); border: 1px solid rgba(0,0,0,0.15); border-radius: 6px; padding: 16px; width: 100%; max-width: 340px; font-family: 'Space Grotesk', system-ui; font-size: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.15), inset 0 1px 0 rgba(255,255,255,0.6);">
            <div style="color: rgba(0,0,0,0.70); font-weight: 600; margin-bottom: 12px;">Save Preset</div>
            <div style="margin-bottom: 12px;">
                <label style="display: block; color: rgba(0,0,0,0.55); font-size: 10px; text-transform: uppercase; margin-bottom: 4px;">Preset Name</label>
                <input id="save-name" type="text" value="${escapeAttr(state.currentName)}" style="width: 100%; background: rgba(255,255,255,0.6); color: rgba(0,0,0,0.70); border: 1px solid rgba(0,0,0,0.12); padding: 6px 8px; border-radius: 3px; font-size: 12px; box-sizing: border-box; font-family: 'Space Grotesk', system-ui;">
            </div>
            <div style="margin-bottom: 12px;">
                <label style="display: block; color: rgba(0,0,0,0.55); font-size: 10px; text-transform: uppercase; margin-bottom: 4px;">Type</label>
                <select id="save-type" style="width: 100%; background: rgba(255,255,255,0.6); color: rgba(0,0,0,0.70); border: 1px solid rgba(0,0,0,0.12); padding: 6px 8px; border-radius: 3px; font-size: 12px; box-sizing: border-box; font-family: 'Space Grotesk', system-ui;">
                    <option>Piano</option>
                    <option>Drone</option>
                    <option>Synth</option>
                    <option>Bass</option>
                    <option selected>Experimental</option>
                </select>
            </div>
            <div style="margin-bottom: 12px;">
                <label style="display: block; color: rgba(0,0,0,0.55); font-size: 10px; text-transform: uppercase; margin-bottom: 4px;">Designer</label>
                <input id="save-designer" type="text" value="User" style="width: 100%; background: rgba(255,255,255,0.6); color: rgba(0,0,0,0.70); border: 1px solid rgba(0,0,0,0.12); padding: 6px 8px; border-radius: 3px; font-size: 12px; box-sizing: border-box; font-family: 'Space Grotesk', system-ui;">
            </div>
            <div style="margin-bottom: 12px;">
                <label style="display: block; color: rgba(0,0,0,0.55); font-size: 10px; text-transform: uppercase; margin-bottom: 4px;">Description</label>
                <textarea id="save-description" rows="3" placeholder="Optional notes about this preset…" style="width: 100%; background: rgba(255,255,255,0.6); color: rgba(0,0,0,0.70); border: 1px solid rgba(0,0,0,0.12); padding: 6px 8px; border-radius: 3px; font-size: 12px; box-sizing: border-box; font-family: 'Space Grotesk', system-ui; resize: vertical;"></textarea>
            </div>
            <div style="margin-bottom: 12px; font-size: 10px; color: rgba(0,0,0,0.60);">
                <input id="save-overwrite" type="checkbox" ${isFactory ? 'disabled' : ''}>
                <label for="save-overwrite">Overwrite existing preset${isFactory ? ' (factory presets cannot be overwritten)' : ''}</label>
            </div>
            <div style="display: flex; gap: 6px;">
                <button id="save-confirm" style="flex: 1; background: linear-gradient(135deg, #B8B9BB 0%, #9A9C9E 100%); color: rgba(0,0,0,0.75); border: 1px solid rgba(0,0,0,0.10); padding: 6px; border-radius: 3px; cursor: pointer; font-weight: 600; font-size: 11px; box-shadow: 0 2px 4px rgba(0,0,0,0.08), inset 0 1px 0 rgba(255,255,255,0.5); font-family: 'Space Grotesk', system-ui;">Save</button>
                <button id="save-cancel" style="flex: 1; background: linear-gradient(135deg, #BBBDBF 0%, #AEAFB1 100%); color: rgba(0,0,0,0.60); border: 1px solid rgba(0,0,0,0.10); padding: 6px; border-radius: 3px; cursor: pointer; font-size: 11px; box-shadow: 0 2px 4px rgba(0,0,0,0.06), inset 0 1px 0 rgba(255,255,255,0.5); font-family: 'Space Grotesk', system-ui;">Cancel</button>
            </div>
        </div>
    `;
    el.saveModal.style.display = 'flex';

    const nameInput = document.getElementById('save-name');
    nameInput.focus();
    nameInput.select();

    document.getElementById('save-confirm').addEventListener('click', async () => {
        const name = nameInput.value.trim();
        if (!name) return;
        const type        = document.getElementById('save-type').value;
        const designer    = document.getElementById('save-designer').value.trim() || 'User';
        const description = document.getElementById('save-description').value.trim();
        const overwrite   = document.getElementById('save-overwrite').checked;

        await savePreset(name, type, designer, description, overwrite);
        el.saveModal.style.display = 'none';
    });
    document.getElementById('save-cancel').addEventListener('click', () => {
        el.saveModal.style.display = 'none';
    });
}

// ── Modals helper ────────────────────────────────────────────────────────

function closeAllModals() {
    if (el.dropdown) el.dropdown.style.display = 'none';
    closeBrowser();
    if (el.saveModal) el.saveModal.style.display = 'none';
}

// ── Parameter change → modified indicator ────────────────────────────────

function wireParameterListeners() {
    if (!window.Juce || !window.__JUCE__ || !window.__JUCE__.initialisationData) return;
    const data = window.__JUCE__.initialisationData;

    const onChange = () => {
        if (!state.isLoadingPreset) setModified(true);
    };

    if (window.Juce.getSliderState) {
        (data.__juce__sliders || []).forEach(id => {
            const s = window.Juce.getSliderState(id);
            if (s && s.valueChangedEvent) s.valueChangedEvent.addListener(onChange);
        });
    }
    if (window.Juce.getToggleState) {
        (data.__juce__toggles || []).forEach(id => {
            const t = window.Juce.getToggleState(id);
            if (t && t.valueChangedEvent) t.valueChangedEvent.addListener(onChange);
        });
    }
    if (window.Juce.getComboBoxState) {
        (data.__juce__comboBoxes || []).forEach(id => {
            const c = window.Juce.getComboBoxState(id);
            if (c && c.valueChangedEvent) c.valueChangedEvent.addListener(onChange);
        });
    }
}

// ── HTML escape helpers ──────────────────────────────────────────────────

function escapeHtml(s) {
    return String(s).replace(/[&<>"']/g, c => ({
        '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'
    }[c]));
}
function escapeAttr(s) { return escapeHtml(s); }

// ── Initialization ───────────────────────────────────────────────────────

function initClickHandlers() {
    if (el.browserBtn)    el.browserBtn.addEventListener('click', toggleBrowser);
    if (el.favoriteBtn)   el.favoriteBtn.addEventListener('click', async (e) => {
        e.stopPropagation();  // don't let this bubble to the pill container
        if (!state.currentName || !state.currentPack) return;  // nothing to favorite
        await setFavorite(state.currentName, state.currentPack, !state.isFavorite);
    });
    if (el.nameContainer) el.nameContainer.addEventListener('click', (e) => {
        e.stopPropagation();
        toggleDropdown();
    });
    if (el.prevBtn)       el.prevBtn.addEventListener('click', () => navigatePreset(-1));
    if (el.nextBtn)       el.nextBtn.addEventListener('click', () => navigatePreset(1));
    if (el.saveBtn)       el.saveBtn.addEventListener('click', openSaveModal);

    // Close dropdown when clicking elsewhere.
    document.addEventListener('click', (e) => {
        if (!el.dropdown) return;
        if (el.dropdown.style.display === 'none') return;
        if (el.nameContainer && el.nameContainer.contains(e.target)) return;
        if (el.dropdown.contains(e.target)) return;
        el.dropdown.style.display = 'none';
    });
}

function init() {
    cacheElements();
    if (!initNativeBridge()) return;
    initClickHandlers();
    wireParameterListeners();
    wireFocusTracking();
    refreshHeader();
    // Prime the cache in the background.
    refreshCache();
}

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
} else {
    init();
}

})();
