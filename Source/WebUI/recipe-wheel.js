/**
 * recipe-wheel.js — Holographic Recipe Wheel
 * Kaigen Phantom WebUI — Three.js holographic harmonic display
 *
 * Renders a ghostly holographic wheel showing H2–H8 harmonic amplitudes
 * as tank spokes with energy flow particles. Designed in the v7 ethereal style
 * from mockup-v21.
 */

import * as THREE from 'three';
import { EffectComposer } from 'three/addons/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/addons/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/addons/postprocessing/UnrealBloomPass.js';
import { OutputPass } from 'three/addons/postprocessing/OutputPass.js';

// ─── Module state ────────────────────────────────────────────────────────────
let _renderer = null;
let _composer = null;
let _animId = null;
let _harmonicNodes = [];   // { glow, core, trackLine, fillLine, capDot, ba }
let _particles = null;     // { geometry, data[] }
let _rings = [];
let _scanLine = null;
let _centerGlow = null;
let _t0 = 0;

// Default amplitudes for H2–H8
let _amps = [0.85, 0.66, 0.88, 0.40, 0.50, 0.76, 0.22];

const TAU = Math.PI * 2;
const NUM_HARMONICS = 7;
const SPOKE_RADIUS = 108;
const PARTICLE_COUNT = 140; // 20 per spoke

// Ring config: [radius, halfWidth, opacity, colorHex]
const RING_DEFS = [
    [120, 0.8, 0.10, 0xffffff],
    [100, 0.6, 0.07, 0xddeeff],
    [ 78, 0.5, 0.05, 0xffeedd],
    [ 52, 0.6, 0.08, 0xeeffee],
    [ 28, 0.8, 0.13, 0xffffff],
    [126, 0.3, 0.035, 0xeeddff],
];

// Ring rotation speeds (rad/frame at 60fps, approx)
const RING_SPEEDS = [0.015, -0.02, 0.01, -0.025, 0.012, -0.008];

// ─── Helpers ─────────────────────────────────────────────────────────────────

function additiveMat(color, opacity) {
    return new THREE.MeshBasicMaterial({
        color,
        transparent: true,
        opacity,
        blending: THREE.AdditiveBlending,
        depthWrite: false,
        side: THREE.DoubleSide,
    });
}

function lineMat(opacity) {
    return new THREE.LineBasicMaterial({
        color: 0xffffff,
        transparent: true,
        opacity,
        blending: THREE.AdditiveBlending,
        depthWrite: false,
    });
}

// ─── Scene construction ───────────────────────────────────────────────────────

function buildScene(W, H) {
    const scene = new THREE.Scene();
    scene.background = new THREE.Color(0x010108);

    // Ambient light — very dim
    scene.add(new THREE.AmbientLight(0x040410, 1.4));

    // Dark base plane — fills the canvas
    scene.add(new THREE.Mesh(
        new THREE.PlaneGeometry(W, H),
        new THREE.MeshStandardMaterial({ color: 0x020210, roughness: 0.96, metalness: 0.02 })
    ));

    return scene;
}

