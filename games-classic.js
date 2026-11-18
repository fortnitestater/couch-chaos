/* Couch Chaos — classic games. Needs engine.js */

/* =====================================================================
   ✈️ SKY BRAWL — top-down dogfight
   ===================================================================== */
const SKY_ITEMS = {
  triple: { c: '#ffd43b', s: '3x', n: 'TRIPLE SHOT' },
  missile: { c: '#ff6b6b', s: '🚀', n: 'HOMING MISSILES' },
  shield: { c: '#66d9e8', s: '🛡️', n: 'SHIELD' },
  mega: { c: '#cc5de8', s: 'XL', n: 'MEGA PLANE' },
  speed: { c: '#51cf66', s: '⚡', n: 'TURBO' },
};
const SKY_EVENTS = { meteors: '☄️ METEOR SHOWER!', reverse: '🙃 REVERSE CONTROLS!', frenzy: '🔥 BULLET FRENZY!', tiny: '🐜 TINY PLANES!' };

class SkyBrawl {
  constructor(c) {
    this.c = c; this.T = 120; this.planes = []; this.shots = []; this.items = []; this.rocks = []; this.fx = new Particles();
    this.itemT = 3; this.eventT = rnd(16, 22); this.event = null; this.evLeft = 0; this.rockT = 0; this.feedL = [];
    this.layout = { name: 'pad', stick: 'x', a: 'FIRE', b: 'BOOST', hint: 'Tilt, drag, or ← → to steer' };
    this.clouds = Array.from({ length: 14 }, () => ({ x: rnd(0, 1600), y: rnd(0, 900), r: rnd(40, 110), s: rnd(10, 35), a: rnd(0.25, 0.6) }));
  }
  start() {
    const ps = this.c.roster(2);
    ps.forEach((p, i) => { const pl = { p, kills: 0, deaths: 0 }; this.spawn(pl, i, ps.length); this.planes.push(pl); });
    this.c.setLayout('all', this.layout);
    this.c.banner('✈️ SKY BRAWL!');
  }
  onJoin(p) {
    if (this.planes.some(pl => pl.p === p)) return false;
    const pl = { p, kills: 0, deaths: 0 }; this.spawn(pl); this.planes.push(pl);
    this.c.setLayout(p, this.layout); return true;
  }
  spawn(pl, i, n) {
    const a = (i === undefined ? Math.random() : i / n) * TAU;
    Object.assign(pl, { x: 800 + Math.cos(a) * 340, y: 450 + Math.sin(a) * 300, ang: a + Math.PI + 0.4, hp: 3, alive: true, inv: 2, cd: 0, mcd: 0,
      energy: 1, speed: 300, pow: null, powT: 0, bank: 0, flash: 0, trail: 0 });
  }
  scale(pl) { return 1.35 * (pl.pow === 'mega' ? 1.8 : 1) * (this.event === 'tiny' ? 0.55 : 1); }
  feed(s) { this.feedL.unshift({ s, t: 4 }); this.feedL.length = Math.min(this.feedL.length, 5); }
  update(dt) {
    this.T -= dt; this.fx.update(dt);
    if (this.T <= 0) return this.end();
    for (const c of this.clouds) { c.x += c.s * dt; if (c.x > 1600 + c.r * 2) { c.x = -c.r * 2; c.y = rnd(0, 900); } }
    for (const f of this.feedL) f.t -= dt;
    this.feedL = this.feedL.filter(f => f.t > 0);
    // crazy events
    if (!this.event) {
      this.eventT -= dt;
      if (this.eventT <= 0 && this.T > 12) { this.event = pick(Object.keys(SKY_EVENTS)); this.evLeft = 10; this.c.banner(SKY_EVENTS[this.event], 2200, '#ff922b'); this.c.sfx.alarm(); }
    } else { this.evLeft -= dt; if (this.evLeft <= 0) { this.event = null; this.eventT = rnd(14, 22); } }
    // power-up crates
    this.itemT -= dt;
    if (this.itemT <= 0 && this.items.length < 3) { this.items.push({ x: rnd(120, 1480), y: rnd(130, 800), k: pick(Object.keys(SKY_ITEMS)), t: 0 }); this.itemT = rnd(4, 7); }
    for (const it of this.items) it.t += dt;
    // meteors
    if (this.event === 'meteors') {
      this.rockT -= dt;
      if (this.rockT <= 0) { this.rockT = 0.3; this.rocks.push({ x: rnd(-150, 1450), y: -60, vx: rnd(60, 180), vy: rnd(260, 420), r: rnd(20, 38), a: 0, va: rnd(-3, 3), hp: 2 }); }
    }
    for (const r of this.rocks) {
      r.x += r.vx * dt; r.y += r.vy * dt; r.a += r.va * dt;
      if (Math.random() < 0.6) this.fx.puff(r.x, r.y, 'rgba(255,146,43,.7)', r.r * 0.4, 0.4);
    }
    // planes
    for (const pl of this.planes) {
      if (!pl.alive) { pl.respawn -= dt; if (pl.respawn <= 0) this.spawn(pl); continue; }
      const inp = pl.p.bot ? this.bot(pl) : pl.p.input;
      let turn = clamp(inp.x || 0); if (this.event === 'reverse') turn = -turn;
      pl.bank += (turn - pl.bank) * Math.min(1, dt * 8);
      pl.ang += turn * 3.3 * dt;
      const boost = inp.b && pl.energy > 0.02;
      pl.energy = boost ? Math.max(0, pl.energy - dt * 0.7) : Math.min(1, pl.energy + dt * 0.22);
      const tgt = (boost ? 560 : 310) * (pl.pow === 'speed' ? 1.45 : 1);
      pl.speed += (tgt - pl.speed) * Math.min(1, dt * 3);
      pl.x = (pl.x + Math.cos(pl.ang) * pl.speed * dt + 1600) % 1600;
      pl.y = (pl.y + Math.sin(pl.ang) * pl.speed * dt + 900) % 900;
      pl.inv = Math.max(0, pl.inv - dt); pl.flash = Math.max(0, pl.flash - dt); pl.mcd -= dt;
      if (pl.pow) { pl.powT -= dt; if (pl.powT <= 0) pl.pow = null; }
      const sc = this.scale(pl);
      pl.trail -= dt;
      if (pl.trail <= 0) { pl.trail = 0.03; this.fx.puff(pl.x - Math.cos(pl.ang) * 26 * sc, pl.y - Math.sin(pl.ang) * 26 * sc, boost ? 'rgba(255,190,70,.85)' : 'rgba(255,255,255,.55)', (boost ? 8 : 5) * sc, 0.5); }
      pl.cd -= dt;
      if (inp.a && pl.cd <= 0) this.fire(pl);
      for (const it of this.items) {
        if (!it.dead && Math.hypot(it.x - pl.x, it.y - pl.y) < 46 * sc) {
          it.dead = true; pl.pow = it.k; pl.powT = it.k === 'shield' ? 7 : 9;
          this.fx.burst(it.x, it.y, 20, SKY_ITEMS[it.k].c, 260, 0.6, 6); this.c.sfx.pickup();
          this.feed(`${pl.p.name} got ${SKY_ITEMS[it.k].n}`); this.c.send(pl.p, { t: 'buzz', ms: 40 });
        }
      }
      for (const r of this.rocks) {
        if (r.hp > 0 && pl.alive && pl.inv <= 0 && Math.hypot(r.x - pl.x, r.y - pl.y) < r.r + 20 * sc) { r.hp = 0; this.rockBoom(r); this.hit(pl, null, 2); }
      }
    }
    this.items = this.items.filter(i => !i.dead);
    // bullets
    for (const s of this.shots) {
      if (s.homing) {
        const t = this.nearest(s.x, s.y, s.o);
        if (t) {
          const cur = Math.atan2(s.vy, s.vx), da = angDiff(cur, Math.atan2(t.y - s.y, t.x - s.x));
          const a = cur + clamp(da, -4 * dt, 4 * dt), sp = Math.hypot(s.vx, s.vy);
          s.vx = Math.cos(a) * sp; s.vy = Math.sin(a) * sp;
        }
        if (Math.random() < 0.5) this.fx.puff(s.x, s.y, 'rgba(255,255,255,.6)', 4, 0.4);
      }
      s.x = (s.x + s.vx * dt + 1600) % 1600; s.y = (s.y + s.vy * dt + 900) % 900; s.life -= dt;
      for (const pl of this.planes) {
        if (pl === s.o || !pl.alive || pl.inv > 0) continue;
        const r = 22 * this.scale(pl) + s.r;
        if ((pl.x - s.x) ** 2 + (pl.y - s.y) ** 2 < r * r) { s.life = 0; this.hit(pl, s.o, s.dmg); break; }
      }
      for (const r of this.rocks) {
        if (s.life > 0 && r.hp > 0 && Math.hypot(r.x - s.x, r.y - s.y) < r.r + s.r) { s.life = 0; if (--r.hp <= 0) this.rockBoom(r); }
      }
    }
    this.shots = this.shots.filter(s => s.life > 0);
    this.rocks = this.rocks.filter(r => r.hp > 0 && r.y < 980);
  }
  nearest(x, y, not) {
    let best = null, bd = 1e9;
    for (const o of this.planes) { if (o === not || !o.alive) continue; const d = Math.hypot(o.x - x, o.y - y); if (d < bd) { bd = d; best = o; } }
    return best;
  }
  fire(pl) {
    pl.cd = this.event === 'frenzy' ? 0.06 : 0.16;
    const sc = this.scale(pl), mega = pl.pow === 'mega';
    const mk = (da, homing) => {
      const a = pl.ang + da, sp = homing ? 520 : 980;
      this.shots.push({ x: pl.x + Math.cos(a) * 30 * sc, y: pl.y + Math.sin(a) * 30 * sc, vx: Math.cos(a) * sp, vy: Math.sin(a) * sp,
        life: homing ? 2.4 : 0.85, o: pl, r: mega ? 11 : 5, dmg: homing || mega ? 2 : 1, homing, color: pl.p.color });
    };
    if (pl.pow === 'triple') { mk(-0.2); mk(0); mk(0.2); } else mk(0);
    if (pl.pow === 'missile' && pl.mcd <= 0) { mk(0, true); pl.mcd = 0.55; }
    this.c.sfx.shoot();
  }
  hit(pl, by, dmg) {
    if (pl.pow === 'shield') { this.fx.burst(pl.x, pl.y, 8, '#66d9e8', 150, 0.4, 4); this.c.sfx.tink(); return; }
    pl.hp -= dmg; pl.flash = 0.12;
    this.fx.burst(pl.x, pl.y, 6, '#ffd43b', 180, 0.4, 4); this.c.sfx.hit(); this.c.send(pl.p, { t: 'buzz', ms: 60 });
    if (pl.hp <= 0) {
      pl.alive = false; pl.respawn = 2.5; pl.deaths++;
      if (by && by !== pl) by.kills++;
      this.fx.burst(pl.x, pl.y, 40, pl.p.color, 380, 1.1, 9); this.fx.burst(pl.x, pl.y, 25, '#ff922b', 260, 0.9, 12);
      this.c.sfx.boom(); this.c.shake(16); this.c.send(pl.p, { t: 'buzz', ms: 400 });
      this.feed(by ? `${by.p.name} 💥 ${pl.p.name}` : `☄️ bonked ${pl.p.name}`);
    }
  }
  rockBoom(r) { this.fx.burst(r.x, r.y, 20, '#8d6e63', 240, 0.7, 7); this.fx.burst(r.x, r.y, 10, '#ff922b', 200, 0.5, 8); this.c.sfx.boom(); this.c.shake(5); }
  bot(pl) {
    const t = this.nearest(pl.x, pl.y, pl);
    let target = t, td = t ? Math.hypot(t.x - pl.x, t.y - pl.y) : 1e9;
    for (const it of this.items) { const d = Math.hypot(it.x - pl.x, it.y - pl.y); if (d < 350 && d < td) { target = it; td = d; } }
    if (!target) return { x: Math.sin(this.T + pl.y) * 0.5, a: false, b: false };
    const da = angDiff(pl.ang, Math.atan2(target.y - pl.y, target.x - pl.x));
    const aim = t ? Math.abs(angDiff(pl.ang, Math.atan2(t.y - pl.y, t.x - pl.x))) : 9;
    return { x: clamp(da * 2.2) * 0.8 * (this.event === 'reverse' ? -1 : 1), a: aim < 0.3 && td < 650, b: td > 700 && pl.energy > 0.5 };
  }
  end() {
    if (this.done) return; this.done = true;
    const r = [...this.planes].sort((a, b) => b.kills - a.kills || a.deaths - b.deaths).map(pl => ({ p: pl.p, stat: `${pl.kills} KOs · ${pl.deaths} crashes` }));
    this.c.finish(r);
  }
  cloud(g, c) {
    g.globalAlpha = c.a; g.fillStyle = '#fff';
    for (let k = 0; k < 4; k++) { g.beginPath(); g.arc(c.x + k * c.r * 0.55 - c.r * 0.8, c.y + (k % 2 ? -c.r * 0.25 : 0), c.r * (k % 2 ? 0.7 : 0.55), 0, TAU); g.fill(); }
    g.globalAlpha = 1;
  }
  drawPlane(g, pl) {
    const sc = this.scale(pl);
    // shadow
    g.save(); g.translate(pl.x + 18 * sc, pl.y + 26 * sc); g.rotate(pl.ang); g.scale(sc, sc * (1 - Math.abs(pl.bank) * 0.45));
    g.fillStyle = 'rgba(0,30,80,.18)'; this.planePath(g); g.fill(); g.restore();
    g.save(); g.translate(pl.x, pl.y); g.rotate(pl.ang); g.scale(sc, sc * (1 - Math.abs(pl.bank) * 0.45));
    if (pl.inv > 0 && ((pl.inv * 10) | 0) % 2) g.globalAlpha = 0.35;
    this.planePath(g);
    g.fillStyle = pl.flash > 0 ? '#fff' : pl.p.color; g.fill();
    g.lineWidth = 3; g.strokeStyle = '#1a1438'; g.stroke();
    g.fillStyle = '#d0ebff'; g.beginPath(); g.ellipse(8, 0, 8, 4.5, 0, 0, TAU); g.fill(); g.stroke();
    g.restore(); g.globalAlpha = 1;
    if (pl.pow === 'shield') { g.strokeStyle = 'rgba(102,217,232,.85)'; g.lineWidth = 4; g.beginPath(); g.arc(pl.x, pl.y, 40 * sc, 0, TAU); g.stroke(); }
    txt(g, pl.p.name, pl.x, pl.y - 44 * sc - 6, 17, pl.p.color, 'center', 'Nunito');
    for (let i = 0; i < 3; i++) { g.fillStyle = i < pl.hp ? '#51cf66' : 'rgba(0,0,0,.35)'; g.fillRect(pl.x - 22 + i * 15, pl.y + 34 * sc, 12, 5); }
    if (pl.pow) txt(g, SKY_ITEMS[pl.pow].s, pl.x + 34 * sc, pl.y - 26 * sc, 18);
  }
  planePath(g) {
    g.beginPath(); g.moveTo(30, 0); g.lineTo(8, -6); g.lineTo(-4, -28); g.lineTo(-13, -28); g.lineTo(-8, -6); g.lineTo(-20, -5); g.lineTo(-27, -15);
    g.lineTo(-32, -15); g.lineTo(-27, 0); g.lineTo(-32, 15); g.lineTo(-27, 15); g.lineTo(-20, 5); g.lineTo(-8, 6); g.lineTo(-13, 28); g.lineTo(-4, 28); g.lineTo(8, 6); g.closePath();
  }
  draw(g) {
    const gr = g.createLinearGradient(0, 0, 0, 900);
    const met = this.event === 'meteors';
    gr.addColorStop(0, met ? '#ff7a59' : '#3b82f6'); gr.addColorStop(1, met ? '#ffd3a1' : '#bde3ff');
    g.fillStyle = gr; g.fillRect(0, 0, 1600, 900);
    for (let i = 0; i < 8; i++) this.cloud(g, this.clouds[i]);
    for (const it of this.items) {
      const m = SKY_ITEMS[it.k], y = it.y + Math.sin(it.t * 4) * 6;
      g.save(); g.translate(it.x, y); g.rotate(Math.sin(it.t * 2) * 0.2);
      g.fillStyle = m.c; rr(g, -24, -24, 48, 48, 10); g.fill(); g.lineWidth = 4; g.strokeStyle = '#fff'; g.stroke();
      g.restore(); txt(g, m.s, it.x, y + 1, 20);
    }
    for (const r of this.rocks) {
      g.save(); g.translate(r.x, r.y); g.rotate(r.a); g.fillStyle = '#6d4c41'; g.beginPath();
      for (let k = 0; k < 8; k++) { const a = (k / 8) * TAU, rr2 = r.r * (k % 2 ? 0.8 : 1); g.lineTo(Math.cos(a) * rr2, Math.sin(a) * rr2); }
      g.closePath(); g.fill(); g.lineWidth = 3; g.strokeStyle = '#3e2723'; g.stroke(); g.restore();
    }
    for (const s of this.shots) {
      g.fillStyle = s.color; g.globalAlpha = 0.45; g.beginPath(); g.arc(s.x, s.y, s.r * 2.2, 0, TAU); g.fill(); g.globalAlpha = 1;
      g.fillStyle = s.homing ? '#ff6b6b' : '#fff9c4'; g.beginPath(); g.arc(s.x, s.y, s.r, 0, TAU); g.fill();
    }
    for (const pl of this.planes) if (pl.alive) this.drawPlane(g, pl);
    this.fx.draw(g);
    for (let i = 8; i < this.clouds.length; i++) this.cloud(g, this.clouds[i]);
    // HUD
    const m = Math.max(0, Math.ceil(this.T));
    txt(g, `${(m / 60) | 0}:${String(m % 60).padStart(2, '0')}`, 800, 42, 44, m <= 10 ? '#ff4d6d' : '#fff');
    board(g, [...this.planes].sort((a, b) => b.kills - a.kills).map(pl => ({ name: pl.p.name, color: pl.p.color, val: pl.kills })), 20, 20, 'KOs');
    if (this.event) txt(g, `${SKY_EVENTS[this.event]} ${Math.ceil(this.evLeft)}`, 800, 92, 26, '#ffd43b');
    this.feedL.forEach((f, i) => { g.globalAlpha = Math.min(1, f.t); txt(g, f.s, 1580, 34 + i * 34, 20, '#fff', 'right', 'Nunito'); });
    g.globalAlpha = 1;
    for (const pl of this.planes) if (!pl.alive && !pl.p.bot) txt(g, `${pl.p.name} respawning…`, 800, 860, 22, pl.p.color, 'center', 'Nunito');
  }
}

