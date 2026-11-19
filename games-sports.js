/* Couch Chaos — 🎾 Motion Sports: pseudo-3D stadium games played by swinging the phone.
   Phones send {a:'swing', p:power 0..1} through onAct. */

const lerp = (a, b, u) => a + (b - a) * u;

/* ---------- tiny 3D camera: world x = right, y = up, z = away from the camera ---------- */
class Cam3D {
  constructor(o) { Object.assign(this, { x: 0, y: 3, z: -6, pitch: 0.2, f: 1000, cx: 800, cy: 300 }, o); }
  p(x, y, z) {
    const dx = x - this.x, dy = y - this.y, dz = z - this.z, c = Math.cos(this.pitch), s = Math.sin(this.pitch);
    const zc = dz * c - dy * s, yc = dy * c + dz * s, k = this.f / Math.max(0.08, zc);
    return { x: this.cx + dx * k, y: this.cy - yc * k, s: k, z: zc };
  }
  horizon() { return this.cy - Math.tan(this.pitch) * this.f; }
}
function poly3(g, cam, pts, fill, stroke, lw = 2) {
  g.beginPath(); pts.forEach((q, i) => { const p = cam.p(q[0], q[1], q[2]); i ? g.lineTo(p.x, p.y) : g.moveTo(p.x, p.y); }); g.closePath();
  if (fill) { g.fillStyle = fill; g.fill(); } if (stroke) { g.strokeStyle = stroke; g.lineWidth = lw; g.stroke(); }
}
function line3(g, cam, a, b, color, lw = 3) { const p = cam.p(...a), q = cam.p(...b); g.strokeStyle = color; g.lineWidth = lw; g.beginPath(); g.moveTo(p.x, p.y); g.lineTo(q.x, q.y); g.stroke(); }
function shadow3(g, cam, x, z, r, a = 0.35) { const p = cam.p(x, 0.01, z); g.fillStyle = `rgba(0,0,0,${a})`; g.beginPath(); g.ellipse(p.x, p.y, Math.max(1, r * p.s), Math.max(1, r * p.s * 0.35), 0, 0, TAU); g.fill(); }
function ball3(g, cam, x, y, z, r, color, glow) {
  const p = cam.p(x, y, z), R = Math.max(2, r * p.s);
  if (glow) { g.save(); g.globalCompositeOperation = 'lighter'; const gr = g.createRadialGradient(p.x, p.y, 0, p.x, p.y, R * 4); gr.addColorStop(0, glow); gr.addColorStop(1, 'rgba(0,0,0,0)'); g.fillStyle = gr; g.beginPath(); g.arc(p.x, p.y, R * 4, 0, TAU); g.fill(); g.restore(); }
  g.fillStyle = color; g.beginPath(); g.arc(p.x, p.y, R, 0, TAU); g.fill();
  g.fillStyle = 'rgba(255,255,255,.55)'; g.beginPath(); g.arc(p.x - R * 0.35, p.y - R * 0.35, R * 0.35, 0, TAU); g.fill();
  return p;
}
/* glowing comet trail from a list of screen points */
function trail(g, pts, fire) {
  if (pts.length < 2) return;
  g.save(); g.globalCompositeOperation = 'lighter'; g.lineCap = 'round';
  for (let i = 1; i < pts.length; i++) {
    const a = pts[i - 1], b = pts[i], k = i / pts.length;
    g.strokeStyle = fire ? `hsla(${20 + k * 30},100%,${45 + k * 20}%,${k * 0.9})` : `rgba(200,240,255,${k * 0.5})`;
    g.lineWidth = Math.max(1, b.r * (fire ? 2.6 : 1.4) * k);
    g.beginPath(); g.moveTo(a.x, a.y); g.lineTo(b.x, b.y); g.stroke();
  }
  g.restore();
}
/* night stadium: sky, stars, sweeping spotlights, a crowd that goes wild */
function stadium(g, t, hy, excite, c1 = '#1b1046', c2 = '#4c1d95') {
  const gr = g.createLinearGradient(0, 0, 0, hy); gr.addColorStop(0, '#05030f'); gr.addColorStop(0.6, c1); gr.addColorStop(1, c2);
  g.fillStyle = gr; g.fillRect(0, 0, 1600, Math.max(0, hy) + 2);
  g.fillStyle = 'rgba(255,255,255,.7)'; for (let i = 0; i < 60; i++) { const x = (i * 263.7) % 1600, y = (i * 97.3) % Math.max(40, hy - 120); g.globalAlpha = 0.3 + 0.7 * Math.abs(Math.sin(t * 1.3 + i)); g.fillRect(x, y, 2, 2); }
  g.globalAlpha = 1;
  // spotlights
  g.save(); g.globalCompositeOperation = 'lighter';
  for (let i = 0; i < 4; i++) {
    const bx = [120, 560, 1040, 1480][i], ang = Math.sin(t * (0.5 + i * 0.13) + i * 1.7) * 0.5 + (i < 2 ? 0.35 : -0.35);
    const len = 1100, w = 0.09;
    const gg = g.createLinearGradient(bx, 0, bx + Math.sin(ang) * len, Math.cos(ang) * len);
    const col = ['255,77,109', '77,171,255', '255,212,59', '204,93,232'][i];
    gg.addColorStop(0, `rgba(${col},${0.32 + excite * 0.25})`); gg.addColorStop(1, `rgba(${col},0)`);
    g.fillStyle = gg; g.beginPath(); g.moveTo(bx, -10);
    g.lineTo(bx + Math.sin(ang - w) * len, Math.cos(ang - w) * len); g.lineTo(bx + Math.sin(ang + w) * len, Math.cos(ang + w) * len); g.closePath(); g.fill();
  }
  g.restore();
  // crowd stands
  const top = hy - 150; if (top < -40) return;
  g.fillStyle = '#120a2e'; g.fillRect(0, top, 1600, 152);
  const cols = ['#ff4d6d', '#ffd43b', '#4dabff', '#51cf66', '#cc5de8', '#ff922b', '#f1f3f5'];
  for (let row = 0; row < 5; row++) {
    const y0 = top + 22 + row * 27, n = 64;
    for (let i = 0; i < n; i++) {
      const x = (i + (row % 2) * 0.5) * (1600 / n), j = Math.sin(t * (6 + excite * 6) + i * 1.9 + row) * (1.5 + excite * 9);
      g.fillStyle = cols[(i * 7 + row * 3) % cols.length]; g.beginPath(); g.arc(x, y0 + Math.min(0, j), 7, 0, TAU); g.fill();
      if (excite > 0.4 && (i + row) % 3 === 0) { g.fillRect(x - 2, y0 - 16 + j, 4, 9); }
    }
  }
  g.fillStyle = 'rgba(0,0,0,.35)'; g.fillRect(0, hy - 8, 1600, 10);
}
/* screen-space pop text with elastic scale */
class PopText {
  constructor() { this.l = []; }
  add(s, x, y, color = '#ffd43b', size = 90, life = 1.3) { this.l.push({ s, x, y, color, size, life, max: life }); }
  update(dt) { for (const p of this.l) p.life -= dt; this.l = this.l.filter(p => p.life > 0); }
  draw(g) {
    for (const p of this.l) {
      const a = 1 - p.life / p.max, sc = a < 0.18 ? 0.3 + (a / 0.18) * 1.0 : 1 + Math.sin(a * 20) * 0.04 * (1 - a);
      g.save(); g.globalAlpha = Math.min(1, p.life / 0.3); g.translate(p.x, p.y - a * 30); g.rotate(Math.sin(a * 9) * 0.05); g.scale(sc, sc);
      txt(g, p.s, 0, 0, p.size, p.color); g.restore();
    }
    g.globalAlpha = 1;
  }
}
function fireworks(fx, n = 3) { for (let i = 0; i < n; i++) fx.burst(rnd(200, 1400), rnd(80, 300), 40, pick(['#ff4d6d', '#ffd43b', '#4dabff', '#51cf66', '#cc5de8', '#fff']), 420, 1.3, 5); }
/* racket / bat held by a projected blob. sw: 0 rest → 1 just swung */
function drawGear(g, x, y, s, color, kind, sw, side = 1) {
  g.save(); g.translate(x + side * s * 0.55, y); g.rotate(side * (-0.9 + (1 - Math.pow(1 - sw, 2)) * 2.6) * (sw > 0 ? 1 : 0.35));
  if (kind === 'bat') { g.fillStyle = '#c08552'; rr(g, -s * 0.07, -s * 1.15, s * 0.14, s * 1.15, s * 0.07); g.fill(); g.fillStyle = '#5c3a1e'; g.fillRect(-s * 0.05, -s * 0.2, s * 0.1, s * 0.2); }
  else { g.fillStyle = '#343a40'; g.fillRect(-s * 0.04, -s * 0.5, s * 0.08, s * 0.5); g.strokeStyle = color; g.lineWidth = Math.max(2, s * 0.07); g.beginPath(); g.ellipse(0, -s * 0.78, s * 0.24, s * 0.3, 0, 0, TAU); g.stroke(); g.fillStyle = 'rgba(255,255,255,.25)'; g.fill(); }
  g.restore();
  if (sw > 0.6) { g.save(); g.globalCompositeOperation = 'lighter'; g.strokeStyle = `rgba(255,255,255,${(sw - 0.6) * 1.5})`; g.lineWidth = s * 0.12; g.beginPath(); g.arc(x, y - s * 0.2, s * 1.1, side > 0 ? -2.4 : -0.7, side > 0 ? -0.3 : 1.4); g.stroke(); g.restore(); }
}
const SWING = (sport, extra = {}) => Object.assign({ name: 'swing', sport, label: 'SWING!' }, extra);

