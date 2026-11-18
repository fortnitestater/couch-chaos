/* Couch Chaos — ❄️ Winter games. Needs engine.js */

function snowField(g, tint = '#eaf4ff') {
  const gr = g.createLinearGradient(0, 0, 0, 900); gr.addColorStop(0, '#dbeeff'); gr.addColorStop(1, tint);
  g.fillStyle = gr; g.fillRect(0, 0, 1600, 900);
}
class Snowfall {
  constructor(n = 90) { this.f = Array.from({ length: n }, () => ({ x: rnd(0, 1600), y: rnd(0, 900), r: rnd(1.5, 4.5), s: rnd(30, 90), w: rnd(0, 6) })); }
  update(dt) { for (const f of this.f) { f.y += f.s * dt; f.w += dt; f.x += Math.sin(f.w) * 12 * dt; if (f.y > 910) { f.y = -10; f.x = rnd(0, 1600); } } }
  draw(g, a = 0.85) { g.fillStyle = '#fff'; g.globalAlpha = a; for (const f of this.f) { g.beginPath(); g.arc(f.x, f.y, f.r, 0, TAU); g.fill(); } g.globalAlpha = 1; }
}

/* ---------------------------------------------------------------- ❄️ SNOWBALL FIGHT */
class SnowballFight extends ArenaGame {
  constructor(c) { super(c, { time: 75, a: 'THROW', b: 'DASH', hint: 'Move, then THROW where you face. Hide behind forts!', unit: 'hits', fric: 3.2, acc: 1400 }); this.balls = []; this.snow = new Snowfall(70); }
  setup() {
    const bx = this.box;
    this.forts = [];
    for (let i = 0; i < 6; i++) {
      let f, ok = false, tries = 0;
      while (!ok && tries++ < 40) { f = { x: rnd(bx.x + 180, bx.x + bx.w - 180), y: rnd(bx.y + 140, bx.y + bx.h - 140), r: rnd(45, 70) }; ok = this.forts.every(o => Math.hypot(o.x - f.x, o.y - f.y) > o.r + f.r + 90) && this.blobs.every(b => Math.hypot(b.x - f.x, b.y - f.y) > f.r + 80); }
      if (ok) this.forts.push(f);
    }
    for (const b of this.blobs) b.tcd = 0;
  }
  idle(dt) { this.snow.update(dt); }
  tick(dt) {
    this.snow.update(dt);
    for (const b of this.blobs) {
      if (!b.alive) continue;
      b.tcd -= dt;
      for (const f of this.forts) { const dx = b.x - f.x, dy = b.y - f.y, d = Math.hypot(dx, dy) || 1, min = f.r + b.r; if (d < min) { b.x = f.x + dx / d * min; b.y = f.y + dy / d * min; const vn = b.vx * dx / d + b.vy * dy / d; if (vn < 0) { b.vx -= vn * dx / d; b.vy -= vn * dy / d; } } }
      const inp = this.inputFor(b);
      if (Math.hypot(inp.x || 0, inp.y || 0) > 0.3) b.aim = Math.atan2(inp.y || 0, inp.x || 0);
      if (inp.a && b.tcd <= 0 && b.stun <= 0) {
        const a = b.aim ?? b.face; b.tcd = 0.45;
        this.balls.push({ x: b.x + Math.cos(a) * (b.r + 12), y: b.y + Math.sin(a) * (b.r + 12), vx: Math.cos(a) * 760 + b.vx * 0.3, vy: Math.sin(a) * 760 + b.vy * 0.3, life: 1.1, o: b });
        this.c.sfx.whoosh();
      }
    }
    for (const s of this.balls) {
      s.x += s.vx * dt; s.y += s.vy * dt; s.life -= dt;
      const bx = this.box;
      if (s.x < bx.x || s.x > bx.x + bx.w || s.y < bx.y || s.y > bx.y + bx.h) s.life = 0;
      for (const f of this.forts) if (s.life > 0 && Math.hypot(s.x - f.x, s.y - f.y) < f.r) { s.life = 0; this.fx.burst(s.x, s.y, 10, '#fff', 160, 0.5, 6); }
      for (const b of this.blobs) {
        if (s.life <= 0 || b === s.o || !b.alive || b.inv > 0) continue;
        if (Math.hypot(s.x - b.x, s.y - b.y) < b.r + 10) {
          s.life = 0; b.stun = 0.6; b.inv = 0.8; b.vx += s.vx * 0.5; b.vy += s.vy * 0.5;
          this.score.set(s.o.p, this.score.get(s.o.p) + 1);
          this.fx.burst(s.x, s.y, 22, '#fff', 260, 0.6, 8); this.fx.text(s.o.x, s.o.y - 50, '+1', s.o.p.color);
          this.c.sfx.bonk(); this.c.shake(5); this.c.send(b.p, { t: 'buzz', ms: 120 });
        }
      }
    }
    this.balls = this.balls.filter(s => s.life > 0);
  }
  botInput(b) {
    let t = null, bd = 1e9;
    for (const o of this.blobs) if (o !== b && o.alive) { const d = Math.hypot(o.x - b.x, o.y - b.y); if (d < bd) { bd = d; t = o; } }
    if (!t) return { x: 0, y: 0 };
    let dx = t.x - b.x, dy = t.y - b.y; const d = Math.hypot(dx, dy) || 1;
    const keep = d < 260 ? -1 : 1;
    return { x: dx / d * keep * 0.8 + Math.sin(this.el * 2 + b.x) * 0.3, y: dy / d * keep * 0.8 + Math.cos(this.el * 2 + b.y) * 0.3, a: d < 600 && Math.random() < 0.5, b: false, aimAt: Math.atan2(dy, dx) };
  }
  inputFor(b) {
    if (!b.alive) return { x: 0, y: 0 };
    if (!b.p.bot) return b.p.input;
    if (!b._bi || this.el - (b._bt || 0) > 0.15) { b._bi = this.botInput(b); b._bt = this.el; b.aim = b._bi.aimAt; }
    return b._bi;
  }
  drawArena(g) {
    snowField(g);
    const bx = this.box;
    g.fillStyle = '#f8fbff'; rr(g, bx.x, bx.y, bx.w, bx.h, 40); g.fill();
    g.lineWidth = 6; g.strokeStyle = '#a5d8ff'; g.stroke();
    // pine trees around the edge
    for (let i = 0; i < 14; i++) { const x = 30 + i * 120, y = 892; g.fillStyle = '#2b8a3e'; g.beginPath(); g.moveTo(x, y - 70); g.lineTo(x - 30, y); g.lineTo(x + 30, y); g.fill(); g.fillStyle = '#fff'; g.beginPath(); g.moveTo(x, y - 70); g.lineTo(x - 12, y - 42); g.lineTo(x + 12, y - 42); g.fill(); }
  }
  drawUnder(g) {
    for (const f of this.forts) {
      g.fillStyle = 'rgba(80,120,170,.25)'; g.beginPath(); g.ellipse(f.x + 6, f.y + f.r * 0.5, f.r * 1.05, f.r * 0.55, 0, 0, TAU); g.fill();
      g.fillStyle = '#ffffff'; g.beginPath(); g.arc(f.x, f.y, f.r, 0, TAU); g.fill(); g.lineWidth = 4; g.strokeStyle = '#a5d8ff'; g.stroke();
      g.fillStyle = '#e7f5ff'; g.beginPath(); g.arc(f.x - f.r * 0.25, f.y - f.r * 0.25, f.r * 0.45, 0, TAU); g.fill();
    }
  }
  blobStyle(b) { return { hat: b.p.color === '#ff4d6d' ? '#4dabff' : '#ff4d6d' }; }
  drawOver(g) {
    for (const s of this.balls) { g.fillStyle = 'rgba(0,40,90,.2)'; g.beginPath(); g.arc(s.x + 4, s.y + 8, 9, 0, TAU); g.fill(); g.fillStyle = '#fff'; g.beginPath(); g.arc(s.x, s.y, 10, 0, TAU); g.fill(); g.lineWidth = 2; g.strokeStyle = '#a5d8ff'; g.stroke(); }
    for (const b of this.blobs) if (b.alive && b.aim != null) { const a = b.aim; g.strokeStyle = b.p.color; g.lineWidth = 4; g.globalAlpha = 0.6; g.beginPath(); g.moveTo(b.x + Math.cos(a) * (b.r + 6), b.y + Math.sin(a) * (b.r + 6)); g.lineTo(b.x + Math.cos(a) * (b.r + 26), b.y + Math.sin(a) * (b.r + 26)); g.stroke(); g.globalAlpha = 1; }
    this.snow.draw(g, 0.7);
  }
}

