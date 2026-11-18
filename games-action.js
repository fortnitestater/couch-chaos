/* Couch Chaos — ⚡ Action & arcade games. Needs engine.js */

/* ---------------------------------------------------------------- 🏍️ LIGHT TRAILS */
class LightTrails {
  constructor(c) { this.c = c; this.fx = new Particles(); this.round = 0; this.R = 3; this.pts = new Map(); }
  start() {
    this.ps = this.c.roster(2); this.ps.forEach(p => this.pts.set(p, 0));
    this.c.setLayout('all', { name: 'pad', stick: 'x', a: null, b: 'BOOST', hint: 'Steer left/right. Never touch a trail!' });
    this.next();
  }
  next() {
    this.round++; if (this.round > this.R) return this.end();
    this.CELL = 8; this.gw = 200; this.gh = 102; this.top = 84;
    this.grid = new Int16Array(this.gw * this.gh).fill(-1); this.stamp = new Float32Array(this.gw * this.gh);
    const n = this.ps.length;
    this.riders = this.ps.map((p, i) => { const a = (i / n) * TAU, x = 800 - Math.cos(a) * 560, y = this.top + 408 - Math.sin(a) * 300; return { p, i, x, y, ang: Math.atan2(this.top + 408 - y, 800 - x), alive: true, energy: 1, trail: [[x, y]], cell: -1 }; });
    this.out = []; this.cd = new Countdown(this.c); this.el = 0; this.over = false;
  }
  cellOf(x, y) { const cx = Math.floor(x / this.CELL), cy = Math.floor((y - this.top) / this.CELL); if (cx < 0 || cy < 0 || cx >= this.gw || cy >= this.gh) return -2; return cy * this.gw + cx; }
  free(x, y, i) { const c = this.cellOf(x, y); if (c === -2) return false; const o = this.grid[c]; return o === -1 || (o === i && this.el - this.stamp[c] < 0.25); }
  bot(r) {
    let best = 0, bs = -1;
    for (const da of [-0.6, 0, 0.6]) { let s = 0; for (let d = 12; d < 220; d += 8) { if (!this.free(r.x + Math.cos(r.ang + da) * d, r.y + Math.sin(r.ang + da) * d, r.i)) break; s = d; } s += da === 0 ? 10 : rnd(0, 8); if (s > bs) { bs = s; best = da; } }
    return { x: clamp(best * 2.5), b: bs > 200 && Math.random() < 0.02 };
  }
  update(dt) {
    this.fx.update(dt);
    if (!this.cd.update(dt)) return;
    if (this.over) { this.overT -= dt; if (this.overT <= 0) this.next(); return; }
    this.el += dt;
    for (const r of this.riders) {
      if (!r.alive) continue;
      const inp = r.p.bot ? this.bot(r) : r.p.input;
      r.ang += clamp(inp.x || 0) * 3.4 * dt;
      const boost = inp.b && r.energy > 0.02; r.energy = boost ? Math.max(0, r.energy - dt * 0.6) : Math.min(1, r.energy + dt * 0.15);
      let dist = (boost ? 400 : 250) * dt;
      while (dist > 0 && r.alive) {
        const st = Math.min(4, dist); dist -= st;
        r.x += Math.cos(r.ang) * st; r.y += Math.sin(r.ang) * st;
        const c = this.cellOf(r.x, r.y);
        if (c !== r.cell) {
          if (c === -2 || (this.grid[c] !== -1 && !(this.grid[c] === r.i && this.el - this.stamp[c] < 0.25))) { this.crash(r); break; }
          this.grid[c] = r.i; this.stamp[c] = this.el; r.cell = c;
        }
      }
      const last = r.trail[r.trail.length - 1]; if (Math.hypot(last[0] - r.x, last[1] - r.y) > 6) r.trail.push([r.x, r.y]);
    }
    const alive = this.riders.filter(r => r.alive);
    if (alive.length <= 1 && !this.over) {
      this.over = true; this.overT = 2.5;
      this.out.forEach((p, i) => this.pts.set(p, this.pts.get(p) + i));
      if (alive[0]) { this.pts.set(alive[0].p, this.pts.get(alive[0].p) + this.ps.length); this.c.banner(`${alive[0].p.name} WINS ROUND!`, 2000, alive[0].p.color); this.c.sfx.win(); }
      else this.c.banner('DOUBLE CRASH!', 1800);
    }
  }
  crash(r) { r.alive = false; this.out.push(r.p); this.fx.burst(r.x, r.y, 40, r.p.color, 380, 1, 7); this.c.sfx.boom(); this.c.shake(10); this.c.send(r.p, { t: 'buzz', ms: 400 }); }
  end() { if (this.done) return; this.done = true; this.c.finish(rankBy(this.ps, p => this.pts.get(p), p => `${this.pts.get(p)} pts`)); }
  draw(g) {
    g.fillStyle = '#05030f'; g.fillRect(0, 0, 1600, 900);
    g.strokeStyle = 'rgba(77,171,255,.08)'; g.lineWidth = 1; for (let x = 0; x <= 1600; x += 40) { g.beginPath(); g.moveTo(x, this.top); g.lineTo(x, this.top + this.gh * 8); g.stroke(); } for (let y = this.top; y <= this.top + this.gh * 8; y += 40) { g.beginPath(); g.moveTo(0, y); g.lineTo(1600, y); g.stroke(); }
    g.strokeStyle = '#4dabff'; g.lineWidth = 4; g.strokeRect(2, this.top, 1596, this.gh * 8);
    g.lineCap = 'round'; g.lineJoin = 'round';
    for (const r of this.riders) {
      for (const [w, a] of [[12, 0.25], [4, 1]]) { g.globalAlpha = r.alive ? a : a * 0.4; g.strokeStyle = r.p.color; g.lineWidth = w; g.beginPath(); r.trail.forEach(([x, y], i) => i ? g.lineTo(x, y) : g.moveTo(x, y)); g.lineTo(r.x, r.y); g.stroke(); }
      g.globalAlpha = 1;
      if (r.alive) { g.fillStyle = '#fff'; g.beginPath(); g.arc(r.x, r.y, 7, 0, TAU); g.fill(); txt(g, r.p.name, r.x, r.y - 22, 16, r.p.color, 'center', 'Nunito'); }
    }
    this.fx.draw(g);
    strip(g, rankBy(this.ps, p => this.pts.get(p), () => '').map(x => ({ name: x.p.name, color: x.p.color, val: this.pts.get(x.p), dim: this.riders && !this.riders.find(r => r.p === x.p).alive })), 40);
    txt(g, `ROUND ${Math.min(this.round, this.R)}/${this.R}`, 1530, 40, 22, '#4dabff');
    this.cd.draw(g);
  }
}

