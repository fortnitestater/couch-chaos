/* Couch Chaos — shared game engine: helpers, particles, characters, HUD, base classes.
   Games get a ctx from tv.js: ctx.roster(min), ctx.setLayout(player|'all', layout), ctx.send(p,msg),
   ctx.banner(text,ms,color), ctx.sfx, ctx.shake(px), ctx.finish([{p, stat}...] best first) */

const TAU = Math.PI * 2;
const rnd = (a, b) => a + Math.random() * (b - a);
const irnd = (a, b) => Math.floor(rnd(a, b + 1));
const clamp = (v, a = -1, b = 1) => (v < a ? a : v > b ? b : v);
const pick = a => a[(Math.random() * a.length) | 0];
const angDiff = (a, b) => { let d = (b - a) % TAU; if (d > Math.PI) d -= TAU; if (d < -Math.PI) d += TAU; return d; };
function shuffle(a) { for (let i = a.length - 1; i > 0; i--) { const j = (Math.random() * (i + 1)) | 0; [a[i], a[j]] = [a[j], a[i]]; } return a; }
function rr(g, x, y, w, h, r) { g.beginPath(); g.moveTo(x + r, y); g.arcTo(x + w, y, x + w, y + h, r); g.arcTo(x + w, y + h, x, y + h, r); g.arcTo(x, y + h, x, y, r); g.arcTo(x, y, x + w, y, r); g.closePath(); }
function txt(g, s, x, y, size, color = '#fff', align = 'center', font = 'Bungee') {
  g.font = font === 'Nunito' ? `700 ${size}px Inter, system-ui, sans-serif` : `700 ${size}px Fredoka, Inter, system-ui, sans-serif`;
  g.textAlign = align; g.textBaseline = 'middle'; g.lineJoin = 'round';
  g.lineWidth = Math.max(3, size / 6); g.strokeStyle = 'rgba(14,10,36,.8)'; g.strokeText(s, x, y);
  g.fillStyle = color; g.fillText(s, x, y);
}
function wrapLines(g, s, maxW, size) {
  g.font = `900 ${size}px Nunito, sans-serif`;
  const words = String(s).split(' '), lines = []; let cur = '';
  for (const w of words) { const t = cur ? cur + ' ' + w : w; if (g.measureText(t).width > maxW && cur) { lines.push(cur); cur = w; } else cur = t; }
  if (cur) lines.push(cur);
  return lines;
}
function para(g, s, x, y, maxW, size, color = '#fff') {
  const lines = wrapLines(g, s, maxW, size), lh = size * 1.18;
  lines.forEach((l, i) => txt(g, l, x, y + (i - (lines.length - 1) / 2) * lh, size, color, 'center', 'Nunito'));
}
function bgGrad(g, c1, c2, cx = 800, cy = 450) {
  const gr = g.createRadialGradient(cx, cy, 60, cx, cy, 1000);
  gr.addColorStop(0, c1); gr.addColorStop(1, c2); g.fillStyle = gr; g.fillRect(0, 0, 1600, 900);
}
function board(g, rows, x = 20, y = 20, title) {
  const h = 34, top = title ? 34 : 0;
  g.fillStyle = 'rgba(14,10,36,.5)'; rr(g, x, y, 270, rows.length * h + 16 + top, 16); g.fill();
  if (title) txt(g, title, x + 135, y + 24, 18, '#b3acdf');
  rows.forEach((r, i) => {
    const yy = y + top + 8 + h * i + h / 2;
    g.fillStyle = r.color; g.beginPath(); g.arc(x + 22, yy, 10, 0, TAU); g.fill();
    txt(g, r.name, x + 42, yy, 20, '#fff', 'left', 'Nunito');
    txt(g, String(r.val), x + 254, yy, 22, '#ffd43b', 'right');
  });
}
/* horizontal score chips across the top of the screen */
function strip(g, rows, y = 46, maxW = 1380) {
  const w = Math.min(190, (maxW - 20) / Math.max(1, rows.length) - 6);
  rows.forEach((r, i) => {
    const x = 20 + i * (w + 6);
    g.fillStyle = r.dim ? 'rgba(14,10,36,.35)' : 'rgba(14,10,36,.68)'; rr(g, x, y - 24, w, 48, 14); g.fill();
    if (r.hi) { g.lineWidth = 3; g.strokeStyle = r.color; g.stroke(); }
    g.globalAlpha = r.dim ? 0.45 : 1;
    g.fillStyle = r.color; g.beginPath(); g.arc(x + 20, y, 10, 0, TAU); g.fill();
    txt(g, String(r.name).slice(0, w > 150 ? 10 : 7), x + 36, y, 17, '#fff', 'left', 'Nunito');
    txt(g, String(r.val), x + w - 10, y, 20, '#ffd43b', 'right');
    g.globalAlpha = 1;
  });
}
function clock(g, T, x = 1530, y = 46) {
  const m = Math.max(0, Math.ceil(T));
  g.fillStyle = 'rgba(14,10,36,.68)'; rr(g, x - 64, y - 26, 128, 52, 16); g.fill();
  txt(g, `${(m / 60) | 0}:${String(m % 60).padStart(2, '0')}`, x, y + 1, 30, m <= 10 ? '#ff4d6d' : '#fff');
}
function timeBar(g, frac, y = 96, color = '#ffd43b') {
  g.fillStyle = 'rgba(255,255,255,.12)'; rr(g, 200, y, 1200, 14, 7); g.fill();
  g.fillStyle = frac < 0.25 ? '#ff4d6d' : color; rr(g, 200, y, Math.max(14, 1200 * clamp(frac, 0, 1)), 14, 7); g.fill();
}