function buildWheel(scene) {
    const holo = new THREE.Group();
    // Position at center of canvas (PlaneGeometry is centered at 0,0)
    holo.position.set(0, 0, 2);

    // ── Dark circular base plane ──────────────────────────────────────────────
    {
        const baseMesh = new THREE.Mesh(
            new THREE.CircleGeometry(138, 64),
            new THREE.MeshStandardMaterial({ color: 0x010108, roughness: 0.98, metalness: 0.02 })
        );
        baseMesh.position.set(0, 0, -1.8);
        holo.add(baseMesh);
    }

    // ── Holographic rings ────────────────────────────────────────────────────
    const rings = [];
    for (const [r, w, op, col] of RING_DEFS) {
        const ring = new THREE.Mesh(
            new THREE.RingGeometry(r - w, r + w, 128),
            additiveMat(col, op)
        );
        holo.add(ring);
        rings.push(ring);
    }

    // ── Harmonic spokes (track + fill + cap) and nodes ───────────────────────
    const harmonicNodes = [];

    for (let i = 0; i < NUM_HARMONICS; i++) {
        const angle = i * (TAU / NUM_HARMONICS) - Math.PI / 2;
        const amp = _amps[i];
        const cosA = Math.cos(angle);
        const sinA = Math.sin(angle);
        const endX = SPOKE_RADIUS * cosA;
        const endY = SPOKE_RADIUS * sinA;

        // ── Dim track line (full spoke, opacity 0.05, width 5 — approximated) ─
        const trackGeo = new THREE.BufferGeometry().setFromPoints([
            new THREE.Vector3(0, 0, 0),
            new THREE.Vector3(endX, endY, 0),
        ]);
        const trackLine = new THREE.Line(trackGeo, lineMat(0.05));
        trackLine.material.linewidth = 5; // note: only works with WebGL2 line ext
        holo.add(trackLine);

        // ── Bright fill line (proportional to amplitude) ─────────────────────
        const fillLength = amp * SPOKE_RADIUS;
        const fillGeo = new THREE.BufferGeometry().setFromPoints([
            new THREE.Vector3(0, 0, 0.1),
            new THREE.Vector3(fillLength * cosA, fillLength * sinA, 0.1),
        ]);
        const fillMat = new THREE.LineBasicMaterial({
            color: 0xffffff,
            transparent: true,
            opacity: 0.3 + amp * 0.5,
            blending: THREE.AdditiveBlending,
            depthWrite: false,
            linewidth: 3.5,
        });
        const fillLine = new THREE.Line(fillGeo, fillMat);
        holo.add(fillLine);

        // ── Cap dot at spoke endpoint ─────────────────────────────────────────
        const capDot = new THREE.Mesh(
            new THREE.CircleGeometry(2.5, 16),
            additiveMat(0xffffff, 0.4 + amp * 0.35)
        );
        capDot.position.set(fillLength * cosA, fillLength * sinA, 0.15);
        holo.add(capDot);

        // ── Dual-element harmonic node ────────────────────────────────────────
        const nodeSize = 6 + amp * 12;

        // Large soft glow halo
        const glow = new THREE.Mesh(
            new THREE.CircleGeometry(nodeSize * 1.6, 32),
            additiveMat(0xffffff, amp * 0.14)
        );
        glow.position.set(endX, endY, 0.05);
        holo.add(glow);

        // Small bright core
        const core = new THREE.Mesh(
            new THREE.CircleGeometry(nodeSize * 0.55, 32),
            additiveMat(0xffffff, 0.4 + amp * 0.45)
        );
        core.position.set(endX, endY, 0.12);
        holo.add(core);

        harmonicNodes.push({
            glow,
            core,
            trackLine,
            fillLine,
            capDot,
            angle,
            ba: amp, // base amplitude (updated by updateHarmonics)
        });
    }

    // ── Scan line ────────────────────────────────────────────────────────────
    const scanLine = new THREE.Line(
        new THREE.BufferGeometry().setFromPoints([
            new THREE.Vector3(0, 0, 0),
            new THREE.Vector3(SPOKE_RADIUS + 14, 0, 0),
        ]),
        lineMat(0.05)
    );
    holo.add(scanLine);

    // ── Center glow ───────────────────────────────────────────────────────────
    holo.add(new THREE.Mesh(
        new THREE.CircleGeometry(22, 32),
        additiveMat(0xffffff, 0.07)
    ));
    const centerGlow = new THREE.Mesh(
        new THREE.CircleGeometry(12, 32),
        additiveMat(0xffffff, 0.14)
    );
    holo.add(centerGlow);

    // ── Energy flow particles ─────────────────────────────────────────────────
    const positions = new Float32Array(PARTICLE_COUNT * 3);
    const particleData = [];

    for (let i = 0; i < PARTICLE_COUNT; i++) {
        const spokeIdx = i % NUM_HARMONICS;
        const progress = Math.random(); // 0 → 1 along spoke
        const angle = harmonicNodes[spokeIdx].angle;
        const dist = progress * SPOKE_RADIUS;

        positions[i * 3]     = dist * Math.cos(angle);
        positions[i * 3 + 1] = dist * Math.sin(angle);
        positions[i * 3 + 2] = 0.2;

        particleData.push({
            spokeIdx,
            progress,
        });
    }

    const particleGeo = new THREE.BufferGeometry();
    particleGeo.setAttribute('position', new THREE.BufferAttribute(positions, 3));

    const particleMat = new THREE.PointsMaterial({
        color: 0xffffff,
        size: 1.8,
        transparent: true,
        opacity: 0.22,
        blending: THREE.AdditiveBlending,
        depthWrite: false,
        sizeAttenuation: false,
    });

    const particlePoints = new THREE.Points(particleGeo, particleMat);
    holo.add(particlePoints);

    scene.add(holo);

    return { rings, harmonicNodes, scanLine, centerGlow, particles: { geo: particleGeo, data: particleData } };
}

