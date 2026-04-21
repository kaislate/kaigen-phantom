/**
 * preset-spectrum.js — renders the preset-preview harmonic fingerprint.
 *
 * Gaussian-peak-sum curve through H2..H8 on a log-frequency axis, plus a
 * dashed crossover marker labeled in Hz. Monochrome ink on the preset
 * browser's translucent preview panel.
 *
 * Data contract:
 *   preview = { h: [7 floats 0..1], crossover: Hz }
 *
 * Uses inline SVG so the browser resolution-scales automatically and no
 * canvas resize logic is needed.
 */
(function(){

const DISPLAY_FREQ_LOW = 30;
const SIGMA            = 0.04; // Gaussian peak width in log10(Hz)
const SAMPLES          = 48;

// Same log-frequency mapping the main spectrum analyzer uses.
function freqToX(freq, width, displayHigh) {
    const logLo = Math.log10(DISPLAY_FREQ_LOW);
    const logHi = Math.log10(displayHigh);
    const f = Math.max(freq, DISPLAY_FREQ_LOW);
    return ((Math.log10(f) - logLo) / (logHi - logLo)) * width;
}

// Sum of seven Gaussian peaks at 2f..8f where f = crossover.
function evaluate(freq, preview) {
    const fundamental = Math.max(10, preview.crossover || 80);
    const logF = Math.log10(freq);
    let s = 0;
    for (let i = 0; i < 7; ++i) {
        const centerHz = (i + 2) * fundamental;
        const d = (logF - Math.log10(centerHz)) / SIGMA;
        s += (preview.h[i] || 0) * Math.exp(-d * d);
    }
    return Math.max(0, Math.min(1, s));
}

// Pick three axis labels from a fixed set, keeping those that fit inside
// [DISPLAY_FREQ_LOW, displayHigh] and are visually spread.
function pickAxisLabels(displayHigh) {
    const candidates = [
        { hz: 30,    label: '30' },
        { hz: 100,   label: '100' },
        { hz: 300,   label: '300' },
        { hz: 1000,  label: '1k' },
        { hz: 3000,  label: '3k' },
        { hz: 10000, label: '10k' },
    ];
    const inRange = candidates.filter(c => c.hz >= DISPLAY_FREQ_LOW && c.hz <= displayHigh);
    if (inRange.length <= 3) return inRange;
    // Evenly pick three: first, middle, last
    return [inRange[0], inRange[Math.floor(inRange.length / 2)], inRange[inRange.length - 1]];
}

// Build a smooth path (quadratic-bezier through midpoints) from an array
// of points. Matches spectrum.js:strokeCurve technique.
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

// Escape text for safe insertion as SVG content.
function esc(s) { return String(s).replace(/[&<>]/g, c => ({ '&':'&amp;','<':'&lt;','>':'&gt;' }[c])); }

function render(svgEl, preview) {
    if (!svgEl || !preview || !Array.isArray(preview.h)) return;

    const W = 280;
    const H = 56;
    svgEl.setAttribute('viewBox', `0 0 ${W} ${H}`);
    svgEl.setAttribute('preserveAspectRatio', 'none');

    const crossover = Math.max(10, preview.crossover || 80);
    const displayHigh = Math.max(1000, 10 * crossover);

    // Build curve samples.
    const logLo = Math.log10(DISPLAY_FREQ_LOW);
    const logHi = Math.log10(displayHigh);
    const pts = [];
    for (let i = 0; i < SAMPLES; ++i) {
        const t = i / (SAMPLES - 1);
        const f = Math.pow(10, logLo + (logHi - logLo) * t);
        const x = freqToX(f, W, displayHigh);
        const y = H * (1 - evaluate(f, preview));
        pts.push({ x, y });
    }

    const curvePath = buildPath(pts);
    const fillPath  = `${curvePath} L ${W.toFixed(2)} ${H} L 0 ${H} Z`;

    const xoverX = freqToX(crossover, W, displayHigh);
    const xoverLabel = crossover < 100
        ? `${crossover.toFixed(0)}Hz`
        : `${(crossover / 1000).toFixed(crossover < 1000 ? 0 : 1)}${crossover < 1000 ? 'Hz' : 'kHz'}`;

    const axisLabels = pickAxisLabels(displayHigh);

    // Build the SVG content as an innerHTML blob — simpler than DOM building,
    // and safer because all inputs are numeric or escaped.
    const guideLines = [14, 28, 42].map(y =>
        `<line x1="0" y1="${y}" x2="${W}" y2="${y}" stroke="rgba(0,0,0,0.05)" stroke-width="1"/>`
    ).join('');

    const axisLabelsSvg = axisLabels.map(a => {
        const ax = freqToX(a.hz, W, displayHigh);
        return `<text x="${ax.toFixed(2)}" y="52" font-size="6" fill="rgba(0,0,0,0.38)" font-family="monospace" text-anchor="middle">${esc(a.label)}</text>`;
    }).join('');

    svgEl.innerHTML = `
        <defs>
            <linearGradient id="preset-spec-fill" x1="0" y1="0" x2="0" y2="1">
                <stop offset="0%" stop-color="rgba(0,0,0,0.22)"/>
                <stop offset="100%" stop-color="rgba(0,0,0,0.02)"/>
            </linearGradient>
        </defs>
        ${guideLines}
        <line x1="${xoverX.toFixed(2)}" y1="2" x2="${xoverX.toFixed(2)}" y2="50"
              stroke="rgba(0,0,0,0.30)" stroke-width="0.6" stroke-dasharray="2 2"/>
        <text x="${(xoverX + 2).toFixed(2)}" y="9" font-size="5.5"
              fill="rgba(0,0,0,0.40)" font-family="monospace">${esc(xoverLabel)}</text>
        <path d="${fillPath}" fill="url(#preset-spec-fill)"/>
        <path d="${curvePath}" fill="none" stroke="rgba(0,0,0,0.72)" stroke-width="1.2"
              stroke-linejoin="round" stroke-linecap="round"/>
        ${axisLabelsSvg}
    `;
}

window.PresetSpectrum = { render };

})();
