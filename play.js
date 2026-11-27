/* Couch Chaos — controller (phone or PC) */
(() => {
  const IS_PC = matchMedia('(hover: hover) and (pointer: fine)').matches;  // keyboard hints only on a computer
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
  function setStatus(t) { const el = $('#status'); el.classList.toggle('warn', /🟠|🔴/.test(t)); el.textContent = t.replace(/^[🟢🟠🔴]\s*/u, ''); }
  function joinErr(t) { $('#joinErr').textContent = t; $('#joinBtn').disabled = false; $('#joinBtn').textContent = 'Join the party'; }
  const light = c => { const m = /^#([0-9a-f]{6})$/i.exec(c || ''); if (!m) return /^#fff$/i.test(c || ''); const v = parseInt(m[1], 16); return 0.299 * (v >> 16) + 0.587 * ((v >> 8) & 255) + 0.114 * (v & 255) > 160; };
  // ---------- one icon set for every phone (OS emoji look different on every device) ----------
  const SV = d => `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.6" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">${d}</svg>`;
  const ICONS = {
    left: SV('<path d="M20 12H5M11 5l-7 7 7 7"/>'), right: SV('<path d="M4 12h15M13 5l7 7-7 7"/>'),
    down: SV('<path d="M12 4v15M5 13l7 7 7-7"/>'), up: SV('<path d="M12 20V5M5 11l7-7 7 7"/>'),
    check: SV('<path d="M4 12.5l5.2 5.5L20 6.5"/>'), cross: SV('<path d="M6 6l12 12M18 6L6 18"/>'),
    rock: SV('<path d="M3.8 15.8l1.9-6.6 4.9-3.9 6.4.9 3.2 4.9-1.3 6.4-5.9 2.3-6.3-1z" fill="currentColor" fill-opacity=".4"/><path d="M8.5 11.5l2.2 1.200M14.8 9.300l-1.3 2.2"/>'),
    paper: SV('<path d="M6.5 3.5h8l3.5 3.5v13.5h-11.5z" fill="currentColor" fill-opacity=".28"/><path d="M14.5 3.5V7H18M9.3 11.5h5.6M9.3 15h5.6"/>'),
    scissors: SV('<circle cx="6.5" cy="17.5" r="2.7"/><circle cx="17.5" cy="17.5" r="2.7"/><path d="M8.2 15.4L17 3.5M15.8 15.4L7 3.5"/>'),
    handsup: SV('<circle cx="12" cy="8" r="2.6" fill="currentColor" fill-opacity=".28"/><path d="M12 11v6M12 12.5L6.5 4.5M12 12.5l5.5-8M12 17l-3 4.500M12 17l3 4.5"/>'),
    clap: SV('<path d="M7.5 12.500l4.5-4.5 3 3-4.5 4.500a2.1 2.1 0 0 1-3-3z" fill="currentColor" fill-opacity=".28"/><path d="M11 17l4.5-4.5 3 3-3.7 3.700a3 3 0 0 1-4.2 0zM12 2.500v2.500M6.5 4.500l1.5 2M17.5 4.500l-1.5 2"/>'),
    jump: SV('<path d="M5.5 12l6.5-6.5 6.5 6.500M5.5 19l6.5-6.5 6.5 6.5"/>'),
    squat: SV('<path d="M5.5 5l6.5 6.5 6.5-6.500M5.5 12l6.5 6.5 6.5-6.5"/>'),
    tap: SV('<circle cx="12" cy="12" r="3.2" fill="currentColor"/><circle cx="12" cy="12" r="8.5"/>'),
    run: SV('<path d="M4 6l6.5 6-6.5 6M12.5 6l6.5 6-6.5 6"/>'),
    dice: SV('<rect x="4" y="4" width="16" height="16" rx="3.5" fill="currentColor" fill-opacity=".28"/><circle cx="8.7" cy="8.7" r=".9" fill="currentColor"/><circle cx="15.3" cy="15.3" r=".9" fill="currentColor"/><circle cx="12" cy="12" r=".9" fill="currentColor"/>'),
    hand: SV('<path d="M7 12V6.500a1.5 1.5 0 0 1 3 0V11M10 11V4.500a1.5 1.5 0 0 1 3 0V11M13 11V5.500a1.5 1.5 0 0 1 3 0V12M16 12V8.500a1.5 1.5 0 0 1 3 0V15a6 6 0 0 1-6 6h-1.500a5.5 5.5 0 0 1-4.6-2.500L4 14a1.5 1.5 0 0 1 2.5-1.600L7 13.2" fill="currentColor" fill-opacity=".22"/>'),
    eye: SV('<path d="M2.5 12s3.5-6.5 9.5-6.5 9.5 6.5 9.5 6.5-3.5 6.5-9.5 6.500S2.5 12 2.5 12z" fill="currentColor" fill-opacity=".22"/><circle cx="12" cy="12" r="2.8" fill="currentColor"/>'),
    clock: SV('<circle cx="12" cy="12" r="8.5" fill="currentColor" fill-opacity=".22"/><path d="M12 7v5.200l3.5 2"/>'),
    star: SV('<path d="M12 3l2.7 5.6 6.1.800-4.5 4.3 1.1 6.100L12 16.9 6.6 19.800l1.1-6.100L3.2 9.400l6.1-.800z" fill="currentColor" fill-opacity=".3"/>'),
  };
  const EMO = { '\u2B05': 'left', '\u27A1': 'right', '\u2B07': 'down', '\u2B06': 'up', '\u2705': 'check', '\u2611': 'check', '\u2714': 'check', '\u274C': 'cross', '\u270A': 'rock', '\u270B': 'hand', '\u270C': 'scissors',
    '\uD83D\uDE4C': 'handsup', '\uD83D\uDC4F': 'clap', '\uD83E\uDD98': 'jump', '\uD83D\uDC06': 'jump', '\uD83D\uDC07': 'jump', '\uD83D\uDC46': 'tap', '\uD83C\uDFC3': 'run', '\uD83C\uDFB2': 'dice', '\uD83E\uDD1A': 'hand', '\uD83D\uDD90': 'hand',
    '\uD83D\uDC40': 'eye', '\u23F0': 'clock', '\u2B50': 'star', '\uD83E\uDDE0': 'eye', '\uD83D\uDC47': 'down', '\uD83D\uDC48': 'left', '\uD83D\uDC49': 'right' };
  const WORD_ICON = { LEFT: 'left', RIGHT: 'right', JUMP: 'jump', SQUAT: 'squat', CLAP: 'clap', 'HANDS UP': 'handsup', ROCK: 'rock', PAPER: 'paper', SCISSORS: 'scissors' };
  // split "emoji label" into a house icon + clean text (unknown emoji stay as they are)
  function icoSplit(label) {
    let s = String(label == null ? '' : label).replace(/\uFE0F/g, ''), key = null;
    const m = /^(\uD83C[\uDC00-\uDFFF]|\uD83D[\uDC00-\uDFFF]|\uD83E[\uDC00-\uDFFF]|[\u2190-\u2BFF])\s*/.exec(s);
    if (m && EMO[m[1]]) { key = EMO[m[1]]; s = s.slice(m[0].length); }
    const w = WORD_ICON[s.trim().toUpperCase()]; if (w) { key = w; if (m && !EMO[m[1]]) s = s.slice(m[0].length); }
    return { icon: key ? ICONS[key] : '', text: s };
  }
  // strip a leading emoji from short status lines (the phone uses its own icons)
  const noEmo = s => String(s == null ? '' : s).replace(/\uFE0F/g, '').replace(/^((\uD83C[\uDC00-\uDFFF]|\uD83D[\uDC00-\uDFFF]|\uD83E[\uDC00-\uDFFF]|[\u2190-\u2BFF])\s*)+/, '');
  const icoBtn = label => { const q = icoSplit(label); return (q.icon ? `<i class="kIco">${q.icon}</i>` : '') + `<span>${esc(q.text)}</span>`; };
  // phone prompts stay short: the TV explains the game
  const shortHint = h => { let s = String(h || '').replace(/\uFE0F/g, '').split(/\s[\u00B7\u2014\u2013]\s|[.!]\s|\s\(/)[0].trim().replace(/[.!]$/, ''); return s.length > 44 ? s.slice(0, 42).replace(/\s+\S*$/, '') + '\u2026' : s; };

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
    const cb = $('#ctrlBtn'); if (cb) { cb.classList.toggle('hidden', !joined || camMode || !layout || !((layout.name === 'pad' && layout.stick) || layout.name === 'swing')); cb.innerHTML = ctrlMode === 'motion' ? '📱 Motion' : '🕹️ Joystick'; cb.classList.toggle('on', ctrlMode === 'motion'); }
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
    if (layout && layout.name === 'touch' && layout.mode === 'go') {
      const s = TM.stk, me = (TM.objs || []).find(o => o.k === 1), asp = +layout.aspect || 1;
      if (!s) return { x: 0, y: 0, a: keys.a || btn.a, b: keys.b || btn.b, touch: false };
      const cx = me ? me.x : 0.5, cy = me ? me.y : 0.5, reach = 0.22;
      const tx = clamp(cx + s.x * reach, 0, 1), ty = clamp(cy + s.y * reach * asp, 0, 1);
      return { x: tx * 2 - 1, y: ty * 2 - 1, a: keys.a || btn.a, b: keys.b || btn.b, touch: true };
    }
    if (layout && layout.name === 'touch') { const f = TM.touching && TM.fing; return { x: f ? TM.fing.x * 2 - 1 : 0, y: f ? TM.fing.y * 2 - 1 : 0, a: keys.a || btn.a, b: keys.b || btn.b, touch: !!f }; }
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
  function diceRoller(holder, onRoll, label = 'Flick the dice up — or shake your phone!', key = '') {
    holder.innerHTML = key  // compact: one re-roll key, used where the real dice are already on screen
      ? `<div class="roller compact"><div class="rdice rkey">🎲 ${esc(key)}</div><div class="rhint">${esc(label)}</div></div>`
      : `<div class="roller"><div class="rdice"><span class="rdie">${PIPS[5]}</span><span class="rdie b">${PIPS[3]}</span></div><div class="rhint">🎲 ${esc(label)}</div></div>`;
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
    if (m.i !== undefined && m.i !== TM.info) { if (TM.L.trail && TM.info.split('·')[0] !== m.i.split('·')[0]) TM.tr = {}; TM.info = m.i; const e = document.getElementById('tInfo'); if (e) e.textContent = noEmo(m.i); }
    if (TM.L.trail) n.forEach(o => { if (o.k < 1 || o.k > 3) return; const tr = TM.tr[o.c] || (TM.tr[o.c] = []), l = tr[tr.length - 1]; if (l && Math.hypot(l[0] - o.x, l[1] - o.y) > 0.08) tr.length = 0; if (!l || Math.hypot(l[0] - o.x, l[1] - o.y) > 0.002) tr.push([o.x, o.y]); });
    // "go" mode: arrived at the spot you tapped → stop
    if (TM.L.mode === 'go' && TM.fing && !TM.down) { const me = n.find(o => o.k === 1); if (me && Math.hypot(me.x - TM.fing.x, (me.y - TM.fing.y) / TM.L.aspect) < 0.012) { TM.touching = false; TM.fing = null; flush(true); } }
  }
  function renderTouch(L) {
    TM.on = true; TM.L = L; TM.objs = []; TM.fing = null; TM.touching = false; TM.down = false; TM.info = ''; TM.tr = {}; TM.t0 = 0; TM.paint = null; TM.stk = null;
    if (L.mode === 'go') return renderStick(L);
    TM.fill = L.mode === 'aim' && !L.btn && !L.btn2; TM.rk = TM.fill ? 1.9 : 1;
    stage.innerHTML = `<div class="touchC ${L.btn || L.btn2 ? '' : 'full'}"><div class="touchInfo" id="tInfo"></div><div class="touchWrap" id="tw" style="--asp:${+L.aspect || 1.9}"><canvas id="tcv"></canvas></div>
      <div class="touchBottom"><div class="rhint">${esc(shortHint(L.hint))}</div>${L.btn2 ? `<button class="pbtn pb touchBtn" data-b="b">${icoBtn(L.btn2)}</button>` : ''}${L.btn ? `<button class="pbtn pa touchBtn" data-b="a">${icoBtn(L.btn)}</button>` : ''}</div></div>`;
    stage.querySelectorAll('.pbtn').forEach(b => bindHold(b, b.dataset.b));
    const tw = $('#tw'), cv = $('#tcv'), g = cv.getContext('2d');
    const fit = () => {
      const W = tw.clientWidth, H = tw.clientHeight; if (!W || !H) return;
      TM.rot = false; const asp = TM.fill ? W / H : L.aspect;  // never turn the map: up on the phone is up on the TV
      let cw = W, ch = W / asp; if (ch > H) { ch = H; cw = H * asp; }
      const d = Math.min(2, devicePixelRatio || 1); cv.width = Math.round(cw * d); cv.height = Math.round(ch * d); cv.style.width = cw + 'px'; cv.style.height = ch + 'px';
      TM.cw = cw; TM.ch = ch; TM.d = d;
    };
    fit(); addEventListener('resize', fit);
    const at = e => { const r = cv.getBoundingClientRect(), fx = Math.max(0, Math.min(1, (e.clientX - r.left) / r.width)), fy = Math.max(0, Math.min(1, (e.clientY - r.top) / r.height)); return TM.rot ? { x: 1 - fy, y: fx } : { x: fx, y: fy }; };
    let pid = null;
    cv.style.touchAction = 'none'; tw.style.touchAction = 'none';
    tw.addEventListener('pointerdown', e => { e.preventDefault(); pid = e.pointerId; try { tw.setPointerCapture(pid); } catch (_) {} let f = at(e);
      if (TM.fill) {  // only a tap that lands on a target counts, and it counts dead centre
        const r = cv.getBoundingClientRect(); let best = null, bd = 1e9;
        for (const o of TM.objs) { if (o.k === 1 || o.k === 2 || o.k === 3 || !(o.r > 0)) continue; const dd = Math.hypot((o.x - f.x) * r.width, (o.y - f.y) * r.height) - o.r * r.width * TM.rk; if (dd < 16 && dd < bd) { bd = dd; best = o; } }
        if (!best) { pid = null; return; }
        f = { x: best.x, y: best.y };
      }
      TM.fing = f; TM.touching = true; TM.down = true; TM.t0 = performance.now(); navigator.vibrate?.(8); flush(true); });
    tw.addEventListener('pointermove', e => { if (e.pointerId !== pid || TM.fill) return; TM.fing = at(e); flush(); });
    const up = e => { if (e.pointerId !== pid) return; pid = null; TM.down = false;
      if (L.mode !== 'go') { const rel = () => { if (TM.down) return; TM.touching = false; TM.fing = null; flush(true); }; const held = performance.now() - TM.t0; if (held < 130) { flush(true); setTimeout(rel, 130 - held); } else rel(); return; }
      flush(true); };
    tw.addEventListener('pointerup', up); tw.addEventListener('pointercancel', up);
    cancelAnimationFrame(TM.raf);
    const frame = () => {
      if (!TM.on || !document.body.contains(cv)) return;
      if (!TM.cw) fit();
      drawTouch(g, L); TM.raf = requestAnimationFrame(frame);
    };
    TM.raf = requestAnimationFrame(frame);
  }
  // a big thumbstick for the run-around games: push a direction, your character runs that way on the TV
  function renderStick(L) {
    stage.innerHTML = `<div class="stickC"><div class="touchInfo" id="tInfo"></div>
      <div class="stkZone" id="stz"><div class="stkBase"><i class="stkAr u"></i><i class="stkAr d"></i><i class="stkAr l"></i><i class="stkAr r"></i><div class="stkKnob" id="stk"></div></div></div>
      <div class="rhint">Push the stick to move</div>
      <div class="stkBtns">${L.btn2 ? `<button class="pbtn pb touchBtn" data-b="b">${icoBtn(L.btn2)}</button>` : ''}${L.btn ? `<button class="pbtn pa touchBtn" data-b="a">${icoBtn(L.btn)}</button>` : ''}</div></div>`;
    stage.querySelectorAll('.pbtn').forEach(b => bindHold(b, b.dataset.b));
    const z = $('#stz'), base = z.querySelector('.stkBase'), knob = $('#stk'); let pid = null;
    z.style.touchAction = 'none';
    const set = e => {
      const r = base.getBoundingClientRect(), R = r.width * 0.36; let dx = e.clientX - (r.left + r.width / 2), dy = e.clientY - (r.top + r.height / 2);
      const d = Math.hypot(dx, dy); if (d > R) { dx *= R / d; dy *= R / d; }
      knob.style.transform = `translate(${dx}px, ${dy}px)`;
      const m = Math.min(1, d / R); TM.stk = m < 0.14 ? null : { x: dx / R, y: dy / R };
    };
    z.addEventListener('pointerdown', e => { e.preventDefault(); pid = e.pointerId; try { z.setPointerCapture(pid); } catch (_) {} base.classList.add('on'); set(e); navigator.vibrate?.(8); flush(true); });
    z.addEventListener('pointermove', e => { if (e.pointerId !== pid) return; set(e); flush(); });
    const up = e => { if (e.pointerId !== pid) return; pid = null; TM.stk = null; knob.style.transform = ''; base.classList.remove('on'); flush(true); };
    z.addEventListener('pointerup', up); z.addEventListener('pointercancel', up);
    // the game expects "where on the map to run to": aim a point ahead of my character in the stick's direction
    clearInterval(TM.stkIv); TM.stkIv = setInterval(() => { if (!TM.on || !document.body.contains(z)) { clearInterval(TM.stkIv); return; } if (TM.stk) flush(); }, 50);
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
    } else if (th === 'grid') {
      g.fillStyle = '#0b0a24'; g.fillRect(0, 0, W, H); g.strokeStyle = '#1c3d7a'; g.lineWidth = 1 * d;
      for (let i = 1; i < 20; i++) { g.beginPath(); g.moveTo(W * i / 20, 0); g.lineTo(W * i / 20, H); g.stroke(); }
      for (let i = 1; i < 10; i++) { g.beginPath(); g.moveTo(0, H * i / 10); g.lineTo(W, H * i / 10); g.stroke(); }
      g.strokeStyle = '#22d3ee'; g.lineWidth = 5 * d; g.strokeRect(0, 0, W, H);
    } else if (th === 'sea' || th === 'sky') {
      const gr = g.createLinearGradient(0, 0, 0, H); gr.addColorStop(0, th === 'sea' ? '#1d6fc0' : '#3aa0e8'); gr.addColorStop(1, th === 'sea' ? '#124a8a' : '#1f78c9'); g.fillStyle = gr; g.fillRect(0, 0, W, H);
      g.strokeStyle = 'rgba(255,255,255,.12)'; g.lineWidth = 2 * d; for (let i = 0; i < 9; i++) { const y = ((i / 9 + t * 0.02) % 1) * H; g.beginPath(); for (let x = 0; x <= W; x += 10 * d) g.lineTo(x, y + Math.sin(x / (30 * d) + t * 2 + i) * 3 * d); g.stroke(); }
    } else if (th === 'carnival') {
      const gr = g.createLinearGradient(0, 0, 0, H); gr.addColorStop(0, '#5b3cc4'); gr.addColorStop(1, '#ffb3d9'); g.fillStyle = gr; g.fillRect(0, 0, W, H);
    } else if (th === 'snow' || th === 'slope') {
      const gr = g.createLinearGradient(0, 0, 0, H); gr.addColorStop(0, '#ffffff'); gr.addColorStop(1, '#dfeeff'); g.fillStyle = gr; g.fillRect(0, 0, W, H);
      if (th === 'slope') { g.fillStyle = '#ff922b'; const off = (t * 0.6 % 1) * H / 8; for (let y = -H / 8; y < H; y += H / 8) { g.fillRect(0, y + off, 5 * d, H / 16); g.fillRect(W - 5 * d, y + off, 5 * d, H / 16); } }
    } else if (th === 'party') {
      g.fillStyle = '#f783ac'; g.fillRect(0, 0, W, H); g.fillStyle = 'rgba(255,255,255,.12)'; const s = W / 12; for (let y = 0; y < H; y += s) for (let x = ((y / s) % 2) * s; x < W; x += 2 * s) g.fillRect(x, y, s, s);
      g.fillStyle = 'rgba(255,212,59,.55)'; g.beginPath(); g.arc(W / 2, H / 2, W * 0.13, 0, 7); g.fill();
    } else if (th === 'track') {
      g.fillStyle = '#f8f9fa'; g.fillRect(0, 0, W, H);
      for (let i = 0; i < 24; i++) for (let r = 0; r < 2; r++) { g.fillStyle = (i + r) % 2 ? '#212529' : '#fff'; g.fillRect(W * i / 24, r * H * 0.03, W / 24, H * 0.03); }
      g.fillStyle = '#4dabf7'; g.fillRect(0, H * 0.94, W, 3 * d);
    } else {
      const gr = g.createLinearGradient(0, 0, 0, H); gr.addColorStop(0, '#6cc45f'); gr.addColorStop(1, '#4fa548'); g.fillStyle = gr; g.fillRect(0, 0, W, H);
      g.fillStyle = 'rgba(255,255,255,.06)'; for (let i = 0; i < 12; i += 2) g.fillRect(W * i / 12, 0, W / 12, H);
    }
    // ---- painted floor (Paint Wars) ----
    if (TM.paint) { const P = TM.paint, cw = W / P.w, chh = H / P.h; g.globalAlpha = 0.85;
      for (let r = 0; r < P.h; r++) for (let q = 0; q < P.w; q++) { const v = P.g.charCodeAt(r * P.w + q) - 97; if (v < 0 || !P.c[v]) continue; g.fillStyle = P.c[v]; g.fillRect(q * cw - 0.5, r * chh - 0.5, cw + 1, chh + 1); }
      g.globalAlpha = 1; }
    // ---- objects (smoothed) ----
    const objs = TM.objs; objs.forEach(o => { o.dx += (o.x - o.dx) * 0.45; o.dy += (o.y - o.dy) * 0.45; });
    if (L.trail) for (const c in TM.tr) { const tr = TM.tr[c]; if (tr.length < 2) continue; g.strokeStyle = c; g.lineWidth = 4 * d; g.lineJoin = g.lineCap = 'round'; g.shadowColor = c; g.shadowBlur = 10 * d; g.beginPath(); tr.forEach((q, i) => i ? g.lineTo(q[0] * W, q[1] * H) : g.moveTo(q[0] * W, q[1] * H)); g.stroke(); g.shadowBlur = 0; }
    // 8 gate (r = half width), 9 solid thing (tree / fort / bush / chair), 10 bomb, 11 floor disc, 12 small dot (shot, snowball)
    const order = [11, 7, 6, 5, 8, 9, 13, 4, 10, 12, 3, 2, 0, 1];
    const meDrag = L.mode === 'drag' && TM.touching && TM.fing;
    for (const k of order) for (const o of objs) {
      if (o.k !== k) continue;
      let x = o.dx * W, y = o.dy * H; const r = o.r * W * (TM.rk || 1);
      if (k === 6 || k === 7) { const hw = o.r * W, hh = o.r2 * H; g.fillStyle = k === 6 ? `rgba(255,59,59,${0.55 + 0.3 * Math.sin(t * 10)})` : '#0b1b3a'; rr(g, x - hw + d, y - hh + d, hw * 2 - 2 * d, hh * 2 - 2 * d, 4 * d); g.fill(); continue; }
      if (k === 5) { g.fillStyle = 'rgba(255,59,59,.28)'; g.beginPath(); g.arc(x, y, r, 0, 7); g.fill(); g.strokeStyle = '#ff3b3b'; g.lineWidth = 3 * d; g.setLineDash([8 * d, 6 * d]); g.lineDashOffset = -t * 30 * d; g.stroke(); g.setLineDash([]); continue; }
      if (k === 11) { g.fillStyle = o.c; g.beginPath(); g.arc(x, y, r, 0, 7); g.fill(); g.strokeStyle = 'rgba(255,255,255,.9)'; g.lineWidth = 4 * d; g.stroke(); continue; }
      if (k === 8) { g.strokeStyle = o.c; g.globalAlpha = 0.45; g.lineWidth = 6 * d; g.beginPath(); g.moveTo(x - r, y); g.lineTo(x + r, y); g.stroke(); g.globalAlpha = 1;
        for (const s of [-1, 1]) { const px = x + s * r; g.fillStyle = '#495057'; g.fillRect(px - 1.5 * d, y - 16 * d, 3 * d, 16 * d); g.fillStyle = o.c; g.beginPath(); g.moveTo(px, y - 16 * d); g.lineTo(px - s * 12 * d, y - 12 * d); g.lineTo(px, y - 8 * d); g.fill(); } continue; }
      if (k === 9) { const rad = Math.max(r, 5 * d); g.fillStyle = 'rgba(0,0,0,.18)'; g.beginPath(); g.arc(x + 2 * d, y + 3 * d, rad, 0, 7); g.fill(); g.fillStyle = o.c; g.beginPath(); g.arc(x, y, rad, 0, 7); g.fill(); g.strokeStyle = 'rgba(0,0,0,.25)'; g.lineWidth = 2 * d; g.stroke(); g.fillStyle = 'rgba(255,255,255,.3)'; g.beginPath(); g.arc(x - rad * 0.3, y - rad * 0.3, rad * 0.35, 0, 7); g.fill(); continue; }
      if (k === 10) { const rad = Math.max(r, 7 * d); g.fillStyle = '#1c1f26'; g.beginPath(); g.arc(x, y, rad, 0, 7); g.fill(); g.strokeStyle = '#ff3b3b'; g.lineWidth = 3 * d; g.stroke(); g.fillStyle = `rgba(255,200,60,${0.6 + 0.4 * Math.sin(t * 20)})`; g.beginPath(); g.arc(x + rad * 0.6, y - rad * 0.9, 3 * d, 0, 7); g.fill(); continue; }
      if (k === 13) { const s = Math.max(r, 7 * d); if (o.r2) { g.shadowColor = o.c; g.shadowBlur = (10 + 8 * Math.sin(t * 8)) * d; } g.fillStyle = o.c; rr(g, x - s, y - s, 2 * s, 2 * s, 4 * d); g.fill(); g.shadowBlur = 0; g.strokeStyle = o.r2 ? '#fff' : 'rgba(0,0,0,.3)'; g.lineWidth = (o.r2 ? 3 : 2) * d; g.stroke(); continue; }
      if (k === 12) { g.fillStyle = o.c; g.beginPath(); g.arc(x, y, Math.max(r, 3 * d), 0, 7); g.fill(); continue; }
      if (k === 4) { g.shadowColor = o.c; g.shadowBlur = 12 * d; g.fillStyle = o.c; g.beginPath(); g.arc(x, y, Math.max(r, 6 * d), 0, 7); g.fill(); g.shadowBlur = 0; g.strokeStyle = '#fff'; g.lineWidth = 2 * d; g.stroke(); continue; }
      if (k === 1 && meDrag) {  // your mallet sits right under your finger, kept on your half
        const rn = r / H; x = TM.fing.x * W; y = Math.max(0.5 + rn, Math.min(1 - rn, TM.fing.y)) * H; x = Math.max(r, Math.min(W - r, x));
      }
      const rad = Math.max(r, (k === 0 ? 7 : 10) * d);
      g.fillStyle = 'rgba(0,0,0,.22)'; g.beginPath(); g.arc(x + 2 * d, y + 4 * d, rad, 0, 7); g.fill();
      const gr = g.createRadialGradient(x - rad * 0.35, y - rad * 0.4, rad * 0.1, x, y, rad); gr.addColorStop(0, '#ffffff'); gr.addColorStop(0.25, o.c); gr.addColorStop(1, o.c);
      g.fillStyle = k === 0 ? '#1c1f26' : gr; g.beginPath(); g.arc(x, y, rad, 0, 7); g.fill();
      if (k === 1) { g.strokeStyle = '#fff'; g.lineWidth = 4 * d; g.stroke(); g.strokeStyle = 'rgba(255,255,255,.5)'; g.lineWidth = 2 * d; g.beginPath(); g.arc(x, y, rad + 7 * d + 3 * d * Math.sin(t * 6), 0, 7); g.stroke();
        g.fillStyle = '#fff'; g.font = `800 ${12 * d}px Inter, sans-serif`; g.textAlign = 'center'; g.fillText('YOU', x, y - rad - 10 * d); }
      else if (k === 2) { g.strokeStyle = 'rgba(255,255,255,.85)'; g.lineWidth = 2 * d; g.stroke(); }
      else if (k === 0) { g.strokeStyle = '#555c6b'; g.lineWidth = 2 * d; g.stroke(); g.fillStyle = '#ffd43b'; g.beginPath(); g.arc(x, y, rad * 0.62, 0, 7); g.fill(); }
      else { g.strokeStyle = 'rgba(0,0,0,.35)'; g.lineWidth = 2 * d; g.stroke(); }
    }
    if (L.mode === 'aim' && TM.fing) { const fx = TM.fing.x * W, fy = TM.fing.y * H, rr2 = 18 * d; g.strokeStyle = '#fff'; g.lineWidth = 3 * d; g.beginPath(); g.arc(fx, fy, rr2, 0, 7); g.stroke();
      for (let q = 0; q < 4; q++) { const a = q * Math.PI / 2; g.beginPath(); g.moveTo(fx + Math.cos(a) * (rr2 - 6 * d), fy + Math.sin(a) * (rr2 - 6 * d)); g.lineTo(fx + Math.cos(a) * (rr2 + 9 * d), fy + Math.sin(a) * (rr2 + 9 * d)); g.stroke(); } }
    if (L.mode === 'steer' && TM.fing) { const fx = TM.fing.x * W; g.strokeStyle = 'rgba(255,255,255,.85)'; g.lineWidth = 3 * d; g.setLineDash([8 * d, 8 * d]); g.beginPath(); g.moveTo(fx, 0); g.lineTo(fx, H); g.stroke(); g.setLineDash([]);
      const me = objs.find(o => o.k === 1); if (me) { g.fillStyle = 'rgba(255,255,255,.9)'; g.beginPath(); g.moveTo(fx, me.dy * H - 26 * d); g.lineTo(fx - 9 * d, me.dy * H - 40 * d); g.lineTo(fx + 9 * d, me.dy * H - 40 * d); g.fill(); } }
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
  const camBtnHtml = () => `<button class="rowBtn noRb" id="camB"><i class="ic">📷</i><span><b>Use this phone as the camera</b><small>Body tracking for up to 4 players, no phones needed</small></span><i class="chev">›</i></button>`;
  function bindCamBtn() { const b = document.getElementById('camB'); if (b) b.onclick = startCam; }
  function render(L) {
    if (camMode) return; // the camera phone ignores game screens
    layout = L; TM.on = false; cancelAnimationFrame(TM.raf); resetInput(); pumps = 0; shakeRoll = null;
    if ((L.name === 'pad' && L.stick) || L.name === 'swing') recenterSoon = true;
    if (L.name === 'swing') orRecenter = true;
    if (needPerm) setTimeout(motionGate, 0);
    if (L.name !== 'cards') lastHand = null;
    const R = { panel: L => (lastPanel ? renderPanel(lastPanel) : renderMsg({ emoji: '🃏', text: 'Shuffling…' })), swing: renderSwing, fight: renderFight, bowl: renderBowl, tennis: renderTennis, lobby: renderLobby, pad: renderPad, button: renderButton, shake: renderShake, intro: renderIntro, choice: renderChoice, draw: renderDraw, bingo: renderBingo, dice: renderDice, song: renderSong, wires: renderWires, curl: renderCurl, four: renderFour, touch: renderTouch };
    if (R[L.name]) R[L.name](L);
    else if (L.name === 'cards') { lastHand ? renderHand(lastHand) : (stage.innerHTML = '<div class="center"><p>Dealing…</p></div>'); }
    else renderMsg(L);
    updateTiltUI(); updateIdleDock();
  }
  // the phone wears the colours and pattern of the game being played
  function applyCh(id) {
    const ch = id && typeof chOf === 'function' ? chOf(id) : null, b = document.body;
    if (!ch) { delete b.dataset.ch; delete b.dataset.pat; return; }
    b.dataset.ch = id; b.dataset.pat = ch.pat;
    b.style.setProperty('--ch1', ch.c1); b.style.setProperty('--ch2', ch.c2); b.style.setProperty('--chDeep', ch.deep); b.style.setProperty('--chAcc', ch.accent);
  }
  // ---------- 😀 reactions: something to do while you wait; they pop up over the TV with your name ----------
  const REACTS = ['🔥', '👏', '😂', '😱', '😭', '🤯', '😎', '🎉'];
  let lastReact = 0;
  function setupReacts() {
    const row = $('#reactRow'), btn = $('#reactBtn'), dock = $('#emoteDock'); if (!row || !btn || row.dataset.ready) return; row.dataset.ready = '1';
    const html = REACTS.map(e => `<button class="reactE" data-e="${e}">${e}</button>`).join('');
    row.innerHTML = html; if (dock) { dock.innerHTML = '<div class="edLabel">While you wait, say something!</div><div class="edRow">' + html + '</div>'; dock.addEventListener('click', ev => sendReact(ev)); }
    btn.onclick = () => { row.classList.toggle('hidden'); btn.classList.toggle('on', !row.classList.contains('hidden')); };
    function sendReact(ev) {
      const b = ev.target.closest('.reactE'); if (!b) return;
      const now = Date.now(); if (now - lastReact < 700) return; lastReact = now;
      sendMsg({ t: 'react', e: b.dataset.e }); navigator.vibrate?.(12);
      b.classList.remove('pop'); void b.offsetWidth; b.classList.add('pop');
    }
    row.addEventListener('click', sendReact);
  }
  // while it isn't your turn (watching a bowler, someone else's move, a "look at the TV" screen) the emotes sit at the bottom, big, ready to tap
  function updateIdleDock() {
    const dock = $('#emoteDock'); if (!dock || !joined) return;
    const n = layout && layout.name; let idle = false;
    if (n === 'bowl') idle = !BW.mine;
    else if (n === 'panel') idle = !!lastPanel && !lastPanel.myTurn;
    else if (n === 'four') idle = !layout.my;
    else if (n === 'tennis') idle = false;
    else idle = n === 'msg' || (layout && !['lobby', 'intro', 'choice', 'pad', 'button', 'swing', 'shake', 'draw', 'bingo', 'dice', 'song', 'wires', 'curl', 'touch', 'cards', 'fight'].includes(n));
    dock.classList.toggle('hidden', !idle); document.body.classList.toggle('dockOn', idle);
  }
  // ---------- 🎳 bowling: slide the ball, FLICK it up, curve your flick for a hook, steer it while it rolls ----------
  // The lane is drawn on your phone; the TV streams the ball back so you can steer it. Lane units: x in metres (screen-right positive), z down the lane 0..20.
  const BW = { st: { ph: 'aim', x: 0, z: 0.3, pins: 1023, msg: '' }, raf: 0, x: 0, aimN: 0, hookN: 0, powN: 0, mode: null, steer: 0, thrown: 0, throwT: 0 };
  const BW_L = 18.3, BW_PIN = i => { let r = 0, k = i; while (k > r) { r++; k -= r; } return { x: (k - r / 2) * 0.305, z: 18.3 + r * 0.264 }; };
  // the ball's planned path: starts at x0 with a straight slope, then curves (hook) late
  // the oil pattern: slick for 11.5 m, then the boards grip. The hook is a sideways acceleration there, so a slower ball curves more.
  const bwProf = z => { const t = clamp((z - 11.5) / 4, 0, 1); return 0.12 + 0.88 * t * t * (3 - 2 * t); };
  const BW_I = (() => { const a = [0]; let s0 = 0, s1 = 0; for (let i = 1; i <= 230; i++) { const u = (i - 0.5) * 0.1, pf = bwProf(u); s0 += pf * 0.1; s1 += u * pf * 0.1; a.push(i * 0.1 * s0 - s1); } return a; })();
  const bwI = z => { const f = clamp(z, 0, 22.9) * 10, i = Math.floor(f); return BW_I[i] + (BW_I[Math.min(i + 1, 230)] - BW_I[i]) * (f - i); };
  // The ball's whole path, simulated step by step exactly as the TV does it (same numbers as bowling.cpp): the skid, the late hook, the little pull toward the pocket,
  // and the rebounds off the bumpers (or, with no bumpers, the slide into the gutter). So the line on your phone is the line on the TV, 1:1.
  const BW_BUMP = { X: 0.54 - 0.1085, END: 17.9, E: 0.45 };
  function bwSim(x0, aimN, hookN, powN, bump) {
    const vz0 = 6.5 + 4.5 * powN, Ln = 18.29; let slope = aimN * 0.035;
    const xi = x0 + slope * Ln + hookN * 0.9 * bwI(Ln) / (vz0 * vz0);
    if (Math.abs(xi) < 0.26) { const tgt = (xi === 0 ? -1 : Math.sign(xi)) * 0.075, xi2 = xi + (tgt - xi) * 0.4; slope += (xi2 - xi) / Ln; }
    let x = x0, z = -0.1, vx = slope * vz0, vz = vz0, gut = false; const ax = hookN * 0.9, h = 1 / 240, out = [x0]; let g = 1;
    for (let n = 0; n < 4000 && g <= 184; n++) {
      const xp = x, zp = z;
      if (z < Ln) vx += ax * bwProf(z) * h;
      x += vx * h; z += vz * h;
      if (bump) { if (z < BW_BUMP.END && Math.abs(x) > BW_BUMP.X) { const sg = Math.sign(x); x = sg * BW_BUMP.X; if (vx * sg > 0) { vx *= -BW_BUMP.E; vz *= 0.985; } } }
      else if (!gut && z < 19.2 && Math.abs(x) > 0.5) gut = true;
      if (gut) { const sg = Math.sign(x) || 1; vx = 0; x += (sg * 0.65 - x) * Math.min(1, 14 * h); }
      while (g <= 184 && z >= g * 0.1) { const f = (g * 0.1 - zp) / (z - zp); out.push(xp + (x - xp) * f); g++; }
    }
    while (out.length < 185) out.push(out[out.length - 1]);
    return out;
  }
  let bwCache = { k: '', xs: [] };
  const bwPath = (x0, aimN, hookN, z) => {
    const pw = BW.mode === 'throw' ? BW.powN : 0.55, bump = BW.bump !== false, k = [x0, aimN, hookN, pw, bump].join('|');
    if (bwCache.k !== k) bwCache = { k, xs: bwSim(x0, aimN, hookN, pw, bump) };
    const f = clamp(z, 0, 18.4) * 10, i = Math.min(Math.floor(f), 183); return bwCache.xs[i] + (bwCache.xs[i + 1] - bwCache.xs[i]) * (f - i);
  };
  if (/debug/.test(location.search)) { window.CCBW = BW; window.CCBWSIM = bwSim; }   // for the tests: compares the phone's prediction with what the TV really rolls
  // fit a flick: initial slope + curvature from the finger's path, power from how fast it ended
  function bwFit(pts) {
    if (pts.length < 3) return null;
    const p0 = pts[0], p1 = pts[pts.length - 1], dy = p0.y - p1.y; if (dy < 24) return null;
    let s11 = 0, s12 = 0, s22 = 0, b1 = 0, b2 = 0;
    for (const q of pts) { const u = (p0.y - q.y) / dy, x = q.x - p0.x; s11 += u * u; s12 += u * u * u; s22 += u * u * u * u; b1 += x * u; b2 += x * u * u; }
    const det = s11 * s22 - s12 * s12; let c1 = 0, c2 = 0;
    if (Math.abs(det) > 1e-6) { c1 = (b1 * s22 - b2 * s12) / det; c2 = (s11 * b2 - s12 * b1) / det; } else c1 = (p1.x - p0.x);
    const r1 = c1 / dy, r2 = c2 / dy;
    const dz = v => (Math.abs(v) < 0.16 ? 0 : Math.sign(v) * (Math.abs(v) - 0.16) / 0.84);   // a straight flick stays straight
    let k = pts.length - 1; while (k > 0 && p1.t - pts[k].t < 90) k--;
    const q = pts[k], dt = Math.max(30, p1.t - q.t), v = Math.max(0, (q.y - p1.y) / dt);       // px per ms, upward
    return { aimN: clamp(r1 * 1.5), hookN: clamp(dz(r2 * 1.3)), powN: clamp((v - 0.35) / 1.7, 0.18, 1), dy, v };
  }
  function renderBowl(L) {
    BW.L = L; BW.mine = !!L.mine; BW.bump = L.bump == null ? true : !!L.bump; BW.mode = null; BW.thrown = 0; BW.steer = 0;
    BW.st.who = L.who || ''; BW.st.frame = L.frame || 1; BW.st.frames = L.frames || 3; BW.st.ball = L.ball || 1; BW.st.ph = L.ph || 'aim'; BW.st.msg = ''; if (L.pins != null) BW.st.pins = L.pins;
    if (BW.mine) { BW.x = 0; BW.aimN = 0; BW.hookN = 0; BW.powN = 0; }
    stage.innerHTML = `<div class="bowlPad ${BW.mine ? 'mine' : ''}"><div class="bwTop"><b>${BW.mine ? 'YOUR ROLL' : esc((L.who || 'Someone') + ' is bowling')}</b>${BW.mine ? '<button class="bwBump" id="bwBump"></button>' : ''}<span>Frame ${BW.st.frame}/${BW.st.frames} · ${BW.st.ball === 1 ? '1st' : '2nd'} ball</span></div>
      <div class="bwWrap" id="bwWrap"><canvas id="bwCv"></canvas><div class="bwPrompt" id="bwPrompt"></div></div>
      <div class="bwBot"><div class="bwPow ${BW.mine ? '' : 'off'}"><i id="bwPowI"></i><span>POWER</span></div><div class="rhint" id="bwHint"></div></div></div>`;
    const wrap = $('#bwWrap'), cv = $('#bwCv'), g = cv.getContext('2d'); let cw = 0, ch = 0, d = 1;
    const fit = () => { const W = wrap.clientWidth, H = wrap.clientHeight; if (!W || !H) return; d = Math.min(2, devicePixelRatio || 1); cw = W; ch = H; cv.width = Math.round(W * d); cv.height = Math.round(H * d); cv.style.width = W + 'px'; cv.style.height = H + 'px'; };
    fit(); addEventListener('resize', fit);
    // lane → screen (a gentle perspective so the pins feel far away)
    const yB = () => ch - 46, yT = () => 30, hwB = () => cw * 0.42, hwT = () => cw * 0.2;
    const T = z => Math.min(1, z / 20), Y = z => { const t = T(z); return yB() - (yB() - yT()) * (1 - Math.pow(1 - t, 1.45)); };
    const HW = z => { const t = Y(z); const k = (yB() - t) / (yB() - yT()); return hwB() + (hwT() - hwB()) * k; };
    const X = (x, z) => cw / 2 + (x / 0.53) * HW(z) * 0.78, S = z => HW(z) / cw * 2.4;    // metres → px scale at depth z
    const fromPx = px => clamp(((px - cw / 2) / (HW(0) * 0.78)) * 0.53 / 0.42);             // finger x at the foul line → -1..1
    const hint = t => { const e = $('#bwHint'); if (e && e.textContent !== t) e.textContent = t; };
    const draw = () => {
      if (!document.body.contains(cv)) return; if (!cw || wrap.clientWidth !== cw || wrap.clientHeight !== ch) fit();
      g.setTransform(d, 0, 0, d, 0, 0); g.clearRect(0, 0, cw, ch);
      // lane
      const grd = g.createLinearGradient(0, yT(), 0, yB()); grd.addColorStop(0, '#c99a5f'); grd.addColorStop(1, '#f0c58a');
      g.fillStyle = '#17102e'; g.fillRect(0, 0, cw, ch);
      const quad = (xa, xb, za, zb, fill) => { g.beginPath(); g.moveTo(X(xa, za), Y(za)); g.lineTo(X(xb, za), Y(za)); g.lineTo(X(xb, zb), Y(zb)); g.lineTo(X(xa, zb), Y(zb)); g.closePath(); g.fillStyle = fill; g.fill(); };
      quad(-0.78, 0.78, 0, 20, '#0d0a20');                       // gutters
      quad(-0.53, 0.53, 0, 20, grd);                              // wood
      g.strokeStyle = 'rgba(120,70,20,.28)'; g.lineWidth = 1; for (let i = -6; i <= 6; i++) { g.beginPath(); g.moveTo(X(i * 0.0815, 0), Y(0)); g.lineTo(X(i * 0.0815, 20), Y(20)); g.stroke(); }
      if (BW.bump) for (const sd of [-1, 1]) { quad(sd * 0.54, sd * 0.69, 0, 17.9, '#1c1e26'); quad(sd * 0.54, sd * 0.63, 0, 17.9, '#9aa2b0'); quad(sd * 0.575, sd * 0.625, 0, 17.9, '#eef1f6'); for (let z = 0.5; z < 17.9; z += 3) quad(sd * 0.54, sd * 0.645, z, z + 0.16, '#2f333d'); }   // bumpers: the same grey rails with a chrome bar and posts that the TV shows
      g.fillStyle = 'rgba(120,70,20,.55)'; for (let i = -3; i <= 3; i++) { const z = 4.6 + Math.abs(i) * 0.3, x = i * 0.12, w = 0.04 * S(z) * cw * 0.12; g.beginPath(); g.moveTo(X(x, z) - w, Y(z)); g.lineTo(X(x, z) + w, Y(z)); g.lineTo(X(x, z + 0.9), Y(z + 0.9)); g.closePath(); g.fill(); }
      g.fillStyle = '#e03131'; g.fillRect(X(-0.53, 1), Y(1) - 2, X(0.53, 1) - X(-0.53, 1), 3);
      // pins (standing ones)
      const st = BW.st, pr = 10;
      for (let i = pr - 1; i >= 0; i--) if (st.pins & (1 << i)) { const q = BW_PIN(i), px = X(q.x, q.z), py = Y(q.z), s = Math.max(5, S(q.z) * 17);
        g.fillStyle = 'rgba(0,0,0,.28)'; g.beginPath(); g.ellipse(px, py + s * 0.5, s * 0.5, s * 0.22, 0, 0, 7); g.fill();
        g.fillStyle = '#f8f9fa'; g.beginPath(); g.ellipse(px, py, s * 0.42, s * 0.72, 0, 0, 7); g.fill(); g.fillStyle = '#e03131'; g.fillRect(px - s * 0.4, py - s * 0.18, s * 0.8, s * 0.12); }
      // plan: where this ball will go (the dashed path). Your phone and the TV draw the same line.
      const rolling = st.ph === 'roll', bx = rolling ? st.x : BW.x * 0.42;
      if (BW.mine && st.ph === 'aim') {
        g.setLineDash([2, 9]); g.lineCap = 'round'; g.lineWidth = 5; const hot = BW.mode === 'throw';
        for (let z = 0.6; z < 18.6; z += 0.5) { const a = 1 - z / 24; g.strokeStyle = hot ? `rgba(120,255,200,${a})` : `rgba(255,255,255,${a * 0.7})`; g.beginPath(); g.moveTo(X(bwPath(bx, BW.aimN, BW.hookN, z), z), Y(z)); g.lineTo(X(bwPath(bx, BW.aimN, BW.hookN, z + 0.5), z + 0.5), Y(z + 0.5)); g.stroke(); }
        g.setLineDash([]); const ex = X(bwPath(bx, BW.aimN, BW.hookN, 18.3), 18.3), ey = Y(18.3); g.strokeStyle = hot ? '#7dffc8' : '#fff'; g.lineWidth = 3; g.beginPath(); g.arc(ex, ey, 9, 0, 7); g.stroke();
      }
      // the ball
      const bz = rolling || st.ph === 'show' ? Math.max(0.3, st.z) : 0.3, bxx = rolling ? st.x : bx;
      if (!(st.ph === 'show' && bz > 20.5)) { const br = Math.max(5, S(bz) * 20), px = X(bxx, bz), py = Y(bz);
        g.fillStyle = 'rgba(0,0,0,.3)'; g.beginPath(); g.ellipse(px, py + br * 0.7, br * 0.9, br * 0.35, 0, 0, 7); g.fill();
        const bg = g.createRadialGradient(px - br * 0.3, py - br * 0.35, br * 0.1, px, py, br); bg.addColorStop(0, '#9fd3ff'); bg.addColorStop(1, me && me.color ? me.color : '#3b7bff'); g.fillStyle = bg; g.beginPath(); g.arc(px, py, br, 0, 7); g.fill();
        g.fillStyle = '#0a0a14'; for (const o of [[-0.28, -0.35], [0.1, -0.5], [-0.05, -0.15]]) { g.beginPath(); g.arc(px + o[0] * br, py + o[1] * br, br * 0.13, 0, 7); g.fill(); } }
      // prompts
      const pe = $('#bwPrompt'); let pt = '';
      if (!BW.mine) pt = st.ph === 'show' && st.msg ? st.msg : '';
      else if (st.ph === 'roll') pt = '◀  DRAG TO STEER  ▶';
      else if (st.ph === 'show') pt = st.msg || '';
      else if (BW.mode === 'throw') pt = BW.hookN ? (BW.hookN > 0 ? 'HOOK →' : '← HOOK') : 'RELEASE!';
      if (pe && pe.textContent !== pt) { pe.textContent = pt; pe.classList.toggle('bwBig', st.ph === 'show'); }
      const bb = $('#bwBump'); if (bb) { const t = BW.bump ? '🛡 Bumpers ON' : '🛡 Bumpers OFF'; if (bb.textContent !== t) { bb.textContent = t; bb.classList.toggle('on', BW.bump); } bb.disabled = st.ph !== 'aim'; }
      const pw = $('#bwPowI'); if (pw) pw.style.width = Math.round((BW.mode === 'throw' ? BW.powN : (st.ph === 'roll' ? 0 : 0)) * 100) + '%';
      if (!BW.mine) hint(st.ph === 'roll' ? 'Rolling…' : 'Watch the TV — send a reaction 😀');
      else if (st.ph === 'aim') hint(BW.mode === 'throw' ? 'Keep going up — faster = harder' : 'Slide the ball side to side, then FLICK it up. Curve the flick for a hook!');
      else if (st.ph === 'roll') hint('Nudge the ball with your thumb');
      else hint('');
      BW.raf = requestAnimationFrame(draw);
    };
    cancelAnimationFrame(BW.raf); BW.raf = requestAnimationFrame(draw);
    if (!BW.mine) return;
    $('#bwBump').addEventListener('click', () => { if (BW.st.ph !== 'aim') return; BW.bump = !BW.bump; sendMsg({ t: 'act', a: 'bumpers', i: BW.bump ? 1 : 0 }); navigator.vibrate?.(10); });
    // ----- touch -----
    let pid = null, pts = [], sx0 = 0, lastSend = 0;
    wrap.style.touchAction = 'none';
    const pos = e => { const r = cv.getBoundingClientRect(); return { x: e.clientX - r.left, y: e.clientY - r.top, t: performance.now() }; };
    const sendAim = (force) => { const n = performance.now(); if (!force && n - lastSend < 70) return; lastSend = n; sendMsg({ t: 'act', a: 'bowlaim', id: [BW.x, BW.aimN, BW.hookN].map(v => +v.toFixed(3)).join('|'), p: +BW.powN.toFixed(2) }); };
    wrap.addEventListener('pointerdown', e => {
      e.preventDefault(); if (pid != null) return; pid = e.pointerId; try { wrap.setPointerCapture(pid); } catch (_) {}
      const q = pos(e); pts = [q]; sx0 = q.x;
      if (BW.st.ph === 'aim') { BW.mode = 'slide'; BW.x = fromPx(q.x); BW.aimN = 0; BW.hookN = 0; BW.powN = 0; sendAim(true); navigator.vibrate?.(8); }
      else if (BW.st.ph === 'roll') { BW.mode = 'steer'; }
    });
    wrap.addEventListener('pointermove', e => {
      if (e.pointerId !== pid) return; const q = pos(e); pts.push(q); if (pts.length > 80) pts.shift();
      if (BW.mode === 'slide') { const dy = pts[0].y - q.y; if (dy > 22) { BW.mode = 'throw'; navigator.vibrate?.(10); let k = pts.length - 1; while (k > 0 && pts[k].y < pts[0].y - 6) k--; pts = pts.slice(k); } /* the flick starts where the finger started rising */ else { BW.x = fromPx(q.x); sendAim(); } }
      if (BW.mode === 'throw') { const f = bwFit(pts); if (f) { BW.aimN = f.aimN; BW.hookN = f.hookN; BW.powN = f.powN; sendAim(); } }
      if (BW.mode === 'steer') { BW.steer = clamp((q.x - sx0) / (cw * 0.22)); const n = performance.now(); if (n - lastSend > 50) { lastSend = n; sendMsg({ t: 'in', x: +BW.steer.toFixed(2), y: 0, a: false, b: false, touch: true }); } }
    });
    const end = e => {
      if (e.pointerId !== pid) return; pid = null;
      if (BW.mode === 'throw') {
        const f = bwFit(pts);
        if (f && f.dy >= 70 && f.v >= 0.3) {
          BW.aimN = f.aimN; BW.hookN = f.hookN; BW.powN = f.powN; BW.thrown = Date.now();
          sendMsg({ t: 'act', a: 'bowl', id: [BW.x, f.aimN, f.hookN].map(v => +v.toFixed(3)).join('|'), p: +f.powN.toFixed(2) }); navigator.vibrate?.([30, 20, 50]);
        } else { toast('Flick a bit longer and faster! 💨'); BW.mode = null; BW.powN = 0; }
      } else if (BW.mode === 'steer') { BW.steer = 0; sendMsg({ t: 'in', x: 0, y: 0, a: false, b: false, touch: false }); }
      else if (BW.mode === 'slide') sendAim(true);
      if (BW.mode !== 'throw' || !BW.thrown) BW.mode = null;
    };
    wrap.addEventListener('pointerup', end); wrap.addEventListener('pointercancel', end);
  }
  // ---------- 🎾 tennis: the court is on your phone. Watch the ring close, FLICK to hit, pick DRIVE / LOB / DROP ----------
  const TN = { st: null, shot: 'd', raf: 0, trail: null, sw: 0, lv: 0 };
  const TN_LV = ['😊 Easy', '🎾 Normal', '🔥 Hard'];   // the difficulty, shown on the button
  // the court on the phone arrives as snapshots; between them every player and the ball glide to where they are now, so nothing moves in steps
  function tnSm(st) {
    const now = performance.now(), dt = Math.min(0.1, (now - (TN.smT || now)) / 1000); TN.smT = now; const a = 1 - Math.exp(-dt * 26);
    const pl = st.pl || []; if (!TN.sp || TN.sp.length !== pl.length) TN.sp = pl.map(q => q.slice());
    pl.forEach((q, i) => { const d = TN.sp[i]; if (Math.hypot(q[0] - d[0], q[1] - d[1]) > 5) { d[0] = q[0]; d[1] = q[1]; } else { d[0] += (q[0] - d[0]) * a; d[1] += (q[1] - d[1]) * a; } d[2] = q[2]; });
    if (st.b) { if (!TN.sb || !st.live || Math.hypot(st.b[0] - TN.sb[0], st.b[1] - TN.sb[1]) > 7) TN.sb = st.b.slice(); else for (let i = 0; i < 3; i++) TN.sb[i] += (st.b[i] - TN.sb[i]) * a; }
    TN.sk = TN.sk == null ? st.k : TN.sk + (st.k - TN.sk) * a;
    return TN.sp;
  }
  function renderTennis(L) {
    TN.st = null; TN.trail = null; TN.sp = null; TN.sb = null; TN.sk = null; TN.serve = !!L.serve; TN.lv = L.lv | 0;
    stage.innerHTML = `<div class="tnPad"><div class="bwTop"><b>YOUR SHOT</b><button class="bwBump on" id="tnLvl">${TN_LV[TN.lv]}</button><span id="tnScore">${L.serve ? 'You serve' : 'Rally'}</span></div>
      <div class="tnBars"><div class="tnBar"><i id="tnSt"></i><span>💨 STAMINA</span></div><div class="tnBar hv"><i id="tnHv"></i><span id="tnHvT">🔥 POWER</span></div></div>
      <div class="bwWrap" id="tnWrap"><canvas id="tnCv"></canvas><div class="bwPrompt" id="tnPrompt"></div></div>
      <div class="tnAdv" id="tnAdv"></div>
      <div class="tnShots">${[['d', '💥 DRIVE', 'fast · tires them'], ['l', '🌈 LOB', 'beats the net man'], ['p', '🪶 DROP', 'beats the deep one'], ['s', '🛡 SLICE', 'safe · saves breath']].map(s => `<button data-s="${s[0]}" class="${TN.shot === s[0] ? 'on' : ''}"><b>${s[1]}</b><small>${s[2]}</small></button>`).join('')}</div>
      <div class="rhint" id="tnHint"></div></div>`;
    stage.querySelectorAll('.tnShots button').forEach(b => b.addEventListener('pointerdown', e => { e.stopPropagation(); TN.shot = b.dataset.s; stage.querySelectorAll('.tnShots button').forEach(x => x.classList.toggle('on', x === b)); navigator.vibrate?.(10); }));
    $('#tnLvl').addEventListener('click', () => { TN.lv = (TN.lv + 1) % 3; $('#tnLvl').textContent = TN_LV[TN.lv]; sendMsg({ t: 'act', a: 'tlevel', i: TN.lv }); navigator.vibrate?.(10); });   // easy → normal → hard
    const wrap = $('#tnWrap'), cv = $('#tnCv'), g = cv.getContext('2d'); let cw = 0, ch = 0, d = 1;
    const fit = () => { const W = wrap.clientWidth, H = wrap.clientHeight; if (!W || !H) return; d = Math.min(2, devicePixelRatio || 1); cw = W; ch = H; cv.width = Math.round(W * d); cv.height = Math.round(H * d); cv.style.width = W + 'px'; cv.style.height = H + 'px'; };
    fit(); addEventListener('resize', fit);
    // court metres → screen: depth 0 = my baseline (bottom), 23.77 = theirs; a mild perspective
    const HLn = 11.885, DWn = 5.485, HWn = 4.115, SVn = 6.4, DMIN = -4.5, DMAX = 28.3;
    const yB = () => ch - 22, yT = () => 26, Yd = dd => { const t = (dd - DMIN) / (DMAX - DMIN); return yB() - (yB() - yT()) * (1 - Math.pow(1 - t, 1.3)); };
    const HWd = dd => { const k = (yB() - Yd(dd)) / (yB() - yT()); return cw * 0.5 * (0.94 + (0.5 - 0.94) * k); };
    const Xd = (sx, dd) => cw / 2 + (sx / (DWn + 1.6)) * HWd(dd), Sc = dd => HWd(dd) / cw * 2;
    const line = (x0, d0, x1, d1, w = 2.5, col = '#fff') => { g.strokeStyle = col; g.lineWidth = w; g.beginPath(); g.moveTo(Xd(x0, d0), Yd(d0)); g.lineTo(Xd(x1, d1), Yd(d1)); g.stroke(); };
    const poly = (pts, fill) => { g.beginPath(); pts.forEach((q, i) => (i ? g.lineTo(Xd(q[0], q[1]), Yd(q[1])) : g.moveTo(Xd(q[0], q[1]), Yd(q[1])))); g.closePath(); g.fillStyle = fill; g.fill(); };
    const draw = () => {
      if (!document.body.contains(cv)) return; if (!cw || wrap.clientWidth !== cw || wrap.clientHeight !== ch) fit();
      g.setTransform(d, 0, 0, d, 0, 0); g.clearRect(0, 0, cw, ch); g.fillStyle = '#0e4a36'; g.fillRect(0, 0, cw, ch);
      poly([[-DWn - 1.6, DMIN], [DWn + 1.6, DMIN], [DWn + 1.6, DMAX], [-DWn - 1.6, DMAX]], '#1d7a55');
      poly([[-DWn - 0.8, -2.2], [DWn + 0.8, -2.2], [DWn + 0.8, 2 * HLn + 2.2], [-DWn - 0.8, 2 * HLn + 2.2]], '#2b6a52');
      poly([[-DWn, 0], [DWn, 0], [DWn, 2 * HLn], [-DWn, 2 * HLn]], '#2a6fb0');
      const st = TN.st, mineSide = st && st.ph === 'rally' && st.r;
      if (mineSide) poly([[-DWn, 0], [DWn, 0], [DWn, HLn], [-DWn, HLn]], 'rgba(255,255,255,.07)');
      line(-DWn, 0, DWn, 0); line(-DWn, 2 * HLn, DWn, 2 * HLn); for (const x of [-DWn, DWn, -HWn, HWn]) line(x, 0, x, 2 * HLn, 2);
      line(-HWn, HLn - SVn, HWn, HLn - SVn, 2); line(-HWn, HLn + SVn, HWn, HLn + SVn, 2); line(0, HLn - SVn, 0, HLn + SVn, 2);
      // the net
      g.strokeStyle = 'rgba(255,255,255,.85)'; g.lineWidth = 4; const nx0 = Xd(-DWn - 0.6, HLn), nx1 = Xd(DWn + 0.6, HLn), ny = Yd(HLn); g.beginPath(); g.moveTo(nx0, ny - 9); g.lineTo(nx1, ny - 9); g.stroke();
      g.strokeStyle = 'rgba(255,255,255,.35)'; g.lineWidth = 1; for (let x = nx0; x < nx1; x += 7) { g.beginPath(); g.moveTo(x, ny - 9); g.lineTo(x, ny); g.stroke(); } g.fillStyle = '#fff'; g.fillRect(nx0 - 3, ny - 13, 5, 14); g.fillRect(nx1 - 2, ny - 13, 5, 14);
      let prompt = '', hint = '';
      if (!st) { prompt = ''; hint = 'Get ready…'; }
      else {
        if (st.tg && (st.ph === 'serve' || st.ph === 'rally')) st.tg.forEach((q, ti) => {   // the target rings: land the ball in the bright one for a BULLSEYE (2 points)
          const x = Xd(q[0], q[1]), y = Yd(q[1]), r = Math.max(9, Sc(q[1]) * 24), pu = 0.5 + 0.5 * Math.sin(performance.now() / 160 + ti), mine = ti === 0;
          g.save(); g.globalAlpha = mine ? 1 : 0.4; g.fillStyle = `rgba(255,212,59,${0.18 + 0.15 * pu})`; g.beginPath(); g.ellipse(x, y, r, r * 0.5, 0, 0, 7); g.fill(); g.lineWidth = 3; g.strokeStyle = '#fff3bf'; g.stroke();
          g.fillStyle = 'rgba(255,77,109,.75)'; g.beginPath(); g.ellipse(x, y, r * 0.42, r * 0.21, 0, 0, 7); g.fill();
          if (mine) { g.fillStyle = '#fff'; g.font = '800 11px system-ui'; g.textAlign = 'center'; g.fillText('🎯 ×2', x, y - r * 0.62); } g.restore(); });
        const pls = tnSm(st);
        for (const q of pls.slice().sort((a, b) => b[1] - a[1])) { const mine = q[2] === 1, x = Xd(q[0], q[1]), y = Yd(q[1]), r = Math.max(7, Sc(q[1]) * 15);
          g.fillStyle = 'rgba(0,0,0,.3)'; g.beginPath(); g.ellipse(x, y + r * 0.7, r * 0.95, r * 0.38, 0, 0, 7); g.fill();
          g.fillStyle = mine ? (me && me.color) || '#ff4d6d' : '#cfd8ea'; g.beginPath(); g.arc(x, y - r * 0.9, r, 0, 7); g.fill(); g.lineWidth = 3; g.strokeStyle = mine ? '#fff' : 'rgba(0,0,0,.35)'; g.stroke();
          if (mine) { g.fillStyle = '#fff'; g.font = '800 11px system-ui'; g.textAlign = 'center'; g.fillText('YOU', x, y - r * 2.2); } }
        // my timing ring
        const my = pls.find(q => q[2] === 1);
        if (my && st.r && st.live) { const x = Xd(my[0], my[1]), y = Yd(my[1]) - 4, rr = (16 + (TN.sk == null ? st.k : TN.sk) * 78) * Math.max(0.8, Sc(my[1])), col = st.z === 2 ? '#51cf66' : st.z === 1 ? '#ffd43b' : '#ffffff';
          g.lineWidth = 6; g.strokeStyle = col; g.globalAlpha = 0.95; g.beginPath(); g.arc(x, y, rr, 0, 7); g.stroke(); g.globalAlpha = 1; g.lineWidth = 3; g.strokeStyle = 'rgba(255,255,255,.7)'; g.beginPath(); g.arc(x, y, 16 * Math.max(0.8, Sc(my[1])), 0, 7); g.stroke();
          if (st.z === 2) prompt = 'NOW!'; else if (st.z === 1) prompt = 'almost…'; }
        // ball (a shadow on the court, the ball above it)
        if (st.live && st.b) { const B = TN.sb || st.b; const bx = Xd(B[0], B[1]), by = Yd(B[1]), s = Math.max(0.7, Sc(B[1])), h = B[2] * 15 * s, br = Math.max(4.5, 7 * s);
          g.fillStyle = 'rgba(0,0,0,.35)'; g.beginPath(); g.ellipse(bx, by, br * 1.1, br * 0.45, 0, 0, 7); g.fill();
          g.fillStyle = '#e6f76a'; g.beginPath(); g.arc(bx, by - h - br, br, 0, 7); g.fill(); g.strokeStyle = 'rgba(255,255,255,.9)'; g.lineWidth = 1.4; g.beginPath(); g.arc(bx, by - h - br, br * 0.7, 0.5, 2.6); g.stroke(); }
        if (st.ph === 'serve') { if (st.sv) { prompt = 'FLICK UP TO SERVE'; hint = 'Flick up to toss and serve · flick faster = harder'; } else hint = 'Get ready to return'; }
        else if (st.ph === 'rally') hint = st.r ? (st.z === 2 ? 'Flick NOW!' : 'Wait for the ring to close, then flick') : 'Their ball — get ready';
        else if (st.ph === 'point') { prompt = st.msg || ''; hint = ''; }
        else if (st.ph === 'var') { prompt = 'HAWK-EYE'; hint = 'Reviewing the line…'; }
        else if (st.ph === 'end') { prompt = st.msg || 'GAME OVER'; }
        if (hint === '' && st.ph !== 'point' && st.ph !== 'end' && st.ph !== 'var') hint = 'Pick a shot, then flick up — flick faster = harder';
      }
      // the finger's trail
      if (TN.trail) { const a = 1 - (performance.now() - TN.trail.t) / 450; if (a > 0) { g.lineCap = 'round'; g.lineJoin = 'round'; g.lineWidth = 8; g.strokeStyle = `rgba(255,255,255,${a * 0.8})`; g.beginPath(); TN.trail.pts.forEach((q, i) => (i ? g.lineTo(q.x, q.y) : g.moveTo(q.x, q.y))); g.stroke(); } else TN.trail = null; }
      if (st) { const a = $('#tnSt'), h = $('#tnHv'); if (a) { a.style.width = Math.round((st.sa == null ? 1 : st.sa) * 100) + '%'; a.style.background = st.sa > 0.5 ? '#51cf66' : st.sa > 0.25 ? '#ffd43b' : '#ff4d6d'; }
        if (h) { const full = (st.hv || 0) >= 1; h.style.width = Math.round((st.hv || 0) * 100) + '%'; h.parentNode.classList.toggle('full', full); const ht = $('#tnHvT'); if (ht) ht.textContent = full ? '🔥 FIREBALL READY!' : '🔥 POWER'; if (full && st.r) hint = 'POWER FULL — your next hit is an unstoppable FIREBALL!'; } }
      if (st) { const av = $('#tnAdv'); if (av) { const t = st.ph === 'rally' || st.ph === 'serve' ? (st.am || '') : ''; if (av.textContent !== t) av.textContent = t; }
        stage.querySelectorAll('.tnShots button').forEach(b => b.classList.toggle('rec', !!st.ad && b.dataset.s === st.ad && (st.ph === 'rally' || st.ph === 'serve'))); }
      const sc = $('#tnScore'); if (sc) { const t = !st ? '' : st.ph === 'serve' ? (st.sv ? 'You serve' : 'Return') : st.ph === 'rally' ? (st.r ? 'Your shot' : 'Their ball') : st.ph === 'var' ? 'Review' : ''; if (sc.textContent !== t) sc.textContent = t; }
      const pe = $('#tnPrompt'); if (pe && pe.textContent !== prompt) { pe.textContent = prompt; pe.classList.toggle('bwBig', !!prompt && (st && (st.ph === 'point' || st.ph === 'end' || st.z === 2))); }
      const he = $('#tnHint'); if (he && he.textContent !== hint) he.textContent = hint;
      TN.raf = requestAnimationFrame(draw);
    };
    cancelAnimationFrame(TN.raf); TN.raf = requestAnimationFrame(draw);
    // ----- a flick (or just a tap) is a swing -----
    let pid = null, pts = [];
    wrap.style.touchAction = 'none';
    const pos = e => { const r = cv.getBoundingClientRect(); return { x: e.clientX - r.left, y: e.clientY - r.top, t: performance.now() }; };
    wrap.addEventListener('pointerdown', e => { e.preventDefault(); if (pid != null) return; pid = e.pointerId; try { wrap.setPointerCapture(pid); } catch (_) {} pts = [pos(e)]; });
    wrap.addEventListener('pointermove', e => { if (e.pointerId !== pid) return; pts.push(pos(e)); if (pts.length > 60) pts.shift(); TN.trail = { pts: pts.slice(), t: performance.now() + 9999 }; });
    const end = e => {
      if (e.pointerId !== pid) return; pid = null;
      const f = bwFit(pts); let aim = 0, pw = 0.62; const shot = TN.shot;   // the shot is the one you picked: the swipe only gives power and direction
      if (f && f.dy >= 50) { aim = clamp(f.aimN * 1.2); pw = clamp(0.3 + f.powN * 0.7, 0.3, 1); }
      else if (pts.length > 4 && Math.abs(pts[pts.length - 1].x - pts[0].x) > 30) return;   // a sideways scribble isn't a swing
      TN.trail = pts.length > 4 ? { pts: pts.slice(), t: performance.now() } : null;
      sendMsg({ t: 'act', a: 'swing', id: aim.toFixed(2) + '|' + shot, p: +pw.toFixed(2) }); navigator.vibrate?.([20, 15, 30]);
    };
    wrap.addEventListener('pointerup', end); wrap.addEventListener('pointercancel', end);
  }
  // ---------- 🥊 cage fight: a stick to move, buttons to strike, HOLD to block, DODGE; when you are knocked down, TAP to get up ----------
  const FI = { st: null, raf: 0 };
  function renderFight(L) {
    FI.st = null; cancelAnimationFrame(FI.raf);
    stage.innerHTML = `<div class="fiPad">
      <div class="fiTop"><b id="fiOp">vs …</b><span id="fiRd"></span><span id="fiClk"></span></div>
      <div class="fiBar opp"><i id="fiOh"></i><span id="fiOhT"></span></div>
      <div class="fiBar"><i id="fiHp"></i><span id="fiHpT">HEALTH</span></div>
      <div class="fiBar sta"><i id="fiSta"></i><span>STAMINA</span></div>
      <div class="fiDmg"><span>HEAD<i id="fiH"></i></span><span>BODY<i id="fiB"></i></span><span>LEGS<i id="fiL"></i></span></div>
      <div class="fiMsg" id="fiMsg"></div>
      <div class="fiCtl" id="fiCtl">
        <div class="fiStick" id="fiStick"><i id="fiKnob"></i><em>MOVE</em></div>
        <div class="fiBtns" id="fiBtns">
          <button data-a="j" class="a1">👊<b>JAB</b></button><button data-a="c" class="a2">💥<b>CROSS</b></button><button data-a="b" class="a3">🥊<b>BODY</b></button>
          <button data-a="l" class="k1">🦵<b>LOW KICK</b></button><button data-a="h" class="k2">🦶<b>HEAD KICK</b></button><button data-d="1" class="dg">💨<b>DODGE</b></button>
          <button data-blk="1" class="blk">🛡<b>BLOCK</b><small>hold</small></button>
        </div>
        <button class="fiUp" id="fiUp" style="display:none"><span>GET UP!</span><small>tap, tap, tap!</small><u><i id="fiUpBar"></i></u></button>
      </div></div>`;
    const stick = $('#fiStick'), knob = $('#fiKnob'); let sp = null, last = [9, 9], lastT = 0;
    const setMv = (x, y, force) => { const n = performance.now(); if (!force && Math.abs(x - last[0]) < 0.06 && Math.abs(y - last[1]) < 0.06) return; if (!force && n - lastT < 45) return; last = [x, y]; lastT = n; sendMsg({ t: 'act', a: 'mv', id: x.toFixed(2) + '|' + y.toFixed(2) }); };
    const move = e => { const r = stick.getBoundingClientRect(), R0 = r.width / 2; let dx = (e.clientX - (r.left + R0)) / R0, dy = (e.clientY - (r.top + R0)) / R0; const l = Math.hypot(dx, dy); if (l > 1) { dx /= l; dy /= l; }
      knob.style.transform = `translate(${dx * R0 * 0.55}px,${dy * R0 * 0.55}px)`; const dz = v => (Math.abs(v) < 0.18 ? 0 : v); setMv(dz(dx), dz(-dy)); };
    stick.addEventListener('pointerdown', e => { e.preventDefault(); sp = e.pointerId; try { stick.setPointerCapture(sp); } catch (_) {} move(e); });
    stick.addEventListener('pointermove', e => { if (e.pointerId === sp) move(e); });
    const rel = e => { if (e.pointerId !== sp) return; sp = null; knob.style.transform = ''; setMv(0, 0, true); }; stick.addEventListener('pointerup', rel); stick.addEventListener('pointercancel', rel);
    stage.querySelectorAll('.fiBtns button').forEach(b => {
      if (b.dataset.blk) { b.addEventListener('pointerdown', e => { e.preventDefault(); try { b.setPointerCapture(e.pointerId); } catch (_) {} sendMsg({ t: 'act', a: 'blk', id: '1' }); b.classList.add('held'); navigator.vibrate?.(10); });
        const up = () => { sendMsg({ t: 'act', a: 'blk', id: '0' }); b.classList.remove('held'); }; b.addEventListener('pointerup', up); b.addEventListener('pointercancel', up); }
      else b.addEventListener('pointerdown', e => { e.preventDefault(); if (b.dataset.d) sendMsg({ t: 'act', a: 'dg' }); else sendMsg({ t: 'act', a: 'atk', id: b.dataset.a }); navigator.vibrate?.(12); });
    });
    $('#fiUp').addEventListener('pointerdown', e => { e.preventDefault(); sendMsg({ t: 'act', a: 'up' }); navigator.vibrate?.(15); });
    const draw = () => {
      if (!document.getElementById('fiStick')) return; const st = FI.st;
      if (st) {
        const hp = Math.max(0, Math.min(100, st.hp)); $('#fiHp').style.width = hp + '%'; $('#fiHp').style.background = hp > 55 ? '#51cf66' : hp > 28 ? '#ffd43b' : '#ff4d4d'; $('#fiHpT').textContent = '❤️ ' + Math.round(hp);
        $('#fiSta').style.width = Math.max(0, st.sta) + '%'; $('#fiH').style.width = st.h + '%'; $('#fiB').style.width = st.b + '%'; $('#fiL').style.width = st.l + '%';
        $('#fiOp').textContent = 'vs ' + (st.op || ''); const oh = Math.max(0, Math.min(100, st.oh == null ? 100 : st.oh)); $('#fiOh').style.width = oh + '%'; $('#fiOhT').textContent = (st.op || 'Opponent') + '  ' + Math.round(oh); $('#fiRd').textContent = 'ROUND ' + st.rd + '/' + st.rds; const t = st.tm | 0; $('#fiClk').textContent = Math.floor(t / 60) + ':' + String(t % 60).padStart(2, '0');
        const down = st.st === 'down', ko = st.st === 'ko'; $('#fiUp').style.display = down ? '' : 'none'; $('#fiStick').style.display = down ? 'none' : ''; $('#fiBtns').style.display = down ? 'none' : '';
        $('#fiUpBar').style.width = Math.round((st.tap || 0) * 100) + '%';
        $('#fiMsg').textContent = ko ? '💀 K.O.' : down ? 'YOU ARE DOWN!' : st.st === 'rocked' ? '😵 ROCKED! Cover up!' : st.st === 'getup' ? 'Getting up…' : st.ph === 'bell' ? 'Get ready…' : st.ph === 'rest' ? 'Rest in your corner' : st.ph === 'result' ? 'Fight over' : (st.ms || (st.sta < 18 ? 'GASSED: stop swinging, breathe!' : st.bl ? '🩸 You are bleeding' : ''));
        document.querySelector('.fiPad').classList.toggle('hurt', st.hp < 28 || st.st === 'rocked');
      }
      FI.raf = requestAnimationFrame(draw);
    };
    FI.raf = requestAnimationFrame(draw);
  }
  // ---------- the VIP can always end the game from the phone (a TV has no keyboard); it asks first, because it ends it for everyone ----------
  let ROLE = { vip: false, st: 'home' };
  function quitChip() {
    let b = $('#quitBtn'); const on = ROLE.vip && (ROLE.st === 'game' || ROLE.st === 'intro');
    if (!b) { b = document.createElement('button'); b.id = 'quitBtn'; b.className = 'chip quitBtn noRb'; b.setAttribute('aria-label', 'End game'); b.textContent = '✕'; const tb = document.querySelector('#ctrlScr .topbar'); if (tb) tb.appendChild(b); b.addEventListener('click', quitAsk); }
    b.classList.toggle('hidden', !on); if (!on) { const s = $('#quitSheet'); if (s) s.remove(); }
  }
  // The back gesture (iPhone edge swipe, Android back) would drop you out of the party without a word. Once you have joined,
  // the page keeps one extra history step: going back lands on it and asks first.
  let backGuard = false;
  function guardBack() { if (backGuard) return; backGuard = true; try { history.pushState({ cc: 1 }, ''); } catch (e) {} }
  addEventListener('popstate', () => {
    if (!backGuard || !joined) return;
    backGuard = false; guardBack();
    if ($('#leaveSheet')) return; const d = document.createElement('div'); d.id = 'leaveSheet'; d.className = 'sheetBg';
    d.innerHTML = '<div class="sheet"><h3>Leave the party?</h3><p>You will drop out of the game on the TV.</p><button id="lvNo" class="sheetMain">Stay</button><button id="lvYes" class="sheetDanger">Leave</button></div>';
    document.body.appendChild(d); const close = () => d.remove();
    d.addEventListener('click', e => { if (e.target === d) close(); }); $('#lvNo').onclick = close;
    $('#lvYes').onclick = () => { close(); backGuard = false; try { if (conn) conn.close(); } catch (e) {} history.go(-2); };
  });
  function quitAsk() {
    if ($('#quitSheet')) return; const d = document.createElement('div'); d.id = 'quitSheet'; d.className = 'sheetBg';
    d.innerHTML = '<div class="sheet"><h3>End this game?</h3><p>It ends for everyone and goes back to the menu.</p><button id="qsNo" class="sheetMain">Keep playing</button><button id="qsYes" class="sheetDanger">End game</button></div>';
    document.body.appendChild(d); const close = () => d.remove();
    d.addEventListener('click', e => { if (e.target === d) close(); }); $('#qsNo').onclick = close;
    $('#qsYes').onclick = () => { close(); sendMsg({ t: 'nav', d: 'back' }); navigator.vibrate?.(30); };
  }
  function renderMsg(L) { if (L.btns && L.btns.length) { stage.innerHTML = `<div class="center"><div class="bigEm">${icoSplit(L.emoji || '').icon || esc(L.emoji || '📺')}</div><h2>${esc(L.text || '')}</h2>${L.sub ? `<p class="muted">${esc(L.sub)}</p>` : ''}<div class="msgBtns">${L.btns.map(b => `<button data-b="${esc(b.id)}" class="${b.main ? 'main' : ''}">${esc(b.label)}</button>`).join('')}</div></div>`;
      stage.querySelectorAll('.msgBtns button').forEach(b => b.onclick = () => { sendMsg({ t: 'res', a: b.dataset.b }); navigator.vibrate?.(20); stage.querySelectorAll('.msgBtns button').forEach(x => { x.disabled = true; }); }); return; }
    stage.innerHTML = `<div class="center"><div class="bigEm">${icoSplit(L.emoji || '').icon || esc(L.emoji || '📺')}</div><h2>${esc(L.text || 'Look at the TV!')}</h2>${L.sub ? `<p class="muted">${esc(L.sub)}</p>` : ''}</div>`; }
  const COVER = id => id ? `covers/sm/${String(id).replace(/^_/, 'q-')}.jpg` : '';
  const ARROW = { up: 'M6 15l6-6 6 6', down: 'M6 9l6 6 6-6', left: 'M15 6l-6 6 6 6', right: 'M9 6l6 6-6 6' };
  const arrowSvg = d => `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.6" stroke-linecap="round" stroke-linejoin="round"><path d="${ARROW[d]}"/></svg>`;
  function showFocus() {
    const f = vipFocus, card = document.getElementById('nowCard'); if (!card || !f) return;
    const id = f.id || '';
    card.style.backgroundImage = id ? `url(${COVER(id)})` : '';
    card.querySelector('h2').textContent = f.name; card.querySelector('.emo').textContent = id ? '' : f.emoji;
    stage.querySelectorAll('.gtl').forEach(t => t.classList.toggle('cur', t.dataset.g === id));
  }
  function renderLobby(L) {
    const av = me ? esc(me.avatar) : '🎉', col = me ? esc(me.color) : '#3b7bff';
    if (!L.vip) {
      stage.innerHTML = `<div class="lobby"><div class="waitCard">
        <div class="waitAv" style="--c:${col}">${av}</div>
        <h2>You're in, ${esc(me ? me.name : '')}!</h2>
        ${L.trophies ? `<span class="tro">🏆 ${L.trophies} ${L.trophies === 1 ? 'trophy' : 'trophies'}</span>` : ''}
        <p><b>${esc(L.vipName || 'The VIP')}</b> is choosing a game<span class="dots3"><i></i><i></i><i></i></span><br>Watch the TV.</p>
      </div>${camBtnHtml()}</div>`;
      bindCamBtn(); return;
    }
    const shelves = L.cats.map(c => {
      const gs = L.games.filter(gm => gm.cat === c.id); if (!gs.length) return '';
      const nm = c.name.replace(/^\P{L}+/u, '');
      return `<section class="shelf"><h3>${esc(nm)}<small>${gs.length}</small></h3><div class="shelfRow">${gs.map(gm =>
        `<button class="gtl noRb" data-g="${esc(gm.id)}"><i style="background-image:url(${COVER(gm.id)})"></i><b>${esc(gm.name)}</b></button>`).join('')}</div></section>`;
    }).join('');
    stage.innerHTML = `<div class="lobby">
      <div class="nowCard" id="nowCard"><div class="emo"></div><div class="nowTxt"><span class="lbl">👑 On the TV</span><h2>—</h2></div></div>
      <div class="rpadWrap"><div class="rpad">${['up', 'left', 'right', 'down'].map(d => `<button class="noRb" data-d="${d}" aria-label="${d}">${arrowSvg(d)}</button>`).join('')}<button class="ok noRb" data-d="ok">OK</button></div></div>
      <div class="secH">Quick start</div>
      <div class="quick">
        <button class="qcard noRb" id="partyB" style="background-image:url(${COVER('_party')})"><span><b>Party Mode</b><small>6 games in a row</small></span></button>
        <button class="qcard noRb" id="randB" style="background-image:url(${COVER('_random')})"><span><b>Surprise Me</b><small>A random game</small></span></button>
      </div>
      <div class="secH">All games <b>${L.games.length}</b></div>
      ${shelves}
      ${camBtnHtml()}</div>`;
    bindCamBtn(); showFocus();
    stage.querySelectorAll('.rpad button').forEach(b => {
      b.addEventListener('pointerdown', e => { e.preventDefault(); b.classList.add('on'); sendMsg({ t: 'nav', d: b.dataset.d }); navigator.vibrate?.(10); });
      const off = () => b.classList.remove('on'); b.addEventListener('pointerup', off); b.addEventListener('pointercancel', off); b.addEventListener('pointerleave', off);
    });
    $('#partyB').onclick = () => { sendMsg({ t: 'party' }); navigator.vibrate?.(30); }; $('#randB').onclick = () => { sendMsg({ t: 'random' }); navigator.vibrate?.(30); };
    stage.querySelectorAll('.gtl').forEach(b => b.onclick = () => { sendMsg({ t: 'pick', g: b.dataset.g }); navigator.vibrate?.(30); });
  }
  function readyUp() { if (!layout || layout.name !== 'intro' || layout.ready) return; layout.ready = true; sendMsg({ t: 'ready' }); navigator.vibrate?.(30); renderIntro(layout); }
  function renderIntro(L) {
    stage.innerHTML = `<div class="introC"><div class="introHero" style="${L.id ? `background-image:url(${COVER(L.id)})` : ''}">${L.id ? '' : `<div class="emo">${esc(L.emoji)}</div>`}</div>
      <div class="introBody2"><h2>${esc(L.title)}</h2>${L.ctrl ? `<span class="ctrlPill">${esc(L.ctrl)}</span>` : ''}
      <ol class="steps">${(L.how || []).map(h => `<li>${esc(h)}</li>`).join('')}</ol></div>
      <button class="readyBtn ${L.ready ? 'done' : ''}" id="readyB">${L.ready ? '✓ Ready — waiting for others' : "I'm ready"}</button></div>`;
    $('#readyB').onclick = readyUp;
  }
  function renderChoice(L) {
    const multi = L.multi || 0; let taps = 0;
    // phone layout: pick columns from how many options there are and how long the labels are, so words never break
    const opts = L.options || [], n = opts.length, maxLen = Math.max(0, ...opts.map(o => [...String(o.label)].length));
    const shortL = maxLen <= 3;
    const cols = n <= 1 ? 1 : shortL ? (n <= 10 ? 2 : n <= 20 ? 5 : 6) : (n <= 3 ? 1 : 2);
    const rows = Math.ceil(n / cols);
    stage.innerHTML = `<div class="choiceC"><div class="prompt ${L.secret ? 'secret' : ''}">${L.secret && /\n/.test(L.prompt || '') ? L.prompt.split('\n').map((t, i) => i === 1 ? `<b class="sw">${esc(t)}</b>` : `<small>${esc(t)}</small>`).join('') : esc(noEmo(L.prompt || ''))}</div>${multi ? `<div class="multiDots" id="dots">${'○'.repeat(multi)}</div>` : ''}
      <div class="choiceGrid r${Math.min(rows, 5)} ${L.big ? 'cbig' : ''} ${shortL ? 'short' : ''}" style="grid-template-columns:repeat(${cols},1fr);grid-auto-rows:minmax(0,1fr)">${opts.map((o, i) => `<button data-i="${i}" class="${light(o.color) ? 'light' : ''}" style="--bc:${esc(o.color)}" ${o.disabled ? 'disabled' : ''}>${L.letters && !shortL ? `<i class="cLet">${'ABCDEFGHIJ'[i] || ''}</i>` : ''}${icoBtn(o.label)}</button>`).join('')}</div></div>`;
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
    stage.innerHTML = `<div class="diceC"><div class="muted">Round ${L.round}/${L.rounds} · tap the dice you want to keep</div>
      <div class="diceRow">${L.dice.map((v, i) => `<button class="die ${L.held[i] ? 'held' : ''}" data-i="${i}" ${L.done ? 'disabled' : ''}>${PIPS[v]}</button>`).join('')}</div>
      <div class="handName">${esc(L.hand)}</div>
      ${L.done || !L.left ? '' : `<div class="rollHold" id="rollH"></div>`}<div class="diceBtns"><button id="doneB" class="done" ${L.done ? 'disabled' : ''}>${L.done ? '✓ Locked' : '✋ Keep these'}</button></div></div>`;
    stage.querySelectorAll('.die').forEach(b => b.onclick = () => { sendMsg({ t: 'act', a: 'hold', i: +b.dataset.i }); b.classList.toggle('held'); });
    if ($('#rollH')) diceRoller($('#rollH'), () => sendMsg({ t: 'act', a: 'roll' }), 'Tap, flick up or shake', `Re-roll · ${L.left} left`);
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
      <div class="keys" ${IS_PC ? '' : 'hidden'}>PC: ${noStick ? '' : xOnly ? '← → · ' : 'WASD/arrows · '}${L.a ? 'Space = ' + esc(L.a) : ''}${L.b ? ' · Shift = ' + esc(L.b) : ''}</div></div>`;
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
        ${motion ? '<button class="chip on" id="swCenter">⟲ CENTER (hold still, screen to you)</button>' : `<button class="chip on" id="swMotion">📱 Turn on motion controls</button><div class="swHint"><small>No motion sensor? Tap here to swing${IS_PC ? ' · PC: Space + ← →' : ''}</small></div>`}
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
    const isSuit = s => /^[\u2660\u2665\u2666\u2663]/.test(s || ''), deck = (m.cards || []).length > 0 && (m.cards || []).every(c => isSuit(c.icon));
    const cards = deck ? (m.cards || []).map((c, i) => { const red = /[\u2665\u2666]/.test(c.icon), n = m.cards.length, a = (i - (n - 1) / 2) * Math.min(9, 34 / Math.max(1, n - 1 || 1));
        return `<button class="tcard pcard ${red ? 'red' : ''} ${c.ok ? 'ok' : ''}" data-i="${i}" ${c.ok ? '' : 'disabled'} style="--a:${a}deg;--lift:${Math.abs(a) * 1.1}px"><span class="pcC"><b>${esc(c.label)}</b><i>${esc(c.icon)}</i></span><span class="pcBig">${esc(c.icon)}</span><span class="pcC bot"><b>${esc(c.label)}</b><i>${esc(c.icon)}</i></span></button>`; }).join('')
      : (m.cards || []).map((c, i) => `<button class="tcard ${c.ok ? 'ok' : ''}" data-i="${i}" ${c.ok ? '' : 'disabled'} style="--cc:${esc(c.color || '#495057')}">${c.icon ? `<span class="tci">${esc(c.icon)}</span>` : ''}<b>${esc(c.label)}</b>${c.sub ? `<small>${esc(c.sub)}</small>` : ''}</button>`).join('');
    const btns = (m.buttons || []).map(b => `<button class="pbtn2 ${b.big ? 'big' : ''}" data-b="${esc(b.id)}" ${b.off ? 'disabled' : ''} style="--bc:${esc(b.color || '#4dabff')}">${icoBtn(b.label)}</button>`).join('');
    stage.innerHTML = `<div class="gpanel ${m.myTurn ? 'my' : ''}"><div class="gpTop"><b>${esc(m.title || '')}</b><span>${esc(m.info || '')}</span></div>
      ${m.sub ? `<div class="gpSub">${esc(shortHint(m.sub))}</div>` : ''}<div class="gpHand ${deck ? 'fan n' + Math.min(7, (m.cards || []).length) : ''}">${cards}</div><div class="gpBtns">${btns}</div></div>`;
    stage.querySelectorAll('.tcard.ok').forEach(b => flickable(b, () => sendMsg({ t: 'act', a: 'card', i: +b.dataset.i })));
    if (stage.querySelector('.tcard.ok')) stage.querySelector('.gpHand').insertAdjacentHTML('beforebegin', '<div class="rhint">👆 Flick a glowing card up to throw it</div>');
    stage.querySelectorAll('.pbtn2').forEach(b => {
      if (b.dataset.b === 'roll') { const h = document.createElement('div'); h.className = 'rollHold'; b.replaceWith(h); diceRoller(h, () => sendMsg({ t: 'act', a: 'btn', id: 'roll' })); return; }
      b.onclick = () => { sendMsg({ t: 'act', a: 'btn', id: b.dataset.b }); navigator.vibrate?.(25); };
    });
  }
  // ---------- 🎵 song guess: type + pick from live suggestions ----------
  const sNorm = s => String(s).normalize('NFD').replace(/[̀-ͯ]/g, '').toLowerCase().replace(/&/g, 'and').replace(/[^a-z0-9 ]/g, ' ').replace(/\s+/g, ' ').trim();
  let songList = [], songPending = null, songTick = 0, songEnd = 0, songWait = 10;
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
    let root = $('#songUI');
    if (!root || +root.dataset.r !== L.round) {
      clearInterval(songTick); songPending = null;
      stage.innerHTML = `<div class="songUI" id="songUI" data-r="${L.round}">
        <div class="songTop"><b id="sgRound"></b><span id="sgPts"></span></div>
        <div class="songStages" id="sgStages"></div>
        <div class="songStat" id="sgStat"></div>
        <div class="songTimer" id="sgTimer"><i></i></div>
        <div class="songBtns" id="sgBtns"><button id="sgRep">🔁 Replay</button><button id="sgSkip">⏭ Skip · more music</button></div>
        <div class="songIn" id="sgBox"><input id="sgIn" type="search" enterkeyhint="go" autocomplete="off" autocorrect="off" autocapitalize="off" spellcheck="false" placeholder="🔎 Type a song or artist…"><button id="sgX" aria-label="Clear">✕</button></div>
        <div class="songSug" id="sgSug"></div>
        <div class="songTried" id="sgTried"></div>
        <div class="songHost" id="sgHost"></div></div>`;
      root = $('#songUI');
      const inp = $('#sgIn'), sug = $('#sgSug'), round = L.round;
      // a tap = finger down and up in the same place (so scrolling the list never picks a song). Pressing never takes focus away from the text box, so the keyboard stays up and nothing jumps under your finger.
      const tap = (el, fn, sel) => {
        let d = null;
        el.addEventListener('mousedown', e => e.preventDefault());
        el.addEventListener('pointerdown', e => { const b = sel ? e.target.closest(sel) : el; d = b ? { b, x: e.clientX, y: e.clientY, t: Date.now(), id: e.pointerId } : null; });
        el.addEventListener('pointermove', e => { if (d && e.pointerId === d.id && Math.hypot(e.clientX - d.x, e.clientY - d.y) > 12) d = null; });
        el.addEventListener('pointercancel', () => { d = null; });
        el.addEventListener('pointerup', e => { const k = d; d = null; if (!k || e.pointerId !== k.id || Date.now() - k.t > 1200) return; e.preventDefault(); fn(k.b); });
      };
      const pickSong = it => {
        if (!it || !layout || layout.name !== 'song' || layout.state !== 'guess') return;
        sendMsg({ t: 'act', a: 'guess', i: it.i }); navigator.vibrate?.(25);
        songPending = it; inp.value = ''; sug.innerHTML = ''; sug._list = []; inp.blur();
        layout.state = 'sent'; songUpdate(layout);
        setTimeout(() => { if (layout && layout.name === 'song' && layout.state === 'sent' && layout.round === round) { layout.state = 'guess'; songUpdate(layout); } }, 3500);   // never stuck on "checking": if the TV did not answer, you can go on
      };
      const draw = () => {
        const list = songMatches(inp.value);
        sug.innerHTML = list.map((it, k) => `<button data-k="${k}"><b>${esc(it.title)}</b><small>${esc(it.a)}</small></button>`).join('') || (inp.value.trim() ? '<div class="muted sgNone">No match — try the artist name</div>' : '');
        sug._list = list;
      };
      tap(sug, b => pickSong((sug._list || [])[+b.dataset.k]), 'button[data-k]');
      inp.addEventListener('input', draw);
      inp.addEventListener('focus', () => setTimeout(() => { const s = $('#sgStat'); if (s && s.scrollIntoView) s.scrollIntoView({ block: 'start', behavior: 'smooth' }); }, 280));   // lift the answer area above the keyboard
      inp.addEventListener('keydown', e => { if (e.key === 'Enter') { e.preventDefault(); const l = sug._list || []; if (l[0]) pickSong(l[0]); } });
      tap($('#sgX'), () => { inp.value = ''; draw(); inp.focus(); });
      tap($('#sgRep'), () => { sendMsg({ t: 'act', a: 'replay' }); navigator.vibrate?.(15); });
      tap($('#sgSkip'), () => { if (!layout || layout.state !== 'guess') return; sendMsg({ t: 'act', a: 'skip' }); navigator.vibrate?.(15); inp.blur(); layout.state = 'skip'; songUpdate(layout); });
      songTick = setInterval(() => {   // the time left on this clip
        const el = $('#sgTimer'); if (!el) return clearInterval(songTick);
        const left = Math.max(0, songEnd - Date.now()) / 1000, f = Math.min(1, left / (songWait || 10));
        el.firstChild.style.width = (f * 100) + '%'; el.classList.toggle('low', f < 0.3);
      }, 200);
      if (window.visualViewport && !window._songVV) { window._songVV = 1; const set = () => { document.documentElement.style.setProperty('--vvh', visualViewport.height + 'px'); document.body.classList.toggle('vvShort', visualViewport.height < 560); }; visualViewport.addEventListener('resize', set); set(); }
    }
    songUpdate(L);
  }
  function songUpdate(L) {
    const st = L.state, live = st === 'guess';
    if (st !== 'sent') { songEnd = Date.now() + (+L.left || 0) * 1000; songWait = +L.wait || 10; }   // (a local "sent" does not reset the clock)
    $('#sgRound').textContent = L.round ? (L.total ? `🎵 Song ${L.round} / ${L.total}` : `🎵 Song ${L.round} · endless`) : '🎵 Song Guess';
    $('#sgPts').textContent = `${L.pts || 0} pts`;
    $('#sgStages').innerHTML = (L.stages || []).map((s, i) => `<span class="${i < L.stage ? 'past' : i === L.stage && st !== 'reveal' ? 'cur' : ''}">${s}s</span>`).join('');
    const msg = {
      load: '⏳ Next song coming…', guess: L.stage === 0 ? '👂 0.1 seconds… what is it?!' : `👂 Heard ${L.stages[L.stage]}s — what is it?`,
      sent: songPending ? `📨 Checking “${songPending.title}”…` : '📨 Checking…', wrong: '❌ Not that one! More music is coming…', skip: '⏭ Skipped — listen to the next bit',
      right: `✅ YOU GOT IT! ${L.mine || ''}`, reveal: `It was <b>${esc(L.answer || '')}</b><br>${esc(L.mine || '')}`,
    }[st] || '';
    const sEl = $('#sgStat'); sEl.innerHTML = st === 'reveal' ? msg : esc(msg) + (L.hint && st !== 'right' && st !== 'load' ? `<i class="sgHint">💡 ${esc(L.hint)}</i>` : ''); sEl.className = 'songStat ' + st;
    const inp = $('#sgIn'); inp.disabled = !live; if (!live) { inp.value = ''; $('#sgSug').innerHTML = ''; $('#sgSug')._list = []; }
    $('#sgBox').style.display = st === 'right' || st === 'reveal' || st === 'load' ? 'none' : '';
    $('#sgBtns').style.display = st === 'load' || st === 'reveal' ? 'none' : '';
    $('#sgSkip').style.display = live ? '' : 'none'; $('#sgBtns').style.gridTemplateColumns = live ? '' : '1fr';   // you can always hear the clip again; skipping only while you can still guess
    $('#sgTimer').style.display = st === 'guess' || st === 'sent' || st === 'wrong' || st === 'skip' ? '' : 'none';
    // wrong guesses come from the TV, which is the one that judged them: a right guess can never show up crossed out
    const tr = L.tried || [];
    $('#sgTried').innerHTML = tr.length && st !== 'reveal' && st !== 'load' ? tr.map(s => `<div>❌ <s>${esc(s)}</s></div>`).join('') : '';
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
    stage.innerHTML = `<div class="center"><button class="tapBig" id="tap">${icoBtn(L.label || 'TAP!')}</button><p class="muted">${esc(shortHint(L.hint))}</p>${IS_PC ? '<p class="muted" style="font-size:13px">PC: Space</p>' : ''}</div>`;
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
      <div class="legend">Match suit or rank · 8 wild · 2 = +2 · J skip · Q reverse · A = CHAOS${IS_PC ? '<br>PC: ← → select · Enter play · D draw · P pass' : ''}</div>
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

  // ---------- owner cheats: a hidden door (tap your own name 7 times), a code only the owner knows, and a panel. The TV checks the code; nothing here can skip that. ----------
  const OW = { ok: false, taps: 0, t0: 0, pending: '' };
  const ownerKey = () => { try { return localStorage.getItem('cc_owner') || ''; } catch (e) { return ''; } };
  const ownerAuto = () => { const c = ownerKey(); if (c) { OW.pending = ''; sendMsg({ t: 'owner', code: c }); } };
  function ownerPrompt() {
    if ($('#owBox')) return; const d = document.createElement('div'); d.id = 'owBox'; d.className = 'owBox';
    d.innerHTML = '<div class="owCard"><b>👑</b><input id="owIn" type="password" autocomplete="off" autocapitalize="off" spellcheck="false" placeholder="code"><div class="owRow"><button id="owNo">Cancel</button><button id="owGo">Go</button></div></div>';
    document.body.appendChild(d); const inp = $('#owIn'); inp.focus();
    const close = () => d.remove(), go = () => { const v = inp.value.trim(); close(); if (v) { OW.pending = v; sendMsg({ t: 'owner', code: v }); } };
    $('#owNo').onclick = close; $('#owGo').onclick = go; inp.addEventListener('keydown', e => { if (e.key === 'Enter') go(); });
  }
  function ownerShow(on) {
    OW.ok = on; let b = $('#owCrown'); if (!on) { if (b) b.remove(); const p = $('#owPanel'); if (p) p.remove(); return; }
    if (b) return; b = document.createElement('button'); b.id = 'owCrown'; b.className = 'owCrown'; b.textContent = '👑'; b.onclick = ownerPanel; document.body.appendChild(b);
  }
  function ownerPanel() {
    const old = $('#owPanel'); if (old) { old.remove(); return; }
    const gid = window.__gid || '', U = [['win', '🏆 Win this game'], ['end', '⏭ End the game'], ['speed', '🐢 Slow-mo', 0.4], ['speed', '⏱ Normal speed', 1], ['speed', '⚡ Fast', 1.8], ['confetti', '🎉 Confetti']];
    const G = { bowling: [['n:strike', '🎳 Next ball is a strike']], tennis: [['n:point', '🎾 Win this point'], ['n:freeze', '🧊 Freeze the others'], ['n:super', '⚡ Super speed + reach']] }[gid] || [];
    const p = document.createElement('div'); p.id = 'owPanel'; p.className = 'owPanel';
    p.innerHTML = '<div class="owHead"><b>👑 Owner</b><button id="owX">✕</button></div><div class="owGrid">' + G.concat(U).map((c, i) => `<button data-i="${i}">${c[1]}</button>`).join('') + '</div>';
    document.body.appendChild(p); const all = G.concat(U);
    $('#owX').onclick = () => p.remove();
    p.querySelectorAll('.owGrid button').forEach(b => b.onclick = () => { const c = all[+b.dataset.i]; sendMsg({ t: 'cheat', c: c[0], a: c[2] }); navigator.vibrate?.(15); b.classList.add('done'); setTimeout(() => b.classList.remove('done'), 500); });
  }
  $('#meName').addEventListener('pointerdown', () => { const n = Date.now(); if (n - OW.t0 > 3500) OW.taps = 0; if (!OW.taps) OW.t0 = n; if (++OW.taps >= 7) { OW.taps = 0; OW.ok ? ownerPanel() : ownerPrompt(); } });

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
    if (m.t === 'owner') { if (m.ok) { if (OW.pending) { try { localStorage.setItem('cc_owner', OW.pending); } catch (e) {} OW.pending = ''; } ownerShow(true); } else { if (OW.pending) toast(m.wait ? 'Wait ' + m.wait + ' s' : 'Nope'); else { try { localStorage.removeItem('cc_owner'); } catch (e) {} } OW.pending = ''; ownerShow(false); } return; }
    if (m.t === 'welcome') {
      setupReacts(); ownerAuto();
      joined = true; me = m; document.body.style.setProperty('--me', m.color || '#3b7bff'); document.body.classList.add('inGame'); tries = 0; joinTries = 0; netBanner(false); if (camMode) sendMsg({ t: 'camOn' }); setTimeout(motionGate, 50); calButton(); if (cal) sendMsg({ t: 'calib', yaw: Math.round(cal.yaw) }); sendMsg({ t: 'ctrl', mode: ctrlMode }); updateTiltUI();
      $('#joinScr').classList.add('hidden'); $('#ctrlScr').classList.remove('hidden'); guardBack();
      $('#meDot').style.background = m.color; $('#meDot').textContent = m.avatar; $('#meName').textContent = m.name;
      document.body.style.setProperty('--c', m.color); setStatus('🟢 Room ' + m.code);
      if (!layout) renderMsg({ text: 'Connected!' });
    } else if (m.t === 'ch') { applyCh(m.id); window.__gid = m.id; }
    else if (m.t === 'layout') { if (m.role) { ROLE = m.role; quitChip(); } if (m.layout && m.layout.name === 'intro') applyCh(m.layout.id); else if (m.layout && m.layout.name === 'lobby') applyCh(null); render(m.layout); }
    else if (m.t === 'panel') { const was = lastPanel && lastPanel.myTurn; lastPanel = m; if (layout && layout.name === 'panel') { renderPanel(m); updateIdleDock(); } if (m.myTurn && !was) navigator.vibrate?.([60, 40, 60]); }
    else if (m.t === 'hand') { const was = lastHand && lastHand.myTurn; renderHand(m); if (m.myTurn && !was) navigator.vibrate?.([60, 40, 60]); }
    else if (m.t === 'focus') { vipFocus = m; showFocus(); }
    else if (m.t === 'snap') onSnap(m);
    else if (m.t === 'fi') { FI.st = m; }
    else if (m.t === 'tn') { TN.st = m; if (m.lv != null && m.lv !== TN.lv) { TN.lv = m.lv; const lb = $('#tnLvl'); if (lb) lb.textContent = TN_LV[TN.lv]; } }
    else if (m.t === 'bowl') { const s = BW.st; s.ph = m.ph; if (m.x != null) { s.x = +m.x; s.z = +m.z; } if (m.pins != null) s.pins = m.pins | 0; if (m.bump != null) BW.bump = !!m.bump; s.msg = m.msg || ''; if (m.ph !== 'aim') BW.thrown = BW.thrown || 1; if (m.ph === 'aim') { BW.thrown = 0; BW.mode = null; } }
    else if (m.t === 'paint') { if (TM.on) TM.paint = m; }
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
  // keep the phone's screen on while you are in the room: a screen that dims and locks drops you out of the game
  let wake = null; const keepAwake = async () => { try { if (navigator.wakeLock && joined && !document.hidden && !wake) { wake = await navigator.wakeLock.request('screen'); wake.addEventListener('release', () => { wake = null; }); } } catch (e) { wake = null; } };
  document.addEventListener('visibilitychange', () => { keepAwake(); });
  document.addEventListener('pointerdown', () => { keepAwake(); }, { passive: true });
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