/* the cute blob character used by many games */
function drawBlob(g, x, y, r, color, face = 0, o = {}) {
  const sq = 1 + (o.squish || 0) * 0.25;
  g.save(); g.translate(x, y); g.globalAlpha = o.alpha ?? 1;
  if (o.shadow !== false) { g.fillStyle = 'rgba(0,0,0,.25)'; g.beginPath(); g.ellipse(4, r * 0.85, r * 0.9, r * 0.32, 0, 0, TAU); g.fill(); }
  if (o.ring) { g.lineWidth = 7; g.strokeStyle = o.ring; g.beginPath(); g.arc(0, 0, r + 7, 0, TAU); g.stroke(); }
  g.scale(sq, 1 / sq);
  g.fillStyle = o.fill || color; g.beginPath(); g.arc(0, 0, r, 0, TAU); g.fill();
  g.lineWidth = 4; g.strokeStyle = 'rgba(20,14,50,.75)'; g.stroke();
  g.fillStyle = 'rgba(255,255,255,.38)'; g.beginPath(); g.ellipse(-r * 0.35, -r * 0.42, r * 0.32, r * 0.18, -0.6, 0, TAU); g.fill();
  const fx = Math.cos(face) * r * 0.28, fy = Math.sin(face) * r * 0.28, er = r * 0.25;
  for (const side of [-1, 1]) {
    const ex = fx + Math.cos(face + Math.PI / 2) * r * 0.34 * side, ey = fy + Math.sin(face + Math.PI / 2) * r * 0.34 * side - r * 0.1;
    if (o.dead) {
      g.strokeStyle = '#111'; g.lineWidth = 3; g.beginPath(); g.moveTo(ex - er * 0.6, ey - er * 0.6); g.lineTo(ex + er * 0.6, ey + er * 0.6); g.moveTo(ex + er * 0.6, ey - er * 0.6); g.lineTo(ex - er * 0.6, ey + er * 0.6); g.stroke();
    } else {
      g.fillStyle = '#fff'; g.beginPath(); g.arc(ex, ey, er, 0, TAU); g.fill();
      g.fillStyle = '#111'; g.beginPath(); g.arc(ex + Math.cos(face) * er * 0.35, ey + Math.sin(face) * er * 0.35, er * 0.5, 0, TAU); g.fill();
    }
  }
  if (o.happy) { g.strokeStyle = '#111'; g.lineWidth = 3; g.beginPath(); g.arc(fx * 0.6, fy * 0.6 + r * 0.2, r * 0.3, 0.2, Math.PI - 0.2); g.stroke(); }
  g.restore(); g.globalAlpha = 1;
  if (o.hat) { // winter beanie
    g.save(); g.translate(x, y); g.globalAlpha = o.alpha ?? 1;
    g.fillStyle = o.hat; g.beginPath(); g.arc(0, -r * 0.35, r * 0.82, Math.PI * 1.08, Math.PI * 1.92); g.closePath(); g.fill();
    g.fillStyle = '#fff'; rr(g, -r * 0.82, -r * 0.62, r * 1.64, r * 0.26, r * 0.12); g.fill();
    g.beginPath(); g.arc(0, -r * 1.2, r * 0.22, 0, TAU); g.fill();
    g.restore(); g.globalAlpha = 1;
  }
  if (o.crown) {
    g.fillStyle = '#ffd43b'; g.strokeStyle = '#7a5300'; g.lineWidth = 2; g.beginPath();
    const cy = y - r - 4; g.moveTo(x - 16, cy); g.lineTo(x - 18, cy - 18); g.lineTo(x - 8, cy - 8); g.lineTo(x, cy - 22); g.lineTo(x + 8, cy - 8); g.lineTo(x + 18, cy - 18); g.lineTo(x + 16, cy); g.closePath(); g.fill(); g.stroke();
  }
  if (o.name) txt(g, o.name, x, y - r - (o.crown ? 36 : 16), Math.max(14, Math.min(20, r * 0.6)), color, 'center', 'Nunito');
}

