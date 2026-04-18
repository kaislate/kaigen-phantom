// phantom-mini-knob — compact dark-mode rotary for the advanced panel.
// Basic-mode knobs are in knob.js (separate component).

const MK_DEG = Math.PI / 180;
const MK_ARC_START = 135;
const MK_ARC_SWEEP = 270;
const MK_ARC_END = MK_ARC_START + MK_ARC_SWEEP;

function mkPolarToXY(cx, cy, r, deg) {
  const rad = deg * MK_DEG;
  return { x: cx + r * Math.cos(rad), y: cy + r * Math.sin(rad) };
}

function mkDescribeArc(cx, cy, r, s, e) {
  const a = mkPolarToXY(cx, cy, r, s);
  const b = mkPolarToXY(cx, cy, r, e);
  const large = (e - s) > 180 ? 1 : 0;
  return `M ${a.x} ${a.y} A ${r} ${r} 0 ${large} 1 ${b.x} ${b.y}`;
}

const MK_TEMPLATE = document.createElement('template');
MK_TEMPLATE.innerHTML = `
<style>
:host {
  display: inline-flex;
  flex-direction: column;
  align-items: center;
  width: 44px;
  height: 60px;
  cursor: ns-resize;
  user-select: none;
  -webkit-user-select: none;
  position: relative;
}
svg { width: 36px; height: 36px; display: block; }
.mini-label {
  margin-top: 3px;
  font-family: 'Kalam', 'Segoe Script', cursive;
  font-size: 8px;
  font-weight: 400;
  letter-spacing: 1px;
  color: #FFFFFF;
  text-align: center;
  line-height: 1;
  text-transform: lowercase;
}
.mini-readout {
  position: absolute;
  top: -16px;
  left: 50%;
  transform: translateX(-50%);
  padding: 1px 4px;
  background: rgba(0,0,0,0.72);
  color: #FFFFFF;
  font-family: 'Courier New', monospace;
  font-size: 9px;
  font-weight: 700;
  border-radius: 2px;
  opacity: 0;
  pointer-events: none;
  transition: opacity 150ms ease;
  white-space: nowrap;
}
.mini-readout.visible { opacity: 1; transition: opacity 80ms ease; }
</style>
<svg viewBox="0 0 36 36"></svg>
<div class="mini-label"></div>
<div class="mini-readout"></div>
`;

class PhantomMiniKnob extends HTMLElement {
  static get observedAttributes() {
    return ['value', 'display-value', 'label', 'default-value', 'data-param'];
  }
  constructor() {
    super();
    this.attachShadow({ mode: 'open' });
    this.shadowRoot.appendChild(MK_TEMPLATE.content.cloneNode(true));
    this._svg = this.shadowRoot.querySelector('svg');
    this._labelEl = this.shadowRoot.querySelector('.mini-label');
    this._readoutEl = this.shadowRoot.querySelector('.mini-readout');
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
    this._labelEl.textContent = (this.getAttribute('label') || '').toLowerCase();
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
    if (name === 'value') {
      this._value = Math.max(0, Math.min(1, parseFloat(val) || 0));
      this._render();
    } else if (name === 'display-value') {
      this._displayValue = val || '';
      this._render();
    } else if (name === 'label') {
      if (this._labelEl) this._labelEl.textContent = (val || '').toLowerCase();
    }
  }
  get value() { return this._value; }
  set value(v) {
    this._value = Math.max(0, Math.min(1, parseFloat(v) || 0));
    this._render();
  }
  get displayValue() { return this._displayValue; }
  set displayValue(s) {
    this._displayValue = s || '';
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
    this._readoutEl.classList.add('visible');
    e.preventDefault();
  }
  _onPointerMove(e) {
    if (!this._dragging) return;
    const dy = e.clientY - this._lastY;
    this._lastY = e.clientY;
    const nv = Math.max(0, Math.min(1, this._value + dy * -0.005));
    if (nv !== this._value) {
      this._value = nv;
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
    this._readoutEl.classList.remove('visible');
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
    const cx = 18, cy = 18, outerR = 16, arcR = 14;
    const valEndDeg = MK_ARC_START + MK_ARC_SWEEP * this._value;
    const trackPath = mkDescribeArc(cx, cy, arcR, MK_ARC_START, MK_ARC_END - 0.01);
    const valPath = this._value > 0.001
      ? mkDescribeArc(cx, cy, arcR, MK_ARC_START, valEndDeg)
      : '';
    // Indicator tick: short radial line from the arc inward by 4px at valEndDeg.
    const tipOuter = mkPolarToXY(cx, cy, arcR - 1, valEndDeg);
    const tipInner = mkPolarToXY(cx, cy, arcR - 6, valEndDeg);
    this._svg.innerHTML = `
      <circle cx="${cx}" cy="${cy}" r="${outerR}" fill="#1A1B1C"/>
      <circle cx="${cx}" cy="${cy}" r="${outerR}" fill="none"
        stroke="rgba(255,255,255,0.15)" stroke-width="0.5"/>
      <path d="${trackPath}" fill="none"
        stroke="rgba(255,255,255,0.08)" stroke-width="2" stroke-linecap="round"/>
      ${valPath ? `<path d="${valPath}" fill="none"
        stroke="#FFFFFF" stroke-width="2.2" stroke-linecap="round"/>` : ''}
      <line x1="${tipOuter.x.toFixed(2)}" y1="${tipOuter.y.toFixed(2)}"
        x2="${tipInner.x.toFixed(2)}" y2="${tipInner.y.toFixed(2)}"
        stroke="#FFFFFF" stroke-width="1.5" stroke-linecap="round"/>
    `;
    this._readoutEl.textContent = this._displayValue || this._value.toFixed(2);
  }
}

customElements.define('phantom-mini-knob', PhantomMiniKnob);