/* ---------------------------------------------------------------- ⛷️ SKI SLALOM */
class SkiSlalom {
  constructor(c) { this.c = c; this.fx = new Particles(); this.cd = new Countdown(c); this.T = 60; this.obs = []; this.dist = 0; this.nextObs = 300; this.snow = new Snowfall(60); }
  start() {
    this.ps = this.c.roster(2);
    const n = this.ps.length;
    this.sk = this.ps.map((p, i) => ({ p, x: 800 + (i - (n - 1) / 2) * 90, vx: 0, crash: 0, gates: 0, crashes: 0, lastGate: -1, trail: [] }));
    this.c.setLayout('all', { name: 'pad', stick: 'x', a: null, b: 'TUCK', hint: 'Steer through the flag gates. Hold TUCK to go faster!' });
    this.speed = 330;
  }
  update(dt) {
    this.fx.update(dt); this.snow.update(dt);
    if (!this.cd.update(dt)) return;
    if (this.over) { this.overT -= dt; if (this.overT <= 0) this.end(); return; }
    this.T -= dt; if (this.T <= 0) { this.over = true; this.overT = 2; this.c.sfx.win(); this.fx.confetti(80); return; }
    this.speed = 330 + (60 - this.T) * 4;
    const tuckAny = this.sk.some(s => (s.p.bot ? false : s.p.input.b));
    const dy = this.speed * dt * (tuckAny ? 1 : 1);
    this.dist += dy;
    while (this.dist + 1000 > this.nextObs) {
      const y = this.nextObs;
      if (Math.random() < 0.55) { const gx = rnd(330, 1270), w = rnd(170, 240); this.obs.push({ type: 'gate', x: gx, w, y, id: y, color: Math.random() < 0.5 ? '#ff4d6d' : '#4dabff' }); }
      else for (let k = 0; k < irnd(1, 3); k++) this.obs.push({ type: 'tree', x: rnd(180, 1420), y: y + rnd(-40, 40), r: rnd(26, 40) });
      if (Math.random() < 0.25) this.obs.push({ type: 'jump', x: rnd(300, 1300), y: y + 120 });
      this.nextObs += rnd(220, 300) * (1 - Math.min(0.35, (60 - this.T) / 170));
    }
    const SY = 230; // skier screen y
    for (const s of this.sk) {
      const inp = s.p.bot ? this.bot(s) : s.p.input;
      if (s.crash > 0) { s.crash -= dt; s.vx *= 0.9; continue; }
      s.vx += ((inp.x || 0) * 620 - s.vx) * Math.min(1, dt * 5);
      s.x = clamp(s.x + s.vx * dt, 140, 1460);
      s.tuck = !!inp.b;
      if (Math.random() < 0.5) this.fx.puff(s.x + rnd(-8, 8), SY + 18, 'rgba(255,255,255,.9)', 5, 0.5, -s.vx * 0.2, -this.speed * 0.3);
      for (const o of this.obs) {
        const oy = o.y - this.dist;
        if (o.type === 'tree' && !o.hit && Math.abs(oy - SY) < o.r && Math.abs(o.x - s.x) < o.r * 0.7 + 14) {
          o.hit = true; s.crash = 1.1; s.crashes++; this.fx.burst(s.x, SY, 25, '#fff', 260, 0.8, 7); this.fx.burst(o.x, oy, 10, '#2b8a3e', 200, 0.6, 5);
          this.c.sfx.bonk(); this.c.shake(6); this.c.send(s.p, { t: 'buzz', ms: 250 }); this.fx.text(s.x, SY - 50, 'OUCH!', '#ff4d6d');
        }
        if (o.type === 'gate' && s.lastGate !== o.id && oy < SY && oy > SY - this.speed * dt * 2 - 4) {
          s.lastGate = o.id;
          if (Math.abs(s.x - o.x) < o.w / 2) { s.gates++; this.fx.text(s.x, SY - 50, '+1', s.p.color); this.c.sfx.ding(); this.fx.burst(s.x, SY, 12, o.color, 180, 0.5, 5); }
        }
      }
      s.trail.push({ x: s.x, y: this.dist + SY }); if (s.trail.length > 60) s.trail.shift();
    }
    this.obs = this.obs.filter(o => o.y - this.dist > -120);
  }
  bot(s) {
    const SY = 230;
    let tgt = null;
    for (const o of this.obs) { const oy = o.y - this.dist; if (o.type === 'gate' && oy > SY + 10) { if (!tgt || oy < tgt.y - this.dist) tgt = o; } }
    let want = tgt ? tgt.x + (s.x < tgt.x ? -20 : 20) : 800;
    for (const o of this.obs) { const oy = o.y - this.dist; if (o.type === 'tree' && oy > SY && oy < SY + 160 && Math.abs(o.x - s.x) < 70) want = s.x + (s.x < o.x ? -150 : 150); }
    return { x: clamp((want - s.x) / 120) * 0.9, b: false };
  }
  score(s) { return s.gates * 10 - s.crashes * 5; }
  end() { if (this.done) return; this.done = true; this.c.finish(rankBy(this.ps, p => this.score(this.sk.find(s => s.p === p)), p => { const s = this.sk.find(x => x.p === p); return `${s.gates} gates · ${s.crashes} crashes`; })); }
  draw(g) {
    snowField(g, '#f4f9ff');
    // grooming lines
    g.strokeStyle = 'rgba(165,216,255,.35)'; g.lineWidth = 2;
    for (let i = 0; i < 18; i++) { const y = ((i * 60 - this.dist) % 1080 + 1080) % 1080 - 90; g.beginPath(); g.moveTo(0, y); g.lineTo(1600, y + 40); g.stroke(); }
    for (const s of this.sk) {
      g.strokeStyle = 'rgba(120,160,210,.35)'; g.lineWidth = 3; g.beginPath();
      s.trail.forEach((t, i) => { const y = t.y - this.dist; i ? g.lineTo(t.x, y) : g.moveTo(t.x, y); }); g.stroke();
    }
    const items = this.obs.slice().sort((a, b) => a.y - b.y);
    for (const o of items) {
      const y = o.y - this.dist;
      if (o.type === 'gate') {
        for (const sx of [-1, 1]) { const x = o.x + sx * o.w / 2; g.strokeStyle = '#555'; g.lineWidth = 4; g.beginPath(); g.moveTo(x, y + 10); g.lineTo(x, y - 50); g.stroke(); g.fillStyle = o.color; g.beginPath(); g.moveTo(x, y - 50); g.lineTo(x + 34 * -sx, y - 40); g.lineTo(x, y - 28); g.fill(); }
        g.strokeStyle = 'rgba(0,0,0,.08)'; g.setLineDash([8, 10]); g.beginPath(); g.moveTo(o.x - o.w / 2, y + 6); g.lineTo(o.x + o.w / 2, y + 6); g.stroke(); g.setLineDash([]);
      } else if (o.type === 'tree') {
        g.fillStyle = 'rgba(60,90,130,.18)'; g.beginPath(); g.ellipse(o.x + 10, y + o.r * 0.6, o.r, o.r * 0.35, 0, 0, TAU); g.fill();
        g.fillStyle = '#6b4226'; g.fillRect(o.x - 5, y, 10, o.r * 0.6);
        for (let k = 0; k < 3; k++) { const ty = y - k * o.r * 0.6, w = o.r * (1 - k * 0.25); g.fillStyle = '#2f9e44'; g.beginPath(); g.moveTo(o.x, ty - o.r * 1.1); g.lineTo(o.x - w, ty + 6); g.lineTo(o.x + w, ty + 6); g.fill(); g.fillStyle = '#fff'; g.beginPath(); g.moveTo(o.x, ty - o.r * 1.1); g.lineTo(o.x - w * 0.35, ty - o.r * 0.6); g.lineTo(o.x + w * 0.35, ty - o.r * 0.6); g.fill(); }
      } else if (o.type === 'jump') { g.fillStyle = '#d0ebff'; g.beginPath(); g.ellipse(o.x, y, 60, 18, 0, 0, TAU); g.fill(); }
    }
    const SY = 230;
    for (const s of this.sk) {
      const lean = clamp(s.vx / 600) * 0.5;
      g.save(); g.translate(s.x, SY + 22); g.rotate(lean); g.strokeStyle = '#212529'; g.lineWidth = 5; g.beginPath(); g.moveTo(-14, -26); g.lineTo(-14, 26); g.moveTo(14, -26); g.lineTo(14, 26); g.stroke(); g.restore();
      drawBlob(g, s.x, SY, s.tuck ? 20 : 24, s.p.color, Math.PI / 2 + lean, { name: s.p.name, hat: '#fff', dead: s.crash > 0, alpha: s.crash > 0 ? 0.7 : 1 });
    }
    this.fx.draw(g); this.snow.draw(g, 0.6);
    strip(g, rankBy(this.ps, p => this.score(this.sk.find(s => s.p === p)), () => '').map(r => ({ name: r.p.name, color: r.p.color, val: this.sk.find(s => s.p === r.p).gates })));
    clock(g, this.T); this.cd.draw(g);
  }
}

