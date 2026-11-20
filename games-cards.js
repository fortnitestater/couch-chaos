/* Couch Chaos — 🃏 Game Night: original card & board games (our own rules and art).
   Phones show a private "panel": your cards (tap to play) + action buttons.
   TV → phone: {t:'panel', title, info, sub, cards:[{label,sub,icon,color,ok}], buttons:[{id,label,color,off,big}], myTurn}
   phone → TV: {t:'act', a:'card', i} or {t:'act', a:'btn', id} */

const panelMsg = (o) => Object.assign({ t: 'panel' }, o);
function seatPos(i, n, cx = 800, cy = 470, rx = 600, ry = 300) { const a = Math.PI / 2 + (i / n) * TAU; return { x: cx + Math.cos(a) * rx, y: cy + Math.sin(a) * ry }; }
function felt(g, c1 = '#1d6b45', c2 = '#0b2a1c') { const gr = g.createRadialGradient(800, 450, 80, 800, 450, 950); gr.addColorStop(0, c1); gr.addColorStop(1, c2); g.fillStyle = gr; g.fillRect(0, 0, 1600, 900); g.strokeStyle = 'rgba(255,255,255,.06)'; g.lineWidth = 2; g.beginPath(); g.ellipse(800, 470, 660, 340, 0, 0, TAU); g.stroke(); }
function cardTable(g, t, tint = '#13406b') {
  const gr = g.createRadialGradient(800, 470, 60, 800, 470, 980); gr.addColorStop(0, shadeHex(tint, 0.12)); gr.addColorStop(0.55, tint); gr.addColorStop(1, '#05070e'); g.fillStyle = gr; g.fillRect(0, 0, 1600, 900);
  // felt texture: faint noise rings + the table edge
  g.strokeStyle = 'rgba(255,255,255,.05)'; g.lineWidth = 2; for (let k = 0; k < 3; k++) { g.beginPath(); g.ellipse(800, 470, 380 + k * 140, 200 + k * 75, 0, 0, TAU); g.stroke(); }
  const v = g.createRadialGradient(800, 450, 420, 800, 450, 1000); v.addColorStop(0, 'rgba(0,0,0,0)'); v.addColorStop(1, 'rgba(0,0,0,.55)'); g.fillStyle = v; g.fillRect(0, 0, 1600, 900);
}
function namePlate(g, p, x, y, active, sub) {
  const w = 230, h = 64;
  g.save();
  if (active) { g.shadowColor = p.color; g.shadowBlur = 36; }
  g.fillStyle = active ? 'rgba(255,255,255,.95)' : 'rgba(10,12,22,.72)'; rr(g, x - w / 2, y - h / 2, w, h, h / 2); g.fill(); g.shadowBlur = 0;
  g.lineWidth = 1.5; g.strokeStyle = active ? 'transparent' : 'rgba(255,255,255,.12)'; g.stroke();
  g.fillStyle = p.color; g.beginPath(); g.arc(x - w / 2 + 32, y, 24, 0, TAU); g.fill();
  g.font = '26px serif'; g.textAlign = 'center'; g.textBaseline = 'middle'; g.fillText(p.avatar || '🙂', x - w / 2 + 32, y + 1);
  g.font = '600 22px Inter, system-ui, sans-serif'; g.textAlign = 'left'; g.fillStyle = active ? '#0b0d17' : '#f7f8fc'; g.fillText(p.name.slice(0, 11), x - w / 2 + 64, y - 9);
  g.font = '500 16px Inter, system-ui, sans-serif'; g.fillStyle = active ? '#3c4566' : '#b9c0d8'; g.fillText(sub || '', x - w / 2 + 64, y + 14);
  g.restore();
}
// a hand of card backs fanned toward the table centre
function backFan(g, x, y, n, painter, w = 62, h = 90) {
  const base = Math.atan2(470 - y, 800 - x) - Math.PI / 2, m = Math.min(n, 12), mid = (m - 1) / 2;
  g.save(); g.translate(x, y); g.rotate(base);
  const step = Math.min(0.16, 0.9 / Math.max(1, m)); // spread like a real hand, tighter when it gets big
  for (let k = 0; k < m; k++) { g.save(); g.rotate((k - mid) * step); painter(g, (k - mid) * 6, -h * 0.55, w, h, null, true, 0); g.restore(); }
  g.restore();
}
function seat(g, p, x, y, active, extra) {
  if (active) { g.save(); g.shadowColor = p.color; g.shadowBlur = 40; g.fillStyle = 'rgba(255,255,255,.08)'; rr(g, x - 110, y - 52, 220, 104, 26); g.fill(); g.restore(); g.lineWidth = 4; g.strokeStyle = p.color; rr(g, x - 110, y - 52, 220, 104, 26); g.stroke(); }
  drawBlob(g, x - 62, y, 30, p.color, -Math.PI / 2, { happy: active, shadow: false });
  txt(g, p.name.slice(0, 10), x + 22, y - 18, 22, '#fff', 'center', 'Nunito');
  if (extra) txt(g, extra, x + 22, y + 16, 20, '#ffd43b', 'center', 'Nunito');
}

/* =====================================================================
   🌪️ COLOR STORM — shed all your cards. Match the COLOR or the SYMBOL.
   +2 (stackable) · STOP (skip) · ↺ (reverse) · TWIN (play another of that color)
   Wilds: COLOR (pick a color) · BLIZZARD (+4, pick a color) · SWAP (trade hands with the leader)
   One card left? Tap LAST CARD! — or anyone can CATCH you for +2.
   ===================================================================== */
