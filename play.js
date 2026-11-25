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
  let lastSwing = 0, swingWin = null, curRot = 0, curAcc = 0, motionSeen = false, grav = null, neutral = null, recenterSoon = false, ctrlMode = store.get('cc_ctrl') === 'motion' ? 'motion' : 'stick', tiltOn = store.get('cc_ctrl') === 'motion', flip = store.get('cc_flip') === '1' ? -1 : 1, lastShake = 0, motionOn = false;
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
    const show = needPerm && touch && permState !== 'granted' && joined && !camMode && ctrlMode === 'motion';
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
  if (!needPerm && ctrlMode === 'motion') askMotion();
  function onMotion(e) {
    const a = e.accelerationIncludingGravity; if (!a || a.x == null) return;
    if (!motionSeen) { motionSeen = true; updateTiltUI(); if (layout && layout.name === 'swing' && ctrlMode === 'motion') setTimeout(() => render(layout), 0); }
    if (ctrlMode !== 'motion' && !calRun) return; // joystick mode: the phone's movement is ignored
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
      if (swingWin) { swingWin.rot = Math.max(swingWin.rot, rot); swingWin.acc = Math.max(swingWin.acc, mag); // follow the swing to its peak
        // swing PATH: integrate the up/down acceleration → low-to-high = topspin, high-to-low = slice
        if (l && l.x != null) { const gx = a.x - l.x, gy = a.y - l.y, gz = (a.z || 0) - (l.z || 0), gm = Math.hypot(gx, gy, gz) || 1; swingWin.vUp += ((l.x * gx + l.y * gy + (l.z || 0) * gz) / gm) * (e.interval > 1 ? e.interval / 1000 : e.interval || 0.016); } } // Android: ms, iPhone: seconds
      curRot = rot; curAcc = mag;
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
    const show = tiltApplies() && ctrlMode === 'motion';
    const cb = $('#ctrlBtn'); if (cb) { cb.classList.toggle('hidden', !joined || camMode || (layout && layout.name === 'song')); cb.innerHTML = ctrlMode === 'motion' ? '📱 Motion' : '🕹️ Joystick'; cb.classList.toggle('on', ctrlMode === 'motion'); }
    $('#tiltBtn').classList.add('hidden');
    const kb = document.getElementById('calBtn'); if (kb) kb.classList.toggle('hidden', ctrlMode !== 'motion'); $('#centerBtn').classList.toggle('hidden', !show || !tiltOn); $('#flipBtn').classList.toggle('hidden', !show || !tiltOn);
    $('#tiltBtn').textContent = tiltOn ? 'Tilt: ON' : 'Tilt: off'; $('#tiltBtn').classList.toggle('on', tiltOn);
    const tb = document.getElementById('tiltBadge'); if (tb) tb.classList.toggle('hidden', !(show && tiltOn));
  }
  function setCtrl(mode) {
    ctrlMode = mode; store.set('cc_ctrl', mode); tiltOn = mode === 'motion';
    if (mode === 'motion') { askMotion(); neutral = grav ? { ...grav } : null; toast('📱 Motion on — tilt to steer, swing to hit'); }
    else { if (calRun) endCalib(false); const mg = document.getElementById('motionGate'); if (mg) mg.remove(); toast('🕹️ Joystick on'); }
    sendMsg({ t: 'ctrl', mode }); if (layout) render(layout); else updateTiltUI();
  }
  $('#ctrlBtn').onclick = () => setCtrl(ctrlMode === 'motion' ? 'stick' : 'motion');
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
  function maybeCalib() { if (camMode || ctrlMode !== 'motion') return; if (joined && !calDone && !calRun && motionSeen && (!needPerm || permState === 'granted')) startCalib(); }
  setInterval(maybeCalib, 400);

  // ---------- input state ----------
  const keys = { left: false, right: false, up: false, down: false, a: false, b: false }, btn = { a: false, b: false }, stick = { active: false, x: 0, y: 0 };
  const KEYMAP = { ArrowLeft: 'left', KeyA: 'left', ArrowRight: 'right', KeyD: 'right', ArrowUp: 'up', KeyW: 'up', ArrowDown: 'down', KeyS: 'down', Space: 'a', KeyJ: 'a', Enter: 'a', ShiftLeft: 'b', ShiftRight: 'b', KeyK: 'b', KeyE: 'b' };
  function readInput() {
    if (layout && layout.name === 'touch') { const f = TM.touching && TM.fing; return { x: f ? TM.fing.x * 2 - 1 : 0, y: f ? TM.fing.y * 2 - 1 : 0, a: keys.a || btn.a, b: false, touch: !!f }; }
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
    if (!conn || !conn.open || !layout || !['pad', 'button', 'swing', 'touch'].includes(layout.name)) return;
    const s = readInput(), fine = layout.name === 'touch', eps = fine ? 0.004 : 0.03;
    if (!force && Math.abs(s.x - lastSent.x) < eps && Math.abs(s.y - lastSent.y) < eps && s.a === lastSent.a && s.b === lastSent.b && s.touch === lastSent.touch) return;
    s.x = +s.x.toFixed(fine ? 3 : 2); s.y = +s.y.toFixed(fine ? 3 : 2);
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
  setInterval(() => flush(false), 25); setInterval(() => flush(true), 1000);
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

  // ---------- physical gestures (instead of buttons) ----------
  // flick a card up to throw it onto the table (a plain tap still works)
  function flickable(el, onPlay) {
    let sx = 0, sy = 0, st = 0, id = null, done = false;
    el.style.touchAction = 'none';
    el.addEventListener('pointerdown', e => { if (el.disabled) return; id = e.pointerId; sx = e.clientX; sy = e.clientY; st = performance.now(); try { el.setPointerCapture(id); } catch (_) {} el.style.transition = 'none'; el.style.zIndex = 5; });
    el.addEventListener('pointermove', e => { if (e.pointerId !== id) return; const dx = e.clientX - sx, dy = Math.min(20, e.clientY - sy); el.style.transform = `translate(${dx * 0.6}px,${dy}px) rotate(${dx * 0.08}deg) scale(${1 + Math.min(0.15, -dy / 600)})`; });
    const up = e => {
      if (e.pointerId !== id) return; id = null;
      const dx = e.clientX - sx, dy = e.clientY - sy, v = dy / Math.max(1, performance.now() - st), tap = Math.hypot(dx, dy) < 10;
      if (!done && (dy < -55 || v < -0.6 || tap)) {
        done = true; el.style.transition = 'transform .28s cubic-bezier(.2,.8,.3,1), opacity .28s'; el.style.transform = `translate(${dx}px,-${innerHeight * 0.7}px) rotate(${dx * 0.2 + 18}deg) scale(1.2)`; el.style.opacity = '0';
        navigator.vibrate?.([12, 30, 18]); onPlay();
      } else { el.style.transition = 'transform .35s cubic-bezier(.3,1.6,.5,1)'; el.style.transform = ''; el.style.zIndex = ''; }
    };
    el.addEventListener('pointerup', up); el.addEventListener('pointercancel', up);
    el.onclick = null;
  }
  // a big die: flick it upward or shake the phone to roll
  let shakeRoll = null, lastRollShake = 0;
  addEventListener('devicemotion', e => {
    if (!shakeRoll) return; const a = e.acceleration || {}; const mag = a.x != null ? Math.hypot(a.x, a.y, a.z) : 0;
    if (mag > 15 && performance.now() - lastRollShake > 900) { lastRollShake = performance.now(); shakeRoll(); }
  });
  function diceRoller(holder, onRoll, label = 'Flick the dice up — or shake your phone!') {
    holder.innerHTML = `<div class="roller"><div class="rdice"><span class="rdie">${PIPS[5]}</span><span class="rdie b">${PIPS[3]}</span></div><div class="rhint">🎲 ${esc(label)}</div></div>`;
    const box = holder.querySelector('.roller'), dice = holder.querySelector('.rdice');
    let sy = 0, st = 0, id = null, rolled = false;
    const roll = () => { if (rolled) return; rolled = true; shakeRoll = null; box.classList.add('go'); navigator.vibrate?.([20, 40, 20, 40, 30]); onRoll(); };
    shakeRoll = roll;
    box.addEventListener('pointerdown', e => { id = e.pointerId; sy = e.clientY; st = performance.now(); try { box.setPointerCapture(id); } catch (_) {} });
    box.addEventListener('pointermove', e => { if (e.pointerId !== id) return; const dy = Math.min(0, e.clientY - sy); dice.style.transform = `translateY(${dy * 0.7}px) rotate(${dy * 0.4}deg)`; });
    const up = e => { if (e.pointerId !== id) return; id = null; const dy = e.clientY - sy, v = dy / Math.max(1, performance.now() - st); dice.style.transform = ''; if (dy < -40 || v < -0.5 || Math.abs(dy) < 8) roll(); };
    box.addEventListener('pointerup', up); box.addEventListener('pointercancel', up);
  }
  // slash across a wire to cut it
  function renderWires(L) {
    const n = L.wires.length;
    stage.innerHTML = `<div class="wiresC"><div class="prompt">${esc(L.prompt || '')}</div><div class="bombBox" id="bb"></div><div class="rhint">✂️ Swipe your finger across a wire to CUT it</div></div>`;
    const bb = $('#bb'), H = Math.max(200, 300 * bb.clientHeight / Math.max(1, bb.clientWidth)), gap = (H - 30) / n;
    const half = (y, k, sw, col, hi) => k === 0
      ? `<path d="M22 ${y} C 61 ${y + 12}, 105 ${y + 6}, 150 ${y}" stroke="${col}" stroke-width="${sw}" fill="none" stroke-linecap="round"${hi ? ' transform="translate(0,-3)"' : ''}/>`
      : `<path d="M150 ${y} C 195 ${y - 6}, 239 ${y - 12}, 278 ${y}" stroke="${col}" stroke-width="${sw}" fill="none" stroke-linecap="round"${hi ? ' transform="translate(0,-3)"' : ''}/>`;
    const layer = (y, k, c) => half(y, k, 24, '#111') + half(y, k, 16, c) + half(y, k, 4, 'rgba(255,255,255,.5)', true);
    bb.innerHTML = `<svg id="wsvg" viewBox="0 0 300 ${H}">${L.wires.map((w, i) => { const y = 15 + gap * (i + 0.5), c = esc(w.color); return `<g class="wire" data-i="${i}">
        <rect x="6" y="${y - 12}" width="16" height="24" rx="5" fill="#adb5bd"/><rect x="278" y="${y - 12}" width="16" height="24" rx="5" fill="#adb5bd"/>
        <g class="whole">${half(y, 0, 24, '#111')}${half(y, 1, 24, '#111')}${half(y, 0, 16, c)}${half(y, 1, 16, c)}${half(y, 0, 4, 'rgba(255,255,255,.5)', true)}${half(y, 1, 4, 'rgba(255,255,255,.5)', true)}</g>
        <g class="wl">${layer(y, 0, c)}</g><g class="wr">${layer(y, 1, c)}</g>
        <text x="4" y="${y - 18}" fill="#fff" font-size="16" font-weight="800">${i + 1}</text></g>`; }).join('')}
      <path id="slash" d="" stroke="#fff" stroke-width="5" fill="none" stroke-linecap="round" opacity=".85"/></svg>`;
    const svg = $('#wsvg'), slash = $('#slash'); let pts = [], cut = false, id = null;
    const P2 = e => { const pt = svg.createSVGPoint(); pt.x = e.clientX; pt.y = e.clientY; const q = pt.matrixTransform(svg.getScreenCTM().inverse()); return [q.x, q.y]; };
    svg.style.touchAction = 'none';
    svg.addEventListener('pointerdown', e => { if (cut) return; id = e.pointerId; try { svg.setPointerCapture(id); } catch (_) {} pts = [P2(e)]; });
    svg.addEventListener('pointermove', e => {
      if (e.pointerId !== id || cut) return; const p = P2(e), q = pts[pts.length - 1]; pts.push(p); if (pts.length > 14) pts.shift();
      slash.setAttribute('d', 'M' + pts.map(x => x.join(' ')).join(' L'));
      if (p[0] < 30 || p[0] > 270) return;
      for (let i = 0; i < n; i++) { const y = 15 + gap * (i + 0.5); if ((q[1] - y) * (p[1] - y) <= 0 && Math.abs(p[1] - q[1]) > 1) {
        cut = true; svg.querySelector(`.wire[data-i="${i}"]`).classList.add('cut'); navigator.vibrate?.([30, 20, 60]);
        sendMsg({ t: 'act', a: 'choice', i }); setTimeout(() => slash.setAttribute('d', ''), 220); return; } }
    });
    const up = e => { if (e.pointerId === id) { id = null; if (!cut) slash.setAttribute('d', ''); } };
    svg.addEventListener('pointerup', up); svg.addEventListener('pointercancel', up);
  }
  // curling: pull the stone back like a slingshot and let go; then rub the ice to sweep
  function renderCurl(L) {
    if (L.sweep) {
      stage.innerHTML = `<div class="curlC sweep" id="sw"><div class="broom">🧹</div><h2>RUB THE ICE!</h2><div class="rhint">Scrub left-right as fast as you can to make it slide further</div></div>`;
      const z = $('#sw'), br = z.querySelector('.broom'); let lx = null, dir = 0, last = 0; z.style.touchAction = 'none';
      z.addEventListener('pointermove', e => { if (!e.buttons && e.pointerType === 'mouse') return; br.style.left = e.clientX + 'px'; br.style.top = e.clientY + 'px';
        if (lx != null) { const d = Math.sign(e.clientX - lx); if (d && d !== dir && Math.abs(e.clientX - lx) > 2) { dir = d; const now = performance.now(); if (now - last > 70) { last = now; sendMsg({ t: 'act', a: 'sweep' }); navigator.vibrate?.(8); } } }
        lx = e.clientX; });
      z.addEventListener('pointerup', () => (lx = null));
      return;
    }
    stage.innerHTML = `<div class="curlC"><div class="sheet" id="sh"><div class="house"></div><svg class="aimline" id="al"><line id="aimL" x1="0" y1="0" x2="0" y2="0" stroke="#ffd43b" stroke-width="5" stroke-dasharray="10 8" stroke-linecap="round"/></svg><div class="stone" id="st"></div></div>
      <div class="rhint">🥌 Pull the stone DOWN, aim, and let go! Further back = more power</div></div>`;
    const sh = $('#sh'), stn = $('#st'), aimL = $('#aimL'); let id = null, ox = 0, oy = 0, thrown = false; sh.style.touchAction = 'none';
    const home = () => { const r = sh.getBoundingClientRect(); return [r.width / 2, r.height - 202]; };
    const vec = e => { const r = sh.getBoundingClientRect(), [hx, hy] = home(); let dx = e.clientX - r.left - hx, dy = e.clientY - r.top - hy; const l = Math.hypot(dx, dy), m = 140; if (l > m) { dx *= m / l; dy *= m / l; } return [dx, Math.max(0, dy)]; };
    sh.addEventListener('pointerdown', e => { if (thrown) return; id = e.pointerId; try { sh.setPointerCapture(id); } catch (_) {} });
    sh.addEventListener('pointermove', e => { if (e.pointerId !== id) return; const [dx, dy] = vec(e), [hx, hy] = home(); stn.style.transform = `translate(${dx}px,${dy}px)`; aimL.setAttribute('x1', hx); aimL.setAttribute('y1', hy); aimL.setAttribute('x2', hx - dx * 3); aimL.setAttribute('y2', hy - dy * 3); ox = dx; oy = dy; });
    const up = e => {
      if (e.pointerId !== id) return; id = null;
      if (oy < 20) { stn.style.transform = ''; aimL.setAttribute('x2', aimL.getAttribute('x1')); return; }
      thrown = true; const power = Math.min(1, oy / 140), aim = Math.max(-1, Math.min(1, -ox / Math.max(30, oy)));
      stn.style.transition = 'transform .45s cubic-bezier(.2,.8,.3,1)'; stn.style.transform = `translate(${-ox * 2}px,-${innerHeight}px)`;
      navigator.vibrate?.(40); sendMsg({ t: 'act', a: 'curl', i: Math.round(aim * 1000), p: +power.toFixed(3) });
    };
    sh.addEventListener('pointerup', up); sh.addEventListener('pointercancel', up);
  }


  // ---------- live touch map: the TV streams the play area, your finger moves you ----------
  // mode "drag": your piece sits under your finger (air hockey mallet). mode "go": tap/hold where you want to run.
  const TM = { on: false, L: null, objs: [], info: '', fing: null, touching: false, raf: 0, cw: 0, ch: 0 };
  function onSnap(m) {
    if (!TM.on) return;
    const n = m.o.map(a => ({ x: a[0], y: a[1], r: a[2], c: a[3], k: a[4], r2: a[5] }));
    if (n.length === TM.objs.length && n.every((o, i) => o.k === TM.objs[i].k)) n.forEach((o, i) => { o.dx = TM.objs[i].dx; o.dy = TM.objs[i].dy; });
    n.forEach(o => { if (o.dx === undefined) { o.dx = o.x; o.dy = o.y; } });
    TM.objs = n;
    if (m.i !== undefined && m.i !== TM.info) { TM.info = m.i; const e = document.getElementById('tInfo'); if (e) e.textContent = m.i; }
    // "go" mode: arrived at the spot you tapped → stop
    if (TM.L.mode === 'go' && TM.fing && !TM.down) { const me = n.find(o => o.k === 1); if (me && Math.hypot(me.x - TM.fing.x, (me.y - TM.fing.y) / TM.L.aspect) < 0.012) { TM.touching = false; TM.fing = null; flush(true); } }
  }
  function renderTouch(L) {
    TM.on = true; TM.L = L; TM.objs = []; TM.fing = null; TM.touching = false; TM.down = false; TM.info = '';
    stage.innerHTML = `<div class="touchC"><div class="touchInfo" id="tInfo"></div><div class="touchWrap" id="tw"><canvas id="tcv"></canvas></div>
      <div class="touchBottom">${L.btn ? `<button class="pbtn pa touchBtn" data-b="a">${esc(L.btn)}</button>` : ''}<div class="rhint">${esc(L.hint || '')}</div></div></div>`;
    stage.querySelectorAll('.pbtn').forEach(b => bindHold(b, b.dataset.b));
    const tw = $('#tw'), cv = $('#tcv'), g = cv.getContext('2d');
    const fit = () => {
      const W = tw.clientWidth, H = tw.clientHeight; if (!W || !H) return;
      TM.rot = L.aspect > 1.2 && H > W * 1.1; const asp = TM.rot ? 1 / L.aspect : L.aspect;
      let cw = W, ch = W / asp; if (ch > H) { ch = H; cw = H * asp; }
      const d = Math.min(2, devicePixelRatio || 1); cv.width = Math.round(cw * d); cv.height = Math.round(ch * d); cv.style.width = cw + 'px'; cv.style.height = ch + 'px';
      TM.cw = cw; TM.ch = ch; TM.d = d;
    };
    fit(); addEventListener('resize', fit);
    const at = e => { const r = cv.getBoundingClientRect(), fx = Math.max(0, Math.min(1, (e.clientX - r.left) / r.width)), fy = Math.max(0, Math.min(1, (e.clientY - r.top) / r.height)); return TM.rot ? { x: 1 - fy, y: fx } : { x: fx, y: fy }; };
    let pid = null;
    cv.style.touchAction = 'none';
    cv.addEventListener('pointerdown', e => { e.preventDefault(); pid = e.pointerId; try { cv.setPointerCapture(pid); } catch (_) {} TM.fing = at(e); TM.touching = true; TM.down = true; navigator.vibrate?.(8); flush(true); });
    cv.addEventListener('pointermove', e => { if (e.pointerId !== pid) return; TM.fing = at(e); flush(); });
    const up = e => { if (e.pointerId !== pid) return; pid = null; TM.down = false; if (L.mode === 'drag') { TM.touching = false; TM.fing = null; } flush(true); };
    cv.addEventListener('pointerup', up); cv.addEventListener('pointercancel', up);
    cancelAnimationFrame(TM.raf);
    const frame = () => {
      if (!TM.on || !document.body.contains(cv)) return;
      if (!TM.cw) fit();
      drawTouch(g, L); TM.raf = requestAnimationFrame(frame);
    };
    TM.raf = requestAnimationFrame(frame);
  }
  function rr(g, x, y, w, h, r) { g.beginPath(); g.moveTo(x + r, y); g.arcTo(x + w, y, x + w, y + h, r); g.arcTo(x + w, y + h, x, y + h, r); g.arcTo(x, y + h, x, y, r); g.arcTo(x, y, x + w, y, r); g.closePath(); }
  function drawTouch(g, L) {
    const d = TM.d || 1, t = performance.now() / 1000, rot = !!TM.rot;
    const W = (rot ? TM.ch : TM.cw) * d, H = (rot ? TM.cw : TM.ch) * d;  // logical (TV-oriented) size
    g.setTransform(1, 0, 0, 1, 0, 0); g.clearRect(0, 0, g.canvas.width, g.canvas.height);
    if (rot) g.setTransform(0, -1, 1, 0, 0, W);  // turn the phone sideways and it lines up with the TV
    // ---- the surface ----
    const th = L.theme, R = 18 * d;
    rr(g, 0, 0, W, H, R); g.save(); g.clip();
    if (th === 'hockey' || th === 'rink') {
      const gr = g.createLinearGradient(0, 0, 0, H); gr.addColorStop(0, '#f4f9ff'); gr.addColorStop(1, '#dcebff'); g.fillStyle = gr; g.fillRect(0, 0, W, H);
      g.strokeStyle = '#ff4d5e'; g.lineWidth = 4 * d; g.beginPath();
      if (th === 'hockey') { g.moveTo(0, H / 2); g.lineTo(W, H / 2); } else { g.moveTo(W / 2, 0); g.lineTo(W / 2, H); } g.stroke();
      g.strokeStyle = '#4d9bff'; g.lineWidth = 3 * d; g.beginPath(); g.arc(W / 2, H / 2, Math.min(W, H) * 0.16, 0, 7); g.stroke();
      g.fillStyle = '#16203f';
      if (th === 'hockey') { const gw = W * 0.4167; g.fillRect((W - gw) / 2, 0, gw, 9 * d); g.fillRect((W - gw) / 2, H - 9 * d, gw, 9 * d);
        g.globalAlpha = 0.08; g.fillStyle = '#2f7bff'; g.fillRect(0, H / 2, W, H / 2); g.globalAlpha = 1; }
      else { const gh = H * 0.47; g.fillRect(0, (H - gh) / 2, 9 * d, gh); g.fillRect(W - 9 * d, (H - gh) / 2, 9 * d, gh); }
    } else if (th === 'ice') {
      g.fillStyle = '#cfe7ff'; g.fillRect(0, 0, W, H); g.strokeStyle = 'rgba(255,255,255,.8)'; g.lineWidth = 2 * d;
      for (let i = 1; i < 10; i++) { g.beginPath(); g.moveTo(W * i / 10, 0); g.lineTo(W * i / 10, H); g.stroke(); }
      for (let i = 1; i < 6; i++) { g.beginPath(); g.moveTo(0, H * i / 6); g.lineTo(W, H * i / 6); g.stroke(); }
    } else {
      const gr = g.createLinearGradient(0, 0, 0, H); gr.addColorStop(0, '#6cc45f'); gr.addColorStop(1, '#4fa548'); g.fillStyle = gr; g.fillRect(0, 0, W, H);
      g.fillStyle = 'rgba(255,255,255,.06)'; for (let i = 0; i < 12; i += 2) g.fillRect(W * i / 12, 0, W / 12, H);
    }
    // ---- objects (smoothed) ----
    const objs = TM.objs; objs.forEach(o => { o.dx += (o.x - o.dx) * 0.45; o.dy += (o.y - o.dy) * 0.45; });
    const order = [7, 6, 5, 4, 3, 2, 0, 1];
    const meDrag = L.mode === 'drag' && TM.touching && TM.fing;
    for (const k of order) for (const o of objs) {
      if (o.k !== k) continue;
      let x = o.dx * W, y = o.dy * H; const r = o.r * W;
      if (k === 6 || k === 7) { const hw = o.r * W, hh = o.r2 * H; g.fillStyle = k === 6 ? `rgba(255,59,59,${0.55 + 0.3 * Math.sin(t * 10)})` : '#0b1b3a'; rr(g, x - hw + d, y - hh + d, hw * 2 - 2 * d, hh * 2 - 2 * d, 4 * d); g.fill(); continue; }
      if (k === 5) { g.fillStyle = 'rgba(255,59,59,.28)'; g.beginPath(); g.arc(x, y, r, 0, 7); g.fill(); g.strokeStyle = '#ff3b3b'; g.lineWidth = 3 * d; g.setLineDash([8 * d, 6 * d]); g.lineDashOffset = -t * 30 * d; g.stroke(); g.setLineDash([]); continue; }
      if (k === 4) { g.shadowColor = o.c; g.shadowBlur = 12 * d; g.fillStyle = o.c; g.beginPath(); g.arc(x, y, Math.max(r, 6 * d), 0, 7); g.fill(); g.shadowBlur = 0; g.strokeStyle = '#fff'; g.lineWidth = 2 * d; g.stroke(); continue; }
      if (k === 1 && meDrag) {  // your mallet sits right under your finger, kept on your half
        const rn = r / H; x = TM.fing.x * W; y = Math.max(0.5 + rn, Math.min(1 - rn, TM.fing.y)) * H; x = Math.max(r, Math.min(W - r, x));
      }
      const rad = Math.max(r, (k === 0 ? 7 : 10) * d);
      g.fillStyle = 'rgba(0,0,0,.22)'; g.beginPath(); g.arc(x + 2 * d, y + 4 * d, rad, 0, 7); g.fill();
      const gr = g.createRadialGradient(x - rad * 0.35, y - rad * 0.4, rad * 0.1, x, y, rad); gr.addColorStop(0, '#ffffff'); gr.addColorStop(0.25, o.c); gr.addColorStop(1, o.c);
      g.fillStyle = k === 0 ? '#1c1f26' : gr; g.beginPath(); g.arc(x, y, rad, 0, 7); g.fill();
      if (k === 1) { g.strokeStyle = '#fff'; g.lineWidth = 4 * d; g.stroke(); g.strokeStyle = 'rgba(255,255,255,.5)'; g.lineWidth = 2 * d; g.beginPath(); g.arc(x, y, rad + 7 * d + 3 * d * Math.sin(t * 6), 0, 7); g.stroke();
        g.fillStyle = '#fff'; g.font = `800 ${12 * d}px Nunito, sans-serif`; g.textAlign = 'center'; g.fillText('YOU', x, y - rad - 10 * d); }
      else if (k === 2) { g.strokeStyle = 'rgba(255,255,255,.85)'; g.lineWidth = 2 * d; g.stroke(); }
      else if (k === 0) { g.strokeStyle = '#555c6b'; g.lineWidth = 2 * d; g.stroke(); }
      else { g.strokeStyle = 'rgba(0,0,0,.35)'; g.lineWidth = 2 * d; g.stroke(); }
    }
    // "go" mode: where you are heading
    if (L.mode === 'go' && TM.fing) {
      const me = objs.find(o => o.k === 1), fx = TM.fing.x * W, fy = TM.fing.y * H;
      if (me) { g.strokeStyle = 'rgba(255,255,255,.75)'; g.lineWidth = 3 * d; g.setLineDash([6 * d, 6 * d]); g.beginPath(); g.moveTo(me.dx * W, me.dy * H); g.lineTo(fx, fy); g.stroke(); g.setLineDash([]); }
      g.strokeStyle = '#fff'; g.lineWidth = 3 * d; g.beginPath(); g.arc(fx, fy, (14 + 4 * Math.sin(t * 8)) * d, 0, 7); g.stroke();
      g.beginPath(); g.arc(fx, fy, 4 * d, 0, 7); g.fillStyle = '#fff'; g.fill();
    }
    g.restore();
    rr(g, 1 * d, 1 * d, W - 2 * d, H - 2 * d, R); g.strokeStyle = 'rgba(255,255,255,.9)'; g.lineWidth = 3 * d; g.stroke();
  }

  // four in a row: the real board on your phone; drag your disc over a column and let go to drop it
  function renderFour(L) {
    const C = ['', '#ff3b5c', '#ffcc1a'], mine = C[L.side + 1], win = new Set((L.win || []).map(w => w[0] + ',' + w[1]));
    stage.innerHTML = `<div class="fourC"><div class="fourTxt ${L.my ? 'my' : ''}">${esc(L.text || '')}</div>
      <div class="fourRail" id="rail">${L.my ? `<div class="fdisc hold" id="fd" style="--dc:${mine}"></div>` : ''}</div>
      <div class="fourBoard" id="fb">${L.board.map((row, r) => row.map((v, c) => `<div class="fcell" data-c="${c}"><i class="${v ? 'on' : ''} ${win.has(c + ',' + r) ? 'w' : ''}" style="--dc:${C[v] || 'transparent'}"></i></div>`).join('')).join('')}</div>
      <div class="rhint">${L.my ? '👆 Drag the disc left / right above the board, let go to drop' : 'Watch the board — your turn is coming'}</div></div>`;
    if (!L.my) return;
    const fb = $('#fb'), rail = $('#rail'), fd = $('#fd'); let col = 3, id = null;
    const colOf = x => { const r = fb.getBoundingClientRect(); return Math.max(0, Math.min(6, Math.floor((x - r.left) / (r.width / 7)))); };
    const place = c => { const r = fb.getBoundingClientRect(), rr = rail.getBoundingClientRect(); fd.style.left = (r.left - rr.left + (c + 0.5) * r.width / 7) + 'px'; fb.querySelectorAll('.fcell').forEach(e => e.classList.toggle('hot', +e.dataset.c === c)); };
    const full = c => L.board[0][c] !== 0;
    place(col);
    const z = stage.querySelector('.fourC'); z.style.touchAction = 'none';
    z.addEventListener('pointerdown', e => { id = e.pointerId; try { z.setPointerCapture(id); } catch (_) {} const c = colOf(e.clientX); if (c !== col) { col = c; place(c); navigator.vibrate?.(6); } });
    z.addEventListener('pointermove', e => { if (e.pointerId !== id) return; const c = colOf(e.clientX); if (c !== col) { col = c; place(c); navigator.vibrate?.(6); } });
    const up = e => {
      if (e.pointerId !== id) return; id = null;
      if (full(col)) { fd.classList.remove('wiggle'); void fd.offsetWidth; fd.classList.add('wiggle'); navigator.vibrate?.(40); return; }
      let row = 5; while (row > 0 && L.board[row][col] !== 0) row--;
      const cell = fb.querySelector(`.fcell:nth-child(${row * 7 + col + 1})`), cr = cell.getBoundingClientRect(), dr = fd.getBoundingClientRect();
      fd.style.transition = 'transform .38s cubic-bezier(.5,0,.75,0)'; fd.style.transform = `translate(-50%, ${cr.top - dr.top + 2}px)`;
      navigator.vibrate?.([10, 30, 25]); setTimeout(() => sendMsg({ t: 'act', a: 'choice', i: col }), 300);
      z.style.pointerEvents = 'none';
    };
    z.addEventListener('pointerup', up); z.addEventListener('pointercancel', up);
  }

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
    layout = L; TM.on = false; cancelAnimationFrame(TM.raf); resetInput(); pumps = 0; shakeRoll = null;
    if ((L.name === 'pad' && L.stick) || L.name === 'swing') recenterSoon = true;
    if (L.name === 'swing') orRecenter = true;
    if (needPerm) setTimeout(motionGate, 0);
    if (L.name !== 'cards') lastHand = null;
    const R = { panel: L => (lastPanel ? renderPanel(lastPanel) : renderMsg({ emoji: '🃏', text: 'Shuffling…' })), swing: renderSwing, lobby: renderLobby, pad: renderPad, button: renderButton, shake: renderShake, intro: renderIntro, choice: renderChoice, draw: renderDraw, bingo: renderBingo, dice: renderDice, song: renderSong, wires: renderWires, curl: renderCurl, four: renderFour, touch: renderTouch };
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
      else stage.querySelectorAll('.choiceGrid button').forEach(x => { x.disabled = true; if (x === b) { x.disabled = false; x.classList.add('picked'); } });
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
      ${L.done || !L.left ? '' : `<div class="rollHold" id="rollH"></div>`}<div class="diceBtns"><button id="doneB" class="done" ${L.done ? 'disabled' : ''}>${L.done ? '✓ Locked' : '✋ Keep these'}</button></div></div>`;
    stage.querySelectorAll('.die').forEach(b => b.onclick = () => { sendMsg({ t: 'act', a: 'hold', i: +b.dataset.i }); b.classList.toggle('held'); });
    if ($('#rollH')) diceRoller($('#rollH'), () => sendMsg({ t: 'act', a: 'roll' }), `Flick up or shake to re-roll (${L.left} left) — tap dice to hold`);
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
    if (!noStick && ctrlMode !== 'motion') return renderJoy(L);
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
  // ---------- joystick pad: thumbstick on the left, big buttons on the right (works held sideways) ----------
  function joyHtml(xOnly, hint) { return `<div class="joyZone ${xOnly ? 'xonly' : ''}" id="jz"><div class="jsBase" id="jsB"><div class="jsKnob" id="jsK"></div>${xOnly ? '<i class="jsAr l">◀</i><i class="jsAr r">▶</i>' : ''}</div><div class="jzHint">${esc(hint || '')}<br><small>Put your thumb anywhere here</small></div></div>`; }
  function bindJoy(xOnly) {
    const z = $('#jz'), base = $('#jsB'), knob = $('#jsK'); if (!z) return; let pid = null, cx = 0, cy = 0;
    const R = () => Math.max(44, Math.min(80, z.clientHeight * 0.2));
    let atEdge = false;
    const move = e => {
      const r = R(); let dx = e.clientX - cx, dy = xOnly ? 0 : e.clientY - cy; const l = Math.hypot(dx, dy);
      // the base follows your thumb if you drag past the edge, so you never "run out" of stick
      if (l > r * 1.35) { const k = (l - r * 1.35) / l; cx += dx * k; cy += xOnly ? 0 : dy * k; const zr = z.getBoundingClientRect(); base.style.left = (cx - zr.left) + 'px'; base.style.top = (cy - zr.top) + 'px'; dx -= dx * k; dy -= dy * k; }
      const l2 = Math.hypot(dx, dy); if (l2 > r) { dx *= r / l2; dy *= r / l2; }
      knob.style.transform = `translate(${dx}px,${dy}px)`;
      // radial dead zone + gentle response curve: precise near the middle, full speed at the edge
      const m = Math.min(1, Math.hypot(dx, dy) / r), dz = 0.1, o = m < dz ? 0 : ((m - dz) / (1 - dz)) * 0.7 + Math.pow((m - dz) / (1 - dz), 2) * 0.3;
      const nx = m > 0 ? dx / (m * r) : 0, ny = m > 0 ? dy / (m * r) : 0; stick.x = nx * o; stick.y = ny * o;
      if (m > 0.97 && !atEdge) { atEdge = true; navigator.vibrate?.(6); } else if (m < 0.9) atEdge = false;
      flush();
    };
    z.addEventListener('pointerdown', e => { e.preventDefault(); if (pid != null) return; pid = e.pointerId; try { z.setPointerCapture(pid); } catch (_) {} const zr = z.getBoundingClientRect(); cx = e.clientX; cy = xOnly ? zr.top + zr.height / 2 : e.clientY; base.style.left = (cx - zr.left) + 'px'; base.style.top = (cy - zr.top) + 'px'; z.classList.add('on'); stick.active = true; move(e); navigator.vibrate?.(8); });
    z.addEventListener('pointermove', e => { if (e.pointerId === pid) move(e); });
    const end = e => { if (e.pointerId !== pid) return; pid = null; stick.active = false; stick.x = stick.y = 0; knob.style.transform = ''; base.style.left = base.style.top = ''; z.classList.remove('on'); flush(); };
    z.addEventListener('pointerup', end); z.addEventListener('pointercancel', end);
  }
  function renderJoy(L) {
    const xOnly = L.stick === 'x';
    stage.innerHTML = `<div class="joyPad ${L.a || L.b ? '' : 'nobtn'}">${joyHtml(xOnly, L.hint)}<div class="joyBtns">${L.b ? `<button class="pbtn pb" data-b="b">${esc(L.b)}</button>` : ''}${L.a ? `<button class="pbtn pa" data-b="a">${esc(L.a)}</button>` : ''}</div></div>`;
    stage.querySelectorAll('.pbtn').forEach(b => bindHold(b, b.dataset.b));
    bindJoy(xOnly);
  }
  // ---------- swing (motion sports) ----------
  const SPORT = { tennis: ['🎾', 'a tennis racket'], bowling: ['🎳', 'a bowling arm'], bat: ['⚾', 'a baseball bat'] };
  // racket-head speed from the phone: spin rate (rad/s) × ~1.1 m (arm + racket), or the jolt if there is no gyro.
  // A tennis ball leaves the strings ~1.4× faster than the racket head.
  function shotKmh(rotDeg, acc) { const racket = rotDeg > 0 ? (rotDeg * Math.PI / 180) * 1.1 : acc * 0.3; return Math.round(clamp(racket * 3.6 * 1.4, 20, 240)); }
  function swing(power, how) {
    if (!layout || layout.name !== 'swing') return;
    lastSwing = performance.now();
    if (how === 'motion') {
      const t0 = performance.now(), hasOr = orQ && orNeu, pre = hasOr ? yawAt(220) : 0, y0 = hasOr ? yawNow() : 0, q0 = hasOr ? relQ() : undefined;
      swingWin = { rot: curRot, acc: curAcc, vUp: 0 };
      setTimeout(() => {
        const w = swingWin; swingWin = null;
        const kmh = shotKmh(w.rot, w.acc), pw = clamp((kmh - 40) / 150 + 0.35, 0.3, 1), spin = +clamp(w.vUp / 1.4).toFixed(2);
        const aim = hasOr ? clamp(clamp((yawNow() - y0) / 32) * 0.85 + clamp(pre / ((cal && cal.yaw) || 35)) * 0.45) : undefined;
        sendMsg({ t: 'act', a: 'swing', p: +pw.toFixed(2), kmh, spin, how, q: q0, aim: aim == null ? undefined : +aim.toFixed(2), lag: +((performance.now() - t0) / 1000).toFixed(3) });
        const lb = document.getElementById('swPowTxt'); if (lb) lb.textContent = `🎾 ${kmh} km/h · ${spin > 0.25 ? 'TOPSPIN ⤴' : spin < -0.15 ? 'SLICE ⤵' : 'FLAT'} ${kmh > 160 ? '💥' : ''}`;
        const pb = document.getElementById('swPow'); if (pb) { pb.style.transition = 'none'; pb.style.width = Math.round(pw * 100) + '%'; void pb.offsetWidth; pb.style.transition = 'width 1.4s ease-in'; pb.style.width = '0%'; }
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
    const [icon, noun] = SPORT[L.sport] || ['🏏', 'a bat'], motion = motionSeen && tiltOn && ctrlMode === 'motion';
    if (ctrlMode !== 'motion') {
      // joystick mode: aim with the stick, press the big button to swing. No timing stress: the game helps you.
      stage.innerHTML = `<div class="joyPad">${joyHtml(true, 'Aim left / right')}<div class="joyBtns"><button class="pbtn pa swingBtn" id="swB"><span>${icon}</span>${esc(L.sport === 'bowling' ? 'ROLL' : 'SWING')}</button></div></div>`;
      bindJoy(true);
      const sb = $('#swB'); sb.addEventListener('pointerdown', e => { e.preventDefault(); sb.classList.add('on'); swing(0.8, 'stick'); setTimeout(() => sb.classList.remove('on'), 120); });
      return;
    }
    stage.innerHTML = `<div class="swingPad ${motion ? 'motion' : ''}">
      <div class="aimBar" id="aimBar"><div class="aimTrack"><i id="aimDot"></i></div><small>${motion ? '📱 TILT your phone left / right to aim 🎯' : '⬅ drag here to aim ➡'}</small></div>
      <div class="swingZone" id="swz">
        <div class="swRings"><i></i><i></i><i></i></div>
        <div class="swIcon" id="swIcon">${icon}</div>
        <div class="swLabel">${motion ? 'SWING IT!' : esc(L.label || 'SWING!')}</div>
        <div class="swPowBar"><i id="swPow"></i></div><div class="swPowTxt" id="swPowTxt">${motion ? 'Hold it like ' + noun + ' and SWING!' : 'Motion is off'}</div>
        <div class="swHint">${motion ? (L.sport === 'tennis' ? 'Hold it like a racket. <b>Serve:</b> just swing. <b>Rally:</b> you run to the ball by yourself — swing when the ring around you turns <b>green</b>. Tilt left/right to aim. High ball + big swing = <b>SMASH!</b>' : 'Hold the phone <b>upright, screen facing you</b>, like a handle. It moves 1:1 on the TV — point where you want it and swing!') : esc(L.hint || '')}</div>
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
  // generic game panel: your private cards (tap to play) + action buttons
  let lastPanel = null;
  function renderPanel(m) {
    const cards = (m.cards || []).map((c, i) => `<button class="tcard ${c.ok ? 'ok' : ''}" data-i="${i}" ${c.ok ? '' : 'disabled'} style="--cc:${esc(c.color || '#495057')}">${c.icon ? `<span class="tci">${esc(c.icon)}</span>` : ''}<b>${esc(c.label)}</b>${c.sub ? `<small>${esc(c.sub)}</small>` : ''}</button>`).join('');
    const btns = (m.buttons || []).map(b => `<button class="pbtn2 ${b.big ? 'big' : ''}" data-b="${esc(b.id)}" ${b.off ? 'disabled' : ''} style="--bc:${esc(b.color || '#4dabff')}">${esc(b.label)}</button>`).join('');
    stage.innerHTML = `<div class="gpanel ${m.myTurn ? 'my' : ''}"><div class="gpTop"><b>${esc(m.title || '')}</b><span>${esc(m.info || '')}</span></div>
      ${m.sub ? `<div class="gpSub">${esc(m.sub)}</div>` : ''}<div class="gpHand">${cards || '<div class="muted" style="padding:20px;grid-column:1/-1;text-align:center">Nothing in your hand yet</div>'}</div><div class="gpBtns">${btns}</div></div>`;
    stage.querySelectorAll('.tcard.ok').forEach(b => flickable(b, () => sendMsg({ t: 'act', a: 'card', i: +b.dataset.i })));
    if (stage.querySelector('.tcard.ok')) stage.querySelector('.gpHand').insertAdjacentHTML('beforebegin', '<div class="rhint">👆 Flick a glowing card up to throw it</div>');
    stage.querySelectorAll('.pbtn2').forEach(b => {
      if (b.dataset.b === 'roll') { const h = document.createElement('div'); h.className = 'rollHold'; b.replaceWith(h); diceRoller(h, () => sendMsg({ t: 'act', a: 'btn', id: 'roll' })); return; }
      b.onclick = () => { sendMsg({ t: 'act', a: 'btn', id: b.dataset.b }); navigator.vibrate?.(25); };
    });
  }
  // ---------- 🎵 song guess: type + pick from live suggestions ----------
  const sNorm = s => String(s).normalize('NFD').replace(/[̀-ͯ]/g, '').toLowerCase().replace(/&/g, 'and').replace(/[^a-z0-9 ]/g, ' ').replace(/\s+/g, ' ').trim();
  let songList = [], songTried = [], songRound = -1;
  function songMatches(q) {
    q = sNorm(q); if (!q) return [];
    const words = q.split(' '), out = [];
    for (const it of songList) {
      let sc = 0, ok = true;
      for (const w of words) {
        if (it.t.startsWith(w)) sc += 0; else if (it.w.some(x => x.startsWith(w))) sc += 1; else if (it.n.includes(w)) sc += 3; else { ok = false; break; }
      }
      if (ok) out.push({ it, sc: sc + (it.t.startsWith(q) ? -5 : 0) + it.t.length / 100 });
    }
    return out.sort((a, b) => a.sc - b.sc).slice(0, 8).map(x => x.it);
  }
  function renderSong(L) {
    if (L.list && L.list.length !== songList.length) songList = L.list.map((s, i) => { const [t, a = ''] = s.split(' — '); return { i, s, t: sNorm(t), a, title: t, n: sNorm(s), w: sNorm(s).split(' ') }; });
    if (L.round !== songRound) { songRound = L.round; songTried = []; }
    let root = $('#songUI');
    if (!root || +root.dataset.r !== L.round) {
      stage.innerHTML = `<div class="songUI" id="songUI" data-r="${L.round}">
        <div class="songTop"><b id="sgRound"></b><span id="sgPts"></span></div>
        <div class="songStages" id="sgStages"></div>
        <div class="songStat" id="sgStat"></div>
        <div class="songIn" id="sgBox"><input id="sgIn" type="search" enterkeyhint="go" autocomplete="off" autocorrect="off" autocapitalize="off" spellcheck="false" placeholder="🔎 Type a song or artist…"><button id="sgX" aria-label="Clear">✕</button></div>
        <div class="songSug" id="sgSug"></div>
        <div class="songTried" id="sgTried"></div>
        <div class="songBtns" id="sgBtns"><button id="sgRep">🔁 Replay</button><button id="sgSkip">⏭ Skip · more music</button></div>
        <div class="songHost" id="sgHost"></div></div>`;
      root = $('#songUI');
      const inp = $('#sgIn'), sug = $('#sgSug');
      const pickSong = it => {
        if (!layout || layout.name !== 'song' || layout.state !== 'guess') return;
        sendMsg({ t: 'act', a: 'guess', i: it.i }); navigator.vibrate?.(25);
        songTried.push(it.s); inp.value = ''; sug.innerHTML = ''; inp.blur();
        layout.state = 'sent'; songUpdate(layout);
      };
      const draw = () => {
        const list = songMatches(inp.value);
        sug.innerHTML = list.map((it, k) => `<button data-k="${k}"><b>${esc(it.title)}</b><small>${esc(it.a)}</small></button>`).join('') || (inp.value.trim() ? '<div class="muted sgNone">No match — try the artist name</div>' : '');
        sug.querySelectorAll('button').forEach(b => b.addEventListener('pointerdown', e => { e.preventDefault(); pickSong(list[+b.dataset.k]); }));
        sug._list = list;
      };
      inp.addEventListener('input', draw);
      inp.addEventListener('keydown', e => { if (e.key === 'Enter') { e.preventDefault(); const l = sug._list || []; if (l[0]) pickSong(l[0]); } });
      $('#sgX').onclick = () => { inp.value = ''; draw(); inp.focus(); };
      $('#sgRep').onclick = () => { sendMsg({ t: 'act', a: 'replay' }); navigator.vibrate?.(15); };
      $('#sgSkip').onclick = () => { sendMsg({ t: 'act', a: 'skip' }); navigator.vibrate?.(15); inp.blur(); layout.state = 'skip'; songUpdate(layout); };
    }
    songUpdate(L);
  }
  function songUpdate(L) {
    const st = L.state, live = st === 'guess';
    $('#sgRound').textContent = L.round ? (L.total ? `🎵 Song ${L.round} / ${L.total}` : `🎵 Song ${L.round} · endless`) : '🎵 Song Guess';
    $('#sgPts').textContent = `${L.pts || 0} pts`;
    $('#sgStages').innerHTML = (L.stages || []).map((s, i) => `<span class="${i < L.stage ? 'past' : i === L.stage && st !== 'reveal' ? 'cur' : ''}">${s}s</span>`).join('');
    const msg = {
      load: '⏳ Next song coming…', guess: L.stage === 0 ? '👂 0.1 seconds… what is it?!' : `👂 Heard ${L.stages[L.stage]}s — what is it?`,
      sent: '📨 Checking…', wrong: '❌ Not that one! More music is coming…', skip: '⏭ Skipped — listen to the next bit',
      right: `✅ YOU GOT IT! ${L.mine || ''}`, reveal: `It was <b>${esc(L.answer || '')}</b><br>${esc(L.mine || '')}`,
    }[st] || '';
    const sEl = $('#sgStat'); sEl.innerHTML = st === 'reveal' ? msg : esc(msg); sEl.className = 'songStat ' + st;
    const inp = $('#sgIn'); inp.disabled = !live; if (!live) { inp.value = ''; $('#sgSug').innerHTML = ''; }
    $('#sgBox').style.display = st === 'right' || st === 'reveal' || st === 'load' ? 'none' : '';
    $('#sgBtns').style.display = live ? '' : 'none';
    $('#sgTried').innerHTML = songTried.length && st !== 'reveal' ? songTried.map(s => `<div>❌ <s>${esc(s)}</s></div>`).join('') : '';
    const h = $('#sgHost');
    h.innerHTML = st === 'reveal' && L.host ? `<button data-b="next">⏭ Next song</button><button data-b="end" class="end">🏁 End game</button>` : '';
    h.querySelectorAll('button').forEach(b => b.onclick = () => sendMsg({ t: 'act', a: 'btn', id: b.dataset.b }));
  }
  function renderButton(L) {
    if (/🎲|ROLL/i.test(L.label || '')) {  // rolling dice = flick or shake, not a button
      stage.innerHTML = `<div class="center"><div class="rollHold" id="rollH"></div><p class="muted">${esc(L.hint || '')}</p></div>`;
      diceRoller($('#rollH'), () => { btn.a = true; flush(); setTimeout(() => { btn.a = false; flush(); }, 150); });
      return;
    }
    stage.innerHTML = `<div class="center"><button class="tapBig" id="tap">${esc(L.label || 'TAP!')}</button><p class="muted">${esc(L.hint || '')}</p><p class="muted" style="font-size:13px">PC: Space</p></div>`;
    bindHold($('#tap'), 'a');
  }
  function renderShake(L) {
    if (L.rope) {  // tug of war: pull the rope down, hand over hand
      stage.innerHTML = `<div class="ropeC" id="rp"><div class="rope" id="ropeV"></div><div class="hands">✊</div><h2>PULL!</h2><div class="shakeCount" id="shakeN">0</div><div class="rhint">${esc(L.hint || 'Grab the rope and pull it DOWN, again and again!')}</div></div>`;
      const z = $('#rp'), rope = $('#ropeV'); let id = null, ly = 0, acc = 0, off = 0; z.style.touchAction = 'none';
      z.addEventListener('pointerdown', e => { id = e.pointerId; ly = e.clientY; try { z.setPointerCapture(id); } catch (_) {} });
      z.addEventListener('pointermove', e => { if (e.pointerId !== id) return; const d = e.clientY - ly; ly = e.clientY; if (d > 0) { acc += d; off = (off + d) % 60; rope.style.backgroundPositionY = off + 'px'; while (acc > 45) { acc -= 45; pump(); navigator.vibrate?.(10); } } });
      const up = e => { if (e.pointerId === id) { id = null; acc = 0; } }; z.addEventListener('pointerup', up); z.addEventListener('pointercancel', up);
      return;
    }
    stage.innerHTML = `<div class="center"><h2>${motionSeen && ctrlMode === 'motion' ? '📳 SHAKE YOUR PHONE!' : '👆 TAP AS FAST AS YOU CAN!'}</h2><div class="shakeCount" id="shakeN">0</div>
      <button class="tapBig" id="tap" style="font-size:30px">${motionSeen && ctrlMode === 'motion' ? 'or TAP here' : 'TAP!'}</button><p class="muted">${esc(L.hint || '')}</p></div>`;
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
    stage.querySelectorAll('.hand .card').forEach(b => { const i = +b.dataset.i;
      if (m.myTurn && m.playable[i]) { flickable(b, () => sendMsg({ t: 'act', a: 'play', i })); return; }
      b.onclick = () => { if (!m.myTurn) return toast('Not your turn yet'); b.classList.remove('wiggle'); void b.offsetWidth; b.classList.add('wiggle'); navigator.vibrate?.(40); }; });
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
      joined = true; me = m; tries = 0; joinTries = 0; netBanner(false); if (camMode) sendMsg({ t: 'camOn' }); setTimeout(motionGate, 50); calButton(); if (cal) sendMsg({ t: 'calib', yaw: Math.round(cal.yaw) }); sendMsg({ t: 'ctrl', mode: ctrlMode }); updateTiltUI();
      $('#joinScr').classList.add('hidden'); $('#ctrlScr').classList.remove('hidden');
      $('#meDot').style.background = m.color; $('#meName').textContent = `${m.avatar} ${m.name}`;
      document.body.style.setProperty('--c', m.color); setStatus('🟢 Room ' + m.code);
      if (!layout) renderMsg({ text: 'Connected!' });
    } else if (m.t === 'layout') render(m.layout);
    else if (m.t === 'panel') { const was = lastPanel && lastPanel.myTurn; lastPanel = m; if (layout && layout.name === 'panel') renderPanel(m); if (m.myTurn && !was) navigator.vibrate?.([60, 40, 60]); }
    else if (m.t === 'hand') { const was = lastHand && lastHand.myTurn; renderHand(m); if (m.myTurn && !was) navigator.vibrate?.([60, 40, 60]); }
    else if (m.t === 'focus') { vipFocus = m; const f = document.getElementById('focusName'); if (f) f.textContent = `${m.emoji} ${m.name}`; }
    else if (m.t === 'snap') onSnap(m);
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
    if (ctrlMode === 'motion') await askMotion(); await ICE_READY; connect();
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
