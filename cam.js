/* Couch Chaos — 📷 AI camera.
   One phone stands by the TV and watches the room. MediaPipe pose tracking (runs in the browser, free)
   finds up to 4 people and turns their body into controls:
     swing an arm → swing (tennis / bowling / derby), arm direction → aim,
     lean left/right → steer, hand above your head → A button, waving → shake.
   Sends to the TV: {t:'cam', ppl:[{s,x,l,u,k}]}, {t:'camSwing', s, p}, {t:'camPump', s}. */
window.CCCam = (() => {
  const MP = 'https://cdn.jsdelivr.net/npm/@mediapipe/tasks-vision@0.10.14';
  const MODEL = 'https://storage.googleapis.com/mediapipe-models/pose_landmarker/pose_landmarker_lite/float16/1/pose_landmarker_lite.task';
  // keypoints we send (MediaPipe ids): nose, shoulders, elbows, wrists, hips, knees, ankles
  const KP = [0, 11, 12, 13, 14, 15, 16, 23, 24, 25, 26, 27, 28];
  const EDGES = [[1, 2], [1, 3], [3, 5], [2, 4], [4, 6], [1, 7], [2, 8], [7, 8], [7, 9], [9, 11], [8, 10], [10, 12]];
  const COLS = ['#ff4d6d', '#4dabff', '#ffd43b', '#51cf66'];
  const clamp = (v, a = -1, b = 1) => (v < a ? a : v > b ? b : v);
  const mid = (a, b) => ({ x: (a.x + b.x) / 2, y: (a.y + b.y) / 2 });
  const dist = (a, b) => Math.hypot(a.x - b.x, a.y - b.y);
  let st = null;

  async function start(send, root, toast) {
    if (st) return;
    st = { send, toast, slots: [], facing: 'environment', run: true, lastT: 0, frames: 0, fpsT: performance.now(), fps: 0 };
    root.innerHTML = `<div class="camWrap">
      <div class="camView"><video id="camV" playsinline muted autoplay></video><canvas id="camC"></canvas></div>
      <div class="camHud"><b>📷 AI CAMERA</b><span id="camStat">Loading the AI… (first time ~10 s)</span></div>
      <div class="camTips">Stand this phone by the TV, pointing at the players (sideways works best). Step back 2–3 m so your <b>whole upper body</b> is in view. Up to 4 players — no phones needed!<br>
      <b>Swing</b> an arm to hit · <b>point</b> your arm to aim · <b>lean</b> to steer · <b>hand up</b> = button · <b>wave</b> = shake</div>
      <div class="camRemote"><button data-d="left">◀</button><button data-d="up">▲</button><button data-d="ok" class="ok">OK</button><button data-d="down">▼</button><button data-d="right">▶</button><button data-d="back" class="bk">↩</button></div>
      <div class="camBtns"><button class="chip" id="camFlip">🔄 Flip camera</button><button class="chip" id="camStop">✖ Stop camera</button></div></div>`;
    // mini TV remote (used when nobody else has a phone)
    root.querySelectorAll('.camRemote button').forEach(b => b.addEventListener('pointerdown', e => { e.preventDefault(); st && st.send({ t: 'nav', d: b.dataset.d }); navigator.vibrate?.(10); }));
    st.v = root.querySelector('#camV'); st.c = root.querySelector('#camC'); st.g = st.c.getContext('2d');
    root.querySelector('#camFlip').onclick = () => { st.facing = st.facing === 'user' ? 'environment' : 'user'; openCam(); };
    root.querySelector('#camStop').onclick = () => { stop(); window.dispatchEvent(new Event('cc-cam-stop')); };
    try { if (navigator.wakeLock) st.wake = await navigator.wakeLock.request('screen'); } catch (e) {}
    await openCam();
    try {
      const vision = await import(MP + '/vision_bundle.mjs');
      const fs = await vision.FilesetResolver.forVisionTasks(MP + '/wasm');
      const opts = (delegate) => ({ baseOptions: { modelAssetPath: MODEL, delegate }, runningMode: 'IMAGE', numPoses: 4, minPoseDetectionConfidence: 0.3, minPosePresenceConfidence: 0.3, minTrackingConfidence: 0.3 });
      try { st.lm = await vision.PoseLandmarker.createFromOptions(fs, opts('GPU')); } catch (e) { st.lm = await vision.PoseLandmarker.createFromOptions(fs, opts('CPU')); }
      if (!st.camErr) stat('Looking for players…');
    } catch (e) { console.error(e); stat('⚠️ Could not load the AI. Check the internet and try again.'); return; }
    loop();
  }
  async function openCam() {
    if (!st) return;
    if (st.stream) st.stream.getTracks().forEach(t => t.stop());
    try {
      st.stream = await navigator.mediaDevices.getUserMedia({ video: { facingMode: st.facing, width: { ideal: 640 }, height: { ideal: 480 } }, audio: false });
      st.v.srcObject = st.stream; await st.v.play();
      const mirror = st.facing === 'user'; st.v.style.transform = st.c.style.transform = mirror ? 'scaleX(-1)' : '';
    } catch (e) { st.camErr = true; stat('⚠️ Camera blocked — allow camera access for this site, then tap Stop and start again.'); }
  }
  function stat(t) { const el = document.getElementById('camStat'); if (el) el.textContent = t; }
  function stop() {
    if (!st) return; st.run = false;
    if (st.stream) st.stream.getTracks().forEach(t => t.stop());
    try { st.wake && st.wake.release(); } catch (e) {}
    try { st.lm && st.lm.close(); } catch (e) {}
    st = null;
  }
  function loop() {
    if (!st || !st.run) return;
    requestAnimationFrame(loop);
    const v = st.v, now = performance.now();
    if (!st.lm || v.readyState < 2 || now - st.lastT < 55) return; // ~18 fps; every frame is a full multi-person detection
    const dt = Math.min(0.2, (now - (st.lastT || now)) / 1000) || 0.05; st.lastT = now;
    let res; try { res = st.lm.detect(v); } catch (e) { return; }
    st.frames++; if (now - st.fpsT > 1000) { st.fps = st.frames; st.frames = 0; st.fpsT = now; }
    track(res.landmarks || [], dt, now);
    draw();
  }
  // ---- turn bodies into controls ----
  function track(list, dt, now) {
    const vis = l => l && (l.visibility == null || l.visibility > 0.35);
    let people = list.map(L => {
      if (!L[11] || !L[12] || !L[15] || !L[16]) return null;
      const sc = mid(L[11], L[12]), sw = dist(L[11], L[12]);
      // hips are often hidden (couch, table, other player): fall back to shoulder width
      const hipsOk = vis(L[23]) && vis(L[24]);
      const hc = hipsOk ? mid(L[23], L[24]) : { x: sc.x, y: sc.y + sw * 1.5 };
      const torso = Math.max(0.05, hipsOk ? dist(sc, hc) : sw * 1.5);
      return { L, sc, hc, torso, sw, cx: sc.x };
    }).filter(Boolean);
    // the detector sometimes reports one person twice: drop overlapping copies
    people = people.filter((p, i) => !people.some((q, j) => j < i && Math.abs(q.cx - p.cx) < Math.max(0.04, Math.min(p.sw, q.sw) * 0.5) && Math.abs(q.sc.y - p.sc.y) < 0.08));
    people.sort((a, b) => a.cx - b.cx);
    st.slots.forEach(s => (s.hit = false));
    const pairs = [];
    for (const p of people) for (const s of st.slots) { const d = Math.hypot(s.cx - p.cx, (s.cy || 0) - p.sc.y); if (d < 0.25) pairs.push({ p, s, d }); }
    pairs.sort((a, b) => a.d - b.d);
    for (const { p, s } of pairs) if (!p.slot && !s.hit) { p.slot = s; s.hit = true; }
    for (const p of people) { // keep the same slot for the same person (nearest by position)
      let best = p.slot || null;
      if (!best) { const used = new Set(st.slots.map(s => s.id)); const id = [0, 1, 2, 3].find(i => !used.has(i)); if (id == null) continue; best = { id, aim: 0, lean: 0, lastSwing: 0, lastPump: 0, prev: null, vEma: [0, 0] }; st.slots.push(best); }
      best.hit = true; best.seen = now; best.cx = p.cx; best.cy = p.sc.y; best.p = p;
    }
    st.slots = st.slots.filter(s => now - s.seen < 1500);
    const out = [];
    for (const s of st.slots) {
      if (!s.hit) continue;
      const { L, sc, hc, torso } = s.p, W = [L[15], L[16]];
      // wrist speeds in "torso lengths per second" so it works near or far from the camera
      const sp = W.map((w, i) => (s.prev ? dist(w, s.prev[i]) / dt / torso : 0));
      const vx = W.map((w, i) => (s.prev ? (w.x - s.prev[i].x) / dt / torso : 0));
      s.prev = W.map(w => ({ x: w.x, y: w.y }));
      sp.forEach((v, i) => (s.vEma[i] = s.vEma[i] * 0.7 + v * 0.3));
      const dom = s.vEma[1] >= s.vEma[0] ? 1 : 0, w = W[dom];
      // the camera faces the players: their right side is on the LEFT of the image → flip the sign
      const aim = clamp(-(w.x - sc.x) / (torso * 1.3)), lean = clamp(-(sc.x - hc.x) / torso * 3.2);
      s.aim = s.aim * 0.5 + aim * 0.5; s.lean = s.lean * 0.5 + (Math.abs(lean) < 0.12 ? 0 : lean) * 0.5;
      const up = (L[15].y < L[0].y - torso * 0.15) || (L[16].y < L[0].y - torso * 0.15);
      const vmax = Math.max(sp[0], sp[1]);
      // a swing starts when a wrist moves fast; we follow it for ~150 ms to catch its PEAK speed, then send
      if (!s.pend && vmax > 5.5 && now - s.lastSwing > 450) {
        const f = sp[1] >= sp[0] ? 1 : 0;
        s.pend = { t0: now, vmax, dir: clamp(-vx[f] / Math.max(1, sp[f]) * 1.4), aim: s.aim, f, y0: W[f].y }; s.lastSwing = now; s.flash = now;
      } else if (s.pend) {
        s.pend.vmax = Math.max(s.pend.vmax, vmax);
        if (now - s.pend.t0 > 150) {
          // torso (shoulders→hips) ≈ 0.5 m gives real units; racket head ≈ 1.6× the wrist; ball ≈ 1.4× the racket
          const wristMs = s.pend.vmax * 0.5, kmh = Math.round(clamp(wristMs * 1.6 * 1.4 * 3.6, 20, 240));
          const pw = clamp((kmh - 40) / 150 + 0.35, 0.3, 1), aimNow = clamp(s.pend.dir * 0.85 + s.pend.aim * 0.4);
          const spin = +clamp(-(W[s.pend.f].y - s.pend.y0) / torso * 1.3).toFixed(2); // wrist rising = topspin (image y grows downward)
          st.send({ t: 'camSwing', s: s.id, p: +pw.toFixed(2), kmh, spin, x: +aimNow.toFixed(2), lag: +((now - s.pend.t0) / 1000).toFixed(3) });
          s.lastKmh = kmh; s.kmhAt = now; s.pend = null; navigator.vibrate?.(20);
        }
      }
      if (vmax > 3.5 && now - s.lastPump > 140) { s.lastPump = now; st.send({ t: 'camPump', s: s.id }); }
      out.push({ s: s.id, x: +s.aim.toFixed(2), l: +s.lean.toFixed(2), u: up, k: KP.flatMap(i => [+L[i].x.toFixed(3), +L[i].y.toFixed(3)]) });
    }
    st.send({ t: 'cam', ppl: out });
    stat(out.length ? `Tracking ${out.length} player${out.length > 1 ? 's' : ''} · ${st.fps} fps` : 'Looking for players… step into view');
  }
  function draw() {
    const v = st.v, c = st.c, g = st.g, W = c.width = v.videoWidth || 640, H = c.height = v.videoHeight || 480, now = performance.now();
    g.clearRect(0, 0, W, H); g.lineCap = 'round';
    for (const s of st.slots) {
      if (!s.hit || !s.p) continue;
      const L = s.p.L, col = COLS[s.id], P = i => ({ x: L[KP[i]].x * W, y: L[KP[i]].y * H });
      g.strokeStyle = col; g.lineWidth = now - (s.flash || 0) < 250 ? 12 : 6;
      for (const [a, b] of EDGES) { const A = P(a), B = P(b); g.beginPath(); g.moveTo(A.x, A.y); g.lineTo(B.x, B.y); g.stroke(); }
      const n = P(0); g.fillStyle = col; g.beginPath(); g.arc(n.x, n.y, 14, 0, Math.PI * 2); g.fill();
      g.font = 'bold 28px sans-serif'; g.textAlign = 'center'; g.fillStyle = '#fff'; g.save(); g.translate(n.x, n.y - 30); if (st.facing === 'user') g.scale(-1, 1); g.fillText(`P${s.id + 1}${s.lastKmh && now - s.kmhAt < 2000 ? ` · ${s.lastKmh} km/h` : ''}`, 0, 0); g.restore();
    }
  }
  return { start, stop, active: () => !!st, send: m => st && st.send(m), state: () => st };
})();
