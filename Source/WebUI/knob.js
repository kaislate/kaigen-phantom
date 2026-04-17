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
    const wp = warpPhase(t, 0.5);
    const y = shapedWave(wp, step);
    pts.push(`${(xL + (i / nPts) * (xR - xL)).toFixed(1)},${(yMid - y * yAmp).toFixed(1)}`);
  }
  return pts.join(' ');
}

// ── Size tier lookup ─────────────────────────────────────────────────────────
function getSizeTier(attr) {
  if (attr === 'large')  return { sz: 114, inset: 14 };
  if (attr === 'small')  return { sz: 40,  inset: 6  };
  return                        { sz: 88,  inset: 11 };  // 'medium' and default
}

const TEMPLATE = document.createElement('template');
TEMPLATE.innerHTML = `
<style>
:host {
  display: inline-block;
  border-radius: 50%;
  /* Icy halo — soft white radial glow behind the knob, blending into bezel */
  box-shadow:
    0 0 24px 6px rgba(255,255,255,0.22),
    0 0 48px 14px rgba(255,255,255,0.10);
  cursor: ns-resize;
  user-select: none;
  -webkit-user-select: none;
}
:host([size="large"])  { width: 114px; height: 114px; }
:host([size="medium"]) { width: 88px;  height: 88px;  }
:host([size="small"])  { width: 40px;  height: 40px;  }
svg { display: block; width: 100%; height: 100%; }
.label-below { display: none; }
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

    this._onPointerDown = this._onPointerDown.bind(this);
    this._onPointerMove = this._onPointerMove.bind(this);
    this._onPointerUp = this._onPointerUp.bind(this);
    this._onDblClick = this._onDblClick.bind(this);
  }

  connectedCallback() {
    if (!this.hasAttribute('value')) {
      const def = parseFloat(this.getAttribute('default-value')) || 0;
      this._value = Math.max(0, Math.min(1, def));
    }
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
      case 'size':
        this._render();
        break;
    }
  }

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

  _render() {
    const sizeAttr = this.getAttribute('size') || 'medium';
    const { sz, inset } = getSizeTier(sizeAttr);
    const isLarge = sizeAttr === 'large';
    const cx = sz / 2;
    const cy = sz / 2;
    const volcanoR = sz / 2;
    const oledR = volcanoR - inset;
    const arcR = oledR - 4;

    // Value text: large inside OLED. Label: small at bottom of OLED in Silkscreen.
    const valueFontSize = isLarge ? 17 : 12;
    const labelFontSize = isLarge ? 7  : 6;
    const label = this.getAttribute('label') || '';
    const displayText = this._displayValue || this._value.toFixed(2);

    const isWaveform = this.getAttribute('data-oled') === 'waveform';
    const waveNumFontSize = isLarge ? 10 : 8;

    // Value positioned slightly above OLED center; label sits near the bottom edge.
    const valueY = cy - (isLarge ? 4 : 3);
    const labelY = cy + oledR - (isLarge ? 9 : 8);

    const oledContent = isWaveform
      ? `
      <polyline points="${buildWaveformPoints(this._value, cx, cy, oledR)}"
        fill="none" stroke="#fff" stroke-opacity="0.85" stroke-width="1.5" stroke-linecap="round"/>
      <text x="${cx}" y="${cy + oledR * 0.5}" text-anchor="middle" dominant-baseline="central"
        font-family="'Courier New',monospace" font-weight="700" font-size="${waveNumFontSize}"
        fill="#fff" opacity="0.3">${displayText}</text>
      <text x="${cx}" y="${cy + oledR * 0.5}" text-anchor="middle" dominant-baseline="central"
        font-family="'Courier New',monospace" font-weight="700" font-size="${waveNumFontSize}"
        fill="#fff" opacity="0.6">${displayText}</text>
      <text x="${cx}" y="${cy + oledR * 0.5}" text-anchor="middle" dominant-baseline="central"
        font-family="'Courier New',monospace" font-weight="700" font-size="${waveNumFontSize}"
        fill="#fff" opacity="1">${displayText}</text>`
      : `
      <!-- Value text (triple glow stack) -->
      <text x="${cx}" y="${valueY}" text-anchor="middle" dominant-baseline="central"
        font-family="'Courier New',monospace" font-weight="700" font-size="${valueFontSize}"
        fill="#fff" opacity="0.3">${displayText}</text>
      <text x="${cx}" y="${valueY}" text-anchor="middle" dominant-baseline="central"
        font-family="'Courier New',monospace" font-weight="700" font-size="${valueFontSize}"
        fill="#fff" opacity="0.6">${displayText}</text>
      <text x="${cx}" y="${valueY}" text-anchor="middle" dominant-baseline="central"
        font-family="'Courier New',monospace" font-weight="700" font-size="${valueFontSize}"
        fill="#fff" opacity="1">${displayText}</text>

      <!-- Label: Kalam handwritten, icy white, bottom of OLED -->
      <text x="${cx}" y="${labelY}" text-anchor="middle" dominant-baseline="central"
        font-family="'Kalam', cursive" font-size="${labelFontSize}" font-weight="400"
        fill="#FFFFFF">${label.toLowerCase()}</text>`;

    const valEndDeg = ARC_START + ARC_SWEEP * this._value;

    const svg = this._svg;
    svg.setAttribute('viewBox', `0 0 ${sz} ${sz}`);
    svg.setAttribute('width', sz);
    svg.setAttribute('height', sz);

    const trackPath = describeArc(cx, cy, arcR, ARC_START, ARC_END - 0.01);
    const valPath = this._value > 0.001
      ? describeArc(cx, cy, arcR, ARC_START, valEndDeg)
      : '';

    svg.innerHTML = `
      <defs>
        <radialGradient id="vg-${sz}" cx="50%" cy="50%" r="50%" fx="50%" fy="50%">
          <!-- Inverted volcano: bright at OLED lip, fades to exact bezel color at outer edge -->
          <stop offset="0%"   stop-color="rgba(220,222,225,1)"/>
          <stop offset="70%"  stop-color="rgba(220,222,225,1)"/>
          <stop offset="85%"  stop-color="rgba(198,200,203,1)"/>
          <stop offset="100%" stop-color="rgba(171,172,174,1)"/>
        </radialGradient>
        <filter id="glow-${sz}" x="-20%" y="-20%" width="140%" height="140%">
          <feGaussianBlur in="SourceGraphic" stdDeviation="2"/>
        </filter>
      </defs>

      <!-- Volcano surface -->
      <circle cx="${cx}" cy="${cy}" r="${volcanoR}" fill="url(#vg-${sz})"/>

      <!-- OLED well -->
      <circle cx="${cx}" cy="${cy}" r="${oledR}" fill="#000"/>

      <!-- Lip / ridge — inner bright ring, outer dark bevel, outer glow -->
      <circle cx="${cx}" cy="${cy}" r="${oledR}" fill="none"
        stroke="rgba(255,255,255,0.28)" stroke-width="1.5"/>
      <circle cx="${cx}" cy="${cy}" r="${oledR + 1.5}" fill="none"
        stroke="rgba(0,0,0,0.92)" stroke-width="1.5"/>
      <circle cx="${cx}" cy="${cy}" r="${oledR + 3}" fill="none"
        stroke="rgba(255,255,255,0.10)" stroke-width="1"/>

      <!-- Arc track (full 270 deg) -->
      <path d="${trackPath}" fill="none"
        stroke="rgba(255,255,255,0.06)" stroke-width="3.5" stroke-linecap="round"/>

      ${valPath ? `
      <!-- Arc glow -->
      <path d="${valPath}" fill="none"
        stroke="rgba(255,255,255,0.45)" stroke-width="6" stroke-linecap="round"
        filter="url(#glow-${sz})"/>

      <!-- Arc value -->
      <path d="${valPath}" fill="none"
        stroke="#fff" stroke-width="2.8" stroke-linecap="round"/>
      ` : ''}

      ${oledContent}
    `;
  }
}

customElements.define('phantom-knob', PhantomKnob);