class Particles {
  constructor() { this.l = []; }
  burst(x, y, n, color, spd = 200, life = 0.8, size = 6) {
    for (let i = 0; i < n; i++) { const a = Math.random() * TAU, s = Math.random() * spd; this.l.push({ x, y, vx: Math.cos(a) * s, vy: Math.sin(a) * s, life: life * rnd(0.6, 1), max: life, size: size * rnd(0.5, 1.3), color }); }
  }
  puff(x, y, color, size = 8, life = 0.6, vx = 0, vy = 0) { this.l.push({ x, y, vx, vy, life, max: life, size, color, grow: true }); }
  text(x, y, s, color = '#ffd43b', size = 30) { this.l.push({ x, y, vx: 0, vy: -70, life: 1.1, max: 1.1, size, color, text: s }); }
  confetti(n = 120) {
    const cols = ['#ff4d6d', '#ffd43b', '#4dabff', '#51cf66', '#cc5de8', '#ff922b'];
    for (let i = 0; i < n; i++) this.l.push({ x: rnd(0, 1600), y: rnd(-200, 0), vx: rnd(-60, 60), vy: rnd(150, 380), life: 4, max: 4, size: rnd(5, 10), color: pick(cols), conf: true, rot: rnd(0, 6) });
  }
  update(dt) {
    const k = Math.pow(0.08, dt);
    for (const p of this.l) {
      p.x += p.vx * dt; p.y += p.vy * dt; p.life -= dt;
      if (p.conf) p.rot += dt * 8; else if (!p.text) { p.vx *= k; p.vy *= k; }
    }
    this.l = this.l.filter(p => p.life > 0);
  }
  draw(g) {
    for (const p of this.l) {
      const t = p.life / p.max;
      g.globalAlpha = Math.min(1, t * 1.5);
      if (p.text) { txt(g, p.text, p.x, p.y, p.size, p.color); continue; }
      g.fillStyle = p.color;
      if (p.conf) { g.save(); g.translate(p.x, p.y); g.rotate(p.rot); g.fillRect(-p.size / 2, -p.size / 4, p.size, p.size / 2); g.restore(); continue; }
      g.beginPath(); g.arc(p.x, p.y, p.grow ? p.size * (2 - t) : p.size * t + 1, 0, TAU); g.fill();
    }
    g.globalAlpha = 1;
  }
}

