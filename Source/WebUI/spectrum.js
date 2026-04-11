/**
 * spectrum.js — Canvas2D spectrum analyzer (bar + line modes) and I/O meters
 * Listens for CustomEvents dispatched by phantom.js on `document`:
 *   'spectrum-data'  detail: Array(80) of floats 0-1
 *   'peak-data'      detail: { inL, inR, outL, outR } floats 0-1
 */

const specCanvas    = document.getElementById('spectrumCanvas');
const meterInCanvas = document.getElementById('meterIn');
const meterOutCanvas= document.getElementById('meterOut');

// ── State ────────────────────────────────────────────────────────────────────
const BIN_COUNT = 80;
const bins      = new Float32Array(BIN_COUNT);
const smoothed  = new Float32Array(BIN_COUNT);
const peakCaps  = new Float32Array(BIN_COUNT);   // per-bin peak-hold heights

let mode     = 'bar';   // 'bar' | 'line'
let inLevel  = 0;
let outLevel = 0;
let inPeak   = 0;
let outPeak  = 0;

// ── Helpers ──────────────────────────────────────────────────────────────────
function ctx2d(canvas) {
    return canvas ? canvas.getContext('2d') : null;
}

// ── Grid ─────────────────────────────────────────────────────────────────────
const FREQ_LABELS = ['30','60','125','250','500','1k','2k','4k','8k','16k'];
const DB_LABELS   = ['-48','-36','-24','-12','0'];

function drawGrid(ctx, w, h) {
    ctx.save();

    // Horizontal lines
    ctx.strokeStyle = 'rgba(255,255,255,0.02)';
    ctx.lineWidth   = 1;
    for (let i = 1; i <= 5; i++) {
        const y = Math.round(h * i / 6) + 0.5;
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(w, y);
        ctx.stroke();
    }

    ctx.font      = '5px monospace';
    ctx.fillStyle = 'rgba(255,255,255,0.08)';

    // Frequency labels (bottom)
    const step = w / FREQ_LABELS.length;
    for (let i = 0; i < FREQ_LABELS.length; i++) {
        const x = step * i + step * 0.5;
        ctx.textAlign = 'center';
        ctx.fillText(FREQ_LABELS[i], x, h - 2);
    }

    // dB labels (right edge)
    ctx.textAlign = 'right';
    for (let i = 0; i < DB_LABELS.length; i++) {
        const y = h * (1 - (i + 1) / (DB_LABELS.length + 1));
        ctx.fillText(DB_LABELS[i], w - 2, y + 2);
    }

    ctx.restore();
}

// ── Spectrum rendering ────────────────────────────────────────────────────────
function drawSpectrum() {
    if (!specCanvas) return;
    const ctx = ctx2d(specCanvas);
    if (!ctx) return;

    const w = specCanvas.width;
    const h = specCanvas.height;

    ctx.clearRect(0, 0, w, h);
    drawGrid(ctx, w, h);

    if (mode === 'bar') {
        drawBars(ctx, w, h);
    } else {
        drawLine(ctx, w, h);
    }
}

function drawBars(ctx, w, h) {
    const barW = w / BIN_COUNT;
    const gap  = 1;

    for (let i = 0; i < BIN_COUNT; i++) {
        const x   = i * barW;
        const bw  = Math.max(1, barW - gap);
        const bh  = smoothed[i] * h * 0.88;
        const y   = h - bh;

        // Bar gradient
        const grad = ctx.createLinearGradient(0, y, 0, h);
        grad.addColorStop(0, 'rgba(255,255,255,0.04)');
        grad.addColorStop(1, 'rgba(255,255,255,0.50)');

        ctx.fillStyle = grad;
        ctx.fillRect(x, y, bw, bh);

        // Peak cap
        const capY = h - peakCaps[i] * h * 0.88;
        ctx.fillStyle = 'rgba(255,255,255,0.65)';
        ctx.fillRect(x, capY, bw, 1);
    }
}

