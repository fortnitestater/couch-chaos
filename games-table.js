/* Couch Chaos — 🃏 Cards & table games (no motion needed). Needs engine.js + games-classic.js (drawCard) */

function feltBg(g, c1 = '#2b8a3e', c2 = '#0b3d1e') { bgGrad(g, c1, c2); g.strokeStyle = 'rgba(255,255,255,.06)'; g.lineWidth = 6; g.beginPath(); g.ellipse(800, 470, 640, 340, 0, 0, TAU); g.stroke(); }
function turnChoice(p, prompt, labels, disabled = [], cols = 4, big = false) {
  return { name: 'choice', prompt, cols, big, options: labels.map((l, i) => ({ label: String(l), color: OPT_COLORS[i % 10], disabled: !!disabled[i] })) };
}

/* ---------------------------------------------------------------- 🂡 HIGHER OR LOWER */
class HigherLower extends QuizGame {
  constructor(c) { super(c, { rounds: 10, time: 9, title: 'HIGHER OR LOWER', bg: ['#1e6b3a', '#06240f'], botSkill: 0.6 }); }
  init() { this.cur = this.card(); }
  card(not) { let v; do { v = irnd(1, 13); } while (v === (not && not.v)); return { v, s: pick(['♠', '♥', '♦', '♣']) }; }
  name(c) { return ({ 1: 'A', 11: 'J', 12: 'Q', 13: 'K' })[c.v] || String(c.v); }
  makeRound() {
    const cur = this.cur, nxt = this.card(cur), self = this;
    return {
      text: '', phone: `Current card: ${this.name(cur)}${cur.s}. Next one is…`, opts: ['⬆️ HIGHER', '⬇️ LOWER'], cols: 2, big: true, correct: nxt.v > cur.v ? 0 : 1,
      revealText: `It was ${this.name(nxt)}${nxt.s}!`,
      resolve: (ans, gm) => { self.cur = nxt; return gm.defaultResolve(); },
      botAnswer: () => (cur.v < 7 ? 0 : cur.v > 7 ? 1 : irnd(0, 1)),
      draw(g, gm) {
        drawCard(g, gm.phase === 'reveal' ? 640 : 800, 300, 170, 238, self.name(cur) + cur.s, -0.04);
        if (gm.phase === 'reveal') drawCard(g, 960, 300, 170, 238, self.name(nxt) + nxt.s, 0.05);
        else drawCard(g, 1010, 300, 150, 210, null, 0.08);
      },
    };
  }
}