/* ---------------------------------------------------------------- 🎨 PAINT WARS */
class PaintWars extends ArenaGame {
  constructor(c) { super(c, { time: 60, b: 'DASH', hint: 'Roll over the floor to paint it your color!', unit: '%' }); }
  setup() {
    this.cols = 40; this.rows = 21; this.cw = this.box.w / this.cols; this.ch = this.box.h / this.rows;
    this.grid = new Int8Array(this.cols * this.rows).fill(-1); this.blobs.forEach((b, i) => b.idx = i); this.bombs = []; this.bombT = 4; this.cntT = 0;
  }
  paint(x, y, rad, idx) {
    const bx = this.box, c0 = Math.max(0, Math.floor((x - rad - bx.x) / this.cw)), c1 = Math.min(this.cols - 1, Math.floor((x + rad - bx.x) / this.cw));
    const r0 = Math.max(0, Math.floor((y - rad - bx.y) / this.ch)), r1 = Math.min(this.rows - 1, Math.floor((y + rad - bx.y) / this.ch));
    for (let r = r0; r <= r1; r++) for (let c = c0; c <= c1; c++) { const cx = bx.x + (c + 0.5) * this.cw, cy = bx.y + (r + 0.5) * this.ch; if ((cx - x) ** 2 + (cy - y) ** 2 < rad * rad) this.grid[r * this.cols + c] = idx; }
  }
  tick(dt) {
    for (const b of this.blobs) if (b.alive) this.paint(b.x, b.y, b.r * 1.15, b.idx);
    this.bombT -= dt;
    if (this.bombT <= 0 && this.bombs.length < 3) { this.bombT = rnd(3, 6); this.bombs.push({ x: rnd(this.box.x + 80, this.box.x + this.box.w - 80), y: rnd(this.box.y + 80, this.box.y + this.box.h - 80), t: 0 }); }
    for (const pb of this.bombs) { pb.t += dt; for (const b of this.blobs) if (!pb.dead && Math.hypot(pb.x - b.x, pb.y - b.y) < b.r + 26) { pb.dead = true; this.paint(pb.x, pb.y, 170, b.idx); this.fx.burst(pb.x, pb.y, 50, b.p.color, 420, 0.9, 10); this.c.sfx.boom(); this.c.shake(8); this.fx.text(pb.x, pb.y - 40, 'SPLAT!', b.p.color); } }
    this.bombs = this.bombs.filter(b => !b.dead);
    this.cntT -= dt;
    if (this.cntT <= 0) { this.cntT = 0.25; const cnt = new Array(this.blobs.length).fill(0); for (const v of this.grid) if (v >= 0) cnt[v]++; this.blobs.forEach((b, i) => this.score.set(b.p, Math.round(cnt[i] / this.grid.length * 100))); }
  }
  botTarget(b) {
    if (this.bombs.length) { const pb = this.bombs[0]; if (Math.hypot(pb.x - b.x, pb.y - b.y) < 400) return pb; }
    if (!b.goal || this.el > b.goalT || Math.hypot(b.goal.x - b.x, b.goal.y - b.y) < 40) {
      for (let t = 0; t < 12; t++) { const i = irnd(0, this.grid.length - 1); if (this.grid[i] !== b.idx) { b.goal = { x: this.box.x + (i % this.cols + 0.5) * this.cw, y: this.box.y + (((i / this.cols) | 0) + 0.5) * this.ch }; break; } }
      b.goalT = this.el + 1.5;
    }
    return b.goal;
  }
  drawUnder(g) {
    const bx = this.box;
    for (let i = 0; i < this.grid.length; i++) { const v = this.grid[i]; if (v < 0) continue; g.fillStyle = this.blobs[v].p.color; g.fillRect(bx.x + (i % this.cols) * this.cw - 0.5, bx.y + ((i / this.cols) | 0) * this.ch - 0.5, this.cw + 1, this.ch + 1); }
    for (const pb of this.bombs) { const s = 1 + Math.sin(pb.t * 6) * 0.1; g.fillStyle = '#212529'; g.beginPath(); g.arc(pb.x, pb.y, 24 * s, 0, TAU); g.fill(); txt(g, '🎨', pb.x, pb.y, 26); }
  }
}