const STORM_COLORS = { R: '#ff4d6d', B: '#4dabff', G: '#51cf66', Y: '#ffd43b' }, STORM_NAMES = { R: 'Red', B: 'Blue', G: 'Green', Y: 'Yellow' };
const stormText = c => c.v === 'REV' ? '↺' : c.v === 'STOP' ? '⛔' : c.v === 'TWIN' ? '✌' : c.v === 'COLOR' ? '🌈' : c.v === 'BLIZZARD' ? '+4' : c.v === 'SWAP' ? '⇄' : c.v;
function drawStormCard(g, x, y, w, h, c, back) {
  g.save(); g.translate(x, y);
  g.shadowColor = 'rgba(0,0,0,.45)'; g.shadowBlur = 18; g.shadowOffsetY = 8;
  g.fillStyle = '#fff'; rr(g, -w / 2, -h / 2, w, h, w * 0.12); g.fill(); g.shadowColor = 'transparent';
  if (back) { const gr = g.createLinearGradient(-w / 2, -h / 2, w / 2, h / 2); gr.addColorStop(0, '#3b1f7a'); gr.addColorStop(1, '#0b7285'); g.fillStyle = gr; rr(g, -w / 2 + 6, -h / 2 + 6, w - 12, h - 12, w * 0.1); g.fill(); txt(g, '🌪️', 0, 0, w * 0.45); g.restore(); return; }
  const col = c.c ? STORM_COLORS[c.c] : null;
  if (col) { g.fillStyle = col; rr(g, -w / 2 + 6, -h / 2 + 6, w - 12, h - 12, w * 0.1); g.fill(); }
  else { const gr = g.createLinearGradient(-w / 2, -h / 2, w / 2, h / 2); ['#ff4d6d', '#ffd43b', '#51cf66', '#4dabff'].forEach((k, i) => gr.addColorStop(i / 3, k)); g.fillStyle = gr; rr(g, -w / 2 + 6, -h / 2 + 6, w - 12, h - 12, w * 0.1); g.fill(); g.fillStyle = 'rgba(10,6,30,.55)'; rr(g, -w / 2 + 14, -h / 2 + 14, w - 28, h - 28, w * 0.08); g.fill(); }
  g.fillStyle = 'rgba(255,255,255,.9)'; g.beginPath(); g.ellipse(0, 0, w * 0.34, h * 0.36, -0.5, 0, TAU); g.fill();
  const t = stormText(c); txt(g, t, 0, 4, t.length > 2 ? w * 0.22 : w * 0.38, col || '#0a1024');
  txt(g, t, -w / 2 + 18, -h / 2 + 22, w * 0.14, '#fff', 'center', 'Nunito');
  g.restore();
}
class ColorStorm {
  constructor(c) { this.c = c; this.fx = new Particles(); this.fly = null; this.t = 0; this.log = ''; }
  start() {
    this.ps = this.c.roster(2);
    this.deck = []; const C = Object.keys(STORM_COLORS);
    for (const c of C) { for (let v = 1; v <= 9; v++) { this.deck.push({ c, v: String(v) }, { c, v: String(v) }); } for (const v of ['+2', 'STOP', 'REV', 'TWIN']) this.deck.push({ c, v }, { c, v }); }
    for (let i = 0; i < 4; i++) this.deck.push({ c: null, v: 'COLOR' }, { c: null, v: 'BLIZZARD' });
    for (let i = 0; i < 2; i++) this.deck.push({ c: null, v: 'SWAP' });
    this.deck.forEach((d, i) => (d.id = i)); shuffle(this.deck);
    const n = this.ps.length > 5 ? 5 : 7;
    this.hands = new Map(this.ps.map(p => [p, this.deck.splice(0, n)]));
    let top; do { top = this.deck.pop(); if (!top.c || isNaN(+top.v)) { this.deck.unshift(top); top = null; } } while (!top);
    this.pile = [top]; this.color = top.c; this.turn = irnd(0, this.ps.length - 1); this.dir = 1; this.pending = 0; this.drew = false; this.needColor = false; this.twin = false;
    this.timer = 25; this.botT = rnd(1, 1.8); this.lastCall = null; this.over = false; this.winner = null;
    this.pile[0].rot = rnd(-0.2, 0.2); this.fl = new CardFlights(); this.shown = new Map(this.ps.map(p => [p, 0]));
    // deal: cards fly from the deck to every seat, one by one
    const n0 = this.hands.get(this.ps[0]).length;
    for (let k = 0; k < n0; k++) this.ps.forEach((p, i) => this.fl.add({ from: { x: 560, y: 450 }, to: this.fanPos(i), card: null, w: 54, h: 78, delay: (k * this.ps.length + i) * 0.045, dur: 0.38, arc: -40, onLand: () => { this.shown.set(p, this.shown.get(p) + 1); this.c.sfx.card(); } }));
    this.c.setLayout('all', { name: 'panel' }); this.sync(); this.c.banner('🌪️ COLOR STORM!');
  }
  get cur() { return this.ps[this.turn]; }
  top() { return this.pile[this.pile.length - 1]; }
  seatAt(i) { return seatPos(i, this.ps.length, 800, 470, 640, 330); }
  fanPos(i) { const s = this.seatAt(i); return { x: s.x + (800 - s.x) * 0.3, y: s.y + (470 - s.y) * 0.42 }; }
  idx(k) { const n = this.ps.length; return (((this.turn + this.dir * k) % n) + n) % n; }
  playable(card) {
    if (this.twin) return card.c === this.color;
    if (this.pending > 0) return card.v === '+2' || card.v === 'BLIZZARD';
    return !card.c || card.c === this.color || card.v === this.top().v;
  }
  panelFor(p) {
    const h = this.hands.get(p), my = p === this.cur && !this.over, call = this.lastCall;
    const buttons = [];
    if (my && this.needColor) Object.keys(STORM_COLORS).forEach(k => buttons.push({ id: 'col' + k, label: STORM_NAMES[k], color: STORM_COLORS[k], big: true }));
    else if (my) {
      if (!this.drew && !this.twin) buttons.push({ id: 'draw', label: this.pending ? `DRAW ${this.pending}` : 'DRAW', color: '#4dabff' });
      if (this.drew || this.twin) buttons.push({ id: 'pass', label: this.twin ? 'DONE' : 'PASS', color: '#868e96' });
    }
    if (call && !call.safe) { if (call.p === p) buttons.push({ id: 'last', label: '☝ LAST CARD!', color: '#ff922b', big: true }); else buttons.push({ id: 'catch', label: `🚨 CATCH ${call.p.name}!`, color: '#ff4d6d', big: true }); }
    return panelMsg({
      myTurn: my, title: my ? (this.needColor ? '🌈 Pick a color!' : this.twin ? '✌ TWIN: play another ' + STORM_NAMES[this.color] + ' card' : this.pending ? `⚠️ Stack a +2 / +4 or draw ${this.pending}` : '🌪️ Your turn!') : `${this.cur.name}'s turn`,
      info: `${h.length} cards · color: ${STORM_NAMES[this.color]}`,
      cards: h.map(c => ({ label: stormText(c), sub: c.c ? '' : c.v, color: c.c ? STORM_COLORS[c.c] : '#7048e8', ok: my && !this.needColor && this.playable(c) })),
      buttons,
    });
  }
  sync() { for (const p of this.ps) if (!p.bot) this.c.send(p, this.panelFor(p)); }
  resync(p) { if (this.hands.has(p)) this.c.send(p, this.panelFor(p)); }
  onAct(p, m) {
    if (this.over) return;
    if (m.a === 'btn' && m.id === 'last' && this.lastCall && this.lastCall.p === p) { this.lastCall.safe = true; this.c.banner(`☝ ${p.name}: LAST CARD!`, 1300, p.color); this.c.sfx.ding(); return this.sync(); }
    if (m.a === 'btn' && m.id === 'catch' && this.lastCall && !this.lastCall.safe && this.lastCall.p !== p) { const v = this.lastCall.p; this.lastCall = null; this.give(v, 2); this.c.banner(`🚨 ${p.name} caught ${v.name}! +2`, 1600, '#ff4d6d'); this.c.sfx.buzz(); return this.sync(); }
    if (p !== this.cur) return;
    if (m.a === 'btn' && m.id && m.id.startsWith('col') && this.needColor) { this.color = m.id.slice(3); this.needColor = false; this.log = `${p.name} → ${STORM_NAMES[this.color]}`; this.c.sfx.blip(); return this.afterPlay(); }
    if (this.needColor) return;
    if (m.a === 'card') this.play(p, m.i | 0);
    else if (m.a === 'btn' && m.id === 'draw') this.doDraw(p);
    else if (m.a === 'btn' && m.id === 'pass' && (this.drew || this.twin)) { this.twin = false; this.next(1); }
  }
  draw1(p) { if (!this.deck.length) { const top = this.pile.pop(); this.deck = shuffle(this.pile); this.pile = [top]; } return this.deck.pop(); }
  give(p, n) {
    const h = this.hands.get(p), i0 = this.ps.indexOf(p);
    for (let i = 0; i < n; i++) { const c = this.draw1(p); if (c) h.push(c); if (this.fl) this.fl.add({ from: { x: 560, y: 450 }, to: this.fanPos(i0), card: null, w: 54, h: 78, delay: i * 0.09, dur: 0.4, arc: -50, onLand: () => this.c.sfx.card() }); }
  }
  doDraw(p) {
    if (this.drew || this.twin) return;
    if (this.pending) { const n = this.pending; this.pending = 0; this.give(p, n); this.log = `${p.name} drew ${n}! 😱`; this.c.shake(8); return this.next(1); }
    this.give(p, 1); this.drew = true; this.log = `${p.name} drew a card`;
    const h = this.hands.get(p); if (!this.playable(h[h.length - 1])) return this.next(1);
    this.sync();
  }
  play(p, i) {
    const h = this.hands.get(p), card = h[i]; if (!card || !this.playable(card)) return;
    h.splice(i, 1); card.rot = rnd(-0.3, 0.3); card.flying = true; this.pile.push(card); this.c.sfx.card();
    if (this.fl) this.fl.add({ from: this.fanPos(this.turn), to: { x: 800, y: 450 }, card, w: 150, h: 220, dur: 0.42, arc: -90, rot0: rnd(-0.6, 0.6), rot1: card.rot, onLand: () => { card.flying = false; this.c.shake(3); } });
    this.twin = false; this.log = `${p.name} played ${card.c ? STORM_NAMES[card.c] + ' ' : ''}${stormText(card)}`;
    if (card.c) this.color = card.c;
    if (h.length === 1) this.lastCall = { p, safe: false, t: 3.5 };
    if (h.length === 0) return this.win(p);
    if (card.v === '+2') this.pending += 2;
    if (card.v === 'BLIZZARD') { this.pending += 4; this.c.shake(14); this.fx.confetti(40); }
    if (card.v === 'REV') { this.dir *= -1; if (this.ps.length === 2) { this.turn = this.idx(1); } }
    if (card.v === 'SWAP') { const leader = this.ps.filter(q => q !== p).sort((a, b) => this.hands.get(a).length - this.hands.get(b).length)[0]; const mine = this.hands.get(p); this.hands.set(p, this.hands.get(leader)); this.hands.set(leader, mine); this.log = `${p.name} SWAPPED hands with ${leader.name}! ⇄`; this.c.shake(10); if (this.hands.get(p).length === 0) return this.win(p); }
    if (!card.c) { this.needColor = true; this.sync(); return; }
    this.afterPlay(card);
  }
  afterPlay(card = this.top()) {
    if (card.v === 'TWIN' && this.hands.get(this.cur).some(c => c.c === this.color)) { this.twin = true; this.drew = false; this.sync(); return; }
    this.next(card.v === 'STOP' ? 2 : 1);
  }
  next(k) { this.turn = this.idx(k); this.drew = false; this.twin = false; this.timer = 25; this.botT = rnd(1, 2); this.sync(); }
  win(p) { this.over = true; this.winner = p; this.fx.confetti(200); this.c.sfx.win(); this.c.banner(`🏆 ${p.name} WINS!`, 2500, p.color); this.sync(); this.endT = 2.5; }
  botTurn(p) {
    const h = this.hands.get(p);
    if (this.needColor) { const cnt = {}; h.forEach(c => c.c && (cnt[c.c] = (cnt[c.c] || 0) + 1)); const best = Object.keys(STORM_COLORS).sort((a, b) => (cnt[b] || 0) - (cnt[a] || 0))[0]; return this.onAct(p, { a: 'btn', id: 'col' + best }); }
    const opts = h.map((c, i) => [c, i]).filter(([c]) => this.playable(c));
    if (opts.length) { opts.sort((a, b) => (a[0].c ? 0 : 1) - (b[0].c ? 0 : 1)); return this.play(p, opts[0][1]); }
    if (this.twin || this.drew) return this.onAct(p, { a: 'btn', id: 'pass' });
    this.doDraw(p);
  }
  update(dt) {
    this.t += dt; this.fx.update(dt); if (this.fl) this.fl.update(dt);
    if (this.over) { this.endT -= dt; if (this.endT <= 0 && !this.ended) { this.ended = true; this.c.finish(rankBy(this.ps, p => -this.hands.get(p).length + (p === this.winner ? 100 : 0), p => `${this.hands.get(p).length} cards left`)); } return; }
    if (this.lastCall) { // 'last card' race: the player calls it, or someone catches them
      const lc = this.lastCall; lc.t -= dt;
      if (lc.p.bot && !lc.safe && !lc.tried && lc.t < 3) { lc.tried = true; if (Math.random() < 0.85) { lc.safe = true; this.c.banner(`☝ ${lc.p.name}: LAST CARD!`, 1200, lc.p.color); this.sync(); } }
      if (!lc.safe) { const bot = this.ps.find(q => q.bot && q !== lc.p); if (bot && lc.t < 1.8 && Math.random() < dt * 1.2) this.onAct(bot, { a: 'btn', id: 'catch' }); }
      if (this.lastCall === lc && (lc.t <= 0 || (lc.safe && lc.t < 2.2))) { this.lastCall = null; this.sync(); }
    }
    const p = this.cur;
    if (p.bot) { this.botT -= dt; if (this.botT <= 0) { this.botT = rnd(0.8, 1.5); this.botTurn(p); } return; }
    this.timer -= dt;
    if (this.timer <= 0) { this.timer = 25; if (this.needColor) this.onAct(p, { a: 'btn', id: 'colR' }); else this.botTurn(p); }
  }
  draw(g) {
    cardTable(g, this.t, '#0f4a5c');
    const n = this.ps.length, ink = CARD_INK[this.color];
    // current-color halo + direction arrows sweeping around the pile
    g.save(); const halo = g.createRadialGradient(800, 450, 90, 800, 450, 260); halo.addColorStop(0, shadeHex(ink, 0.2).replace('rgb', 'rgba').replace(')', ',.35)')); halo.addColorStop(1, 'rgba(0,0,0,0)'); g.fillStyle = halo; g.fillRect(500, 150, 600, 600); g.restore();
    g.save(); g.translate(800, 450); g.rotate(this.t * this.dir * 0.5); g.strokeStyle = 'rgba(255,255,255,.35)'; g.lineWidth = 7; g.lineCap = 'round';
    for (let k = 0; k < 2; k++) { g.rotate(Math.PI); g.beginPath(); g.arc(0, 0, 205, -0.9, 0.55); g.stroke(); const ax = Math.cos(0.55) * 205, ay = Math.sin(0.55) * 205; g.save(); g.translate(ax, ay); g.rotate(0.55 + Math.PI / 2 * this.dir); g.fillStyle = 'rgba(255,255,255,.55)'; g.beginPath(); g.moveTo(0, -14 * this.dir); g.lineTo(-12, 6 * this.dir); g.lineTo(12, 6 * this.dir); g.fill(); g.restore(); }
    g.restore();
    // deck with depth
    for (let k = 5; k >= 0; k--) drawStormCard(g, 560 - k * 1.5, 450 - k * 2.5, 128, 186, null, true, -0.04);
    // pile: every card keeps the angle it landed at
    this.pile.filter(c => !c.flying).slice(-6).forEach(c => drawStormCard(g, 800, 450, 150, 220, c, false, c.rot || 0));
    if (this.pending) { g.save(); g.fillStyle = 'rgba(232,51,63,.92)'; rr(g, 900, 300, 130, 64, 32); g.fill(); g.restore(); txt(g, `+${this.pending}`, 965, 333, 40, '#fff'); }
    // seats: fans of real card backs + glass name plates
    this.ps.forEach((p, i) => {
      const s = this.seatAt(i), f = this.fanPos(i), cnt = this.fl && this.fl.l.some(x => x.delay > 0 || x.t < 1) && this.shown.get(p) < this.hands.get(p).length && this.t < 3 ? this.shown.get(p) : this.hands.get(p).length;
      backFan(g, f.x, f.y, cnt, drawStormCard);
      const lc = this.lastCall && this.lastCall.p === p ? (this.lastCall.safe ? ' · ☝ LAST CARD' : ' · ⚠️ 1 card') : '';
      namePlate(g, p, s.x, s.y, i === this.turn && !this.over, `${cnt} card${cnt === 1 ? '' : 's'}${lc}`);
    });
    if (this.fl) this.fl.draw(g, drawStormCard);
    // status line
    g.fillStyle = 'rgba(10,12,22,.65)'; rr(g, 560, 34, 480, 52, 26); g.fill(); txt(g, this.log, 800, 60, 22, '#fff', 'center', 'Nunito');
    g.fillStyle = ink; g.beginPath(); g.arc(800, 640, 14, 0, TAU); g.fill(); txt(g, STORM_NAMES[this.color], 830, 640, 22, '#fff', 'left', 'Nunito');
    this.fx.draw(g);
  }
}

