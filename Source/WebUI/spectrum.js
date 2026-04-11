/**
 * spectrum.js — Professional EQ-style spectrum analyzer
 * Smooth curve over log-frequency axis (20Hz - 20kHz), dB scale (-60 to 0),
 * with fill beneath the curve.
 */

(function(){
const specCanvas    = document.getElementById('spectrumCanvas');
const meterInCanvas = document.getElementById('meterIn');
const meterOutCanvas= document.getElementById('meterOut');

if (!specCanvas) return;

// ─── Resize canvases to match CSS size at device pixel ratio ────────────────
function resize() {
    const pr = window.devicePixelRatio || 1;
    for (const c of [specCanvas, meterInCanvas, meterOutCanvas]) {
        if (!c) continue;
        const r = c.getBoundingClientRect();
        c.width  = Math.max(1, Math.floor(r.width  * pr));
        c.height = Math.max(1, Math.floor(r.height * pr));
    }
}
resize();
window.addEventListener('resize', resize);

// ─── State ──────────────────────────────────────────────────────────────────
const BIN_COUNT = 80;
const BIN_FREQ_LOW  = 30;     // matches C++ FFT binning
const BIN_FREQ_HIGH = 16000;
const DISPLAY_FREQ_LOW  = 20;    // display axis
const DISPLAY_FREQ_HIGH = 20000;

const smoothed = new Float32Array(BIN_COUNT);
const peakHold = new Float32Array(BIN_COUNT);

let inL = 0, inR = 0, outL = 0, outR = 0;
let inLSmooth = 0, inRSmooth = 0, outLSmooth = 0, outRSmooth = 0;

const SMOOTH_UP = 0.5;    // fast attack
const SMOOTH_DN = 0.08;   // slow release
const PEAK_DECAY = 0.003;

// ─── Frequency → X position (log scale) ─────────────────────────────────────
function freqToX(freq, w) {
    const logLo = Math.log10(DISPLAY_FREQ_LOW);
    const logHi = Math.log10(DISPLAY_FREQ_HIGH);
    const logF  = Math.log10(Math.max(freq, DISPLAY_FREQ_LOW));
    return ((logF - logLo) / (logHi - logLo)) * w;
}

// ─── Bin index → frequency (matches C++ log-spaced binning) ─────────────────
function binToFreq(bin) {
    const logLo = Math.log10(BIN_FREQ_LOW);
    const logHi = Math.log10(BIN_FREQ_HIGH);
    return Math.pow(10, logLo + (logHi - logLo) * (bin + 0.5) / BIN_COUNT);
}

// ─── Grid + labels ──────────────────────────────────────────────────────────
const FREQ_LABELS = [
    { hz: 30,   label: '30'  },
    { hz: 100,  label: '100' },
    { hz: 300,  label: '300' },
    { hz: 1000, label: '1k'  },
    { hz: 3000, label: '3k'  },
    { hz: 10000,label: '10k' },
];
const DB_LINES = [-12, -24, -36, -48];

function drawGrid(ctx, w, h) {
    ctx.save();
    ctx.lineWidth = 1;

    // Horizontal dB grid
    ctx.strokeStyle = 'rgba(255,255,255,0.04)';
    for (const db of DB_LINES) {
        const y = Math.round(h * (-db) / 60) + 0.5;
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(w, y);
        ctx.stroke();
    }

    // Vertical freq grid at major frequencies
    const majors = [50, 100, 200, 500, 1000, 2000, 5000, 10000];
    ctx.strokeStyle = 'rgba(255,255,255,0.035)';
    for (const f of majors) {
        const x = Math.round(freqToX(f, w)) + 0.5;
        ctx.beginPath();
        ctx.moveTo(x, 0);
        ctx.lineTo(x, h);
        ctx.stroke();
    }

    // Frequency labels
    const labelFontPx = Math.max(9, Math.round(h * 0.04));
    ctx.font = labelFontPx + 'px monospace';
    ctx.fillStyle = 'rgba(255,255,255,0.22)';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'bottom';
    for (const { hz, label } of FREQ_LABELS) {
        const x = freqToX(hz, w);
        if (x < 4 || x > w - 4) continue;
        ctx.fillText(label, x, h - 2);
    }

    // dB labels (right edge)
    ctx.textAlign = 'right';
    ctx.textBaseline = 'middle';
    for (const db of [0, -12, -24, -36, -48]) {
        const y = h * (-db) / 60;
        ctx.fillText(db + 'dB', w - 3, y);
    }

    ctx.restore();
}

// ─── Main spectrum draw ─────────────────────────────────────────────────────
function drawSpectrum() {
    if (!specCanvas) return;
    const ctx = specCanvas.getContext('2d');
    if (!ctx) return;

    const w = specCanvas.width;
    const h = specCanvas.height;

    // Clear with solid black
    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, w, h);

    drawGrid(ctx, w, h);

    // Build smooth path: one point per bin at its center freq
    // Skip bins outside display range
    const points = [];
    for (let i = 0; i < BIN_COUNT; i++) {
        const f = binToFreq(i);
        if (f < DISPLAY_FREQ_LOW || f > DISPLAY_FREQ_HIGH) continue;
        const x = freqToX(f, w);
        // smoothed[i] is 0..1 where 1 = 0dB, 0 = -60dB
        const y = h * (1 - smoothed[i]);
        points.push({ x, y });
    }

    if (points.length < 2) return;

    // Fill under curve
    ctx.beginPath();
    ctx.moveTo(points[0].x, h);
    for (const p of points) ctx.lineTo(p.x, p.y);
    ctx.lineTo(points[points.length - 1].x, h);
    ctx.closePath();
    const fillGrad = ctx.createLinearGradient(0, 0, 0, h);
    fillGrad.addColorStop(0, 'rgba(255,255,255,0.28)');
    fillGrad.addColorStop(0.5, 'rgba(255,255,255,0.12)');
    fillGrad.addColorStop(1, 'rgba(255,255,255,0.02)');
    ctx.fillStyle = fillGrad;
    ctx.fill();

    // Stroke curve on top
    ctx.beginPath();
    ctx.moveTo(points[0].x, points[0].y);
    // Smooth curve via quadratic midpoints
    for (let i = 1; i < points.length - 1; i++) {
        const xc = (points[i].x + points[i + 1].x) / 2;
        const yc = (points[i].y + points[i + 1].y) / 2;
        ctx.quadraticCurveTo(points[i].x, points[i].y, xc, yc);
    }
    ctx.lineTo(points[points.length - 1].x, points[points.length - 1].y);
    ctx.strokeStyle = 'rgba(255,255,255,0.85)';
    ctx.lineWidth = Math.max(1.2, h * 0.006);
    ctx.shadowColor = 'rgba(255,255,255,0.6)';
    ctx.shadowBlur = 4;
    ctx.stroke();
    ctx.shadowBlur = 0;

    // Peak hold line (dim, no fill)
    const peakPoints = [];
    for (let i = 0; i < BIN_COUNT; i++) {
        const f = binToFreq(i);
        if (f < DISPLAY_FREQ_LOW || f > DISPLAY_FREQ_HIGH) continue;
        const x = freqToX(f, w);
        const y = h * (1 - peakHold[i]);
        peakPoints.push({ x, y });
    }
    if (peakPoints.length >= 2) {
        ctx.beginPath();
        ctx.moveTo(peakPoints[0].x, peakPoints[0].y);
        for (let i = 1; i < peakPoints.length; i++) {
            ctx.lineTo(peakPoints[i].x, peakPoints[i].y);
        }
        ctx.strokeStyle = 'rgba(255,255,255,0.25)';
        ctx.lineWidth = 1;
        ctx.stroke();
    }
}

// ─── Meter drawing ──────────────────────────────────────────────────────────
function drawMeter(canvas, level, peak, label) {
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    const w = canvas.width;
    const h = canvas.height;

    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, w, h);

    const fillH = level * h * 0.9;
    if (fillH > 0.5) {
        const grad = ctx.createLinearGradient(0, h, 0, h - fillH);
        grad.addColorStop(0, 'rgba(255,255,255,0.55)');
        grad.addColorStop(1, 'rgba(255,255,255,0.12)');
        ctx.fillStyle = grad;
        ctx.fillRect(1, h - fillH, w - 2, fillH);
    }

    // Peak hold
    if (peak > 0) {
        const py = h - peak * h * 0.9;
        ctx.fillStyle = 'rgba(255,255,255,0.7)';
        ctx.fillRect(1, py, w - 2, 1);
    }

    // Label
    ctx.fillStyle = 'rgba(255,255,255,0.25)';
    ctx.font = Math.max(8, Math.round(h * 0.04)) + 'px monospace';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'bottom';
    ctx.fillText(label, w / 2, h - 2);
}

