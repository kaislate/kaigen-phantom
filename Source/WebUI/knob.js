// phantom-knob — neumorphic rotary knob web component
// Renders SVG volcano surface with OLED well and arc indicator

const TAU = Math.PI * 2;
const DEG = Math.PI / 180;
const ARC_START = 135;           // degrees
const ARC_SWEEP = 270;           // degrees
const ARC_END = ARC_START + ARC_SWEEP; // 405 degrees

function polarToXY(cx, cy, r, angleDeg) {
  const rad = angleDeg * DEG;
  return { x: cx + r * Math.cos(rad), y: cx + r * Math.sin(rad) };
}

function describeArc(cx, cy, r, startDeg, endDeg) {
  const s = polarToXY(cx, cy, r, startDeg);
  const e = polarToXY(cx, cy, r, endDeg);
  const sweep = endDeg - startDeg;
  const largeArc = sweep > 180 ? 1 : 0;
  return `M ${s.x} ${s.y} A ${r} ${r} 0 ${largeArc} 1 ${e.x} ${e.y}`;
}

// ── Waveform helpers (exact replica of DSP warpPhase / shapedWave) ──────────
function warpPhase(phase, duty) {
  const d = Math.min(0.95, Math.max(0.05, duty));
  if (phase < TAU * d)
    return phase / (2 * d);
  else
    return Math.PI + (phase - TAU * d) / (2 * (1 - d));
}

function shapedWave(wp, step) {
  const s = Math.sin(wp);
  if (step <= 0) return s;
  const drive = 1 + step * 19;
  const tanhD = Math.tanh(drive);
  return Math.tanh(s * drive) / tanhD;
}

function buildWaveformPoints(step, cx, cy, oledR) {
  const nPts = 64;
  const xL = cx - (oledR - 5);
  const xR = cx + (oledR - 5);
  const yMid = cy - oledR * 0.15;
  const yAmp = oledR * 0.45;
  const pts = [];
  for (let i = 0; i <= nPts; i++) {
    const t = (i / nPts) * TAU;
    const wp = warpPhase(t, 0.5);   // duty fixed: Push affects harmonic balance, not visual shape
    const y = shapedWave(wp, step);
    pts.push(`${(xL + (i / nPts) * (xR - xL)).toFixed(1)},${(yMid - y * yAmp).toFixed(1)}`);
  }
  return pts.join(' ');
}

const TEMPLATE = document.createElement('template');
TEMPLATE.innerHTML = `
<style>
:host {
  display: inline-block;
  filter: drop-shadow(-5px -5px 12px rgba(255,255,255,0.03)) drop-shadow(6px 6px 16px rgba(0,0,0,0.75));
  cursor: ns-resize;
  user-select: none;
  -webkit-user-select: none;
}
:host([size="large"]) { width: 100px; height: 100px; }
:host([size="medium"]) { width: 72px; height: 72px; }
:host([size="small"]) { width: 38px; height: 38px; }
svg { display: block; width: 100%; height: 100%; }
.label-below {
  text-align: center;
  font-family: 'Space Grotesk', sans-serif;
  font-size: 5px;
  font-weight: 500;
  letter-spacing: 2px;
  color: rgba(255,255,255,0.45);
  text-transform: uppercase;
  white-space: nowrap;
  margin-top: 2px;
  pointer-events: none;
}
</style>
<svg></svg>
<div class="label-below"></div>
`;

class PhantomKnob extends HTMLElement {
  static get observedAttributes() {
    return ['value', 'display-value', 'size', 'label', 'default-value', 'data-param'];
  }

  constructor() {
    super();
    this.attachShadow({ mode: 'open' });
    this.shadowRoot.appendChild(TEMPLATE.content.cloneNode(true));
    this._svg = this.shadowRoot.querySelector('svg');
    this._labelEl = this.shadowRoot.querySelector('.label-below');
    this._value = 0;
    this._displayValue = '';
    this._dragging = false;
    this._lastY = 0;

    // Bind handlers
    this._onPointerDown = this._onPointerDown.bind(this);
    this._onPointerMove = this._onPointerMove.bind(this);
    this._onPointerUp = this._onPointerUp.bind(this);
    this._onDblClick = this._onDblClick.bind(this);
  }