/* ---------------------------------------------------------------- 🏒 ICE HOCKEY */
class IceHockey extends ArenaGame {
  constructor(c) { super(c, { time: 120, b: 'SHOOT', hint: 'Bump the puck into the other goal. SHOOT = power dash!', unit: 'goals', fric: 1.6, acc: 1200, dash: 650, r: 30 }); }
  setup() {
    this.teams = [[], []]; this.blobs.forEach((b, i) => { b.team = i % 2; this.teams[b.team].push(b); });
    this.goals = [0, 0]; this.indiv = new Map(this.ps.map(p => [p, 0]));
    this.puck = { x: 800, y: 485, vx: 0, vy: 0, r: 16 }; this.resetPos(); this.TC = ['#ff4d6d', '#4dabff'];
  }
  resetPos() {
    const bx = this.box, cy = bx.y + bx.h / 2;
    for (const t of [0, 1]) this.teams[t].forEach((b, k, arr) => { b.x = t === 0 ? bx.x + bx.w * 0.3 : bx.x + bx.w * 0.7; b.y = cy + (k - (arr.length - 1) / 2) * 120; b.vx = b.vy = 0; });
    Object.assign(this.puck, { x: bx.x + bx.w / 2, y: cy, vx: 0, vy: 0 }); this.pause = 1.2;
  }
  tick(dt) {
    const pk = this.puck, bx = this.box, cy = bx.y + bx.h / 2, gh = 110;
    if (this.pause > 0) { this.pause -= dt; return; }
    const k = Math.exp(-0.45 * dt); pk.vx *= k; pk.vy *= k;
    const sp = Math.hypot(pk.vx, pk.vy); if (sp > 1300) { pk.vx *= 1300 / sp; pk.vy *= 1300 / sp; }
    pk.x += pk.vx * dt; pk.y += pk.vy * dt;
    if (pk.y < bx.y + pk.r) { pk.y = bx.y + pk.r; pk.vy = Math.abs(pk.vy) * 0.9; }
    if (pk.y > bx.y + bx.h - pk.r) { pk.y = bx.y + bx.h - pk.r; pk.vy = -Math.abs(pk.vy) * 0.9; }
    const inGoal = Math.abs(pk.y - cy) < gh;
    if (pk.x < bx.x + pk.r && !inGoal) { pk.x = bx.x + pk.r; pk.vx = Math.abs(pk.vx) * 0.9; }
    if (pk.x > bx.x + bx.w - pk.r && !inGoal) { pk.x = bx.x + bx.w - pk.r; pk.vx = -Math.abs(pk.vx) * 0.9; }
    for (const b of this.blobs) {
      const dx = pk.x - b.x, dy = pk.y - b.y, d = Math.hypot(dx, dy), min = b.r + pk.r;
      if (d < min && d > 0.01) {
        const nx = dx / d, ny = dy / d; pk.x = b.x + nx * min; pk.y = b.y + ny * min;
        const rv = (pk.vx - b.vx) * nx + (pk.vy - b.vy) * ny;
        if (rv < 0) { const boost = b.dashT > 0 ? 1.9 : 1.25; pk.vx -= rv * nx * boost; pk.vy -= rv * ny * boost; pk.vx += b.vx * 0.3; pk.vy += b.vy * 0.3; this.c.sfx.tink(); }
        this.last = b;
      }
    }
    if (pk.x < bx.x - 10 || pk.x > bx.x + bx.w + 10) {
      const team = pk.x < bx.x ? 1 : 0; this.goals[team]++;
      if (this.last && this.last.team === team) this.indiv.set(this.last.p, this.indiv.get(this.last.p) + 1);
      for (const b of this.blobs) this.score.set(b.p, this.goals[b.team]);
      this.c.banner(`GOAL! ${team ? 'BLUE' : 'RED'}${this.last && this.last.team === team ? ' · ' + this.last.p.name : ''}`, 1800, this.TC[team]);
      this.fx.burst(pk.x, pk.y, 60, this.TC[team], 450, 1.1, 9); this.c.sfx.win(); this.c.shake(12);
      this.resetPos();
    }
  }
  botTarget(b) {
    const pk = this.puck, bx = this.box, gx = b.team === 0 ? bx.x + bx.w : bx.x, gy = bx.y + bx.h / 2;
    const dx = pk.x - gx, dy = pk.y - gy, d = Math.hypot(dx, dy) || 1;
    return { x: pk.x + dx / d * 40, y: pk.y + dy / d * 40, dash: true, dashRange: 90 };
  }
  rank() {
    const win = this.goals[0] === this.goals[1] ? -1 : this.goals[0] > this.goals[1] ? 0 : 1;
    return [...this.blobs].sort((a, b) => ((b.team === win) - (a.team === win)) || this.indiv.get(b.p) - this.indiv.get(a.p))
      .map(b => ({ p: b.p, stat: `${b.team ? 'Blue' : 'Red'} ${this.goals[b.team]}-${this.goals[1 - b.team]} · ${this.indiv.get(b.p)} goals` }));
  }
  drawArena(g) {
    bgGrad(g, '#24476b', '#0a1a2f');
    const bx = this.box, cy = bx.y + bx.h / 2;
    g.fillStyle = '#eef7ff'; rr(g, bx.x, bx.y, bx.w, bx.h, 90); g.fill();
    g.lineWidth = 10; g.strokeStyle = '#ffd43b'; g.stroke();
    g.strokeStyle = '#ff6b6b'; g.lineWidth = 6; g.beginPath(); g.moveTo(800, bx.y); g.lineTo(800, bx.y + bx.h); g.stroke();
    g.strokeStyle = '#4dabff'; g.lineWidth = 5; for (const x of [bx.x + bx.w * 0.33, bx.x + bx.w * 0.67]) { g.beginPath(); g.moveTo(x, bx.y); g.lineTo(x, bx.y + bx.h); g.stroke(); }
    g.strokeStyle = '#ff6b6b'; g.lineWidth = 4; g.beginPath(); g.arc(800, cy, 90, 0, TAU); g.stroke();
    for (const [x, t] of [[bx.x, 0], [bx.x + bx.w, 1]]) { g.fillStyle = 'rgba(77,171,255,.25)'; g.beginPath(); g.arc(x, cy, 110, t ? Math.PI / 2 : -Math.PI / 2, t ? Math.PI * 1.5 : Math.PI / 2); g.fill(); g.fillStyle = '#495057'; g.fillRect(t ? x : x - 26, cy - 110, 26, 220); }
  }
  blobStyle(b) { return { ring: this.TC[b.team] }; }
  drawOver(g) {
    const pk = this.puck; g.fillStyle = 'rgba(0,0,0,.25)'; g.beginPath(); g.ellipse(pk.x + 3, pk.y + 6, pk.r, pk.r * 0.5, 0, 0, TAU); g.fill();
    g.fillStyle = '#111'; g.beginPath(); g.arc(pk.x, pk.y, pk.r, 0, TAU); g.fill(); g.strokeStyle = '#555'; g.lineWidth = 3; g.stroke();
  }
  drawHUD(g) {
    g.fillStyle = 'rgba(14,10,36,.75)'; rr(g, 560, 18, 480, 60, 20); g.fill();
    txt(g, String(this.goals[0]), 700, 49, 40, '#ff4d6d'); txt(g, 'RED  ·  BLUE', 800, 49, 18, '#fff', 'center', 'Nunito'); txt(g, String(this.goals[1]), 900, 49, 40, '#4dabff');
    clock(g, this.T);
  }
}