class Countdown {
  constructor(c, secs = 3) { this.c = c; this.t = secs + 0.5; this.last = 99; }
  get done() { return this.t <= 0.5; }
  update(dt) {
    this.t -= dt;
    const n = Math.ceil(this.t - 0.5);
    if (n < this.last && n > 0) { this.last = n; this.c.sfx.tick(); }
    if (this.t <= 0.5 && !this.went) { this.went = true; this.c.sfx.go(); }
    return this.t <= 0.5;
  }
  draw(g) {
    if (this.t > 0.5) { const n = Math.ceil(this.t - 0.5), f = (this.t - 0.5) % 1; txt(g, String(n), 800, 450, 160 + f * 90, '#ffd43b'); }
    else if (this.t > -0.4) txt(g, 'GO!', 800, 450, 220, '#51cf66');
  }
}
const rankBy = (ps, val, stat) => [...ps].sort((a, b) => val(b) - val(a)).map(p => ({ p, stat: stat(p) }));
/* rank: still-alive players first (by score), then eliminated in reverse order of elimination */
function survivorRank(ps, alive, outOrder, stat) {
  const a = ps.filter(p => alive(p)), o = [...outOrder].reverse().filter(p => !a.includes(p));
  const rest = ps.filter(p => !a.includes(p) && !o.includes(p));
  return [...a, ...o, ...rest].map((p, i) => ({ p, stat: stat ? stat(p, i) : (a.includes(p) ? 'survived!' : `out #${ps.length - i}`) }));
}

/* =====================================================================
   ArenaGame — top-down arena with blob players (move + dash), walls,
   bumping, countdown, clock, scores. Subclasses add rules via hooks:
   setup(), tick(dt), onBump(a,b,impulse), onDash(b), botTarget(b),
   isOver(), rank(), drawArena(g), drawUnder(g), drawOver(g), blobStyle(b)
   ===================================================================== */