/* =====================================================================
   🫧 SUMO BLOBS — shove everyone into the lava
   ===================================================================== */
class Sumo {
  constructor(c) { this.c = c; this.round = 0; this.fx = new Particles(); this.pts = new Map(); this.bubbles = Array.from({ length: 30 }, () => ({ x: rnd(0, 1600), y: rnd(0, 900), r: rnd(6, 26), t: rnd(0, 3) })); }
  start() {
    this.ps = this.c.roster(2);
    this.ps.forEach(p => this.pts.set(p, 0));
    this.c.setLayout('all', { name: 'pad', stick: 'xy', a: 'HEAVY', b: 'DASH', hint: 'Move: tilt / drag / WASD. Hold HEAVY to become a rock.' });
    this.nextRound();
  }
  nextRound() {
    this.round++;
    this.mod = [null, 'ice', 'bouncy'][this.round - 1];
    this.R = 420; this.phase = 'count'; this.t = 3.5; this.lastTick = 4; this.out = []; this.el = 0;
    this.blobs = this.ps.map((p, i) => {
      const a = (i / this.ps.length) * TAU - Math.PI / 2;
      return { p, x: 800 + Math.cos(a) * 270, y: 480 + Math.sin(a) * 270, vx: 0, vy: 0, r: 36, m: 1, alive: true, cd: 0, fall: 0, face: a + Math.PI, heavy: false, squish: 0 };
    });
    this.c.banner(`ROUND ${this.round}` + (this.mod === 'ice' ? ' · ICE!' : this.mod === 'bouncy' ? ' · MEGA BOUNCE!' : ''), 2200);
  }
  update(dt) {
    this.fx.update(dt);
    for (const b of this.bubbles) { b.t += dt; if (b.t > 3) { b.t = 0; b.x = rnd(0, 1600); b.y = rnd(0, 900); } }
    for (const b of this.blobs) if (!b.alive && b.fall > 0) { b.fall -= dt * 1.4; b.x += b.vx * dt * 0.3; b.y += b.vy * dt * 0.3; }
    if (this.phase === 'count') {
      this.t -= dt;
      if (Math.ceil(this.t) < this.lastTick && this.t > 0) { this.lastTick = Math.ceil(this.t); this.c.sfx.tick(); }
      if (this.t <= 0) { this.phase = 'fight'; this.c.sfx.go(); this.c.banner('PUSH!', 900, '#ff4d6d'); }
      return;
    }
    if (this.phase === 'over') { this.t -= dt; if (this.t <= 0) { this.round >= 3 ? this.end() : this.nextRound(); } return; }
    this.el += dt;
    this.R = Math.max(0, this.R - dt * (this.el > 30 ? 14 : 6));
    const fr = this.mod === 'ice' ? 0.45 : 2.4, e = this.mod === 'bouncy' ? 1.7 : 0.9;
    const alive = this.blobs.filter(b => b.alive);
    for (const b of alive) {
      const inp = b.p.bot ? this.bot(b) : b.p.input;
      b.heavy = !!inp.a; b.m = b.heavy ? 3.5 : 1;
      let ax = inp.x || 0, ay = inp.y || 0; const l = Math.hypot(ax, ay); if (l > 1) { ax /= l; ay /= l; }
      const acc = (b.heavy ? 450 : 1500) * (this.mod === 'ice' ? 0.6 : 1);
      b.vx += ax * acc * dt; b.vy += ay * acc * dt;
      b.cd -= dt;
      if (inp.b && b.cd <= 0 && !b.heavy) {
        let dx = ax, dy = ay;
        if (Math.hypot(dx, dy) < 0.2) { const v = Math.hypot(b.vx, b.vy) || 1; dx = b.vx / v; dy = b.vy / v; if (!b.vx && !b.vy) { dx = Math.cos(b.face); dy = Math.sin(b.face); } }
        const dl = Math.hypot(dx, dy) || 1; b.vx += (dx / dl) * 950; b.vy += (dy / dl) * 950; b.cd = 1.2;
        this.fx.burst(b.x, b.y, 12, b.p.color, 220, 0.5, 6); this.c.sfx.dash();
      }
      const k = Math.exp(-fr * dt); b.vx *= k; b.vy *= k;
      b.x += b.vx * dt; b.y += b.vy * dt;
      if (Math.hypot(b.vx, b.vy) > 40) b.face = Math.atan2(b.vy, b.vx);
      b.squish = Math.max(0, b.squish - dt * 3);
    }
    for (let i = 0; i < alive.length; i++) for (let j = i + 1; j < alive.length; j++) {
      const a = alive[i], b = alive[j];
      const dx = b.x - a.x, dy = b.y - a.y, d = Math.hypot(dx, dy), min = a.r + b.r;
      if (d < min && d > 0.01) {
        const nx = dx / d, ny = dy / d, ia = 1 / a.m, ib = 1 / b.m, ov = min - d;
        a.x -= nx * ov * ia / (ia + ib); a.y -= ny * ov * ia / (ia + ib); b.x += nx * ov * ib / (ia + ib); b.y += ny * ov * ib / (ia + ib);
        const rv = (b.vx - a.vx) * nx + (b.vy - a.vy) * ny;
        if (rv < 0) {
          const jj = -(1 + e) * rv / (ia + ib);
          a.vx -= jj * ia * nx; a.vy -= jj * ia * ny; b.vx += jj * ib * nx; b.vy += jj * ib * ny;
          if (jj > 250) {
            a.squish = b.squish = Math.min(1, jj / 1200);
            this.c.sfx.bonk(); this.fx.burst(a.x + nx * a.r, a.y + ny * a.r, 8, '#fff', 200, 0.4, 5); this.c.shake(Math.min(12, jj / 150));
            this.c.send(a.p, { t: 'buzz', ms: 50 }); this.c.send(b.p, { t: 'buzz', ms: 50 });
          }
        }
      }
    }
    for (const b of alive) {
      if (Math.hypot(b.x - 800, b.y - 480) > this.R + b.r * 0.2) {
        b.alive = false; b.fall = 1; this.out.push(b);
        this.fx.burst(b.x, b.y, 30, '#a5d8ff', 300, 0.9, 9); this.fx.burst(b.x, b.y, 14, '#fff', 200, 0.7, 6); this.c.sfx.splash(); this.c.shake(10);
        this.c.send(b.p, { t: 'buzz', ms: 500 });
      }
    }
    const left = this.blobs.filter(b => b.alive);
    if (left.length <= 1) {
      this.phase = 'over'; this.t = 3;
      this.out.forEach((b, i) => this.pts.set(b.p, this.pts.get(b.p) + i));
      if (left[0]) { this.pts.set(left[0].p, this.pts.get(left[0].p) + this.blobs.length); this.c.banner(`${left[0].p.name} WINS ROUND!`, 2200, left[0].p.color); this.fx.confetti(80); this.c.sfx.win(); }
      else this.c.banner('DOUBLE SPLASH!', 2000);
    }
  }
  bot(b) {
    let t = null, bd = 1e9;
    for (const o of this.blobs) { if (o === b || !o.alive) continue; const d = Math.hypot(o.x - b.x, o.y - b.y); if (d < bd) { bd = d; t = o; } }
    let dx = 0, dy = 0;
    if (t) { dx = (t.x - b.x) / bd; dy = (t.y - b.y) / bd; }
    const ex = b.x - 800, ey = b.y - 480, ed = Math.hypot(ex, ey) || 1;
    if (ed > this.R * 0.55) { const k = 2 * (ed / Math.max(1, this.R)); dx -= (ex / ed) * k; dy -= (ey / ed) * k; }
    return { x: dx, y: dy, a: false, b: !!t && bd < 140 && Math.random() < 0.04 };
  }
  end() {
    if (this.done) return; this.done = true;
    this.c.finish([...this.ps].sort((a, b) => this.pts.get(b) - this.pts.get(a)).map(p => ({ p, stat: `${this.pts.get(p)} pts` })));
  }
  draw(g) {
    const gr = g.createRadialGradient(800, 480, 100, 800, 480, 900);
    gr.addColorStop(0, '#1971c2'); gr.addColorStop(1, '#061a33');
    g.fillStyle = gr; g.fillRect(0, 0, 1600, 900);
    for (const b of this.bubbles) { const k = Math.sin((b.t / 3) * Math.PI); g.strokeStyle = `rgba(200,235,255,${0.35 * k})`; g.lineWidth = 3; g.beginPath(); g.ellipse(b.x, b.y, b.r * 3 * k, b.r * k, 0, 0, TAU); g.stroke(); }
    // arena
    if (this.R > 0) {
      g.fillStyle = 'rgba(0,0,0,.35)'; g.beginPath(); g.arc(808, 498, this.R, 0, TAU); g.fill();
      const ag = g.createRadialGradient(800, 470, 20, 800, 480, this.R);
      ag.addColorStop(0, '#ffffff'); ag.addColorStop(0.7, this.mod === 'ice' ? '#d0ebff' : '#e7f5ff'); ag.addColorStop(1, this.mod === 'ice' ? '#4dabf7' : '#a5d8ff');
      g.fillStyle = ag; g.beginPath(); g.arc(800, 480, this.R, 0, TAU); g.fill();
      g.lineWidth = 10; g.strokeStyle = this.mod === 'bouncy' ? '#cc5de8' : '#74c0fc'; g.stroke();
      g.lineWidth = 3; g.strokeStyle = 'rgba(0,0,0,.12)'; g.beginPath(); g.arc(800, 480, this.R * 0.55, 0, TAU); g.stroke();
    }
    const ordered = [...this.blobs].sort((a, b) => (a.alive - b.alive) || a.y - b.y);
    for (const b of ordered) {
      const s = b.alive ? 1 : Math.max(0, b.fall);
      if (s <= 0) continue;
      g.save(); g.translate(b.x, b.y); g.globalAlpha = b.alive ? 1 : s;
      const sq = 1 + b.squish * 0.25;
      g.scale(s * sq, s / sq);
      if (b.alive) { g.fillStyle = 'rgba(0,0,0,.25)'; g.beginPath(); g.ellipse(4, b.r * 0.8, b.r * 0.9, b.r * 0.35, 0, 0, TAU); g.fill(); }
      g.fillStyle = b.heavy ? '#495057' : b.p.color; g.beginPath(); g.arc(0, 0, b.r, 0, TAU); g.fill();
      g.lineWidth = b.heavy ? 7 : 4; g.strokeStyle = b.heavy ? b.p.color : 'rgba(20,14,50,.7)'; g.stroke();
      g.fillStyle = 'rgba(255,255,255,.35)'; g.beginPath(); g.ellipse(-b.r * 0.35, -b.r * 0.4, b.r * 0.3, b.r * 0.18, -0.6, 0, TAU); g.fill();
      const fx = Math.cos(b.face) * 10, fy = Math.sin(b.face) * 10;
      for (const side of [-1, 1]) {
        const ex = fx + Math.cos(b.face + Math.PI / 2) * 12 * side, ey = fy + Math.sin(b.face + Math.PI / 2) * 12 * side - 4;
        g.fillStyle = '#fff'; g.beginPath(); g.arc(ex, ey, 9, 0, TAU); g.fill();
        g.fillStyle = '#111'; g.beginPath(); g.arc(ex + Math.cos(b.face) * 3, ey + Math.sin(b.face) * 3, b.alive ? 4.5 : 2, 0, TAU); g.fill();
      }
      g.restore(); g.globalAlpha = 1;
      if (b.alive) txt(g, b.p.name, b.x, b.y - b.r - 18, 18, b.p.color, 'center', 'Nunito');
    }
    this.fx.draw(g);
    txt(g, `ROUND ${this.round}/3`, 800, 40, 34);
    board(g, [...this.ps].sort((a, b) => this.pts.get(b) - this.pts.get(a)).map(p => ({ name: p.name, color: p.color, val: this.pts.get(p) })), 20, 20, 'POINTS');
    if (this.phase === 'count') txt(g, String(Math.max(1, Math.ceil(this.t))), 800, 480, 200, '#ffd43b');
  }
}

