/**
 * recipe-wheel.js — interactive holographic recipe wheel
 *
 * Each spoke represents one harmonic (H2-H8).
 * Drag inward/outward on a spoke to change its amplitude (0-1).
 * Fires a 'spoke-change' CustomEvent: { detail: { index: 0-6, value: 0-1 } }
 * so phantom.js can route the value to the JUCE parameter.
 */

(function(){
const canvas = document.getElementById('wheelCanvas');
if (!canvas) return;

const ctx = canvas.getContext('2d');
if (!ctx) return;

function resize() {
    const r  = canvas.getBoundingClientRect();
    const pr = window.devicePixelRatio || 1;
    canvas.width  = Math.max(1, Math.floor(r.width  * pr));
    canvas.height = Math.max(1, Math.floor(r.height * pr));
}
resize();
window.addEventListener('resize', resize);

// ─── State ────────────────────────────────────────────────────────────────
let harmonicAmps = [0.80, 0.70, 0.50, 0.35, 0.20, 0.12, 0.07];
let ringRot   = [0, 0, 0, 0, 0, 0];
let ringSpeed = [0.015, -0.020, 0.010, -0.025, 0.012, -0.008];
let scanAngle = 0;
let shimmerT  = 0;

// Interaction state
let dragSpoke  = -1;   // which spoke is being dragged (-1 = none)
let hovSpoke   = -1;   // which spoke is being hovered (-1 = none)

// ─── Public API ──────────────────────────────────────────────────────────
window.PhantomRecipeWheel = {
    setAmplitudes(amps) {
        if (Array.isArray(amps) && amps.length >= 7)
            for (let i = 0; i < 7; i++) harmonicAmps[i] = Math.max(0, Math.min(1, amps[i]));
    }
};

// ─── Geometry helpers ─────────────────────────────────────────────────────
function getDimensions() {
    const w  = canvas.width;
    const h  = canvas.height;
    const cx = w / 2;
    const cy = h / 2;
    const R  = Math.min(w, h) * 0.5 - 4;
    return { w, h, cx, cy, R,
             innerR: R * 0.22,
             outerR: R * 0.90 };
}

function spokeAngle(i) {
    return (i / 7) * Math.PI * 2 - Math.PI / 2;
}

// Hit-test: returns spoke index (0-6) if point is near a spoke, else -1
function hitSpoke(px, py) {
    const { cx, cy, R, innerR, outerR } = getDimensions();
    for (let i = 0; i < 7; i++) {
        const a   = spokeAngle(i);
        const cos = Math.cos(a);
        const sin = Math.sin(a);
        // Project (px-cx, py-cy) onto spoke direction and perpendicular
        const dx   = px - cx;
        const dy   = py - cy;
        const proj = dx * cos + dy * sin;   // distance along spoke
        const perp = Math.abs(-dx * sin + dy * cos); // distance from spoke line
        const hitW = Math.max(12, R * 0.06);  // hit width in pixels
        if (proj >= innerR * 0.8 && proj <= outerR * 1.1 && perp < hitW)
            return i;
    }
    return -1;
}

// Convert client pointer position to canvas pixel coordinates
function clientToCanvas(clientX, clientY) {
    const rect = canvas.getBoundingClientRect();
    const pr   = window.devicePixelRatio || 1;
    return {
        x: (clientX - rect.left) * pr,
        y: (clientY - rect.top)  * pr
    };
}

// Project pointer position onto a spoke → normalised amplitude [0,1]
function pointerToAmp(px, py, spokeIdx) {
    const { cx, cy, innerR, outerR } = getDimensions();
    const a    = spokeAngle(spokeIdx);
    const proj = (px - cx) * Math.cos(a) + (py - cy) * Math.sin(a);
    return Math.max(0, Math.min(1, (proj - innerR) / (outerR - innerR)));
}

// Emit parameter change event
function emitChange(idx, val) {
    canvas.dispatchEvent(new CustomEvent('spoke-change', {
        bubbles: true,
        detail: { index: idx, value: val }
    }));
}

// ─── Pointer events ──────────────────────────────────────────────────────
canvas.style.cursor = 'default';

canvas.addEventListener('pointerdown', e => {
    const { x, y } = clientToCanvas(e.clientX, e.clientY);
    const s = hitSpoke(x, y);
    if (s < 0) return;
    dragSpoke = s;
    canvas.setPointerCapture(e.pointerId);
    const val = pointerToAmp(x, y, s);
    harmonicAmps[s] = val;
    emitChange(s, val);
    e.preventDefault();
});

canvas.addEventListener('pointermove', e => {
    const { x, y } = clientToCanvas(e.clientX, e.clientY);
    if (dragSpoke >= 0) {
        const val = pointerToAmp(x, y, dragSpoke);
        harmonicAmps[dragSpoke] = val;
        emitChange(dragSpoke, val);
    } else {
        const h = hitSpoke(x, y);
        hovSpoke = h;
        canvas.style.cursor = h >= 0 ? 'ew-resize' : 'default';
    }
});

canvas.addEventListener('pointerup', e => {
    dragSpoke = -1;
    canvas.releasePointerCapture(e.pointerId);
});

canvas.addEventListener('pointercancel', () => { dragSpoke = -1; });

// ─── Particles ────────────────────────────────────────────────────────────
const PARTICLES_PER_SPOKE = 20;
const particles = [];
for (let s = 0; s < 7; s++)
    for (let i = 0; i < PARTICLES_PER_SPOKE; i++)
        particles.push({ spoke: s, progress: Math.random() });

// ─── Draw ─────────────────────────────────────────────────────────────────
function draw() {
    requestAnimationFrame(draw);

    const { w, h, cx, cy, R, innerR, outerR } = getDimensions();

    ctx.clearRect(0, 0, w, h);

    // Background radial gradient
    const bgGrad = ctx.createRadialGradient(cx, cy, 0, cx, cy, R);
    bgGrad.addColorStop(0,   'rgba(10,10,20,0.4)');
    bgGrad.addColorStop(0.6, 'rgba(3,3,8,0.6)');
    bgGrad.addColorStop(1,   'rgba(0,0,0,0.8)');
    ctx.fillStyle = bgGrad;
    ctx.beginPath();
    ctx.arc(cx, cy, R, 0, Math.PI * 2);
    ctx.fill();

    // ── Holographic rings ──────────────────────────────────────────────
    const ringRadii  = [0.92, 0.77, 0.60, 0.40, 0.22, 0.96];
    const ringWidths = [0.8, 0.6, 0.5, 0.6, 0.8, 0.3];
    const ringAlphas = [0.18, 0.12, 0.08, 0.13, 0.22, 0.06];
    for (let i = 0; i < 6; i++) {
        const rr = R * ringRadii[i];
        ctx.strokeStyle = `rgba(255,255,255,${ringAlphas[i]})`;
        ctx.lineWidth = ringWidths[i] * (window.devicePixelRatio || 1);
        ctx.beginPath();
        ctx.arc(cx, cy, rr, ringRot[i], ringRot[i] + Math.PI * 2);
        ctx.stroke();
        ringRot[i] += ringSpeed[i] * 0.5;
    }

    // ── Spokes ─────────────────────────────────────────────────────────
    for (let i = 0; i < 7; i++) {
        const a    = spokeAngle(i);
        const cos  = Math.cos(a);
        const sin  = Math.sin(a);
        const amp  = harmonicAmps[i];
        const hot  = (i === dragSpoke || i === hovSpoke);

        // Dim track (full spoke)
        ctx.strokeStyle = hot ? 'rgba(255,255,255,0.12)' : 'rgba(255,255,255,0.05)';
        ctx.lineWidth = 5 * (window.devicePixelRatio || 1);
        ctx.lineCap = 'round';
        ctx.beginPath();
        ctx.moveTo(cx + innerR * cos, cy + innerR * sin);
        ctx.lineTo(cx + outerR * cos, cy + outerR * sin);
        ctx.stroke();

        // Filled portion proportional to amplitude
        const fillEnd = innerR + (outerR - innerR) * amp;
        const gx1 = cx + innerR * cos, gy1 = cy + innerR * sin;
        const gx2 = cx + fillEnd * cos, gy2 = cy + fillEnd * sin;

        const sg = ctx.createLinearGradient(gx1, gy1, gx2, gy2);
        sg.addColorStop(0, `rgba(255,255,255,${0.55 * amp + (hot ? 0.2 : 0)})`);
        sg.addColorStop(1, `rgba(255,255,255,${0.08 * amp + (hot ? 0.1 : 0)})`);

        // Glow halo
        ctx.strokeStyle = `rgba(255,255,255,${0.25 * amp + (hot ? 0.15 : 0)})`;
        ctx.lineWidth = (hot ? 12 : 9) * (window.devicePixelRatio || 1);
        ctx.beginPath(); ctx.moveTo(gx1, gy1); ctx.lineTo(gx2, gy2); ctx.stroke();

        // Sharp fill line
        ctx.strokeStyle = sg;
        ctx.lineWidth = (hot ? 5 : 3.5) * (window.devicePixelRatio || 1);
        ctx.beginPath(); ctx.moveTo(gx1, gy1); ctx.lineTo(gx2, gy2); ctx.stroke();

        // Cap dot at fill endpoint
        ctx.fillStyle = `rgba(255,255,255,${0.8 * amp + (hot ? 0.2 : 0)})`;
        ctx.beginPath();
        ctx.arc(gx2, gy2, (hot ? 5 : 3) * (window.devicePixelRatio || 1), 0, Math.PI * 2);
        ctx.fill();

        // Outer node
        const nx = cx + outerR * cos, ny = cy + outerR * sin;
        const nodeSize = (3 + amp * 10) * (window.devicePixelRatio || 1);
        const haloGrad = ctx.createRadialGradient(nx, ny, 0, nx, ny, nodeSize * 3);
        haloGrad.addColorStop(0, `rgba(255,255,255,${0.4 * amp})`);
        haloGrad.addColorStop(1, 'rgba(255,255,255,0)');
        ctx.fillStyle = haloGrad;
        ctx.beginPath(); ctx.arc(nx, ny, nodeSize * 3, 0, Math.PI * 2); ctx.fill();
        ctx.fillStyle = `rgba(255,255,255,${0.5 + amp * 0.45})`;
        ctx.beginPath(); ctx.arc(nx, ny, nodeSize * 0.5, 0, Math.PI * 2); ctx.fill();

        // Label on hover/drag: show percentage value near the cap dot
        if (hot && amp > 0) {
            const lx = cx + (fillEnd + 16 * (window.devicePixelRatio || 1)) * cos;
            const ly = cy + (fillEnd + 16 * (window.devicePixelRatio || 1)) * sin;
            ctx.save();
            ctx.font = `bold ${Math.round(9 * (window.devicePixelRatio || 1))}px monospace`;
            ctx.fillStyle = 'rgba(255,255,255,0.9)';
            ctx.textAlign = 'center';
            ctx.textBaseline = 'middle';
            ctx.fillText(Math.round(amp * 100) + '%', lx, ly);
            ctx.restore();
        }
    }

    // ── Particles ──────────────────────────────────────────────────────
    for (const p of particles) {
        const a    = spokeAngle(p.spoke);
        const cos  = Math.cos(a);
        const sin  = Math.sin(a);
        const amp  = harmonicAmps[p.spoke];
        const spd  = 0.004 + amp * 0.020;
        p.progress += spd;
        if (p.progress > 1) p.progress = 0;
        const r  = innerR + (outerR - innerR) * p.progress;
        const al = amp * (1 - p.progress) * 0.85;
        ctx.fillStyle = `rgba(255,255,255,${al})`;
        ctx.beginPath();
        ctx.arc(cx + r * cos, cy + r * sin, 1.4 * (window.devicePixelRatio || 1), 0, Math.PI * 2);
        ctx.fill();
    }

    // ── Scan line ──────────────────────────────────────────────────────
    scanAngle += 0.018;
    const sx = cx + Math.cos(scanAngle) * outerR * 0.95;
    const sy = cy + Math.sin(scanAngle) * outerR * 0.95;
    const scanGrad = ctx.createLinearGradient(cx, cy, sx, sy);
    scanGrad.addColorStop(0, 'rgba(255,255,255,0)');
    scanGrad.addColorStop(0.7, 'rgba(255,255,255,0.04)');
    scanGrad.addColorStop(1, 'rgba(255,255,255,0.12)');
    ctx.strokeStyle = scanGrad;
    ctx.lineWidth = 1.5 * (window.devicePixelRatio || 1);
    ctx.beginPath(); ctx.moveTo(cx, cy); ctx.lineTo(sx, sy); ctx.stroke();

    // ── Centre glow ────────────────────────────────────────────────────
    shimmerT += 0.05;
    const pulse = 0.7 + 0.3 * Math.sin(shimmerT);
    const cg = ctx.createRadialGradient(cx, cy, 0, cx, cy, innerR);
    cg.addColorStop(0, `rgba(255,255,255,${0.25 * pulse})`);
    cg.addColorStop(0.5, `rgba(255,255,255,${0.08 * pulse})`);
    cg.addColorStop(1, 'rgba(255,255,255,0)');
    ctx.fillStyle = cg;
    ctx.beginPath(); ctx.arc(cx, cy, innerR, 0, Math.PI * 2); ctx.fill();
}

draw();

// Listen for amplitude updates pushed from phantom.js
document.addEventListener('harmonics-update', e => {
    if (e && e.detail) window.PhantomRecipeWheel.setAmplitudes(e.detail);
});
})();