/* ---------------------------------------------------------------- 🔴 FOUR IN A ROW (teams, turn-based) */
class FourInRow {
  constructor(c) { this.c = c; this.fx = new Particles(); this.grid = Array.from({ length: 6 }, () => Array(7).fill(-1)); this.turn = 0; this.drop = null; this.over = false; this.TC = ['#ff4d6d', '#ffd43b']; this.TN = ['RED', 'YELLOW']; }
  start() {
    this.ps = this.c.roster(2);
    this.teams = [[], []]; this.ps.forEach((p, i) => this.teams[i % 2].push(p));
    this.ptr = [0, 0]; this.placed = new Map(this.ps.map(p => [p, 0]));
    this.c.setLayout('all', { name: 'msg', emoji: '🔴', text: 'Four in a Row', sub: 'Teams take turns. Line up 4!' });
    setTimeout(() => this.nextTurn(true), 1200);
  }
  get cur() { const t = this.teams[this.turn]; return t[this.ptr[this.turn] % t.length]; }
  full(c) { return this.grid[0][c] !== -1; }
  nextTurn(first) {
    if (this.over) return;
    if (!first) { this.ptr[this.turn]++; this.turn = 1 - this.turn; }
    this.t = 25; const p = this.cur;
    for (const q of this.ps) if (!q.bot) this.c.setLayout(q, q === p ? turnChoice(q, `Your turn (${this.TN[this.turn]})! Pick a column`, [1, 2, 3, 4, 5, 6, 7], [0, 1, 2, 3, 4, 5, 6].map(c => this.full(c)), 7, true)
      : { name: 'msg', emoji: this.teams[this.turn].includes(q) ? '🤝' : '⏳', text: `${p.name} is choosing…`, sub: `Team ${this.TN[this.turn]}'s turn — help them out loud!` });
    this.botT = p.bot ? rnd(1, 2) : null;
  }
  onAct(p, m) { if (m.a === 'choice' && p === this.cur && !this.drop && !this.over) this.play(m.i | 0); }
  play(c) {
    if (c < 0 || c > 6 || this.full(c)) return;
    let r = 5; while (this.grid[r][c] !== -1) r--;
    this.drop = { c, r, y: 120, team: this.turn, p: this.cur }; this.c.sfx.whoosh();
    for (const q of this.ps) if (!q.bot && q === this.cur) this.c.setLayout(q, { name: 'msg', emoji: '⬇️', text: 'Dropping…' });
  }
  cell(r, c) { return { x: 800 + (c - 3) * 110, y: 230 + r * 100 }; }
  winLine(team) {
    const G = this.grid, D = [[0, 1], [1, 0], [1, 1], [1, -1]];
    for (let r = 0; r < 6; r++) for (let c = 0; c < 7; c++) for (const [dr, dc] of D) {
      const line = []; for (let k = 0; k < 4; k++) { const rr2 = r + dr * k, cc = c + dc * k; if (rr2 < 0 || rr2 > 5 || cc < 0 || cc > 6 || G[rr2][cc] !== team) break; line.push([rr2, cc]); }
      if (line.length === 4) return line;
    }
    return null;
  }
  botMove() {
    const tryCol = (c, team) => { if (this.full(c)) return false; let r = 5; while (this.grid[r][c] !== -1) r--; this.grid[r][c] = team; const w = !!this.winLine(team); this.grid[r][c] = -1; return w; };
    for (const team of [this.turn, 1 - this.turn]) for (let c = 0; c < 7; c++) if (tryCol(c, team)) return c;
    const pref = shuffle([3, 3, 3, 2, 4, 2, 4, 1, 5, 0, 6]); return pref.find(c => !this.full(c)) ?? 0;
  }
  update(dt) {
    this.fx.update(dt);
    if (this.over) { this.endT -= dt; if (this.endT <= 0) this.end(); return; }
    if (this.drop) {
      const d = this.drop, ty = this.cell(d.r, d.c).y; d.y += 1400 * dt;
      if (d.y >= ty) {
        this.grid[d.r][d.c] = d.team; this.placed.set(d.p, this.placed.get(d.p) + 1); this.drop = null; this.c.sfx.bonk();
        const w = this.winLine(d.team);
        if (w) { this.over = true; this.endT = 3.5; this.win = { team: d.team, line: w }; this.c.banner(`TEAM ${this.TN[d.team]} WINS!`, 2500, this.TC[d.team]); this.c.sfx.win(); this.fx.confetti(140); return; }
        if (this.grid[0].every(v => v !== -1)) { this.over = true; this.endT = 3; this.c.banner("IT'S A DRAW!", 2200); return; }
        this.nextTurn();
      }
      return;
    }
    this.t -= dt;
    if (this.botT != null) { this.botT -= dt; if (this.botT <= 0) { this.botT = null; this.play(this.botMove()); } }
    if (this.t <= 0) { const free = [0, 1, 2, 3, 4, 5, 6].filter(c => !this.full(c)); this.play(pick(free)); }
  }
  end() {
    if (this.done) return; this.done = true;
    const w = this.win ? this.win.team : -1;
    this.c.finish([...this.ps].sort((a, b) => ((this.teams[w] || []).includes(b) - (this.teams[w] || []).includes(a)) || this.placed.get(b) - this.placed.get(a))
      .map(p => ({ p, stat: `Team ${this.TN[this.teams[0].includes(p) ? 0 : 1]}${w < 0 ? ' · draw' : ''}` })));
  }
  draw(g) {
    bgGrad(g, '#2f3e9e', '#0b1033');
    g.fillStyle = 'rgba(0,0,0,.3)'; rr(g, 418, 178, 780, 620, 30); g.fill();
    g.fillStyle = '#1c7ed6'; rr(g, 410, 170, 780, 620, 30); g.fill();
    for (let c = 0; c < 7; c++) txt(g, String(c + 1), this.cell(0, c).x, 145, 26, '#b3acdf');
    if (this.drop) { g.fillStyle = this.TC[this.drop.team]; g.beginPath(); g.arc(this.cell(0, this.drop.c).x, this.drop.y, 40, 0, TAU); g.fill(); }
    for (let r = 0; r < 6; r++) for (let c = 0; c < 7; c++) {
      const { x, y } = this.cell(r, c), v = this.grid[r][c];
      g.fillStyle = v < 0 ? '#0b1033' : this.TC[v]; g.beginPath(); g.arc(x, y, 40, 0, TAU); g.fill();
      if (v >= 0) { g.fillStyle = 'rgba(255,255,255,.3)'; g.beginPath(); g.arc(x - 12, y - 12, 12, 0, TAU); g.fill(); }
      g.lineWidth = 4; g.strokeStyle = '#1864ab'; g.beginPath(); g.arc(x, y, 40, 0, TAU); g.stroke();
    }
    if (this.win) { g.strokeStyle = '#fff'; g.lineWidth = 12; g.lineCap = 'round'; const a = this.cell(...this.win.line[0]), b = this.cell(...this.win.line[3]); g.beginPath(); g.moveTo(a.x, a.y); g.lineTo(b.x, b.y); g.stroke(); }
    [0, 1].forEach(t => {
      const x = t ? 1390 : 210;
      g.fillStyle = this.TC[t]; g.beginPath(); g.arc(x, 250, 46, 0, TAU); g.fill();
      txt(g, `TEAM ${this.TN[t]}`, x, 330, 26, this.TC[t]);
      this.teams[t].forEach((p, i) => txt(g, (p === this.cur && !this.over ? '▶ ' : '') + p.name, x, 380 + i * 36, 22, '#fff', 'center', 'Nunito'));
    });
    if (!this.over && !this.drop) { txt(g, `${this.cur.name}'s move`, 800, 60, 34, this.TC[this.turn]); timeBar(g, this.t / 25, 840); }
    this.fx.draw(g);
  }
}