/* =====================================================================
   🃏 WILD EIGHTS — crazy-eights style card game, hands on the phones
   ===================================================================== */
const SUITS = ['♠', '♥', '♦', '♣'];
const RANKS = ['A', '2', '3', '4', '5', '6', '7', '8', '9', '10', 'J', 'Q', 'K'];
const cRank = c => c.slice(0, -1), cSuit = c => c.slice(-1);
const CARD_TAG = { '8': 'WILD', '2': '+2', 'J': 'SKIP', 'Q': 'REVERSE', 'A': 'CHAOS' };
function drawCard(g, x, y, w, h, card, rot = 0) {
  g.save(); g.translate(x, y); g.rotate(rot);
  g.fillStyle = 'rgba(0,0,0,.3)'; rr(g, -w / 2 + 4, -h / 2 + 6, w, h, w * 0.1); g.fill();
  if (card) {
    g.fillStyle = '#fff'; rr(g, -w / 2, -h / 2, w, h, w * 0.1); g.fill();
    const red = '♥♦'.includes(cSuit(card)), col = red ? '#e03131' : '#1a1438';
    const tag = CARD_TAG[cRank(card)];
    if (tag) { g.lineWidth = w * 0.05; g.strokeStyle = '#7048e8'; rr(g, -w / 2 + w * 0.05, -h / 2 + w * 0.05, w * 0.9, h - w * 0.1, w * 0.07); g.stroke(); }
    g.fillStyle = col; g.textBaseline = 'middle';
    g.font = `900 ${w * 0.28}px Nunito, sans-serif`; g.textAlign = 'left'; g.fillText(cRank(card), -w / 2 + w * 0.1, -h / 2 + w * 0.22);
    g.font = `${w * 0.55}px serif`; g.textAlign = 'center'; g.fillText(cSuit(card), 0, tag ? -h * 0.04 : h * 0.03);
    if (tag) { g.fillStyle = '#7048e8'; g.font = `900 ${w * 0.15}px Nunito, sans-serif`; g.fillText(tag, 0, h * 0.32); }
  } else {
    g.fillStyle = '#5f3dc4'; rr(g, -w / 2, -h / 2, w, h, w * 0.1); g.fill();
    g.lineWidth = w * 0.05; g.strokeStyle = '#fff'; rr(g, -w / 2 + w * 0.08, -h / 2 + w * 0.08, w * 0.84, h - w * 0.16, w * 0.07); g.stroke();
    g.fillStyle = '#ffd43b'; g.font = `${w * 0.35}px Bungee, Impact`; g.textAlign = 'center'; g.textBaseline = 'middle'; g.fillText('C', 0, 2);
  }
  g.restore();
}

