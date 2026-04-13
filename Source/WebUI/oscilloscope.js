/**
 * oscilloscope.js — 3-layer time-domain waveform display
 *
 * Layers (back → front):
 *   1. input  — raw dry input signal (dim gray)
 *   2. synth  — phantom harmonics only, envelope-scaled (amber-gold)
 *   3. output — final mixed output (bright white)
 *
 * Display is positive-slope zero-crossing triggered on the input
 * waveform so the view stays stable as pitch changes.
 *
 * Polling runs at ~20 fps (every 3rd rAF frame) to keep bridge traffic
 * reasonable given the 2048-sample × 3 channel payload.
 */

(function () {
if (!window.Juce) return;

const OSC_BUF_SIZE       = 2048;
const OSC_DISPLAY_SAMPLES = 1024; // ~23 ms at 44.1 kHz, ≈ 2 cycles at 100 Hz

const canvas = document.getElementById('oscCanvas');
if (!canvas) return;

// ── Resize to device pixel ratio ───────────────────────────────────────────
function resize() {
    const pr = window.devicePixelRatio || 1;
    const r  = canvas.getBoundingClientRect();
    if (r.width > 0 && r.height > 0) {
        canvas.width  = Math.floor(r.width  * pr);
        canvas.height = Math.floor(r.height * pr);
    }
}
resize();
window.addEventListener('resize', resize);

// ── State ───────────────────────────────────────────────────────────────────
const inBuf  = new Float32Array(OSC_BUF_SIZE);
const synBuf = new Float32Array(OSC_BUF_SIZE);
const outBuf = new Float32Array(OSC_BUF_SIZE);
let inWrPos = 0, synWrPos = 0, outWrPos = 0;
let hasData = false;

// ── Ring buffer → chronological linear array ────────────────────────────────
// wrPos points to the slot that will be written next, so wrPos is the oldest.
function linearize(src, wrPos) {
    const dst = new Float32Array(OSC_BUF_SIZE);
    for (let i = 0; i < OSC_BUF_SIZE; i++)
        dst[i] = src[(wrPos + i) & (OSC_BUF_SIZE - 1)];
    return dst;
}

// ── Trigger: first positive-slope zero crossing ─────────────────────────────
// Search within the older half of the buffer so the display window fits after it.
function findTrigger(data) {
    const searchEnd = OSC_BUF_SIZE - OSC_DISPLAY_SAMPLES - 8;
    for (let i = 8; i < searchEnd; i++) {
        if (data[i - 1] <= 0.0 && data[i] > 0.0) return i;
    }
    return 8; // free-run fallback — no crossing found
}

// ── Draw a single waveform layer ────────────────────────────────────────────
function drawWave(ctx, data, trigStart, w, h, color, lineWidth, glow) {
    const mid    = h * 0.5;
    const scaleY = h * 0.38;

    ctx.save();
    ctx.beginPath();
    for (let px = 0; px < w; px++) {
        const idx = trigStart + Math.round(px * OSC_DISPLAY_SAMPLES / w);
        const clamped = Math.min(idx, OSC_BUF_SIZE - 1);
        const y = mid - data[clamped] * scaleY;
        if (px === 0) ctx.moveTo(px + 0.5, y);
        else          ctx.lineTo(px + 0.5, y);
    }

    ctx.strokeStyle = color;
    ctx.lineWidth   = lineWidth;
    if (glow) {
        ctx.shadowColor = color;
        ctx.shadowBlur  = 5;
    }
    ctx.stroke();
    ctx.shadowBlur = 0;
    ctx.restore();
}

// ── Main draw ────────────────────────────────────────────────────────────────
function draw() {
    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    const w = canvas.width;
    const h = canvas.height;
    if (w === 0 || h === 0) return;

    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, w, h);

    // Zero line
    ctx.strokeStyle = 'rgba(255,255,255,0.06)';
    ctx.lineWidth = 1;
    ctx.beginPath(); ctx.moveTo(0, h * 0.5); ctx.lineTo(w, h * 0.5); ctx.stroke();

    // Vertical time-grid lines
    ctx.strokeStyle = 'rgba(255,255,255,0.03)';
    for (let i = 1; i < 8; i++) {
        const x = Math.round(w * i / 8) + 0.5;
        ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, h); ctx.stroke();
    }

    if (!hasData) return;

    // Gate threshold lines (RESYN mode only, Gate > 0)
    const gateState = window.Juce?.getSliderState?.('synth_gate_threshold');
    const modeState = window.Juce?.getSliderState?.('mode');
    const gateThr   = gateState ? gateState.getValue()  : 0;
    const isResyn   = modeState ? modeState.getValue() >= 0.5 : false;

    if (isResyn && gateThr > 0) {
        const mid    = h * 0.5;
        const scaleY = h * 0.38;
        const lineAbove = mid - gateThr * scaleY;
        const lineBelow = mid + gateThr * scaleY;
        ctx.save();
        ctx.strokeStyle = 'rgba(255,178,38,0.50)';
        ctx.lineWidth   = 1;
        ctx.setLineDash([4, 4]);
        ctx.beginPath(); ctx.moveTo(0, lineAbove); ctx.lineTo(w, lineAbove); ctx.stroke();
        ctx.beginPath(); ctx.moveTo(0, lineBelow); ctx.lineTo(w, lineBelow); ctx.stroke();
        ctx.setLineDash([]);
        ctx.restore();
    }

    const lin = {
        in:  linearize(inBuf,  inWrPos),
        syn: linearize(synBuf, synWrPos),
        out: linearize(outBuf, outWrPos),
    };
    const trig = findTrigger(lin.in);

    // Layer order: input (back), synth (mid), output (front)
    drawWave(ctx, lin.in,  trig, w, h, 'rgba(160,160,175,0.28)', 1.0, false);
    drawWave(ctx, lin.syn, trig, w, h, 'rgba(255,178,38,0.82)',  1.5, true);
    drawWave(ctx, lin.out, trig, w, h, 'rgba(255,255,255,0.62)', 1.0, false);

    // Legend
    const fs = Math.max(7, Math.round(h * 0.10));
    ctx.font = fs + 'px monospace';
    ctx.textBaseline = 'top';
    const gap = fs * 3.4;
    ctx.fillStyle = 'rgba(160,160,175,0.45)';  ctx.fillText('IN',    4,               3);
    ctx.fillStyle = 'rgba(255,178,38,0.90)';   ctx.fillText('SYNTH', 4 + gap,         3);
    ctx.fillStyle = 'rgba(255,255,255,0.62)';  ctx.fillText('OUT',   4 + gap * 2.65,  3);
}