/* =====================================================================
   💣 AVALANCHE! — the deck hides AVALANCHE cards. Draw one without a SHOVEL and you're out.
   On your turn: play any action cards, then DRAW (that ends your turn).
   🛡 SHOVEL dig out (and hide the avalanche back in the deck) · 👀 PEEK top 3 · ⏭ SKIP (end turn, no draw)
   ⏩ DOUBLE (end turn, next player takes 2) · 🔀 SHUFFLE · 🧤 STEAL a card · ✋ BLOCK (cancel an action)
   🧣🎩🧢 pairs: play two of a kind to steal a random card. Last one standing wins!
   ===================================================================== */
const AV = { avalanche: ['💥', 'AVALANCHE', '#ff4d6d'], shovel: ['🛡️', 'SHOVEL', '#51cf66'], peek: ['👀', 'PEEK', '#4dabff'], skip: ['⏭️', 'SKIP', '#ffd43b'], double: ['⏩', 'DOUBLE', '#ff922b'], shuffle: ['🔀', 'SHUFFLE', '#cc5de8'], steal: ['🧤', 'STEAL', '#22d3ee'], block: ['✋', 'BLOCK', '#f783ac'], scarf: ['🧣', 'Scarf', '#868e96'], hat: ['🎩', 'Top hat', '#868e96'], cap: ['🧢', 'Cap', '#868e96'] };
const AV_PAIRS = ['scarf', 'hat', 'cap'];
class Avalanche {
  constructor(c) { this.c = c; this.fx = new Particles(); this.t = 0; this.log = ''; this.flash = 0; }
  start() {
    this.ps = this.c.roster(2); const n = this.ps.length;
    const base = []; const add = (k, m) => { for (let i = 0; i < m; i++) base.push(k); };
    add('peek', 4); add('skip', 4); add('double', 4); add('shuffle', 4); add('steal', 4); add('block', 4); AV_PAIRS.forEach(k => add(k, 4));
    shuffle(base);
    this.hands = new Map(this.ps.map(p => [p, ['shovel', ...base.splice(0, 5)]]));
    this.deck = base; for (let i = 0; i < n - 1; i++) this.deck.push('avalanche'); for (let i = 0; i < (n <= 3 ? 2 : 1); i++) this.deck.push('shovel'); shuffle(this.deck);
    this.alive = new Set(this.ps); this.out = []; this.turn = irnd(0, n - 1); this.owed = 1; this.state = 'play'; this.timer = 25; this.botT = rnd(1.2, 2); this.pendingAct = null; this.over = false; this.peekFor = null;
    this.c.setLayout('all', { name: 'panel' }); this.sync(); this.c.banner('💣 AVALANCHE!');
  }
  get cur() { return this.ps[this.turn]; }
  nextAlive(i) { const n = this.ps.length; for (let k = 1; k <= n; k++) { const j = (i + k) % n; if (this.alive.has(this.ps[j])) return j; } return i; }
  danger() { const a = this.deck.filter(c => c === 'avalanche').length; return this.deck.length ? a / this.deck.length : 0; }
  panelFor(p) {
    const h = this.hands.get(p), my = p === this.cur && !this.over && this.alive.has(p), pa = this.pendingAct;
    const buttons = [];
    if (!this.alive.has(p)) return panelMsg({ title: '❄️ You got buried!', info: 'Watch the others…', cards: [], buttons: [] });
    if (this.state === 'bury' && my) ['top', '2nd', 'middle', 'bottom', 'random'].forEach(k => buttons.push({ id: 'put_' + k, label: k === 'top' ? '⬆ On top (mean!)' : k === 'random' ? '🎲 Random' : k.toUpperCase(), color: '#ff4d6d' }));
    else if (this.state === 'target' && my) this.ps.filter(q => q !== p && this.alive.has(q) && this.hands.get(q).length).forEach(q => buttons.push({ id: 'tgt_' + this.ps.indexOf(q), label: `🧤 ${q.name} (${this.hands.get(q).length})`, color: q.color }));
    else if (my && this.state === 'play' && !pa) buttons.push({ id: 'draw', label: `DRAW ${this.owed > 1 ? `(${this.owed} turns)` : ''}`, color: '#4dabff', big: true });
    if (pa && pa.by !== p && h.includes('block') && !pa.blockedBy) buttons.push({ id: 'block', label: `✋ BLOCK ${AV[pa.card][1]}!`, color: '#f783ac', big: true });
    const cards = h.map(k => { const [icon, label, color] = AV[k]; const pairOk = AV_PAIRS.includes(k) && h.filter(x => x === k).length >= 2; return { icon, label, color, ok: my && this.state === 'play' && !pa && (k === 'peek' || k === 'skip' || k === 'double' || k === 'shuffle' || k === 'steal' || pairOk) }; });
    return panelMsg({ myTurn: my, title: my ? (this.state === 'bury' ? '🛡️ Dug out! Hide the avalanche…' : this.state === 'target' ? '🧤 Steal from who?' : '💣 Your turn — play cards, then DRAW') : `${this.cur.name}'s turn`,
      info: `${this.deck.length} in deck · danger ${Math.round(this.danger() * 100)}%`, sub: this.peekFor === p ? `👀 Top 3: ${this.peekCards.map(k => AV[k][0] + ' ' + AV[k][1]).join(' · ')}` : '', cards, buttons });
  }
  sync() { for (const p of this.ps) if (!p.bot) this.c.send(p, this.panelFor(p)); }
  resync(p) { if (this.hands.has(p)) this.c.send(p, this.panelFor(p)); }
  onAct(p, m) {
    if (this.over || !this.alive.has(p)) return;
    if (m.a === 'btn' && m.id === 'block' && this.pendingAct && this.pendingAct.by !== p) { const h = this.hands.get(p), i = h.indexOf('block'); if (i < 0) return; h.splice(i, 1); this.pendingAct.blockedBy = p; this.log = `✋ ${p.name} BLOCKED ${AV[this.pendingAct.card][1]}!`; this.c.sfx.zap(); this.c.shake(6); return this.sync(); }
    if (p !== this.cur) return;
    if (this.state === 'bury' && m.a === 'btn' && m.id && m.id.startsWith('put_')) return this.bury(m.id.slice(4));
    if (this.state === 'target' && m.a === 'btn' && m.id && m.id.startsWith('tgt_')) { const q = this.ps[+m.id.slice(4)]; this.state = 'play'; return this.steal(p, q); }
    if (this.state !== 'play' || this.pendingAct) return;
    if (m.a === 'btn' && m.id === 'draw') return this.drawCard(p);
    if (m.a === 'card') {
      const h = this.hands.get(p), k = h[m.i | 0]; if (!k) return;
      if (AV_PAIRS.includes(k)) { if (h.filter(x => x === k).length < 2) return; h.splice(h.indexOf(k), 1); h.splice(h.indexOf(k), 1); return this.announce(p, k, 'pair'); }
      if (!['peek', 'skip', 'double', 'shuffle', 'steal'].includes(k)) return;
      h.splice(m.i | 0, 1); this.announce(p, k);
    }
  }
  // actions wait 2.5 s so others can BLOCK them
  announce(p, card, kind) { (this.discard = this.discard || []).push({ k: card, rot: rnd(-0.3, 0.3) }); this.pendingAct = { by: p, card, kind, t: 2.5, blockedBy: null }; this.log = `${p.name} plays ${AV[card][0]} ${kind === 'pair' ? 'a PAIR' : AV[card][1]}…`; this.c.sfx.card(); this.sync(); }
  resolve(a) {
    const p = a.by; this.pendingAct = null;
    if (a.blockedBy) { this.sync(); return; }
    if (a.kind === 'pair' || a.card === 'steal') { const opts = this.ps.filter(q => q !== p && this.alive.has(q) && this.hands.get(q).length); if (!opts.length) return this.sync(); if (p.bot) return this.steal(p, opts.sort((x, y) => this.hands.get(y).length - this.hands.get(x).length)[0]); this.state = 'target'; return this.sync(); }
    if (a.card === 'peek') { this.peekFor = p; this.peekCards = this.deck.slice(-3).reverse(); this.peekT = 6; this.log = `👀 ${p.name} peeked at the deck`; if (p.bot) p.knowsTop = this.peekCards[0]; }
    if (a.card === 'shuffle') { shuffle(this.deck); this.log = `🔀 ${p.name} shuffled the deck`; this.c.sfx.whoosh(); this.ps.forEach(q => (q.knowsTop = null)); }
    if (a.card === 'skip') { this.log = `⏭️ ${p.name} skips`; return this.endTurn(false); }
    if (a.card === 'double') { this.log = `⏩ ${p.name}: next player takes 2 turns!`; this.owed = 1; const nx = this.nextAlive(this.turn); this.turn = nx; this.owed = 2; this.startTurn(); return; }
    this.sync();
  }
  steal(p, q) { const h = this.hands.get(q); if (!h.length) return this.sync(); const k = h.splice(irnd(0, h.length - 1), 1)[0]; this.hands.get(p).push(k); this.log = `🧤 ${p.name} stole a card from ${q.name}`; this.c.sfx.pickup(); this.sync(); }
  drawCard(p) {
    const k = this.deck.pop(); p.knowsTop = null;
    if (k !== 'avalanche') { this.hands.get(p).push(k); this.log = `${p.name} drew a card… phew 😮‍💨`; this.c.sfx.card(); return this.endTurn(true); }
    // AVALANCHE!
    this.flash = 1; this.c.shake(30); this.c.sfx.boom(); this.c.sfx.alarm(); this.fx.burst(800, 450, 160, '#ffffff', 700, 1.6, 9);
    const h = this.hands.get(p), si = h.indexOf('shovel');
    if (si >= 0) { h.splice(si, 1); this.log = `💥 AVALANCHE! ${p.name} dug out with a SHOVEL 🛡️`; this.state = 'bury'; this.c.banner(`🛡️ ${p.name} DUG OUT!`, 1600, '#51cf66'); this.buryT = 12; return this.sync(); }
    this.alive.delete(p); this.out.push(p); this.log = `💥 AVALANCHE! ${p.name} is buried ❄️`; this.c.banner(`❄️ ${p.name} IS OUT!`, 2000, '#ff4d6d');
    if (this.alive.size <= 1) { this.over = true; const w = [...this.alive][0]; this.winner = w; this.fx.confetti(200); this.c.sfx.win(); this.c.banner(`🏆 ${w.name} SURVIVES!`, 2500, w.color); this.endT = 2.8; return this.sync(); }
    this.owed = 1; this.turn = this.nextAlive(this.turn); this.startTurn();
  }
  bury(where) {
    const n = this.deck.length, pos = where === 'top' ? n : where === '2nd' ? Math.max(0, n - 1) : where === 'middle' ? Math.floor(n / 2) : where === 'bottom' ? 0 : irnd(0, n);
    this.deck.splice(pos, 0, 'avalanche'); this.state = 'play'; this.log = `🤫 ${this.cur.name} hid the avalanche back in the deck…`; this.ps.forEach(q => (q.knowsTop = null)); this.endTurn(true);
  }
  endTurn(drew) { this.owed--; if (this.owed > 0) { this.startTurn(); return; } this.owed = 1; this.turn = this.nextAlive(this.turn); this.startTurn(); }
  startTurn() { this.timer = 25; this.botT = rnd(1.2, 2.2); this.peekFor = null; this.state = 'play'; this.sync(); }
  botMove(p) {
    const h = this.hands.get(p), d = this.danger();
    const has = k => h.indexOf(k);
    if (p.knowsTop === 'avalanche' || (d > 0.3 && Math.random() < 0.6)) { for (const k of ['skip', 'double', 'shuffle']) if (has(k) >= 0) return this.onAct(p, { a: 'card', i: has(k) }); }
    if (d > 0.15 && has('peek') >= 0 && !p.knowsTop && Math.random() < 0.5) return this.onAct(p, { a: 'card', i: has('peek') });
    if (Math.random() < 0.25) { const pr = AV_PAIRS.find(k => h.filter(x => x === k).length >= 2); if (pr) return this.onAct(p, { a: 'card', i: has(pr) }); if (has('steal') >= 0) return this.onAct(p, { a: 'card', i: has('steal') }); }
    this.drawCard(p);
  }
  update(dt) {
    this.t += dt; this.fx.update(dt); this.flash = Math.max(0, this.flash - dt * 1.5);
    if (this.over) { this.endT -= dt; if (this.endT <= 0 && !this.ended) { this.ended = true; this.c.finish(survivorRank(this.ps, p => this.alive.has(p), this.out)); } return; }
    if (this.peekFor) { this.peekT -= dt; if (this.peekT <= 0) { this.peekFor = null; this.sync(); } }
    const a = this.pendingAct;
    if (a) {
      a.t -= dt;
      if (!a.blockedBy) for (const q of this.ps) if (q.bot && q !== a.by && this.alive.has(q) && this.hands.get(q).includes('block') && ['double', 'steal'].includes(a.card) && Math.random() < dt * 0.6) this.onAct(q, { a: 'btn', id: 'block' });
      if (a.t <= 0) this.resolve(a);
      return;
    }
    const p = this.cur;
    if (this.state === 'bury') { this.buryT -= dt; if (this.buryT <= 0 || (p.bot && this.buryT < 10.8)) this.bury(p.bot ? pick(['random', 'top', 'middle', 'bottom', '2nd']) : 'random'); return; }
    if (p.bot) { this.botT -= dt; if (this.botT <= 0) { this.botT = rnd(1, 1.8); if (this.state === 'target') { const q = this.ps.find(x => x !== p && this.alive.has(x) && this.hands.get(x).length); this.state = 'play'; if (q) this.steal(p, q); } else this.botMove(p); } return; }
    this.timer -= dt; if (this.timer <= 0) { this.timer = 25; if (this.state === 'target') { this.state = 'play'; this.sync(); } else this.drawCard(p); }
  }
  draw(g) {
    // night mountain
    const gr = g.createLinearGradient(0, 0, 0, 900); gr.addColorStop(0, '#0b1433'); gr.addColorStop(1, '#2b4a7a'); g.fillStyle = gr; g.fillRect(0, 0, 1600, 900);
    g.fillStyle = '#dbe4ff'; g.beginPath(); g.moveTo(0, 620); g.lineTo(300, 300); g.lineTo(520, 520); g.lineTo(800, 180); g.lineTo(1080, 500); g.lineTo(1300, 320); g.lineTo(1600, 600); g.lineTo(1600, 900); g.lineTo(0, 900); g.fill();
    g.fillStyle = '#a5b4d8'; g.beginPath(); g.moveTo(800, 180); g.lineTo(700, 330); g.lineTo(760, 300); g.lineTo(800, 360); g.lineTo(850, 300); g.lineTo(900, 330); g.fill();
    const d = this.danger();
    // the deck: a wobbling pile, redder as danger grows
    const wob = Math.sin(this.t * (6 + d * 20)) * d * 6;
    for (let i = Math.min(10, this.deck.length) - 1; i >= 0; i--) drawAvCard(g, 800 + wob - i * 1.5, 445 - i * 3, 160, 232, null, -0.02);
    if (d > 0.12) { g.save(); g.globalAlpha = Math.min(0.55, d); g.fillStyle = '#ff4d5e'; rr(g, 720 + wob, 329, 160, 232, 15); g.fill(); g.restore(); } 
    g.fillStyle = 'rgba(10,12,22,.72)'; rr(g, 600, 104, 400, 44, 22); g.fill(); g.fillStyle = d > 0.3 ? '#ff4d5e' : d > 0.15 ? '#ff9f43' : '#34c77b'; rr(g, 604, 108, 392 * Math.min(1, d * 2.2) + 36, 36, 18); g.fill(); txt(g, `${this.deck.length} CARDS  ·  DANGER ${Math.round(d * 100)}%`, 800, 127, 19, '#fff', 'center', 'Nunito');
    const n = this.ps.length;
    this.ps.forEach((p, i) => {
      const s = seatPos(i, n, 800, 470, 640, 320), alive = this.alive.has(p);
      g.globalAlpha = alive ? 1 : 0.35; if (alive) backFan(g, s.x + (800 - s.x) * 0.3, s.y + (470 - s.y) * 0.42, this.hands.get(p).length, (gg, x, y, w, h) => drawAvCard(gg, x, y, w, h, null)); namePlate(g, p, s.x, s.y, i === this.turn && alive && !this.over, alive ? `${this.hands.get(p).length} cards${i === this.turn && this.owed > 1 ? ' · 2 turns' : ''}` : '❄️ buried'); g.globalAlpha = 1;
    });
    (this.discard || []).slice(-4).forEach((d, k, arr) => drawAvCard(g, 1050, 440, 130, 190, d.k, d.rot));
    if (this.pendingAct) { const a = this.pendingAct, [ic, lb] = AV[a.card]; g.fillStyle = 'rgba(10,6,30,.85)'; rr(g, 560, 200, 480, 110, 24); g.fill(); txt(g, `${ic} ${a.kind === 'pair' ? 'PAIR → STEAL' : lb}`, 800, 240, 44, a.blockedBy ? '#868e96' : '#ffd43b'); txt(g, a.blockedBy ? `✋ BLOCKED by ${a.blockedBy.name}!` : `block it? ${a.t.toFixed(1)}s`, 800, 285, 24, a.blockedBy ? '#f783ac' : '#fff', 'center', 'Nunito'); }
    txt(g, this.log, 800, 70, 30, '#fff', 'center', 'Nunito');
    if (this.flash > 0) { g.fillStyle = `rgba(255,255,255,${this.flash})`; g.fillRect(0, 0, 1600, 900); }
    this.fx.draw(g);
  }
}