class WildEights {
  constructor(c) { this.c = c; this.fx = new Particles(); this.fly = null; this.botWait = 1.2; }
  start() {
    this.ps = this.c.roster(2);
    const decks = this.ps.length > 4 ? 2 : 1;
    this.deck = [];
    for (let d = 0; d < decks; d++) for (const s of SUITS) for (const r of RANKS) this.deck.push(r + s);
    shuffle(this.deck);
    const n = this.ps.length > 5 ? 5 : 7;
    this.hands = new Map(this.ps.map(p => [p, this.deck.splice(0, n)]));
    let top;
    do { top = this.deck.pop(); if (CARD_TAG[cRank(top)]) { this.deck.unshift(top); top = null; } } while (!top);
    this.pile = [top]; this.suit = cSuit(top);
    this.turn = (Math.random() * this.ps.length) | 0; this.dir = 1; this.pending = 0; this.drew = false; this.needSuit = false; this.timer = 30; this.over = false;
    this.c.setLayout('all', { name: 'cards' });
    this.sync();
    this.c.banner('🃏 WILD EIGHTS!');
  }
  get cur() { return this.ps[this.turn]; }
  top() { return this.pile[this.pile.length - 1]; }
  idx(k) { const n = this.ps.length; return (((this.turn + this.dir * k) % n) + n) % n; }
  playable(card) {
    if (this.pending > 0) return cRank(card) === '2';
    return cRank(card) === '8' || cSuit(card) === this.suit || cRank(card) === cRank(this.top());
  }
  handMsg(p) {
    const h = this.hands.get(p), my = p === this.cur && !this.over;
    return { t: 'hand', cards: h, playable: h.map(c => my && !this.needSuit && this.playable(c)), myTurn: my, needSuit: my && this.needSuit,
      canDraw: my && !this.needSuit && (!this.drew || this.pending > 0), canPass: my && this.drew && !this.needSuit,
      top: this.top(), suit: this.suit, pending: this.pending, turnName: this.cur.name, turnColor: this.cur.color };
  }
  sync() { for (const p of this.ps) if (!p.bot) this.c.send(p, this.handMsg(p)); }
  resync(p) { if (this.hands.has(p)) this.c.send(p, this.handMsg(p)); }
  onAct(p, m) {
    if (this.over || p !== this.cur) return;
    if (m.a === 'suit') {
      if (!this.needSuit || !SUITS.includes(m.s)) return;
      this.suit = m.s; this.needSuit = false;
      this.c.banner(`${p.name} picks ${m.s}`, 1300, '♥♦'.includes(m.s) ? '#ff6b6b' : '#fff');
      return this.advance(1);
    }
    if (this.needSuit) return;
    if (m.a === 'play') this.play(p, m.i | 0);
    else if (m.a === 'draw') this.doDraw(p);
    else if (m.a === 'pass' && this.drew) this.advance(1);
  }
  play(p, i) {
    const h = this.hands.get(p), card = h[i];
    if (!card || !this.playable(card)) return;
    h.splice(i, 1); this.pile.push(card); this.suit = cSuit(card);
    const s = this.seat(this.turn); this.fly = { card, x: s.x, y: s.y, t: 0 };
    this.c.sfx.card();
    if (h.length === 0) return this.win(p);
    if (h.length === 1) setTimeout(() => this.c.banner(`${p.name}: LAST CARD!`, 1500, '#ff4d6d'), 300);
    const r = cRank(card);
    if (r === '8') { this.needSuit = true; this.timer = Math.max(this.timer, 12); this.c.sfx.ding(); this.sync(); return; }
    if (r === '2') { this.pending += 2; this.c.banner(`+${this.pending}!`, 1100, '#ff922b'); this.c.sfx.alarm(); this.advance(1); }
    else if (r === 'J') { this.c.banner(`${this.ps[this.idx(1)].name} SKIPPED!`, 1300); this.c.sfx.buzz(); this.advance(2); }
    else if (r === 'Q') { this.dir *= -1; this.c.banner('🔄 REVERSE!', 1100); this.c.sfx.whoosh(); this.advance(this.ps.length === 2 ? 2 : 1); }
    else if (r === 'A') { this.chaos(); this.advance(1); }
    else this.advance(1);
  }
  chaos() {
    const n = this.ps.length, hs = this.ps.map(p => this.hands.get(p));
    this.ps.forEach((p, i) => this.hands.set(p, hs[(((i - this.dir) % n) + n) % n]));
    this.c.banner('🌀 CHAOS! HANDS PASS!', 2000, '#cc5de8'); this.c.sfx.whoosh(); this.c.shake(14);
    this.chaosT = 1.2;
    for (const p of this.ps) this.c.send(p, { t: 'buzz', ms: 200 });
  }
  give(p, n) {
    const h = this.hands.get(p);
    for (let k = 0; k < n; k++) {
      if (!this.deck.length) { const top = this.pile.pop(); this.deck = shuffle(this.pile); this.pile = [top]; }
      if (!this.deck.length) break;
      h.push(this.deck.pop());
    }
    this.c.sfx.card();
  }
  doDraw(p) {
    if (this.pending > 0) {
      const n = this.pending; this.give(p, n); this.pending = 0;
      this.c.banner(`${p.name} eats +${n}!`, 1300, '#ff922b'); this.c.send(p, { t: 'buzz', ms: 300 });
      return this.advance(1);
    }
    if (this.drew) return;
    this.give(p, 1); this.drew = true;
    if (!this.hands.get(p).some(c => this.playable(c))) return this.advance(1);
    this.sync();
  }
  advance(k) {
    this.turn = this.idx(k); this.drew = false; this.needSuit = false; this.timer = 30; this.botWait = rnd(1, 1.8);
    this.sync();
  }
  win(p) {
    this.over = true; this.endT = 3;
    this.c.banner(`${p.name} WINS!`, 2600, p.color); this.c.sfx.win(); this.fx.confetti(160);
    this.sync();
    this.winner = p;
  }
  update(dt) {
    this.fx.update(dt);
    if (this.fly) { this.fly.t += dt * 3.5; if (this.fly.t >= 1) this.fly = null; }
    if (this.chaosT) this.chaosT = Math.max(0, this.chaosT - dt);
    if (this.over) {
      this.endT -= dt;
      if (this.endT <= 0 && !this.done) {
        this.done = true;
        const rest = this.ps.filter(p => p !== this.winner).sort((a, b) => this.hands.get(a).length - this.hands.get(b).length);
        this.c.finish([this.winner, ...rest].map(p => ({ p, stat: p === this.winner ? 'empty hand!' : `${this.hands.get(p).length} cards left` })));
      }
      return;
    }
    const p = this.cur;
    if (p.bot) { this.botWait -= dt; if (this.botWait <= 0) { this.botWait = 0.9; this.botMove(); } return; }
    this.timer -= dt;
    if (this.timer <= 0) {
      if (this.needSuit) this.onAct(p, { a: 'suit', s: this.bestSuit(this.hands.get(p)) });
      else if (this.pending > 0 || !this.drew) { this.doDraw(p); if (this.cur === p && !this.over) this.advance(1); }
      else this.advance(1);
    }
  }
  bestSuit(h) { const cnt = {}; h.forEach(c => { if (cRank(c) !== '8') cnt[cSuit(c)] = (cnt[cSuit(c)] || 0) + 1; }); return Object.keys(cnt).sort((a, b) => cnt[b] - cnt[a])[0] || pick(SUITS); }
  botMove() {
    const p = this.cur, h = this.hands.get(p);
    if (this.needSuit) return this.onAct(p, { a: 'suit', s: this.bestSuit(h) });
    let i = h.findIndex(c => this.playable(c) && cRank(c) !== '8');
    if (i < 0) i = h.findIndex(c => this.playable(c));
    if (i >= 0) this.play(p, i);
    else if (!this.drew || this.pending) this.doDraw(p);
    else this.advance(1);
  }
  seat(i) { const n = this.ps.length, a = Math.PI / 2 + (i / n) * TAU; return { x: 800 + Math.cos(a) * 610, y: 450 + Math.sin(a) * 330 }; }
  draw(g) {
    const gr = g.createRadialGradient(800, 450, 80, 800, 450, 900);
    gr.addColorStop(0, '#2b8a3e'); gr.addColorStop(1, '#0b3d1e');
    g.fillStyle = gr; g.fillRect(0, 0, 1600, 900);
    g.strokeStyle = 'rgba(255,255,255,.08)'; g.lineWidth = 6; g.beginPath(); g.ellipse(800, 450, 560, 300, 0, 0, TAU); g.stroke();
    // direction arrows
    const t = performance.now() / 1000 * this.dir;
    g.strokeStyle = 'rgba(255,255,255,.22)'; g.lineWidth = 8;
    for (let k = 0; k < 4; k++) {
      const a0 = t * 0.6 + (k * TAU) / 4;
      g.beginPath(); g.ellipse(800, 450, 300, 190, 0, a0, a0 + 0.9); g.stroke();
      const a1 = this.dir > 0 ? a0 + 0.9 : a0, ax = 800 + Math.cos(a1) * 300, ay = 450 + Math.sin(a1) * 190;
      g.fillStyle = 'rgba(255,255,255,.22)'; g.beginPath(); g.arc(ax, ay, 10, 0, TAU); g.fill();
    }
    // piles
    for (let k = 0; k < 4; k++) drawCard(g, 660 - k * 2, 450 - k * 3, 130, 182, null);
    txt(g, `${this.deck.length}`, 660, 570, 22, '#fff', 'center', 'Nunito');
    const showTop = this.fly ? this.pile[this.pile.length - 2] : this.top();
    if (showTop) drawCard(g, 880, 450, 140, 196, showTop, 0.05);
    if (this.fly) {
      const e = 1 - Math.pow(1 - this.fly.t, 3);
      drawCard(g, this.fly.x + (880 - this.fly.x) * e, this.fly.y + (450 - this.fly.y) * e, 140, 196, this.fly.card, (1 - e) * 3 + 0.05);
    }
    // current suit
    g.fillStyle = '#fff'; g.beginPath(); g.arc(1040, 390, 44, 0, TAU); g.fill();
    g.fillStyle = '♥♦'.includes(this.suit) ? '#e03131' : '#1a1438'; g.font = '60px serif'; g.textAlign = 'center'; g.textBaseline = 'middle'; g.fillText(this.suit, 1040, 394);
    txt(g, 'SUIT', 1040, 450, 16, '#fff', 'center', 'Nunito');
    if (this.pending) txt(g, `+${this.pending}`, 1040, 520, 54, '#ff922b');
    // seats
    this.ps.forEach((p, i) => {
      const s = this.seat(i), h = this.hands.get(p), cur = i === this.turn && !this.over;
      const fan = Math.min(h.length, 10);
      for (let k = 0; k < fan; k++) drawCard(g, s.x + (k - (fan - 1) / 2) * 14, s.y - 62 + Math.abs(k - (fan - 1) / 2) * 1.5, 44, 62, null, (k - (fan - 1) / 2) * 0.08);
      g.fillStyle = cur ? p.color : 'rgba(14,10,36,.7)'; rr(g, s.x - 120, s.y - 30, 240, 64, 18); g.fill();
      if (cur) { g.lineWidth = 5; g.strokeStyle = '#fff'; g.stroke(); }
      txt(g, `${p.avatar || ''} ${p.name}`, s.x, s.y - 6, 22, cur ? '#120e2b' : '#fff', 'center', 'Nunito');
      txt(g, `${h.length} card${h.length === 1 ? '' : 's'}`, s.x, s.y + 20, 15, cur ? '#120e2b' : '#b3acdf', 'center', 'Nunito');
      if (cur && !p.bot) {
        g.strokeStyle = this.timer < 8 ? '#ff4d6d' : '#ffd43b'; g.lineWidth = 6;
        g.beginPath(); g.arc(s.x + 140, s.y, 18, -Math.PI / 2, -Math.PI / 2 + TAU * Math.max(0, this.timer / 30)); g.stroke();
      }
      if (cur && this.needSuit) txt(g, 'picking suit…', s.x, s.y + 52, 18, '#ffd43b', 'center', 'Nunito');
    });
    if (this.chaosT) { g.fillStyle = `rgba(204,93,232,${this.chaosT * 0.3})`; g.fillRect(0, 0, 1600, 900); }
    this.fx.draw(g);
    txt(g, '8 wild · 2 = +2 · J skip · Q reverse · A = CHAOS', 800, 30, 20, 'rgba(255,255,255,.75)', 'center', 'Nunito');
  }
}