/* ---------------------------------------------------------------- 🥌 SNOW CURLING (turn-based, no motion needed) */
class SnowCurling {
  constructor(c) { this.c = c; this.fx = new Particles(); this.stones = []; this.turnI = -1; this.phase = 'intro'; this.t = 1.5; this.snow = new Snowfall(40); }
  start() {
    this.ps = this.c.roster(2);
    this.order = []; for (let k = 0; k < 2; k++) for (const p of this.ps) this.order.push(p);
    this.house = { x: 1300, y: 480 };
    this.c.setLayout('all', { name: 'msg', emoji: '🥌', text: 'Snow Curling', sub: 'Wait for your turn — slide closest to the target!' });
  }
  get cur() { return this.order[this.turnI]; }
  nextTurn() {
    this.turnI++;
    if (this.turnI >= this.order.length) { this.phase = 'done'; this.t = 3; this.scoreUp(); return; }
    this.phase = 'aim'; this.aim = 0; this.power = 0; this.t = 8; this.sweep = 0;
    const p = this.cur;
    this.c.banner(`${p.name}'s throw`, 1200, p.color);
    for (const q of this.ps) if (!q.bot) this.c.setLayout(q, q === p ? { name: 'button', label: 'LOCK AIM', hint: 'Tap when the arrow points where you want' } : { name: 'msg', emoji: '👀', text: `${p.name} is throwing`, sub: 'Get ready to cheer!' });
    this.botAt = rnd(0.8, 2.2);
  }
  onButton(p, b) {
    if (b !== 'a' || p !== this.cur) return;
    if (this.phase === 'aim') { this.phase = 'power'; this.t = 8; this.c.sfx.blip(); if (!p.bot) this.c.setLayout(p, { name: 'button', label: 'THROW!', hint: 'Tap at the right power' }); this.botAt = rnd(0.6, 2); }
    else if (this.phase === 'power') this.throwStone();
    else if (this.phase === 'slide' && this.stone) { this.sweep = Math.min(1, this.sweep + 0.12); }
  }
  aimVal() { return Math.sin(this.osc * 2.1) * 0.22; }
  powVal() { return 0.5 - Math.cos(this.osc2 * 2.6) * 0.5; }
  throwStone() {
    const a = this.aimVal(), pw = this.powVal();
    const sp = 560 + pw * 900;
    this.stone = { p: this.cur, x: 170, y: 480, vx: Math.cos(a) * sp, vy: Math.sin(a) * sp, r: 26, spin: 0 };
    this.stones.push(this.stone); this.phase = 'slide'; this.c.sfx.whoosh();
    if (!this.cur.bot) this.c.setLayout(this.cur, { name: 'button', label: 'SWEEP!', hint: 'Tap fast to make it slide further' });
  }
  update(dt) {
    this.fx.update(dt); this.snow.update(dt);
    this.osc = (this.osc || 0) + dt; this.osc2 = (this.osc2 || 0) + dt;
    if (this.phase === 'intro') { this.t -= dt; if (this.t <= 0) this.nextTurn(); return; }
    if (this.phase === 'done') { this.t -= dt; if (this.t <= 0) this.end(); return; }
    const p = this.cur;
    if (this.phase === 'aim' || this.phase === 'power') {
      this.t -= dt;
      if (p.bot) { this.botAt -= dt; if (this.botAt <= 0) this.onButton(p, 'a'); }
      if (this.t <= 0) this.onButton(p, 'a');
      if (this.phase === 'aim') this.osc2 = 0;
    }
    // physics for all stones
    let moving = false;
    for (const s of this.stones) {
      const sp = Math.hypot(s.vx, s.vy);
      if (sp > 4) {
        moving = true;
        const fr = (s === this.stone ? 260 - this.sweep * 150 : 260) * dt;
        const ns = Math.max(0, sp - fr); s.vx *= ns / sp; s.vy *= ns / sp;
        s.x += s.vx * dt; s.y += s.vy * dt; s.spin += sp * dt * 0.01;
        if (s.y < 190 + s.r || s.y > 770 - s.r) { s.vy = -s.vy * 0.6; s.y = clamp(s.y, 190 + s.r, 770 - s.r); }
        if (s.x > 1560) s.out = true;
      } else { s.vx = s.vy = 0; }
    }
    if (this.stone && this.sweep > 0) { this.sweep = Math.max(0, this.sweep - dt * 0.8); if (Math.random() < 0.5) this.fx.puff(this.stone.x + 40, this.stone.y + rnd(-20, 20), '#fff', 6, 0.4); }
    if (p && p.bot && this.phase === 'slide' && Math.random() < dt * 4) this.onButton(p, 'a');
    for (let i = 0; i < this.stones.length; i++) for (let j = i + 1; j < this.stones.length; j++) {
      const a = this.stones[i], b = this.stones[j]; if (a.out || b.out) continue;
      const dx = b.x - a.x, dy = b.y - a.y, d = Math.hypot(dx, dy), min = a.r + b.r;
      if (d < min && d > 0.01) {
        const nx = dx / d, ny = dy / d, ov = (min - d) / 2; a.x -= nx * ov; a.y -= ny * ov; b.x += nx * ov; b.y += ny * ov;
        const rv = (b.vx - a.vx) * nx + (b.vy - a.vy) * ny;
        if (rv < 0) { const jj = -rv * 0.95; a.vx -= jj * nx; a.vy -= jj * ny; b.vx += jj * nx; b.vy += jj * ny; this.c.sfx.bonk(); this.fx.burst(a.x + nx * a.r, a.y + ny * a.r, 8, '#fff', 150, 0.4, 4); }
      }
    }
    this.stones = this.stones.filter(s => !s.out);
    if (this.phase === 'slide' && !moving) { this.phase = 'wait'; this.t = 1.2; }
    if (this.phase === 'wait') { this.t -= dt; if (this.t <= 0) this.nextTurn(); }
  }
  dist(s) { return Math.hypot(s.x - this.house.x, s.y - this.house.y); }
  best(p) { const ds = this.stones.filter(s => s.p === p).map(s => this.dist(s)); return ds.length ? Math.min(...ds) : 9999; }
  scoreUp() { const r = rankBy(this.ps, p => -this.best(p), () => ''); this.c.banner(`${r[0].p.name} IS CLOSEST!`, 2400, r[0].p.color); this.c.sfx.win(); this.fx.confetti(90); }
  end() { if (this.done) return; this.done = true; this.c.finish(rankBy(this.ps, p => -this.best(p), p => this.best(p) > 9000 ? 'no stone left' : `${Math.round(this.best(p) / 3)} cm away`)); }
  draw(g) {
    bgGrad(g, '#1c3b5e', '#091526');
    g.fillStyle = '#f1f8ff'; rr(g, 120, 190, 1440, 580, 30); g.fill(); g.lineWidth = 6; g.strokeStyle = '#a5d8ff'; g.stroke();
    const h = this.house;
    for (const [r, c] of [[170, '#4dabff'], [120, '#fff'], [75, '#ff6b6b'], [28, '#fff']]) { g.fillStyle = c; g.beginPath(); g.arc(h.x, h.y, r, 0, TAU); g.fill(); }
    g.strokeStyle = 'rgba(0,0,0,.12)'; g.lineWidth = 2; g.beginPath(); g.moveTo(120, 480); g.lineTo(1560, 480); g.stroke();
    g.beginPath(); g.moveTo(h.x, 190); g.lineTo(h.x, 770); g.stroke();
    if (this.phase === 'aim' || this.phase === 'power') {
      const a = this.aimVal(); g.strokeStyle = this.cur.color; g.lineWidth = 6; g.setLineDash([16, 12]);
      g.beginPath(); g.moveTo(170, 480); g.lineTo(170 + Math.cos(a) * 700, 480 + Math.sin(a) * 700); g.stroke(); g.setLineDash([]);
      if (this.phase === 'power') { const pw = this.powVal(); g.fillStyle = 'rgba(14,10,36,.75)'; rr(g, 160, 800, 520, 40, 14); g.fill(); const gr = g.createLinearGradient(170, 0, 670, 0); gr.addColorStop(0, '#51cf66'); gr.addColorStop(0.6, '#ffd43b'); gr.addColorStop(1, '#ff4d6d'); g.fillStyle = gr; rr(g, 170, 808, 500 * pw, 24, 10); g.fill(); txt(g, 'POWER', 760, 820, 22); }
      drawBlob(g, 110, 480, 30, this.cur.color, 0, { hat: '#fff', name: this.cur.name });
    }
    for (const s of this.stones) {
      g.fillStyle = 'rgba(0,0,0,.25)'; g.beginPath(); g.ellipse(s.x + 4, s.y + 8, s.r, s.r * 0.6, 0, 0, TAU); g.fill();
      g.fillStyle = '#868e96'; g.beginPath(); g.arc(s.x, s.y, s.r, 0, TAU); g.fill();
      g.fillStyle = s.p.color; g.beginPath(); g.arc(s.x, s.y, s.r * 0.65, 0, TAU); g.fill();
      g.save(); g.translate(s.x, s.y); g.rotate(s.spin); g.fillStyle = '#212529'; rr(g, -s.r * 0.5, -5, s.r, 10, 5); g.fill(); g.restore();
    }
    this.fx.draw(g); this.snow.draw(g, 0.5);
    const rows = rankBy(this.ps, p => -this.best(p), () => '').map(r => ({ name: r.p.name, color: r.p.color, val: this.best(r.p) > 9000 ? '-' : Math.round(this.best(r.p) / 3), hi: r.p === this.cur }));
    strip(g, rows);
    const left = this.order.length - Math.max(0, this.turnI);
    txt(g, this.phase === 'done' ? 'FINAL!' : `${left} throws left · cm from the target`, 800, 120, 22, '#b3acdf', 'center', 'Nunito');
    if (this.phase === 'slide' && this.sweep > 0.05) txt(g, 'SWEEP! SWEEP!', this.stone.x, this.stone.y - 60, 26, '#fff');
  }
}