// ─── Data ingest (from phantom.js polling) ──────────────────────────────────
document.addEventListener('spectrum-data', (e) => {
    const data = e.detail;
    if (!data || data.length < BIN_COUNT) return;
    for (let i = 0; i < BIN_COUNT; i++) {
        const v = +data[i] || 0;
        // Asymmetric smoothing — fast up, slow down
        if (v > smoothed[i])
            smoothed[i] += (v - smoothed[i]) * SMOOTH_UP;
        else
            smoothed[i] += (v - smoothed[i]) * SMOOTH_DN;

        if (smoothed[i] > peakHold[i])
            peakHold[i] = smoothed[i];
        else
            peakHold[i] = Math.max(0, peakHold[i] - PEAK_DECAY);
    }
});

document.addEventListener('peak-data', (e) => {
    const d = e.detail;
    if (!d) return;
    inL  = +d.inL  || 0;
    inR  = +d.inR  || 0;
    outL = +d.outL || 0;
    outR = +d.outR || 0;
});

// ─── Animation loop — renders at 60fps ──────────────────────────────────────
function tick() {
    requestAnimationFrame(tick);

    // Smooth meters
    const tgtIn  = Math.max(inL, inR);
    const tgtOut = Math.max(outL, outR);
    inLSmooth  += (tgtIn  - inLSmooth)  * (tgtIn  > inLSmooth  ? 0.5 : 0.08);
    outLSmooth += (tgtOut - outLSmooth) * (tgtOut > outLSmooth ? 0.5 : 0.08);
    if (inLSmooth  > inRSmooth)  inRSmooth  = inLSmooth;
    else inRSmooth  = Math.max(0, inRSmooth  - 0.003);
    if (outLSmooth > outRSmooth) outRSmooth = outLSmooth;
    else outRSmooth = Math.max(0, outRSmooth - 0.003);

    drawSpectrum();
    drawMeter(meterInCanvas,  inLSmooth,  inRSmooth,  'IN');
    drawMeter(meterOutCanvas, outLSmooth, outRSmooth, 'OUT');
}
tick();

// Mode toggle button (bar/line) — no-op now, we only do line style
const toggleBtn = document.getElementById('specToggle');
if (toggleBtn) toggleBtn.style.display = 'none';
})();
