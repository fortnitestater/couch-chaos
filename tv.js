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
      send(p, { t: 'ping' });
      if (p.pongAt && now - p.lastSeen > 12000) { const c = p.conn; onClose(c); try { c.close(); } catch (e) {} }
    }
  }, 2000);
  const joinUrl = () => new URL('play.html?room=' + S.code, location.href).href;
  function showJoinInfo() {
    $('#code').textContent = S.code; $('#codeSmall').textContent = S.code;
    $('#joinUrl').textContent = joinUrl().replace(/^https?:\/\//, '');
    const q = $('#qr'); q.innerHTML = '';
    if (window.QRCode) new QRCode(q, { text: joinUrl(), width: 256, height: 256, colorDark: '#0a1024', colorLight: '#ffffff' });
  }
  function send(p, m) { if (p && !p.bot && p.conn && p.conn.open) { try { p.conn.send(m); } catch (e) {} } }
  const humans = () => S.players.filter(p => p.connected);
  const vip = () => humans()[0] || null;

  function onMsg(conn, m) {
    if (!m || typeof m !== 'object') return;
    if (m.t === 'join') return onJoin(conn, m);
    const p = S.players.find(q => q.conn === conn); if (!p) return;
    p.lastSeen = Date.now(); if (m.t === 'pong') { p.pongAt = p.lastSeen; return; }
    const isVip = p === vip();
    switch (m.t) {
      case 'in': {
        const prev = p.input; p.input = { x: +m.x || 0, y: +m.y || 0, a: !!m.a, b: !!m.b };
        if (S.state === 'game' && S.game && S.game.onButton) { if (!prev.a && p.input.a) S.game.onButton(p, 'a'); if (!prev.b && p.input.b) S.game.onButton(p, 'b'); }
        break;
      }
      case 'act': if (S.state === 'game' && S.game && S.game.onAct) S.game.onAct(p, m); break;
      case 'pump': if (S.state === 'game' && S.game && S.game.onPump) S.game.onPump(p, +m.n || 1); break;
      case 'ready': if (S.state === 'intro') { S.ready.add(p); renderReady(); if (humans().every(h => S.ready.has(h))) launchSoon(0.8); } break;
      case 'pick': if (isVip && S.state === 'home') { const d = GAMES.find(x => x.id === m.g); if (d) { focusGame(d.id); startIntro(d); } } break;
      case 'party': if (isVip && S.state === 'home') startParty(); break;
      case 'random': if (isVip && S.state === 'home') surprise(); break;
      case 'nav': if (isVip) nav(m.d); break;
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
  function onClose(conn) {
    const p = S.players.find(q => q.conn === conn); if (!p) return;
    p.connected = false; p.conn = null; p.input = { x: 0, y: 0, a: false, b: false };
    if (S.state === 'home') { renderPlayers(); sendLobby(); }
  }

  // ---------- home screen ----------
  const QUICK = [
    { id: '_party', name: 'Party Mode', emoji: '🎉', art: ['#ffd43b', '#ff4d6d'], desc: '5 random games in a row from every category. Trophies decide the champion!', how: [], ctrl: 'tap', players: '1-8', mins: 15, cat: 'quick' },
    { id: '_random', name: 'Surprise Me', emoji: '🎲', art: ['#63e6be', '#7048e8'], desc: 'Let fate pick a game that fits how many of you are playing.', how: [], ctrl: 'tap', players: '1-8', mins: 3, cat: 'quick' },
  ];
  let rowsData = [], focus = { r: 0, c: 0 }, heroDef = null, idleT = 0, heroCycle = 0;
  function buildRows() {
    rowsData = [{ name: '🔥 Quick Play', items: QUICK.concat(recommended()) }, ...CATEGORIES.map(c => ({ name: c.name, items: GAMES.filter(gm => gm.cat === c.id) }))];
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
    else if (d === 'up') { focus.r = Math.max(0, focus.r - 1); focus.c = Math.min(focus.c, rowsData[focus.r].items.length - 1); }
    else if (d === 'down') { focus.r = Math.min(rowsData.length - 1, focus.r + 1); focus.c = Math.min(focus.c, rowsData[focus.r].items.length - 1); }
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
    S.party = { list: list.slice(0, 5), i: 0, pts: new Map(humans().map(p => [p, 0])) };
    banner('🎉 PARTY MODE!', 1600); SFX.win();
    setTimeout(() => startIntro(S.party.list[0]), 1200);
  }
  function renderPlayers() {
    const v = vip();
    $('#avatars').innerHTML = S.players.map(p => `<div class="av2 ${p.connected ? '' : 'away'}" style="--c:${p.color}" title="${esc(p.name)}">${p === v ? '<span class="crown">👑</span>' : ''}${p.avatar}</div>`).join('');
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
  function renderReady() { $('#introReady').innerHTML = humans().map(p => `<span class="rchip ${S.ready.has(p) ? 'ok' : ''}" style="--c:${p.color}">${S.ready.has(p) ? '✓ ' : ''}${p.avatar} ${esc(p.name)}</span>`).join(''); }
  function launchSoon(s) { if (launchT) return; launchT = setTimeout(() => { launchT = null; launch(); }, s * 1000); }
  function launch() {
    if (S.state !== 'intro') return;
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
        const list = humans().slice(); let i = 0;
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
    $('#resList').innerHTML = results.slice(3).map((r, i) => `<li style="--c:${r.p.color}"><span class="pl">#${i + 4}</span><span>${r.p.avatar || ''} ${esc(r.p.name)}</span><span class="st">${esc(r.stat || '')}</span><span class="aw"></span></li>`).join('');
    const nxt = S.party && S.party.list[S.party.i + 1];
    $('#resFoot').textContent = nxt ? `Next up: ${nxt.emoji} ${nxt.name}…` : S.party ? 'And the party champion is…' : 'Back to the menu in a moment · VIP: OK to skip';
    show('results'); SFX.win();
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
      S.party = null; SFX.win(); timer = setTimeout(toHome, 9000);
      return;
    }
    toHome();
  }
  function toHome() {
    clearTimeout(timer); clearTimeout(launchT); launchT = null;
    if (S.state === 'game' || S.state === 'intro') S.party = null;
    S.state = 'home'; S.game = null; S.def = null;
    show('home'); renderPlayers(); sendLobby();
  }

  // ---------- render loop ----------
  function fit() {
    const dpr = Math.min(2, window.devicePixelRatio || 1);
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
    if (S.game && (S.state === 'game' || S.state === 'results')) {
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
    SFX.init(); SFX.join();
    $('#start').classList.add('hidden');
    S.state = 'home'; show('home'); buildRows(); renderPlayers();
    let prev = null; try { prev = sessionStorage.getItem('cc_room'); } catch (e) {}
    openRoom(prev);
  }
  $('#startBtn').onclick = boot;
  $('#start').onclick = boot;
  if (document.fonts) document.fonts.load('40px Bungee');
})();