/* ---------------------------------------------------------------- 🎱 BINGO */
class Bingo {
  constructor(c) { this.c = c; this.fx = new Particles(); this.called = []; this.winners = []; this.callT = 2.5; this.bag = shuffle(Array.from({ length: 75 }, (_, i) => i + 1)); this.lock = new Map(); this.ballAnim = 0; }
  start() {
    this.ps = this.c.roster(2);
    this.cards = new Map(); this.marks = new Map();
    for (const p of this.ps) {
      const card = [];
      for (let col = 0; col < 5; col++) { const nums = shuffle(Array.from({ length: 15 }, (_, i) => col * 15 + i + 1)).slice(0, 5); for (let r = 0; r < 5; r++) card[r * 5 + col] = nums[r]; }
      card[12] = 0; this.cards.set(p, card); this.marks.set(p, new Set([12]));
    }
    this.pushAll();
  }
  letter(n) { return 'BINGO'[Math.floor((n - 1) / 15)]; }
  pushAll() { for (const p of this.ps) if (!p.bot) this.push(p); }
  push(p) {
    this.c.setLayout(p, { name: 'bingo', card: this.cards.get(p), marks: [...this.marks.get(p)], called: this.called.slice(-12), last: this.called[this.called.length - 1] || null,
      won: this.winners.includes(p), locked: (this.lock.get(p) || 0) > performance.now() });
  }
  resync(p) { this.push(p); }
  hasBingo(p) {
    const card = this.cards.get(p), m = this.marks.get(p), ok = i => i === 12 || (m.has(i) && this.called.includes(card[i]));
    const lines = [];
    for (let r = 0; r < 5; r++) lines.push([0, 1, 2, 3, 4].map(c => r * 5 + c));
    for (let c = 0; c < 5; c++) lines.push([0, 1, 2, 3, 4].map(r => r * 5 + c));
    lines.push([0, 6, 12, 18, 24], [4, 8, 12, 16, 20]);
    return lines.some(l => l.every(ok));
  }
  onAct(p, m) {
    if (this.over || !this.cards.has(p) || this.winners.includes(p)) return;
    if (m.a === 'mark') { const i = m.i | 0; if (i < 0 || i > 24 || i === 12) return; const s = this.marks.get(p); s.has(i) ? s.delete(i) : s.add(i); this.push(p); }
    else if (m.a === 'bingo') {
      if ((this.lock.get(p) || 0) > performance.now()) return;
      if (this.hasBingo(p)) {
        this.winners.push(p); this.c.banner(`🎉 ${p.name}: BINGO!`, 2400, p.color); this.c.sfx.win(); this.fx.confetti(120);
        if (this.winners.length >= Math.min(3, this.ps.length - 1)) { this.over = true; this.endT = 3.5; }
      } else { this.lock.set(p, performance.now() + 5000); this.c.banner(`❌ FALSE BINGO, ${p.name}!`, 1800, '#ff4d6d'); this.c.sfx.buzz(); this.c.send(p, { t: 'buzz', ms: 500 }); }
      this.push(p);
    }
  }
  update(dt) {
    this.fx.update(dt); this.ballAnim = Math.max(0, this.ballAnim - dt * 2);
    if (this.over) { this.endT -= dt; if (this.endT <= 0) this.end(); return; }
    this.callT -= dt;
    if (this.callT <= 0) {
      if (!this.bag.length) { this.over = true; this.endT = 2; return; }
      const n = this.bag.pop(); this.called.push(n); this.ballAnim = 1; this.c.sfx.ding();
      this.callT = Math.max(3.2, 5.5 - this.called.length * 0.05);
      for (const p of this.ps) if (p.bot) { const i = this.cards.get(p).indexOf(n); if (i >= 0) this.marks.get(p).add(i); }
      this.pushAll();
    }
    for (const p of this.ps) if (p.bot && !this.winners.includes(p) && this.hasBingo(p)) { p._bt = (p._bt ?? rnd(1, 3)) - dt; if (p._bt <= 0) this.onAct(p, { a: 'bingo' }); }
  }
  end() {
    if (this.done) return; this.done = true;
    const rest = this.ps.filter(p => !this.winners.includes(p)).sort((a, b) => this.good(b) - this.good(a));
    this.c.finish([...this.winners, ...rest].map(p => ({ p, stat: this.winners.includes(p) ? 'BINGO!' : `${this.good(p)} marked` })));
  }
  good(p) { const c = this.cards.get(p); return [...this.marks.get(p)].filter(i => i !== 12 && this.called.includes(c[i])).length; }
  draw(g) {
    bgGrad(g, '#7b2cbf', '#1a0533');
    const last = this.called[this.called.length - 1];
    // big ball
    const s = 1 + this.ballAnim * 0.25, colors = ['#4dabff', '#ff4d6d', '#fff', '#51cf66', '#ffd43b'];
    g.save(); g.translate(330, 420); g.scale(s, s);
    g.fillStyle = 'rgba(0,0,0,.3)'; g.beginPath(); g.arc(10, 16, 170, 0, TAU); g.fill();
    g.fillStyle = last ? colors['BINGO'.indexOf(this.letter(last))] : '#666'; g.beginPath(); g.arc(0, 0, 170, 0, TAU); g.fill();
    g.fillStyle = '#fff'; g.beginPath(); g.arc(0, 0, 105, 0, TAU); g.fill();
    g.restore();
    g.fillStyle = '#1a0533'; g.textAlign = 'center'; g.textBaseline = 'middle';
    if (last) { g.font = `${44 * s}px Bungee, Impact`; g.fillText(this.letter(last), 330, 365); g.font = `${90 * s}px Bungee, Impact`; g.fillText(String(last), 330, 440); }
    else { g.font = '90px Bungee, Impact'; g.fillText('?', 330, 420); }
    txt(g, `${this.called.length} called`, 330, 640, 24, '#e5dbff', 'center', 'Nunito');
    // board of all numbers
    for (let L = 0; L < 5; L++) {
      txt(g, 'BINGO'[L], 640, 200 + L * 120, 44, colors[L]);
      for (let k = 0; k < 15; k++) {
        const n = L * 15 + k + 1, x = 700 + k * 56, y = 200 + L * 120, on = this.called.includes(n);
        g.fillStyle = on ? colors[L] : 'rgba(255,255,255,.08)'; g.beginPath(); g.arc(x, y, 24, 0, TAU); g.fill();
        if (n === last) { g.lineWidth = 5; g.strokeStyle = '#fff'; g.stroke(); }
        txt(g, String(n), x, y + 1, 18, on ? '#1a0533' : 'rgba(255,255,255,.4)', 'center', 'Nunito');
      }
    }
    strip(g, this.ps.map(p => ({ name: p.name, color: p.color, val: this.winners.includes(p) ? '🏆' : this.good(p), hi: this.winners.includes(p) })));
    txt(g, 'Mark your card on your phone · press BINGO! when you get a line', 1100, 830, 22, '#e5dbff', 'center', 'Nunito');
    this.fx.draw(g);
  }
}

