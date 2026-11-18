/* Couch Chaos — TV host */
(() => {
  const $ = s => document.querySelector(s);
  const W = 1600, H = 900;
  const COLORS = ['#ff4d6d', '#4dabff', '#ffd43b', '#51cf66', '#cc5de8', '#ff922b', '#22d3ee', '#f783ac'];
  const AVATARS = ['🦊', '🐸', '🐼', '🐯', '🐙', '🦄', '🐵', '🐧'];
  const S = { players: [], state: 'boot', game: null, def: null, code: null };
  let peer = null, view = { s: 1, ox: 0, oy: 0 }, shakeAmt = 0, lobbyTimer = null;

  const cv = $('#cv'), g = cv.getContext('2d');

  // ---------- networking ----------
  function makeCode() { let s = ''; for (let i = 0; i < 4; i++) s += CODE_LETTERS[(Math.random() * CODE_LETTERS.length) | 0]; return s; }
  let claimTries = 0;
  function openRoom(preferred) {
    S.code = preferred || makeCode();
    $('#netStatus').textContent = 'Opening room…';
    peer = new Peer(PREFIX + S.code);
    peer.on('open', () => {
      claimTries = 0;
      try { sessionStorage.setItem('cc_room', S.code); } catch (e) {}
      showJoinInfo();
      $('#netStatus').textContent = '🟢 Room open — join now!';
    });
    peer.on('error', e => {
      console.warn('peer error', e.type, e);
      if (e.type === 'unavailable-id') {
        peer.destroy();
        // after a TV refresh the old id is held for a few seconds; try to reclaim it, then give up
        if (preferred && claimTries++ < 4) setTimeout(() => openRoom(preferred), 2500);
        else openRoom();
      } else if (['network', 'server-error', 'socket-error', 'socket-closed'].includes(e.type)) {
        $('#netStatus').textContent = '🟠 Connection hiccup — retrying…';
        setTimeout(() => { if (peer && peer.disconnected && !peer.destroyed) peer.reconnect(); }, 2000);
      }
    });
    peer.on('disconnected', () => { setTimeout(() => { try { if (!peer.destroyed) peer.reconnect(); } catch (e) {} }, 1000); });
    peer.on('connection', conn => {
      conn.on('data', m => { try { onMsg(conn, m); } catch (err) { console.error(err); } });
      conn.on('close', () => onClose(conn));
      conn.on('error', () => onClose(conn));
    });
  }
  function joinUrl() { return new URL('play.html?room=' + S.code, location.href).href; }
  function showJoinInfo() {
    $('#code').textContent = S.code;
    $('#joinUrl').textContent = joinUrl().replace(/^https?:\/\//, '');
    const q = $('#qr'); q.innerHTML = '';
    if (window.QRCode) new QRCode(q, { text: joinUrl(), width: 256, height: 256, colorDark: '#120e2b', colorLight: '#ffffff' });
  }

  function send(p, m) { if (p && !p.bot && p.conn && p.conn.open) { try { p.conn.send(m); } catch (e) {} } }
  const humans = () => S.players.filter(p => p.connected);
  const vip = () => humans()[0] || null;

  function onMsg(conn, m) {
    if (!m || typeof m !== 'object') return;
    if (m.t === 'join') return onJoin(conn, m);
    const p = S.players.find(q => q.conn === conn);
    if (!p) return;
    if (m.t === 'in') {
      const prev = p.input;
      p.input = { x: +m.x || 0, y: +m.y || 0, a: !!m.a, b: !!m.b };
      if (S.state === 'game' && S.game) {
        if (!prev.a && p.input.a && S.game.onButton) S.game.onButton(p, 'a');
        if (!prev.b && p.input.b && S.game.onButton) S.game.onButton(p, 'b');
      }
    } else if (m.t === 'pick') {
      if (S.state === 'lobby' && p === vip()) startGame(m.g);
    } else if (m.t === 'act') {
      if (S.state === 'game' && S.game && S.game.onAct) S.game.onAct(p, m);
    } else if (m.t === 'pump') {
      if (S.state === 'game' && S.game && S.game.onPump) S.game.onPump(p, +m.n || 1);
    }
  }

  function onJoin(conn, m) {
    const name = String(m.name || 'Player').slice(0, 12).trim() || 'Player';
    let p = S.players.find(q => q.cid === m.cid);
    if (p) {
      if (p.conn && p.conn !== conn) { try { p.conn.close(); } catch (e) {} }
      p.conn = conn; p.connected = true; p.name = name;
    } else {
      if (humans().length >= 8) { try { conn.send({ t: 'full' }); } catch (e) {} return; }
      // drop the oldest disconnected player if the color slots are all taken
      if (S.players.length >= 8) S.players.splice(S.players.findIndex(q => !q.connected), 1);
      const used = new Set(S.players.map(q => q.color));
      const idx = COLORS.findIndex(c => !used.has(c));
      p = { cid: m.cid, name, color: COLORS[idx], avatar: AVATARS[idx], conn, connected: true, trophies: 0, input: { x: 0, y: 0, a: false, b: false }, layout: null };
      S.players.push(p);
      SFX.join();
    }
    send(p, { t: 'welcome', name: p.name, color: p.color, avatar: p.avatar, code: S.code });
    if (S.state === 'lobby') { renderLobby(); sendLobby(); }
    else if (S.state === 'game' && S.game) {
      if (S.game.onJoin && S.game.onJoin(p)) { /* game took them in */ }
      else if (p.layout && p.inGame) { send(p, { t: 'layout', layout: p.layout }); if (S.game.resync) S.game.resync(p); }
      else send(p, { t: 'layout', layout: { name: 'msg', emoji: '🍿', text: 'Game in progress', sub: "Watch the TV — you're in the next one!" } });
    } else if (S.state === 'results') send(p, { t: 'layout', layout: { name: 'msg', emoji: '🏁', text: 'Round over!', sub: 'Back to the lobby in a sec…' } });
  }

  function onClose(conn) {
    const p = S.players.find(q => q.conn === conn);
    if (!p) return;
    p.connected = false; p.conn = null; p.input = { x: 0, y: 0, a: false, b: false };
    if (S.state === 'lobby') { renderLobby(); sendLobby(); }
  }

  // ---------- lobby ----------
  function renderLobby() {
    const hs = S.players;
    const v = vip();
    $('#pcount').textContent = humans().length ? `${humans().length}/8` : '';
    $('#plist').innerHTML = hs.length ? hs.map(p => `<div class="pchip ${p.connected ? '' : 'away'}" style="--c:${p.color}">
        <span class="av">${p.avatar}</span>${p === v ? '👑 ' : ''}${esc(p.name)}${p.trophies ? ` <span class="tro">🏆${p.trophies}</span>` : ''}${p.connected ? '' : ' (away)'}</div>`).join('')
      : '<div class="empty">Waiting for players… scan the code! 📱</div>';
  }
  function renderGames() {
    $('#glist').innerHTML = GAMES.map((gm, i) => `<button class="gtile" data-g="${gm.id}"><span class="ge">${gm.emoji}</span><b>${i + 1}. ${esc(gm.name)}</b><small>${esc(gm.desc)}</small></button>`).join('');
    $('#glist').querySelectorAll('.gtile').forEach(b => b.onclick = () => startGame(b.dataset.g));
  }
  function sendLobby() {
    const v = vip();
    const games = GAMES.map(({ id, name, emoji, desc }) => ({ id, name, emoji, desc }));
    for (const p of humans()) {
      p.layout = { name: 'lobby', vip: p === v, vipName: v ? v.name : '', games, trophies: p.trophies };
      p.inGame = false;
      send(p, { t: 'layout', layout: p.layout });
    }
  }
  function showScreen(which) {
    $('#lobby').classList.toggle('hidden', which !== 'lobby');
    $('#cv').classList.toggle('hidden', which === 'lobby');
    $('#results').classList.toggle('hidden', which !== 'results');
  }

  // ---------- banner ----------
  function banner(text, ms = 1600, color = '#ffd43b') {
    const b = $('#banner');
    b.textContent = text; b.style.color = color; b.style.setProperty('--d', ms + 'ms');
    b.style.fontSize = text.length > 16 ? '6vh' : '9vh';
    b.classList.remove('show'); void b.offsetWidth; b.classList.add('show');
  }

  // ---------- games ----------
  const BOT_NAMES = ['Robo Rita', 'Bot Bob', 'Capt. Clank', 'Sir Beeps', 'Gizmo Gran'];
  function makeCtx() {
    return {
      W, H, sfx: SFX, banner,
      humans,
      roster(min) {
        const list = humans().slice();
        let i = 0;
        while (list.length < min) {
          const used = new Set(list.map(p => p.color));
          const color = COLORS.find(c => !used.has(c)) || '#aaa';
          list.push({ cid: 'bot' + i, name: BOT_NAMES[i % BOT_NAMES.length], color, avatar: '🤖', bot: true, input: { x: 0, y: 0, a: false, b: false } });
          i++;
        }
        return list;
      },
      setLayout(target, layout) {
        const ps = target === 'all' ? humans() : [target];
        for (const p of ps) { if (p.bot) continue; p.layout = layout; p.inGame = true; send(p, { t: 'layout', layout }); }
      },
      send(p, m) { send(p, m); },
      shake(a) { shakeAmt = Math.max(shakeAmt, a); },
      finish(results) { endGame(results); },
    };
  }

  function startGame(id) {
    const def = GAMES.find(x => x.id === id);
    if (!def || S.state !== 'lobby') return;
    if (!humans().length) { banner('Need at least 1 player!', 1500, '#ff4d6d'); return; }
    SFX.init();
    S.state = 'game'; S.def = def;
    showScreen('game');
    S.game = def.make(makeCtx());
    S.game.start();
  }

  function endGame(results) {
    if (S.state !== 'game') return;
    S.state = 'results';
    const awards = [3, 2, 1];
    $('#resTitle').textContent = `${S.def.emoji} ${S.def.name}`;
    $('#resList').innerHTML = results.map((r, i) => {
      const aw = !r.p.bot && i < 3 && results.length > 1 ? awards[i] : 0;
      r.p.trophies = (r.p.trophies || 0) + aw;
      if (!r.p.bot) send(r.p, { t: 'layout', layout: { name: 'msg', emoji: i === 0 ? '🏆' : i === 1 ? '🥈' : i === 2 ? '🥉' : '🫠', text: `You finished #${i + 1}!`, sub: aw ? `+${aw} trophies` : 'Next time!' } });
      return `<li style="--c:${r.p.color};animation-delay:${i * 0.12}s"><span class="pl">#${i + 1}</span><span>${r.p.avatar || ''} ${esc(r.p.name)}</span><span class="st">${esc(r.stat || '')}</span><span class="aw">${aw ? '+' + aw + ' 🏆' : ''}</span></li>`;
    }).join('');
    showScreen('results');
    $('#cv').classList.remove('hidden');
    SFX.win();
    clearTimeout(lobbyTimer);
    lobbyTimer = setTimeout(toLobby, 7000);
  }

  function toLobby() {
    clearTimeout(lobbyTimer);
    if (S.game && S.game.destroy) S.game.destroy();
    S.state = 'lobby'; S.game = null; S.def = null;
    showScreen('lobby');
    renderLobby(); sendLobby();
  }

  // ---------- render loop ----------
  function fit() {
    const dpr = Math.min(2, window.devicePixelRatio || 1);
    cv.width = Math.round(innerWidth * dpr); cv.height = Math.round(innerHeight * dpr);
    const s = Math.min(cv.width / W, cv.height / H);
    view = { s, ox: (cv.width - W * s) / 2, oy: (cv.height - H * s) / 2 };
  }
  addEventListener('resize', fit); fit();

  let lastT = performance.now();
  function frame(t) {
    const dt = Math.min(0.05, (t - lastT) / 1000); lastT = t;
    if (S.game && (S.state === 'game' || S.state === 'results')) {
      try {
        if (S.state === 'game') S.game.update(dt);
        g.setTransform(1, 0, 0, 1, 0, 0);
        g.fillStyle = '#000'; g.fillRect(0, 0, cv.width, cv.height);
        const sx = (Math.random() - 0.5) * shakeAmt, sy = (Math.random() - 0.5) * shakeAmt;
        shakeAmt = Math.max(0, shakeAmt - dt * 45);
        g.setTransform(view.s, 0, 0, view.s, view.ox + sx * view.s, view.oy + sy * view.s);
        g.save(); g.beginPath(); g.rect(0, 0, W, H); g.clip();
        if (S.game) S.game.draw(g);
        g.restore();
        g.font = '800 18px Nunito, sans-serif'; g.textAlign = 'right'; g.textBaseline = 'bottom';
        g.fillStyle = 'rgba(255,255,255,.7)';
        g.fillText(`Join: ${S.code}  ·  Esc = quit to lobby`, W - 14, H - 10);
      } catch (err) { console.error(err); }
    }
    requestAnimationFrame(frame);
  }
  requestAnimationFrame(frame);

  // ---------- keys ----------
  addEventListener('keydown', e => {
    if (e.key === 'f' || e.key === 'F') {
      if (!document.fullscreenElement) document.documentElement.requestFullscreen?.().catch(() => {});
      else document.exitFullscreen?.();
    }
    if (S.state === 'lobby' && /^[1-9]$/.test(e.key)) { const gm = GAMES[+e.key - 1]; if (gm) startGame(gm.id); }
    if (e.key === 'Escape' && (S.state === 'game' || S.state === 'results')) toLobby();
  });

  // ---------- boot ----------
  $('#startBtn').onclick = () => {
    SFX.init(); SFX.join();
    $('#start').classList.add('hidden');
    S.state = 'lobby';
    showScreen('lobby');
    renderGames(); renderLobby();
    let prev = null; try { prev = sessionStorage.getItem('cc_room'); } catch (e) {}
    openRoom(prev);
  };
  if (document.fonts) document.fonts.load('40px Bungee');
})();
