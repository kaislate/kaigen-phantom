// phantom-knob — neumorphic rotary knob web component
// Renders SVG volcano surface with OLED well and arc indicator

const TAU = Math.PI * 2;
const DEG = Math.PI / 180;
const SVG_NS = 'http://www.w3.org/2000/svg';
const ARC_START = 135;           // degrees
const ARC_SWEEP = 270;           // degrees
const ARC_END = ARC_START + ARC_SWEEP; // 405 degrees

function polarToXY(cx, cy, r, angleDeg) {
  const rad = angleDeg * DEG;
  return { x: cx + r * Math.cos(rad), y: cy + r * Math.sin(rad) };
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
  // Compact waveform — spans ~55% of OLED width at ~22% amplitude so the
  // numeric value below has clear breathing room.
  const xL = cx - oledR * 0.55;
  const xR = cx + oledR * 0.55;
  const yMid = cy - oledR * 0.20;
  const yAmp = oledR * 0.22;
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
  if (attr === 'large')  return { sz: 114, inset: 6 };
  if (attr === 'small')  return { sz: 56,  inset: 3 };
  return                        { sz: 88,  inset: 5 };  // 'medium' and default
}

const TEMPLATE = document.createElement('template');
TEMPLATE.innerHTML = `
<style>
:host {
  display: inline-block;
  border-radius: 50%;
  /* Volcano face — same pattern as .wheel-mount in styles.css. Transparent
     radial-gradient stops let the bezel alpha-blend through at the edge, so
     the slope base dissolves into the panel with no perceptible line. */
  background: radial-gradient(circle at 35% 30%,
    rgba(255,255,255,0.24) 0%,
    rgba(255,255,255,0.12) 22%,
    rgba(0,0,0,0.02) 60%,
    rgba(0,0,0,0.07) 100%);
  /* Default (medium 88px): two-layer TL highlight + two-layer BR shadow.
     Sharp inner layer defines direction; wider outer layer wraps around. */
  box-shadow:
    -3px -3px 12px rgba(255,255,255,0.70),
    -5px -6px 22px rgba(255,255,255,0.34),
    3px 4px 14px rgba(0,0,0,0.34),
    5px 7px 24px rgba(0,0,0,0.16);
  cursor: ns-resize;
  user-select: none;
  -webkit-user-select: none;
}
:host([size="large"]) {
  width: 114px; height: 114px;
  box-shadow:
    -4px -4px 16px rgba(255,255,255,0.70),
    -7px -8px 30px rgba(255,255,255,0.36),
    4px 5px 18px rgba(0,0,0,0.36),
    7px 9px 32px rgba(0,0,0,0.18);
}
:host([size="medium"]) { width: 88px; height: 88px; }
:host([size="small"]) {
  width: 56px; height: 56px;
  box-shadow:
    -2px -2px 9px rgba(255,255,255,0.66),
    -3px -4px 15px rgba(255,255,255,0.30),
    2px 3px 10px rgba(0,0,0,0.30),
    3px 5px 17px rgba(0,0,0,0.14);
}
svg { display: block; width: 100%; height: 100%; }
/* Label is fixed across states. Only value-text transitions between rest and drag. */
.value-text { transition: font-size 150ms ease; }

/* Rest sizes (label sizes are final — unchanged on drag) */
:host([size="small"])  .value-text { font-size: 9px; }
:host([size="small"])  .label-text { font-size: 6px; }
:host([size="medium"]) .value-text { font-size: 12px; }
:host([size="medium"]) .label-text { font-size: 9px; }
:host([size="large"])  .value-text { font-size: 14px; }
:host([size="large"])  .label-text { font-size: 10px; }
:host(:not([size="small"]):not([size="medium"]):not([size="large"])) .value-text { font-size: 12px; }
:host(:not([size="small"]):not([size="medium"]):not([size="large"])) .label-text { font-size: 9px; }

/* Drag sizes — value-text only */
:host([size="small"])  svg[data-dragging="true"] .value-text { font-size: 13px; }
:host([size="medium"]) svg[data-dragging="true"] .value-text { font-size: 18px; }
:host([size="large"])  svg[data-dragging="true"] .value-text { font-size: 22px; }
:host(:not([size="small"]):not([size="medium"]):not([size="large"])) svg[data-dragging="true"] .value-text { font-size: 18px; }
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

    this._scaffoldSize = null;
    this._scaffoldIsWaveform = null;
    this._geom = null;
    this._parts = null;

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
    this._updateDragState();
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
    this._updateDragState();
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

  _updateDragState() {
    if (this._svg) this._svg.setAttribute('data-dragging', this._dragging ? 'true' : 'false');
  }

  _ensureScaffold() {
    const sizeAttr = this.getAttribute('size') || 'medium';
    const isWaveform = this.getAttribute('data-oled') === 'waveform';
    if (this._parts &&
        this._scaffoldSize === sizeAttr &&
        this._scaffoldIsWaveform === isWaveform) return;

    this._scaffoldSize = sizeAttr;
    this._scaffoldIsWaveform = isWaveform;

    const { sz, inset } = getSizeTier(sizeAttr);
    const isLarge = sizeAttr === 'large';
    const cx = sz / 2;
    const cy = sz / 2;
    const oledR = sz / 2 - inset;
    const arcR = oledR - 4;
    this._geom = { cx, cy, oledR, arcR };

    const svg = this._svg;
    svg.setAttribute('viewBox', `0 0 ${sz} ${sz}`);
    svg.setAttribute('width', sz);
    svg.setAttribute('height', sz);
    while (svg.firstChild) svg.removeChild(svg.firstChild);

    const defs = document.createElementNS(SVG_NS, 'defs');
    defs.innerHTML =
      `<filter id="glow-${sz}" x="-20%" y="-20%" width="140%" height="140%">` +
        `<feGaussianBlur in="SourceGraphic" stdDeviation="2"/>` +
      `</filter>`;
    svg.appendChild(defs);

    const mkCircle = (r, fill, stroke, sw) => {
      const c = document.createElementNS(SVG_NS, 'circle');
      c.setAttribute('cx', cx);
      c.setAttribute('cy', cy);
      c.setAttribute('r', r);
      if (fill) c.setAttribute('fill', fill); else c.setAttribute('fill', 'none');
      if (stroke) { c.setAttribute('stroke', stroke); c.setAttribute('stroke-width', sw); }
      svg.appendChild(c);
      return c;
    };
    mkCircle(oledR, '#000');                                          // OLED well
    mkCircle(oledR,        null, 'rgba(255,255,255,0.28)', 0.75);     // inner bright
    mkCircle(oledR + 0.75, null, 'rgba(0,0,0,0.92)',       0.75);     // dark bevel
    mkCircle(oledR + 1.5,  null, 'rgba(255,255,255,0.10)', 0.5);      // outer soft

    const mkPath = (stroke, sw, filter) => {
      const p = document.createElementNS(SVG_NS, 'path');
      p.setAttribute('fill', 'none');
      p.setAttribute('stroke', stroke);
      p.setAttribute('stroke-width', sw);
      p.setAttribute('stroke-linecap', 'round');
      if (filter) p.setAttribute('filter', filter);
      svg.appendChild(p);
      return p;
    };
    const trackPath = mkPath('rgba(255,255,255,0.06)', 3.5);
    trackPath.setAttribute('d', describeArc(cx, cy, arcR, ARC_START, ARC_END - 0.01));
    const glowPath = mkPath('rgba(255,255,255,0.45)', 6, `url(#glow-${sz})`);
    const valPath  = mkPath('#fff', 2.8);
    glowPath.style.display = 'none';
    valPath.style.display  = 'none';

    const valueTexts = [];
    let waveformPoly = null;
    let labelText = null;

    const mkText = (cls, x, y, family, weight, opacity) => {
      const t = document.createElementNS(SVG_NS, 'text');
      t.setAttribute('class', cls);
      t.setAttribute('x', x);
      t.setAttribute('y', y);
      t.setAttribute('text-anchor', 'middle');
      t.setAttribute('dominant-baseline', 'central');
      t.setAttribute('font-family', family);
      t.setAttribute('font-weight', weight);
      t.setAttribute('fill', '#fff');
      if (opacity != null) t.setAttribute('opacity', opacity);
      svg.appendChild(t);
      return t;
    };

    if (isWaveform) {
      waveformPoly = document.createElementNS(SVG_NS, 'polyline');
      waveformPoly.setAttribute('fill', 'none');
      waveformPoly.setAttribute('stroke', '#fff');
      waveformPoly.setAttribute('stroke-opacity', '0.85');
      waveformPoly.setAttribute('stroke-width', '1.5');
      waveformPoly.setAttribute('stroke-linecap', 'round');
      svg.appendChild(waveformPoly);

      const vy = cy + oledR * 0.5;
      for (const op of [0.3, 0.6, 1]) {
        valueTexts.push(mkText('value-text', cx, vy, "'Courier New',monospace", 700, op));
      }
    } else {
      const valueY = cy - (isLarge ? 4 : 3);
      const labelY = cy + oledR - (isLarge ? 9 : 8);
      for (const op of [0.3, 0.6, 1]) {
        valueTexts.push(mkText('value-text', cx, valueY, "'Courier New',monospace", 700, op));
      }
      labelText = mkText('label-text', cx, labelY, "'Kalam', 'Segoe Script', cursive", 400, null);
      labelText.textContent = (this.getAttribute('label') || '').toLowerCase();
    }

    this._parts = { glowPath, valPath, valueTexts, waveformPoly, labelText };
  }

  setMorphState({ enabled, baseValue, arcDepth, liveValue, morph }) {
    this._morphState = {
      enabled: !!enabled,
      baseValue: baseValue || 0,
      arcDepth: arcDepth || 0,
      liveValue: liveValue || 0,
      morph: morph || 0,
    };
    this._renderMorphRing();
  }

  _renderMorphRing() {
    if (!this._svg || !this._morphState || !this._morphState.enabled) {
      const existing = this._svg && this._svg.querySelector('.morph-ring');
      if (existing) existing.remove();
      return;
    }

    // Get or create ring group
    let ringGroup = this._svg.querySelector('.morph-ring');
    if (!ringGroup) {
      ringGroup = document.createElementNS(SVG_NS, 'g');
      ringGroup.setAttribute('class', 'morph-ring');
      // Insert BEFORE the existing knob face so the ring sits behind/around it
      this._svg.insertBefore(ringGroup, this._svg.firstChild);
    }
    ringGroup.innerHTML = '';

    // Geometry: outer ring at ~48% of viewbox (outside the knob face at ~40%)
    const cx = this._geom ? this._geom.cx : 50;
    const cy = this._geom ? this._geom.cy : 50;
    const r  = this._geom ? (this._geom.oledR * 1.18) : 30;

    const ms = this._morphState;
    const baseAngle  = ARC_START + ms.baseValue  * ARC_SWEEP;
    const targetAngle = ARC_START + Math.max(0, Math.min(1,
      ms.baseValue + ms.arcDepth)) * ARC_SWEEP;
    const liveAngle  = ARC_START + Math.max(0, Math.min(1, ms.liveValue)) * ARC_SWEEP;

    // Track (full parameter range)
    const track = document.createElementNS(SVG_NS, 'path');
    track.setAttribute('d', describeArc(cx, cy, r, ARC_START, ARC_END));
    track.setAttribute('fill', 'none');
    track.setAttribute('stroke', 'rgba(0,0,0,0.10)');
    track.setAttribute('stroke-width', '2');
    ringGroup.appendChild(track);

    // Modulation segment (only if arc depth is non-zero)
    if (Math.abs(ms.arcDepth) > 1e-4) {
      // Bright portion: base → liveAngle (if within the segment direction)
      const segStart = Math.min(baseAngle, targetAngle);
      const segEnd   = Math.max(baseAngle, targetAngle);
      const liveInSeg = Math.max(segStart, Math.min(segEnd, liveAngle));

      // Full (dim) segment
      const dimSeg = document.createElementNS(SVG_NS, 'path');
      dimSeg.setAttribute('d', describeArc(cx, cy, r, segStart, segEnd));
      dimSeg.setAttribute('fill', 'none');
      dimSeg.setAttribute('stroke', 'rgba(74,141,213,0.35)');
      dimSeg.setAttribute('stroke-width', '3');
      dimSeg.setAttribute('stroke-linecap', 'round');
      ringGroup.appendChild(dimSeg);

      // Bright portion (from base toward live)
      const brightStart = (ms.arcDepth >= 0) ? baseAngle : liveInSeg;
      const brightEnd   = (ms.arcDepth >= 0) ? liveInSeg : baseAngle;
      if (brightEnd > brightStart) {
        const brightSeg = document.createElementNS(SVG_NS, 'path');
        brightSeg.setAttribute('d', describeArc(cx, cy, r, brightStart, brightEnd));
        brightSeg.setAttribute('fill', 'none');
        brightSeg.setAttribute('stroke', '#4A8DD5');
        brightSeg.setAttribute('stroke-width', '3');
        brightSeg.setAttribute('stroke-linecap', 'round');
        ringGroup.appendChild(brightSeg);
      }

      // Base tick
      const baseXY = polarToXY(cx, cy, r, baseAngle);
      const baseTick = document.createElementNS(SVG_NS, 'circle');
      baseTick.setAttribute('cx', baseXY.x);
      baseTick.setAttribute('cy', baseXY.y);
      baseTick.setAttribute('r', '2.2');
      baseTick.setAttribute('fill', 'rgba(0,0,0,0.55)');
      ringGroup.appendChild(baseTick);

      // Drag handle at target
      const handleXY = polarToXY(cx, cy, r, targetAngle);
      const handle = document.createElementNS(SVG_NS, 'circle');
      handle.setAttribute('cx', handleXY.x);
      handle.setAttribute('cy', handleXY.y);
      handle.setAttribute('r', '3.5');
      handle.setAttribute('fill', '#4A8DD5');
      handle.setAttribute('stroke', 'rgba(0,0,0,0.3)');
      handle.setAttribute('stroke-width', '1');
      handle.setAttribute('class', 'morph-arc-handle');
      handle.style.cursor = 'grab';
      handle.dataset.paramID = this.getAttribute('data-param') || '';
      ringGroup.appendChild(handle);
    }
  }

  _render() {
    this._ensureScaffold();
    const { cx, cy, oledR, arcR } = this._geom;
    const { glowPath, valPath, valueTexts, waveformPoly, labelText } = this._parts;

    const displayText = this._displayValue || this._value.toFixed(2);
    for (const t of valueTexts) t.textContent = displayText;

    if (this._value > 0.001) {
      const d = describeArc(cx, cy, arcR, ARC_START, ARC_START + ARC_SWEEP * this._value);
      glowPath.setAttribute('d', d);
      valPath.setAttribute('d', d);
      glowPath.style.display = '';
      valPath.style.display  = '';
    } else {
      glowPath.style.display = 'none';
      valPath.style.display  = 'none';
    }

    if (waveformPoly) {
      waveformPoly.setAttribute('points', buildWaveformPoints(this._value, cx, cy, oledR));
    }

    if (labelText) {
      const l = (this.getAttribute('label') || '').toLowerCase();
      if (labelText.textContent !== l) labelText.textContent = l;
    }
  }
}

customElements.define('phantom-knob', PhantomKnob);