/* ---------------------------------------------------------------- 🧊 FROZEN LAKE (ice cracks, don't fall in!) */
class FrozenLake extends ArenaGame {
  constructor(c) { super(c, { time: 999, b: 'DASH', hint: 'Cracking ice turns red — get off it before it breaks!', clock: false, r: 28, fric: 2.4 }); }
  setup() {
    this.cols = 10; this.rows = 6; const bx = this.box; this.tw = bx.w / this.cols; this.th = bx.h / this.rows;
    this.tiles = Array.from({ length: this.cols * this.rows }, () => ({ s: 'ok', t: 0 }));
    this.waveT = 2.5; this.snow = new Snowfall(50);
  }
  tileAt(x, y) { const bx = this.box, c = Math.floor((x - bx.x) / this.tw), r = Math.floor((y - bx.y) / this.th); if (c < 0 || r < 0 || c >= this.cols || r >= this.rows) return null; return this.tiles[r * this.cols + c]; }
  idle(dt) { if (this.snow) this.snow.update(dt); }
  tick(dt) {
    this.snow.update(dt);
    this.waveT -= dt;
    if (this.waveT <= 0) {
      this.waveT = Math.max(1.1, 2.8 - this.el / 25);
      const ok = this.tiles.filter(t => t.s === 'ok'), n = Math.min(ok.length - 3, Math.floor(3 + this.el / 7));
      shuffle(ok).slice(0, Math.max(0, n)).forEach(t => { t.s = 'warn'; t.t = 1.4; });
      this.c.sfx.tick();
    }
    for (const t of this.tiles) {
      if (t.s === 'warn') { t.t -= dt; if (t.t <= 0) { t.s = 'gone'; t.t = Math.max(3, 7 - this.el / 15); t.crack = true; } }
      else if (t.s === 'gone') { t.t -= dt; if (t.t <= 0) t.s = 'ok'; }
    }
    if (this.tiles.some(t => t.crack)) { this.c.sfx.splash(); this.tiles.forEach(t => t.crack = false); }
    for (const b of this.blobs) {
      if (!b.alive) continue;
      const t = this.tileAt(b.x, b.y);
      if (t && t.s === 'gone') this.eliminate(b, 'SPLASH!');
      else this.score.set(b.p, this.el);
    }
  }
  isOver() { return this.blobs.filter(b => b.alive).length <= (this.ps.length > 1 ? 1 : 0); }
  rank() { return survivorRank(this.ps, p => this.blobOf(p).alive, this.out, p => `${Math.round(this.score.get(p))}s on the ice`); }
  botTarget(b) {
    const here = this.tileAt(b.x, b.y);
    if (here && here.s === 'ok' && !b.goal) return null;
    let best = null, bd = 1e9; const bx = this.box;
    this.tiles.forEach((t, i) => { if (t.s !== 'ok') return; const x = bx.x + (i % this.cols + 0.5) * this.tw, y = bx.y + (((i / this.cols) | 0) + 0.5) * this.th, d = Math.hypot(x - b.x, y - b.y); if (d < bd) { bd = d; best = { x, y, stop: true }; } });
    return best;
  }
  drawArena(g) {
    const gr = g.createLinearGradient(0, 0, 0, 900); gr.addColorStop(0, '#0b4f8a'); gr.addColorStop(1, '#03203d'); g.fillStyle = gr; g.fillRect(0, 0, 1600, 900);
    const tt = performance.now() / 1000;
    g.strokeStyle = 'rgba(160,220,255,.18)'; g.lineWidth = 3; for (let i = 0; i < 12; i++) { g.beginPath(); for (let x = 0; x <= 1600; x += 40) g.lineTo(x, 100 + i * 70 + Math.sin(x / 90 + tt * 2 + i) * 8); g.stroke(); }
  }
  drawUnder(g) {
    const bx = this.box, tt = performance.now() / 1000;
    this.tiles.forEach((t, i) => {
      if (t.s === 'gone') return;
      const x = bx.x + (i % this.cols) * this.tw, y = bx.y + ((i / this.cols) | 0) * this.th;
      const sh = t.s === 'warn' ? Math.sin(tt * 50) * 2 : 0;
      g.fillStyle = 'rgba(0,30,60,.35)'; rr(g, x + 6, y + 10, this.tw - 6, this.th - 6, 14); g.fill();
      g.fillStyle = t.s === 'warn' ? (Math.sin(tt * 18) > 0 ? '#ffc9c9' : '#e7f5ff') : '#e7f5ff'; rr(g, x + 3 + sh, y + 3, this.tw - 6, this.th - 6, 14); g.fill();
      g.fillStyle = 'rgba(255,255,255,.7)'; rr(g, x + 12 + sh, y + 10, this.tw * 0.4, 8, 4); g.fill();
      if (t.s === 'warn') { g.strokeStyle = '#c92a2a'; g.lineWidth = 3; g.beginPath(); g.moveTo(x + this.tw * 0.2, y + this.th * 0.3); g.lineTo(x + this.tw * 0.5, y + this.th * 0.55); g.lineTo(x + this.tw * 0.4, y + this.th * 0.8); g.moveTo(x + this.tw * 0.5, y + this.th * 0.55); g.lineTo(x + this.tw * 0.8, y + this.th * 0.45); g.stroke(); }
    });
  }
  blobStyle() { return { hat: '#ff922b' }; }
  drawOver(g) { this.snow.draw(g, 0.6); }
  drawHUD(g) { strip(g, this.ps.map(p => ({ name: p.name, color: p.color, val: this.blobOf(p).alive ? '❄' : '💦', dim: !this.blobOf(p).alive }))); txt(g, `${Math.round(this.el)}s`, 1530, 46, 30); }
}
