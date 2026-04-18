// Circuit-board visual for the advanced panel. Exposes window.PhantomCircuitBoard
// with start()/stop() methods. start() kicks off rAF animation; stop() pauses it.
// Task 4: static draw only. Task 5 adds pulses + LEDs.

(function() {
  let canvas = null, ctx = null;
  let width = 0, height = 0;
  let traces = [];     // [{ points: [[x,y],[x,y],...], length: number }, ...]
  let joints = [];     // [{ x, y, flashUntil: number }]
  let pulses = [];     // [{ traceIdx, t: 0..1, speed }]
  let rafId = 0;
  let lastFrameMs = 0;
  let nextSpawnMs = 0;
  let running = false;
  const MAX_PULSES = 5;

  function resize() {
    if (!canvas) return;
    const dpr = window.devicePixelRatio || 1;
    const rect = canvas.getBoundingClientRect();
    width = rect.width;
    height = rect.height;
    canvas.width = Math.round(width * dpr);
    canvas.height = Math.round(height * dpr);
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    buildTraces();
    drawStatic();
  }

  function buildTraces() {
    const defs = [
      [[2, 50], [15, 50], [15, 20], [30, 20]],
      [[30, 20], [45, 20], [45, 75], [60, 75]],
      [[60, 75], [75, 75], [75, 40], [98, 40]],
      [[2, 80], [20, 80], [20, 90], [50, 90]],
      [[50, 90], [80, 90], [80, 55], [98, 55]],
      [[10, 10], [25, 10], [25, 35], [40, 35]],
      [[55, 30], [70, 30], [70, 10], [90, 10]],
      [[8, 60], [8, 35], [18, 35]],
    ];
    traces = defs.map(points => {
      const p = points.map(([px, py]) => [px * width / 100, py * height / 100]);
      let len = 0;
      for (let i = 1; i < p.length; i++) {
        const dx = p[i][0] - p[i-1][0];
        const dy = p[i][1] - p[i-1][1];
        len += Math.hypot(dx, dy);
      }
      return { points: p, length: len };
    });
    const jset = new Map();
    for (const tr of traces) {
      for (const [x, y] of tr.points) {
        const key = `${Math.round(x)}_${Math.round(y)}`;
        if (!jset.has(key)) jset.set(key, { x, y, flashUntil: 0 });
      }
    }
    joints = [...jset.values()];
  }

  function drawStatic() {
    ctx.clearRect(0, 0, width, height);

    ctx.fillStyle = '#0A0B0C';
    ctx.fillRect(0, 0, width, height);

    ctx.fillStyle = 'rgba(255,255,255,0.08)';
    for (let x = 15; x < width; x += 30) {
      for (let y = 15; y < height; y += 30) {
        ctx.fillRect(x, y, 1, 1);
      }
    }

    ctx.strokeStyle = 'rgba(255,255,255,0.18)';
    ctx.lineWidth = 1;
    ctx.lineCap = 'round';
    ctx.lineJoin = 'round';
    for (const tr of traces) {
      ctx.beginPath();
      ctx.moveTo(tr.points[0][0], tr.points[0][1]);
      for (let i = 1; i < tr.points.length; i++) {
        ctx.lineTo(tr.points[i][0], tr.points[i][1]);
      }
      ctx.stroke();
    }

    ctx.fillStyle = 'rgba(255,255,255,0.20)';
    for (const j of joints) {
      ctx.beginPath();
      ctx.arc(j.x, j.y, 2, 0, Math.PI * 2);
      ctx.fill();
    }
  }

  function pointOnTrace(tr, t) {
    const target = tr.length * Math.max(0, Math.min(1, t));
    let travelled = 0;
    for (let i = 1; i < tr.points.length; i++) {
      const a = tr.points[i-1], b = tr.points[i];
      const seg = Math.hypot(b[0]-a[0], b[1]-a[1]);
      if (travelled + seg >= target) {
        const local = (target - travelled) / seg;
        return [a[0] + (b[0]-a[0]) * local, a[1] + (b[1]-a[1]) * local];
      }
      travelled += seg;
    }
    const last = tr.points[tr.points.length - 1];
    return [last[0], last[1]];
  }

  function spawnPulse() {
    if (pulses.length >= MAX_PULSES || traces.length === 0) return;
    const traceIdx = Math.floor(Math.random() * traces.length);
    const speed = 120 / Math.max(1, traces[traceIdx].length) / 1000;
    pulses.push({ traceIdx, t: 0, speed });
  }

  function flashNearestJoint(x, y) {
    let best = null, bestD = Infinity;
    for (const j of joints) {
      const d = Math.hypot(j.x - x, j.y - y);
      if (d < bestD) { bestD = d; best = j; }
    }
    if (best) best.flashUntil = performance.now() + 300;
  }

  function drawAnimated(nowMs) {
    drawStatic();

    for (const j of joints) {
      const remaining = j.flashUntil - nowMs;
      const alpha = remaining > 0
        ? 0.20 + 0.80 * (remaining / 300)
        : 0.20;
      ctx.fillStyle = `rgba(255,255,255,${alpha})`;
      ctx.beginPath();
      ctx.arc(j.x, j.y, 3, 0, Math.PI * 2);
      ctx.fill();
      if (remaining > 0) {
        ctx.shadowColor = 'rgba(255,255,255,0.8)';
        ctx.shadowBlur = 6;
        ctx.beginPath();
        ctx.arc(j.x, j.y, 3, 0, Math.PI * 2);
        ctx.fill();
        ctx.shadowBlur = 0;
      }
    }

    for (const p of pulses) {
      const tr = traces[p.traceIdx];
      const [x, y] = pointOnTrace(tr, p.t);
      ctx.shadowColor = 'rgba(255,255,255,0.9)';
      ctx.shadowBlur = 8;
      ctx.fillStyle = '#FFFFFF';
      ctx.beginPath();
      ctx.arc(x, y, 2.5, 0, Math.PI * 2);
      ctx.fill();
      ctx.shadowBlur = 0;
    }
  }

  function tick(nowMs) {
    if (!running) return;
    const dt = lastFrameMs ? (nowMs - lastFrameMs) : 16;
    lastFrameMs = nowMs;

    if (nowMs >= nextSpawnMs) {
      spawnPulse();
      nextSpawnMs = nowMs + 800 + Math.random() * 700;
    }

    for (let i = pulses.length - 1; i >= 0; i--) {
      const p = pulses[i];
      p.t += p.speed * dt;
      if (p.t >= 1) {
        const tr = traces[p.traceIdx];
        const end = tr.points[tr.points.length - 1];
        flashNearestJoint(end[0], end[1]);
        pulses.splice(i, 1);
      }
    }

    drawAnimated(nowMs);
    rafId = requestAnimationFrame(tick);
  }

  function onVisibilityChange() {
    if (!running) return;
    if (document.hidden) {
      if (rafId) { cancelAnimationFrame(rafId); rafId = 0; }
    } else if (!rafId) {
      lastFrameMs = 0;
      rafId = requestAnimationFrame(tick);
    }
  }

  window.PhantomCircuitBoard = {
    start(canvasEl) {
      if (running) return;
      canvas = canvasEl;
      ctx = canvas.getContext('2d');
      resize();
      window.addEventListener('resize', resize);
      document.addEventListener('visibilitychange', onVisibilityChange);
      running = true;
      pulses = [];
      lastFrameMs = 0;
      nextSpawnMs = performance.now() + 300;
      if (!document.hidden) rafId = requestAnimationFrame(tick);
    },
    stop() {
      running = false;
      if (rafId) { cancelAnimationFrame(rafId); rafId = 0; }
      window.removeEventListener('resize', resize);
      document.removeEventListener('visibilitychange', onVisibilityChange);
    }
  };
})();