  connectedCallback() {
    // Initialize value from default-value if not explicitly set
    if (!this.hasAttribute('value')) {
      const def = parseFloat(this.getAttribute('default-value')) || 0;
      this._value = Math.max(0, Math.min(1, def));
    }

    this._labelEl.textContent = this.getAttribute('label') || '';
    this._render();

    this.addEventListener('pointerdown', this._onPointerDown);
    this.addEventListener('dblclick', this._onDblClick);
  }

  disconnectedCallback() {
    this.removeEventListener('pointerdown', this._onPointerDown);
    this.removeEventListener('dblclick', this._onDblClick);
    document.removeEventListener('pointermove', this._onPointerMove);
    document.removeEventListener('pointerup', this._onPointerUp);
  }

  attributeChangedCallback(name, _old, val) {
    switch (name) {
      case 'value':
        this._value = Math.max(0, Math.min(1, parseFloat(val) || 0));
        this._render();
        break;
      case 'display-value':
        this._displayValue = val || '';
        this._render();
        break;
      case 'label':
        if (this._labelEl) this._labelEl.textContent = val || '';
        break;
      case 'size':
        this._render();
        break;
    }
  }

  // --- Public API ---
  get value() { return this._value; }
  set value(v) {
    this._value = Math.max(0, Math.min(1, parseFloat(v) || 0));
    this._render();
  }

  get displayValue() { return this._displayValue; }
  set displayValue(str) {
    this._displayValue = str || '';
    this._render();
  }

  get name() { return this.dataset.param; }

  // --- Interaction ---
  _onPointerDown(e) {
    if (e.button !== 0) return;
    this._dragging = true;
    this._lastY = e.clientY;
    this.setPointerCapture(e.pointerId);
    document.body.style.pointerEvents = 'none';
    document.addEventListener('pointermove', this._onPointerMove);
    document.addEventListener('pointerup', this._onPointerUp);
    e.preventDefault();
  }

  _onPointerMove(e) {
    if (!this._dragging) return;
    const dy = e.clientY - this._lastY;
    this._lastY = e.clientY;
    const newVal = Math.max(0, Math.min(1, this._value + dy * -0.005));
    if (newVal !== this._value) {
      this._value = newVal;
      this._render();
      this.dispatchEvent(new CustomEvent('knob-change', {
        bubbles: true,
        detail: { name: this.dataset.param, value: this._value }
      }));
    }
  }

  _onPointerUp(e) {
    this._dragging = false;
    document.body.style.pointerEvents = '';
    document.removeEventListener('pointermove', this._onPointerMove);
    document.removeEventListener('pointerup', this._onPointerUp);
    try { this.releasePointerCapture(e.pointerId); } catch (_) {}
  }

  _onDblClick() {
    const def = parseFloat(this.getAttribute('default-value')) || 0;
    this._value = Math.max(0, Math.min(1, def));
    this._render();
    this.dispatchEvent(new CustomEvent('knob-change', {
      bubbles: true,
      detail: { name: this.dataset.param, value: this._value }
    }));
  }

