/* Couch Chaos — controller (phone or PC) */
(() => {
  const $ = s => document.querySelector(s);
  const clamp = (v, a = -1, b = 1) => (v < a ? a : v > b ? b : v);
  const stage = $('#stage');

  let cid = store.get('cc_cid');
  if (!cid) { cid = Math.random().toString(36).slice(2, 10); store.set('cc_cid', cid); }
  const qs = new URLSearchParams(location.search);
  $('#roomIn').value = (qs.get('room') || '').toUpperCase().slice(0, 4);
  $('#nameIn').value = store.get('cc_name') || '';
  ($('#roomIn').value ? $('#nameIn') : $('#roomIn')).focus();
  $('#roomIn').addEventListener('input', e => { e.target.value = e.target.value.toUpperCase().replace(/[^A-Z]/g, ''); });
  [$('#roomIn'), $('#nameIn')].forEach(i => i.addEventListener('keydown', e => { if (e.key === 'Enter') $('#joinBtn').click(); }));

  const isIOS = /iPad|iPhone|iPod/.test(navigator.userAgent) || (navigator.platform === 'MacIntel' && navigator.maxTouchPoints > 1);

  let peer = null, conn = null, code = '', name = '', joined = false, layout = null, retryT = null, me = null, lastHand = null;

  function toast(t, ms = 2000) { const el = $('#toast'); el.textContent = t; el.classList.add('show'); clearTimeout(toast.t); toast.t = setTimeout(() => el.classList.remove('show'), ms); }
  function setStatus(t) { $('#status').textContent = t; }
  function joinErr(t) { $('#joinErr').textContent = t; $('#joinBtn').disabled = false; $('#joinBtn').textContent = 'Join the party'; }

  // ---------- motion ----------
  let motionSeen = false, grav = null, neutral = null, tiltOn = store.get('cc_tilt') !== '0', flip = store.get('cc_flip') === '1' ? -1 : 1, lastShake = 0;
  async function askMotion() {
    try {
      if (typeof DeviceMotionEvent !== 'undefined' && typeof DeviceMotionEvent.requestPermission === 'function') {
        const r = await DeviceMotionEvent.requestPermission();
        if (r !== 'granted') return;
      }
      addEventListener('devicemotion', onMotion);
    } catch (e) { /* no motion, keyboard/touch still work */ }
  }
  function onMotion(e) {
    const a = e.accelerationIncludingGravity;
    if (!a || a.x == null) return;
    if (!motionSeen) { motionSeen = true; updateTiltUI(); }
    let ax = a.x, ay = a.y;
    if (isIOS) { ax = -ax; ay = -ay; }
    const ang = (((screen.orientation && screen.orientation.angle) ?? (window.orientation || 0)) % 360 + 360) % 360;
    let ux, uy; // screen coords, y pointing up
    if (ang === 90) { ux = -ay; uy = ax; } else if (ang === 180) { ux = -ax; uy = -ay; } else if (ang === 270) { ux = ay; uy = -ax; } else { ux = ax; uy = ay; }
    grav = { x: ux, y: uy };
    if (!neutral) neutral = { ...grav };
    // shake detection
    const l = e.acceleration;
    const mag = l && l.x != null ? Math.hypot(l.x, l.y, l.z) : Math.abs(Math.hypot(a.x, a.y, a.z) - 9.81);
    const now = performance.now();
    if (layout && layout.name === 'shake' && mag > 13 && now - lastShake > 110) { lastShake = now; pump(); }
  }
  function tiltApplies() { return motionSeen && layout && layout.name === 'pad' && layout.stick; }
  function updateTiltUI() {
    const show = tiltApplies();
    $('#tiltBtn').classList.toggle('hidden', !show);
    $('#centerBtn').classList.toggle('hidden', !show || !tiltOn);
    $('#flipBtn').classList.toggle('hidden', !show || !tiltOn);
    $('#tiltBtn').textContent = tiltOn ? 'Tilt: ON' : 'Tilt: off';
    $('#tiltBtn').classList.toggle('on', tiltOn);
    const tb = document.getElementById('tiltBadge');
    if (tb) tb.classList.toggle('hidden', !(show && tiltOn));
  }
  $('#tiltBtn').onclick = () => { tiltOn = !tiltOn; store.set('cc_tilt', tiltOn ? '1' : '0'); neutral = grav ? { ...grav } : null; updateTiltUI(); };
  $('#centerBtn').onclick = () => { neutral = grav ? { ...grav } : null; toast('Centered! Hold it like this = straight'); };
  $('#flipBtn').onclick = () => { flip = -flip; store.set('cc_flip', flip < 0 ? '1' : '0'); toast(flip < 0 ? 'Tilt direction flipped' : 'Tilt direction normal'); };

  // ---------- input state ----------
  const keys = { left: false, right: false, up: false, down: false, a: false, b: false };
  const btn = { a: false, b: false };
  const stick = { active: false, x: 0, y: 0 };
  const KEYMAP = { ArrowLeft: 'left', KeyA: 'left', ArrowRight: 'right', KeyD: 'right', ArrowUp: 'up', KeyW: 'up', ArrowDown: 'down', KeyS: 'down',
    Space: 'a', KeyJ: 'a', Enter: 'a', ShiftLeft: 'b', ShiftRight: 'b', KeyK: 'b', KeyE: 'b' };

  function readInput() {
    let x = 0, y = 0;
    if (keys.left) x -= 1; if (keys.right) x += 1; if (keys.up) y -= 1; if (keys.down) y += 1;
    if (stick.active) { x = stick.x; y = stick.y; }
    else if (!x && !y && tiltOn && tiltApplies() && grav && neutral) {
      x = clamp(-(grav.x - neutral.x) / 3.5 * flip);
      y = layout.stick === 'xy' ? clamp((grav.y - neutral.y) / 3.5) : 0;
      if (Math.abs(x) < 0.08) x = 0;
      if (Math.abs(y) < 0.1) y = 0;
    }
    if (layout && layout.stick === 'x') y = 0;
    return { x, y, a: keys.a || btn.a, b: keys.b || btn.b };
  }
  let lastSent = { x: 0, y: 0, a: false, b: false };
  function flush(force) {
    if (!conn || !conn.open || !layout || !['pad', 'button'].includes(layout.name)) return;
    const s = readInput();
    if (!force && Math.abs(s.x - lastSent.x) < 0.03 && Math.abs(s.y - lastSent.y) < 0.03 && s.a === lastSent.a && s.b === lastSent.b) return;
    s.x = +s.x.toFixed(2); s.y = +s.y.toFixed(2);
    try { conn.send({ t: 'in', ...s }); } catch (e) {}
    lastSent = s;
    const kn = document.getElementById('kn');
    if (kn) kn.style.transform = `translate(${s.x * (layout.stick === 'x' ? 75 : 40)}px, ${s.y * 40}px)`;
  }
  setInterval(() => flush(false), 33);
  setInterval(() => flush(true), 1000);
  function resetInput() {
    Object.keys(keys).forEach(k => keys[k] = false); btn.a = btn.b = false; stick.active = false; stick.x = stick.y = 0;
    flush(true);
  }

  function sendMsg(m) { if (conn && conn.open) { try { conn.send(m); } catch (e) {} } }
  let pumps = 0;
  function pump() {
    sendMsg({ t: 'pump', n: 1 }); pumps++;
    const c = document.getElementById('shakeN'); if (c) { c.textContent = pumps; c.classList.remove('wiggle'); void c.offsetWidth; c.classList.add('wiggle'); }
  }

  // ---------- keyboard ----------
  let lastShakeKey = null, cardSel = 0;
  addEventListener('keydown', e => {
    if (e.target.tagName === 'INPUT') return;
    if (!layout) return;
    if (layout.name === 'shake') { if (!e.repeat && e.code !== lastShakeKey) { lastShakeKey = e.code; pump(); } e.preventDefault(); return; }
    if (layout.name === 'cards') return cardKey(e);
    if (layout.name === 'lobby' && layout.vip && /^Digit[1-9]$/.test(e.code)) { const gm = layout.games[+e.code.slice(5) - 1]; if (gm) sendMsg({ t: 'pick', g: gm.id }); return; }
    const k = KEYMAP[e.code];
    if (k) { e.preventDefault(); if (!keys[k]) { keys[k] = true; flush(); } }
  });
  addEventListener('keyup', e => { const k = KEYMAP[e.code]; if (k) { keys[k] = false; flush(); } });
  addEventListener('blur', resetInput);

  // ---------- layouts ----------
  function render(L) {
    layout = L; resetInput(); pumps = 0;
    if (L.name !== 'cards') lastHand = null;
    if (L.name === 'lobby') renderLobby(L);
    else if (L.name === 'pad') renderPad(L);
    else if (L.name === 'button') renderButton(L);
    else if (L.name === 'shake') renderShake(L);
    else if (L.name === 'cards') { lastHand ? renderHand(lastHand) : (stage.innerHTML = '<div class="center"><p>Dealing…</p></div>'); }
    else renderMsg(L);
    updateTiltUI();
  }
  function renderMsg(L) {
    stage.innerHTML = `<div class="center"><div class="bigEm">${esc(L.emoji || '📺')}</div><h2>${esc(L.text || 'Look at the TV!')}</h2>${L.sub ? `<p class="muted">${esc(L.sub)}</p>` : ''}</div>`;
  }
  function renderLobby(L) {
    stage.innerHTML = `<div class="center">
      <div class="bigEm">${esc(me ? me.avatar : '🎉')}</div>
      <h2>You're in, ${esc(me ? me.name : '')}!</h2>
      ${L.trophies ? `<p>🏆 ${L.trophies} trophies</p>` : ''}
      ${L.vip ? `<p>👑 You're the <b>VIP</b> — pick a game:</p>
        <div class="gbtns">${L.games.map(gm => `<button class="gbtn" data-g="${esc(gm.id)}"><span class="ge">${gm.emoji}</span><span><b>${esc(gm.name)}</b><small>${esc(gm.desc)}</small></span></button>`).join('')}</div>`
      : `<p class="muted">Waiting for 👑 <b>${esc(L.vipName || 'the VIP')}</b> to pick a game…</p>`}
    </div>`;
    stage.querySelectorAll('.gbtn').forEach(b => b.onclick = () => { sendMsg({ t: 'pick', g: b.dataset.g }); navigator.vibrate?.(30); });
  }
  function bindHold(el, key) {
    const dn = e => { e.preventDefault(); try { el.setPointerCapture(e.pointerId); } catch (_) {} btn[key] = true; el.classList.add('on'); flush(); navigator.vibrate?.(12); };
    const up = () => { if (!btn[key]) return; btn[key] = false; el.classList.remove('on'); flush(); };
    el.addEventListener('pointerdown', dn);
    el.addEventListener('pointerup', up); el.addEventListener('pointercancel', up); el.addEventListener('lostpointercapture', up);
    el.addEventListener('contextmenu', e => e.preventDefault());
  }
  function renderPad(L) {
    const xOnly = L.stick === 'x';
    stage.innerHTML = `<div class="pad">
      <div class="stickZone" id="sz">
        <div class="tiltBadge hidden" id="tiltBadge">📱 Tilt steering ON — tilt like a steering wheel</div>
        <div class="stickBase ${xOnly ? 'x' : ''}"><div class="knob" id="kn"></div></div>
        <div class="szHint">${esc(L.hint || '')}</div>
      </div>
      <div class="btns">
        ${L.b ? `<button class="pbtn pb" data-b="b">${esc(L.b)}</button>` : ''}
        <button class="pbtn pa" data-b="a">${esc(L.a || 'A')}</button>
      </div>
      <div class="keys">PC: ${xOnly ? '← →' : 'WASD / arrows'} · Space = ${esc(L.a || 'A')}${L.b ? ' · Shift = ' + esc(L.b) : ''}</div>
    </div>`;
    stage.querySelectorAll('.pbtn').forEach(b => bindHold(b, b.dataset.b));
    const sz = $('#sz');
    let ox = 0, oy = 0, pid = null;
    sz.addEventListener('pointerdown', e => {
      pid = e.pointerId; try { sz.setPointerCapture(pid); } catch (_) {}
      ox = e.clientX; oy = e.clientY; stick.active = true; stick.x = stick.y = 0; flush();
    });
    sz.addEventListener('pointermove', e => {
      if (e.pointerId !== pid) return;
      let dx = (e.clientX - ox) / 60, dy = (e.clientY - oy) / 60;
      const l = Math.hypot(dx, dy); if (l > 1) { dx /= l; dy /= l; }
      stick.x = dx; stick.y = xOnly ? 0 : dy; flush();
    });
    const end = e => { if (e.pointerId !== pid) return; pid = null; stick.active = false; stick.x = stick.y = 0; flush(); };
    sz.addEventListener('pointerup', end); sz.addEventListener('pointercancel', end);
  }
  function renderButton(L) {
    stage.innerHTML = `<div class="center"><button class="tapBig" id="tap">${esc(L.label || 'TAP!')}</button><p class="muted">${esc(L.hint || '')}</p><p class="muted" style="font-size:13px">PC: Space</p></div>`;
    bindHold($('#tap'), 'a');
  }
  function renderShake(L) {
    stage.innerHTML = `<div class="center">
      <h2>${motionSeen ? '📳 SHAKE YOUR PHONE!' : '⌨️ MASH ← → (alternate)'}</h2>
      <div class="shakeCount" id="shakeN">0</div>
      <button class="tapBig" id="tap" style="font-size:30px">${motionSeen ? 'or TAP here' : 'or TAP here'}</button>
      <p class="muted">${esc(L.hint || '')}</p></div>`;
    $('#tap').addEventListener('pointerdown', e => { e.preventDefault(); pump(); $('#tap').classList.add('on'); setTimeout(() => $('#tap') && $('#tap').classList.remove('on'), 80); });
  }

  // ---------- cards ----------
  const SPECIAL = { '8': 'WILD', '2': '+2', 'J': 'SKIP', 'Q': 'REV', 'A': 'CHAOS' };
  const isRed = c => c && (c.endsWith('♥') || c.endsWith('♦'));
  function cardHTML(c, extra = '', i) {
    const r = c.slice(0, -1), s = c.slice(-1);
    return `<button class="card ${isRed(c) ? 'redc' : ''} ${extra}" ${i != null ? `data-i="${i}"` : ''}><span>${esc(r)}</span><span class="cs">${s}</span>${SPECIAL[r] ? `<span class="ct">${SPECIAL[r]}</span>` : ''}</button>`;
  }
  function renderHand(m) {
    lastHand = m;
    if (!layout || layout.name !== 'cards') return;
    if (cardSel >= m.cards.length) cardSel = Math.max(0, m.cards.length - 1);
    stage.innerHTML = `<div class="cardsUI">
      <div class="cardsTop">
        <div class="turnTxt ${m.myTurn ? 'mine' : ''}">${m.myTurn ? '⭐ YOUR TURN!' : `<span style="color:${esc(m.turnColor)}">${esc(m.turnName)}</span>'s turn…`}</div>
        <div class="topInfo">${cardHTML(m.top, 'mini')}<div class="suitNow">suit<b class="${'♥♦'.includes(m.suit) ? 'red' : 'blk'}">${m.suit}</b></div></div>
      </div>
      ${m.pending ? `<div class="pend">🔥 +${m.pending} stack! Play a 2 to pass it on, or take them</div>` : ''}
      <div class="hand">${m.cards.map((c, i) => cardHTML(c, (m.myTurn ? (m.playable[i] ? 'ok' : 'no') : '') + (i === cardSel && m.myTurn ? ' sel' : ''), i)).join('')}</div>
      <div class="cardBtns">
        <button id="drawB" ${m.canDraw ? '' : 'disabled'}>${m.pending ? `Take +${m.pending}` : '🂠 Draw'}</button>
        <button id="passB" ${m.canPass ? '' : 'disabled'}>Pass ➜</button>
      </div>
      <div class="legend">Match suit or rank · 8 = wild · 2 = +2 · J = skip · Q = reverse · A = CHAOS (all hands pass!)<br>PC: ← → select · Enter play · D draw · P pass</div>
      ${m.needSuit ? `<div class="suitPick"><h2>Pick the new suit</h2><div class="row">${['♠', '♥', '♦', '♣'].map(s => `<button data-s="${s}" class="${'♥♦'.includes(s) ? 'red' : ''}">${s}</button>`).join('')}</div></div>` : ''}
    </div>`;
    stage.querySelectorAll('.hand .card').forEach(b => b.onclick = () => {
      const i = +b.dataset.i;
      if (!m.myTurn) return toast("Not your turn yet");
      if (!m.playable[i]) { b.classList.remove('wiggle'); void b.offsetWidth; b.classList.add('wiggle'); navigator.vibrate?.(40); return; }
      sendMsg({ t: 'act', a: 'play', i });
    });
    const d = $('#drawB'), p = $('#passB');
    if (d) d.onclick = () => sendMsg({ t: 'act', a: 'draw' });
    if (p) p.onclick = () => sendMsg({ t: 'act', a: 'pass' });
    stage.querySelectorAll('.suitPick button').forEach(b => b.onclick = () => sendMsg({ t: 'act', a: 'suit', s: b.dataset.s }));
    stage.querySelectorAll('.card.sel').forEach(b => b.style.outline = '4px solid #4dabff');
  }
  function cardKey(e) {
    const m = lastHand; if (!m || !m.myTurn) return;
    if (m.needSuit) { const map = { Digit1: '♠', Digit2: '♥', Digit3: '♦', Digit4: '♣', KeyS: '♠', KeyH: '♥', KeyD: '♦', KeyC: '♣' }; if (map[e.code]) sendMsg({ t: 'act', a: 'suit', s: map[e.code] }); return; }
    if (e.code === 'ArrowLeft') { cardSel = Math.max(0, cardSel - 1); renderHand(m); }
    else if (e.code === 'ArrowRight') { cardSel = Math.min(m.cards.length - 1, cardSel + 1); renderHand(m); }
    else if (e.code === 'Enter' || e.code === 'Space') { e.preventDefault(); if (m.playable[cardSel]) sendMsg({ t: 'act', a: 'play', i: cardSel }); }
    else if (e.code === 'KeyD') sendMsg({ t: 'act', a: 'draw' });
    else if (e.code === 'KeyP') sendMsg({ t: 'act', a: 'pass' });
  }

  // ---------- connection ----------
  function onData(m) {
    if (!m || typeof m !== 'object') return;
    if (m.t === 'welcome') {
      joined = true; me = m;
      $('#joinScr').classList.add('hidden'); $('#ctrlScr').classList.remove('hidden');
      $('#meDot').style.background = m.color; $('#meName').textContent = `${m.avatar} ${m.name}`;
      document.body.style.setProperty('--c', m.color);
      setStatus('🟢 Room ' + m.code);
      if (!layout) renderMsg({ text: 'Connected!' });
    } else if (m.t === 'layout') render(m.layout);
    else if (m.t === 'hand') { const wasMine = lastHand && lastHand.myTurn; renderHand(m); if (m.myTurn && !wasMine) navigator.vibrate?.([60, 40, 60]); }
    else if (m.t === 'buzz') navigator.vibrate?.(m.ms || 80);
    else if (m.t === 'toast') toast(m.text);
    else if (m.t === 'full') { joinErr('That room is full (8 players).'); }
  }
  function scheduleRetry() {
    clearTimeout(retryT);
    setStatus('🟠 Reconnecting…');
    retryT = setTimeout(connect, 2000);
  }
  function dial() {
    if (conn) { try { conn.close(); } catch (e) {} }
    const c = peer.connect(PREFIX + code, { reliable: true, serialization: 'json' });
    conn = c;
    c.on('open', () => { c.send({ t: 'join', cid, name }); });
    c.on('data', onData);
    c.on('close', () => { if (conn === c) { conn = null; if (joined) scheduleRetry(); } });
    c.on('error', () => { if (conn === c && joined) scheduleRetry(); });
    // if the TV never answers, try again
    setTimeout(() => { if (conn === c && !c.open) { if (joined) scheduleRetry(); else joinErr('Could not reach the TV. Check the code and try again.'); } }, 9000);
  }
  function connect() {
    if (peer && !peer.destroyed && peer.open) return dial();
    if (peer && !peer.destroyed && peer.disconnected) { try { peer.reconnect(); } catch (e) {} return; }
    if (peer && !peer.destroyed) return; // still opening
    peer = new Peer();
    peer.on('open', dial);
    peer.on('disconnected', () => { if (joined) scheduleRetry(); });
    peer.on('error', e => {
      console.warn(e.type, e);
      if (e.type === 'peer-unavailable') { if (joined) scheduleRetry(); else joinErr(`Room ${code} not found. Is the TV page open?`); }
      else if (joined) scheduleRetry();
      else { joinErr('Network problem — try again.'); try { peer.destroy(); } catch (_) {} peer = null; }
    });
  }
  $('#joinBtn').onclick = async () => {
    code = $('#roomIn').value.trim().toUpperCase(); name = $('#nameIn').value.trim().slice(0, 12);
    if (code.length !== 4) return joinErr('Enter the 4-letter code shown on the TV');
    if (!name) return joinErr('Pick a name first');
    store.set('cc_name', name);
    $('#joinBtn').disabled = true; $('#joinBtn').textContent = 'Connecting…'; $('#joinErr').textContent = '';
    await askMotion();
    connect();
  };
  // phones drop connections when the screen sleeps — reconnect when we come back
  document.addEventListener('visibilitychange', () => { if (!document.hidden && joined && (!conn || !conn.open)) connect(); });
  addEventListener('orientationchange', () => { neutral = null; });
})();