function drawLine(ctx, w, h) {
    const barW = w / BIN_COUNT;

    ctx.beginPath();
    for (let i = 0; i < BIN_COUNT; i++) {
        const x = i * barW + barW * 0.5;
        const y = h - smoothed[i] * h * 0.88;
        if (i === 0) ctx.moveTo(x, y);
        else         ctx.lineTo(x, y);
    }

    // Close path down to bottom corners for fill
    ctx.lineTo(w, h);
    ctx.lineTo(0, h);
    ctx.closePath();

    const grad = ctx.createLinearGradient(0, 0, 0, h);
    grad.addColorStop(0, 'rgba(255,255,255,0.00)');
    grad.addColorStop(1, 'rgba(255,255,255,0.20)');
    ctx.fillStyle = grad;
    ctx.fill();

    // Stroke the top line
    ctx.beginPath();
    for (let i = 0; i < BIN_COUNT; i++) {
        const x = i * barW + barW * 0.5;
        const y = h - smoothed[i] * h * 0.88;
        if (i === 0) ctx.moveTo(x, y);
        else         ctx.lineTo(x, y);
    }
    ctx.strokeStyle = 'rgba(255,255,255,0.60)';
    ctx.lineWidth   = 1.5;
    ctx.stroke();
}

// ── Meter rendering ───────────────────────────────────────────────────────────
function drawMeter(canvas, level, peak, label) {
    if (!canvas) return;
    const ctx = ctx2d(canvas);
    if (!ctx) return;

    const w = canvas.width;
    const h = canvas.height;

    ctx.clearRect(0, 0, w, h);

    const fillH = level * h * 0.85;
    const y     = h - fillH;

    // Meter bar
    if (fillH > 0) {
        const grad = ctx.createLinearGradient(0, y, 0, h);
        grad.addColorStop(0, 'rgba(255,255,255,0.05)');
        grad.addColorStop(1, 'rgba(255,255,255,0.35)');
        ctx.fillStyle = grad;
        ctx.fillRect(0, y, w, fillH);
    }

    // Peak hold line
    const peakY = h - peak * h * 0.85;
    ctx.fillStyle = 'rgba(255,255,255,0.50)';
    ctx.fillRect(0, peakY, w, 1);

    // Label
    ctx.font      = '5px monospace';
    ctx.fillStyle = 'rgba(255,255,255,0.20)';
    ctx.textAlign = 'center';
    ctx.fillText(label, w / 2, h - 2);
}

// ── Mode toggle ───────────────────────────────────────────────────────────────
document.getElementById('specToggle')?.addEventListener('click', () => {
    mode = mode === 'bar' ? 'line' : 'bar';
    drawSpectrum();
});

// ── Auto-init ────────────────────────────────────────────────────────────────
if (specCanvas) {
    function resize() {
        for (const c of [specCanvas, meterInCanvas, meterOutCanvas]) {
            if (!c) continue;
            const parent = c.parentElement || c;
            const pw = parent.clientWidth;
            const ph = parent.clientHeight;
            if (pw === 0 || ph === 0) continue;
            c.width        = pw * 2;
            c.height       = ph * 2;
            c.style.width  = pw + 'px';
            c.style.height = ph + 'px';
        }
        drawSpectrum();
        drawMeter(meterInCanvas,  inLevel,  inPeak,  'IN');
        drawMeter(meterOutCanvas, outLevel, outPeak, 'OUT');
    }

    resize();
    window.addEventListener('resize', resize);

    // spectrum-data event
    document.addEventListener('spectrum-data', (e) => {
        const data = e.detail;
        if (!data || data.length < BIN_COUNT) return;

        const SMOOTH = 0.3;
        for (let i = 0; i < BIN_COUNT; i++) {
            bins[i]     = data[i];
            smoothed[i] = smoothed[i] * (1 - SMOOTH) + bins[i] * SMOOTH;

            // Peak cap: rise instantly, fall slowly
            if (smoothed[i] >= peakCaps[i]) {
                peakCaps[i] = smoothed[i];
            } else {
                peakCaps[i] = Math.max(0, peakCaps[i] - 0.005);
            }
        }

        drawSpectrum();
    });

    // peak-data event
    document.addEventListener('peak-data', (e) => {
        const d = e.detail;
        if (!d) return;

        const SMOOTH = 0.3;

        // Combine L+R as max for each side
        const rawIn  = Math.max(d.inL  || 0, d.inR  || 0);
        const rawOut = Math.max(d.outL || 0, d.outR || 0);

        inLevel  = inLevel  * (1 - SMOOTH) + rawIn  * SMOOTH;
        outLevel = outLevel * (1 - SMOOTH) + rawOut * SMOOTH;

        // Peak hold
        if (inLevel  >= inPeak)  inPeak  = inLevel;
        else                     inPeak  = Math.max(0, inPeak  - 0.003);

        if (outLevel >= outPeak) outPeak = outLevel;
        else                     outPeak = Math.max(0, outPeak - 0.003);

        drawMeter(meterInCanvas,  inLevel,  inPeak,  'IN');
        drawMeter(meterOutCanvas, outLevel, outPeak, 'OUT');
    });
}