/* ---------------------------------------------------------------- 🎲 DICE DUEL (poker dice, hold & reroll) */
const HANDS = ['Nothing', 'Pair', 'Two Pair', 'Three of a Kind', 'Small Straight', 'Big Straight', 'Full House', 'Four of a Kind', 'FIVE OF A KIND!'];
function diceHand(d) {
  const cnt = {}; d.forEach(v => cnt[v] = (cnt[v] || 0) + 1);
  const c = Object.values(cnt).sort((a, b) => b - a), s = [...new Set(d)].sort().join('');
  let h = 0;
  if (c[0] === 5) h = 8; else if (c[0] === 4) h = 7; else if (c[0] === 3 && c[1] === 2) h = 6;
  else if (s === '12345' || s === '23456') h = 5; else if (/1234|2345|3456/.test(s)) h = 4;
  else if (c[0] === 3) h = 3; else if (c[0] === 2 && c[1] === 2) h = 2; else if (c[0] === 2) h = 1;
  return { h, score: h * 100 + d.reduce((a, b) => a + b, 0), name: HANDS[h] };
}
function drawDie(g, x, y, s, v, held, color) {
  g.fillStyle = 'rgba(0,0,0,.3)'; rr(g, x - s / 2 + 4, y - s / 2 + 6, s, s, s * 0.18); g.fill();
  g.fillStyle = '#fff'; rr(g, x - s / 2, y - s / 2, s, s, s * 0.18); g.fill();
  if (held) { g.lineWidth = 6; g.strokeStyle = color || '#ffd43b'; g.stroke(); }
  const P = { 1: [[0, 0]], 2: [[-1, -1], [1, 1]], 3: [[-1, -1], [0, 0], [1, 1]], 4: [[-1, -1], [1, -1], [-1, 1], [1, 1]], 5: [[-1, -1], [1, -1], [0, 0], [-1, 1], [1, 1]], 6: [[-1, -1], [1, -1], [-1, 0], [1, 0], [-1, 1], [1, 1]] };
  g.fillStyle = v === 1 ? '#e03131' : '#1a1438';
  for (const [a, b] of P[v] || []) { g.beginPath(); g.arc(x + a * s * 0.26, y + b * s * 0.26, s * 0.09, 0, TAU); g.fill(); }
}
class DiceDuel {
  constructor(c) { this.c = c; this.fx = new Particles(); this.round = 0; this.R = 5; this.pts = new Map(); }
  start() { this.ps = this.c.roster(2); this.ps.forEach(p => this.pts.set(p, 0)); this.next(); }
  next() {
    this.round++; if (this.round > this.R) return this.end();
    this.phase = 'roll'; this.t = 25; this.st = new Map();
    for (const p of this.ps) this.st.set(p, { d: Array.from({ length: 5 }, () => irnd(1, 6)), held: [0, 0, 0, 0, 0], left: 2, done: false, shake: 1, botT: rnd(1.5, 3) });
    this.c.sfx.whoosh(); this.pushAll();
  }
  pushAll() { for (const p of this.ps) if (!p.bot) this.push(p); }
  push(p) { const s = this.st.get(p); this.c.setLayout(p, { name: 'dice', dice: s.d, held: s.held, left: s.left, done: s.done, hand: diceHand(s.d).name, round: this.round, rounds: this.R }); }
  resync(p) { if (this.st && this.st.has(p)) this.push(p); }
  onAct(p, m) {
    const s = this.st && this.st.get(p); if (!s || this.phase !== 'roll' || s.done) return;
    if (m.a === 'hold') { const i = m.i | 0; if (i >= 0 && i < 5) s.held[i] = s.held[i] ? 0 : 1; }
    else if (m.a === 'roll' && s.left > 0) { s.d = s.d.map((v, i) => s.held[i] ? v : irnd(1, 6)); s.left--; s.shake = 1; this.c.sfx.card(); if (s.left === 0) s.done = true; }
    else if (m.a === 'done') s.done = true;
    if (!p.bot) this.push(p);
    if ([...this.st.values()].every(x => x.done)) this.t = Math.min(this.t, 0.5);
  }
  botPlay(p, s) {
    const cnt = {}; s.d.forEach(v => cnt[v] = (cnt[v] || 0) + 1);
    const best = +Object.keys(cnt).sort((a, b) => cnt[b] - cnt[a] || b - a)[0];
    if (diceHand(s.d).h >= 5) return this.onAct(p, { a: 'done' });
    s.held = s.d.map(v => v === best && cnt[best] > 1 ? 1 : 0);
    this.onAct(p, { a: 'roll' });
  }
  update(dt) {
    this.fx.update(dt); this.t -= dt;
    if (this.st) for (const s of this.st.values()) s.shake = Math.max(0, s.shake - dt * 3);
    if (this.phase === 'roll') {
      for (const p of this.ps) { const s = this.st.get(p); if (p.bot && !s.done) { s.botT -= dt; if (s.botT <= 0) { s.botT = rnd(1, 2.2); this.botPlay(p, s); } } }
      if (this.t <= 0) {
        this.phase = 'show'; this.t = 4.5;
        const r = rankBy(this.ps, p => diceHand(this.st.get(p).d).score, () => '');
        r.forEach((x, i) => this.pts.set(x.p, this.pts.get(x.p) + (this.ps.length - 1 - i)));
        this.roundWin = r[0].p;
        this.c.banner(`${r[0].p.name}: ${diceHand(this.st.get(r[0].p).d).name}`, 2200, r[0].p.color); this.c.sfx.win();
        for (const p of this.ps) if (!p.bot) this.c.setLayout(p, { name: 'msg', emoji: p === r[0].p ? '🏆' : '🎲', text: diceHand(this.st.get(p).d).name, sub: p === r[0].p ? 'You won the round!' : `${r[0].p.name} won this round` });
      }
    } else if (this.phase === 'show' && this.t <= 0) this.next();
  }
  end() { if (this.done) return; this.done = true; this.c.finish(rankBy(this.ps, p => this.pts.get(p), p => `${this.pts.get(p)} pts`)); }
  draw(g) {
    feltBg(g, '#a61e4d', '#2b0716');
    txt(g, `DICE DUEL · ROUND ${Math.min(this.round, this.R)}/${this.R}`, 800, 110, 32, '#ffd6e0');
    if (this.phase === 'roll') timeBar(g, this.t / 25, 140);
    const n = this.ps.length, rowH = Math.min(150, 700 / n);
    this.ps.forEach((p, i) => {
      const s = this.st.get(p), y = 200 + i * rowH + rowH / 2, hand = diceHand(s.d), win = this.phase === 'show' && p === this.roundWin;
      g.fillStyle = win ? 'rgba(255,212,59,.25)' : 'rgba(0,0,0,.25)'; rr(g, 120, y - rowH / 2 + 6, 1360, rowH - 12, 20); g.fill();
      drawBlob(g, 190, y, Math.min(34, rowH * 0.28), p.color, 0, { shadow: false, crown: win });
      txt(g, p.name, 250, y - 14, 24, '#fff', 'left', 'Nunito'); txt(g, s.done ? '✔ done' : `${s.left} rolls left`, 250, y + 18, 16, '#ffd6e0', 'left', 'Nunito');
      const ds = Math.min(84, rowH * 0.62);
      s.d.forEach((v, k) => drawDie(g, 560 + k * (ds + 18) + rnd(-1, 1) * s.shake * 8, y + rnd(-1, 1) * s.shake * 8, ds, v, s.held[k], p.color));
      txt(g, hand.name, 1250, y, 26, hand.h >= 6 ? '#ffd43b' : '#fff');
    });
    strip(g, rankBy(this.ps, p => this.pts.get(p), () => '').map(r => ({ name: r.p.name, color: r.p.color, val: this.pts.get(r.p) })));
    this.fx.draw(g);
  }
}