/* =====================================================================
   🎾 SMASH TENNIS — Red vs Blue. Players run to the ball by themselves;
   you just swing when it reaches you. Timing aims (early = left, late = right).
   Perfect timing + hard swing = flaming SMASH.
   ===================================================================== */
class SmashTennis {
  constructor(c) { this.c = c; this.fx = new Particles(); this.pop = new PopText(); this.cam = new Cam3D({ x: 0, y: 7, z: -12, pitch: 0.17, f: 1000, cx: 800, cy: 360 }); this.t = 0; this.ts = 1; this.excite = 0; this.score = [0, 0]; this.WIN = 5; this.T = 180; this.trail = []; this.flash = 0; }
  start() {
    this.ps = this.c.roster(2);
    const hs = this.ps.filter(p => !p.bot), bs = this.ps.filter(p => p.bot), order = [...shuffle(hs), ...bs];
    this.teams = [[], []]; order.forEach((p, i) => this.teams[i % 2].push(p));
    this.pl = new Map(); this.hits = new Map(this.ps.map(p => [p, 0]));
    this.teams.forEach((tm, ti) => tm.forEach((p, i) => { const lx = -3.3 + (i + 0.5) * (6.6 / tm.length); this.pl.set(p, { p, team: ti, home: lx, x: lx, z: ti === 0 ? -0.6 : 24.6, sw: 0, cool: 0, botAt: null }); }));
    this.c.setLayout('all', SWING('tennis', { hint: 'Swing when the ball reaches YOU (your ring glows)' }));
    for (const p of this.ps) if (!p.bot) this.c.send(p, { t: 'toast', text: this.pl.get(p).team === 0 ? '🔴 You are on RED (front)' : '🔵 You are on BLUE (back)' });
    this.cd = new Countdown(this.c); this.phase = 'cd'; this.serveTeam = 0;
  }
  lanes(team) { return this.teams[team].map(p => this.pl.get(p)); }
  newServe() {
    const tm = this.lanes(this.serveTeam); this.server = pick(tm); this.phase = 'serve'; this.wait = this.server.p.bot ? rnd(0.9, 1.4) : 5;
    this.ball = null; this.trail = [];
    this.pop.add(`${this.server.p.name} SERVES!`, 800, 520, this.server.p.color, 54, 1.6);
    if (!this.server.p.bot) this.c.send(this.server.p, { t: 'buzz', ms: 120 });
  }
  launch(from, power, timing) {
    const other = 1 - from.team, tgts = this.lanes(other).sort((a, b) => a.home - b.home);
    // timing -1 (early) .. +1 (late) picks the lane; from the far side "left" is mirrored
    let f = clamp(0.5 + (from.team === 0 ? 1 : -1) * timing * 0.6 + rnd(-0.2, 0.2), 0, 1);
    const to = tgts[Math.round(f * (tgts.length - 1))];
    const smash = !!this.smashNext; this.smashNext = false;
    const fz = from.z, near = other === 0, bz = near ? rnd(3.5, 7.5) : rnd(16.5, 20.5), tz = to.z;
    const T = (smash ? 0.62 : 1) * (1.85 - power * 0.45);
    this.ball = { fx: from.x, fy: 1.1, fz, bx: clamp(lerp(from.x, to.home, 0.78) + rnd(-0.4, 0.4), -4, 4), bz, tx: to.home, ty: 1.05, tz, t: 0, t1: T * 0.68, t2: T * 0.32, h1: smash ? 0.75 : rnd(1.7, 2.4), h2: smash ? 0.55 : 1.1, rcv: to, by: from, smash, bounced: false, done: false };
    to.botAt = to.p.bot ? (Math.random() < (smash ? 0.4 : 0.13) ? null : T + rnd(-0.17, 0.17)) : null;
    this.phase = 'rally';
  }
  ballPos() {
    const b = this.ball, t = b.t;
    if (t < b.t1) { const u = t / b.t1; return { x: lerp(b.fx, b.bx, u), y: lerp(b.fy, 0.08, u) + 4 * b.h1 * u * (1 - u), z: lerp(b.fz, b.bz, u) }; }
    const u = (t - b.t1) / b.t2; return { x: lerp(b.bx, b.tx, u), y: Math.max(0.08, lerp(0.08, b.ty, u) + 4 * b.h2 * u * (1 - u)), z: lerp(b.bz, b.tz, u) };
  }
  onAct(p, m) { if (m && m.a === 'swing') this.swing(this.pl.get(p), clamp(+m.p || 0.6, 0.2, 1)); }
  onButton(p, b) { if (b === 'a' && !p.bot) this.swing(this.pl.get(p), 0.6); }
  swing(pl, power) {
    if (!pl || pl.cool > 0 || this.over) return;
    pl.sw = 1; pl.cool = 0.32; this.c.sfx.whoosh();
    if (this.phase === 'serve' && pl === this.server) { this.hitFx(pl, power, false); this.launch(pl, power, rnd(-0.6, 0.6)); return; }
    if (this.phase !== 'rally' || !this.ball || this.ball.done || this.ball.rcv !== pl) return;
    const dt = this.ball.t - (this.ball.t1 + this.ball.t2);
    if (Math.abs(dt) <= 0.27) {
      const perfect = Math.abs(dt) < 0.085; this.hits.set(pl.p, this.hits.get(pl.p) + 1);
      this.smashNext = perfect && power >= 0.68; this.hitFx(pl, power, this.smashNext, perfect);
      this.launch(pl, power, clamp(dt / 0.27, -1, 1));
    } else if (dt < 0) { this.pop.add('TOO EARLY!', this.scr(pl).x, this.scr(pl).y - 120, '#ff922b', 36, 0.8); }
  }
  scr(pl) { return this.cam.p(pl.x, 1, pl.z); }
  hitFx(pl, power, smash, perfect) {
    const s = this.scr(pl);
    this.fx.burst(s.x, s.y - 40, smash ? 60 : 18, smash ? '#ff922b' : '#fff', smash ? 520 : 260, 0.7, smash ? 8 : 5);
    if (smash) { this.pop.add('🔥 SMASH!! 🔥', 800, 360, '#ff922b', 120, 1.4); this.c.shake(26); this.c.sfx.boom(); this.flash = 0.35; this.excite = 1; }
    else { if (perfect) this.pop.add('PERFECT!', s.x, s.y - 140, '#51cf66', 44, 0.9); this.c.shake(5); this.c.sfx.hit(); }
  }
  point(team, why) {
    if (this.phase === 'point' || this.over) return;
    this.score[team]++; this.phase = 'point'; this.wait = 2.4; this.excite = 1; this.ts = 1;
    this.pop.add(why, 800, 400, '#ff4d6d', 80, 1.6);
    this.pop.add(`${team === 0 ? 'RED' : 'BLUE'} SCORES!`, 800, 500, team === 0 ? '#ff4d6d' : '#4dabff', 60, 1.8);
    fireworks(this.fx, 3); this.c.sfx.win();
    this.serveTeam = 1 - this.serveTeam;
    if (this.score[team] >= this.WIN) { this.winTeam = team; this.wait = 3.2; this.fx.confetti(200); }
  }
  update(dt) {
    this.t += dt; this.fx.update(dt); this.pop.update(dt); this.flash = Math.max(0, this.flash - dt); this.excite = Math.max(0, this.excite - dt * 0.5);
    for (const pl of this.pl.values()) { pl.sw = Math.max(0, pl.sw - dt * 3.2); pl.cool -= dt; }
    if (this.over) return;
    if (this.phase === 'cd') { if (this.cd.update(dt)) this.newServe(); return; }
    this.T -= dt;
    const sdt = dt * this.ts;
    // players drift toward where the ball will be (or home)
    for (const pl of this.pl.values()) { const tx = this.ball && !this.ball.done && this.ball.rcv === pl ? this.ball.tx : pl.home; pl.x += (tx - pl.x) * Math.min(1, dt * 5); }
    if (this.phase === 'serve') { this.wait -= dt; if (this.wait <= 0) this.swing(this.server, rnd(0.45, 0.7)); return; }
    if (this.phase === 'point') { this.wait -= dt; if (this.wait <= 0) { if (this.winTeam != null || this.T <= 0) return this.end(); this.newServe(); } return; }
    if (this.phase === 'rally' && this.ball) {
      const b = this.ball; b.t += sdt;
      const arr = b.t1 + b.t2, matchPt = Math.max(...this.score) >= this.WIN - 1;
      this.ts += ((!b.rcv.p.bot && (matchPt || b.smash) && Math.abs(b.t - arr) < 0.35 ? 0.35 : 1) - this.ts) * Math.min(1, dt * 10);
      if (!b.bounced && b.t >= b.t1) { b.bounced = true; const q = this.cam.p(b.bx, 0, b.bz); this.fx.puff(q.x, q.y, 'rgba(255,255,255,.7)', 10, 0.4); this.c.sfx.tink(); }
      if (b.rcv.p.bot && b.rcv.botAt != null && b.t >= b.rcv.botAt) { b.rcv.botAt = null; this.swing(b.rcv, rnd(0.45, 0.9)); }
      const pos = this.ballPos(), sp = this.cam.p(pos.x, pos.y, pos.z);
      this.trail.push({ x: sp.x, y: sp.y, r: Math.max(2, 0.11 * sp.s) }); if (this.trail.length > (b.smash ? 16 : 9)) this.trail.shift();
      if (b.t > arr + 0.3 && this.ball === b) { b.done = true; this.point(1 - b.rcv.team, `${b.rcv.p.name} MISSED!`); }
    }
    if (this.T <= 0 && this.phase !== 'point') this.end();
  }
  end() {
    if (this.done) return; this.done = this.over = true;
    const win = this.score[0] === this.score[1] ? -1 : this.score[0] > this.score[1] ? 0 : 1;
    const key = p => (this.pl.get(p).team === win ? 1000 : 0) + this.hits.get(p);
    this.c.finish(rankBy(this.ps, key, p => `${this.pl.get(p).team === win ? '🏆 ' : ''}${this.hits.get(p)} hits`));
  }
  drawCourt(g) {
    const cam = this.cam;
    poly3(g, cam, [[-14, 0, -6], [14, 0, -6], [14, 0, 34], [-14, 0, 34]], '#1f7a4d');
    poly3(g, cam, [[-5.5, 0, -2], [5.5, 0, -2], [5.5, 0, 26], [-5.5, 0, 26]], '#3b5bdb');
    poly3(g, cam, [[-4.1, 0, 0], [4.1, 0, 0], [4.1, 0, 24], [-4.1, 0, 24]], '#4263eb');
    const W = 'rgba(255,255,255,.95)';
    for (const z of [0, 24]) line3(g, cam, [-4.1, 0, z], [4.1, 0, z], W, 3);
    for (const x of [-4.1, 4.1]) line3(g, cam, [x, 0, 0], [x, 0, 24], W, 3);
    for (const z of [6.4, 17.6]) line3(g, cam, [-4.1, 0, z], [4.1, 0, z], W, 2);
    line3(g, cam, [0, 0, 6.4], [0, 0, 17.6], W, 2);
  }
  drawNet(g) {
    const cam = this.cam;
    poly3(g, cam, [[-5, 0, 12], [5, 0, 12], [5, 0.95, 12], [-5, 0.95, 12]], 'rgba(10,10,20,.45)');
    for (let x = -5; x <= 5; x += 0.5) line3(g, cam, [x, 0, 12], [x, 0.95, 12], 'rgba(255,255,255,.25)', 1);
    for (let y = 0.2; y < 0.95; y += 0.2) line3(g, cam, [-5, y, 12], [5, y, 12], 'rgba(255,255,255,.2)', 1);
    line3(g, cam, [-5, 0.97, 12], [5, 0.97, 12], '#fff', 5);
    for (const x of [-5, 5]) line3(g, cam, [x, 0, 12], [x, 1.1, 12], '#ced4da', 6);
  }
  drawPlayer(g, pl) {
    const cam = this.cam, s = cam.p(pl.x, 0.9, pl.z), r = 0.62 * s.s, team = pl.team === 0 ? '#ff4d6d' : '#4dabff';
    shadow3(g, cam, pl.x, pl.z, 0.7);
    const isR = this.phase === 'rally' && this.ball && !this.ball.done && this.ball.rcv === pl;
    if (isR && !pl.p.bot) { // timing ring: closes in exactly when you should swing
      const left = this.ball.t1 + this.ball.t2 - this.ball.t, k = clamp(left / 1.2, 0, 1), q = cam.p(pl.x, 0.02, pl.z);
      g.strokeStyle = left < 0.09 && left > -0.09 ? '#51cf66' : '#ffd43b'; g.lineWidth = 6;
      g.beginPath(); g.ellipse(q.x, q.y, (0.9 + k * 2.6) * q.s, (0.9 + k * 2.6) * q.s * 0.35, 0, 0, TAU); g.stroke();
      g.strokeStyle = 'rgba(255,255,255,.5)'; g.lineWidth = 3; g.beginPath(); g.ellipse(q.x, q.y, 0.9 * q.s, 0.9 * q.s * 0.35, 0, 0, TAU); g.stroke();
    }
    const bounce = Math.abs(Math.sin(this.t * 9 + pl.home)) * r * 0.12;
    drawBlob(g, s.x, s.y - bounce, r, pl.p.color, pl.team === 0 ? -Math.PI / 2 : Math.PI / 2, { ring: team, squish: pl.sw * 0.6, happy: this.phase === 'point', name: r > 22 ? pl.p.name : null });
    drawGear(g, s.x, s.y - bounce, r * 1.6, team, 'racket', pl.sw, pl.team === 0 ? 1 : -1);
    if (r <= 22) txt(g, pl.p.name, s.x, s.y - r - 16, 15, pl.p.color, 'center', 'Nunito');
  }
  draw(g) {
    const hy = this.cam.horizon();
    stadium(g, this.t, hy, this.excite);
    this.drawCourt(g);
    const far = [...this.pl.values()].filter(p => p.team === 1), near = [...this.pl.values()].filter(p => p.team === 0);
    far.forEach(pl => this.drawPlayer(g, pl));
    const pos = this.ball && !this.ball.done ? this.ballPos() : null;
    const drawBall = () => { if (!pos) return; shadow3(g, this.cam, pos.x, pos.z, 0.14, 0.5); trail(g, this.trail, this.ball.smash); ball3(g, this.cam, pos.x, pos.y, pos.z, 0.13, '#e8ff3b', this.ball.smash ? 'rgba(255,120,0,.9)' : 'rgba(232,255,59,.35)'); };
    if (pos && pos.z > 12) drawBall();
    this.drawNet(g);
    if (pos && pos.z <= 12) drawBall();
    near.forEach(pl => this.drawPlayer(g, pl));
    // scoreboard
    g.fillStyle = 'rgba(10,6,30,.82)'; rr(g, 560, 18, 480, 92, 22); g.fill(); g.lineWidth = 3; g.strokeStyle = 'rgba(255,255,255,.15)'; g.stroke();
    txt(g, 'RED', 650, 46, 24, '#ff4d6d'); txt(g, 'BLUE', 950, 46, 24, '#4dabff');
    txt(g, String(this.score[0]), 700, 86, 52, '#fff'); txt(g, '–', 800, 84, 40, '#adb5bd'); txt(g, String(this.score[1]), 900, 86, 52, '#fff');
    txt(g, `first to ${this.WIN}`, 800, 40, 16, '#adb5bd', 'center', 'Nunito');
    clock(g, this.T);
    if (this.ts < 0.8) { g.fillStyle = `rgba(255,255,255,${(0.8 - this.ts) * 0.12})`; g.fillRect(0, 0, 1600, 900); txt(g, 'SLOW-MO', 120, 860, 30, '#ffd43b'); }
    if (this.flash > 0) { g.fillStyle = `rgba(255,200,120,${this.flash})`; g.fillRect(0, 0, 1600, 900); }
    if (this.winTeam != null && this.phase === 'point') txt(g, `${this.winTeam === 0 ? '🔴 RED' : '🔵 BLUE'} WINS!`, 800, 640, 96, this.winTeam === 0 ? '#ff4d6d' : '#4dabff');
    this.fx.draw(g); this.pop.draw(g);
    if (this.phase === 'cd') this.cd.draw(g);
  }
}