class ArenaGame {
  constructor(c, o = {}) {
    this.c = c; this.fx = new Particles();
    this.o = Object.assign({ min: 2, time: 60, stick: 'xy', a: null, b: 'DASH', hint: '', acc: 1500, fric: 2.6, r: 32, dash: 900, collide: true, e: 0.8, walls: true,
      box: { x: 60, y: 100, w: 1480, h: 770 }, unit: 'pts', clock: true }, o);
    this.T = this.o.time; this.cd = new Countdown(c); this.score = new Map(); this.el = 0; this.out = [];
  }
  get box() { return this.o.box; }
  start() {
    this.ps = this.c.roster(this.o.min);
    this.ps.forEach(p => this.score.set(p, 0));
    this.blobs = this.ps.map((p, i) => this.spawn(p, i, this.ps.length));
    this.c.setLayout('all', { name: 'pad', stick: this.o.stick, a: this.o.a, b: this.o.b, hint: this.o.hint });
    if (this.setup) this.setup();
  }
  spawn(p, i, n) {
    const bx = this.box, a = (i / n) * TAU - Math.PI / 2, rad = Math.min(bx.w, bx.h) * 0.32;
    return { p, x: bx.x + bx.w / 2 + Math.cos(a) * rad, y: bx.y + bx.h / 2 + Math.sin(a) * rad, vx: 0, vy: 0, r: this.o.r, m: 1, alive: true, cd: 0, face: a + Math.PI, squish: 0, stun: 0, inv: 0, dashT: 0, speedMul: 1 };
  }
  blobOf(p) { return this.blobs.find(b => b.p === p); }
  inputFor(b) { if (!b.alive) return { x: 0, y: 0 }; return b.p.bot ? this.botInput(b) : b.p.input; }
  botInput(b) {
    const t = this.botTarget ? this.botTarget(b) : null;
    if (!t) return { x: Math.sin(this.el * 0.9 + b.x * 0.01) * 0.7, y: Math.cos(this.el * 1.1 + b.y * 0.01) * 0.7 };
    let dx = t.x - b.x, dy = t.y - b.y; const d = Math.hypot(dx, dy) || 1;
    if (t.flee) { dx = -dx; dy = -dy; const bx = this.box, cx = bx.x + bx.w / 2 - b.x, cy = bx.y + bx.h / 2 - b.y, cl = Math.hypot(cx, cy) || 1; dx = dx / d + (cx / cl) * 0.6; dy = dy / d + (cy / cl) * 0.6; return { x: dx, y: dy }; }
    const slow = t.stop && d < 30 ? d / 30 : 1;
    return { x: (dx / d) * slow, y: (dy / d) * slow, b: !!t.dash && d < (t.dashRange || 200) && Math.random() < 0.06 };
  }
  update(dt) {
    this.fx.update(dt);
    if (!this.cd.update(dt)) { if (this.idle) this.idle(dt); return; }
    if (this.over) { this.overT -= dt; for (const b of this.blobs) if (b.alive) this.move(b, { x: 0, y: 0 }, dt); if (this.overT <= 0) this.end(); return; }
    this.el += dt; if (this.o.clock) this.T -= dt;
    for (const b of this.blobs) if (b.alive) this.move(b, this.inputFor(b), dt);
    if (this.o.collide) this.collide();
    if (this.tick) this.tick(dt);
    if ((this.o.clock && this.T <= 0) || (this.isOver && this.isOver())) this.gameOver();
  }
  gameOver() {
    if (this.over) return;
    this.over = true; this.overT = 2.2; this.c.sfx.win(); this.fx.confetti(90);
    this.ranking = this.rank();
    const w = this.ranking[0];
    if (w) this.c.banner(`${w.p.name} WINS!`, 2000, w.p.color);
  }
  end() { if (this.done) return; this.done = true; this.c.finish(this.ranking || this.rank()); }
  rank() { return rankBy(this.ps, p => this.score.get(p), p => `${Math.round(this.score.get(p))} ${this.o.unit}`); }
  eliminate(b, why) {
    if (!b.alive) return;
    b.alive = false; b.fade = 1; this.out.push(b.p);
    this.fx.burst(b.x, b.y, 30, b.p.color, 320, 0.9, 9); this.c.sfx.boom(); this.c.shake(10); this.c.send(b.p, { t: 'buzz', ms: 400 });
    if (why) this.fx.text(b.x, b.y - 40, why, '#ff4d6d', 28);
  }
  move(b, inp, dt) {
    b.cd -= dt; b.inv = Math.max(0, b.inv - dt); b.squish = Math.max(0, b.squish - dt * 3); b.dashT = Math.max(0, b.dashT - dt);
    if (b.stun > 0) { b.stun -= dt; inp = { x: 0, y: 0 }; }
    let ax = inp.x || 0, ay = this.o.stick === 'x' ? 0 : (inp.y || 0);
    const l = Math.hypot(ax, ay); if (l > 1) { ax /= l; ay /= l; }
    const acc = this.o.acc * (b.speedMul || 1);
    b.vx += ax * acc * dt; b.vy += ay * acc * dt;
    if (inp.b && this.o.b && b.cd <= 0) {
      let dx = ax, dy = ay;
      if (Math.hypot(dx, dy) < 0.2) { dx = Math.cos(b.face); dy = Math.sin(b.face); }
      const dl = Math.hypot(dx, dy) || 1;
      b.vx += (dx / dl) * this.o.dash; b.vy += (dy / dl) * this.o.dash; b.cd = 1.1; b.dashT = 0.35;
      this.fx.burst(b.x, b.y, 10, b.p.color, 200, 0.45, 6); this.c.sfx.dash();
      if (this.onDash) this.onDash(b);
    }
    const k = Math.exp(-this.o.fric * dt); b.vx *= k; b.vy *= k;
    b.x += b.vx * dt; b.y += b.vy * dt;
    if (Math.hypot(b.vx, b.vy) > 30) b.face = Math.atan2(b.vy, b.vx);
    if (this.o.walls) {
      const bx = this.box;
      if (b.x < bx.x + b.r) { b.x = bx.x + b.r; b.vx = Math.abs(b.vx) * 0.5; }
      if (b.x > bx.x + bx.w - b.r) { b.x = bx.x + bx.w - b.r; b.vx = -Math.abs(b.vx) * 0.5; }
      if (b.y < bx.y + b.r) { b.y = bx.y + b.r; b.vy = Math.abs(b.vy) * 0.5; }
      if (b.y > bx.y + bx.h - b.r) { b.y = bx.y + bx.h - b.r; b.vy = -Math.abs(b.vy) * 0.5; }
    }
  }
  collide() {
    const al = this.blobs.filter(b => b.alive), e = this.o.e;
    for (let i = 0; i < al.length; i++) for (let j = i + 1; j < al.length; j++) {
      const a = al[i], b = al[j];
      const dx = b.x - a.x, dy = b.y - a.y, d = Math.hypot(dx, dy), min = a.r + b.r;
      if (d < min && d > 0.01) {
        const nx = dx / d, ny = dy / d, ia = 1 / a.m, ib = 1 / b.m, ov = min - d;
        a.x -= nx * ov * ia / (ia + ib); a.y -= ny * ov * ia / (ia + ib); b.x += nx * ov * ib / (ia + ib); b.y += ny * ov * ib / (ia + ib);
        const rv = (b.vx - a.vx) * nx + (b.vy - a.vy) * ny;
        let jj = 0;
        if (rv < 0) {
          jj = -(1 + e) * rv / (ia + ib);
          a.vx -= jj * ia * nx; a.vy -= jj * ia * ny; b.vx += jj * ib * nx; b.vy += jj * ib * ny;
          if (jj > 280) { a.squish = b.squish = Math.min(1, jj / 1200); this.c.sfx.bonk(); this.fx.burst(a.x + nx * a.r, a.y + ny * a.r, 6, '#fff', 180, 0.35, 5); this.c.shake(Math.min(8, jj / 200)); }
        }
        if (this.onBump) this.onBump(a, b, jj);
      }
    }
  }
  drawArena(g) {
    bgGrad(g, '#3b2f7a', '#120e2b');
    const bx = this.box;
    g.fillStyle = 'rgba(0,0,0,.3)'; rr(g, bx.x + 8, bx.y + 12, bx.w, bx.h, 30); g.fill();
    g.fillStyle = '#e9e4f7'; rr(g, bx.x, bx.y, bx.w, bx.h, 30); g.fill();
    g.lineWidth = 8; g.strokeStyle = '#5f3dc4'; g.stroke();
  }
  drawBlobs(g) {
    const list = this.blobs.filter(b => b.alive || (b.fade > 0)).sort((a, b) => a.y - b.y);
    for (const b of list) {
      if (!b.alive) { b.fade -= 0.03; drawBlob(g, b.x, b.y, b.r * Math.max(0.1, b.fade), b.p.color, b.face, { alpha: Math.max(0, b.fade), dead: true }); continue; }
      const st = this.blobStyle ? this.blobStyle(b) : {};
      drawBlob(g, b.x, b.y, b.r, b.p.color, b.face, Object.assign({ squish: b.squish, name: b.p.name, alpha: b.inv > 0 && ((b.inv * 10) | 0) % 2 ? 0.45 : 1, dead: b.stun > 0 }, st));
    }
  }
  scoreRows() { return [...this.ps].sort((a, b) => this.score.get(b) - this.score.get(a)).map(p => ({ name: p.name, color: p.color, val: Math.round(this.score.get(p)), dim: this.blobOf(p) && !this.blobOf(p).alive })); }
  drawHUD(g) { strip(g, this.scoreRows()); if (this.o.clock) clock(g, this.T); }
  draw(g) {
    this.drawArena(g);
    if (this.drawUnder) this.drawUnder(g);
    this.drawBlobs(g);
    if (this.drawOver) this.drawOver(g);
    this.fx.draw(g);
    this.drawHUD(g);
    this.cd.draw(g);
  }
}