/* ---------------------------------------------------------------- 🧠 MEMORY MATCH (turn-based pairs) */
const MEM_EMOJI = ['⛄', '🎿', '🧣', '🧤', '❄️', '🦌', '🐧', '🎄', '🍪', '☕', '🛷', '🏂', '🦭', '🐻‍❄️', '🧊', '🎁'];
class MemoryMatch {
  constructor(c) { this.c = c; this.fx = new Particles(); this.pts = new Map(); this.sel = []; this.turnI = 0; this.mem = new Map(); }
  start() {
    this.ps = this.c.roster(2); this.ps.forEach(p => this.pts.set(p, 0));
    const em = shuffle(MEM_EMOJI.slice()).slice(0, 12);
    this.cards = shuffle([...em, ...em]).map(e => ({ e, up: false, gone: false, flip: 0, owner: null }));
    this.c.setLayout('all', { name: 'msg', emoji: '🧠', text: 'Memory Match', sub: 'Remember where the pictures are!' });
    this.wait = 1.5;
  }
  get cur() { return this.ps[this.turnI % this.ps.length]; }
  ask() {
    const p = this.cur; this.t = 15;
    for (const q of this.ps) if (!q.bot) this.c.setLayout(q, q === p ? turnChoice(q, this.sel.length ? 'Pick the 2nd card' : 'Your turn! Pick a card', this.cards.map((_, i) => i + 1), this.cards.map((c, i) => c.gone || this.sel.includes(i)), 6)
      : { name: 'msg', emoji: '👀', text: `${p.name}'s turn`, sub: 'Watch the TV and remember!' });
    this.botT = p.bot ? rnd(0.8, 1.6) : null;
  }
  onAct(p, m) { if (m.a === 'choice' && p === this.cur && this.wait <= 0 && this.sel.length < 2) this.pickCard(m.i | 0); }
  pickCard(i) {
    const c = this.cards[i]; if (!c || c.gone || this.sel.includes(i)) return;
    c.up = true; this.sel.push(i); this.c.sfx.card();
    for (const b of this.ps) if (b.bot && Math.random() < 0.7) this.mem.set(`${b.cid}:${i}`, c.e);
    if (this.sel.length === 2) {
      const [a, b] = this.sel.map(k => this.cards[k]);
      if (a.e === b.e) { a.gone = b.gone = true; a.owner = b.owner = this.cur; this.pts.set(this.cur, this.pts.get(this.cur) + 1); this.c.sfx.ding(); this.fx.text(800, 450, 'MATCH!', this.cur.color, 60); this.wait = 1.2; this.again = true; }
      else { this.wait = 1.8; this.again = false; this.c.sfx.buzz(); }
      for (const q of this.ps) if (!q.bot) this.c.setLayout(q, { name: 'msg', emoji: a.e === b.e ? '🎉' : '🙈', text: a.e === b.e ? 'Match!' : 'No match', sub: '' });
    } else this.ask();
  }
  botPick() {
    const p = this.cur, open = this.cards.map((c, i) => i).filter(i => !this.cards[i].gone && !this.sel.includes(i));
    const known = i => this.mem.get(`${p.cid}:${i}`);
    if (this.sel.length === 1) { const e = this.cards[this.sel[0]].e, j = open.find(i => known(i) === e); return j ?? pick(open); }
    for (const i of open) for (const j of open) if (i < j && known(i) && known(i) === known(j)) return i;
    return pick(open.filter(i => !known(i)).concat(open).slice(0, Math.max(1, open.length)));
  }
  update(dt) {
    this.fx.update(dt);
    for (const c of this.cards) c.flip += ((c.up || c.gone ? 1 : 0) - c.flip) * Math.min(1, dt * 10);
    if (this.over) { this.endT -= dt; if (this.endT <= 0) this.end(); return; }
    if (this.wait > 0) {
      this.wait -= dt;
      if (this.wait <= 0) {
        if (this.sel.length === 2) { for (const k of this.sel) if (!this.cards[k].gone) this.cards[k].up = false; this.sel = []; if (!this.again) this.turnI++; }
        if (this.cards.every(c => c.gone)) { this.over = true; this.endT = 2.5; const r = rankBy(this.ps, p => this.pts.get(p), () => ''); this.c.banner(`${r[0].p.name} WINS!`, 2200, r[0].p.color); this.c.sfx.win(); this.fx.confetti(120); return; }
        this.ask();
      }
      return;
    }
    this.t -= dt;
    if (this.botT != null) { this.botT -= dt; if (this.botT <= 0) { this.botT = null; this.pickCard(this.botPick()); } }
    if (this.t <= 0) { const open = this.cards.map((c, i) => i).filter(i => !this.cards[i].gone && !this.sel.includes(i)); this.pickCard(pick(open)); }
  }
  end() { if (this.done) return; this.done = true; this.c.finish(rankBy(this.ps, p => this.pts.get(p), p => `${this.pts.get(p)} pairs`)); }
  draw(g) {
    bgGrad(g, '#1864ab', '#06203d');
    const W = 150, H = 170, gx = 16, gy = 16, x0 = 800 - (6 * W + 5 * gx) / 2, y0 = 130;
    this.cards.forEach((c, i) => {
      const x = x0 + (i % 6) * (W + gx), y = y0 + ((i / 6) | 0) * (H + gy), f = Math.abs(Math.cos(c.flip * Math.PI)), face = c.flip > 0.5;
      g.save(); g.translate(x + W / 2, y + H / 2); g.scale(Math.max(0.05, f), 1);
      if (c.gone && c.flip > 0.99) g.globalAlpha = 0.45;
      g.fillStyle = 'rgba(0,0,0,.3)'; rr(g, -W / 2 + 4, -H / 2 + 6, W, H, 18); g.fill();
      if (face) { g.fillStyle = '#fff'; rr(g, -W / 2, -H / 2, W, H, 18); g.fill(); if (c.owner) { g.lineWidth = 6; g.strokeStyle = c.owner.color; g.stroke(); } g.font = '80px serif'; g.textAlign = 'center'; g.textBaseline = 'middle'; g.fillStyle = '#000'; g.fillText(c.e, 0, 6); }
      else { const gr = g.createLinearGradient(0, -H / 2, 0, H / 2); gr.addColorStop(0, '#74c0fc'); gr.addColorStop(1, '#1c7ed6'); g.fillStyle = gr; rr(g, -W / 2, -H / 2, W, H, 18); g.fill(); g.lineWidth = 4; g.strokeStyle = '#fff'; g.stroke(); txt(g, String(i + 1), 0, 4, 42, '#fff'); }
      g.restore(); g.globalAlpha = 1;
    });
    strip(g, this.ps.map(p => ({ name: p.name, color: p.color, val: this.pts.get(p), hi: p === this.cur && !this.over })));
    if (!this.over && this.wait <= 0) txt(g, `${this.cur.name}: pick ${this.sel.length ? 'another' : 'a'} card`, 800, 880, 24, this.cur.color, 'center', 'Nunito');
    this.fx.draw(g);
  }
}