/* =====================================================================
   🎳 STRIKE BOWLING — take turns. Tilt / drag to aim, swing to roll.
   Camera chases the ball down the lane. Strike = 20, spare = 15.
   ===================================================================== */
class StrikeBowling {
  constructor(c) { this.c = c; this.fx = new Particles(); this.pop = new PopText(); this.cam = new Cam3D({ x: 0, y: 1.6, z: -3.2, pitch: 0.12, f: 1100, cx: 800, cy: 420 }); this.t = 0; this.ts = 1; this.excite = 0; this.flash = 0; this.hist = []; }
  start() {
    this.ps = this.c.roster(1); this.frames = this.ps.length > 4 ? 2 : 3;
    this.pts = new Map(this.ps.map(p => [p, 0])); this.strikes = new Map(this.ps.map(p => [p, 0]));
    this.turns = []; for (let f = 0; f < this.frames; f++) for (const p of this.ps) this.turns.push(p);
    this.ti = -1; this.c.setLayout('all', { name: 'msg', emoji: '🎳', text: 'Bowling!', sub: 'Wait for your turn' }); this.nextTurn();
  }
  setPins() { this.pins = []; for (let r = 0; r < 4; r++) for (let i = 0; i <= r; i++) this.pins.push({ x: (i - r / 2) * 0.305, z: 18.3 + r * 0.264, vx: 0, vz: 0, down: false, fall: 0, rot: 0 }); }
  nextTurn() {
    this.ti++; if (this.ti >= this.turns.length) return this.end();
    this.cur = this.turns[this.ti]; this.roll = 0; this.setPins(); this.frameNo = Math.floor(this.ti / this.ps.length) + 1;
    this.aimPhase();
  }
  aimPhase() {
    this.phase = 'aim'; this.wait = this.cur.bot ? rnd(1.6, 2.6) : 14; this.ball = null; this.aim = 0; this.hist = [];
    this.cam.z = -3.2; this.cam.y = 1.6; this.cam.pitch = 0.12;
    for (const p of this.ps) if (!p.bot) this.c.setLayout(p, p === this.cur ? SWING('bowling', { aim: true, label: 'ROLL!', hint: 'Tilt or drag to aim, then swing!' }) : { name: 'msg', emoji: '🎳', text: `${this.cur.name} is bowling`, sub: 'Watch the TV!' });
    this.pop.add(`${this.cur.name} · ${this.roll === 0 ? `FRAME ${this.frameNo}/${this.frames}` : '2ND BALL'}`, 800, 260, this.cur.color, 52, 1.6);
  }
  onAct(p, m) { if (m && m.a === 'swing' && p === this.cur) this.release(clamp(+m.p || 0.6, 0.2, 1)); }
  onButton(p, b) { if (b === 'a' && p === this.cur && !p.bot) this.release(0.6); }
  release(power) {
    if (this.phase !== 'aim') return;
    // use the aim from a moment BEFORE the swing (swinging the phone tilts it)
    const now = this.t, old = this.hist.find(h => h[0] >= now - 0.4); const aim = old ? old[1] : this.aim;
    const x = clamp(aim, -1, 1) * 0.42, spd = 7 + power * 9;
    this.ball = { x, z: 0.2, vx: -x * 0.09 + rnd(-0.02, 0.02), vz: spd, spin: 0, gutter: false, rot: 0 };
    this.phase = 'roll'; this.rollT = 0; this.c.sfx.whoosh(); this.c.shake(4);
    if (!this.cur.bot) this.c.setLayout(this.cur, { name: 'msg', emoji: '🎳', text: 'Rolling…', sub: power > 0.8 ? 'Monster throw! 💪' : 'Fingers crossed!' });
  }
  update(dt) {
    this.t += dt; this.fx.update(dt); this.pop.update(dt); this.flash = Math.max(0, this.flash - dt); this.excite = Math.max(0, this.excite - dt * 0.4);
    if (this.over) return;
    if (this.phase === 'aim') {
      const inp = this.cur.input || { x: 0 };
      this.aim = this.cur.bot ? Math.sin(this.t * 1.6) * 0.5 : clamp(inp.x || 0);
      this.hist.push([this.t, this.aim]); if (this.hist.length > 60) this.hist.shift();
      this.wait -= dt; if (this.wait <= 0) this.release(this.cur.bot ? rnd(0.5, 1) : 0.5);
      return;
    }
    if (this.phase === 'roll') {
      const sdt = dt * this.ts, b = this.ball; this.rollT += dt;
      b.z += b.vz * sdt; b.x += b.vx * sdt; b.rot += b.vz * sdt * 3;
      if (!b.gutter && b.z < 17.6 && Math.abs(b.x) > 0.53) { b.gutter = true; b.x = Math.sign(b.x) * 0.62; b.vx = 0; this.pop.add('GUTTER!', 800, 300, '#adb5bd', 70, 1.2); this.c.sfx.buzz(); }
      // pins
      for (const pn of this.pins) {
        if (!b.gutter && b.z < 21) { const dx = pn.x - b.x, dz = pn.z - b.z, d = Math.hypot(dx, dz); if (d < 0.17 && d > 0) { const k = Math.min(6, b.vz * 0.45); pn.vx += dx / d * k + b.vx * 0.3; pn.vz += dz / d * k + b.vz * 0.25; b.vx -= dx / d * 0.35; b.vz *= 0.96; this.knock(pn); } }
        if (Math.hypot(pn.vx, pn.vz) > 0.05) for (const o of this.pins) { if (o === pn) continue; const dx = o.x - pn.x, dz = o.z - pn.z, d = Math.hypot(dx, dz); if (d < 0.13 && d > 0) { const sp = Math.min(5, Math.hypot(pn.vx, pn.vz) * 0.75); o.vx += dx / d * sp; o.vz += dz / d * sp; pn.vx *= 0.7; pn.vz *= 0.7; this.knock(o); } }
      }
      // ball past the pins: let it drop into the pit
      if (b.z > 17.6 && Math.abs(b.x) > 0.62) { b.x = Math.sign(b.x) * 0.62; b.vx = 0; }
      for (const pn of this.pins) { pn.x += pn.vx * sdt; pn.z += pn.vz * sdt;
        if (Math.abs(pn.x) > 0.68) { pn.x = Math.sign(pn.x) * 0.68; pn.vx *= -0.4; } // side kickbacks
        if (pn.z > 21.1) { pn.z = 21.1; pn.vz = 0; pn.vx *= 0.5; } const k = Math.pow(0.12, sdt); pn.vx *= k; pn.vz *= k; if (pn.down) { pn.fall = Math.min(1, pn.fall + sdt * 5); pn.rot += sdt * (pn.vx * 4); } }
      // camera chases the ball, slows down at the pins for drama
      const tz = Math.min(b.z - 3.4, 14.2); this.cam.z += (tz - this.cam.z) * Math.min(1, dt * 4); this.cam.y += (1.15 - this.cam.y) * Math.min(1, dt * 2);
      const nearPins = b.z > 16.5 && b.z < 19.5 && !b.gutter && this.roll === 0;
      this.ts += ((nearPins ? 0.3 : 1) - this.ts) * Math.min(1, dt * 8);
      const moving = this.pins.some(p => Math.hypot(p.vx, p.vz) > 0.06);
      if ((b.z > 21 && !moving) || this.rollT > 7) this.score();
    }
    if (this.phase === 'show') { this.wait -= dt; if (this.wait <= 0) { if (this.again) this.aimPhase(); else this.nextTurn(); } }
  }
  knock(pn) { if (pn.down) return; pn.down = true; this.c.sfx.bonk(); const q = this.cam.p(pn.x, 0.2, pn.z); this.fx.burst(q.x, q.y, 6, '#fff', 200, 0.4, 4); }
  score() {
    this.ts = 1;
    const newly = this.pins.filter(p => p.down).length, before = this.roll === 0 ? 0 : this.firstDown, down = before + newly; let gain = newly, msg = `${newly} PIN${newly === 1 ? '' : 'S'}`, color = '#fff';
    this.again = false;
    if (this.roll === 0 && down === 10) { gain = 20; msg = '💥 STRIKE!! 💥'; color = '#ffd43b'; this.strikes.set(this.cur, this.strikes.get(this.cur) + 1); this.flash = 0.5; this.excite = 1; this.c.shake(30); this.c.sfx.boom(); fireworks(this.fx, 6); this.fx.confetti(160); }
    else if (this.roll === 1 && down === 10) { gain = 15 - before; msg = '✨ SPARE! ✨'; color = '#51cf66'; this.excite = 0.8; fireworks(this.fx, 3); this.c.sfx.win(); }
    else if (this.roll === 0) { this.again = true; this.firstDown = down; this.c.sfx.ding(); if (down === 0) msg = 'OOPS… 0'; }
    this.pts.set(this.cur, this.pts.get(this.cur) + gain);
    this.pop.add(msg, 800, 300, color, down === 10 ? 110 : 70, 2);
    if (gain) this.pop.add(`+${gain}`, 800, 420, this.cur.color, 60, 2);
    if (this.again) { this.pins = this.pins.filter(p => !p.down); this.roll = 1; this.pins.forEach(p => { p.vx = p.vz = 0; }); }
    this.phase = 'show'; this.wait = down === 10 ? 3 : 2.2;
  }
  end() { if (this.done) return; this.done = this.over = true; this.c.finish(rankBy(this.ps, p => this.pts.get(p) * 10 + this.strikes.get(p), p => `${this.pts.get(p)} pts${this.strikes.get(p) ? ` · ${this.strikes.get(p)}× strike` : ''}`)); }
  drawPin(g, pn) {
    const cam = this.cam, base = cam.p(pn.x, 0, pn.z), s = base.s;
    if (base.z < 0.3) return;
    g.save(); g.translate(base.x, base.y);
    if (pn.down) { g.rotate((pn.rot || 0.001) > 0 ? pn.fall * 1.45 : -pn.fall * 1.45); }
    const h = 0.38 * s, w = 0.06 * s;
    g.fillStyle = '#f8f9fa'; g.beginPath(); g.ellipse(0, -h * 0.28, w * 1.25, h * 0.3, 0, 0, TAU); g.fill();
    g.beginPath(); g.ellipse(0, -h * 0.78, w * 0.75, h * 0.2, 0, 0, TAU); g.fill();
    g.fillStyle = '#e03131'; g.fillRect(-w * 0.7, -h * 0.62, w * 1.4, h * 0.06); g.fillRect(-w * 0.7, -h * 0.54, w * 1.4, h * 0.04);
    g.strokeStyle = 'rgba(0,0,0,.25)'; g.lineWidth = 1.5; g.beginPath(); g.ellipse(0, -h * 0.28, w * 1.25, h * 0.3, 0, 0, TAU); g.stroke();
    g.restore();
  }
  draw(g) {
    const cam = this.cam, hy = cam.horizon();
    // neon bowling alley
    const gr = g.createLinearGradient(0, 0, 0, 900); gr.addColorStop(0, '#0b0620'); gr.addColorStop(1, '#2b0b4f'); g.fillStyle = gr; g.fillRect(0, 0, 1600, 900);
    g.save(); g.globalCompositeOperation = 'lighter';
    for (let i = 0; i < 7; i++) { const x = 100 + i * 233, c = ['#ff4d6d', '#4dabff', '#ffd43b', '#cc5de8', '#51cf66', '#22d3ee', '#ff922b'][i]; g.strokeStyle = c; g.globalAlpha = 0.35 + 0.35 * Math.abs(Math.sin(this.t * 2 + i)) + this.excite * 0.3; g.lineWidth = 6; g.beginPath(); g.moveTo(x, 0); g.lineTo(800 + (x - 800) * 0.15, hy + 30); g.stroke(); }
    g.restore(); g.globalAlpha = 1;
    txt(g, 'STRIKE ZONE', 800, 70, 54, `hsl(${(this.t * 80) % 360},100%,65%)`);
    // lane, gutters, pin deck
    poly3(g, cam, [[-3, 0, -4], [3, 0, -4], [3, 0, 26], [-3, 0, 26]], '#140b2b');
    poly3(g, cam, [[-0.75, 0, -1], [-0.53, 0, -1], [-0.53, 0, 22], [-0.75, 0, 22]], '#2b2b3a');
    poly3(g, cam, [[0.53, 0, -1], [0.75, 0, -1], [0.75, 0, 22], [0.53, 0, 22]], '#2b2b3a');
    poly3(g, cam, [[-0.53, 0, -1], [0.53, 0, -1], [0.53, 0, 21], [-0.53, 0, 21]], '#d9a066');
    for (let i = -10; i <= 10; i += 2) line3(g, cam, [i * 0.053, 0.001, -1], [i * 0.053, 0.001, 21], 'rgba(120,70,20,.25)', 1);
    line3(g, cam, [-0.53, 0.002, 0], [0.53, 0.002, 0], '#e03131', 3);
    for (let i = -3; i <= 3; i++) { const z = 4.6 + Math.abs(i) * 0.25; poly3(g, cam, [[i * 0.13 - 0.03, 0.003, z], [i * 0.13 + 0.03, 0.003, z], [i * 0.13, 0.003, z + 0.3]], '#7b2cbf'); }
    poly3(g, cam, [[-0.75, 0, 21], [0.75, 0, 21], [0.75, 1.2, 21.3], [-0.75, 1.2, 21.3]], '#0a0614');
    // aim guide
    if (this.phase === 'aim') {
      const x = clamp(this.aim, -1, 1) * 0.42;
      for (let z = 1; z < 17; z += 0.8) { const q = cam.p(x - x * 0.09 * (z / 17) * 0, 0.01, z); g.fillStyle = `rgba(255,212,59,${0.8 - z / 25})`; g.beginPath(); g.arc(q.x, q.y, Math.max(2, 0.03 * q.s), 0, TAU); g.fill(); }
      const q = cam.p(x, 0.11, 0.2); txt(g, '▲', q.x, q.y + 40, 30, '#ffd43b');
      txt(g, this.cur.bot ? `${this.cur.name} is aiming…` : `${this.cur.name}: aim, then SWING!`, 800, 830, 40, this.cur.color);
    }
    const items = this.pins.map(p => ({ z: p.z, d: () => this.drawPin(g, p) }));
    if (this.ball) items.push({ z: this.ball.z, d: () => { const b = this.ball; shadow3(g, cam, b.x, b.z, 0.13, 0.45); const q = ball3(g, cam, b.x, 0.11, b.z, 0.11, this.cur.color); g.strokeStyle = 'rgba(0,0,0,.35)'; g.lineWidth = 2; g.beginPath(); g.arc(q.x, q.y, Math.max(2, 0.11 * q.s * 0.6), b.rot, b.rot + 1.5); g.stroke(); } });
    else if (this.phase === 'aim') items.push({ z: 0.2, d: () => { const x = clamp(this.aim, -1, 1) * 0.42; shadow3(g, cam, x, 0.2, 0.13); ball3(g, cam, x, 0.11, 0.2, 0.11, this.cur.color, 'rgba(255,255,255,.2)'); } });
    items.sort((a, b) => b.z - a.z).forEach(i => i.d());
    // HUD
    board(g, rankBy(this.ps, p => this.pts.get(p), () => '').map(r => ({ name: r.p.name, color: r.p.color, val: this.pts.get(r.p) })), 20, 20, `FRAME ${Math.min(this.frameNo || 1, this.frames)}/${this.frames}`);
    if (this.ts < 0.8) { txt(g, 'SLOW-MO', 1480, 860, 30, '#ffd43b'); }
    if (this.flash > 0) { g.fillStyle = `rgba(255,240,180,${this.flash})`; g.fillRect(0, 0, 1600, 900); }
    this.fx.draw(g); this.pop.draw(g);
  }
}