/* ---------------------------------------------------------------- 🪙 COIN RUSH */
class CoinRush extends ArenaGame {
  constructor(c) { super(c, { time: 60, b: 'DASH', hint: 'Grab coins! Dash into people to steal. Avoid the bombs!', unit: 'coins' }); this.coins = []; this.bombs = []; this.bombT = 4; }
  setup() { for (let i = 0; i < 10; i++) this.addCoin(); }
  addCoin(x, y, v = 1, vx = 0, vy = 0) { const bx = this.box; this.coins.push({ x: x ?? rnd(bx.x + 40, bx.x + bx.w - 40), y: y ?? rnd(bx.y + 40, bx.y + bx.h - 40), v, vx, vy, t: rnd(0, 6) }); }
  tick(dt) {
    const bx = this.box;
    if (this.coins.length < 14 && Math.random() < dt * 3) this.addCoin(undefined, undefined, Math.random() < 0.08 ? 5 : 1);
    for (const c of this.coins) { c.t += dt; c.x = clamp(c.x + c.vx * dt, bx.x + 20, bx.x + bx.w - 20); c.y = clamp(c.y + c.vy * dt, bx.y + 20, bx.y + bx.h - 20); c.vx *= 0.92; c.vy *= 0.92;
      for (const b of this.blobs) if (!c.dead && b.alive && b.stun <= 0 && Math.hypot(c.x - b.x, c.y - b.y) < b.r + 16) { c.dead = true; this.score.set(b.p, this.score.get(b.p) + c.v); this.fx.burst(c.x, c.y, 10, '#ffd43b', 160, 0.4, 5); this.c.sfx.pickup(); if (c.v > 1) this.fx.text(c.x, c.y - 30, '+5', '#ffd43b'); } }
    this.coins = this.coins.filter(c => !c.dead);
    this.bombT -= dt;
    if (this.bombT <= 0) { this.bombT = rnd(1.8, 3.5) * (this.T > 30 ? 1 : 0.6); this.bombs.push({ x: rnd(bx.x + 100, bx.x + bx.w - 100), y: rnd(bx.y + 100, bx.y + bx.h - 100), t: 1.6 }); }
    for (const bm of this.bombs) {
      bm.t -= dt;
      if (bm.t <= 0 && !bm.done) {
        bm.done = true; this.fx.burst(bm.x, bm.y, 50, '#ff922b', 450, 0.9, 10); this.c.sfx.boom(); this.c.shake(10);
        for (const b of this.blobs) {
          const d = Math.hypot(b.x - bm.x, b.y - bm.y); if (d > 140) continue;
          const lose = Math.min(this.score.get(b.p), 3); this.score.set(b.p, this.score.get(b.p) - lose); b.stun = 1; b.vx += (b.x - bm.x) / (d || 1) * 700; b.vy += (b.y - bm.y) / (d || 1) * 700;
          for (let k = 0; k < lose; k++) { const a = rnd(0, TAU); this.addCoin(b.x, b.y, 1, Math.cos(a) * 500, Math.sin(a) * 500); }
          this.c.send(b.p, { t: 'buzz', ms: 300 }); if (lose) this.fx.text(b.x, b.y - 50, `-${lose}`, '#ff4d6d');
        }
      }
    }
    this.bombs = this.bombs.filter(b => !b.done);
  }
  onBump(a, b) {
    for (const [att, vic] of [[a, b], [b, a]]) if (att.dashT > 0 && vic.inv <= 0 && this.score.get(vic.p) > 0) {
      this.score.set(vic.p, this.score.get(vic.p) - 1); this.score.set(att.p, this.score.get(att.p) + 1); vic.inv = 0.8;
      this.fx.text(att.x, att.y - 50, 'STEAL!', att.p.color); this.c.sfx.ding();
    }
  }
  botTarget(b) {
    for (const bm of this.bombs) if (Math.hypot(bm.x - b.x, bm.y - b.y) < 170) return { x: bm.x, y: bm.y, flee: true };
    let t = null, bd = 1e9; for (const c of this.coins) { const d = Math.hypot(c.x - b.x, c.y - b.y) / c.v; if (d < bd) { bd = d; t = c; } }
    return t;
  }
  drawArena(g) { bgGrad(g, '#5c4a0a', '#1a1403'); const bx = this.box; g.fillStyle = '#fff4d6'; rr(g, bx.x, bx.y, bx.w, bx.h, 30); g.fill(); g.lineWidth = 8; g.strokeStyle = '#f08c00'; g.stroke(); }
  drawUnder(g) {
    for (const bm of this.bombs) { const k = 1 - bm.t / 1.6; g.fillStyle = `rgba(255,0,0,${0.12 + 0.15 * Math.abs(Math.sin(bm.t * 12))})`; g.beginPath(); g.arc(bm.x, bm.y, 140, 0, TAU); g.fill(); g.strokeStyle = '#e03131'; g.lineWidth = 4; g.beginPath(); g.arc(bm.x, bm.y, 140 * k, 0, TAU); g.stroke(); txt(g, '💣', bm.x, bm.y, 40); }
    for (const c of this.coins) { const w = Math.abs(Math.cos(c.t * 4)); g.fillStyle = c.v > 1 ? '#ff922b' : '#fab005'; g.beginPath(); g.ellipse(c.x, c.y, (c.v > 1 ? 22 : 15) * Math.max(0.2, w), c.v > 1 ? 22 : 15, 0, 0, TAU); g.fill(); g.strokeStyle = '#e67700'; g.lineWidth = 3; g.stroke(); }
  }
}