  // --- Render ---
  _render() {
    const isLarge = this.getAttribute('size') !== 'medium';
    const sz = isLarge ? 100 : 72;
    const inset = isLarge ? 13 : 9;
    const cx = sz / 2;
    const cy = sz / 2;
    const volcanoR = sz / 2;
    const oledR = volcanoR - inset;
    const arcR = oledR - 4;
    const fontSize = isLarge ? 12 : 10;
    const label = this.getAttribute('label') || '';
    const displayText = this._displayValue || this._value.toFixed(2);

    const valEndDeg = ARC_START + ARC_SWEEP * this._value;

    // Build SVG
    const svg = this._svg;
    svg.setAttribute('viewBox', `0 0 ${sz} ${sz}`);
    svg.setAttribute('width', sz);
    svg.setAttribute('height', sz);

    // Full arc track path
    const trackPath = describeArc(cx, cy, arcR, ARC_START, ARC_END - 0.01);
    // Value arc path (avoid zero-length path)
    const valPath = this._value > 0.001
      ? describeArc(cx, cy, arcR, ARC_START, valEndDeg)
      : '';

    svg.innerHTML = `
      <defs>
        <radialGradient id="vg-${sz}" cx="32%" cy="28%" r="72%" fx="32%" fy="28%">
          <stop offset="0%" stop-color="rgba(255,255,255,0.13)"/>
          <stop offset="12%" stop-color="rgba(255,255,255,0.06)"/>
          <stop offset="30%" stop-color="rgba(255,255,255,0.025)"/>
          <stop offset="65%" stop-color="rgba(6,6,14,0.5)"/>
          <stop offset="100%" stop-color="rgba(0,0,0,0.6)"/>
        </radialGradient>
        <filter id="glow-${sz}" x="-20%" y="-20%" width="140%" height="140%">
          <feGaussianBlur in="SourceGraphic" stdDeviation="2"/>
        </filter>
      </defs>

      <!-- Volcano surface -->
      <circle cx="${cx}" cy="${cy}" r="${volcanoR}" fill="url(#vg-${sz})"/>

      <!-- OLED well -->
      <circle cx="${cx}" cy="${cy}" r="${oledR}" fill="#000"/>

      <!-- Lip / ridge -->
      <circle cx="${cx}" cy="${cy}" r="${oledR}" fill="none"
        stroke="rgba(255,255,255,0.16)" stroke-width="1.5"/>
      <circle cx="${cx}" cy="${cy}" r="${oledR + 1.5}" fill="none"
        stroke="rgba(0,0,0,0.7)" stroke-width="1.5"/>
      <circle cx="${cx}" cy="${cy}" r="${oledR + 3}" fill="none"
        stroke="rgba(255,255,255,0.07)" stroke-width="1"/>

      <!-- Arc track (full 270 deg) -->
      <path d="${trackPath}" fill="none"
        stroke="rgba(255,255,255,0.07)" stroke-width="3.5" stroke-linecap="round"/>

      ${valPath ? `
      <!-- Arc glow -->
      <path d="${valPath}" fill="none"
        stroke="rgba(255,255,255,0.3)" stroke-width="6" stroke-linecap="round"
        filter="url(#glow-${sz})"/>

      <!-- Arc value -->
      <path d="${valPath}" fill="none"
        stroke="#fff" stroke-width="2.8" stroke-linecap="round"/>
      ` : ''}

      <!-- Value text (triple glow stack) -->
      <text x="${cx}" y="${cy - 1}" text-anchor="middle" dominant-baseline="central"
        font-family="'Courier New', monospace" font-weight="bold" font-size="${fontSize}"
        fill="#fff" opacity="0.3">${displayText}</text>
      <text x="${cx}" y="${cy - 1}" text-anchor="middle" dominant-baseline="central"
        font-family="'Courier New', monospace" font-weight="bold" font-size="${fontSize}"
        fill="#fff" opacity="0.6">${displayText}</text>
      <text x="${cx}" y="${cy - 1}" text-anchor="middle" dominant-baseline="central"
        font-family="'Courier New', monospace" font-weight="bold" font-size="${fontSize}"
        fill="#fff" opacity="1">${displayText}</text>

      <!-- Label text -->
      <text x="${cx}" y="${cy + fontSize / 2 + 5}" text-anchor="middle" dominant-baseline="central"
        font-family="'Space Grotesk', sans-serif" font-size="5" font-weight="500"
        letter-spacing="2" fill="rgba(255,255,255,0.45)" text-transform="uppercase"
        style="text-transform:uppercase">${label.toUpperCase()}</text>
    `;
  }
}

customElements.define('phantom-knob', PhantomKnob);
