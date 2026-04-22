/**
 * preset-spectrum.js — renders the preset-preview harmonic fingerprint.
 *
 * Gaussian-peak-sum curve through H2..H8 on a log-frequency axis, plus a
 * dashed crossover marker. Monochrome ink on the preset browser's translucent
 * preview panel.
 *
 * Data contract:
 *   preview = { h: [7 floats 0..1], crossover: Hz, skip: int 0..8 }
 *
 * Call signature:
 *   render(svgEl, preview)                           → preview-card variant (default)
 *   render(svgEl, preview, { variant: 'thumbnail' }) → stripped for browser rows
 *
 * If the effective fundamental (crossover / 2^skip) drops below 6 Hz,
 * the curve is replaced by a big muted "SUB BASS" label.
 */
(function(){

const DISPLAY_FREQ_LOW  = 30;
const DISPLAY_FREQ_HIGH = 10000;
const SIGMA             = 0.04; // Gaussian peak width in log10(Hz)
const SAMPLES           = 48;
const SUB_BASS_CUTOFF   = 6.0;  // Hz — below this, show SUB BASS instead of curve

function esc(s) { return String(s).replace(/[&<>]/g, c => ({ '&':'&amp;','<':'&lt;','>':'&gt;' }[c])); }

function freqToX(freq, width) {
    const logLo = Math.log10(DISPLAY_FREQ_LOW);
    const logHi = Math.log10(DISPLAY_FREQ_HIGH);
    const f = Math.max(freq, DISPLAY_FREQ_LOW);
    return ((Math.log10(f) - logLo) / (logHi - logLo)) * width;
}

function effectiveFundamental(preview) {
    const cross = Math.max(10, preview.crossover || 120);
    const skip  = Math.max(0, preview.skip || 0);
    return cross / Math.pow(2, skip);
}

function evaluate(freq, preview, fundamental) {
    const logF = Math.log10(freq);
    let s = 0;
    for (let i = 0; i < 7; ++i) {
        const centerHz = (i + 2) * fundamental;
        const d = (logF - Math.log10(centerHz)) / SIGMA;
        s += (preview.h[i] || 0) * Math.exp(-d * d);
    }
    return Math.max(0, Math.min(1, s));
}

function pickAxisLabels() {
    // Fixed display range means fixed label choices.
    return [
        { hz: 30,    label: '30' },
        { hz: 300,   label: '300' },
        { hz: 3000,  label: '3k' },
    ];
}

function buildPath(pts) {
    if (pts.length < 2) return '';
    let d = `M ${pts[0].x.toFixed(2)} ${pts[0].y.toFixed(2)}`;
    for (let i = 1; i < pts.length - 1; ++i) {
        const xc = (pts[i].x + pts[i + 1].x) / 2;
        const yc = (pts[i].y + pts[i + 1].y) / 2;
        d += ` Q ${pts[i].x.toFixed(2)} ${pts[i].y.toFixed(2)} ${xc.toFixed(2)} ${yc.toFixed(2)}`;
    }
    const last = pts[pts.length - 1];
    d += ` L ${last.x.toFixed(2)} ${last.y.toFixed(2)}`;
    return d;
}

function renderSubBassLabel(svgEl, variant) {
    const isThumbnail = variant === 'thumbnail';
    const W = isThumbnail ? 170 : 280;
    const H = isThumbnail ? 26  : 56;
    const fontSize = isThumbnail ? 10 : 20;
    const spacing  = isThumbnail ? '0.08em' : '0.15em';
    svgEl.setAttribute('viewBox', `0 0 ${W} ${H}`);
    svgEl.setAttribute('preserveAspectRatio', 'none');
    svgEl.innerHTML = `
        <text x="${W/2}" y="${H/2 + fontSize/3.5}" text-anchor="middle"
              font-size="${fontSize}" font-weight="700" letter-spacing="${spacing}"
              fill="rgba(0,0,0,0.22)"
              font-family="'Space Grotesk', system-ui, sans-serif">SUB BASS</text>
    `;
}

function render(svgEl, preview, options) {
    if (!svgEl || !preview || !Array.isArray(preview.h)) return;
    options = options || {};
    const isThumbnail = options.variant === 'thumbnail';
    const variant     = isThumbnail ? 'thumbnail' : 'preview';

    const fundamentalHz = effectiveFundamental(preview);
    if (fundamentalHz < SUB_BASS_CUTOFF) {
        renderSubBassLabel(svgEl, variant);
        return;
    }

    const W = isThumbnail ? 170 : 280;
    const H = isThumbnail ? 26  : 56;
    svgEl.setAttribute('viewBox', `0 0 ${W} ${H}`);
    svgEl.setAttribute('preserveAspectRatio', 'none');

    const crossover = Math.max(10, preview.crossover || 120);

    // Build curve samples across the fixed display range.
    const logLo = Math.log10(DISPLAY_FREQ_LOW);
    const logHi = Math.log10(DISPLAY_FREQ_HIGH);
    const pts = [];
    for (let i = 0; i < SAMPLES; ++i) {
        const t = i / (SAMPLES - 1);
        const f = Math.pow(10, logLo + (logHi - logLo) * t);
        const x = freqToX(f, W);
        const y = H * (1 - evaluate(f, preview, fundamentalHz));
        pts.push({ x, y });
    }

    const curvePath = buildPath(pts);
    const fillPath  = `${curvePath} L ${W.toFixed(2)} ${H} L 0 ${H} Z`;

    const xoverInRange = crossover >= DISPLAY_FREQ_LOW && crossover <= DISPLAY_FREQ_HIGH;
    const xoverX = xoverInRange ? freqToX(crossover, W) : -1;

    // Variant-specific chrome: thumbnail drops axis labels, Hz label, and guide lines.
    const guideLines = isThumbnail ? '' : [14, 28, 42].map(y =>
        `<line x1="0" y1="${y}" x2="${W}" y2="${y}" stroke="rgba(0,0,0,0.05)" stroke-width="1"/>`
    ).join('');

    const xoverMarker = xoverInRange
        ? (isThumbnail
            ? `<line x1="${xoverX.toFixed(2)}" y1="1" x2="${xoverX.toFixed(2)}" y2="${H - 1}" stroke="rgba(0,0,0,0.28)" stroke-width="0.5" stroke-dasharray="1.5 1.5"/>`
            : `<line x1="${xoverX.toFixed(2)}" y1="2" x2="${xoverX.toFixed(2)}" y2="50" stroke="rgba(0,0,0,0.30)" stroke-width="0.6" stroke-dasharray="2 2"/>`)
        : '';

    const xoverLabel = (!isThumbnail && xoverInRange)
        ? (() => {
              const lbl = crossover < 100
                  ? `${crossover.toFixed(0)}Hz`
                  : `${(crossover / 1000).toFixed(crossover < 1000 ? 0 : 1)}${crossover < 1000 ? 'Hz' : 'kHz'}`;
              return `<text x="${(xoverX + 2).toFixed(2)}" y="9" font-size="5.5" fill="rgba(0,0,0,0.40)" font-family="monospace">${esc(lbl)}</text>`;
          })()
        : '';

    const axisLabelsSvg = isThumbnail ? '' : pickAxisLabels().map(a => {
        const ax = freqToX(a.hz, W);
        return `<text x="${ax.toFixed(2)}" y="52" font-size="6" fill="rgba(0,0,0,0.38)" font-family="monospace" text-anchor="middle">${esc(a.label)}</text>`;
    }).join('');

    const curveStroke = isThumbnail ? 1.0 : 1.2;
    const fillGradientId = isThumbnail ? 'preset-spec-fill-thumb' : 'preset-spec-fill';

    svgEl.innerHTML = `
        <defs>
            <linearGradient id="${fillGradientId}" x1="0" y1="0" x2="0" y2="1">
                <stop offset="0%" stop-color="rgba(0,0,0,0.22)"/>
                <stop offset="100%" stop-color="rgba(0,0,0,0.02)"/>
            </linearGradient>
        </defs>
        ${guideLines}
        ${xoverMarker}
        ${xoverLabel}
        <path d="${fillPath}" fill="url(#${fillGradientId})"/>
        <path d="${curvePath}" fill="none" stroke="rgba(0,0,0,0.72)" stroke-width="${curveStroke}"
              stroke-linejoin="round" stroke-linecap="round"/>
        ${axisLabelsSvg}
    `;
}

window.PresetSpectrum = { render };

})();