/* ---------------------------------------------------------------- 🎯 TARGET BLAST */
class TargetBlast {
  constructor(c) { this.c = c; this.fx = new Particles(); this.cd = new Countdown(c); this.T = 60; this.targets = []; this.spawnT = 0; this.score = new Map(); }
  start() {
    this.ps = this.c.roster(2); this.ps.forEach(p => this.score.set(p, 0));
    this.cross = this.ps.map((p, i) => ({ p, x: 300 + i * 150, y: 500, vx: 0, vy: 0, cd: 0, kick: 0 }));
    this.c.setLayout('all', { name: 'pad', stick: 'xy', a: 'SHOOT', b: null, hint: 'Aim with tilt/drag/WASD. SHOOT the balloons, not the bombs!' });
  }
  onButton(p, b) { if (b === 'a' && this.cd.done) { const c = this.cross.find(x => x.p === p); if (c) this.shoot(c); } }
  shoot(c) {
    if (c.cd > 0) return; c.cd = 0.22; c.kick = 1; this.c.sfx.shoot();
    for (const t of this.targets) if (!t.dead && Math.hypot(t.x - c.x, t.y - c.y) < t.r + 6) {
      t.dead = true; const v = t.kind === 'gold' ? 5 : t.kind === 'bomb' ? -5 : 1;
      this.score.set(c.p, this.score.get(c.p) + v);
      this.fx.burst(t.x, t.y, t.kind === 'bomb' ? 40 : 22, t.kind === 'bomb' ? '#ff922b' : t.color, 300, 0.7, 7);
      this.fx.text(t.x, t.y - 30, v > 0 ? `+${v}` : `${v}`, v > 0 ? c.p.color : '#ff4d6d');
      t.kind === 'bomb' ? (this.c.sfx.boom(), this.c.shake(8), this.c.send(c.p, { t: 'buzz', ms: 300 })) : this.c.sfx.pickup();
      break;
    }
  }
  update(dt) {
    this.fx.update(dt);
    for (const c of this.cross) {
      const inp = c.p.bot ? this.bot(c) : c.p.input;
      c.vx += ((inp.x || 0) * 1100 - c.vx) * Math.min(1, dt * 9); c.vy += ((inp.y || 0) * 1100 - c.vy) * Math.min(1, dt * 9);
      c.x = clamp(c.x + c.vx * dt, 20, 1580); c.y = clamp(c.y + c.vy * dt, 90, 880); c.cd -= dt; c.kick = Math.max(0, c.kick - dt * 6);
      if (c.p.bot && inp.a && this.cd.done) this.shoot(c);
      if (!c.p.bot && c.p.input.a && this.cd.done) this.shoot(c);
    }
    if (!this.cd.update(dt)) return;
    if (this.over) { this.overT -= dt; if (this.overT <= 0) this.end(); return; }
    this.T -= dt; if (this.T <= 0) { this.over = true; this.overT = 2; this.c.sfx.win(); this.fx.confetti(90); return; }
    this.spawnT -= dt;
    if (this.spawnT <= 0) {
      this.spawnT = rnd(0.25, 0.6);
      const r = Math.random(), kind = r < 0.08 ? 'gold' : r < 0.22 ? 'bomb' : 'normal', side = Math.random() < 0.5;
      this.targets.push({ kind, x: rnd(120, 1480), y: 960, vx: rnd(-60, 60), vy: -rnd(110, 200) * (kind === 'gold' ? 1.6 : 1), r: kind === 'gold' ? 30 : 42, color: pick(['#ff4d6d', '#4dabff', '#51cf66', '#cc5de8', '#ff922b']), w: rnd(0, 6), side });
    }
    for (const t of this.targets) { t.w += dt * 2; t.x += (t.vx + Math.sin(t.w) * 40) * dt; t.y += t.vy * dt; }
    this.targets = this.targets.filter(t => !t.dead && t.y > -80);
  }
  bot(c) {
    let t = null, bd = 1e9; for (const x of this.targets) if (x.kind !== 'bomb' && x.y < 880) { const d = Math.hypot(x.x - c.x, x.y - c.y) / (x.kind === 'gold' ? 2 : 1); if (d < bd) { bd = d; t = x; } }
    if (!t) return { x: 0, y: 0 };
    const dx = t.x - c.x, dy = t.y - c.y, d = Math.hypot(dx, dy) || 1;
    return { x: clamp(dx / 120) * 0.8, y: clamp(dy / 120) * 0.8, a: d < t.r * 0.8 && Math.random() < 0.3 };
  }
  end() { if (this.done) return; this.done = true; this.c.finish(rankBy(this.ps, p => this.score.get(p), p => `${this.score.get(p)} pts`)); }
  draw(g) {
    const gr = g.createLinearGradient(0, 0, 0, 900); gr.addColorStop(0, '#3b1d6e'); gr.addColorStop(1, '#ab4fd1'); g.fillStyle = gr; g.fillRect(0, 0, 1600, 900);
    for (let i = 0; i < 16; i++) { g.fillStyle = i % 2 ? 'rgba(255,255,255,.05)' : 'rgba(0,0,0,.05)'; g.fillRect(i * 100, 0, 100, 900); }
    for (const t of this.targets) {
      g.strokeStyle = 'rgba(255,255,255,.5)'; g.lineWidth = 2; g.beginPath(); g.moveTo(t.x, t.y + t.r); g.quadraticCurveTo(t.x + 10, t.y + t.r + 40, t.x - 4, t.y + t.r + 80); g.stroke();
      g.fillStyle = t.kind === 'bomb' ? '#212529' : t.kind === 'gold' ? '#fcc419' : t.color; g.beginPath(); g.ellipse(t.x, t.y, t.r * 0.86, t.r, 0, 0, TAU); g.fill();
      g.fillStyle = 'rgba(255,255,255,.35)'; g.beginPath(); g.ellipse(t.x - t.r * 0.3, t.y - t.r * 0.35, t.r * 0.2, t.r * 0.32, -0.5, 0, TAU); g.fill();
      if (t.kind === 'bomb') txt(g, '💣', t.x, t.y, 34); if (t.kind === 'gold') txt(g, '★', t.x, t.y + 2, 30, '#fff');
    }
    this.fx.draw(g);
    for (const c of this.cross) {
      const r = 26 + c.kick * 10; g.strokeStyle = c.p.color; g.lineWidth = 5;
      g.beginPath(); g.arc(c.x, c.y, r, 0, TAU); g.stroke();
      g.beginPath(); g.moveTo(c.x - r - 10, c.y); g.lineTo(c.x - 8, c.y); g.moveTo(c.x + 8, c.y); g.lineTo(c.x + r + 10, c.y); g.moveTo(c.x, c.y - r - 10); g.lineTo(c.x, c.y - 8); g.moveTo(c.x, c.y + 8); g.lineTo(c.x, c.y + r + 10); g.stroke();
      txt(g, c.p.name, c.x, c.y + r + 22, 16, c.p.color, 'center', 'Nunito');
    }
    strip(g, rankBy(this.ps, p => this.score.get(p), () => '').map(x => ({ name: x.p.name, color: x.p.color, val: this.score.get(x.p) })));
    clock(g, this.T); this.cd.draw(g);
  }
}