// ── Data ingest ──────────────────────────────────────────────────────────────
document.addEventListener('osc-data', (e) => {
    const d = e.detail;
    if (!d) return;

    if (d.input  && d.input.length  >= OSC_BUF_SIZE)
        for (let i = 0; i < OSC_BUF_SIZE; i++) inBuf[i]  = +d.input[i]  || 0;
    if (d.synth  && d.synth.length  >= OSC_BUF_SIZE)
        for (let i = 0; i < OSC_BUF_SIZE; i++) synBuf[i] = +d.synth[i]  || 0;
    if (d.output && d.output.length >= OSC_BUF_SIZE)
        for (let i = 0; i < OSC_BUF_SIZE; i++) outBuf[i] = +d.output[i] || 0;

    if (typeof d.inputWrPos  === 'number') inWrPos  = d.inputWrPos;
    if (typeof d.synthWrPos  === 'number') synWrPos = d.synthWrPos;
    if (typeof d.outputWrPos === 'number') outWrPos = d.outputWrPos;

    hasData = true;
});

// ── Polling + animation loop ─────────────────────────────────────────────────
const getOscData = window.Juce.getNativeFunction('getOscilloscopeData');
let frameCount = 0;

function tick() {
    requestAnimationFrame(tick);
    frameCount++;

    // Poll native function at ~20 fps (every 3rd frame)
    if (getOscData && frameCount % 3 === 0) {
        getOscData().then((d) => {
            document.dispatchEvent(new CustomEvent('osc-data', { detail: d }));
        });
    }

    // Only render when the canvas is visible
    if (!canvas.classList.contains('hidden')) draw();
}
tick();

// ── Spectrum ↔ Oscilloscope toggle ──────────────────────────────────────────
const toggleBtn = document.getElementById('specToggle');
const specCanvas = document.getElementById('spectrumCanvas');

if (toggleBtn) {
    toggleBtn.style.display = ''; // make visible (spectrum.js hid it)
    let showingOsc = false;

    toggleBtn.addEventListener('click', () => {
        showingOsc = !showingOsc;
        if (showingOsc) {
            specCanvas?.classList.add('hidden');
            canvas.classList.remove('hidden');
            resize(); // match canvas size to newly-visible element
            toggleBtn.innerHTML = '&#x25CC;'; // dotted circle ◌ = oscilloscope
        } else {
            canvas.classList.add('hidden');
            specCanvas?.classList.remove('hidden');
            toggleBtn.innerHTML = '&#x2261;'; // ≡ = spectrum
        }
    });
}

})();
