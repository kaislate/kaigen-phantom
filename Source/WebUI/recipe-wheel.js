/**
 * recipe-wheel.js — Canvas2D holographic recipe wheel
 * Pure Canvas2D implementation (no Three.js, no CDN, no modules).
 */

(function(){
const canvas = document.getElementById('wheelCanvas');
if (!canvas) return;

const ctx = canvas.getContext('2d');
if (!ctx) return;

// Match canvas pixel size to its CSS size (2x for retina)
function resize() {
    const r = canvas.getBoundingClientRect();
    const pr = window.devicePixelRatio || 1;
    canvas.width  = Math.max(1, Math.floor(r.width  * pr));
    canvas.height = Math.max(1, Math.floor(r.height * pr));
}
resize();
window.addEventListener('resize', resize);

// ─── State ───────────────────────────────────────────────────────────────
let harmonicAmps = [0.80, 0.70, 0.50, 0.35, 0.20, 0.12, 0.07];
let ringRot = [0, 0, 0, 0, 0, 0];
let ringSpeed = [0.015, -0.020, 0.010, -0.025, 0.012, -0.008];
let scanAngle = 0;
let shimmerT = 0;

// Energy flow particles — 20 per spoke = 140 total
const PARTICLES_PER_SPOKE = 20;
const particles = [];
for (let s = 0; s < 7; s++) {
    for (let i = 0; i < PARTICLES_PER_SPOKE; i++) {
        particles.push({ spoke: s, progress: Math.random() });
    }
}

// ─── Public API ──────────────────────────────────────────────────────────
window.PhantomRecipeWheel = {
    setAmplitudes(amps) {
        if (Array.isArray(amps) && amps.length >= 7) {
            for (let i = 0; i < 7; i++) harmonicAmps[i] = amps[i];
        }
    }
};

// ─── Drawing ─────────────────────────────────────────────────────────────
function draw() {
    requestAnimationFrame(draw);

    const w = canvas.width;
    const h = canvas.height;
    const cx = w / 2;
    const cy = h / 2;
    const R  = Math.min(w, h) * 0.5 - 4;  // outer radius

    ctx.clearRect(0, 0, w, h);

    // Background gradient fill (dark with slight glow)
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

    // ── Harmonic tank spokes and nodes ─────────────────────────────────
    const innerR = R * 0.22;
    const outerR = R * 0.90;
    for (let i = 0; i < 7; i++) {
        const angle = (i / 7) * Math.PI * 2 - Math.PI / 2;
        const cos = Math.cos(angle);
        const sin = Math.sin(angle);
        const amp = harmonicAmps[i];

        // Dim track (full spoke)
        ctx.strokeStyle = 'rgba(255,255,255,0.05)';
        ctx.lineWidth = 5 * (window.devicePixelRatio || 1);
        ctx.lineCap = 'round';
        ctx.beginPath();
        ctx.moveTo(cx + innerR * cos, cy + innerR * sin);
        ctx.lineTo(cx + outerR * cos, cy + outerR * sin);
        ctx.stroke();

        // Bright fill (proportional to amplitude)
        const fillEnd = innerR + (outerR - innerR) * amp;
        const gradX1 = cx + innerR * cos;
        const gradY1 = cy + innerR * sin;
        const gradX2 = cx + fillEnd * cos;
        const gradY2 = cy + fillEnd * sin;

        const spokeGrad = ctx.createLinearGradient(gradX1, gradY1, gradX2, gradY2);
        spokeGrad.addColorStop(0, `rgba(255,255,255,${0.55 * amp})`);
        spokeGrad.addColorStop(1, `rgba(255,255,255,${0.08 * amp})`);

        // Glow halo
        ctx.strokeStyle = `rgba(255,255,255,${0.25 * amp})`;
        ctx.lineWidth = 9 * (window.devicePixelRatio || 1);
        ctx.beginPath();
        ctx.moveTo(gradX1, gradY1);
        ctx.lineTo(gradX2, gradY2);
        ctx.stroke();

        // Sharp fill line
        ctx.strokeStyle = spokeGrad;
        ctx.lineWidth = 3.5 * (window.devicePixelRatio || 1);
        ctx.beginPath();
        ctx.moveTo(gradX1, gradY1);
        ctx.lineTo(gradX2, gradY2);
        ctx.stroke();

        // Cap dot at fill endpoint
        ctx.fillStyle = `rgba(255,255,255,${0.8 * amp})`;
        ctx.beginPath();
        ctx.arc(gradX2, gradY2, 3 * (window.devicePixelRatio || 1), 0, Math.PI * 2);
        ctx.fill();

        // Outer node (glow halo + bright core)
        const nx = cx + outerR * cos;
        const ny = cy + outerR * sin;
        const nodeSize = (3 + amp * 10) * (window.devicePixelRatio || 1);
        // Halo
        const haloGrad = ctx.createRadialGradient(nx, ny, 0, nx, ny, nodeSize * 3);
        haloGrad.addColorStop(0, `rgba(255,255,255,${0.4 * amp})`);
        haloGrad.addColorStop(1, 'rgba(255,255,255,0)');
        ctx.fillStyle = haloGrad;
        ctx.beginPath();
        ctx.arc(nx, ny, nodeSize * 3, 0, Math.PI * 2);
        ctx.fill();
        // Core
        ctx.fillStyle = `rgba(255,255,255,${0.5 + amp * 0.45})`;
        ctx.beginPath();
        ctx.arc(nx, ny, nodeSize * 0.5, 0, Math.PI * 2);
        ctx.fill();
    }

    // ── Energy flow particles ──────────────────────────────────────────
    for (const p of particles) {
        const spokeAngle = (p.spoke / 7) * Math.PI * 2 - Math.PI / 2;
        const cos = Math.cos(spokeAngle);
        const sin = Math.sin(spokeAngle);
        const amp = harmonicAmps[p.spoke];
        const speed = 0.004 + amp * 0.020;
        p.progress += speed;
        if (p.progress > 1) p.progress = 0;

        // Position along the spoke
        const r = innerR + (outerR - innerR) * p.progress;
        const px = cx + r * cos;
        const py = cy + r * sin;
        const alpha = amp * (1 - p.progress) * 0.85;

        ctx.fillStyle = `rgba(255,255,255,${alpha})`;
        ctx.beginPath();
        ctx.arc(px, py, 1.4 * (window.devicePixelRatio || 1), 0, Math.PI * 2);
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
    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.lineTo(sx, sy);
    ctx.stroke();

    // ── Center glow ────────────────────────────────────────────────────
    shimmerT += 0.05;
    const pulse = 0.7 + 0.3 * Math.sin(shimmerT);
    const centerGlow = ctx.createRadialGradient(cx, cy, 0, cx, cy, innerR);
    centerGlow.addColorStop(0,   `rgba(255,255,255,${0.25 * pulse})`);
    centerGlow.addColorStop(0.5, `rgba(255,255,255,${0.08 * pulse})`);
    centerGlow.addColorStop(1,   'rgba(255,255,255,0)');
    ctx.fillStyle = centerGlow;
    ctx.beginPath();
    ctx.arc(cx, cy, innerR, 0, Math.PI * 2);
    ctx.fill();
}

draw();

// Listen for harmonic amp updates from phantom.js (if it dispatches them)
document.addEventListener('harmonics-update', (e) => {
    if (e && e.detail) window.PhantomRecipeWheel.setAmplitudes(e.detail);
});
})();