/* =====================================================================
   ⚡ REFLEX RUSH — tap on GO!, not on the fakes
   ===================================================================== */
const FAKES = ['NO!', 'GOAT', 'GO?', 'G0!', 'WAIT', 'OH!', 'SO!', 'DOH!', 'GOLF', 'NOPE'];
class Reflex {
  constructor(c) { this.c = c; this.round = 0; this.R = 6; this.pts = new Map(); this.fx = new Particles(); this.flash = 0; }
  start() {
    this.ps = this.c.roster(2);
    this.ps.forEach(p => this.pts.set(p, 0));
    this.c.setLayout('all', { name: 'button', label: 'TAP!', hint: 'Only on a green GO! Fakes cost a point.' });
    this.next();
  }
  next() {
    this.round++; this.phase = 'ready'; this.t = 2; this.word = null; this.taps = []; this.locked = new Set(); this.bad = new Set();
    this.waitT = rnd(2, 6); this.fakeT = rnd(0.7, 1.6); this.botAt = new Map();
    this.c.sfx.blip();
  }
  update(dt) {
    this.fx.update(dt); this.flash = Math.max(0, this.flash - dt * 2);
    this.t -= dt;
    if (this.phase === 'ready') { if (this.t <= 0) { this.phase = 'wait'; this.t = this.waitT; } }
    else if (this.phase === 'wait') {
      this.fakeT -= dt;
      if (this.word) { this.word.t -= dt; if (this.word.t <= 0) this.word = null; }
      if (this.fakeT <= 0 && this.t > 0.5) {
        this.word = { s: pick(FAKES), col: pick(['#51cf66', '#ff4d6d', '#ffd43b', '#4dabff', '#51cf66']), t: 0.6 }; this.fakeT = rnd(0.9, 1.8);
        this.c.sfx.blip();
        for (const b of this.ps) if (b.bot && Math.random() < 0.15) setTimeout(() => this.onButton(b, 'a'), rnd(200, 400));
      }
      if (this.t <= 0) {
        this.phase = 'go'; this.goAt = performance.now(); this.word = { s: 'GO!', col: '#51cf66', t: 99 }; this.t = 2.4; this.flash = 1;
        this.c.sfx.go();
        for (const b of this.ps) if (b.bot && !this.locked.has(b)) this.botAt.set(b, rnd(0.25, 0.65));
      }
    } else if (this.phase === 'go') {
      for (const [b, at] of this.botAt) { const left = at - dt; this.botAt.set(b, left); if (left <= 0) { this.botAt.delete(b); this.onButton(b, 'a'); } }
      if (this.t <= 0 || this.ps.every(p => this.locked.has(p))) { this.phase = 'show'; this.t = 2.4; this.word = null; }
    } else if (this.phase === 'show' && this.t <= 0) {
      if (this.round >= this.R) return this.end();
      this.next();
    }
  }
  onButton(p, b) {
    if (b !== 'a' || this.locked.has(p) || !this.pts.has(p)) return;
    if (this.phase === 'wait' || this.phase === 'ready') {
      this.locked.add(p); this.bad.add(p); this.pts.set(p, this.pts.get(p) - 1);
      this.c.sfx.buzz(); this.c.send(p, { t: 'buzz', ms: 400 }); this.c.shake(6);
    } else if (this.phase === 'go') {
      this.locked.add(p);
      const ms = Math.round(performance.now() - this.goAt);
      this.taps.push({ p, ms });
      const gain = Math.max(0, 4 - this.taps.length);
      this.pts.set(p, this.pts.get(p) + gain);
      this.c.sfx.ding(); this.c.send(p, { t: 'buzz', ms: 40 });
      if (this.taps.length === 1) this.fx.burst(800, 380, 40, p.color, 500, 0.8, 8);
    }
  }
  end() {
    if (this.done) return; this.done = true;
    this.c.finish([...this.ps].sort((a, b) => this.pts.get(b) - this.pts.get(a)).map(p => ({ p, stat: `${this.pts.get(p)} pts` })));
  }
  draw(g) {
    g.fillStyle = this.phase === 'go' ? '#0f3d1f' : '#140f30'; g.fillRect(0, 0, 1600, 900);
    if (this.flash) { g.fillStyle = `rgba(255,255,255,${this.flash * 0.7})`; g.fillRect(0, 0, 1600, 900); }
    txt(g, `ROUND ${Math.min(this.round, this.R)}/${this.R}`, 800, 50, 34, '#b3acdf');
    if (this.phase === 'ready') txt(g, 'GET READY…', 800, 360, 110, '#fff');
    else if (this.phase === 'wait') { if (this.word) txt(g, this.word.s, 800 + rnd(-4, 4), 360, 220, this.word.col); else txt(g, '…', 800, 360, 160, '#5c548f'); }
    else if (this.phase === 'go') txt(g, 'GO!', 800, 360, 260 + Math.sin(performance.now() / 60) * 8, '#51cf66');
    else if (this.phase === 'show') {
      const f = this.taps[0];
      txt(g, f ? `${f.p.name} ${f.ms}ms!` : 'NOBODY?!', 800, 360, 90, f ? f.p.color : '#ff4d6d');
    }
    this.fx.draw(g);
    const n = this.ps.length, w = Math.min(220, 1500 / n);
    this.ps.forEach((p, i) => {
      const x = 800 + (i - (n - 1) / 2) * w, y = 720;
      g.fillStyle = 'rgba(255,255,255,.07)'; rr(g, x - w / 2 + 8, y - 90, w - 16, 190, 20); g.fill();
      g.fillStyle = p.color; g.beginPath(); g.arc(x, y - 40, 30, 0, TAU); g.fill();
      txt(g, p.name, x, y + 12, 22, '#fff', 'center', 'Nunito');
      txt(g, String(this.pts.get(p)), x, y + 52, 34, '#ffd43b');
      const tap = this.taps.findIndex(t => t.p === p);
      if (this.bad.has(p)) txt(g, 'TOO EARLY ✗', x, y - 112, 20, '#ff4d6d', 'center', 'Nunito');
      else if (tap >= 0) txt(g, `#${tap + 1} · ${this.taps[tap].ms}ms`, x, y - 112, 20, '#51cf66', 'center', 'Nunito');
    });
  }
}