/* =====================================================================
   QuizGame — rounds of multiple choice on the phones.
   makeRound(n) returns { text, opts:[labels], correct?, time?, cols?, draw?(g,game), resolve?(answers,game) -> Map(p->points), revealText? }
   ===================================================================== */
const OPT_COLORS = ['#ff4d6d', '#4dabff', '#ffd43b', '#51cf66', '#cc5de8', '#ff922b', '#22d3ee', '#f783ac', '#94d82d', '#fd7e14'];
class QuizGame {
  constructor(c, o = {}) {
    this.c = c; this.o = Object.assign({ rounds: 8, time: 12, min: 1, title: 'QUIZ', bg: ['#2d2470', '#0e0a24'], botSkill: 0.55 }, o);
    this.round = 0; this.score = new Map(); this.fx = new Particles(); this.phase = 'wait'; this.t = 0.8;
  }
  start() {
    this.ps = this.c.roster(this.o.min);
    this.ps.forEach(p => this.score.set(p, 0));
    if (this.init) this.init();
    this.c.setLayout('all', { name: 'msg', emoji: '👀', text: 'Get ready…', sub: 'Answers appear on your phone' });
  }
  next() {
    this.round++;
    if (this.round > this.o.rounds) return this.end();
    this.q = this.makeRound(this.round);
    this.answers = new Map(); this.phase = 'ask';
    this.t = this.askAt = this.q.time || this.o.time;
    this.c.sfx.blip();
    for (const p of this.ps) if (!p.bot) this.c.setLayout(p, this.layoutFor(p));
    this.botT = new Map(this.ps.filter(p => p.bot).map(p => [p, rnd(1.2, Math.max(1.5, Math.min(7, this.askAt - 1)))]));
  }
  layoutFor(p) {
    const q = this.q;
    return { name: 'choice', prompt: q.phone ?? q.text ?? '', options: q.opts.map((o, i) => ({ label: String(o), color: (q.colors || OPT_COLORS)[i % 10] })), cols: q.cols || (q.opts.length > 4 ? 3 : 2), big: !!q.big, multi: q.multi || 0, round: this.round };
  }
  botAnswer(p) {
    if (this.q.botAnswer) return this.q.botAnswer(p, this);
    if (this.q.correct != null && Math.random() < this.o.botSkill) return this.q.correct;
    return (Math.random() * this.q.opts.length) | 0;
  }
  onAct(p, m) {
    if (m.a !== 'choice' || this.phase !== 'ask' || this.answers.has(p) || !this.score.has(p)) return;
    const i = m.i | 0; if (i < 0 || i >= this.q.opts.length) return;
    if (this.q.allowed && !this.q.allowed(p, i)) return;
    this.answers.set(p, { i, t: this.askAt - this.t });
    this.c.sfx.blip();
    if (!p.bot) this.c.setLayout(p, { name: 'msg', emoji: '🔒', text: 'Locked in!', sub: String(this.q.opts[i]) });
    if (this.answers.size === this.ps.length) this.t = Math.min(this.t, 0.4);
  }
  update(dt) {
    this.fx.update(dt); this.t -= dt;
    if (this.phase === 'wait') { if (this.t <= 0) this.next(); return; }
    if (this.phase === 'ask') {
      for (const [p, at] of this.botT) if (this.askAt - this.t >= at) { this.botT.delete(p); this.onAct(p, { a: 'choice', i: this.botAnswer(p) }); }
      if (this.q.tick) this.q.tick(dt, this);
      if (this.t <= 0) this.reveal();
    } else if (this.phase === 'reveal' && this.t <= 0) this.next();
  }
  reveal() {
    this.phase = 'reveal'; this.t = this.q.revealTime || 4;
    const gains = this.q.resolve ? this.q.resolve(this.answers, this) : this.defaultResolve();
    this.gains = gains;
    let any = false;
    for (const p of this.ps) {
      const gn = gains.get(p) || 0; if (gn > 0) any = true;
      this.score.set(p, this.score.get(p) + gn);
      if (!p.bot) {
        const a = this.answers.get(p);
        this.c.setLayout(p, { name: 'msg', emoji: !a ? '⏰' : gn > 0 ? '✅' : '❌', text: !a ? 'Too slow!' : gn > 0 ? `+${gn} points` : 'Nope!', sub: this.q.revealText || '' });
      }
    }
    any ? this.c.sfx.ding() : this.c.sfx.buzz();
  }
  defaultResolve() {
    const m = new Map();
    for (const [p, a] of this.answers) if (a.i === this.q.correct) m.set(p, Math.round(500 + 500 * Math.max(0, 1 - a.t / this.askAt)));
    return m;
  }
  end() {
    if (this.done) return; this.done = true;
    this.c.finish(rankBy(this.ps, p => this.score.get(p), p => `${this.score.get(p)} pts`));
  }
  drawOptions(g, y0 = 520, y1 = 850) {
    const q = this.q, n = q.opts.length, cols = q.cols || (n > 4 ? 3 : 2), rows = Math.ceil(n / cols);
    const W = 1400, gap = 16, cw = (W - gap * (cols - 1)) / cols, ch = Math.min(150, (y1 - y0 - gap * (rows - 1)) / rows);
    q.opts.forEach((o, i) => {
      const cx = 100 + (i % cols) * (cw + gap), cy = y0 + ((i / cols) | 0) * (ch + gap);
      const rev = this.phase === 'reveal', good = rev && (q.correct === i || (q.goodSet && q.goodSet.has(i)));
      g.globalAlpha = rev && q.correct != null && !good ? 0.3 : 1;
      g.fillStyle = (q.colors || OPT_COLORS)[i % 10]; rr(g, cx, cy, cw, ch, 22); g.fill();
      if (good) { g.lineWidth = 8; g.strokeStyle = '#fff'; g.stroke(); }
      txt(g, String.fromCharCode(65 + i), cx + 34, cy + ch / 2, 30, 'rgba(255,255,255,.85)');
      const label = String(o);
      txt(g, label, cx + cw / 2 + 14, cy + ch / 2, q.big ? Math.min(80, ch * 0.6) : Math.min(40, (cw - 80) / Math.max(4, label.length) * 1.7), '#fff', 'center', 'Nunito');
      g.globalAlpha = 1;
      if (rev) {
        let k = 0;
        for (const [p, a] of this.answers) if (a.i === i) { const px = cx + cw - 30 - k * 30, py = cy + 26; g.fillStyle = p.color; g.beginPath(); g.arc(px, py, 13, 0, TAU); g.fill(); g.lineWidth = 3; g.strokeStyle = '#fff'; g.stroke(); k++; }
      }
    });
  }
  draw(g) {
    bgGrad(g, this.o.bg[0], this.o.bg[1]);
    strip(g, rankBy(this.ps, p => this.score.get(p), () => '').map(r => ({ name: r.p.name, color: r.p.color, val: this.score.get(r.p), hi: this.answers && this.answers.has(r.p) })), 40, 1600);
    if (this.phase === 'wait' || !this.q) { txt(g, this.o.title, 800, 430, 90, '#ffd43b'); txt(g, `${this.o.rounds} rounds · answer on your phone`, 800, 530, 30, '#b3acdf', 'center', 'Nunito'); return; }
    txt(g, `${this.o.title} · ${Math.min(this.round, this.o.rounds)}/${this.o.rounds}`, 800, 92, 22, '#b3acdf', 'center', 'Nunito');
    if (this.phase === 'ask') timeBar(g, this.t / this.askAt, 112);
    if (this.q.draw) this.q.draw(g, this); else para(g, this.q.text, 800, 300, 1350, 56);
    if (!this.q.hideOpts) this.drawOptions(g);
    if (this.phase === 'ask') txt(g, `${this.answers.size}/${this.ps.length} answered`, 800, 875, 20, '#b3acdf', 'center', 'Nunito');
    if (this.phase === 'reveal' && this.q.revealText) txt(g, this.q.revealText, 800, this.q.hideOpts ? 850 : 470, this.q.hideOpts ? 38 : 30, '#ffd43b', 'center', 'Nunito');
    this.fx.draw(g);
  }
}
