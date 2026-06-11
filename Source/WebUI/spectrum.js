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

const smoothedIn      = new Float32Array(BIN_COUNT);  // input (gray)
const smoothedOut     = new Float32Array(BIN_COUNT);  // output (white, post-crossfader)
const smoothedEngineA = new Float32Array(BIN_COUNT);  // Engine A pre-crossfader (split mode)
const smoothedEngineB = new Float32Array(BIN_COUNT);  // Engine B pre-crossfader (split mode)
const peakOut         = new Float32Array(BIN_COUNT);  // peak hold for combined output

let viewMode = 'Split';   // 'Split' | 'Combined'; synced from native data.viewMode

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

// ─── Main spectrum draw — dispatches on viewMode ────────────────────────────
function drawSpectrum() {
    if (!specCanvas) return;
    const ctx = specCanvas.getContext('2d');
    if (!ctx) return;

    const w = specCanvas.width;
    const h = specCanvas.height;

    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, w, h);

    if (viewMode === 'Combined') {
        drawSpectrumCombined(ctx, w, h);
    } else {
        drawSpectrumSplit(ctx, w, h);
    }
}

// ─── Combined view: full-width post-crossfader spectrum ─────────────────────
function drawSpectrumCombined(ctx, w, h) {
    drawGrid(ctx, w, h);

    const inPts  = buildCurvePoints(smoothedIn,  w, h);
    const outPts = buildCurvePoints(smoothedOut, w, h);

    // ── Layer 1: input signal — gray fill + subtle line (matches oscilloscope IN) ──
    if (inPts.length >= 2) {
        fillCurve(ctx, inPts, h);
        const fillGrad = ctx.createLinearGradient(0, 0, 0, h);
        fillGrad.addColorStop(0, 'rgba(160,160,175,0.20)');
        fillGrad.addColorStop(1, 'rgba(160,160,175,0.04)');
        ctx.fillStyle = fillGrad;
        ctx.fill();

        strokeCurve(ctx, inPts);
        ctx.strokeStyle = 'rgba(160,160,175,0.50)';
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

    // ── Crossover frequency line — tracks the active engine via logical resolver ──
    const xoverState = window.Juce?.getSliderStateLogical?.('phantom_threshold');
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

// ─── Split view: side-by-side per-engine panes ─────────────────────────────
//
// When morph sits at exactly 0 or 1 with morph_bypass_idle_engine on, the
// idle engine's process() is skipped in DualEngineHost — its scratch still
// holds the un-processed input copy that was memcpy'd in pre-skip, so the
// pane below would otherwise show the input pass-through. We draw a
// translucent dim layer + "ENGINE X OFF" label over the bypassed pane so
// the user can still see the spectrum but knows the engine isn't running.
function isEngineBypassed(side) {
    const morphState   = window.Juce && window.Juce.getSliderState
                       ? window.Juce.getSliderState('morph_amount') : null;
    const bypassToggle = window.Juce && window.Juce.getToggleState
                       ? window.Juce.getToggleState('morph_bypass_idle_engine') : null;
    if (!morphState || !bypassToggle) return false;
    if (!bypassToggle.getValue()) return false;
    const m = morphState.getNormalisedValue();
    const eps = 1e-6;
    return (side === 'A') ? (m >= 1 - eps) : (m <= eps);
}

function drawBypassOverlay(ctx, xOffset, paneW, h, side) {
    ctx.save();
    ctx.fillStyle = 'rgba(0,0,0,0.45)';
    ctx.fillRect(xOffset, 0, paneW, h);

    const fontPx = Math.max(11, Math.round(h * 0.16));
    ctx.font = '600 ' + fontPx + 'px monospace';
    ctx.fillStyle = 'rgba(255,255,255,0.55)';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText('ENGINE ' + side + ' OFF', xOffset + paneW / 2, h / 2);
    ctx.restore();
}

function drawSpectrumSplit(ctx, w, h) {
    const halfW = Math.floor(w / 2);

    // Draw left half: Engine A
    ctx.save();
    ctx.beginPath();
    ctx.rect(0, 0, halfW, h);
    ctx.clip();
    drawOnePane(ctx, 0, halfW, h, 'A');
    ctx.restore();

    // Draw right half: Engine B
    ctx.save();
    ctx.beginPath();
    ctx.rect(halfW, 0, w - halfW, h);
    ctx.clip();
    drawOnePane(ctx, halfW, w - halfW, h, 'B');
    ctx.restore();

    // Bypass overlays (drawn AFTER panes, BEFORE divider/labels so
    // the spectrum shows through but the side label still reads on top).
    if (isEngineBypassed('A')) drawBypassOverlay(ctx, 0,     halfW,        h, 'A');
    if (isEngineBypassed('B')) drawBypassOverlay(ctx, halfW, w - halfW,    h, 'B');

    // Center divider
    ctx.strokeStyle = 'rgba(255,255,255,0.10)';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(halfW + 0.5, 0);
    ctx.lineTo(halfW + 0.5, h);
    ctx.stroke();

    // A / B labels (top-left / top-right)
    const labelFont = Math.max(9, Math.round(h * 0.10)) + 'px monospace';
    ctx.font = labelFont;
    ctx.textBaseline = 'top';
    ctx.fillStyle = 'rgba(74,144,226,0.70)';
    ctx.textAlign = 'left';
    ctx.fillText('A', 6, 4);
    ctx.textAlign = 'right';
    ctx.fillText('B', w - 6, 4);
}

function drawOnePane(ctx, xOffset, paneW, h, side /* 'A' | 'B' */) {
    ctx.save();
    ctx.translate(xOffset, 0);

    drawGrid(ctx, paneW, h);

    const inPts  = buildCurvePoints(smoothedIn, paneW, h);
    const eng    = (side === 'B') ? smoothedEngineB : smoothedEngineA;
    const engPts = eng ? buildCurvePoints(eng, paneW, h) : null;

    // Layer 1: input (gray, faint) — same color as combined mode
    if (inPts && inPts.length >= 2) {
        fillCurve(ctx, inPts, h);
        const fillGrad = ctx.createLinearGradient(0, 0, 0, h);
        fillGrad.addColorStop(0, 'rgba(160,160,175,0.18)');
        fillGrad.addColorStop(1, 'rgba(160,160,175,0.04)');
        ctx.fillStyle = fillGrad;
        ctx.fill();
        strokeCurve(ctx, inPts);
        ctx.strokeStyle = 'rgba(160,160,175,0.45)';
        ctx.lineWidth = Math.max(1, h * 0.004);
        ctx.stroke();
    }

    // Layer 2: this engine's output (white)
    if (engPts && engPts.length >= 2) {
        fillCurve(ctx, engPts, h);
        const grad = ctx.createLinearGradient(0, 0, 0, h);
        grad.addColorStop(0, 'rgba(255,255,255,0.22)');
        grad.addColorStop(0.5, 'rgba(255,255,255,0.08)');
        grad.addColorStop(1, 'rgba(255,255,255,0.02)');
        ctx.fillStyle = grad;
        ctx.fill();
        strokeCurve(ctx, engPts);
        ctx.strokeStyle = 'rgba(255,255,255,0.85)';
        ctx.lineWidth = Math.max(1.0, h * 0.005);
        ctx.stroke();
    }

    // Per-engine crossover line — reads from a_/b_ APVTS directly (not via
    // logical resolver) since each pane shows its own engine's threshold.
    const xoverParam = (side === 'B') ? 'b_phantom_threshold' : 'a_phantom_threshold';
    const xoverState = window.Juce?.getSliderState?.(xoverParam);
    if (xoverState) {
        const xoverHz = xoverState.getScaledValue();
        if (xoverHz > 20 && xoverHz < 20000) {
            const xPos = Math.round(freqToX(xoverHz, paneW));
            ctx.save();
            ctx.strokeStyle = 'rgba(80,142,215,0.38)';
            ctx.lineWidth   = 1;
            ctx.setLineDash([3, 4]);
            ctx.beginPath();
            ctx.moveTo(xPos + 0.5, 0);
            ctx.lineTo(xPos + 0.5, h);
            ctx.stroke();
            ctx.restore();
        }
    }

    ctx.restore();
}

// ─── Meter drawing ──────────────────────────────────────────────────────────
// Linear amplitude → dB → normalized 0..1 across the [-60, 0] dBFS window.
// -6 dB lands at ~90% of the meter's height (matches Ableton-style readouts);
// the previous linear mapping put -6 dB at ~50% which made the plugin look
// 30+ dB quieter than the host's track meter.
function drawMeter(canvas, level, peak, label) {
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    const w = canvas.width;
    const h = canvas.height;

    function toDbNormalized(lin) {
        if (lin <= 0) return 0;
        const dB = 20 * Math.log10(lin);
        return Math.max(0, Math.min(1, (dB + 60) / 60));
    }

    const fillPct = toDbNormalized(level);
    const peakPct = toDbNormalized(peak);

    // Minimal "DJ-style" white-glowing bar: clear the canvas and let the
    // surrounding panel background show through. No solid fill, no label.
    ctx.clearRect(0, 0, w, h);

    if (fillPct > 0) {
        const fillH = fillPct * h;
        const grad = ctx.createLinearGradient(0, h, 0, h - fillH);
        grad.addColorStop(0, 'rgba(245, 250, 255, 0.85)');
        grad.addColorStop(1, 'rgba(220, 235, 250, 0.95)');
        ctx.fillStyle = grad;
        ctx.fillRect(1, h - fillH, w - 2, fillH);
        // Soft cool-white glow.
        ctx.shadowColor = 'rgba(180, 210, 240, 0.7)';
        ctx.shadowBlur = 4;
        ctx.fillRect(1, h - fillH, w - 2, fillH);
        ctx.shadowBlur = 0;
    }

    if (peakPct > 0) {
        const py = h - peakPct * h;
        ctx.fillStyle = 'rgba(255, 255, 255, 0.95)';
        ctx.fillRect(1, py, w - 2, 1);
    }

    // Clipping indicator: red top edge if peak >= -0.1 dBFS (linear ~0.989).
    if (peak >= 0.989) {
        ctx.fillStyle = 'rgba(255, 80, 80, 0.9)';
        ctx.fillRect(0, 0, w, 2);
    }
}

// ─── Data ingest ─────────────────────────────────────────────────────────────
function smoothInto(target, src) {
    if (!src || src.length < BIN_COUNT) return;
    for (let i = 0; i < BIN_COUNT; i++) {
        const v = +src[i] || 0;
        target[i] += (v - target[i]) * (v > target[i] ? SMOOTH_UP : SMOOTH_DN);
    }
}

document.addEventListener('spectrum-data', (e) => {
    const data = e.detail;
    if (!data) return;

    // Accept both old flat-array format and new object format
    // (keys: input, output, engineA, engineB, viewMode)
    const inp     = Array.isArray(data) ? data : data.input;
    const out     = Array.isArray(data) ? null : data.output;
    const engA    = Array.isArray(data) ? null : data.engineA;
    const engB    = Array.isArray(data) ? null : data.engineB;

    smoothInto(smoothedIn, inp);

    if (out && out.length >= BIN_COUNT) {
        for (let i = 0; i < BIN_COUNT; i++) {
            const v = +out[i] || 0;
            smoothedOut[i] += (v - smoothedOut[i]) * (v > smoothedOut[i] ? SMOOTH_UP : SMOOTH_DN);
            if (smoothedOut[i] > peakOut[i]) peakOut[i] = smoothedOut[i];
            else peakOut[i] = Math.max(0, peakOut[i] - PEAK_DECAY);
        }
    }

    smoothInto(smoothedEngineA, engA);
    smoothInto(smoothedEngineB, engB);

    if (!Array.isArray(data) && typeof data.viewMode === 'string') {
        const next = data.viewMode;
        if (next !== viewMode) {
            viewMode = next;
            window.dispatchEvent(new CustomEvent('spectrum-data', { detail: { viewMode: next } }));
        } else {
            viewMode = next;
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
// Throttled to ~30 fps — a spectrum analyzer at 30 fps is visually
// indistinguishable from 60 fps for monitoring purposes, and halving
// canvas work significantly eases the DWM compositor when multiple
// plugin UIs are on screen at once.
let _specFrame = 0;
function tick() {
    requestAnimationFrame(tick);
    if (document.hidden) return;
    if ((_specFrame++ & 1) === 1) return;

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