/* ---------------------------------------------------------------- ⏱️ STOP THE METER */
class StopMeter {
  constructor(c) { this.c = c; this.fx = new Particles(); this.round = 0; this.R = 6; this.pts = new Map(); }
  start() { this.ps = this.c.roster(2); this.ps.forEach(p => this.pts.set(p, 0)); this.c.setLayout('all', { name: 'button', label: 'STOP!', hint: 'Stop the needle inside the gold zone!' }); this.next(); }
  next() {
    this.round++; if (this.round > this.R) return this.end();
    this.target = rnd(15, 85); this.speed = rnd(0.7, 1.1) + this.round * 0.12; this.ph = rnd(0, TAU);
    this.marks = new Map(); this.phase = 'run'; this.t = 7; this.wait = 1.2; this.bots = new Map(this.ps.filter(p => p.bot).map(p => [p, rnd(1.5, 6)]));
  }
  val() { return 50 + Math.sin(this.ph) * 50; }
  onButton(p, b) {
    if (b !== 'a' || this.phase !== 'run' || this.wait > 0 || this.marks.has(p) || !this.pts.has(p)) return;
    const v = this.val(), d = Math.abs(v - this.target), gain = d < 1.2 ? 150 : Math.max(0, Math.round(100 - d * 4));
    this.marks.set(p, { v, gain }); this.pts.set(p, this.pts.get(p) + gain);
    if (gain >= 150) { this.c.banner(`PERFECT! ${p.name}`, 1300, p.color); this.c.sfx.win(); } else this.c.sfx.ding();
    if (this.marks.size === this.ps.length) this.t = Math.min(this.t, 0.6);
  }
  update(dt) {
    this.fx.update(dt);
    if (this.over) return;
    if (this.wait > 0) { this.wait -= dt; return; }
    if (this.phase === 'run') {
      this.ph += dt * this.speed * TAU * 0.5; this.t -= dt;
      for (const [b, at] of this.bots) { const left = at - dt; this.bots.set(b, left); if (left <= 0 && Math.abs(this.val() - this.target) < rnd(2, 12)) { this.bots.delete(b); this.onButton(b, 'a'); } }
      if (this.t <= 0) { this.phase = 'show'; this.t = 2.4; }
    } else if (this.phase === 'show') { this.t -= dt; if (this.t <= 0) this.next(); }
  }
  end() { if (this.done) return; this.done = true; this.over = true; this.c.finish(rankBy(this.ps, p => this.pts.get(p), p => `${this.pts.get(p)} pts`)); }
  draw(g) {
    bgGrad(g, '#364fc7', '#0b1433');
    txt(g, `STOP THE METER · ${Math.min(this.round, this.R)}/${this.R}`, 800, 120, 34, '#dbe4ff');
    const X = 200, W = 1200, Y = 380;
    g.fillStyle = 'rgba(0,0,0,.35)'; rr(g, X - 10, Y - 50, W + 20, 120, 30); g.fill();
    const gr = g.createLinearGradient(X, 0, X + W, 0); gr.addColorStop(0, '#ff4d6d'); gr.addColorStop(0.5, '#4dabff'); gr.addColorStop(1, '#ff4d6d'); g.fillStyle = gr; rr(g, X, Y - 30, W, 80, 24); g.fill();
    const zx = X + (this.target / 100) * W; g.fillStyle = '#ffd43b'; g.fillRect(zx - W * 0.03, Y - 40, W * 0.06, 100); g.fillStyle = '#fff'; g.fillRect(zx - 2, Y - 40, 4, 100);
    if (this.phase === 'run' && this.wait <= 0) { const nx = X + (this.val() / 100) * W; g.fillStyle = '#fff'; g.beginPath(); g.moveTo(nx, Y - 60); g.lineTo(nx - 18, Y - 90); g.lineTo(nx + 18, Y - 90); g.fill(); g.fillRect(nx - 4, Y - 60, 8, 130); }
    let k = 0; for (const [p, m] of this.marks) { const mx = X + (m.v / 100) * W; g.fillStyle = p.color; g.fillRect(mx - 3, Y - 40, 6, 100); txt(g, `${p.name} +${m.gain}`, mx, Y + 100 + (k % 4) * 34, 20, p.color, 'center', 'Nunito'); k++; }
    if (this.wait > 0) txt(g, 'GET READY…', 800, 650, 50);
    strip(g, rankBy(this.ps, p => this.pts.get(p), () => '').map(x => ({ name: x.p.name, color: x.p.color, val: this.pts.get(x.p), hi: this.marks.has(x.p) })));
    this.fx.draw(g);
  }
}