/* =====================================================================
   🏔️ SNOW TYCOON — a winter resort property game.
   Roll, move, BUY spots, charge rent. Own a whole color → build CABINS (up to 3) for huge rent.
   Pass START: +$200. Snow Chance cards. Avalanche corner sends you to the Hot Tub.
   Ends after 10 rounds (or when one tycoon is left): richest (cash + property) wins.
   ===================================================================== */
const TY_COL = { brown: '#a0522d', cyan: '#22d3ee', pink: '#f783ac', orange: '#ff922b', purple: '#9775fa', gold: '#ffd43b' };
const TY_BOARD = [
  { k: 'start', n: 'START', i: '🏁' }, { k: 'prop', n: 'Igloo Lane', p: 60, g: 'brown' }, { k: 'chance', n: 'Snow Chance', i: '❄️' }, { k: 'prop', n: 'Penguin Pier', p: 70, g: 'brown' }, { k: 'lift', n: 'North Lift', p: 150, i: '🚡' }, { k: 'tax', n: 'Plow Tax', a: 100, i: '🚜' },
  { k: 'tub', n: 'Hot Tub', i: '♨️' }, { k: 'prop', n: 'Cocoa Café', p: 100, g: 'cyan' }, { k: 'prop', n: 'Frost Market', p: 110, g: 'cyan' }, { k: 'chance', n: 'Snow Chance', i: '❄️' }, { k: 'lift', n: 'East Lift', p: 150, i: '🚡' }, { k: 'prop', n: 'Mitten Mall', p: 130, g: 'pink' },
  { k: 'pot', n: 'Sleigh Ride', i: '🛷' }, { k: 'prop', n: 'Sled Shop', p: 140, g: 'pink' }, { k: 'chance', n: 'Snow Chance', i: '❄️' }, { k: 'prop', n: 'Ice Rink', p: 170, g: 'orange' }, { k: 'prop', n: 'Yeti Cave', p: 180, g: 'orange' }, { k: 'lift', n: 'South Lift', p: 150, i: '🚡' },
  { k: 'aval', n: 'Avalanche!', i: '💥' }, { k: 'prop', n: 'Aurora Hotel', p: 220, g: 'purple' }, { k: 'chance', n: 'Snow Chance', i: '❄️' }, { k: 'prop', n: 'Glacier Spa', p: 240, g: 'purple' }, { k: 'tax', n: 'Luxury Tax', a: 150, i: '💎' }, { k: 'prop', n: 'Snow Palace', p: 300, g: 'gold' },
];
const TY_CHANCE = [
  ['🏆 You won the ski race!', 'cash', 100], ['🛷 Your sled broke. Repairs:', 'cash', -80], ['🎿 Free lift pass — go to START!', 'goto', 0], ['❄️ Blizzard! Move back 3 spaces.', 'back', 3],
  ['💥 Caught in an avalanche — to the Hot Tub!', 'aval', 0], ['🎂 Snow party! Every player pays you $20.', 'each', 20], ['☕ You buy everyone cocoa: pay $20 each.', 'each', -20], ['🔨 Free cabin on one of your sets!', 'cabin', 0],
  ['🎁 Holiday bonus!', 'cash', 150], ['🧊 You slipped on ice. Doctor bill:', 'cash', -60],
];
class SnowTycoon {
  constructor(c) { this.c = c; this.fx = new Particles(); this.t = 0; this.log = 'Welcome to SNOW TYCOON!'; this.ROUNDS = 10; }
  start() {
    this.ps = this.c.roster(2).slice(0, 6);
    this.st = new Map(this.ps.map(p => [p, { pos: 0, cash: 1000, out: false, skip: 0, shown: 0 }]));
    this.own = TY_BOARD.map(() => null); this.cab = TY_BOARD.map(() => 0); this.pot = 0;
    this.turn = 0; this.round = 1; this.state = 'roll'; this.dice = [1, 1]; this.rollAnim = 0; this.timer = 20; this.botT = rnd(1, 1.6); this.outOrder = [];
    this.c.setLayout('all', { name: 'panel' }); this.sync(); this.c.banner('🏔️ SNOW TYCOON!');
  }
  get cur() { return this.ps[this.turn]; }
  S(p) { return this.st.get(p); }
  group(gk) { return TY_BOARD.map((b, i) => [b, i]).filter(([b]) => b.g === gk).map(([, i]) => i); }
  hasSet(p, gk) { return this.group(gk).every(i => this.own[i] === p); }
  rent(i) {
    const b = TY_BOARD[i], o = this.own[i]; if (!o) return 0;
    if (b.k === 'lift') { const n = TY_BOARD.filter((x, j) => x.k === 'lift' && this.own[j] === o).length; return 25 * Math.pow(2, n - 1); }
    const base = Math.round(b.p / 8); if (this.cab[i]) return base * [1, 4, 9, 16][this.cab[i]]; return this.hasSet(o, b.g) ? base * 2 : base;
  }
  worth(p) { let w = this.S(p).cash; this.own.forEach((o, i) => { if (o === p) w += TY_BOARD[i].p + this.cab[i] * Math.round(TY_BOARD[i].p / 2); }); return w; }
  buildable(p) { const out = []; TY_BOARD.forEach((b, i) => { if (b.k === 'prop' && this.own[i] === p && this.hasSet(p, b.g) && this.cab[i] < 3 && this.S(p).cash >= Math.round(b.p / 2)) out.push(i); }); return out; }
  panelFor(p) {
    const s = this.S(p), my = p === this.cur && !this.over && !s.out, buttons = [];
    if (s.out) return panelMsg({ title: '💸 Bankrupt!', info: 'Watch the tycoons…', cards: [], buttons: [] });
    if (my && this.state === 'roll') { buttons.push({ id: 'roll', label: '🎲 ROLL', color: '#ffd43b', big: true }); this.buildable(p).slice(0, 3).forEach(i => buttons.push({ id: 'build_' + i, label: `🏠 Cabin on ${TY_BOARD[i].n} ($${Math.round(TY_BOARD[i].p / 2)})`, color: TY_COL[TY_BOARD[i].g] })); }
    if (my && this.state === 'buy') { const b = TY_BOARD[s.pos]; buttons.push({ id: 'buy', label: `💰 BUY ${b.n} — $${b.p}`, color: '#51cf66', big: true, off: s.cash < b.p }); buttons.push({ id: 'pass', label: 'No thanks', color: '#868e96' }); }
    const props = TY_BOARD.map((b, i) => [b, i]).filter(([b, i]) => this.own[i] === p).map(([b, i]) => ({ icon: b.k === 'lift' ? '🚡' : '🏠'.repeat(this.cab[i]) || '📍', label: b.n, sub: `rent $${this.rent(i)}`, color: b.g ? TY_COL[b.g] : '#495057', ok: false }));
    return panelMsg({ myTurn: my, title: my ? (this.state === 'buy' ? `Buy ${TY_BOARD[s.pos].n}?` : this.state === 'roll' ? '🎲 Your turn — roll!' : '…') : `${this.cur.name}'s turn`, info: `💵 $${s.cash} · worth $${this.worth(p)}`, sub: `Round ${this.round}/${this.ROUNDS} · ${this.log}`, cards: props, buttons });
  }
  sync() { for (const p of this.ps) if (!p.bot) this.c.send(p, this.panelFor(p)); }
  resync(p) { if (this.st.has(p)) this.c.send(p, this.panelFor(p)); }
  onAct(p, m) {
    if (this.over || p !== this.cur || m.a !== 'btn') return;
    const s = this.S(p);
    if (this.state === 'roll' && m.id === 'roll') return this.roll(p);
    if (this.state === 'roll' && m.id.startsWith('build_')) { const i = +m.id.slice(6); if (!this.buildable(p).includes(i)) return; s.cash -= Math.round(TY_BOARD[i].p / 2); this.cab[i]++; this.log = `🏠 ${p.name} built a cabin on ${TY_BOARD[i].n}!`; this.c.sfx.pickup(); this.fx.text(800, 400, '🏠 +1', '#ffd43b', 40); return this.sync(); }
    if (this.state === 'buy' && m.id === 'buy') { const b = TY_BOARD[s.pos]; if (s.cash < b.p) return; s.cash -= b.p; this.own[s.pos] = p; this.log = `💰 ${p.name} bought ${b.n}!`; this.c.sfx.win(); if (b.g && this.hasSet(p, b.g)) { this.c.banner(`⭐ ${p.name} owns all ${b.g.toUpperCase()}!`, 1600, TY_COL[b.g]); } return this.endTurn(); }
    if (this.state === 'buy' && m.id === 'pass') { this.log = `${p.name} passed on ${TY_BOARD[s.pos].n}`; return this.endTurn(); }
  }
  roll(p) {
    this.dice = [irnd(1, 6), irnd(1, 6)]; this.state = 'moving'; this.rollAnim = 0.8; this.steps = this.dice[0] + this.dice[1]; this.stepT = 0.8; this.c.sfx.card(); this.sync();
  }
  land(p) {
    const s = this.S(p), b = TY_BOARD[s.pos];
    if (b.k === 'prop' || b.k === 'lift') {
      const o = this.own[s.pos];
      if (!o) { this.state = 'buy'; this.timer = 15; this.log = `${p.name} landed on ${b.n} ($${b.p})`; return this.sync(); }
      if (o !== p && !this.S(o).out) { const r = this.rent(s.pos); this.pay(p, o, r); this.log = `💸 ${p.name} pays $${r} rent to ${o.name}`; this.c.sfx.buzz(); }
      else this.log = `${p.name} relaxes at their own ${b.n}`;
    } else if (b.k === 'tax') { this.pay(p, null, b.a); this.pot += b.a; this.log = `${b.i} ${p.name} pays ${b.n} $${b.a}`; }
    else if (b.k === 'chance') { const [txtC, kind, v] = pick(TY_CHANCE); this.log = `❄️ ${p.name}: ${txtC}${kind === 'cash' ? ` $${Math.abs(v)}` : ''}`; this.c.banner(txtC, 2000, '#a5d8ff');
      if (kind === 'cash') { if (v > 0) s.cash += v; else { this.pay(p, null, -v); this.pot += -v; } }
      if (kind === 'goto') { s.pos = 0; s.cash += 200; }
      if (kind === 'back') { s.pos = (s.pos + TY_BOARD.length - v) % TY_BOARD.length; return this.land(p); }
      if (kind === 'aval') { s.pos = 6; s.skip = 1; this.c.shake(16); this.c.sfx.boom(); }
      if (kind === 'each') this.ps.filter(q => q !== p && !this.S(q).out).forEach(q => (v > 0 ? this.pay(q, p, v) : this.pay(p, q, -v)));
      if (kind === 'cabin') { const sets = TY_BOARD.map((b2, i) => i).filter(i => TY_BOARD[i].k === 'prop' && this.own[i] === p && this.hasSet(p, TY_BOARD[i].g) && this.cab[i] < 3); if (sets.length) this.cab[sets[0]]++; }
    }
    else if (b.k === 'aval') { s.pos = 6; s.skip = 1; this.log = `💥 AVALANCHE! ${p.name} is stuck in the Hot Tub (miss a turn)`; this.c.shake(20); this.c.sfx.boom(); this.fx.burst(800, 450, 80, '#fff', 500, 1.2, 7); }
    else if (b.k === 'pot') { if (this.pot) { s.cash += this.pot; this.log = `🛷 ${p.name} grabs the Sleigh Ride pot: $${this.pot}!`; this.c.sfx.win(); this.fx.confetti(60); this.pot = 0; } else this.log = `🛷 ${p.name} enjoys a sleigh ride`; }
    else this.log = `${p.name} is at ${b.n}`;
    this.endTurn();
  }
  pay(p, to, amt) {
    const s = this.S(p);
    // can't afford it? sell cabins, then property (half price)
    while (s.cash < amt) {
      const ci = this.cab.findIndex((c, i) => c > 0 && this.own[i] === p); if (ci >= 0) { this.cab[ci]--; s.cash += Math.round(TY_BOARD[ci].p / 4); continue; }
      const pi = this.own.findIndex(o => o === p); if (pi >= 0) { this.own[pi] = null; s.cash += Math.round(TY_BOARD[pi].p / 2); continue; }
      break;
    }
    const paid = Math.min(amt, Math.max(0, s.cash)); s.cash -= amt; if (to) this.S(to).cash += paid;
    if (s.cash < 0) { s.out = true; s.cash = 0; this.outOrder.push(p); this.own = this.own.map(o => (o === p ? null : o)); this.c.banner(`💸 ${p.name} is BANKRUPT!`, 2000, '#ff4d6d'); }
  }
  endTurn() {
    this.state = 'roll';
    const alive = this.ps.filter(p => !this.S(p).out);
    if (alive.length <= 1) return this.finish();
    let n = 0;
    do { this.turn = (this.turn + 1) % this.ps.length; if (this.turn === 0) this.round++; n++; } while (this.S(this.cur).out && n < 20);
    if (this.round > this.ROUNDS) return this.finish();
    const s = this.S(this.cur);
    if (s.skip) { s.skip = 0; this.log = `♨️ ${this.cur.name} is relaxing in the Hot Tub…`; this.sync(); this.skipT = 1.6; this.state = 'skip'; return; }
    this.timer = 20; this.botT = rnd(1, 1.6); this.sync();
  }
  finish() { if (this.over) return; this.over = true; const best = [...this.ps].sort((a, b) => this.worth(b) - this.worth(a))[0]; this.fx.confetti(200); this.c.sfx.win(); this.c.banner(`🏆 ${best.name} is the SNOW TYCOON!`, 2500, best.color); this.sync(); this.endT = 3; }
  botMove(p) {
    const s = this.S(p);
    if (this.state === 'roll') { const bl = this.buildable(p); if (bl.length && s.cash - Math.round(TY_BOARD[bl[0]].p / 2) > 150) return this.onAct(p, { a: 'btn', id: 'build_' + bl[0] }); return this.onAct(p, { a: 'btn', id: 'roll' }); }
    if (this.state === 'buy') { const b = TY_BOARD[s.pos]; return this.onAct(p, { a: 'btn', id: s.cash - b.p > 120 ? 'buy' : 'pass' }); }
  }
  update(dt) {
    this.t += dt; this.fx.update(dt);
    for (const p of this.ps) { const s = this.S(p); s.shown += (s.cash - s.shown) * Math.min(1, dt * 6); }
    if (this.over) { this.endT -= dt; if (this.endT <= 0 && !this.ended) { this.ended = true; this.c.finish(rankBy(this.ps, p => this.worth(p) + (this.S(p).out ? -10000 + this.outOrder.indexOf(p) : 0), p => `$${this.worth(p)} worth`)); } return; }
    if (this.state === 'skip') { this.skipT -= dt; if (this.skipT <= 0) this.endTurn(); return; }
    if (this.state === 'moving') {
      this.rollAnim = Math.max(0, this.rollAnim - dt);
      if (this.rollAnim > 0) return;
      this.stepT -= dt;
      if (this.stepT <= 0 && this.steps > 0) { const s = this.S(this.cur); s.pos = (s.pos + 1) % TY_BOARD.length; if (s.pos === 0) { s.cash += 200; this.fx.text(800, 380, '+$200 START', '#51cf66', 34); this.c.sfx.ding(); } this.steps--; this.stepT = 0.22; this.c.sfx.tick(); if (this.steps === 0) this.land(this.cur); }
      return;
    }
    const p = this.cur;
    if (p.bot) { this.botT -= dt; if (this.botT <= 0) { this.botT = rnd(1, 1.6); this.botMove(p); } return; }
    this.timer -= dt; if (this.timer <= 0) { this.timer = 20; this.onAct(p, { a: 'btn', id: this.state === 'buy' ? 'pass' : 'roll' }); }
  }
  cell(i) { // board squares around a rectangle: 7 per side incl. corners
    const side = Math.floor(i / 6), k = i % 6, L = 150, T = 70, W = 1300, H = 760, cw = W / 7, ch = H / 7; // 7 cells per side, corners shared
    if (side === 0) return { x: L + W - k * cw - cw / 2, y: T + H - ch / 2, w: cw, h: ch };
    if (side === 1) return { x: L + cw / 2, y: T + H - k * ch - ch / 2, w: cw, h: ch };
    if (side === 2) return { x: L + k * cw + cw / 2, y: T + ch / 2, w: cw, h: ch };
    return { x: L + W - cw / 2, y: T + k * ch + ch / 2, w: cw, h: ch };
  }
  draw(g) {
    const gr = g.createLinearGradient(0, 0, 0, 900); gr.addColorStop(0, '#0b1433'); gr.addColorStop(1, '#1b2b5a'); g.fillStyle = gr; g.fillRect(0, 0, 1600, 900);
    g.fillStyle = '#e7f5ff'; rr(g, 140, 60, 1320, 780, 30); g.fill();
    TY_BOARD.forEach((b, i) => {
      const c = this.cell(i), o = this.own[i];
      g.fillStyle = '#f8f9fa'; rr(g, c.x - c.w / 2 + 3, c.y - c.h / 2 + 3, c.w - 6, c.h - 6, 10); g.fill();
      if (b.g) { g.fillStyle = TY_COL[b.g]; rr(g, c.x - c.w / 2 + 3, c.y - c.h / 2 + 3, c.w - 6, 22, 10); g.fill(); }
      if (o) { g.lineWidth = 6; g.strokeStyle = o.color; rr(g, c.x - c.w / 2 + 5, c.y - c.h / 2 + 5, c.w - 10, c.h - 10, 10); g.stroke(); }
      txt(g, b.i || (b.k === 'prop' ? '' : ''), c.x, c.y - 8, 30);
      txt(g, b.n, c.x, c.y + (b.i ? 26 : 2), 15, '#0a1024', 'center', 'Nunito');
      if (b.p) txt(g, o ? `rent $${this.rent(i)}` : `$${b.p}`, c.x, c.y + 40, 14, o ? '#c92a2a' : '#495057', 'center', 'Nunito');
      if (this.cab[i]) txt(g, '🏠'.repeat(this.cab[i]), c.x, c.y - 28, 18);
    });
    // center
    g.fillStyle = '#1c3d7a'; rr(g, 390, 200, 820, 500, 26); g.fill();
    txt(g, 'SNOW TYCOON', 800, 250, 52, '#ffd43b'); txt(g, `Round ${Math.min(this.round, this.ROUNDS)} / ${this.ROUNDS}${this.pot ? ` · 🛷 pot $${this.pot}` : ''}`, 800, 300, 24, '#a5d8ff', 'center', 'Nunito');
    const shake = this.rollAnim > 0 ? this.rollAnim * 14 : 0;
    this.dice.forEach((d, k) => { const x = 740 + k * 120 + rnd(-shake, shake), y = 400 + rnd(-shake, shake), v = this.rollAnim > 0 ? irnd(1, 6) : d; g.fillStyle = '#fff'; rr(g, x - 45, y - 45, 90, 90, 18); g.fill(); g.fillStyle = '#0a1024'; const P = { 1: [[0, 0]], 2: [[-1, -1], [1, 1]], 3: [[-1, -1], [0, 0], [1, 1]], 4: [[-1, -1], [1, -1], [-1, 1], [1, 1]], 5: [[-1, -1], [1, -1], [0, 0], [-1, 1], [1, 1]], 6: [[-1, -1], [1, -1], [-1, 0], [1, 0], [-1, 1], [1, 1]] }[v]; P.forEach(([a, b2]) => { g.beginPath(); g.arc(x + a * 24, y + b2 * 24, 9, 0, TAU); g.fill(); }); });
    para(g, this.log, 800, 510, 760, 26, '#fff');
    // leaderboard
    const rows = [...this.ps].sort((a, b) => this.worth(b) - this.worth(a));
    rows.forEach((p, k) => { const s = this.S(p), y = 580 + k * 22; if (k > 4) return; txt(g, `${p === this.cur ? '▶ ' : ''}${p.name}`, 560, y, 18, s.out ? '#868e96' : p.color, 'left', 'Nunito'); txt(g, s.out ? 'bankrupt' : `$${Math.round(s.shown)}  (worth $${this.worth(p)})`, 1040, y, 18, '#fff', 'right', 'Nunito'); });
    // tokens (hop between squares)
    this.ps.forEach((p, k) => { const s = this.S(p); if (s.out) return; const c = this.cell(s.pos), hop = p === this.cur && this.state === 'moving' ? Math.abs(Math.sin(this.stepT * 14)) * 18 : 0; drawBlob(g, c.x - 40 + (k % 3) * 40, c.y + 10 + Math.floor(k / 3) * 30 - hop, 16, p.color, -Math.PI / 2, { shadow: false, happy: p === this.cur }); });
    this.fx.draw(g);
  }
}