/* ---------------------------------------------------------------- ⛄ WORD RESCUE (save the snowman — guess letters) */
const RESCUE_WORDS = ['SNOWFLAKE', 'MITTENS', 'HOT COCOA', 'ICICLE', 'SNOWBOARD', 'BLIZZARD', 'PENGUIN', 'FIREPLACE', 'SLEDDING', 'IGLOO', 'REINDEER', 'SCARF', 'POLAR BEAR', 'GINGERBREAD',
  'SKI LIFT', 'AVALANCHE', 'MARSHMALLOW', 'SNOW ANGEL', 'FROSTY', 'WINTER BREAK', 'CAMPFIRE', 'ICE SKATES', 'HOLIDAY', 'CANDY CANE', 'NORTHERN LIGHTS', 'WALRUS', 'EARMUFFS', 'CHIMNEY', 'PINE TREE', 'CAROLS'];
class WordRescue {
  constructor(c) { this.c = c; this.fx = new Particles(); this.round = 0; this.R = 3; this.pts = new Map(); this.turnI = 0; this.words = shuffle(RESCUE_WORDS.slice()); }
  start() { this.ps = this.c.roster(2); this.ps.forEach(p => this.pts.set(p, 0)); this.next(); }
  next() {
    this.round++; if (this.round > this.R) return this.end();
    this.word = this.words.pop(); this.used = new Set(); this.wrong = 0; this.wait = 1.5; this.solved = false;
    this.c.banner(`WORD ${this.round}/${this.R}`, 1300, '#a5d8ff');
  }
  get cur() { return this.ps[this.turnI % this.ps.length]; }
  hidden() { return [...this.word].filter(ch => ch !== ' ' && !this.used.has(ch)); }
  ask() {
    const p = this.cur, L = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'.split(''); this.t = 20;
    for (const q of this.ps) if (!q.bot) this.c.setLayout(q, q === p ? turnChoice(q, 'Your turn — pick a letter!', L, L.map(l => this.used.has(l)), 6)
      : { name: 'msg', emoji: '⛄', text: `${p.name} is picking a letter`, sub: 'Shout your guess for the word!' });
    this.botT = p.bot ? rnd(1, 2) : null;
  }
  onAct(p, m) { if (m.a === 'choice' && p === this.cur && this.wait <= 0) this.guess('ABCDEFGHIJKLMNOPQRSTUVWXYZ'[m.i | 0]); }
  guess(l) {
    if (!l || this.used.has(l)) return;
    this.used.add(l); const p = this.cur, n = [...this.word].filter(ch => ch === l).length;
    if (n) {
      this.pts.set(p, this.pts.get(p) + n * 10); this.c.sfx.ding(); this.fx.text(800, 520, `+${n * 10}`, p.color, 50);
      if (!this.hidden().length) { this.pts.set(p, this.pts.get(p) + 30); this.solved = true; this.c.banner(`${p.name} SAVED THE SNOWMAN!`, 2200, p.color); this.c.sfx.win(); this.fx.confetti(90); this.wait = 3; this.turnI++; this.roundEnd = true; }
      else this.wait = 0.6;
    } else {
      this.wrong++; this.c.sfx.buzz(); this.turnI++; this.wait = 0.9;
      if (this.wrong >= 6) { this.c.banner('THE SNOWMAN MELTED! 💧', 2200, '#74c0fc'); this.wait = 3; this.roundEnd = true; }
    }
    for (const q of this.ps) if (!q.bot && q === p) this.c.setLayout(q, { name: 'msg', emoji: n ? '✅' : '❌', text: n ? `${n}× ${l}!` : `No ${l}`, sub: '' });
  }
  update(dt) {
    this.fx.update(dt);
    if (this.over) return;
    if (this.wait > 0) { this.wait -= dt; if (this.wait <= 0) { if (this.roundEnd) { this.roundEnd = false; this.next(); if (this.round > this.R) return; } else this.ask(); } return; }
    this.t -= dt;
    if (this.botT != null) { this.botT -= dt; if (this.botT <= 0) { this.botT = null; const freq = 'EAIRNOTSLCUDPMHGBFYWKVXZJQ'; this.guess([...freq].find(l => !this.used.has(l) && Math.random() < 0.75) || [...freq].find(l => !this.used.has(l))); } }
    if (this.t <= 0) { const free = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'.split('').filter(l => !this.used.has(l)); this.guess(pick(free)); }
  }
  end() { if (this.done) return; this.done = true; this.over = true; this.c.finish(rankBy(this.ps, p => this.pts.get(p), p => `${this.pts.get(p)} pts`)); }
  draw(g) {
    const gr = g.createLinearGradient(0, 0, 0, 900); gr.addColorStop(0, '#1b2a4e'); gr.addColorStop(1, '#4a6fa5'); g.fillStyle = gr; g.fillRect(0, 0, 1600, 900);
    g.fillStyle = '#f1f8ff'; g.beginPath(); g.ellipse(800, 930, 1000, 200, 0, 0, TAU); g.fill();
    // snowman melting by wrong count
    const m = this.wrong / 6, sx = 300, base = 760;
    g.fillStyle = '#a5d8ff'; g.beginPath(); g.ellipse(sx, base + 10, 120 + m * 80, 22 + m * 10, 0, 0, TAU); g.fill();
    const parts = [[90, 0], [65, 130], [45, 225]];
    parts.forEach(([r, h], k) => { if (m > 0.9 && k > 0) return; const rr2 = r * (1 - m * 0.45); g.fillStyle = '#fff'; g.beginPath(); g.arc(sx, base - h * (1 - m * 0.5) - rr2 * 0.8, rr2, 0, TAU); g.fill(); g.strokeStyle = '#d0ebff'; g.lineWidth = 3; g.stroke(); });
    if (m < 0.9) { const hy = base - 225 * (1 - m * 0.5) - 45 * (1 - m * 0.45) * 0.8; g.fillStyle = '#111'; g.beginPath(); g.arc(sx - 14, hy - 8, 5, 0, TAU); g.arc(sx + 14, hy - 8, 5, 0, TAU); g.fill(); g.fillStyle = '#ff922b'; g.beginPath(); g.moveTo(sx, hy); g.lineTo(sx + 36, hy + 6); g.lineTo(sx, hy + 10); g.fill(); g.fillStyle = '#e03131'; g.fillRect(sx - 50, hy + 26, 100, 14); }
    txt(g, `${6 - this.wrong} tries left`, sx, 830, 26, '#fff');
    // word
    const chars = [...this.word], cw = Math.min(80, 1000 / chars.length);
    chars.forEach((ch, i) => {
      const x = 1050 - (chars.length * cw) / 2 + i * cw + cw / 2, y = 380;
      if (ch === ' ') return;
      g.fillStyle = 'rgba(255,255,255,.9)'; g.fillRect(x - cw * 0.38, y + 40, cw * 0.76, 6);
      const show = this.used.has(ch) || this.roundEnd || this.solved;
      if (show) txt(g, ch, x, y, cw * 0.85, this.used.has(ch) ? '#fff' : '#ff8787');
    });
    const wrongs = [...this.used].filter(l => !this.word.includes(l));
    txt(g, wrongs.length ? `Not in it: ${wrongs.join(' ')}` : 'Pick letters to save the snowman!', 1050, 560, 28, '#ffc9c9', 'center', 'Nunito');
    if (!this.over && this.wait <= 0) { txt(g, `${this.cur.name}'s turn`, 1050, 650, 36, this.cur.color); timeBar(g, this.t / 20, 700); }
    strip(g, this.ps.map(p => ({ name: p.name, color: p.color, val: this.pts.get(p), hi: p === this.cur })));
    txt(g, `WORD ${Math.min(this.round, this.R)}/${this.R}`, 1530, 46, 22, '#a5d8ff');
    this.fx.draw(g);
  }
}
