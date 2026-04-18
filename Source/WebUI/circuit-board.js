// Circuit-board visual for the advanced panel. Exposes window.PhantomCircuitBoard
// with start()/stop() methods. start() kicks off rAF animation; stop() pauses it.
// Task 4: static draw only. Task 5 adds pulses + LEDs.

(function() {
  let canvas = null, ctx = null;
  let width = 0, height = 0;
  let traces = [];     // [{ points: [[x,y],[x,y],...] }, ...]
  let joints = [];     // [{ x, y }]
  let rafId = 0;
  let running = false;

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
    traces = defs.map(points => ({
      points: points.map(([px, py]) => [px * width / 100, py * height / 100])
    }));
    const jset = new Map();
    for (const tr of traces) {
      for (const [x, y] of tr.points) {
        const key = `${Math.round(x)}_${Math.round(y)}`;
        if (!jset.has(key)) jset.set(key, { x, y });
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

  function tick() {
    if (!running) return;
    rafId = requestAnimationFrame(tick);
  }

  window.PhantomCircuitBoard = {
    start(canvasEl) {
      if (running) return;
      canvas = canvasEl;
      ctx = canvas.getContext('2d');
      resize();
      window.addEventListener('resize', resize);
      running = true;
      rafId = requestAnimationFrame(tick);
    },
    stop() {
      running = false;
      if (rafId) cancelAnimationFrame(rafId);
      window.removeEventListener('resize', resize);
    }
  };
})();