// ─── Animation ────────────────────────────────────────────────────────────────

function animate(composer, rings, harmonicNodes, scanLine, centerGlow, particles, t0) {
    function frame() {
        _animId = requestAnimationFrame(frame);
        const t = (Date.now() - t0) * 0.001;

        // Rotate rings at their individual speeds
        for (let i = 0; i < rings.length; i++) {
            rings[i].rotation.z += RING_SPEEDS[i];
        }

        // Advance scan line
        scanLine.rotation.z += 0.018;

        // Pulse harmonic nodes
        for (let i = 0; i < harmonicNodes.length; i++) {
            const n = harmonicNodes[i];
            const amp = n.ba;
            const pulse = 0.93 + 0.07 * Math.sin(t * 0.5 + i * 0.7);
            n.core.material.opacity = (0.4 + amp * 0.45) * pulse;
            n.glow.material.opacity = amp * 0.14 * pulse;
        }

        // Pulse center glow
        centerGlow.material.opacity = 0.12 + 0.04 * Math.sin(t * 0.3);

        // Advance energy flow particles along spokes
        const pos = particles.geo.attributes.position;
        for (let i = 0; i < PARTICLE_COUNT; i++) {
            const pd = particles.data[i];
            const amp = harmonicNodes[pd.spokeIdx].ba;
            const speed = 0.005 + amp * 0.02;

            pd.progress += speed;
            if (pd.progress >= 1.0) {
                pd.progress = 0.0;
            }

            const angle = harmonicNodes[pd.spokeIdx].angle;
            const dist = pd.progress * SPOKE_RADIUS;
            pos.array[i * 3]     = dist * Math.cos(angle);
            pos.array[i * 3 + 1] = dist * Math.sin(angle);
            // z stays constant

            // Fade brightness: bright near center, dim at perimeter
            // We modulate opacity on the shared material based on average amp
            // (individual per-particle opacity isn't supported easily in PointsMaterial;
            // we use the shared opacity as a global "energy" level)
        }
        pos.needsUpdate = true;

        // Update particle material opacity from average amplitude
        const avgAmp = _amps.reduce((s, v) => s + v, 0) / NUM_HARMONICS;
        particles.geo.userData.mat && (particles.geo.userData.mat.opacity = 0.08 + avgAmp * 0.28);

        composer.render();
    }
    frame();
}

// ─── Public API ──────────────────────────────────────────────────────────────

/**
 * Initialize the holographic wheel scene on the given canvas element.
 * Sizes the canvas to fill its parent container.
 */
