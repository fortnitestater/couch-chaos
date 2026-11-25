/* Couch Chaos — TV host: console home, rooms, intro, party mode, results */
(() => {
  const $ = s => document.querySelector(s);
  const W = 1600, H = 900;
  const COLORS = ['#ff4d6d', '#4dabff', '#ffd43b', '#51cf66', '#cc5de8', '#ff922b', '#22d3ee', '#f783ac'];
  const AVATARS = ['🦊', '🐸', '🐼', '🐯', '🐙', '🦄', '🐵', '🐧'];
  const S = { players: [], state: 'boot', game: null, def: null, code: null, party: null };
  let peer = null, view = { s: 1, ox: 0, oy: 0 }, shakeAmt = 0, timer = null;
  const cv = $('#cv'), g = cv.getContext('2d');

  // ---------- networking ----------
  function makeCode() { let s = ''; for (let i = 0; i < 4; i++) s += CODE_LETTERS[(Math.random() * CODE_LETTERS.length) | 0]; return s; }
  let claimTries = 0;
  function openRoom(preferred) {
    S.code = preferred || makeCode();
    $('#netStatus').textContent = 'Opening room…';
    const me = peer = new Peer(PREFIX + S.code, PEER_OPTS);
    peer.on('open', () => { if (peer !== me) return; claimTries = 0; try { sessionStorage.setItem('cc_room', S.code); } catch (e) {} showJoinInfo(); $('#netStatus').textContent = '🟢 Room open'; });
    peer.on('error', e => {
      console.warn('peer error', e.type);
      if (peer !== me) return;
      if (e.type === 'unavailable-id') { peer._reopening = true; peer.destroy(); if (preferred && claimTries++ < 4) setTimeout(() => openRoom(preferred), 2500); else openRoom(); }
      else if (e.type !== 'peer-unavailable') $('#netStatus').textContent = '🟠 Reconnecting…'; // the watchdog below fixes it
    });
    peer.on('disconnected', () => { if (peer === me) $('#netStatus').textContent = '🟠 Reconnecting…'; });
    peer.on('connection', conn => {
      conn.on('data', m => { try { onMsg(conn, m); } catch (err) { console.error(err); } });
      conn.on('close', () => onClose(conn)); conn.on('error', () => onClose(conn));
    });
  }
  // Watchdog: keep the room alive. Lost the signalling server -> reconnect; peer destroyed -> reopen the SAME code
  // so phones (which keep retrying that code) find the TV again by themselves.
  setInterval(() => {
    if (!peer || S.state === 'boot') return;
    if (peer.destroyed) { if (!peer._reopening) { peer._reopening = true; openRoom(S.code); } }
    else if (peer.disconnected) { try { peer.reconnect(); } catch (e) {} }
    else if (peer.open) $('#netStatus').textContent = '🟢 Room open';
  }, 3000);
  // Heartbeat: ping every phone; a phone that answered before but went silent is dropped so it can rejoin cleanly
  setInterval(() => {
    const now = Date.now();
    for (const p of S.players) {
      if (!p.connected || !p.conn) continue;
      send(p, { t: 'ping', ts: Date.now() });
      if (p.pongAt && now - p.lastSeen > 12000) { const c = p.conn; onClose(c); try { c.close(); } catch (e) {} }
    }
  }, 2000);
  const joinUrl = () => new URL('play.html?room=' + S.code, location.href).href;
  function showJoinInfo() {
    $('#code').textContent = S.code; $('#codeSmall').textContent = S.code;
    $('#joinUrl').textContent = joinUrl().replace(/^https?:\/\//, '');
    const q = $('#qr'); q.innerHTML = '';
    if (window.QRCode) { const qc = new QRCode(q, { text: joinUrl(), width: 256, height: 256, colorDark: '#0a1024', colorLight: '#ffffff' });
      try { const m = qc._oQRCode, n = m.getModuleCount(); let bits = ''; for (let r = 0; r < n; r++) for (let c = 0; c < n; c++) bits += m.isDark(r, c) ? '1' : '0'; S.qr = { n, m: bits }; } catch (e) {} }
  }
  function send(p, m) { if (p && !p.bot && p.conn && p.conn.open) { try { p.conn.send(m); } catch (e) {} } }
  // players = connected phones + people seen by the AI camera (the camera phone itself is not a player)
  const humans = () => S.players.filter(p => p.connected && !p.isCam);
  const vip = () => humans().find(p => !p.cam) || null;
  const CAM_OK = ['swing', 'move', 'shake', 'touch']; // body-tracked players join motion games only
  const camFits = () => !S.def || CAM_OK.includes(S.def.ctrl);

  function onMsg(conn, m) {
    if (!m || typeof m !== 'object') return;
    if (m.t === 'join') return onJoin(conn, m);
    const p = S.players.find(q => q.conn === conn); if (!p) return;
    p.lastSeen = Date.now(); if (m.t === 'pong') { p.pongAt = p.lastSeen; if (m.ts) { const r = Math.min(600, Date.now() - m.ts); p.rtt = p.rtt ? p.rtt * 0.7 + r * 0.3 : r; } return; } // round-trip time → latency compensation
    const isVip = p === vip() || (p.isCam && !vip());
    switch (m.t) {
      case 'in': {
        const prev = p.input; p.input = { x: +m.x || 0, y: +m.y || 0, a: !!m.a, b: !!m.b, touch: !!m.touch };
        if (S.state === 'game' && S.game && S.game.onButton) { if (!prev.a && p.input.a) S.game.onButton(p, 'a'); if (!prev.b && p.input.b) S.game.onButton(p, 'b'); }
        break;
      }
      case 'act': if (S.state === 'game' && S.game && S.game.onAct) S.game.onAct(p, m); break;
      case 'pump': if (S.state === 'game' && S.game && S.game.onPump) S.game.onPump(p, +m.n || 1); break;
      case 'ready': if (S.state === 'intro') { S.ready.add(p); renderReady(); if (humans().filter(h => !h.cam).every(h => S.ready.has(h))) launchSoon(0.8); } break;
      case 'camOn': p.isCam = true; p.input = { x: 0, y: 0, a: false, b: false }; banner('📷 AI camera is ON — step into view!', 2200, '#a5d8ff'); renderPlayers(); if (S.state === 'home') sendLobby(); break;
      case 'camOff': p.isCam = false; camDrop(); renderPlayers(); if (S.state === 'home') sendLobby(); break;
      case 'cam': if (p.isCam && Array.isArray(m.ppl)) camFrame(m.ppl); break;
      case 'camSwing': { const cp = S.cam && S.cam[m.s]; if (cp && cp.connected && S.state === 'game' && S.game && S.game.onAct && S.game.ps && S.game.ps.includes(cp)) S.game.onAct(cp, { a: 'swing', p: +m.p || 0.6, aim: typeof m.x === 'number' ? m.x : undefined, kmh: +m.kmh || undefined, lag: +m.lag || 0, spin: typeof m.spin === 'number' ? m.spin : undefined }); if (cp) cp.lastKmh = +m.kmh || 0; if (cp) cp.swingFx = performance.now(); break; }
      case 'camPump': { const cp = S.cam && S.cam[m.s]; if (cp && S.state === 'game' && S.game && S.game.onPump) S.game.onPump(cp, 1); break; }
      case 'pick': if (isVip && S.state === 'home') { const d = GAMES.find(x => x.id === m.g); if (d) { focusGame(d.id); startIntro(d); } } break;
      case 'party': if (isVip && S.state === 'home') startParty(); break;
      case 'random': if (isVip && S.state === 'home') surprise(); break;
      case 'nav': if (isVip) nav(m.d); break;
      case 'or': if (Array.isArray(m.q) && m.q.length === 4) { p.or = m.q; p.orT = Date.now(); } break;
      case 'ctrl': p.ctrl = m.mode === 'motion' ? 'motion' : 'stick'; break;
      case 'calib': p.calYaw = clamp(+m.yaw || 35, 15, 80); break;
    }
  }
  function onJoin(conn, m) {
    const name = String(m.name || 'Player').slice(0, 12).trim() || 'Player';
    let p = S.players.find(q => q.cid === m.cid);
    if (p) { if (p.conn && p.conn !== conn) { const old = p.conn; p.conn = null; try { old.close(); } catch (e) {} } p.conn = conn; p.connected = true; p.name = name; }
    else {
      if (humans().length >= 8) { try { conn.send({ t: 'full' }); } catch (e) {} return; }
      if (S.players.length >= 8) S.players.splice(S.players.findIndex(q => !q.connected), 1);
      const used = new Set(S.players.map(q => q.color)), idx = COLORS.findIndex(c => !used.has(c));
      p = { cid: m.cid, name, lastSeen: Date.now(), color: COLORS[idx], avatar: AVATARS[idx], conn, connected: true, trophies: 0, input: { x: 0, y: 0, a: false, b: false }, layout: null };
      S.players.push(p); SFX.join(); toastTV(`${p.avatar} ${p.name} joined!`);
    }
    p.lastSeen = Date.now();
    send(p, { t: 'welcome', name: p.name, color: p.color, avatar: p.avatar, code: S.code });
    if (S.state === 'home') { renderPlayers(); sendLobby(); }
    else if (S.state === 'intro') { sendIntro(p); renderReady(); }
    else if (S.state === 'game' && S.game) {
      if (S.game.onJoin && S.game.onJoin(p)) { /* joined mid-game */ }
      else if (p.layout && p.inGame) { send(p, { t: 'layout', layout: p.layout }); if (S.game.resync) S.game.resync(p); }
      else send(p, { t: 'layout', layout: { name: 'msg', emoji: '🍿', text: 'Game in progress', sub: "Watch the TV — you're in the next one!" } });
    } else send(p, { t: 'layout', layout: { name: 'msg', emoji: '🏁', text: 'Results!', sub: 'Look at the TV' } });
  }
  // ---------- AI camera players ----------
  const CAM_AV = ['🕺', '💃', '🤸', '🏃'];
  function camFrame(ppl) {
    S.cam = S.cam || {}; S.camAt = performance.now(); let changed = false;
    for (const q of ppl) {
      let cp = S.cam[q.s];
      if (!cp) {
        if (S.players.filter(x => x.connected && !x.isCam).length >= 8) continue;
        const used = new Set(S.players.filter(x => x.connected).map(x => x.color)), idx = COLORS.findIndex(c => !used.has(c));
        cp = S.cam[q.s] = { cid: 'cam' + q.s, name: `Cam ${q.s + 1}`, color: COLORS[Math.max(0, idx)], avatar: CAM_AV[q.s % 4], cam: true, conn: null, connected: true, trophies: 0, input: { x: 0, y: 0, a: false, b: false } };
        S.players.push(cp); changed = true; banner(`${cp.avatar} ${cp.name} joined (camera)`, 1400, cp.color);
      }
      if (!cp.connected) { cp.connected = true; changed = true; }
      cp.seen = performance.now(); cp.k = q.k;
      const prevA = cp.input.a;
      cp.input = { x: (S.def && S.def.ctrl === 'swing') ? +q.x || 0 : +q.l || 0, y: 0, a: !!q.u, b: false };
      if (S.state === 'game' && S.game && S.game.onButton && S.game.ps && S.game.ps.includes(cp) && !prevA && cp.input.a) S.game.onButton(cp, 'a');
    }
    if (changed) { renderPlayers(); if (S.state === 'intro') renderReady(); }
  }
  function camDrop() { for (const k in (S.cam || {})) S.cam[k].connected = false; }
  setInterval(() => { // people who left the camera view for 6 s leave the game
    let ch = false; const now = performance.now();
    for (const k in (S.cam || {})) { const cp = S.cam[k]; if (cp.connected && now - cp.seen > 6000) { cp.connected = false; ch = true; } }
    if (ch) renderPlayers();
  }, 1000);
  // live skeleton picture-in-picture so players can see the camera tracking them
  function drawCamView() {
    const cv = document.getElementById('camView'); if (!cv) return;
    const on = S.camAt && performance.now() - S.camAt < 3000 && S.state !== 'boot'; cv.classList.toggle('hidden', !on); if (!on) return;
    const g2 = cv.getContext('2d'), W2 = cv.width, H2 = cv.height; g2.clearRect(0, 0, W2, H2);
    g2.fillStyle = 'rgba(8,5,25,.78)'; g2.fillRect(0, 0, W2, H2); g2.lineCap = 'round';
    const E = [[1, 2], [1, 3], [3, 5], [2, 4], [4, 6], [1, 7], [2, 8], [7, 8], [7, 9], [9, 11], [8, 10], [10, 12]];
    for (const k in (S.cam || {})) {
      const cp = S.cam[k]; if (!cp.connected || !cp.k || performance.now() - cp.seen > 800) continue;
      const P = i => ({ x: (1 - cp.k[i * 2]) * W2, y: cp.k[i * 2 + 1] * H2 }); // mirrored, like looking in a mirror
      const hot = performance.now() - (cp.swingFx || 0) < 300;
      g2.strokeStyle = cp.color; g2.lineWidth = hot ? 9 : 5;
      for (const [a, b] of E) { const A = P(a), B = P(b); g2.beginPath(); g2.moveTo(A.x, A.y); g2.lineTo(B.x, B.y); g2.stroke(); }
      const n = P(0); g2.fillStyle = cp.color; g2.beginPath(); g2.arc(n.x, n.y, 10, 0, TAU); g2.fill();
      g2.font = '900 20px Nunito, sans-serif'; g2.textAlign = 'center'; g2.fillStyle = '#fff'; g2.fillText(cp.name + (hot ? ' 💥' : ''), n.x, n.y - 18);
    }
    g2.font = '16px Bungee, Impact, sans-serif'; g2.textAlign = 'left'; g2.fillStyle = '#ffd43b'; g2.fillText('📷 AI CAMERA', 10, 22);
  }
  setInterval(drawCamView, 66);
  function onClose(conn) {
    const p = S.players.find(q => q.conn === conn); if (!p) return;
    if (p.isCam) { p.isCam = false; camDrop(); }
    p.connected = false; p.conn = null; p.input = { x: 0, y: 0, a: false, b: false };
    if (S.state === 'home') { renderPlayers(); sendLobby(); }
  }

  // ---------- home screen ----------
  const QUICK = [
    { id: '_party', name: 'Party Mode', emoji: '🎉', art: ['#ffd43b', '#ff4d6d'], desc: '6 random games in a row, one from every category. Trophies decide the champion!', how: [], ctrl: 'tap', players: '1-8', mins: 15, cat: 'quick' },
    { id: '_random', name: 'Surprise Me', emoji: '🎲', art: ['#63e6be', '#7048e8'], desc: 'Let fate pick a game that fits how many of you are playing.', how: [], ctrl: 'tap', players: '1-8', mins: 3, cat: 'quick' },
  ];
  const colMemo = {};
  let rowsData = [], focus = { r: 0, c: 0 }, heroDef = null, idleT = 0, heroCycle = 0;
  function buildRows() {
    const SHORT = { classic: 'Classics', sports: 'Sports', winter: 'Winter', table: 'Cards & Board', party: 'Party', brain: 'Brain', action: 'Action' };
    rowsData = [{ name: '🔥 Quick Play', short: 'Quick Play', items: QUICK.concat(recommended()) }, ...CATEGORIES.map(c => ({ name: c.name, short: SHORT[c.id] || c.name, items: GAMES.filter(gm => gm.cat === c.id) }))];
    $('#rows').innerHTML = rowsData.map((row, r) => `<div class="row"><h3>${row.name}</h3><div class="track">${row.items.map((gm, c) =>
      `<button class="tile ${gm.id[0] === '_' ? 'special' : ''}" data-r="${r}" data-c="${c}" style="--a1:${gm.art[0]};--a2:${gm.art[1]}"><span class="tc">${CTRL_LABEL[gm.ctrl] || ''}</span><span class="te">${gm.emoji}</span><span class="tn">${esc(gm.name)}</span></button>`).join('')}</div></div>`).join('');
    $('#rows').querySelectorAll('.tile').forEach(t => {
      t.onclick = () => { focus = { r: +t.dataset.r, c: +t.dataset.c }; applyFocus(); activate(); };
      t.onmouseenter = () => { focus = { r: +t.dataset.r, c: +t.dataset.c }; applyFocus(true); };
    });
    focus.r = Math.min(focus.r, rowsData.length - 1); focus.c = Math.min(focus.c, rowsData[focus.r].items.length - 1);
    applyFocus(true);
  }
  function recommended() {
    const n = Math.max(1, humans().length);
    const fits = GAMES.filter(gm => { const [a, b] = gm.players.split('-').map(Number); return n >= a && n <= b; });
    const pref = n >= 3 ? ['elf', 'likely', 'bingo', 'snowball', 'cards', 'bomb'] : n === 2 ? ['four', 'hockey', 'dice', 'trails', 'curling', 'memory'] : ['ski', 'bingo', 'trivia', 'sky', 'copycat', 'bomb'];
    return pref.map(id => fits.find(x => x.id === id)).filter(Boolean).slice(0, 4).map(gm => Object.assign({}, gm, { _rec: true }));
  }
  const focusedDef = () => rowsData[focus.r] && rowsData[focus.r].items[focus.c];
  function applyFocus(noScroll) {
    $('#rows').querySelectorAll('.tile.focus').forEach(t => t.classList.remove('focus'));
    const el = $(`#rows .tile[data-r="${focus.r}"][data-c="${focus.c}"]`);
    if (el) { el.classList.add('focus'); if (!noScroll) el.scrollIntoView({ block: 'nearest', inline: 'center', behavior: 'smooth' }); }
    setHero(focusedDef());
    const v = vip(), d = focusedDef(); if (v && d) send(v, { t: 'focus', name: d.name, emoji: d.emoji });
  }
  function setHero(d) {
    if (!d || d === heroDef) return; heroDef = d;
    const hero = $('#hero'); hero.style.setProperty('--a1', d.art[0]); hero.style.setProperty('--a2', d.art[1]);
    $('#heroEmoji').textContent = d.emoji; $('#heroTitle').textContent = d.name; $('#heroDesc').textContent = d.desc;
    $('#heroCat').textContent = d.cat === 'quick' ? 'Quick Play' : (CATEGORIES.find(c => c.id === d.cat) || {}).name || '';
    $('#heroCtrl').textContent = CTRL_LABEL[d.ctrl]; $('#heroPlayers').textContent = `👥 ${d.players} players`; $('#heroMins').textContent = `⏱️ ~${d.mins} min`;
    const em = $('#heroEmoji'); em.style.animation = 'none'; void em.offsetWidth; em.style.animation = '';
  }
  function nav(d) {
    idleT = 0;
    if (S.state === 'intro') { if (d === 'ok') launchSoon(0.2); else if (d === 'back') toHome(); return; }
    if (S.state === 'results') { if (d === 'ok') { clearTimeout(timer); afterResults(); } return; }
    if (S.state === 'game') { if (d === 'back') toHome(); return; }
    if (S.state !== 'home') return;
    if (d === 'left') focus.c = Math.max(0, focus.c - 1);
    else if (d === 'right') focus.c = Math.min(rowsData[focus.r].items.length - 1, focus.c + 1);
    else if (d === 'up' || d === 'down') { colMemo[focus.r] = focus.c; focus.r = d === 'up' ? Math.max(0, focus.r - 1) : Math.min(rowsData.length - 1, focus.r + 1); focus.c = Math.min(colMemo[focus.r] || 0, rowsData[focus.r].items.length - 1); }
    else if (d === 'ok') return activate();
    SFX.blip(); applyFocus();
  }
  function activate() {
    const d = focusedDef(); if (!d) return;
    if (d.id === '_party') startParty(); else if (d.id === '_random') surprise(); else startIntro(GAMES.find(x => x.id === d.id));
  }
  function focusGame(id) { rowsData.forEach((row, r) => row.items.forEach((it, c) => { if (it.id === id && r > 0) focus = { r, c }; })); applyFocus(); }
  function fitting() { const n = Math.max(1, humans().length); return GAMES.filter(gm => { const [a, b] = gm.players.split('-').map(Number); return n >= a - 1 && n <= b; }); }
  function surprise() { const d = pick(fitting()); focusGame(d.id); SFX.whoosh(); setTimeout(() => startIntro(d), 600); }
  function startParty() {
    if (!humans().length) return banner('Join first! Scan the code 📱', 1600, '#ff4d6d');
    const fit = fitting(), list = [];
    for (const c of shuffle(CATEGORIES.slice())) { const opts = fit.filter(gm => gm.cat === c.id && !list.includes(gm)); if (opts.length) list.push(pick(opts)); }
    S.party = { list: list.slice(0, 6), i: 0, pts: new Map(humans().map(p => [p, 0])) };
    banner('🎉 PARTY MODE!', 1600); SFX.win();
    setTimeout(() => startIntro(S.party.list[0]), 1200);
  }
  function renderPlayers() {
    const v = vip();
    $('#avatars').innerHTML = S.players.map(p => `<div class="av2 ${p.connected ? '' : 'away'}" style="--c:${p.color}" title="${esc(p.name)}${p.isCam ? ' (AI camera)' : ''}">${p === v ? '<span class="crown">👑</span>' : ''}${p.isCam ? '📷' : p.avatar}</div>`).join('');
    if (S.state === 'home') buildRows();
  }
  function sendLobby() {
    const v = vip();
    const games = GAMES.map(({ id, name, emoji, cat, ctrl }) => ({ id, name, emoji, cat, ctrl })), cats = CATEGORIES;
    for (const p of humans()) { p.layout = { name: 'lobby', vip: p === v, vipName: v ? v.name : '', games, cats, trophies: p.trophies, party: !!S.party }; p.inGame = false; send(p, { t: 'layout', layout: p.layout }); }
    if (v && focusedDef()) send(v, { t: 'focus', name: focusedDef().name, emoji: focusedDef().emoji });
  }
  function show(which) {
    $('#home').classList.toggle('hidden', which !== 'home');
    $('#intro').classList.toggle('hidden', which !== 'intro');
    $('#cv').classList.toggle('hidden', which !== 'game' && which !== 'results');
    $('#results').classList.toggle('hidden', which !== 'results');
  }

  // ---------- banner / toast ----------
  function banner(text, ms = 1600, color = '#ffd43b') {
    const b = $('#banner'); b.textContent = text; b.style.color = color; b.style.setProperty('--d', ms + 'ms');
    b.style.fontSize = text.length > 22 ? '5vh' : text.length > 14 ? '6.5vh' : '9vh';
    b.classList.remove('show'); void b.offsetWidth; b.classList.add('show');
  }
  function toastTV(t) { if (S.state === 'game') return; banner(t, 1400, '#a5d8ff'); }

  // ---------- intro (how to play) ----------
  let launchT = null, introLeft = 0;
  function startIntro(def) {
    if (!def || (S.state !== 'home' && S.state !== 'results')) return;
    if (!humans().length) return banner('Join first! Scan the code 📱', 1600, '#ff4d6d');
    SFX.init(); S.state = 'intro'; S.def = def; S.ready = new Set(); introLeft = 15;
    const ia = $('#introArt'); ia.style.setProperty('--a1', def.art[0]); ia.style.setProperty('--a2', def.art[1]);
    $('#introEmoji').textContent = def.emoji; $('#introTitle').textContent = def.name;
    $('#introCat').textContent = (S.party ? `🎉 Party game ${S.party.i + 1} of ${S.party.list.length} · ` : '') + ((CATEGORIES.find(c => c.id === def.cat) || {}).name || '');
    $('#introCtrl').textContent = CTRL_LABEL[def.ctrl];
    $('#introHow').innerHTML = def.how.map(h => `<li>${esc(h)}</li>`).join('');
    show('intro'); renderReady(); SFX.whoosh();
    for (const p of humans()) sendIntro(p);
    clearTimeout(launchT); launchT = null;
  }
  function sendIntro(p) { const d = S.def; p.layout = { name: 'intro', emoji: d.emoji, title: d.name, how: d.how, ctrl: CTRL_LABEL[d.ctrl], ready: S.ready.has(p) }; send(p, { t: 'layout', layout: p.layout }); }
  function renderReady() { $('#introReady').innerHTML = humans().filter(p => !p.cam || camFits()).map(p => p.cam ? `<span class="rchip ok" style="--c:${p.color}">📷 ${p.avatar} ${esc(p.name)}</span>` : `<span class="rchip ${S.ready.has(p) ? 'ok' : ''}" style="--c:${p.color}">${S.ready.has(p) ? '✓ ' : ''}${p.avatar} ${esc(p.name)}</span>`).join(''); }
  function launchSoon(s) { if (launchT) return; launchT = setTimeout(() => { launchT = null; launch(); }, s * 1000); }
  // big diagonal colour wipe between screens
  function wipe(c1, c2) {
    let w = document.getElementById('wipe');
    if (!w) { w = document.createElement('div'); w.id = 'wipe'; w.innerHTML = '<i></i><i></i><i></i><b></b>'; document.body.appendChild(w); }
    w.style.setProperty('--a1', c1); w.style.setProperty('--a2', c2);
    w.querySelector('b').textContent = S.def ? S.def.emoji : '';
    w.classList.remove('go'); void w.offsetWidth; w.classList.add('go');
  }
  function confettiDom(n = 70) {
    const box = document.getElementById('results'); if (!box) return;
    box.querySelectorAll('.cf').forEach(e => e.remove());
    const cols = ['#ff4d6d', '#ffd43b', '#4dabff', '#51cf66', '#cc5de8', '#ff922b'];
    for (let i = 0; i < n; i++) { const e = document.createElement('i'); e.className = 'cf'; e.style.cssText = `left:${Math.random() * 100}%;background:${cols[i % cols.length]};animation-delay:${Math.random() * 1.6}s;animation-duration:${2.4 + Math.random() * 2}s;--r:${Math.random() * 720 - 360}deg`; box.appendChild(e); }
  }
  function launch() {
    if (S.state !== 'intro') return;
    wipe(S.def.art[0], S.def.art[1]); SFX.whoosh(); SFX.musicOff && SFX.musicOff();
    // in-game soundtrack by kind of game (sports have the crowd; Song Guess and Musical Chairs bring their own music)
    const TR = { table: 'lounge', winter: 'winter', classic: 'party', party: 'party', action: 'party', brain: 'lounge' };
    const tr = S.def.id === 'song' || S.def.id === 'chairs' ? null : TR[S.def.cat];
    if (SFX.setTrack) { SFX.setTrack(tr || (S.def.id === 'chairs' ? 'party' : 'menu')); if (tr) setTimeout(() => S.state === 'game' && SFX.musicOn(), 900); }
    S.state = 'game'; show('game');
    try { S.game = S.def.make(makeCtx()); S.game.start(); }
    catch (e) { console.error(e); banner('Oops — that game crashed', 2000, '#ff4d6d'); toHome(); }
  }

  // ---------- game context ----------
  const BOT_NAMES = ['Robo Rita', 'Bot Bob', 'Capt. Clank', 'Sir Beeps', 'Gizmo Gran'];
  function makeCtx() {
    return {
      W, H, sfx: SFX, banner, humans,
      roster(min) {
        const list = humans().filter(p => !p.cam || camFits()); let i = 0;
        while (list.length < min) { const used = new Set(list.map(p => p.color)); list.push({ cid: 'bot' + i, name: BOT_NAMES[i % BOT_NAMES.length], color: COLORS.find(c => !used.has(c)) || '#aaa', avatar: '🤖', bot: true, input: { x: 0, y: 0, a: false, b: false } }); i++; }
        return list;
      },
      setLayout(target, layout) { const ps = target === 'all' ? humans() : [target]; for (const p of ps) { if (!p || p.bot) continue; p.layout = layout; p.inGame = true; send(p, { t: 'layout', layout }); } },
      send(p, m) { send(p, m); },
      shake(a) { shakeAmt = Math.max(shakeAmt, a); },
      finish(results) { endGame(results); },
    };
  }

  // ---------- results / party flow ----------
  function endGame(results) {
    if (S.state !== 'game') return;
    S.state = 'results';
    const awards = [3, 2, 1];
    results.forEach((r, i) => {
      const aw = !r.p.bot && i < 3 && results.length > 1 ? awards[i] : 0; r.aw = aw;
      r.p.trophies = (r.p.trophies || 0) + aw;
      if (S.party && !r.p.bot) S.party.pts.set(r.p, (S.party.pts.get(r.p) || 0) + aw);
      if (!r.p.bot) send(r.p, { t: 'layout', layout: { name: 'msg', emoji: ['🏆', '🥈', '🥉'][i] || '🫠', text: `You finished #${i + 1}!`, sub: aw ? `+${aw} trophies` : 'Next time!' } });
    });
    $('#resKicker').textContent = S.party ? `🎉 PARTY GAME ${S.party.i + 1} OF ${S.party.list.length}` : 'RESULTS';
    $('#resTitle').textContent = `${S.def.emoji} ${S.def.name}`;
    renderPodium(results.slice(0, 3).map(r => ({ p: r.p, sub: r.stat })));
    S.res = { k: $('#resKicker').textContent, title: `${S.def.emoji} ${S.def.name}`, pod: results.slice(0, 3).map(r => ({ n: r.p.name, av: r.p.avatar || '', c: r.p.color, sub: r.stat || '' })), list: results.slice(3).map(r => ({ n: r.p.name, av: r.p.avatar || '', c: r.p.color, st: r.stat || '' })) };
    $('#resList').innerHTML = results.slice(3).map((r, i) => `<li style="--c:${r.p.color}"><span class="pl">#${i + 4}</span><span>${r.p.avatar || ''} ${esc(r.p.name)}</span><span class="st">${esc(r.stat || '')}</span><span class="aw"></span></li>`).join('');
    const nxt = S.party && S.party.list[S.party.i + 1];
    $('#resFoot').textContent = nxt ? `Next up: ${nxt.emoji} ${nxt.name}…` : S.party ? 'And the party champion is…' : 'Back to the menu in a moment · VIP: OK to skip';
    if (S.res) S.res.foot = $('#resFoot').textContent;
    show('results'); SFX.musicOff && SFX.musicOff(); SFX.win(); confettiDom();
    clearTimeout(timer); timer = setTimeout(afterResults, 7000);
  }
  function renderPodium(top) {
    const order = [1, 0, 2].filter(i => top[i]);
    $('#podium').innerHTML = order.map(i => `<div class="pod p${i + 1}" style="--c:${top[i].p.color};animation-delay:${(2 - i) * 0.25}s"><div class="pav">${top[i].p.avatar || '🙂'}</div><div class="pname">${esc(top[i].p.name)}</div><div class="muted">${esc(top[i].sub || '')}</div><div class="block">${i + 1}</div></div>`).join('');
  }
  function afterResults() {
    clearTimeout(timer);
    if (S.state !== 'results') return;
    if (S.party) {
      S.party.i++;
      if (S.party.i < S.party.list.length) { S.game = null; return startIntro(S.party.list[S.party.i]); }
      const ranked = [...S.party.pts.entries()].sort((a, b) => b[1] - a[1]);
      $('#resKicker').textContent = '🎉 PARTY MODE COMPLETE'; $('#resTitle').textContent = '👑 PARTY CHAMPION';
      renderPodium(ranked.slice(0, 3).map(([p, v]) => ({ p, sub: `${v} party trophies` })));
      $('#resList').innerHTML = ranked.slice(3).map(([p, v], i) => `<li style="--c:${p.color}"><span class="pl">#${i + 4}</span><span>${p.avatar} ${esc(p.name)}</span><span class="st">${v} trophies</span><span></span></li>`).join('');
      $('#resFoot').textContent = 'What a party! Back to the menu soon…';
      S.res = { k: '🎉 PARTY MODE COMPLETE', title: '👑 PARTY CHAMPION', pod: ranked.slice(0, 3).map(([p, v]) => ({ n: p.name, av: p.avatar, c: p.color, sub: `${v} party trophies` })), list: ranked.slice(3).map(([p, v]) => ({ n: p.name, av: p.avatar, c: p.color, st: `${v} trophies` })), foot: 'What a party! Back to the menu soon…', id: Date.now() };
      S.party = null; SFX.win(); timer = setTimeout(toHome, 9000);
      return;
    }
    toHome();
  }
  function toHome() { if (SFX.crowdOff) SFX.crowdOff(); if (SFX.setTrack) SFX.setTrack('menu'); if (SFX.musicOn) setTimeout(() => S.state === 'home' && SFX.musicOn(), 600); try { speechSynthesis.cancel(); } catch (e) {}
    clearTimeout(timer); clearTimeout(launchT); launchT = null;
    if (S.state === 'game' || S.state === 'intro') S.party = null;
    S.state = 'home'; S.game = null; S.def = null;
    show('home'); renderPlayers(); sendLobby();
  }

  // ---------- native console shell (C++) ----------
  nativeLoad().catch(e => console.warn('native shell unavailable, using the HTML menu', e));
  function shellSnap() {
    const st = S.state === 'boot' ? 'boot' : S.state;
    const o = { st, code: S.code || '····', url: S.code ? joinUrl().replace(/^https?:\/\//, '') : '', net: ($('#netStatus') || {}).textContent || '', clock: new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }) };
    const v = vip();
    o.players = S.players.filter(p => p.connected || !p.cam).map(p => ({ n: p.name, c: p.color, av: p.avatar, vip: p === v, away: !p.connected, cam: !!p.isCam }));
    if (st === 'home') {
      o.qr = S.qr || null;
      const KW = { touch: 'TOUCH', move: 'JOYSTICK', tap: 'TAP', swing: 'SWING', type: 'TYPE', shake: 'SHAKE', tilt: 'TILT', draw: 'DRAW', vote: 'VOTE', camera: 'CAMERA' };
      o.rows = rowsData.map(r => ({ name: r.name, short: r.short || r.name, items: r.items.map(gm => ({ id: gm.id, n: gm.name, e: gm.emoji, a1: gm.art[0], a2: gm.art[1], ctrl: CTRL_LABEL[gm.ctrl] || '', k: KW[gm.ctrl] || 'MULTIPLAYER', pl: gm.players, d: gm.desc, sp: gm.id[0] === '_', rec: !!gm._rec })) }));
      o.music = !(SFX.musicMuted && SFX.musicMuted());
      o.f = [focus.r, focus.c];
      const d = heroDef;
      if (d) o.hero = { id: d.id, n: d.name, e: d.emoji, d: d.desc, a1: d.art[0], a2: d.art[1], cat: d.cat === 'quick' ? 'QUICK PLAY' : ((CATEGORIES.find(c => c.id === d.cat) || {}).name || '').toUpperCase(), ctrl: CTRL_LABEL[d.ctrl] || '', pl: `👥 ${d.players} players`, mins: `⏱️ ~${d.mins} min` };
    }
    if (st === 'intro' && S.def) {
      const d = S.def;
      o.intro = { id: d.id, n: d.name, e: d.emoji, a1: d.art[0], a2: d.art[1], ctrl: CTRL_LABEL[d.ctrl] || '', how: d.how,
        cat: (S.party ? `🎉 Party game ${S.party.i + 1} of ${S.party.list.length} · ` : '') + ((CATEGORIES.find(c => c.id === d.cat) || {}).name || ''),
        ready: humans().filter(p => !p.cam || camFits()).map(p => ({ n: p.name, c: p.color, av: p.cam ? '📷' : p.avatar, ok: p.cam || S.ready.has(p) })), left: Math.max(0, Math.round(introLeft * 4) / 4) };
    }
    if (st === 'results') o.res = S.res;
    return o;
  }
  const toGame = e => { const r = cv.getBoundingClientRect(), dpr = cv.width / r.width; return { x: ((e.clientX - r.left) * dpr - view.ox) / view.s, y: ((e.clientY - r.top) * dpr - view.oy) / view.s }; };
  let lastHover = 0;
  cv.addEventListener('mousemove', e => {
    if (S.state !== 'home' || !NativeShell.ready()) return;
    const { x, y } = toGame(e), k = NativeShell.pick(x, y);
    if (k >= 1000 && k !== lastHover) { lastHover = k; focus = { r: ((k - 1000) / 100) | 0, c: (k - 1000) % 100 }; applyFocus(true); }
  });
  cv.addEventListener('click', e => {
    if (!NativeShell.ready()) return;
    if (S.state === 'boot') return boot();
    if (S.state === 'intro') return nav('ok');
    if (S.state === 'results') return nav('ok');
    if (S.state !== 'home') return;
    const { x, y } = toGame(e), k = NativeShell.pick(x, y);
    if (k >= 1000) { focus = { r: ((k - 1000) / 100) | 0, c: (k - 1000) % 100 }; applyFocus(true); activate(); }
    else if (k >= 500 && k < 600) { colMemo[focus.r] = focus.c; const r = k - 500; focus = { r, c: Math.min(colMemo[r] || 0, rowsData[r].items.length - 1) }; SFX.blip(); applyFocus(true); }
    else if (k === 2) $('#heroPlay').click();
    else if (k === 3) startParty();
  });

  if (/[?&]debug/.test(location.search)) window.CCDBG = { S, startIntro, launch, endGame, toHome, humans, makeCtx };
  // ---------- render loop ----------
  // FPS saver: only on TVs that can't keep up (under ~42 fps for a few seconds) do we step down, one notch at a time:
  // 1) 3D at a lower resolution (halfway, then 1600x900), 2) text layer at 1x, 3) bloom/glow off. Fast TVs never lose anything.
  const PERF = { n: 0, t: 0, level: 0 };
  function perfTick(dt) {
    if (document.hidden || PERF.level >= 3 || performance.now() < 12000 || dt > 0.2) return;
    PERF.n++; PERF.t += dt;
    if (PERF.t < 4) return;
    const fps = PERF.n / PERF.t; PERF.n = 0; PERF.t = 0;
    if (fps < 42) {
      if (NATIVE.rs > 1.01) { nativeSetScale(NATIVE.rs > 1.4 ? (NATIVE.rs + 1) / 2 : 1); console.info('[perf] 3D resolution', NATIVE.rs, Math.round(fps) + ' fps'); return; }
      PERF.level++; if (PERF.level === 2) fit(); if (PERF.level === 3 && NATIVE.mod) NATIVE.mod._cc_dbg(100); console.info('[perf] step down to level', PERF.level, Math.round(fps) + ' fps');
    }
  }
  function fit() {
    const dpr = PERF.level >= 2 ? 1 : Math.min(2, window.devicePixelRatio || 1);
    cv.width = Math.round(innerWidth * dpr); cv.height = Math.round(innerHeight * dpr);
    const s = Math.min(cv.width / W, cv.height / H); view = { s, ox: (cv.width - W * s) / 2, oy: (cv.height - H * s) / 2 };
    snowCv.width = innerWidth; snowCv.height = innerHeight;
  }
  const snowCv = $('#snow'), sg = snowCv.getContext('2d');
  const flakes = Array.from({ length: 140 }, () => ({ x: Math.random(), y: Math.random(), r: Math.random() * 2.5 + 0.8, s: Math.random() * 0.04 + 0.02, w: Math.random() * 6 }));
  addEventListener('resize', fit); fit();

  let lastT = performance.now();
  function frame(t) {
    const dt = Math.min(0.05, (t - lastT) / 1000); lastT = t;
    perfTick(Math.min(0.25, (t - (frame.pt || t)) / 1000)); frame.pt = t;
    g.imageSmoothingEnabled = true; g.imageSmoothingQuality = 'high';
    if (S.state === 'home' || S.state === 'intro' || S.state === 'boot') {
      sg.clearRect(0, 0, snowCv.width, snowCv.height); sg.fillStyle = 'rgba(255,255,255,.75)';
      for (const f of flakes) { f.y += f.s * dt; f.w += dt; if (f.y > 1.02) { f.y = -0.02; f.x = Math.random(); } sg.beginPath(); sg.arc((f.x + Math.sin(f.w) * 0.01) * snowCv.width, f.y * snowCv.height, f.r, 0, TAU); sg.fill(); }
    }
    if (S.state === 'home') {
      idleT += dt;
      if (idleT > 9) { idleT = 0; const feat = GAMES.filter(x => x.cat === 'winter' || x.cat === 'party'); setHero(feat[heroCycle++ % feat.length]); }
      const d = new Date(); $('#clockTv').textContent = d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
    }
    if (S.state === 'intro') {
      introLeft -= dt; $('#introTimer').textContent = `Starting in ${Math.max(0, Math.ceil(introLeft))}s`;
      if (introLeft <= 0) launchSoon(0);
    }
    const shellOn = NativeShell.ready() && (S.state === 'boot' || S.state === 'home' || S.state === 'intro' || (S.state === 'results' && S.res));
    document.body.classList.toggle('nativeOS', NativeShell.ready());
    if (shellOn) {
      try {
        g.setTransform(1, 0, 0, 1, 0, 0); g.fillStyle = '#000'; g.fillRect(0, 0, cv.width, cv.height);
        g.setTransform(view.s, 0, 0, view.s, view.ox, view.oy);
        NativeShell.frame(dt, shellSnap(), g);
      } catch (err) { console.error(err); }
    } else if (S.game && (S.state === 'game' || S.state === 'results')) {
      try {
        if (S.state === 'game') S.game.update(dt);
        g.setTransform(1, 0, 0, 1, 0, 0); g.fillStyle = '#000'; g.fillRect(0, 0, cv.width, cv.height);
        const sx = (Math.random() - 0.5) * shakeAmt, sy = (Math.random() - 0.5) * shakeAmt; shakeAmt = Math.max(0, shakeAmt - dt * 45);
        g.setTransform(view.s, 0, 0, view.s, view.ox + sx * view.s, view.oy + sy * view.s);
        g.save(); g.beginPath(); g.rect(0, 0, W, H); g.clip(); if (S.game) S.game.draw(g); g.restore();
        g.font = '800 16px Nunito, sans-serif'; g.textAlign = 'right'; g.textBaseline = 'bottom'; g.fillStyle = 'rgba(255,255,255,.55)';
        g.fillText(`Join: ${S.code}  ·  Esc = menu`, W - 12, H - 8);
      } catch (err) { console.error(err); }
    }
    requestAnimationFrame(frame);
  }
  requestAnimationFrame(frame);

  // ---------- keys ----------
  addEventListener('keydown', e => {
    if (S.state === 'boot') { boot(); return; }
    if (e.key === 'm' || e.key === 'M') { const on = SFX.toggleMusic && SFX.toggleMusic(); banner(on ? '🎵 Music on' : '🔇 Music off', 1000, '#a5d8ff'); return; }
    if (e.key === 'f' || e.key === 'F') { if (!document.fullscreenElement) document.documentElement.requestFullscreen?.().catch(() => {}); else document.exitFullscreen?.(); return; }
    const map = { ArrowLeft: 'left', ArrowRight: 'right', ArrowUp: 'up', ArrowDown: 'down', Enter: 'ok', ' ': 'ok', Escape: 'back', Backspace: 'back' };
    if (map[e.key]) { e.preventDefault(); nav(map[e.key]); }
    if (S.state === 'home' && (e.key === 'p' || e.key === 'P')) startParty();
    if (S.state === 'home' && (e.key === 'r' || e.key === 'R')) surprise();
  });
  $('#heroPlay').onclick = () => { const d = heroDef; if (!d) return; if (d.id === '_party') startParty(); else if (d.id === '_random') surprise(); else startIntro(GAMES.find(x => x.id === d.id)); };
  $('#heroParty').onclick = startParty;

  // ---------- boot ----------
  function boot() {
    if (S.state !== 'boot') return;
    SFX.init(); SFX.join(); SFX.musicOn && SFX.musicOn();
    $('#start').classList.add('hidden');
    S.state = 'home'; show('home'); buildRows(); renderPlayers();
    let prev = null; try { prev = sessionStorage.getItem('cc_room'); } catch (e) {}
    ICE_READY.then(() => openRoom(prev));
  }
  $('#startBtn').onclick = boot;
  $('#start').onclick = boot;
  if (document.fonts) document.fonts.load('40px Bungee');
})();