/* =====================================================================
   ⚾ HOME RUN DERBY — everyone bats at the SAME pitch. Swing when the
   ball reaches the plate; better timing + harder swing = farther.
   ===================================================================== */
class HomeRunDerby {
  constructor(c) { this.c = c; this.fx = new Particles(); this.pop = new PopText(); this.cam = new Cam3D({ x: 0, y: 1.5, z: -3.2, pitch: 0.05, f: 1150, cx: 800, cy: 470 }); this.t = 0; this.excite = 0; this.flash = 0; this.N = 8; this.n = 0; }
  start() {
    this.ps = this.c.roster(1); this.dist = new Map(this.ps.map(p => [p, 0])); this.hr = new Map(this.ps.map(p => [p, 0])); this.sw = new Map(this.ps.map(p => [p, 0]));
    this.c.setLayout('all', SWING('bat', { hint: 'Swing when the ball reaches the plate!' }));
    this.cd = new Countdown(this.c); this.phase = 'cd';
  }
  nextPitch() {
    this.n++; if (this.n > this.N) return this.end();
    const kind = this.n === this.N ? 'CRAZY BALL' : pick(['FASTBALL', 'CHANGE-UP', 'CURVEBALL', 'FASTBALL', 'SLIDER']);
    const T = { FASTBALL: 0.72, 'CHANGE-UP': 1.15, CURVEBALL: 0.95, SLIDER: 0.85, 'CRAZY BALL': 1.0 }[kind];
    this.pitch = { kind, T, t: -1.1, swings: new Map(), res: [] }; this.phase = 'pitch';
    this.cam.z = -3.2; this.cam.y = 1.5; this.cam.pitch = 0.05; this.cam.f = 1150; this.cam.cy = 470;
    for (const p of this.ps) if (p.bot) { const r = Math.random(); this.pitch.swings.set(p, { at: T + (r < 0.15 ? rnd(-0.6, 0.5) : rnd(-0.13, 0.13)), pow: rnd(0.5, 1), bot: true }); }
    this.pop.add(`PITCH ${this.n}/${this.N}: ${kind}`, 800, 200, kind === 'CRAZY BALL' ? '#ff4d6d' : '#ffd43b', 46, 1.4);
  }
  ballAt(t) {
    const P = this.pitch, u = clamp(t / P.T, 0, 1.3);
    let x = 0, y = lerp(1.9, 0.85, u), z = lerp(18.4, 0, u);
    if (P.kind === 'CURVEBALL') { y -= Math.sin(u * Math.PI / 2) * 0.35 * u; x = Math.sin(u * 2) * 0.25; }
    if (P.kind === 'SLIDER') x = u * u * 0.45;
    if (P.kind === 'CRAZY BALL') { x = Math.sin(u * 14) * 0.6 * (1 - u * 0.7); y += Math.cos(u * 11) * 0.35 * (1 - u * 0.6); }
    return { x, y, z };
  }
  onAct(p, m) { if (m && m.a === 'swing') this.swing(p, clamp(+m.p || 0.6, 0.2, 1)); }
  onButton(p, b) { if (b === 'a' && !p.bot) this.swing(p, 0.6); }
  swing(p, pow) {
    this.sw.set(p, 1); this.c.sfx.whoosh();
    if (this.phase !== 'pitch' || this.pitch.swings.has(p) || this.pitch.t < -0.2) return;
    this.pitch.swings.set(p, { at: this.pitch.t, pow });
  }
  resolve() {
    const P = this.pitch; P.res = [];
    for (const p of this.ps) {
      const s = P.swings.get(p); let d = 0, type = 'MISS';
      if (s) {
        const e = Math.abs(s.at - P.T);
        if (e < 0.17) {
          const q = 1 - e / 0.17; d = Math.round((q * 0.72 + s.pow * 0.28) * 150 + rnd(-6, 6));
          type = d >= 115 ? 'HOME RUN' : e > 0.13 ? 'FOUL' : 'HIT';
          if (type === 'FOUL') d = 0;
        } else type = s.at < P.T ? 'TOO EARLY' : 'TOO LATE';
      }
      this.dist.set(p, this.dist.get(p) + d); if (type === 'HOME RUN') this.hr.set(p, this.hr.get(p) + 1);
      const ang = rnd(-0.6, 0.6), flight = type === 'FOUL' ? { ang: (Math.random() < 0.5 ? -1 : 1) * rnd(0.85, 1.1), d: rnd(30, 60) } : { ang, d };
      P.res.push({ p, d, type, ang: flight.ang, fd: type === 'FOUL' ? flight.d : d, h: rnd(10, 16) + d * 0.06 });
      if (!p.bot) this.c.send(p, { t: 'toast', text: type === 'HOME RUN' ? `💥 HOME RUN! ${d} m` : d ? `${type}! ${d} m` : type });
    }
    const hrs = P.res.filter(r => r.type === 'HOME RUN');
    if (hrs.length) { this.pop.add(hrs.length > 1 ? `💥 ${hrs.length}× HOME RUN!! 💥` : `💥 HOME RUN ${hrs[0].p.name}! 💥`, 800, 260, '#ffd43b', 80, 2.2); this.flash = 0.45; this.c.shake(24); this.c.sfx.boom(); this.excite = 1; }
    else if (P.res.some(r => r.d)) { this.c.sfx.hit(); this.c.shake(8); this.excite = 0.6; }
    else { this.pop.add('STRIKE!', 800, 300, '#ff4d6d', 90, 1.4); this.c.sfx.buzz(); }
    this.phase = 'fly'; this.flyT = 0;
  }
  update(dt) {
    this.t += dt; this.fx.update(dt); this.pop.update(dt); this.flash = Math.max(0, this.flash - dt); this.excite = Math.max(0, this.excite - dt * 0.4);
    for (const p of this.ps) this.sw.set(p, Math.max(0, this.sw.get(p) - dt * 3));
    if (this.over) return;
    if (this.phase === 'cd') { if (this.cd.update(dt)) this.nextPitch(); return; }
    if (this.phase === 'pitch') {
      const P = this.pitch; P.t += dt;
      for (const [p, s] of P.swings) if (s.bot && !s.anim && P.t >= s.at) { s.anim = true; this.sw.set(p, 1); }
      if (P.t > P.T + 0.3) this.resolve();
      return;
    }
    if (this.phase === 'fly') {
      this.flyT += dt;
      // camera rises and looks out to the field
      const k = Math.min(1, this.flyT / 0.8); this.cam.y = lerp(1.5, 18, k); this.cam.z = lerp(-3.2, -14, k); this.cam.pitch = lerp(0.05, 0.12, k); this.cam.f = lerp(1150, 900, k); this.cam.cy = lerp(470, 330, k);
      if (Math.random() < dt * 4 && this.pitch.res.some(r => r.type === 'HOME RUN') && this.flyT > 1.3) fireworks(this.fx, 1);
      if (this.flyT > 3.8) this.nextPitch();
    }
  }
  end() { if (this.done) return; this.done = this.over = true; this.c.finish(rankBy(this.ps, p => this.dist.get(p) * 10 + this.hr.get(p), p => `${this.dist.get(p)} m${this.hr.get(p) ? ` · ${this.hr.get(p)} HR` : ''}`)); }
  drawField(g) {
    const cam = this.cam;
    poly3(g, cam, [[-200, 0, -10], [200, 0, -10], [200, 0, 220], [-200, 0, 220]], '#1e6b3a');
    // mowing stripes
    for (let z = 0; z < 130; z += 12) poly3(g, cam, [[-130, 0.001, z], [130, 0.001, z], [130, 0.001, z + 6], [-130, 0.001, z + 6]], 'rgba(255,255,255,.04)');
    poly3(g, cam, [[0, 0.002, -1.5], [20, 0.002, 18], [0, 0.002, 38], [-20, 0.002, 18]], '#b5651d');
    poly3(g, cam, [[0, 0.003, 1], [13, 0.003, 14], [0, 0.003, 27], [-13, 0.003, 14]], '#2b8a3e');
    poly3(g, cam, [[-0.25, 0.004, -0.2], [0.25, 0.004, -0.2], [0.25, 0.004, 0.2], [0, 0.004, 0.45], [-0.25, 0.004, 0.2]], '#fff');
    const mp = cam.p(0, 0.25, 18.4); g.fillStyle = '#c47a3a'; g.beginPath(); g.ellipse(mp.x, mp.y, 2.7 * mp.s, 0.6 * mp.s, 0, 0, TAU); g.fill();
    // outfield fence
    const pts = []; for (let a = -0.85; a <= 0.85; a += 0.05) pts.push([Math.sin(a) * 115, Math.cos(a) * 115]);
    for (let i = 1; i < pts.length; i++) { const [x1, z1] = pts[i - 1], [x2, z2] = pts[i]; poly3(g, cam, [[x1, 0, z1], [x2, 0, z2], [x2, 4.5, z2], [x1, 4.5, z1]], i % 2 ? '#0b3d6e' : '#0d4a85'); }
    const lbl = cam.p(0, 6, 115); if (lbl.z > 0) txt(g, '115 m', lbl.x, lbl.y, Math.max(14, 3.2 * lbl.s), '#ffd43b');
  }
  draw(g) {
    const cam = this.cam, hy = cam.horizon();
    stadium(g, this.t, hy, this.excite, '#0d1b4b', '#3b1f7a');
    this.drawField(g);
    if (this.phase === 'pitch' || this.phase === 'cd') {
      const P = this.pitch, pit = cam.p(0, 0.9, 18.6), r = Math.max(10, 0.7 * pit.s);
      const wind = P && P.t < 0 ? (P.t + 1.1) / 1.1 : 1;
      drawBlob(g, pit.x, pit.y - r * 0.3, r, '#adb5bd', Math.PI / 2, { squish: Math.sin(wind * Math.PI) * 0.6 });
      if (P && P.t >= 0) {
        const pos = this.ballAt(P.t), tr = []; for (let k = 6; k >= 0; k--) { const q = this.ballAt(Math.max(0, P.t - k * 0.03)), s = cam.p(q.x, q.y, q.z); tr.push({ x: s.x, y: s.y, r: Math.max(2, 0.04 * s.s) }); }
        trail(g, tr, P.kind === 'CRAZY BALL');
        if (pos.z > -2.5) ball3(g, cam, pos.x, pos.y, pos.z, 0.075, '#fff', P.kind === 'CRAZY BALL' ? 'rgba(255,80,120,.8)' : 'rgba(255,255,255,.3)');
        // timing ring around the plate
        const left = P.T - P.t, k = clamp(left / 0.9, 0, 1), pl = cam.p(0, 0.85, 0);
        g.strokeStyle = Math.abs(left) < 0.07 ? '#51cf66' : '#ffd43b'; g.lineWidth = 6; g.beginPath(); g.arc(pl.x, pl.y, 60 + k * 240, 0, TAU); g.stroke();
        g.strokeStyle = 'rgba(255,255,255,.5)'; g.lineWidth = 3; g.beginPath(); g.arc(pl.x, pl.y, 60, 0, TAU); g.stroke();
      }
    }
    if (this.phase === 'fly') {
      const u = clamp(this.flyT / 2.4, 0, 1);
      for (const r of this.pitch.res) {
        if (!r.fd) continue;
        const x = Math.sin(r.ang) * r.fd * u, z = Math.cos(r.ang) * r.fd * u, y = 1 + 4 * r.h * u * (1 - u);
        const tr = []; for (let k = 8; k >= 0; k--) { const uu = Math.max(0, u - k * 0.02), s = cam.p(Math.sin(r.ang) * r.fd * uu, 1 + 4 * r.h * uu * (1 - uu), Math.cos(r.ang) * r.fd * uu); tr.push({ x: s.x, y: s.y, r: Math.max(3, 0.9 * s.s) }); }
        trail(g, tr, r.type === 'HOME RUN');
        shadow3(g, cam, x, z, 1.1, 0.3);
        const q = ball3(g, cam, x, y, z, 1.0, r.p.color, r.type === 'HOME RUN' ? 'rgba(255,200,0,.8)' : 'rgba(255,255,255,.3)');
        txt(g, `${r.p.name} ${r.type === 'FOUL' ? 'FOUL' : r.d + ' m'}`, q.x, q.y - 26, 20, r.p.color, 'center', 'Nunito');
      }
      // result list
      const rows = [...this.pitch.res].sort((a, b) => b.d - a.d);
      g.fillStyle = 'rgba(10,6,30,.75)'; rr(g, 1250, 120, 330, rows.length * 38 + 20, 18); g.fill();
      rows.forEach((r, i) => { const y = 148 + i * 38; g.fillStyle = r.p.color; g.beginPath(); g.arc(1276, y, 9, 0, TAU); g.fill(); txt(g, r.p.name.slice(0, 10), 1294, y, 19, '#fff', 'left', 'Nunito'); txt(g, r.d ? `${r.d} m` : r.type, 1564, y, 18, r.type === 'HOME RUN' ? '#ffd43b' : r.d ? '#fff' : '#ff8787', 'right'); });
    }
    // batters lineup at the bottom
    const n = this.ps.length, w = Math.min(170, 1500 / n);
    this.ps.forEach((p, i) => {
      const x = 800 + (i - (n - 1) / 2) * w, y = 820, sw = this.sw.get(p), did = this.phase === 'pitch' && this.pitch.swings.has(p) && !this.pitch.swings.get(p).bot;
      drawBlob(g, x, y, 34, p.color, -Math.PI / 2, { squish: sw * 0.5, ring: did ? '#ffd43b' : null });
      drawGear(g, x, y, 54, p.color, 'bat', sw, 1);
      txt(g, p.name.slice(0, 9), x, y + 52, 17, '#fff', 'center', 'Nunito');
    });
    strip(g, rankBy(this.ps, p => this.dist.get(p), () => '').map(r => ({ name: r.p.name, color: r.p.color, val: this.dist.get(r.p) + 'm' })));
    txt(g, `PITCH ${Math.min(this.n, this.N)}/${this.N}`, 1500, 110, 22, '#adb5bd');
    if (this.flash > 0) { g.fillStyle = `rgba(255,240,180,${this.flash})`; g.fillRect(0, 0, 1600, 900); }
    this.fx.draw(g); this.pop.draw(g);
    if (this.phase === 'cd') this.cd.draw(g);
  }
}
