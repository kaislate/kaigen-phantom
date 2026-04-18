/**
 * spectrum.js — dual-layer spectrum analyzer
 *
 * Input signal:  dark gray fill (shows what you're feeding in)
 * Output signal: bright white curve (shows generated harmonics on top)
 *
 * Anywhere white rises above gray = phantom harmonics added.
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
const BIN_FREQ_LOW  = 30;
const BIN_FREQ_HIGH = 16000;
const DISPLAY_FREQ_LOW  = 20;
const DISPLAY_FREQ_HIGH = 20000;

const smoothedIn  = new Float32Array(BIN_COUNT);  // input (gray)
const smoothedOut = new Float32Array(BIN_COUNT);  // output (white)
const peakOut     = new Float32Array(BIN_COUNT);  // peak hold for output only

let inL = 0, inR = 0, outL = 0, outR = 0;
let inLSmooth = 0, inRSmooth = 0, outLSmooth = 0, outRSmooth = 0;

const SMOOTH_UP  = 0.5;
const SMOOTH_DN  = 0.08;
const PEAK_DECAY = 0.003;

// ─── Frequency → X (log scale) ───────────────────────────────────────────────
function freqToX(freq, w) {
    const logLo = Math.log10(DISPLAY_FREQ_LOW);
    const logHi = Math.log10(DISPLAY_FREQ_HIGH);
    return ((Math.log10(Math.max(freq, DISPLAY_FREQ_LOW)) - logLo) / (logHi - logLo)) * w;
}

function binToFreq(bin) {
    const logLo = Math.log10(BIN_FREQ_LOW);
    const logHi = Math.log10(BIN_FREQ_HIGH);
    return Math.pow(10, logLo + (logHi - logLo) * (bin + 0.5) / BIN_COUNT);
}

// ─── Grid + labels ──────────────────────────────────────────────────────────
const FREQ_LABELS = [
    { hz: 30,    label: '30'  },
    { hz: 100,   label: '100' },
    { hz: 300,   label: '300' },
    { hz: 1000,  label: '1k'  },
    { hz: 3000,  label: '3k'  },
    { hz: 10000, label: '10k' },
];
const DB_LINES = [-12, -24, -36, -48];

function drawGrid(ctx, w, h) {
    ctx.save();
    ctx.lineWidth = 1;

    ctx.strokeStyle = 'rgba(255,255,255,0.04)';
    for (const db of DB_LINES) {
        const y = Math.round(h * (-db) / 60) + 0.5;
        ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke();
    }

    const majors = [50, 100, 200, 500, 1000, 2000, 5000, 10000];
    ctx.strokeStyle = 'rgba(255,255,255,0.035)';
    for (const f of majors) {
        const x = Math.round(freqToX(f, w)) + 0.5;
        ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, h); ctx.stroke();
    }

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

    ctx.textAlign = 'right';
    ctx.textBaseline = 'middle';
    for (const db of [0, -12, -24, -36, -48]) {
        const y = h * (-db) / 60;
        ctx.fillText(db + 'dB', w - 3, y);
    }

    ctx.restore();
}

// ─── Build a smooth curve path from bin data ─────────────────────────────────
function buildCurvePoints(data, w, h) {
    const pts = [];
    for (let i = 0; i < BIN_COUNT; i++) {
        const f = binToFreq(i);
        if (f < DISPLAY_FREQ_LOW || f > DISPLAY_FREQ_HIGH) continue;
        pts.push({ x: freqToX(f, w), y: h * (1 - data[i]) });
    }
    return pts;
}

function strokeCurve(ctx, pts) {
    if (pts.length < 2) return;
    ctx.beginPath();
    ctx.moveTo(pts[0].x, pts[0].y);
    for (let i = 1; i < pts.length - 1; i++) {
        const xc = (pts[i].x + pts[i + 1].x) / 2;
        const yc = (pts[i].y + pts[i + 1].y) / 2;
        ctx.quadraticCurveTo(pts[i].x, pts[i].y, xc, yc);
    }
    ctx.lineTo(pts[pts.length - 1].x, pts[pts.length - 1].y);
}

function fillCurve(ctx, pts, h) {
    if (pts.length < 2) return;
    ctx.beginPath();
    ctx.moveTo(pts[0].x, h);
    for (const p of pts) ctx.lineTo(p.x, p.y);
    ctx.lineTo(pts[pts.length - 1].x, h);
    ctx.closePath();
}

// ─── Main spectrum draw ─────────────────────────────────────────────────────
function drawSpectrum() {
    if (!specCanvas) return;
    const ctx = specCanvas.getContext('2d');
    if (!ctx) return;

    const w = specCanvas.width;
    const h = specCanvas.height;

    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, w, h);

    drawGrid(ctx, w, h);

    const inPts  = buildCurvePoints(smoothedIn,  w, h);
    const outPts = buildCurvePoints(smoothedOut, w, h);

    // ── Layer 1: input signal — warm amber fill + subtle line ─────────
    if (inPts.length >= 2) {
        fillCurve(ctx, inPts, h);
        const fillGrad = ctx.createLinearGradient(0, 0, 0, h);
        fillGrad.addColorStop(0, 'rgba(220,170,80,0.20)');
        fillGrad.addColorStop(1, 'rgba(180,130,60,0.04)');
        ctx.fillStyle = fillGrad;
        ctx.fill();

        strokeCurve(ctx, inPts);
        ctx.strokeStyle = 'rgba(230,180,90,0.50)';
        ctx.lineWidth = Math.max(1, h * 0.004);
        ctx.stroke();
    }

    // ── Layer 2: output signal — bright white fill + glowing line ─────
    if (outPts.length >= 2) {
        fillCurve(ctx, outPts, h);
        const outFill = ctx.createLinearGradient(0, 0, 0, h);
        outFill.addColorStop(0, 'rgba(255,255,255,0.28)');
        outFill.addColorStop(0.5, 'rgba(255,255,255,0.10)');
        outFill.addColorStop(1, 'rgba(255,255,255,0.02)');
        ctx.fillStyle = outFill;
        ctx.fill();

        strokeCurve(ctx, outPts);
        ctx.strokeStyle = 'rgba(255,255,255,0.90)';
        ctx.lineWidth = Math.max(1.2, h * 0.006);
        ctx.shadowColor = 'rgba(255,255,255,0.6)';
        ctx.shadowBlur  = 4;
        ctx.stroke();
        ctx.shadowBlur = 0;
    }

    // ── Output peak hold line ─────────────────────────────────────────
    const peakPts = buildCurvePoints(peakOut, w, h);
    if (peakPts.length >= 2) {
        strokeCurve(ctx, peakPts);
        ctx.strokeStyle = 'rgba(255,255,255,0.25)';
        ctx.lineWidth = 1;
        ctx.stroke();
    }

    // ── Crossover frequency line ───────────────────────────────────────
    const xoverState = window.Juce?.getSliderState?.('phantom_threshold');
    if (xoverState) {
        const xoverHz = xoverState.getScaledValue();
        if (xoverHz > 20 && xoverHz < 20000) {
            const xPos = Math.round(freqToX(xoverHz, w));
            const labelFontPx = Math.max(8, Math.round(h * 0.10));
            ctx.save();
            ctx.strokeStyle = 'rgba(80,142,215,0.38)';
            ctx.lineWidth   = 1;
            ctx.setLineDash([3, 4]);
            ctx.beginPath();
            ctx.moveTo(xPos + 0.5, 0);
            ctx.lineTo(xPos + 0.5, h);
            ctx.stroke();
            ctx.setLineDash([]);
            ctx.fillStyle    = 'rgba(80,142,215,0.60)';
            ctx.font         = labelFontPx + 'px monospace';
            ctx.textAlign    = 'left';
            ctx.textBaseline = 'top';
            const labelText = Math.round(xoverHz) + 'Hz';
            ctx.fillText(labelText, xPos + 3, 2);
            ctx.restore();
        }
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

    if (peak > 0) {
        const py = h - peak * h * 0.9;
        ctx.fillStyle = 'rgba(255,255,255,0.7)';
        ctx.fillRect(1, py, w - 2, 1);
    }

    ctx.fillStyle = 'rgba(255,255,255,0.25)';
    ctx.font = Math.max(8, Math.round(h * 0.04)) + 'px monospace';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'bottom';
    ctx.fillText(label, w / 2, h - 2);
}

// ─── Data ingest ─────────────────────────────────────────────────────────────
document.addEventListener('spectrum-data', (e) => {
    const data = e.detail;
    if (!data) return;

    // Accept both old flat-array format and new {input, output} object format
    const inp = Array.isArray(data) ? data : data.input;
    const out = Array.isArray(data) ? null  : data.output;

    if (inp && inp.length >= BIN_COUNT) {
        for (let i = 0; i < BIN_COUNT; i++) {
            const v = +inp[i] || 0;
            smoothedIn[i] += (v - smoothedIn[i]) * (v > smoothedIn[i] ? SMOOTH_UP : SMOOTH_DN);
        }
    }

    if (out && out.length >= BIN_COUNT) {
        for (let i = 0; i < BIN_COUNT; i++) {
            const v = +out[i] || 0;
            smoothedOut[i] += (v - smoothedOut[i]) * (v > smoothedOut[i] ? SMOOTH_UP : SMOOTH_DN);
            if (smoothedOut[i] > peakOut[i]) peakOut[i] = smoothedOut[i];
            else peakOut[i] = Math.max(0, peakOut[i] - PEAK_DECAY);
        }
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

// ─── Animation loop ──────────────────────────────────────────────────────────
function tick() {
    requestAnimationFrame(tick);

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

// Toggle button visibility handled by oscilloscope.js
})();
