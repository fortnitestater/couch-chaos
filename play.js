/* Couch Chaos — controller (phone or PC) */
(() => {
  const $ = s => document.querySelector(s);
  const clamp = (v, a = -1, b = 1) => (v < a ? a : v > b ? b : v);
  const stage = $('#stage');

  let cid = store.get('cc_cid');
  if (!cid) { cid = Math.random().toString(36).slice(2, 10); store.set('cc_cid', cid); }
  const qs = new URLSearchParams(location.search);
  const FUNNY = ['Snowy Sam', 'Frosty', 'Captain Cocoa', 'Ice Queen', 'Penguin Pete', 'Blizzard', 'Mitten Max', 'Sir Sled', 'Polar Pat', 'Yeti Yo', 'Elfie', 'Jingle Jo', 'Icicle Ike', 'Snowball', 'Avalanche', 'Cozy Kai'];
  $('#roomIn').value = (qs.get('room') || '').toUpperCase().slice(0, 4);
  $('#nameIn').value = store.get('cc_name') || '';
  ($('#roomIn').value ? $('#nameIn') : $('#roomIn')).focus();
  $('#roomIn').addEventListener('input', e => { e.target.value = e.target.value.toUpperCase().replace(/[^A-Z]/g, ''); if (e.target.value.length === 4) $('#nameIn').focus(); });
  [$('#roomIn'), $('#nameIn')].forEach(i => i.addEventListener('keydown', e => { if (e.key === 'Enter') $('#joinBtn').click(); }));
  $('#diceName').onclick = () => { $('#nameIn').value = FUNNY[(Math.random() * FUNNY.length) | 0]; };

  const isIOS = /iPad|iPhone|iPod/.test(navigator.userAgent) || (navigator.platform === 'MacIntel' && navigator.maxTouchPoints > 1);
  let peer = null, conn = null, code = '', name = '', joined = false, layout = null, retryT = null, me = null, lastHand = null, vipFocus = null;

  function toast(t, ms = 2000) { const el = $('#toast'); el.textContent = t; el.classList.add('show'); clearTimeout(toast.t); toast.t = setTimeout(() => el.classList.remove('show'), ms); }
  function setStatus(t) { $('#status').textContent = t; }
  function joinErr(t) { $('#joinErr').textContent = t; $('#joinBtn').disabled = false; $('#joinBtn').textContent = 'Join the party'; }
  const light = c => /^#(ffd43b|fcc419|f1f3f5|fff|ffffff|94d82d|ffe066|e9ecef)$/i.test(c);

  // ---------- motion ----------
  let lastSwing = 0, motionSeen = false, grav = null, neutral = null, recenterSoon = false, tiltOn = store.get('cc_tilt') !== '0', flip = store.get('cc_flip') === '1' ? -1 : 1, lastShake = 0, motionOn = false;
  // iPhone/iPad (Safari) only shows the motion popup when requestPermission() is called straight from a real
  // button tap ('click'). Anything else (auto-join, pointerdown, timers) is silently refused.
  const needPerm = typeof DeviceMotionEvent !== 'undefined' && typeof DeviceMotionEvent.requestPermission === 'function';
  let permState = needPerm ? 'unknown' : 'granted';
  function listenMotion() { if (!motionOn) { motionOn = true; addEventListener('devicemotion', onMotion); addEventListener('deviceorientation', onOrient); } }
  // ---------- 1:1 orientation (the racket / bat / arm on the TV copies the phone exactly) ----------
  // quaternion maths, [x,y,z,w]
  const qmul = (a, b) => [a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1], a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0], a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3], a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]];
  const qconj = a => [-a[0], -a[1], -a[2], a[3]];
  function eulerQ(al, be, ga) { // W3C deviceorientation (Z-X'-Y'') → quaternion
    const d = Math.PI / 360, cX = Math.cos(be * d), cY = Math.cos(ga * d), cZ = Math.cos(al * d), sX = Math.sin(be * d), sY = Math.sin(ga * d), sZ = Math.sin(al * d);
    return [sX * cY * cZ - cX * sY * sZ, cX * sY * cZ + sX * cY * sZ, cX * cY * sZ + sX * sY * cZ, cX * cY * cZ - sX * sY * sZ];
  }
  let orQ = null, orNeu = null, orRecenter = false, orSeen = false;
  function onOrient(e) {
    if (e.beta == null && e.gamma == null) return;
    const ang = (((screen.orientation && screen.orientation.angle) ?? (window.orientation || 0)) % 360 + 360) % 360, h = -ang * Math.PI / 360;
    let q = eulerQ(e.alpha || 0, e.beta || 0, e.gamma || 0); q = qmul(q, [0, 0, Math.sin(h), Math.cos(h)]);
    orQ = q; if (!orSeen) { orSeen = true; }
    if (!orNeu || orRecenter) { orNeu = q; orRecenter = false; }
    const tn = performance.now(); yawHist.push([tn, yawNow()]); while (yawHist.length && tn - yawHist[0][0] > 1000) yawHist.shift();
  }
  const yawHist = []; let lastToss = 0;
  const yawAt = ms => { const tn = performance.now() - ms; const h = yawHist.find(x => x[0] >= tn); return h ? h[1] : yawNow(); };
  // rotation of the phone since "center", in the phone's own frame
  const relQ = () => { const r = qmul(qconj(orNeu), orQ); return r.map(v => +v.toFixed(3)); };
  setInterval(() => { if (orQ && orNeu && layout && layout.name === 'swing' && conn && conn.open) { try { conn.send({ t: 'or', q: relQ() }); } catch (e) {} } }, 33);
  let askP = null;
  function askMotion() { return askP || (askP = askMotionOnce().finally(() => { askP = null; })); }
  async function askMotionOnce() {
    if (!needPerm) { listenMotion(); return; }
    if (permState === 'granted') { listenMotion(); return; }
    try {
      // ask for motion AND orientation in the same tap (iPhone shares one prompt)
      const po = typeof DeviceOrientationEvent !== 'undefined' && DeviceOrientationEvent.requestPermission ? DeviceOrientationEvent.requestPermission().catch(() => 'denied') : null;
      const r = await DeviceMotionEvent.requestPermission(); if (po) await po;
      permState = r === 'granted' ? 'granted' : 'denied';
      if (permState === 'granted') { listenMotion(); try { DeviceOrientationEvent.requestPermission && DeviceOrientationEvent.requestPermission().catch(() => {}); } catch (_) {} }
    } catch (e) { /* not called from a tap: stays 'unknown' so the Enable button keeps showing */ }
    motionGate();
  }
  // the gate: a big "Enable motion" card for phones that need permission
  function calButton() {
    if (document.getElementById('calBtn') || !document.querySelector('#ctrlScr .topbar')) return;
    const b = document.createElement('button'); b.id = 'calBtn'; b.className = 'chip'; b.textContent = '🎯 Calibrate';
    b.onclick = () => { if (!motionSeen) return toast('Turn on motion first 📱'); startCalib(true); };
    const st = document.getElementById('status'); st.parentNode.insertBefore(b, st);
  }
  function motionGate() {
    let el = document.getElementById('motionGate');
    const touch = 'ontouchstart' in window || navigator.maxTouchPoints > 0;
    const show = needPerm && touch && permState !== 'granted' && joined && !camMode;
    if (!show) { if (el) el.remove(); return; }
    const big = layout && (layout.name === 'swing' || layout.name === 'shake' || (layout.name === 'pad' && layout.stick));
    if (!el) { el = document.createElement('div'); el.id = 'motionGate'; document.body.appendChild(el); }
    el.className = big ? 'big' : '';
    // only rebuild when something changed, so the button never disappears under your finger
    const key = permState + (big ? 'B' : 's') + (motionGate.skipped ? 'k' : '');
    if (el.dataset.key === key) return; el.dataset.key = key;
    el.innerHTML = permState === 'denied'
      ? `<div class="mgCard"><div class="mgEm">🚫📱</div><b>Motion was blocked</b><p>iPhone remembers "Don't Allow" until Safari restarts. Swipe Safari closed in the app switcher, open the game link again, then tap <b>Allow</b>.</p><button class="mgBtn" id="mgTry">Try again</button><button class="mgSkip" id="mgSkip">Play with touch instead</button></div>`
      : `<div class="mgCard"><div class="mgEm">📱✨</div><b>Turn on motion controls</b><p>Needed to swing, steer and shake. Tap the button, then tap <b>Allow</b>.</p><button class="mgBtn" id="mgGo">Enable motion</button><button class="mgSkip" id="mgSkip">${big ? 'Play with touch instead' : 'Later'}</button></div>`;
    const go = el.querySelector('#mgGo, #mgTry');
    // call requestPermission synchronously inside the click — no awaits before it
    go.onclick = () => { askMotion().then(() => { if (permState === 'granted') { toast('Motion on! 🎉'); if (layout && layout.name === 'swing') setTimeout(() => render(layout), 300); } }); };
    const sk = el.querySelector('#mgSkip'); if (sk) sk.onclick = () => { el.remove(); motionGate.skipped = true; };
    if (motionGate.skipped && !big) el.remove();
  }
  if (!needPerm) askMotion();
  function onMotion(e) {
    const a = e.accelerationIncludingGravity; if (!a || a.x == null) return;
    if (!motionSeen) { motionSeen = true; updateTiltUI(); if (layout && layout.name === 'swing') setTimeout(() => render(layout), 0); }
    let ax = a.x, ay = a.y, az = a.z || 0; if (isIOS) { ax = -ax; ay = -ay; az = -az; }
    const ang = (((screen.orientation && screen.orientation.angle) ?? (window.orientation || 0)) % 360 + 360) % 360;
    let ux, uy;
    if (ang === 90) { ux = -ay; uy = ax; } else if (ang === 180) { ux = -ax; uy = -ay; } else if (ang === 270) { ux = ay; uy = -ax; } else { ux = ax; uy = ay; }
    // low-pass filter so hand jitter doesn't make the plane twitch
    if (!grav) grav = { x: ux, y: uy, z: az }; else { const k = 0.25; grav.x += (ux - grav.x) * k; grav.y += (uy - grav.y) * k; grav.z += (az - grav.z) * k; }
    if (!neutral || recenterSoon) { neutral = { ...grav }; recenterSoon = false; }
    const l = e.acceleration, mag = l && l.x != null ? Math.hypot(l.x, l.y, l.z) : Math.abs(Math.hypot(a.x, a.y, a.z) - 9.81), now = performance.now();
    if (layout && layout.name === 'shake' && mag > 13 && now - lastShake > 110) { lastShake = now; pump(); }
    if (calRun || (layout && layout.name === 'swing')) {
      // a swing = a fast twist of the wrist and/or a hard jolt. Fires the instant it crosses the line (timing matters!)
      const r = e.rotationRate, rot = r ? Math.hypot(r.alpha || 0, r.beta || 0, r.gamma || 0) : 0;
      const strength = Math.max(rot / 420, mag / 14);
      // LIFT = toss: a sharp upward push (along gravity) that is not a swing
      if (!calRun && layout && layout.sport === 'tennis' && l && l.x != null && strength < swingThr * 0.85 && now - lastToss > 900) {
        const gx = a.x - l.x, gy = a.y - l.y, gz = (a.z || 0) - (l.z || 0), gm = Math.hypot(gx, gy, gz) || 1, up = (l.x * gx + l.y * gy + (l.z || 0) * gz) / gm;
        if (up > 6) { lastToss = now; sendMsg({ t: 'act', a: 'toss' }); navigator.vibrate?.(25); toast('⬆ Toss! Now SWING!', 900); }
      }
      if (calRun) calSwing(strength); // calibration measures your swing
      else if (strength > swingThr && now - lastSwing > 420) swing(clamp(0.5 + (strength / swingThr - 1) * 0.5, 0.5, 1), 'motion');
    }
  }
  const tiltApplies = () => motionSeen && layout && ((layout.name === 'pad' && layout.stick) || layout.name === 'swing');
  function updateTiltUI() {
    const show = tiltApplies();
    $('#tiltBtn').classList.toggle('hidden', !show); $('#centerBtn').classList.toggle('hidden', !show || !tiltOn); $('#flipBtn').classList.toggle('hidden', !show || !tiltOn);
    $('#tiltBtn').textContent = tiltOn ? 'Tilt: ON' : 'Tilt: off'; $('#tiltBtn').classList.toggle('on', tiltOn);
    const tb = document.getElementById('tiltBadge'); if (tb) tb.classList.toggle('hidden', !(show && tiltOn));
  }
  $('#tiltBtn').onclick = () => { tiltOn = !tiltOn; store.set('cc_tilt', tiltOn ? '1' : '0'); neutral = grav ? { ...grav } : null; updateTiltUI(); };
  $('#centerBtn').onclick = () => { neutral = grav ? { ...grav } : null; toast('Centered!'); };
  $('#flipBtn').onclick = () => { flip = -flip; store.set('cc_flip', flip < 0 ? '1' : '0'); toast('Tilt direction flipped'); };
  // Angle-based steering, whichever way you hold the phone:
  //  - held up like a steering wheel / yoke -> turn it like a wheel
  //  - held flat -> tip the left or right edge down
  // Measured from the pose captured when the game starts (or when you tap Center).
  const DEG = 180 / Math.PI, DEAD = 3; let LOCK = 28, swingThr = 1;
  const asinG = v => Math.asin(clamp(v / 9.81)) * DEG;
  const wrapDeg = d => ((d + 540) % 360) - 180;
  function shape(deg) { const a = Math.abs(deg); if (a < DEAD) return 0; return Math.sign(deg) * Math.min(1, (a - DEAD) / (LOCK - DEAD)); }
  function tiltAngles() {
    const g = grav, n = neutral, upright = Math.hypot(n.x, n.y) > 5.5;
    let sx, sy;
    if (upright) { // wheel: rotation in the screen plane
      sx = -wrapDeg((Math.atan2(g.x, g.y) - Math.atan2(n.x, n.y)) * DEG);
      sy = -(asinG(g.z) - asinG(n.z));
    } else { // flat: tip the edges
      sx = -(asinG(g.x) - asinG(n.x));
      sy = asinG(g.y) - asinG(n.y);
    }
    return { x: shape(sx * flip), y: shape(sy), dx: sx * flip, dy: sy };
  }

  // ---------- calibration (runs after joining, every time) ----------
  // 1 hold still → sets "straight"  2 turn left  3 turn right → your aim range
  // 4 tilt like a steering wheel → steering range  5 one big swing → swing sensitivity
  let cal = (() => { try { return JSON.parse(store.get('cc_cal') || 'null'); } catch (e) { return null; } })() || { yaw: 35, lock: 28, thr: 1 };
  LOCK = cal.lock; swingThr = cal.thr;
  let calRun = null, calDone = false;
  const qrot = (q, v) => qmul(qmul(q, [v[0], v[1], v[2], 0]), qconj(q)).slice(0, 3);
  function yawNow() { if (!orQ || !orNeu) return 0; const b = qrot(qmul(qconj(orNeu), orQ), [0, 0, -1]); return Math.atan2(b[0], -b[2]) * DEG; } // + = back of phone turned to your right
  const STEPS = [
    { id: 'still', title: 'Hold it upright', text: 'Hold your phone <b>upright</b>, screen facing you, like a handle. Keep it <b>still</b>…', anim: 'still' },
    { id: 'left', title: 'Turn it LEFT ⬅', text: 'Point the back of the phone to the <b>left</b> as far as feels comfy, and hold.', anim: 'left', needOr: true },
    { id: 'right', title: 'Turn it RIGHT ➡', text: 'Now point it to the <b>right</b>, and hold.', anim: 'right', needOr: true },
    { id: 'wheel', title: 'Steer RIGHT 🚗', text: 'Back to the middle, then <b>tilt it like a steering wheel</b> to the right, and hold.', anim: 'wheel' },
    { id: 'swing', title: 'One BIG swing! 🎾', text: 'Swing your phone like a <b>racket</b>. Hold on tight!', anim: 'swing' },
  ];
  function startCalib(force) {
    if (calRun || (calDone && !force)) return;
    if (!motionSeen) return;
    const steps = STEPS.filter(st => !st.needOr || orSeen);
    calRun = { steps, i: -1, res: {}, t0: 0, hold: 0, peak: 0, lastQ: null, still: 0 };
    let el = document.getElementById('calib');
    if (!el) { el = document.createElement('div'); el.id = 'calib'; document.body.appendChild(el); }
    el.innerHTML = `<div class="clCard"><div class="clTop">🎯 QUICK CALIBRATION <span id="clStep"></span></div><div class="clDots" id="clDots">${steps.map(() => '<i></i>').join('')}</div>
      <div class="clStage"><div class="clPhone" id="clPhone"><div class="clScreen">🎮</div></div></div><h2 id="clTitle"></h2><p id="clText"></p>
      <div class="clMeter"><i id="clFill"></i></div><button class="mgSkip" id="clSkip">Skip calibration</button></div>`;
    $('#clSkip').onclick = () => endCalib(false);
    calNext();
    calRun.timer = setInterval(calTick, 50);
  }
  function calNext() {
    const R = calRun; R.i++; R.hold = 0; R.peak = 0; R.t0 = performance.now(); R.best = 0;
    if (R.i >= R.steps.length) return endCalib(true);
    const st = R.steps[R.i];
    $('#clStep').textContent = `${R.i + 1}/${R.steps.length}`; $('#clTitle').textContent = st.title; $('#clText').innerHTML = st.text;
    document.querySelectorAll('#clDots i').forEach((d, k) => d.className = k < R.i ? 'ok' : k === R.i ? 'on' : '');
    $('#clPhone').className = 'clPhone ' + st.anim; $('#clFill').style.width = '0%';
    navigator.vibrate?.(30);
  }
  function calOk(msg) { const R = calRun; toastCal(msg); navigator.vibrate?.([20, 30, 20]); document.querySelectorAll('#clDots i')[R.i].className = 'ok'; R.wait = performance.now() + 450; }
  function toastCal(m) { const t = $('#clText'); if (t) t.innerHTML = `<b style="color:#51cf66">✓ ${m}</b>`; }
  function calTick() {
    const R = calRun; if (!R) return; const st = R.steps[R.i], now = performance.now(), fill = $('#clFill'), ph = $('#clPhone');
    if (R.wait) { if (now > R.wait) { R.wait = 0; calNext(); } return; }
    if (st.id === 'still') {
      // still = the phone barely moved for 1 second
      const q = orQ || (grav ? [grav.x, grav.y, grav.z, 0] : null), moved = q && R.lastQ ? Math.abs(q[0] - R.lastQ[0]) + Math.abs(q[1] - R.lastQ[1]) + Math.abs(q[2] - R.lastQ[2]) + Math.abs(q[3] - R.lastQ[3]) : 1;
      R.lastQ = q ? q.slice() : null;
      R.hold = (orQ ? moved < 0.012 : moved < 0.25) ? R.hold + 50 : 0;
      fill.style.width = Math.min(100, R.hold / 10) + '%';
      if (R.hold >= 1000) { orNeu = orQ; neutral = grav ? { ...grav } : null; calOk('Got it — that is "straight"'); }
    } else if (st.id === 'left' || st.id === 'right') {
      const y = yawNow(), want = st.id === 'left' ? -y : y; ph.style.transform = `perspective(400px) rotateY(${clamp(y / 70) * 70}deg)`;
      fill.style.width = clamp(want / 40, 0, 1) * 100 + '%';
      if (want > 18) { R.best = Math.max(R.best, want); R.hold += 50; } else R.hold = 0;
      if (R.hold >= 600) { R.res[st.id] = R.best; calOk(`${Math.round(R.best)}° — nice`); }
    } else if (st.id === 'wheel') {
      const t = grav && neutral ? tiltAngles() : { dx: 0 }, d = Math.abs(t.dx || 0); ph.style.transform = `rotate(${clamp((t.dx || 0) / 60) * 60}deg)`;
      fill.style.width = clamp(d / 35, 0, 1) * 100 + '%';
      if (d > 12) { R.best = Math.max(R.best, d); R.hold += 50; } else R.hold = 0;
      if (R.hold >= 600) { R.res.wheel = R.best; calOk(`${Math.round(R.best)}° steering`); }
    } else if (st.id === 'swing') {
      fill.style.width = clamp(R.peak / 2.5, 0, 1) * 100 + '%';
      if (R.peak > 0.7 && now - R.peakAt > 250) { R.res.swing = R.peak; calOk(R.peak > 2 ? 'MONSTER swing! 💥' : 'Great swing!'); }
      if (now - R.t0 > 12000) { R.res.swing = 1.6; calOk('OK — using normal swing'); }
    }
  }
  function calSwing(strength) {
    const R = calRun; if (!R || R.wait || R.steps[R.i].id !== 'swing') return;
    if (strength > R.peak) { R.peak = strength; R.peakAt = performance.now(); const ph = $('#clPhone'); if (ph) { ph.classList.remove('hit'); void ph.offsetWidth; ph.classList.add('hit'); } }
  }
  function endCalib(ok) {
    const R = calRun; if (!R) return; clearInterval(R.timer); calRun = null; calDone = true;
    if (ok) {
      const yaw = R.res.left && R.res.right ? (R.res.left + R.res.right) / 2 : cal.yaw;
      cal = { yaw: clamp(yaw, 18, 70), lock: clamp((R.res.wheel || 32) * 0.85, 14, 40), thr: clamp((R.res.swing || 1.6) * 0.5, 0.7, 1.8) };
      store.set('cc_cal', JSON.stringify(cal)); LOCK = cal.lock; swingThr = cal.thr; sendMsg({ t: 'calib', yaw: Math.round(cal.yaw) });
      const el = document.getElementById('calib'); if (el) el.innerHTML = `<div class="clCard"><div class="clDone">✅</div><h2>Calibrated!</h2><p>Aim range ${Math.round(cal.yaw)}° · steering ${Math.round(cal.lock)}° · swing tuned to you</p></div>`;
      navigator.vibrate?.([40, 40, 80]);
      setTimeout(() => { const e2 = document.getElementById('calib'); if (e2) e2.remove(); }, 1500);
    } else { const el = document.getElementById('calib'); if (el) el.remove(); sendMsg({ t: 'calib', yaw: Math.round(cal.yaw) }); }
  }
  // start once motion is actually working (after the Enable-motion tap on iPhone)
  function maybeCalib() { if (camMode) return; if (joined && !calDone && !calRun && motionSeen && (!needPerm || permState === 'granted')) startCalib(); }
  setInterval(maybeCalib, 400);

  // ---------- input state ----------
  const keys = { left: false, right: false, up: false, down: false, a: false, b: false }, btn = { a: false, b: false }, stick = { active: false, x: 0, y: 0 };
  const KEYMAP = { ArrowLeft: 'left', KeyA: 'left', ArrowRight: 'right', KeyD: 'right', ArrowUp: 'up', KeyW: 'up', ArrowDown: 'down', KeyS: 'down', Space: 'a', KeyJ: 'a', Enter: 'a', ShiftLeft: 'b', ShiftRight: 'b', KeyK: 'b', KeyE: 'b' };
  function readInput() {
    let x = 0, y = 0;
    if (keys.left) x -= 1; if (keys.right) x += 1; if (keys.up) y -= 1; if (keys.down) y += 1;
    if (stick.active) { x = stick.x; y = stick.y; }
    else if (!x && !y && tiltOn && tiltApplies() && grav && neutral) {
      const t = tiltAngles(); x = t.x; y = layout.stick === 'xy' ? t.y : 0;
    }
    if (layout && layout.stick === 'x') y = 0;
    return { x, y, a: keys.a || btn.a, b: keys.b || btn.b };
  }
  let lastSent = { x: 0, y: 0, a: false, b: false };
  function flush(force) {
    if (!conn || !conn.open || !layout || !['pad', 'button', 'swing'].includes(layout.name)) return;
    const s = readInput();
    if (!force && Math.abs(s.x - lastSent.x) < 0.03 && Math.abs(s.y - lastSent.y) < 0.03 && s.a === lastSent.a && s.b === lastSent.b) return;
    s.x = +s.x.toFixed(2); s.y = +s.y.toFixed(2);
    try { conn.send({ t: 'in', ...s }); } catch (e) {}
    lastSent = s;
    drawYoke(s);
    const ad = document.getElementById('aimDot'); if (ad) ad.style.left = (50 + s.x * 46) + '%';
  }
  function drawYoke(s) {
    const yk = document.getElementById('yoke'); if (!yk) return;
    yk.style.transform = `translateY(${s.y * 26}px) rotate(${s.x * 55}deg)`;
    const L = document.getElementById('yL'), R = document.getElementById('yR');
    if (L) L.style.opacity = s.x < -0.05 ? 0.35 + Math.min(1, -s.x) * 0.65 : 0.15;
    if (R) R.style.opacity = s.x > 0.05 ? 0.35 + Math.min(1, s.x) * 0.65 : 0.15;
    const m = document.getElementById('yMeter'); if (m) m.style.left = (50 + s.x * 50) + '%';
  }
  setInterval(() => flush(false), 33); setInterval(() => flush(true), 1000);
  function resetInput() { Object.keys(keys).forEach(k => keys[k] = false); btn.a = btn.b = false; stick.active = false; stick.x = stick.y = 0; flush(true); }
  function sendMsg(m) { if (conn && conn.open) { try { conn.send(m); } catch (e) {} } }
  let pumps = 0;
  function pump() { sendMsg({ t: 'pump', n: 1 }); pumps++; const c = document.getElementById('shakeN'); if (c) { c.textContent = pumps; c.classList.remove('wiggle'); void c.offsetWidth; c.classList.add('wiggle'); } }

  // ---------- keyboard ----------
  let lastShakeKey = null, cardSel = 0;
  addEventListener('keydown', e => {
    if (e.target.tagName === 'INPUT' || !layout) return;
    if (layout.name === 'shake') { if (!e.repeat && e.code !== lastShakeKey) { lastShakeKey = e.code; pump(); } e.preventDefault(); return; }
    if (layout.name === 'swing' && (e.code === 'Space' || e.code === 'Enter')) { e.preventDefault(); if (!e.repeat) swing(0.7, 'key'); return; }
    if (layout.name === 'cards') return cardKey(e);
    if (layout.name === 'lobby' && layout.vip) { const m = { ArrowLeft: 'left', ArrowRight: 'right', ArrowUp: 'up', ArrowDown: 'down', Enter: 'ok', Space: 'ok', Escape: 'back' }[e.code]; if (m) { e.preventDefault(); sendMsg({ t: 'nav', d: m }); } return; }
    if (layout.name === 'intro' && (e.code === 'Enter' || e.code === 'Space')) { e.preventDefault(); readyUp(); return; }
    if (layout.name === 'choice') { const n = parseInt(e.key, 10); if (n >= 1 && n <= (layout.options || []).length) { const b = stage.querySelectorAll('.choiceGrid button')[n - 1]; if (b && !b.disabled) b.click(); } return; }
    const k = KEYMAP[e.code]; if (k) { e.preventDefault(); if (!keys[k]) { keys[k] = true; flush(); } }
  });
  addEventListener('keyup', e => { const k = KEYMAP[e.code]; if (k) { keys[k] = false; flush(); } });
  addEventListener('blur', resetInput);

  // ---------- layouts ----------
  let camMode = false;
  function startCam() {
    if (!window.CCCam) return toast('AI camera not available');
    camMode = true; calDone = true; if (calRun) endCalib(false); const mg = document.getElementById('motionGate'); if (mg) mg.remove();
    sendMsg({ t: 'camOn' }); CCCam.start(m => sendMsg(m), stage, toast);
  }
  addEventListener('cc-cam-stop', () => { camMode = false; sendMsg({ t: 'camOff' }); stage.innerHTML = '<div class="center"><p>Back to being a player…</p></div>'; });
  const camBtnHtml = () => `<button class="camBtn" id="camB">📷 Use this phone as the AI camera<small>Body tracking for up to 4 players — no phones needed</small></button>`;
  function bindCamBtn() { const b = document.getElementById('camB'); if (b) b.onclick = startCam; }
  function render(L) {
    if (camMode) return; // the camera phone ignores game screens
    layout = L; resetInput(); pumps = 0;
    if ((L.name === 'pad' && L.stick) || L.name === 'swing') recenterSoon = true;
    if (L.name === 'swing') orRecenter = true;
    if (needPerm) setTimeout(motionGate, 0);
    if (L.name !== 'cards') lastHand = null;
    const R = { swing: renderSwing, lobby: renderLobby, pad: renderPad, button: renderButton, shake: renderShake, intro: renderIntro, choice: renderChoice, draw: renderDraw, bingo: renderBingo, dice: renderDice };
    if (R[L.name]) R[L.name](L);
    else if (L.name === 'cards') { lastHand ? renderHand(lastHand) : (stage.innerHTML = '<div class="center"><p>Dealing…</p></div>'); }
    else renderMsg(L);
    updateTiltUI();
  }
  function renderMsg(L) { stage.innerHTML = `<div class="center"><div class="bigEm">${esc(L.emoji || '📺')}</div><h2>${esc(L.text || 'Look at the TV!')}</h2>${L.sub ? `<p class="muted">${esc(L.sub)}</p>` : ''}</div>`; }
  let showList = false;
  function renderLobby(L) {
    if (!L.vip) {
      stage.innerHTML = `<div class="center"><div class="bigEm">${esc(me ? me.avatar : '🎉')}</div><h2>You're in, ${esc(me ? me.name : '')}!</h2>${L.trophies ? `<p>🏆 ${L.trophies} trophies</p>` : ''}<p class="muted">👑 <b>${esc(L.vipName || 'The VIP')}</b> is picking a game on the TV…</p>${camBtnHtml()}</div>`;
      bindCamBtn(); return;
    }
    const list = showList ? L.cats.map(c => `<div class="catH">${esc(c.name)}</div>` + L.games.filter(gm => gm.cat === c.id).map(gm => `<button class="gbtn" data-g="${esc(gm.id)}"><span class="ge">${gm.emoji}</span><span><b>${esc(gm.name)}</b></span></button>`).join('')).join('') : '';
    stage.innerHTML = `<div class="remote">
      <div class="muted">👑 You're the VIP — you control the TV</div>
      <div class="focusCard">Selected<b id="focusName">${vipFocus ? esc(vipFocus.emoji + ' ' + vipFocus.name) : '—'}</b></div>
      <div class="dpad"><span></span><button data-d="up">▲</button><span></span><button data-d="left">◀</button><button class="ok" data-d="ok">OK</button><button data-d="right">▶</button><span></span><button data-d="down">▼</button><span></span></div>
      <div class="qbtns"><button id="partyB">🎉 Party Mode</button><button id="randB" class="alt">🎲 Surprise Me</button></div>
      <button class="linkBtn" id="listT">${showList ? 'Hide game list' : 'Or pick from a list'}</button>
      <div class="gbtns">${list}</div>${camBtnHtml()}</div>`;
    bindCamBtn();
    stage.querySelectorAll('.dpad button').forEach(b => b.addEventListener('pointerdown', e => { e.preventDefault(); sendMsg({ t: 'nav', d: b.dataset.d }); navigator.vibrate?.(10); }));
    $('#partyB').onclick = () => sendMsg({ t: 'party' }); $('#randB').onclick = () => sendMsg({ t: 'random' });
    $('#listT').onclick = () => { showList = !showList; renderLobby(L); };
    stage.querySelectorAll('.gbtn').forEach(b => b.onclick = () => { sendMsg({ t: 'pick', g: b.dataset.g }); navigator.vibrate?.(30); });
  }
  function readyUp() { if (!layout || layout.name !== 'intro' || layout.ready) return; layout.ready = true; sendMsg({ t: 'ready' }); navigator.vibrate?.(30); renderIntro(layout); }
  function renderIntro(L) {
    stage.innerHTML = `<div class="introC"><div class="bigEm">${esc(L.emoji)}</div><h2>${esc(L.title)}</h2><div class="muted">${esc(L.ctrl || '')}</div>
      <ol>${(L.how || []).map(h => `<li>${esc(h)}</li>`).join('')}</ol>
      <button class="readyBtn ${L.ready ? 'done' : ''}" id="readyB">${L.ready ? '✓ READY' : "I'M READY!"}</button></div>`;
    $('#readyB').onclick = readyUp;
  }
  function renderChoice(L) {
    const multi = L.multi || 0; let taps = 0;
    stage.innerHTML = `<div class="choiceC"><div class="prompt ${L.secret ? 'secret' : ''}">${esc(L.prompt || '')}</div>${multi ? `<div class="multiDots" id="dots">${'○'.repeat(multi)}</div>` : ''}
      <div class="choiceGrid ${L.big ? 'big' : ''}" style="grid-template-columns:repeat(${L.cols || 2},1fr)">${(L.options || []).map((o, i) => `<button data-i="${i}" class="${light(o.color) ? 'light' : ''}" style="background:${esc(o.color)}" ${o.disabled ? 'disabled' : ''}>${esc(o.label)}</button>`).join('')}</div></div>`;
    stage.querySelectorAll('.choiceGrid button').forEach(b => b.onclick = () => {
      sendMsg({ t: 'act', a: 'choice', i: +b.dataset.i }); navigator.vibrate?.(20);
      if (multi) { taps++; const d = $('#dots'); if (d) d.textContent = '●'.repeat(taps) + '○'.repeat(Math.max(0, multi - taps)); b.style.transform = 'scale(.92)'; setTimeout(() => b.style.transform = '', 100); if (taps >= multi) stage.querySelectorAll('.choiceGrid button').forEach(x => x.disabled = true); }
      else stage.querySelectorAll('.choiceGrid button').forEach(x => { x.disabled = true; if (x === b) { x.disabled = false; x.style.outline = '5px solid #fff'; } });
    });
  }
  function renderDraw(L) {
    const COLS = ['#1a1438', '#e03131', '#1c7ed6', '#2f9e44', '#f59f00', '#ae3ec9', '#8d5524', '#fffdf7'];
    stage.innerHTML = `<div class="drawC"><div class="drawWord">Draw: <b>${esc(L.word)}</b> <span class="muted">(no letters!)</span>${innerHeight > innerWidth ? '<div class="muted" style="font-size:13px">📱↻ Turn your phone sideways for a bigger canvas</div>' : ''}</div>
      <div class="drawWrap" id="dw"><canvas id="pad"></canvas></div>
      <div class="drawTools">${COLS.map((c, i) => `<button data-c="${c}" class="${i ? '' : 'sel'}" style="background:${c}"></button>`).join('')}<button class="clr" data-w="1">✏️ Thin</button><button class="clr" id="clrB">🗑 Clear</button></div></div>`;
    const cvp = $('#pad'), dw = $('#dw'), ratio = 1100 / 690;
    const sz = () => { const w = Math.min(dw.clientWidth, dw.clientHeight * ratio); cvp.width = Math.round(w); cvp.height = Math.round(w / ratio); };
    sz(); const ctx = cvp.getContext('2d'); ctx.lineCap = ctx.lineJoin = 'round';
    let color = COLS[0], width = 7, sid = 0, cur = null, buf = [];
    stage.querySelectorAll('.drawTools button[data-c]').forEach(b => b.onclick = () => { color = b.dataset.c; width = color === '#fffdf7' ? 24 : width === 24 ? 7 : width; stage.querySelectorAll('.drawTools button[data-c]').forEach(x => x.classList.toggle('sel', x === b)); });
    stage.querySelector('[data-w]').onclick = e => { width = width === 3 ? 7 : width === 7 ? 14 : 3; e.target.textContent = { 3: '✏️ Thin', 7: '🖊 Medium', 14: '🖌 Thick' }[width]; };
    $('#clrB').onclick = () => { ctx.clearRect(0, 0, cvp.width, cvp.height); sendMsg({ t: 'act', a: 'clear' }); };
    const pt = e => { const r = cvp.getBoundingClientRect(); return [clamp((e.clientX - r.left) / r.width, 0, 1), clamp((e.clientY - r.top) / r.height, 0, 1)]; };
    const flushS = () => { if (cur && buf.length) { sendMsg({ t: 'act', a: 'stroke', id: cur.id, c: cur.c, w: cur.w, pts: buf.map(p => [+p[0].toFixed(3), +p[1].toFixed(3)]) }); buf = []; } };
    const timerS = setInterval(() => { if (!document.getElementById('pad')) return clearInterval(timerS); flushS(); }, 60);
    cvp.addEventListener('pointerdown', e => { e.preventDefault(); try { cvp.setPointerCapture(e.pointerId); } catch (_) {} cur = { id: `${Date.now()}-${sid++}`, c: color, w: width }; const p = pt(e); buf.push(p); cur.last = p; ctx.strokeStyle = color; ctx.lineWidth = width * cvp.width / 600; ctx.beginPath(); ctx.arc(p[0] * cvp.width, p[1] * cvp.height, ctx.lineWidth / 2, 0, 7); ctx.fillStyle = color; ctx.fill(); });
    cvp.addEventListener('pointermove', e => { if (!cur) return; const p = pt(e); ctx.strokeStyle = cur.c; ctx.lineWidth = cur.w * cvp.width / 600; ctx.beginPath(); ctx.moveTo(cur.last[0] * cvp.width, cur.last[1] * cvp.height); ctx.lineTo(p[0] * cvp.width, p[1] * cvp.height); ctx.stroke(); cur.last = p; buf.push(p); });
    const up = () => { flushS(); cur = null; }; cvp.addEventListener('pointerup', up); cvp.addEventListener('pointercancel', up);
  }
  function renderBingo(L) {
    const marks = new Set(L.marks), called = new Set(L.called || []);
    stage.innerHTML = `<div class="bingoC"><div class="bingoLast">${L.last ? `Last call: <b>${'BINGO'[Math.floor((L.last - 1) / 15)]}-${L.last}</b>` : 'Waiting for the first ball…'}</div>
      <div class="bcard">${'BINGO'.split('').map(c => `<div class="bh">${c}</div>`).join('')}${L.card.map((n, i) => i === 12 ? `<button class="free mk" disabled>FREE</button>` : `<button data-i="${i}" class="${marks.has(i) ? 'mk' : ''} ${called.has(n) ? 'called' : ''}">${n}</button>`).join('')}</div>
      <button class="bingoBtn" id="bingoB" ${L.won || L.locked ? 'disabled' : ''}>${L.won ? '🏆 BINGO!' : L.locked ? '❄️ FROZEN' : 'BINGO!'}</button>
      <div class="muted" style="font-size:13px">Blue outline = recently called. Tap to mark.</div></div>`;
    stage.querySelectorAll('.bcard button[data-i]').forEach(b => b.onclick = () => { b.classList.toggle('mk'); sendMsg({ t: 'act', a: 'mark', i: +b.dataset.i }); navigator.vibrate?.(10); });
    $('#bingoB').onclick = () => { sendMsg({ t: 'act', a: 'bingo' }); navigator.vibrate?.([40, 30, 40]); };
  }
  const PIPPOS = { 1: [4], 2: [0, 8], 3: [0, 4, 8], 4: [0, 2, 6, 8], 5: [0, 2, 4, 6, 8], 6: [0, 2, 3, 5, 6, 8] };
  const PIPS = Array.from({ length: 7 }, (_, v) => `<span class="pips">${Array.from({ length: 9 }, (_, k) => `<i class="${(PIPPOS[v] || []).includes(k) ? (v === 1 ? 'on red' : 'on') : ''}"></i>`).join('')}</span>`);
  function renderDice(L) {
    stage.innerHTML = `<div class="diceC"><div class="muted">Round ${L.round}/${L.rounds} · tap dice to HOLD</div>
      <div class="diceRow">${L.dice.map((v, i) => `<button class="die ${L.held[i] ? 'held' : ''}" data-i="${i}" ${L.done ? 'disabled' : ''}>${PIPS[v]}</button>`).join('')}</div>
      <div class="handName">${esc(L.hand)}</div>
      <div class="diceBtns"><button id="rollB" ${L.done || !L.left ? 'disabled' : ''}>🎲 Roll (${L.left})</button><button id="doneB" class="done" ${L.done ? 'disabled' : ''}>${L.done ? '✓ Locked' : '✋ Keep'}</button></div></div>`;
    stage.querySelectorAll('.die').forEach(b => b.onclick = () => { sendMsg({ t: 'act', a: 'hold', i: +b.dataset.i }); b.classList.toggle('held'); });
    $('#rollB').onclick = () => { sendMsg({ t: 'act', a: 'roll' }); navigator.vibrate?.(40); };
    $('#doneB').onclick = () => sendMsg({ t: 'act', a: 'done' });
  }
  function bindHold(el, key) {
    const dn = e => { e.preventDefault(); try { el.setPointerCapture(e.pointerId); } catch (_) {} btn[key] = true; el.classList.add('on'); flush(); navigator.vibrate?.(12); };
    const up = () => { if (!btn[key]) return; btn[key] = false; el.classList.remove('on'); flush(); };
    el.addEventListener('pointerdown', dn); el.addEventListener('pointerup', up); el.addEventListener('pointercancel', up); el.addEventListener('lostpointercapture', up);
    el.addEventListener('contextmenu', e => e.preventDefault());
  }
  function renderPad(L) {
    const xOnly = L.stick === 'x', noStick = !L.stick;
    stage.innerHTML = `<div class="pad ${noStick ? 'nostick' : ''}">
      ${noStick ? '' : `<div class="stickZone yokeZone" id="sz"><div class="tiltBadge hidden" id="tiltBadge">📱 Turn your phone like a steering wheel</div>
        <div class="yArrow l" id="yL">◀</div><div class="yArrow r" id="yR">▶</div>
        <div class="yokeWrap"><svg id="yoke" class="yoke" viewBox="0 0 240 150" aria-hidden="true">
          <rect x="108" y="88" width="24" height="62" rx="8" fill="#495057"/>
          <path d="M20 40 Q20 20 40 20 L70 20 Q84 20 90 34 L100 58 L140 58 L150 34 Q156 20 170 20 L200 20 Q220 20 220 40 L220 80 Q220 104 196 104 L44 104 Q20 104 20 80 Z" fill="#343a40" stroke="#868e96" stroke-width="4"/>
          <rect x="14" y="28" width="38" height="70" rx="16" fill="var(--c,#4dabff)"/><rect x="188" y="28" width="38" height="70" rx="16" fill="var(--c,#4dabff)"/>
          <circle cx="120" cy="82" r="20" fill="#212529" stroke="#adb5bd" stroke-width="4"/><circle cx="120" cy="82" r="6" fill="#ffd43b"/>
          <rect x="30" y="34" width="6" height="58" rx="3" fill="rgba(255,255,255,.35)"/><rect x="204" y="34" width="6" height="58" rx="3" fill="rgba(255,255,255,.35)"/>
        </svg></div>
        <div class="yBar"><i id="yMeter"></i></div>
        <div class="szHint">${esc(L.hint || '')}<br><small>${xOnly ? 'No tilt? Touch and hold the left or right side.' : 'No tilt? Drag the yoke.'}</small></div></div>`}
      <div class="btns ${noStick ? 'solo' : ''}">${L.b ? `<button class="pbtn pb" data-b="b">${esc(L.b)}</button>` : ''}${L.a ? `<button class="pbtn pa" data-b="a">${esc(L.a)}</button>` : ''}</div>
      <div class="keys">PC: ${noStick ? '' : xOnly ? '← → · ' : 'WASD/arrows · '}${L.a ? 'Space = ' + esc(L.a) : ''}${L.b ? ' · Shift = ' + esc(L.b) : ''}</div></div>`;
    if (noStick && L.hint) stage.querySelector('.btns').insertAdjacentHTML('beforebegin', `<div class="szHint" style="position:static;padding:12px">${esc(L.hint)}</div>`);
    stage.querySelectorAll('.pbtn').forEach(b => bindHold(b, b.dataset.b));
    if (noStick) return;
    const sz = $('#sz'); let ox = 0, oy = 0, pid = null;
    // x-only games: where you touch = how hard you turn (left side = left, right side = right, middle = straight)
    const absX = e => { const r = sz.getBoundingClientRect(), v = (e.clientX - (r.left + r.width / 2)) / (r.width * 0.32), a = Math.abs(v); return a < 0.18 ? 0 : Math.sign(v) * Math.min(1, 0.35 + (a - 0.18) * 0.8); };
    sz.addEventListener('pointerdown', e => { pid = e.pointerId; try { sz.setPointerCapture(pid); } catch (_) {} ox = e.clientX; oy = e.clientY; stick.active = true; stick.x = xOnly ? absX(e) : 0; stick.y = 0; flush(); });
    sz.addEventListener('pointermove', e => { if (e.pointerId !== pid) return; if (xOnly) { stick.x = absX(e); stick.y = 0; flush(); return; } let dx = (e.clientX - ox) / 60, dy = (e.clientY - oy) / 60; const l = Math.hypot(dx, dy); if (l > 1) { dx /= l; dy /= l; } stick.x = dx; stick.y = dy; flush(); });
    const end = e => { if (e.pointerId !== pid) return; pid = null; stick.active = false; stick.x = stick.y = 0; flush(); };
    sz.addEventListener('pointerup', end); sz.addEventListener('pointercancel', end);
  }
  // ---------- swing (motion sports) ----------
  const SPORT = { tennis: ['🎾', 'a tennis racket'], bowling: ['🎳', 'a bowling arm'], bat: ['⚾', 'a baseball bat'] };
  function swing(power, how) {
    if (!layout || layout.name !== 'swing') return;
    lastSwing = performance.now();
    if (how === 'motion' && orQ && orNeu) {
      const t0 = performance.now(), pre = yawAt(220), y0 = yawNow(), q0 = relQ();
      setTimeout(() => {
        const dir = clamp((yawNow() - y0) / 32), point = clamp(pre / ((cal && cal.yaw) || 35));
        const aim = clamp(dir * 0.85 + point * 0.45);
        sendMsg({ t: 'act', a: 'swing', p: +power.toFixed(2), how, q: q0, aim: +aim.toFixed(2), lag: +((performance.now() - t0) / 1000).toFixed(3) });
      }, 110);
    } else sendMsg({ t: 'act', a: 'swing', p: +power.toFixed(2), how, q: orQ && orNeu ? relQ() : undefined });
    navigator.vibrate?.(power > 0.8 ? [30, 20, 60] : 35);
    const ic = document.getElementById('swIcon'); if (ic) { ic.classList.remove('hit'); void ic.offsetWidth; ic.classList.add('hit'); }
    const pw = document.getElementById('swPow'); if (pw) { pw.style.transition = 'none'; pw.style.width = Math.round(power * 100) + '%'; void pw.offsetWidth; pw.style.transition = 'width 1.2s ease-in'; pw.style.width = '0%'; }
    const z = document.getElementById('swz'); if (z) { z.classList.remove('flash'); void z.offsetWidth; z.classList.add('flash'); }
    const lb = document.getElementById('swPowTxt'); if (lb) lb.textContent = power > 0.85 ? 'MONSTER SWING! 💥' : power > 0.65 ? 'Nice swing!' : 'Swing!';
  }
  // Motion sports are played with MOTION ONLY: tilt the phone to aim, swing it to hit.
  // Tap / drag only appear as a fallback when the device has no motion (PC, or motion blocked).
  function renderSwing(L) {
    const [icon, noun] = SPORT[L.sport] || ['🏏', 'a bat'], motion = motionSeen && tiltOn;
    stage.innerHTML = `<div class="swingPad ${motion ? 'motion' : ''}">
      <div class="aimBar" id="aimBar"><div class="aimTrack"><i id="aimDot"></i></div><small>${motion ? '📱 TILT your phone left / right to aim 🎯' : '⬅ drag here to aim ➡'}</small></div>
      <div class="swingZone" id="swz">
        <div class="swRings"><i></i><i></i><i></i></div>
        <div class="swIcon" id="swIcon">${icon}</div>
        <div class="swLabel">${motion ? 'SWING IT!' : esc(L.label || 'SWING!')}</div>
        <div class="swPowBar"><i id="swPow"></i></div><div class="swPowTxt" id="swPowTxt">${motion ? 'Hold it like ' + noun + ' and SWING!' : 'Motion is off'}</div>
        <div class="swHint">${motion ? (L.sport === 'tennis' ? 'Hold it like a racket (screen facing you). <b>Serve:</b> just swing. <b>Rally:</b> swing when the ring closes, <b>toward</b> where you want it. <b>Purple ring</b> = high ball → swing hard to SMASH!' : 'Hold the phone <b>upright, screen facing you</b>, like a handle. It moves 1:1 on the TV — point where you want it and swing!') : esc(L.hint || '')}</div>
        ${motion ? '<button class="chip on" id="swCenter">⟲ CENTER (hold still, screen to you)</button>' : `<button class="chip on" id="swMotion">📱 Turn on motion controls</button><div class="swHint"><small>No motion? Tap here to swing · PC: Space + ← →</small></div>`}
      </div></div>`;
    if (motion) {
      $('#swCenter').onclick = e => { e.stopPropagation(); neutral = grav ? { ...grav } : null; orRecenter = true; toast('Centered! Hold it like this = straight ahead'); };
      return;
    }
    $('#swMotion').onclick = async e => { e.stopPropagation(); if (!tiltOn) { tiltOn = true; store.set('cc_tilt', '1'); } await askMotion(); setTimeout(() => { if (motionSeen) render(layout); else toast('This device has no motion sensor — tap to swing instead'); }, 600); };
    $('#swz').addEventListener('pointerdown', e => { if (e.target.closest('#aimBar, button')) return; swing(0.7, 'tap'); });
    const bar = $('#aimBar'); let pid = null;
    const setX = e => { const r = bar.getBoundingClientRect(); stick.x = clamp(((e.clientX - r.left) / r.width - 0.5) * 2.2); stick.y = 0; flush(); };
    bar.addEventListener('pointerdown', e => { pid = e.pointerId; try { bar.setPointerCapture(pid); } catch (_) {} stick.active = true; setX(e); });
    bar.addEventListener('pointermove', e => { if (e.pointerId === pid) setX(e); });
    const end = e => { if (e.pointerId !== pid) return; pid = null; };
    bar.addEventListener('pointerup', end); bar.addEventListener('pointercancel', end);
  }
  function renderButton(L) {
    stage.innerHTML = `<div class="center"><button class="tapBig" id="tap">${esc(L.label || 'TAP!')}</button><p class="muted">${esc(L.hint || '')}</p><p class="muted" style="font-size:13px">PC: Space</p></div>`;
    bindHold($('#tap'), 'a');
  }
  function renderShake(L) {
    stage.innerHTML = `<div class="center"><h2>${motionSeen ? '📳 SHAKE YOUR PHONE!' : '⌨️ MASH ← → (alternate)'}</h2><div class="shakeCount" id="shakeN">0</div>
      <button class="tapBig" id="tap" style="font-size:30px">or TAP here</button><p class="muted">${esc(L.hint || '')}</p></div>`;
    $('#tap').addEventListener('pointerdown', e => { e.preventDefault(); pump(); $('#tap').classList.add('on'); setTimeout(() => $('#tap') && $('#tap').classList.remove('on'), 80); });
  }

  // ---------- cards (Wild Eights) ----------
  const SPECIAL = { '8': 'WILD', '2': '+2', 'J': 'SKIP', 'Q': 'REV', 'A': 'CHAOS' };
  const isRed = c => c && (c.endsWith('♥') || c.endsWith('♦'));
  function cardHTML(c, extra = '', i) { const r = c.slice(0, -1), s = c.slice(-1); return `<button class="card ${isRed(c) ? 'redc' : ''} ${extra}" ${i != null ? `data-i="${i}"` : ''}><span>${esc(r)}</span><span class="cs">${s}</span>${SPECIAL[r] ? `<span class="ct">${SPECIAL[r]}</span>` : ''}</button>`; }
  function renderHand(m) {
    lastHand = m; if (!layout || layout.name !== 'cards') return;
    if (cardSel >= m.cards.length) cardSel = Math.max(0, m.cards.length - 1);
    stage.innerHTML = `<div class="cardsUI"><div class="cardsTop"><div class="turnTxt ${m.myTurn ? 'mine' : ''}">${m.myTurn ? '⭐ YOUR TURN!' : `<span style="color:${esc(m.turnColor)}">${esc(m.turnName)}</span>'s turn…`}</div>
      <div class="topInfo">${cardHTML(m.top, 'mini')}<div class="suitNow">suit<b class="${'♥♦'.includes(m.suit) ? 'red' : 'blk'}">${m.suit}</b></div></div></div>
      ${m.pending ? `<div class="pend">🔥 +${m.pending} stack! Play a 2 to pass it on, or take them</div>` : ''}
      <div class="hand">${m.cards.map((c, i) => cardHTML(c, (m.myTurn ? (m.playable[i] ? 'ok' : 'no') : '') + (i === cardSel && m.myTurn ? ' sel' : ''), i)).join('')}</div>
      <div class="cardBtns"><button id="drawB" ${m.canDraw ? '' : 'disabled'}>${m.pending ? `Take +${m.pending}` : '🂠 Draw'}</button><button id="passB" ${m.canPass ? '' : 'disabled'}>Pass ➜</button></div>
      <div class="legend">Match suit or rank · 8 wild · 2 = +2 · J skip · Q reverse · A = CHAOS<br>PC: ← → select · Enter play · D draw · P pass</div>
      ${m.needSuit ? `<div class="suitPick"><h2>Pick the new suit</h2><div class="row">${['♠', '♥', '♦', '♣'].map(s => `<button data-s="${s}" class="${'♥♦'.includes(s) ? 'red' : ''}">${s}</button>`).join('')}</div></div>` : ''}</div>`;
    stage.querySelectorAll('.hand .card').forEach(b => b.onclick = () => { const i = +b.dataset.i; if (!m.myTurn) return toast('Not your turn yet'); if (!m.playable[i]) { b.classList.remove('wiggle'); void b.offsetWidth; b.classList.add('wiggle'); navigator.vibrate?.(40); return; } sendMsg({ t: 'act', a: 'play', i }); });
    const d = $('#drawB'), p = $('#passB'); if (d) d.onclick = () => sendMsg({ t: 'act', a: 'draw' }); if (p) p.onclick = () => sendMsg({ t: 'act', a: 'pass' });
    stage.querySelectorAll('.suitPick button').forEach(b => b.onclick = () => sendMsg({ t: 'act', a: 'suit', s: b.dataset.s }));
    stage.querySelectorAll('.card.sel').forEach(b => b.style.outline = '4px solid #4dabff');
  }
  function cardKey(e) {
    const m = lastHand; if (!m || !m.myTurn) return;
    if (m.needSuit) { const map = { Digit1: '♠', Digit2: '♥', Digit3: '♦', Digit4: '♣', KeyS: '♠', KeyH: '♥', KeyD: '♦', KeyC: '♣' }; if (map[e.code]) sendMsg({ t: 'act', a: 'suit', s: map[e.code] }); return; }
    if (e.code === 'ArrowLeft') { cardSel = Math.max(0, cardSel - 1); renderHand(m); }
    else if (e.code === 'ArrowRight') { cardSel = Math.min(m.cards.length - 1, cardSel + 1); renderHand(m); }
    else if (e.code === 'Enter' || e.code === 'Space') { e.preventDefault(); if (m.playable[cardSel]) sendMsg({ t: 'act', a: 'play', i: cardSel }); }
    else if (e.code === 'KeyD') sendMsg({ t: 'act', a: 'draw' }); else if (e.code === 'KeyP') sendMsg({ t: 'act', a: 'pass' });
  }

  // ---------- connection ----------
  let lastSeen = 0, tries = 0, joinTries = 0, gotPing = false;
  function netBanner(on, text) {
    let b = document.getElementById('netBanner');
    if (!b) { b = document.createElement('div'); b.id = 'netBanner'; b.onclick = () => { tries = 0; hardReset(); connect(); }; document.body.appendChild(b); }
    b.innerHTML = text || ''; b.classList.toggle('show', !!on);
  }
  function onData(m) {
    if (!m || typeof m !== 'object') return;
    lastSeen = Date.now();
    if (m.t === 'ping') { gotPing = true; try { conn && conn.send({ t: 'pong', ts: m.ts }); } catch (e) {} return; }
    if (m.t === 'welcome') {
      joined = true; me = m; tries = 0; joinTries = 0; netBanner(false); if (camMode) sendMsg({ t: 'camOn' }); setTimeout(motionGate, 50); calButton(); if (cal) sendMsg({ t: 'calib', yaw: Math.round(cal.yaw) });
      $('#joinScr').classList.add('hidden'); $('#ctrlScr').classList.remove('hidden');
      $('#meDot').style.background = m.color; $('#meName').textContent = `${m.avatar} ${m.name}`;
      document.body.style.setProperty('--c', m.color); setStatus('🟢 Room ' + m.code);
      if (!layout) renderMsg({ text: 'Connected!' });
    } else if (m.t === 'layout') render(m.layout);
    else if (m.t === 'hand') { const was = lastHand && lastHand.myTurn; renderHand(m); if (m.myTurn && !was) navigator.vibrate?.([60, 40, 60]); }
    else if (m.t === 'focus') { vipFocus = m; const f = document.getElementById('focusName'); if (f) f.textContent = `${m.emoji} ${m.name}`; }
    else if (m.t === 'buzz') navigator.vibrate?.(m.ms || 80);
    else if (m.t === 'toast') toast(m.text);
    else if (m.t === 'full') joinErr('That room is full (8 players).');
  }
  // Retry with backoff. Every 3rd failure throws away the whole peer and starts fresh (fixes "stuck" states).
  function scheduleRetry() {
    if (retryT) return;
    tries++;
    const wait = Math.min(1000 * tries, 5000);
    setStatus('🟠 Reconnecting…');
    netBanner(true, tries > 3 ? '📡 Still trying to reach the TV… <b>tap to retry now</b><br><small>Is the TV page still open?</small>' : '📡 Reconnecting to the TV…');
    retryT = setTimeout(() => { retryT = null; if (tries % 3 === 0) hardReset(); connect(); }, wait);
  }
  function hardReset() { const c = conn; conn = null; try { c && c.close(); } catch (e) {} if (peer) { try { peer.destroy(); } catch (e) {} } peer = null; }
  function failJoin(msg) {
    // first join: retry quietly a few times before showing an error (the TV may be reconnecting itself)
    if (++joinTries < 3) { $('#joinBtn').textContent = 'Still trying… (' + (joinTries + 1) + '/3)'; clearTimeout(retryT); retryT = setTimeout(() => { retryT = null; hardReset(); connect(); }, 800); return; }
    joinTries = 0; hardReset(); joinErr(msg);
  }
  const lost = msg => joined ? scheduleRetry() : failJoin(msg);
  function dial() {
    if (conn) { const old = conn; conn = null; try { old.close(); } catch (e) {} }
    const c = peer.connect(PREFIX + code, { reliable: true, serialization: 'json' }); conn = c;
    c.on('open', () => { if (conn !== c) return; lastSeen = Date.now(); c.send({ t: 'join', cid, name }); });
    c.on('data', d => { if (conn === c) onData(d); });
    c.on('close', () => { if (conn === c) { conn = null; lost('Lost the TV. Try again.'); } });
    c.on('error', () => { if (conn === c) { conn = null; lost('Could not reach the TV. Try again.'); } });
    setTimeout(() => { if (conn === c && !c.open) { conn = null; try { c.close(); } catch (e) {} lost('Could not reach the TV. Check the 4-letter code, and put this phone on the same Wi-Fi as the TV (not mobile data), then tap Join again.'); } }, 15000);
  }
  function connect() {
    if (conn && conn.open) return;
    if (peer && !peer.destroyed && peer.open) return dial();
    if (peer && !peer.destroyed && peer.disconnected) { try { peer.reconnect(); } catch (e) { hardReset(); return connect(); } return; }
    if (peer && !peer.destroyed) return; // still opening
    const p = peer = new Peer(PEER_OPTS);
    p.on('open', () => { if (peer === p && !(conn && conn.open)) dial(); });
    // lost the signalling server only: the link to the TV usually still works, so just reconnect quietly
    p.on('disconnected', () => { if (peer === p && !p.destroyed) setTimeout(() => { try { if (peer === p && p.disconnected && !p.destroyed) p.reconnect(); } catch (e) {} }, 1000); });
    p.on('error', e => {
      if (peer !== p) return;
      if (e.type === 'peer-unavailable') { conn = null; lost(`Room ${code} not found. Is the TV on?`); }
      else if (conn && conn.open) { /* signalling hiccup, game link is fine */ }
      else { hardReset(); lost('Network problem. Check your Wi-Fi and try again.'); }
    });
  }
  // Heartbeat: the TV pings every 2s. Silence for 7s means the link is dead even if the browser hasn't noticed.
  setInterval(() => {
    if (!joined || retryT) return;
    if (!conn || !conn.open) return scheduleRetry();
    if (gotPing && Date.now() - lastSeen > 7000) { const c = conn; conn = null; try { c.close(); } catch (e) {} scheduleRetry(); }
  }, 2000);
  addEventListener('online', () => { if (joined && !(conn && conn.open)) { tries = 0; hardReset(); connect(); } });
  async function doJoin() {
    code = $('#roomIn').value.trim().toUpperCase(); name = $('#nameIn').value.trim().slice(0, 12);
    if (code.length !== 4) return joinErr('Enter the 4-letter code shown on the TV');
    if (!name) { $('#nameIn').value = FUNNY[(Math.random() * FUNNY.length) | 0]; name = $('#nameIn').value; }
    store.set('cc_name', name);
    $('#joinBtn').disabled = true; $('#joinBtn').textContent = 'Connecting…'; $('#joinErr').textContent = '';
    await askMotion(); await ICE_READY; connect();
  }
  $('#joinBtn').onclick = doJoin;
  // returning player with the room link: join straight away
  if ($('#roomIn').value.length === 4 && store.get('cc_name')) doJoin();
  document.addEventListener('visibilitychange', () => {
    if (document.hidden || !joined) return;
    // back from a locked screen / other app: links are often dead, check right away
    if (!conn || !conn.open || (gotPing && Date.now() - lastSeen > 5000)) { clearTimeout(retryT); retryT = null; tries = 0; hardReset(); connect(); }
  });
  addEventListener('orientationchange', () => { neutral = null; });
  // iPhone: motion permission needs a tap — ask on the first tap anywhere
  // any real tap ('click' / 'touchend' count as a user gesture on iPhone) also asks, until it's granted
  if (needPerm) { const tryAsk = () => { if (permState === 'unknown') askMotion(); if (permState !== 'unknown') { document.removeEventListener('click', tryAsk, true); document.removeEventListener('touchend', tryAsk, true); } }; document.addEventListener('click', tryAsk, true); document.addEventListener('touchend', tryAsk, true); }
})();