export function initWheel(canvas) {
    if (_renderer) return; // already initialized

    const parent = canvas.parentElement;
    const W = parent ? parent.clientWidth  || 280 : 280;
    const H = parent ? parent.clientHeight || 280 : 280;

    // Resize canvas
    canvas.width  = W * Math.min(devicePixelRatio, 2);
    canvas.height = H * Math.min(devicePixelRatio, 2);
    canvas.style.width  = W + 'px';
    canvas.style.height = H + 'px';

    // Renderer
    const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: true });
    renderer.setSize(W, H);
    renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
    renderer.toneMapping = THREE.ACESFilmicToneMapping;
    renderer.toneMappingExposure = 0.82;
    _renderer = renderer;

    // Camera — orthographic, centered at (0,0)
    const cam = new THREE.OrthographicCamera(-W / 2, W / 2, H / 2, -H / 2, 0.1, 2000);
    cam.position.z = 500;

    // Scene
    const scene = buildScene(W, H);

    // Wheel group
    const { rings, harmonicNodes, scanLine, centerGlow, particles } = buildWheel(scene);
    _rings = rings;
    _harmonicNodes = harmonicNodes;
    _scanLine = scanLine;
    _centerGlow = centerGlow;
    _particles = particles;

    // Store mat reference on geo for opacity updates
    const pts = scene.getObjectByProperty('type', 'Points');
    if (pts) particles.geo.userData.mat = pts.material;

    // Post-processing: bloom
    const composer = new EffectComposer(renderer);
    composer.addPass(new RenderPass(scene, cam));
    composer.addPass(new UnrealBloomPass(new THREE.Vector2(W, H), 3.5, 0.85, 0.006));
    composer.addPass(new OutputPass());
    _composer = composer;

    // Handle resize
    function onResize() {
        if (!canvas.parentElement) return;
        const nW = canvas.parentElement.clientWidth  || 280;
        const nH = canvas.parentElement.clientHeight || 280;
        renderer.setSize(nW, nH);
        composer.setSize(nW, nH);
        cam.left   = -nW / 2;
        cam.right  =  nW / 2;
        cam.top    =  nH / 2;
        cam.bottom = -nH / 2;
        cam.updateProjectionMatrix();
    }
    window.addEventListener('resize', onResize);

    _t0 = Date.now();
    animate(composer, rings, harmonicNodes, scanLine, centerGlow, particles, _t0);
}

/**
 * Update harmonic amplitudes. amps is an array of 7 floats in [0, 1].
 * Updates spoke fill lengths, node brightness, and particle speeds.
 */
export function updateHarmonics(amps) {
    if (!Array.isArray(amps) || amps.length < NUM_HARMONICS) return;

    for (let i = 0; i < NUM_HARMONICS; i++) {
        const amp = Math.max(0, Math.min(1, amps[i]));
        _amps[i] = amp;

        const n = _harmonicNodes[i];
        if (!n) continue;

        n.ba = amp;

        // Update fill line endpoint
        const fillLength = amp * SPOKE_RADIUS;
        const cosA = Math.cos(n.angle);
        const sinA = Math.sin(n.angle);

        const fillPositions = n.fillLine.geometry.attributes.position;
        fillPositions.setXYZ(1, fillLength * cosA, fillLength * sinA, 0.1);
        fillPositions.needsUpdate = true;

        // Update fill line opacity
        n.fillLine.material.opacity = 0.3 + amp * 0.5;

        // Move cap dot
        n.capDot.position.set(fillLength * cosA, fillLength * sinA, 0.15);
        n.capDot.material.opacity = 0.4 + amp * 0.35;

        // Resize node glow/core geometry
        const nodeSize = 6 + amp * 12;
        // Rebuild geometries for accurate sizing
        n.glow.geometry.dispose();
        n.glow.geometry = new THREE.CircleGeometry(nodeSize * 1.6, 32);

        n.core.geometry.dispose();
        n.core.geometry = new THREE.CircleGeometry(nodeSize * 0.55, 32);
    }
}

/**
 * Stop animation, dispose GPU resources, and clean up.
 */
export function dispose() {
    if (_animId !== null) {
        cancelAnimationFrame(_animId);
        _animId = null;
    }
    if (_renderer) {
        _renderer.dispose();
        _renderer = null;
    }
    _composer = null;
    _harmonicNodes = [];
    _rings = [];
    _scanLine = null;
    _centerGlow = null;
    _particles = null;
}

// ─── Auto-initialize ──────────────────────────────────────────────────────────

const canvas = document.getElementById('wheelCanvas');
if (canvas) {
    // Defer to next frame so layout is complete
    requestAnimationFrame(() => initWheel(canvas));

    // Listen for harmonic amplitude changes from phantom.js
    document.addEventListener('harmonics-update', (e) => {
        updateHarmonics(e.detail);
    });
}