/* =====================================================================
   🚀 SHAKE RACE — shake the phone (or mash keys) to fly your rocket
   ===================================================================== */
class ShakeRace {
  constructor(c) { this.c = c; this.fx = new Particles(); this.fin = []; this.phase = 'count'; this.t = 3.5; this.lastTick = 4; this.el = 0; this.zapT = rnd(6, 9); this.bolt = null;
    this.stars = Array.from({ length: 120 }, () => ({ x: rnd(0, 1600), y: rnd(0, 900), s: rnd(0.5, 2.5) })); }
  start() {
    this.ps = this.c.roster(2);
    this.st = new Map(this.ps.map(p => [p, { prog: 0, heat: 0, last: 0, rate: rnd(5.5, 7.5) }]));
    this.c.setLayout('all', { name: 'shake', hint: 'Faster shake = faster rocket. Watch out for lightning!' });
    this.c.banner('🛷 SLED DASH!');
  }
  onPump(p) {
    const s = this.st && this.st.get(p);
    if (!s || this.phase !== 'race' || this.fin.includes(p)) return;
    const now = performance.now(); if (now - s.last < 70) return; s.last = now;
    this.add(p, 1.5);
  }
  add(p, v) {
    const s = this.st.get(p); s.prog = Math.min(100, s.prog + v); s.heat = Math.min(1, s.heat + 0.15);
    if (s.prog >= 100 && !this.fin.includes(p)) {
      this.fin.push(p); this.c.sfx.win(); this.fx.burst(1450, this.laneY(this.ps.indexOf(p)), 40, p.color, 400, 1, 8);
      if (this.fin.length === 1) { this.c.banner(`${p.name} WINS!`, 2000, p.color); this.endT = 7; this.fx.confetti(100); }
    }
  }
  laneY(i) { const n = this.ps.length; return 150 + ((i + 0.5) * 700) / n; }
  update(dt) {
    this.fx.update(dt);
    for (const s of this.stars) { s.x -= s.s * 60 * dt; if (s.x < 0) s.x = 1600; }
    if (this.bolt) { this.bolt.t -= dt; if (this.bolt.t <= 0) this.bolt = null; }
    if (this.phase === 'count') {
      this.t -= dt;
      if (Math.ceil(this.t) < this.lastTick && this.t > 0) { this.lastTick = Math.ceil(this.t); this.c.sfx.tick(); }
      if (this.t <= 0) { this.phase = 'race'; this.c.sfx.go(); this.c.banner('MUSH!!!', 900, '#4dabff'); }
      return;
    }
    this.el += dt;
    for (const [p, s] of this.st) {
      s.heat = Math.max(0, s.heat - dt * 0.8);
      if (p.bot && !this.fin.includes(p)) { s.botAcc = (s.botAcc || 0) + dt * s.rate * rnd(0.6, 1.4); while (s.botAcc >= 1) { s.botAcc--; this.add(p, 1.5); } }
      if (!this.fin.includes(p) && s.heat > 0.05 && Math.random() < 0.7) { const i = this.ps.indexOf(p); this.fx.puff(150 + s.prog * 13 - 40, this.laneY(i) + rnd(-6, 6), 'rgba(255,255,255,.9)', 6 + s.heat * 10, 0.5, -220, rnd(-30, 30)); }
    }
    // lightning strikes the leader
    this.zapT -= dt;
    if (this.zapT <= 0) {
      this.zapT = rnd(5, 8);
      const racing = this.ps.filter(p => !this.fin.includes(p));
      if (racing.length > 1) {
        const lead = racing.sort((a, b) => this.st.get(b).prog - this.st.get(a).prog)[0], s = this.st.get(lead);
        if (s.prog > 15) {
          s.prog = Math.max(0, s.prog - 9);
          const x = 150 + s.prog * 13, y = this.laneY(this.ps.indexOf(lead));
          this.bolt = { x, y, t: 0.35 }; this.fx.burst(x, y, 25, '#fff59d', 300, 0.6, 6);
          this.c.sfx.zap(); this.c.shake(10); this.c.send(lead, { t: 'buzz', ms: 300 });
          this.c.banner(`❄️ AVALANCHE on ${lead.name}!`, 1300, '#a5d8ff');
        }
      }
    }
    if (this.endT !== undefined) this.endT -= dt;
    if (this.fin.length === this.ps.length || (this.endT !== undefined && this.endT <= 0) || this.el > 60) this.end();
  }
  end() {
    if (this.done) return; this.done = true;
    const rest = this.ps.filter(p => !this.fin.includes(p)).sort((a, b) => this.st.get(b).prog - this.st.get(a).prog);
    this.c.finish([...this.fin, ...rest].map((p, i) => ({ p, stat: i < this.fin.length ? 'finished!' : `${Math.round(this.st.get(p).prog)}%` })));
  }
  draw(g) {
    const sky = g.createLinearGradient(0, 0, 0, 900); sky.addColorStop(0, '#9fd3ff'); sky.addColorStop(1, '#f1f8ff'); g.fillStyle = sky; g.fillRect(0, 0, 1600, 900);
    g.fillStyle = '#d6ecff'; g.beginPath(); g.moveTo(0, 170); for (let x = 0; x <= 1600; x += 100) g.lineTo(x, 130 + Math.sin(x * 0.013) * 40 - (x % 300 === 0 ? 50 : 0)); g.lineTo(1600, 900); g.lineTo(0, 900); g.fill();
    g.fillStyle = '#fff'; for (const s of this.stars) { g.globalAlpha = 0.8; g.beginPath(); g.arc(s.x, (s.y + performance.now() / 20 * s.s) % 900, s.s * 1.4, 0, TAU); g.fill(); } g.globalAlpha = 1;
    // finish line
    for (let y = 120; y < 860; y += 20) for (let k = 0; k < 2; k++) { g.fillStyle = ((y / 20 + k) % 2) ? '#fff' : '#111'; g.fillRect(1450 + k * 20, y, 20, 20); }
    this.ps.forEach((p, i) => {
      const s = this.st.get(p), y = this.laneY(i), x = 150 + s.prog * 13;
      g.strokeStyle = 'rgba(255,255,255,.08)'; g.lineWidth = 2; g.setLineDash([10, 14]); g.beginPath(); g.moveTo(100, y + 40); g.lineTo(1450, y + 40); g.stroke(); g.setLineDash([]);
      g.save(); g.translate(x + rnd(-1, 1) * s.heat * 4, y + rnd(-1, 1) * s.heat * 4);
      g.strokeStyle = '#7a4a1e'; g.lineWidth = 6; g.lineCap = 'round';
      g.beginPath(); g.moveTo(-50, 30); g.lineTo(46, 30); g.quadraticCurveTo(62, 30, 60, 14); g.stroke();
      g.fillStyle = '#c0703a'; rr(g, -46, 8, 86, 16, 6); g.fill();
      g.restore();
      drawBlob(g, x + 2, y - 12, 26, p.color, 0, { shadow: false, happy: s.heat > 0.3 });
      g.fillStyle = p.color; g.fillRect(x - 22, y - 36, 44, 8);
      txt(g, p.name, 90, y, 22, p.color, 'right', 'Nunito');
      const place = this.fin.indexOf(p);
      txt(g, place >= 0 ? `#${place + 1}` : `${Math.round(s.prog)}%`, 1530, y, 26, place >= 0 ? '#ffd43b' : '#fff');
    });
    if (this.bolt) {
      g.strokeStyle = '#fff59d'; g.lineWidth = 8; g.beginPath(); let bx = this.bolt.x + rnd(-30, 30); g.moveTo(bx, 0);
      for (let y = 0; y < this.bolt.y; y += 60) { bx = this.bolt.x + rnd(-40, 40); g.lineTo(bx, y); }
      g.lineTo(this.bolt.x, this.bolt.y); g.stroke();
    }
    this.fx.draw(g);
    txt(g, this.phase === 'count' ? 'GET READY TO SHAKE' : '🛷 SLED DASH', 800, 60, 40);
    if (this.phase === 'count') txt(g, String(Math.max(1, Math.ceil(this.t))), 800, 470, 200, '#ffd43b');
  }
}

