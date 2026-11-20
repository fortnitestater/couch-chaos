/* Couch Chaos — 🎾 Motion Sports in REAL 3D (three.js, WebGL).
   Phones send {a:'swing', p:power} through onAct, and aim with p.input.x (tilt / drag / arrow keys).
   The 3D scene is rendered to a WebGL canvas, then drawn onto the TV canvas with a 2D HUD on top. */

const lerp = (a, b, u) => a + (b - a) * u;
const has3D = () => typeof THREE !== 'undefined' && typeof document !== 'undefined';

/* ---------- shared renderer (one WebGL context for all games) ---------- */
const S3 = {
  r: null,
  renderer() {
    if (this.r) return this.r;
    const r = new THREE.WebGLRenderer({ antialias: true, preserveDrawingBuffer: true, powerPreference: 'high-performance' });
    r.setPixelRatio(1); r.setSize(1280, 720, false);
    r.shadowMap.enabled = true; r.shadowMap.type = THREE.PCFSoftShadowMap;
    r.outputEncoding = THREE.sRGBEncoding; r.toneMapping = THREE.ACESFilmicToneMapping; r.toneMappingExposure = 1.0;
    return (this.r = r);
  },
};
function canvasTex(w, h, draw, repeat) {
  const cv = document.createElement('canvas'); cv.width = w; cv.height = h; draw(cv.getContext('2d'), w, h);
  const t = new THREE.CanvasTexture(cv); t.encoding = THREE.sRGBEncoding; t.anisotropy = 4;
  if (repeat) { t.wrapS = t.wrapT = THREE.RepeatWrapping; t.repeat.set(repeat[0], repeat[1]); }
  return t;
}
let GLOW_TEX = null;
function glowTex() {
  if (GLOW_TEX) return GLOW_TEX;
  return (GLOW_TEX = canvasTex(64, 64, (g) => { const gr = g.createRadialGradient(32, 32, 0, 32, 32, 32); gr.addColorStop(0, 'rgba(255,255,255,1)'); gr.addColorStop(0.35, 'rgba(255,255,255,.6)'); gr.addColorStop(1, 'rgba(255,255,255,0)'); g.fillStyle = gr; g.fillRect(0, 0, 64, 64); }));
}
// colors are written in sRGB (like CSS); three r128 wants linear, otherwise everything looks pale
const lin = c => new THREE.Color(c).convertSRGBToLinear();
const mat = (color, o = {}) => { const m = new THREE.MeshStandardMaterial(Object.assign({ roughness: 0.55, metalness: 0.05 }, o)); m.color = lin(color); if (o.emissive != null) m.emissive = lin(o.emissive); return m; };
function shadowAll(obj, cast = true, recv = false) { obj.traverse(m => { if (m.isMesh) { m.castShadow = cast; m.receiveShadow = recv; } }); return obj; }

/* ---------- 3D particles: sparks, fire, fireworks, confetti ---------- */
class Burst3D {
  constructor(scene, max = 1800) {
    this.max = max; this.n = 0; this.p = [];
    const geo = new THREE.BufferGeometry();
    this.pos = new Float32Array(max * 3); this.col = new Float32Array(max * 3);
    geo.setAttribute('position', new THREE.BufferAttribute(this.pos, 3)); geo.setAttribute('color', new THREE.BufferAttribute(this.col, 3));
    this.geo = geo;
    this.pts = new THREE.Points(geo, new THREE.PointsMaterial({ size: 0.3, map: glowTex(), vertexColors: true, transparent: true, depthWrite: false, blending: THREE.AdditiveBlending }));
    this.pts.frustumCulled = false; scene.add(this.pts);
  }
  burst(x, y, z, n, color, spd = 8, life = 1, grav = 9) {
    const c = new THREE.Color(color);
    for (let i = 0; i < n && this.p.length < this.max; i++) {
      const u = Math.random() * 2 - 1, a = Math.random() * TAU, s = Math.sqrt(1 - u * u), v = spd * (0.3 + Math.random() * 0.7);
      this.p.push({ x, y, z, vx: Math.cos(a) * s * v, vy: u * v + spd * 0.3, vz: Math.sin(a) * s * v, life: life * (0.6 + Math.random() * 0.5), max: life, c, grav });
    }
  }
  firework(x, y, z) { const cols = [0xff4d6d, 0xffd43b, 0x4dabff, 0x51cf66, 0xcc5de8, 0xffffff]; this.burst(x, y, z, 90, pick(cols), 14, 1.6, 4); }
  update(dt) {
    const k = Math.pow(0.35, dt);
    for (const q of this.p) { q.vy -= q.grav * dt; q.vx *= k; q.vz *= k; q.x += q.vx * dt; q.y += q.vy * dt; q.z += q.vz * dt; q.life -= dt; }
    this.p = this.p.filter(q => q.life > 0);
    let i = 0;
    for (const q of this.p) { const f = Math.min(1, q.life / q.max * 1.5); this.pos[i * 3] = q.x; this.pos[i * 3 + 1] = q.y; this.pos[i * 3 + 2] = q.z; this.col[i * 3] = q.c.r * f; this.col[i * 3 + 1] = q.c.g * f; this.col[i * 3 + 2] = q.c.b * f; i++; }
    this.geo.setDrawRange(0, i); this.geo.attributes.position.needsUpdate = true; this.geo.attributes.color.needsUpdate = true;
  }
}

/* ---------- glowing comet trail behind a ball ---------- */
class Trail3D {
  constructor(scene, n = 16) {
    this.n = n; this.h = []; this.s = [];
    for (let i = 0; i < n; i++) { const sp = new THREE.Sprite(new THREE.SpriteMaterial({ map: glowTex(), color: 0xffffff, transparent: true, depthWrite: false, blending: THREE.AdditiveBlending })); sp.visible = false; scene.add(sp); this.s.push(sp); }
  }
  push(x, y, z) { this.h.push([x, y, z]); if (this.h.length > this.n) this.h.shift(); }
  clear() { this.h = []; this.s.forEach(s => (s.visible = false)); }
  draw(fire, size = 0.35, color = 0xd8f5ff) {
    this.s.forEach((sp, i) => {
      const p = this.h[this.h.length - 1 - i]; if (!p) { sp.visible = false; return; }
      const k = 1 - i / this.n; sp.visible = true; sp.position.set(p[0], p[1], p[2]); sp.scale.setScalar(size * (fire ? 2.4 : 1) * k + 0.05);
      sp.material.color.set(fire ? new THREE.Color().setHSL(0.02 + k * 0.1, 1, 0.45 + k * 0.2) : color); sp.material.opacity = k * (fire ? 1 : 0.6);
    });
  }
}

/* ---------- stadium: stands full of a bouncing crowd, light towers, sweeping colored beams ---------- */
function buildStadium(scene, o) {
  // o: {x0,x1,z0,z1} field rectangle the stands surround, rows, sides: which sides get stands
  const g = new THREE.Group(); scene.add(g);
  const standMat = mat(0x221a44, { roughness: 0.9 }), seats = [];
  const sides = o.sides || ['l', 'r', 'f', 'b'];
  for (const sd of sides) {
    for (let r = 0; r < (o.rows || 8); r++) {
      const h = 0.9 + r * 0.9, off = 2 + r * 1.3;
      let geo, x, z, len, horiz;
      if (sd === 'l' || sd === 'r') { len = o.z1 - o.z0 + off * 2; geo = new THREE.BoxGeometry(1.3, h, len); x = sd === 'l' ? o.x0 - off : o.x1 + off; z = (o.z0 + o.z1) / 2; horiz = false; }
      else { len = o.x1 - o.x0 + off * 2; geo = new THREE.BoxGeometry(len, h, 1.3); x = (o.x0 + o.x1) / 2; z = sd === 'f' ? o.z1 + off : o.z0 - off; horiz = true; }
      const m = new THREE.Mesh(geo, standMat); m.position.set(x, h / 2, z); m.receiveShadow = true; g.add(m);
      const n = Math.floor(len / 1.1);
      for (let i = 0; i < n; i++) { const t = (i + 0.5) / n - 0.5; seats.push(horiz ? [x + t * len, h + 0.35, z] : [x, h + 0.35, z + t * len]); }
    }
  }
  const cols = [0xff4d6d, 0xffd43b, 0x4dabff, 0x51cf66, 0xcc5de8, 0xff922b, 0xf1f3f5, 0x22d3ee].map(c => lin(c));
  const crowd = new THREE.InstancedMesh(new THREE.CylinderGeometry(0.2, 0.26, 0.75, 7), mat(0xffffff, { roughness: 0.8 }), seats.length);
  const d = new THREE.Object3D(), ph = seats.map(() => Math.random() * TAU);
  seats.forEach((s, i) => { d.position.set(s[0], s[1], s[2]); d.updateMatrix(); crowd.setMatrixAt(i, d.matrix); crowd.setColorAt(i, pick(cols)); });
  g.add(crowd);
  // light towers
  const glows = [];
  for (const [x, z] of [[o.x0 - 14, o.z0 - 10], [o.x1 + 14, o.z0 - 10], [o.x0 - 14, o.z1 + 12], [o.x1 + 14, o.z1 + 12]]) {
    const pole = new THREE.Mesh(new THREE.CylinderGeometry(0.3, 0.4, 26, 8), mat(0x495057)); pole.position.set(x, 13, z); g.add(pole);
    const lamp = new THREE.Mesh(new THREE.BoxGeometry(4, 2, 0.6), mat(0xffffff, { emissive: 0xfff3bf, emissiveIntensity: 1.5 })); lamp.position.set(x, 26, z); lamp.lookAt((o.x0 + o.x1) / 2, 0, (o.z0 + o.z1) / 2); g.add(lamp);
    const gl = new THREE.Sprite(new THREE.SpriteMaterial({ map: glowTex(), color: 0xfff3bf, transparent: true, depthWrite: false, blending: THREE.AdditiveBlending })); gl.scale.setScalar(14); gl.position.copy(lamp.position); g.add(gl); glows.push(gl);
  }
  // sweeping colored beams (cones)
  const beams = [];
  [0xff4d6d, 0x4dabff, 0xffd43b, 0xcc5de8].forEach((c, i) => {
    const cone = new THREE.Mesh(new THREE.ConeGeometry(4, 40, 24, 1, true), new THREE.MeshBasicMaterial({ color: c, transparent: true, opacity: 0.12, depthWrite: false, blending: THREE.AdditiveBlending, side: THREE.DoubleSide }));
    cone.geometry.translate(0, -20, 0); cone.position.set(lerp(o.x0 - 10, o.x1 + 10, i / 3), 30, o.z1 + 16); g.add(cone); beams.push(cone);
  });
  // starry sky dome
  const stars = new THREE.Points(new THREE.BufferGeometry().setAttribute('position', new THREE.Float32BufferAttribute(Array.from({ length: 900 }, (_, i) => { const a = Math.random() * TAU, e = Math.random() * 1.2 + 0.15, r = 380; return [Math.cos(a) * Math.cos(e) * r, Math.sin(e) * r, Math.sin(a) * Math.cos(e) * r][i % 3]; }), 3)), new THREE.PointsMaterial({ color: 0xffffff, size: 1.6, sizeAttenuation: false }));
  g.add(stars);
  return {
    update(t, excite) {
      for (let i = 0; i < seats.length; i++) {
        const s = seats[i], j = Math.max(0, Math.sin(t * (5 + excite * 7) + ph[i])) * (0.08 + excite * 0.55);
        d.position.set(s[0], s[1] + j, s[2]); d.rotation.z = excite > 0.3 ? Math.sin(t * 9 + ph[i]) * 0.3 * excite : 0; d.updateMatrix(); crowd.setMatrixAt(i, d.matrix);
      }
      crowd.instanceMatrix.needsUpdate = true;
      beams.forEach((b, i) => { b.rotation.z = Math.sin(t * (0.5 + i * 0.17) + i) * 0.6; b.rotation.x = 0.5 + Math.cos(t * 0.4 + i * 2) * 0.3; b.material.opacity = 0.08 + excite * 0.12; });
      glows.forEach((gl, i) => gl.scale.setScalar(13 + Math.sin(t * 3 + i) * 0.6 + excite * 6));
    },
  };
}

/* ---------- 3D character with arms that swing a racket / bat ---------- */
function makePerson(color, gear) {
  const root = new THREE.Group(), body = new THREE.Group(); root.add(body);
  const shirt = mat(color), skin = mat(0xffd2a6), dark = mat(0x2b2d42), white = mat(0xffffff), black = mat(0x111111);
  const legs = [];
  for (const s of [-1, 1]) { const leg = new THREE.Mesh(new THREE.CylinderGeometry(0.11, 0.09, 0.82, 10), dark); leg.position.set(s * 0.14, 0.41, 0); body.add(leg); legs.push(leg); const shoe = new THREE.Mesh(new THREE.BoxGeometry(0.16, 0.1, 0.3), white); shoe.position.set(s * 0.14, 0.05, 0.05); body.add(shoe); }
  const torso = new THREE.Mesh(new THREE.CylinderGeometry(0.3, 0.24, 0.78, 14), shirt); torso.position.y = 1.2; body.add(torso);
  const shoulders = new THREE.Mesh(new THREE.SphereGeometry(0.3, 14, 10, 0, TAU, 0, Math.PI / 2), shirt); shoulders.position.y = 1.58; body.add(shoulders);
  const head = new THREE.Group(); head.position.y = 1.95; body.add(head);
  head.add(new THREE.Mesh(new THREE.SphereGeometry(0.27, 18, 14), skin));
  const cap = new THREE.Mesh(new THREE.SphereGeometry(0.285, 18, 10, 0, TAU, 0, Math.PI / 2.1), shirt); cap.position.y = 0.03; head.add(cap);
  const brim = new THREE.Mesh(new THREE.CylinderGeometry(0.2, 0.2, 0.03, 16, 1, false, -Math.PI / 2, Math.PI), shirt); brim.position.set(0, 0.06, 0.2); head.add(brim);
  for (const s of [-1, 1]) { const e = new THREE.Mesh(new THREE.SphereGeometry(0.06, 10, 8), white); e.position.set(s * 0.1, 0, 0.23); head.add(e); const p = new THREE.Mesh(new THREE.SphereGeometry(0.032, 8, 6), black); p.position.set(s * 0.1, 0, 0.285); head.add(p); }
  const arms = [];
  for (const s of [-1, 1]) {
    const pivot = new THREE.Group(); pivot.position.set(s * 0.36, 1.52, 0); body.add(pivot);
    const arm = new THREE.Mesh(new THREE.CylinderGeometry(0.08, 0.07, 0.66, 8), shirt); arm.position.y = -0.33; pivot.add(arm);
    const hand = new THREE.Mesh(new THREE.SphereGeometry(0.085, 10, 8), skin); hand.position.y = -0.68; pivot.add(hand);
    arms.push(pivot);
  }
  const arm = arms[1]; let tool = null;
  if (gear === 'racket') {
    tool = new THREE.Group(); tool.position.y = -0.7;
    const handle = new THREE.Mesh(new THREE.CylinderGeometry(0.025, 0.03, 0.32, 8), black); handle.position.y = -0.12; tool.add(handle);
    const ring = new THREE.Mesh(new THREE.TorusGeometry(0.2, 0.025, 8, 24), mat(color, { metalness: 0.4, roughness: 0.3 })); ring.position.y = -0.48; ring.scale.y = 1.25; tool.add(ring);
    const strings = new THREE.Mesh(new THREE.CircleGeometry(0.19, 20), new THREE.MeshStandardMaterial({ color: 0xffffff, transparent: true, opacity: 0.35, side: THREE.DoubleSide })); strings.position.y = -0.48; strings.scale.y = 1.25; tool.add(strings);
    arm.add(tool);
  } else if (gear === 'bat') {
    tool = new THREE.Mesh(new THREE.CylinderGeometry(0.055, 0.025, 0.95, 10), mat(0xc08552, { roughness: 0.4 })); tool.position.y = -1.1; arm.add(tool);
  }
  shadowAll(root);
  return { root, body, arms, legs, head, tool };
}
/* pose a person: sw = swing (1 just swung → 0), run = running phase, kind of swing */
function posePerson(P, t, sw, run, kind) {
  const e = 1 - sw;
  P.legs[0].rotation.x = Math.sin(run) * 0.6; P.legs[1].rotation.x = -Math.sin(run) * 0.6;
  P.body.position.y = Math.abs(Math.sin(run)) * 0.06;
  P.arms[0].rotation.x = -Math.sin(run) * 0.5; P.arms[0].rotation.z = -0.2;
  const R = P.arms[1];
  if (kind === 'racket') { // ready: racket back, swing: sweeps across the body
    const a = sw > 0 ? lerp(-1.9, 1.7, Math.min(1, e * 1.6)) : -0.6 + Math.sin(t * 2) * 0.05;
    R.rotation.set(-1.25, a, 0.25); P.body.rotation.y = (sw > 0 ? lerp(-0.6, 0.5, Math.min(1, e * 1.6)) : -0.25);
  } else if (kind === 'bat') { // bat held up behind the head, swing flattens it through the zone
    const a = sw > 0 ? lerp(0, 1, Math.min(1, e * 1.8)) : 0;
    R.rotation.set(lerp(-2.6, -1.5, a), lerp(-0.9, 2.2, a), lerp(0.5, 0.2, a)); P.arms[0].rotation.set(lerp(-2.4, -1.4, a), lerp(-0.5, 1.8, a), 0);
    P.body.rotation.y = lerp(-0.5, 0.8, a);
  } else if (kind === 'bowl') {
    const a = sw > 0 ? Math.min(1, e * 1.5) : 0; R.rotation.set(lerp(0.4, -1.6, a), 0, 0.1);
  } else { R.rotation.set(Math.sin(run) * 0.5, 0, 0.2); }
}
/* project a 3D point to TV-canvas coords (1600x900) */
function toScreen(cam, x, y, z) { const v = new THREE.Vector3(x, y, z).project(cam); return { x: (v.x + 1) * 800, y: (1 - v.y) * 450, vis: v.z < 1 && v.z > -1 }; }

/* ---------- 2D overlay helpers ---------- */
class PopText {
  constructor() { this.l = []; }
  add(s, x, y, color = '#ffd43b', size = 90, life = 1.3) { this.l.push({ s, x, y, color, size, life, max: life }); }
  update(dt) { for (const p of this.l) p.life -= dt; this.l = this.l.filter(p => p.life > 0); }
  draw(g) {
    for (const p of this.l) {
      const a = 1 - p.life / p.max, sc = a < 0.15 ? 0.3 + (a / 0.15) : 1 + Math.sin(a * 20) * 0.04 * (1 - a);
      g.save(); g.globalAlpha = Math.min(1, p.life / 0.3); g.translate(p.x, p.y - a * 30); g.rotate(Math.sin(a * 9) * 0.05); g.scale(sc, sc); txt(g, p.s, 0, 0, p.size, p.color); g.restore();
    }
    g.globalAlpha = 1;
  }
}
const SWING = (sport, extra = {}) => Object.assign({ name: 'swing', sport, label: 'SWING!', aim: true }, extra);
/* aim value from a moment BEFORE the swing (swinging the phone tilts it) */
function aimTracker() { const h = new Map(); return { push(p, t) { let a = h.get(p); if (!a) h.set(p, a = []); a.push([t, clamp((p.input && p.input.x) || 0)]); if (a.length > 40) a.shift(); }, at(p, t) { const a = h.get(p) || []; const o = a.find(q => q[0] >= t - 0.3); return o ? o[1] : a.length ? a[a.length - 1][1] : 0; } }; }

/* ---------- base: scene, lights, render to the TV canvas ---------- */
class Sport3D {
  constructor(c) { this.c = c; this.pop = new PopText(); this.t = 0; this.ts = 1; this.excite = 0; this.flash = 0; this.aimT = aimTracker(); this.is3d = has3D(); }
  initScene(bg = 0x070418) {
    if (!this.is3d) return;
    const s = this.scene = new THREE.Scene(); s.background = lin(bg); s.fog = new THREE.Fog(lin(bg), 90, 330);
    this.cam = new THREE.PerspectiveCamera(48, 16 / 9, 0.1, 900);
    s.add(new THREE.HemisphereLight(0xc5d8ff, 0x2a1f48, 0.55));
    const sun = this.sun = new THREE.DirectionalLight(0xfff4e0, 1.6); sun.castShadow = true; sun.shadow.mapSize.set(2048, 2048); sun.shadow.bias = -0.0005;
    s.add(sun); s.add(sun.target);
    this.fx = new Burst3D(s);
  }
  shadowBox(cx, cz, size, h = 40) { const sc = this.sun.shadow.camera; sc.left = -size; sc.right = size; sc.top = size; sc.bottom = -size; sc.near = 1; sc.far = 160; sc.updateProjectionMatrix(); this.sun.position.set(cx - 12, h, cz - 18); this.sun.target.position.set(cx, 0, cz); }
  tick3d(dt) { if (!this.is3d) return; this.fx.update(dt); if (this.stadium) this.stadium.update(this.t, this.excite); }
  render(g) {
    if (!this.is3d) { bgGrad(g, '#2b1b6b', '#07041a'); txt(g, '3D needs a newer browser on this TV', 800, 450, 40); return; }
    const r = S3.renderer(); r.render(this.scene, this.cam); g.drawImage(r.domElement, 0, 0, 1600, 900);
  }
  scr(x, y, z) { return this.is3d ? toScreen(this.cam, x, y, z) : { x: 800, y: 450, vis: false }; }
  burst(x, y, z, n, color, spd, life, grav) { if (this.is3d) this.fx.burst(x, y, z, n, color, spd, life, grav); }
  firework(x, y, z) { if (this.is3d) this.fx.firework(x, y, z); }
  nameTag(g, p, x, y, z, extra) { const s = this.scr(x, y, z); if (s.vis) txt(g, p.name + (extra || ''), s.x, s.y, 22, p.color, 'center', 'Nunito'); }
  overlays(g) {
    if (this.ts < 0.8) { g.fillStyle = `rgba(255,255,255,${(0.8 - this.ts) * 0.1})`; g.fillRect(0, 0, 1600, 900); txt(g, '⏱ SLOW-MO', 140, 860, 30, '#ffd43b'); }
    if (this.flash > 0) { g.fillStyle = `rgba(255,220,150,${this.flash})`; g.fillRect(0, 0, 1600, 900); }
    // vignette for drama
    const v = g.createRadialGradient(800, 450, 380, 800, 450, 980); v.addColorStop(0, 'rgba(0,0,0,0)'); v.addColorStop(1, 'rgba(0,0,0,.45)'); g.fillStyle = v; g.fillRect(0, 0, 1600, 900);
    this.pop.draw(g);
  }
}

/* =====================================================================
   🎾 SMASH TENNIS — Red vs Blue. Your player runs to the ball for you.
   AIM by tilting / dragging (your target ring shows on the other court),
   SWING when the ring around you closes. Perfect + hard swing = SMASH.
   ===================================================================== */
class SmashTennis extends Sport3D {
  constructor(c) { super(c); this.score = [0, 0]; this.WIN = 5; this.T = 180; }
  start() {
    this.ps = this.c.roster(2);
    const hs = this.ps.filter(p => !p.bot), bs = this.ps.filter(p => p.bot), order = [...shuffle(hs), ...bs];
    this.teams = [[], []]; order.forEach((p, i) => this.teams[i % 2].push(p));
    this.pl = new Map(); this.hits = new Map(this.ps.map(p => [p, 0]));
    this.teams.forEach((tm, ti) => tm.forEach((p, i) => { const lx = -3.6 + (i + 0.5) * (7.2 / tm.length); this.pl.set(p, { p, team: ti, home: lx, x: lx, z: ti === 0 ? -1.2 : 25.2, sw: 0, cool: 0, botAt: null, run: 0 }); }));
    this.c.setLayout('all', SWING('tennis', { hint: 'Tilt/drag to AIM · swing when the ring closes on you' }));
    for (const p of this.ps) if (!p.bot) this.c.send(p, { t: 'toast', text: this.pl.get(p).team === 0 ? '🔴 You are RED (front court)' : '🔵 You are BLUE (back court)' });
    this.cd = new Countdown(this.c); this.phase = 'cd'; this.serveTeam = 0;
    this.build();
  }
  build() {
    if (!this.is3d) return;
    this.initScene(0x090523); const s = this.scene; this.shadowBox(0, 12, 22);
    const W = 11, L = 24;
    const grass = new THREE.Mesh(new THREE.PlaneGeometry(70, 80), mat(0x1c6b3f, { roughness: 1 })); grass.rotation.x = -Math.PI / 2; grass.position.set(0, -0.01, 12); grass.receiveShadow = true; s.add(grass);
    const court = new THREE.Mesh(new THREE.PlaneGeometry(W + 3, L + 6), mat(0xffffff, { roughness: 0.85, map: canvasTex(560, 1200, (g, w, h) => {
      g.fillStyle = '#2f9e44'; g.fillRect(0, 0, w, h); const m = 60; g.fillStyle = '#3b5bdb'; g.fillRect(m, m, w - m * 2, h - m * 2);
      g.strokeStyle = '#fff'; g.lineWidth = 8; const cx = w / 2, sx = (w - m * 2) / W; g.strokeRect(m, m, w - m * 2, h - m * 2);
      const sl = (z) => m + (z / L) * (h - m * 2); g.lineWidth = 5; [6.4, 17.6].forEach(z => { g.beginPath(); g.moveTo(m + 1.37 * sx, sl(z)); g.lineTo(w - m - 1.37 * sx, sl(z)); g.stroke(); });
      g.beginPath(); g.moveTo(m + 1.37 * sx, m); g.lineTo(m + 1.37 * sx, h - m); g.moveTo(w - m - 1.37 * sx, m); g.lineTo(w - m - 1.37 * sx, h - m); g.moveTo(cx, sl(6.4)); g.lineTo(cx, sl(17.6)); g.stroke();
    }) }));
    court.rotation.x = -Math.PI / 2; court.position.set(0, 0, 12); court.receiveShadow = true; s.add(court);
    // net
    const net = new THREE.Mesh(new THREE.PlaneGeometry(W + 1, 0.95), new THREE.MeshStandardMaterial({ transparent: true, side: THREE.DoubleSide, map: canvasTex(256, 32, (g, w, h) => { g.clearRect(0, 0, w, h); g.strokeStyle = 'rgba(20,20,30,.85)'; g.lineWidth = 1.5; for (let x = 0; x < w; x += 6) { g.beginPath(); g.moveTo(x, 0); g.lineTo(x, h); g.stroke(); } for (let y = 0; y < h; y += 6) { g.beginPath(); g.moveTo(0, y); g.lineTo(w, y); g.stroke(); } }) }));
    net.position.set(0, 0.48, 12); s.add(net);
    const band = new THREE.Mesh(new THREE.BoxGeometry(W + 1, 0.07, 0.05), mat(0xffffff)); band.position.set(0, 0.97, 12); s.add(band);
    for (const x of [-(W / 2 + 0.6), W / 2 + 0.6]) { const post = new THREE.Mesh(new THREE.CylinderGeometry(0.06, 0.06, 1.1, 8), mat(0xced4da, { metalness: 0.6 })); post.position.set(x, 0.55, 12); post.castShadow = true; s.add(post); }
    this.stadium = buildStadium(s, { x0: -12, x1: 12, z0: -8, z1: 32, rows: 7 });
    // players
    for (const pl of this.pl.values()) { pl.P = makePerson(pl.p.color, 'racket'); pl.P.root.rotation.y = pl.team === 0 ? Math.PI : 0; s.add(pl.P.root); }
    // ball
    this.ballM = new THREE.Mesh(new THREE.SphereGeometry(0.13, 18, 14), mat(0xe8ff3b, { emissive: 0x99aa00, emissiveIntensity: 0.6, roughness: 0.4 })); this.ballM.castShadow = true; s.add(this.ballM);
    this.trail = new Trail3D(s, 18);
    // rings: timing ring around you, aim ring on the other court
    const ringMat = () => new THREE.MeshBasicMaterial({ color: 0xffd43b, transparent: true, opacity: 0.9, side: THREE.DoubleSide, depthWrite: false });
    this.tRing = new THREE.Mesh(new THREE.RingGeometry(0.92, 1, 48), ringMat()); this.tRing.rotation.x = -Math.PI / 2; s.add(this.tRing);
    this.tCore = new THREE.Mesh(new THREE.RingGeometry(0.85, 1, 48), ringMat()); this.tCore.rotation.x = -Math.PI / 2; this.tCore.material.color.set(0xffffff); this.tCore.material.opacity = 0.5; s.add(this.tCore);
    this.aims = new Map();
    for (const p of this.ps) if (!p.bot) {
      const grp = new THREE.Group(); const ring = new THREE.Mesh(new THREE.RingGeometry(0.55, 0.75, 40), new THREE.MeshBasicMaterial({ color: p.color, transparent: true, opacity: 0.95, side: THREE.DoubleSide, depthWrite: false })); ring.rotation.x = -Math.PI / 2; grp.add(ring);
      const dot = new THREE.Mesh(new THREE.CircleGeometry(0.18, 20), new THREE.MeshBasicMaterial({ color: p.color, side: THREE.DoubleSide })); dot.rotation.x = -Math.PI / 2; grp.add(dot);
      const beam = new THREE.Mesh(new THREE.CylinderGeometry(0.05, 0.05, 3, 6), new THREE.MeshBasicMaterial({ color: p.color, transparent: true, opacity: 0.5 })); beam.position.y = 1.5; grp.add(beam);
      grp.position.y = 0.03; s.add(grp); this.aims.set(p, grp);
    }
    this.cam.position.set(0, 7.2, -12.5); this.camLook = new THREE.Vector3(0, 0, 12);
  }
  lanes(team) { return this.teams[team].map(p => this.pl.get(p)); }
  newServe() {
    this.server = pick(this.lanes(this.serveTeam)); this.phase = 'serve'; this.wait = this.server.p.bot ? rnd(0.9, 1.4) : 6;
    this.ball = null; if (this.trail) this.trail.clear();
    this.pop.add(`${this.server.p.name} SERVES!`, 800, 240, this.server.p.color, 54, 1.6);
    if (!this.server.p.bot) this.c.send(this.server.p, { t: 'buzz', ms: 120 });
  }
  // aim -1..1 = left..right as seen from the TV
  launch(from, power, aim) {
    const other = 1 - from.team, opp = this.lanes(other);
    const smash = !!this.smashNext; this.smashNext = false;
    const tx = clamp(-aim * 4.3 + rnd(-0.35, 0.35), -4.6, 4.6); // camera looks down +z: screen-right is world -x
    const to = opp.reduce((a, b) => (Math.abs(b.home - tx) < Math.abs(a.home - tx) ? b : a));
    const near = other === 0, bz = near ? rnd(4, 7.5) : rnd(16.5, 20), tz = to.z;
    const T = (smash ? 0.62 : 1) * (1.9 - power * 0.45);
    this.ball = { fx: from.x + 0.4, fy: 1.2, fz: from.z, bx: lerp(from.x, tx, 0.82), bz, tx, ty: 1.05, tz, t: 0, t1: T * 0.68, t2: T * 0.32, h1: smash ? 0.75 : rnd(1.8, 2.5), h2: smash ? 0.6 : 1.15, rcv: to, smash, bounced: false, done: false };
    to.botAt = to.p.bot ? (Math.random() < (smash ? 0.4 : 0.13) ? null : T + rnd(-0.17, 0.17)) : null;
    to.botAim = rnd(-1, 1);
    this.phase = 'rally';
  }
  ballPos() {
    const b = this.ball, t = b.t;
    if (t < b.t1) { const u = t / b.t1; return { x: lerp(b.fx, b.bx, u), y: lerp(b.fy, 0.13, u) + 4 * b.h1 * u * (1 - u), z: lerp(b.fz, b.bz, u) }; }
    const u = (t - b.t1) / b.t2; return { x: lerp(b.bx, b.tx, u), y: Math.max(0.13, lerp(0.13, b.ty, u) + 4 * b.h2 * u * (1 - u)), z: lerp(b.bz, b.tz, u) };
  }
  onAct(p, m) { if (m && m.a === 'swing') this.swing(this.pl.get(p), clamp(+m.p || 0.6, 0.2, 1)); }
  onButton(p, b) { if (b === 'a' && !p.bot) this.swing(this.pl.get(p), 0.6); }
  aimOf(pl) { return pl.p.bot ? (pl.botAim ?? rnd(-1, 1)) : this.aimT.at(pl.p, this.t); }
  swing(pl, power) {
    if (!pl || pl.cool > 0 || this.over) return;
    pl.sw = 1; pl.cool = 0.32; this.c.sfx.whoosh();
    if (this.phase === 'serve' && pl === this.server) { this.hitFx(pl, false, false); this.launch(pl, power, this.aimOf(pl)); return; }
    if (this.phase !== 'rally' || !this.ball || this.ball.done || this.ball.rcv !== pl) return;
    const dt = this.ball.t - (this.ball.t1 + this.ball.t2);
    if (Math.abs(dt) <= 0.3) {
      const perfect = Math.abs(dt) < 0.09; this.hits.set(pl.p, this.hits.get(pl.p) + 1);
      this.smashNext = perfect && power >= 0.68; this.hitFx(pl, this.smashNext, perfect);
      this.launch(pl, power, this.aimOf(pl));
    } else if (dt < 0) { const s = this.scr(pl.x, 2.6, pl.z); this.pop.add('TOO EARLY!', s.x, s.y, '#ff922b', 36, 0.8); }
  }
  hitFx(pl, smash, perfect) {
    const p = this.ball && !this.ball.done ? this.ballPos() : { x: pl.x, y: 1.2, z: pl.z };
    this.burst(p.x, p.y, p.z, smash ? 120 : 25, smash ? 0xff922b : 0xffffff, smash ? 12 : 5, 0.7, 3);
    if (smash) { this.pop.add('🔥 SMASH!! 🔥', 800, 330, '#ff922b', 120, 1.4); this.c.shake(26); this.c.sfx.boom(); this.flash = 0.35; this.excite = 1; }
    else { if (perfect) { const s = this.scr(pl.x, 2.8, pl.z); this.pop.add('PERFECT!', s.x, s.y, '#51cf66', 44, 0.9); } this.c.shake(5); this.c.sfx.hit(); }
  }
  point(team, why) {
    if (this.phase === 'point' || this.over) return;
    this.score[team]++; this.phase = 'point'; this.wait = 2.4; this.excite = 1; this.ts = 1;
    this.pop.add(why, 800, 330, '#ff4d6d', 70, 1.6);
    this.pop.add(`${team === 0 ? 'RED' : 'BLUE'} SCORES!`, 800, 440, team === 0 ? '#ff4d6d' : '#4dabff', 60, 1.8);
    for (let i = 0; i < 4; i++) this.firework(rnd(-14, 14), rnd(14, 22), rnd(20, 34));
    this.c.sfx.win(); this.serveTeam = 1 - this.serveTeam;
    if (this.score[team] >= this.WIN) { this.winTeam = team; this.wait = 3.4; for (let i = 0; i < 8; i++) this.firework(rnd(-16, 16), rnd(12, 24), rnd(15, 35)); }
  }
  update(dt) {
    this.t += dt; this.pop.update(dt); this.flash = Math.max(0, this.flash - dt); this.excite = Math.max(0, this.excite - dt * 0.5);
    for (const p of this.ps) if (!p.bot) this.aimT.push(p, this.t);
    for (const pl of this.pl.values()) { pl.sw = Math.max(0, pl.sw - dt * 2.6); pl.cool -= dt; }
    this.tick3d(dt * this.ts);
    if (this.over) return;
    if (this.phase === 'cd') { if (this.cd.update(dt)) this.newServe(); return; }
    this.T -= dt;
    const sdt = dt * this.ts;
    for (const pl of this.pl.values()) { const tx = this.ball && !this.ball.done && this.ball.rcv === pl ? this.ball.tx - 0.5 : pl.home, dx = (tx - pl.x) * Math.min(1, dt * 5); pl.x += dx; pl.run += Math.min(0.5, Math.abs(dx) * 4); }
    if (this.phase === 'serve') { this.wait -= dt; if (this.wait <= 0) this.swing(this.server, rnd(0.45, 0.7)); return; }
    if (this.phase === 'point') { this.wait -= dt; if (this.wait <= 0) { if (this.winTeam != null || this.T <= 0) return this.end(); this.newServe(); } return; }
    if (this.phase === 'rally' && this.ball) {
      const b = this.ball; b.t += sdt;
      const arr = b.t1 + b.t2, matchPt = Math.max(...this.score) >= this.WIN - 1;
      this.ts += ((!b.rcv.p.bot && (matchPt || b.smash) && Math.abs(b.t - arr) < 0.35 ? 0.35 : 1) - this.ts) * Math.min(1, dt * 10);
      if (!b.bounced && b.t >= b.t1) { b.bounced = true; this.burst(b.bx, 0.1, b.bz, 14, 0xffffff, 3, 0.4, 2); this.c.sfx.tink(); }
      if (b.rcv.p.bot && b.rcv.botAt != null && b.t >= b.rcv.botAt) { b.rcv.botAt = null; this.swing(b.rcv, rnd(0.45, 0.9)); }
      if (b.t > arr + 0.32 && this.ball === b) { b.done = true; this.point(1 - b.rcv.team, `${b.rcv.p.name} MISSED!`); }
    }
    if (this.T <= 0 && this.phase !== 'point') this.end();
  }
  end() {
    if (this.done) return; this.done = this.over = true;
    const win = this.score[0] === this.score[1] ? -1 : this.score[0] > this.score[1] ? 0 : 1;
    this.c.finish(rankBy(this.ps, p => (this.pl.get(p).team === win ? 1000 : 0) + this.hits.get(p), p => `${this.pl.get(p).team === win ? '🏆 ' : ''}${this.hits.get(p)} hits`));
  }
  sync() {
    for (const pl of this.pl.values()) { pl.P.root.position.set(pl.x, 0, pl.z); posePerson(pl.P, this.t, pl.sw, pl.run, 'racket'); }
    const live = this.ball && !this.ball.done;
    this.ballM.visible = !!live;
    if (live) { const p = this.ballPos(); this.ballM.position.set(p.x, p.y, p.z); this.trail.push(p.x, p.y, p.z); this.ballM.material.emissive.set(this.ball.smash ? 0xff5500 : 0x99aa00); }
    this.trail.draw(live && this.ball.smash, 0.32, 0xeaffa0);
    // timing ring around the receiver
    const R = live ? this.ball.rcv : null;
    this.tRing.visible = this.tCore.visible = !!R;
    if (R) {
      const left = this.ball.t1 + this.ball.t2 - this.ball.t, k = clamp(left / 1.2, 0, 1), sc = 0.9 + k * 3;
      this.tRing.position.set(R.x, 0.04, R.z); this.tRing.scale.setScalar(sc); this.tCore.position.set(R.x, 0.035, R.z); this.tCore.scale.setScalar(0.9);
      this.tRing.material.color.set(Math.abs(left) < 0.09 ? 0x51cf66 : 0xffd43b);
    }
    // aim rings: a human's target on the opponent court, bigger when the ball is coming to them
    for (const [p, grp] of this.aims) {
      const pl = this.pl.get(p), mine = (R === pl) || (this.phase === 'serve' && this.server === pl);
      grp.visible = this.phase !== 'cd' && this.phase !== 'point';
      const ax = -clamp((p.input && p.input.x) || 0) * 4.3; grp.position.x += (ax - grp.position.x) * 0.35; grp.position.z = pl.team === 0 ? 18.5 : 5.5;
      grp.scale.setScalar(mine ? 1.5 + Math.sin(this.t * 10) * 0.1 : 0.8); grp.children.forEach(m => (m.material.opacity = mine ? 0.95 : 0.35));
    }
    // camera: behind the red team, drifts with the ball, punches in on a smash
    const bx = live ? this.ballPos().x : 0, punch = this.flash * 4;
    this.cam.position.x += (bx * 0.35 - this.cam.position.x) * 0.06; this.cam.position.y = 7.2 - punch * 0.5; this.cam.position.z = -12.5 + punch;
    this.cam.lookAt(this.cam.position.x * 0.5, 0, 12);
  }
  draw(g) {
    if (this.is3d) this.sync();
    this.render(g);
    if (this.is3d) {
      for (const pl of this.pl.values()) this.nameTag(g, pl.p, pl.x, 2.75, pl.z);
      for (const [p, grp] of this.aims) { const pl = this.pl.get(p), mine = (this.ball && !this.ball.done && this.ball.rcv === pl) || (this.phase === 'serve' && this.server === pl); if (mine && grp.visible) { const s = this.scr(grp.position.x, 3.3, grp.position.z); txt(g, `🎯 ${p.name}`, s.x, s.y, 22, p.color, 'center', 'Nunito'); } }
    }
    g.fillStyle = 'rgba(10,6,30,.82)'; rr(g, 560, 18, 480, 92, 22); g.fill(); g.lineWidth = 3; g.strokeStyle = 'rgba(255,255,255,.15)'; g.stroke();
    txt(g, 'RED', 650, 46, 24, '#ff4d6d'); txt(g, 'BLUE', 950, 46, 24, '#4dabff');
    txt(g, String(this.score[0]), 700, 86, 52, '#fff'); txt(g, '–', 800, 84, 40, '#adb5bd'); txt(g, String(this.score[1]), 900, 86, 52, '#fff');
    txt(g, `first to ${this.WIN}`, 800, 40, 16, '#adb5bd', 'center', 'Nunito');
    clock(g, this.T);
    this.overlays(g);
    if (this.winTeam != null && this.phase === 'point') txt(g, `${this.winTeam === 0 ? '🔴 RED' : '🔵 BLUE'} WINS!`, 800, 600, 96, this.winTeam === 0 ? '#ff4d6d' : '#4dabff');
    if (this.phase === 'cd') { this.cd.draw(g); txt(g, 'Tilt / drag your phone to move your 🎯 target', 800, 700, 34, '#fff', 'center', 'Nunito'); }
  }
}

/* =====================================================================
   🎳 STRIKE BOWLING — take turns. Tilt / drag to slide the ball and its
   glowing guide line, swing to roll. Strike = 20, spare = 15.
   ===================================================================== */
class StrikeBowling extends Sport3D {
  constructor(c) { super(c); this.aim = 0; }
  start() {
    this.ps = this.c.roster(1); this.frames = this.ps.length > 4 ? 2 : 3;
    this.pts = new Map(this.ps.map(p => [p, 0])); this.strikes = new Map(this.ps.map(p => [p, 0]));
    this.turns = []; for (let f = 0; f < this.frames; f++) for (const p of this.ps) this.turns.push(p);
    this.build();
    this.ti = -1; this.c.setLayout('all', { name: 'msg', emoji: '🎳', text: 'Bowling!', sub: 'Wait for your turn' }); this.nextTurn();
  }
  build() {
    if (!this.is3d) return;
    this.initScene(0x0b0620); const s = this.scene; this.shadowBox(0, 12, 14, 30);
    s.fog = new THREE.Fog(lin(0x0b0620), 30, 90);
    const floor = new THREE.Mesh(new THREE.PlaneGeometry(40, 60), mat(0x1a1033, { roughness: 1 })); floor.rotation.x = -Math.PI / 2; floor.position.set(0, -0.02, 15); floor.receiveShadow = true; s.add(floor);
    const lane = new THREE.Mesh(new THREE.PlaneGeometry(1.06, 22.4), mat(0xffffff, { roughness: 0.25, metalness: 0.1, map: canvasTex(128, 2048, (g, w, h) => {
      for (let i = 0; i < 39; i++) { g.fillStyle = `hsl(30,${50 + (i % 3) * 6}%,${58 + ((i * 7) % 5) * 3}%)`; g.fillRect(i * w / 39, 0, w / 39 + 1, h); }
      g.strokeStyle = 'rgba(90,50,10,.25)'; for (let y = 0; y < h; y += 90) { g.beginPath(); g.moveTo(0, y); g.lineTo(w, y); g.stroke(); }
      g.fillStyle = '#7b2cbf'; for (let i = -3; i <= 3; i++) { const x = w / 2 + i * 17, y = h - (5 + Math.abs(i) * 0.3) / 22.4 * h; g.beginPath(); g.moveTo(x, y - 28); g.lineTo(x - 6, y); g.lineTo(x + 6, y); g.fill(); }
      g.fillStyle = '#e03131'; g.fillRect(0, h - (1 / 22.4) * h, w, 6);
    }) }));
    lane.rotation.x = -Math.PI / 2; lane.position.set(0, 0, 10.2); lane.receiveShadow = true; s.add(lane);
    for (const sd of [-1, 1]) {
      const gut = new THREE.Mesh(new THREE.CylinderGeometry(0.12, 0.12, 22.4, 12, 1, true, 0, Math.PI), mat(0x2b2b3a, { side: THREE.DoubleSide, metalness: 0.5 })); gut.rotation.set(Math.PI / 2, 0, Math.PI); gut.position.set(sd * 0.65, 0.05, 10.2); s.add(gut);
      const wall = new THREE.Mesh(new THREE.BoxGeometry(0.1, 0.3, 22.4), mat(0x1c1440)); wall.position.set(sd * 0.82, 0.15, 10.2); s.add(wall);
      const neon = new THREE.Mesh(new THREE.BoxGeometry(0.05, 0.05, 22.4), new THREE.MeshBasicMaterial({ color: sd < 0 ? 0xff4d6d : 0x4dabff })); neon.position.set(sd * 0.82, 0.32, 10.2); s.add(neon); (this.neons = this.neons || []).push(neon);
      // neighbouring lanes for depth
      for (const k of [1, 2]) { const o = new THREE.Mesh(new THREE.PlaneGeometry(1.06, 22.4), mat(0xb98250, { roughness: 0.4 })); o.rotation.x = -Math.PI / 2; o.position.set(sd * 1.9 * k, 0, 10.2); o.receiveShadow = true; s.add(o); }
    }
    const back = new THREE.Mesh(new THREE.BoxGeometry(12, 3.2, 0.3), mat(0x0d0820, { emissive: 0x1a0b3a })); back.position.set(0, 1.6, 22.2); s.add(back);
    this.sign = canvasTex(1024, 160, () => {}); this.signCv = this.sign.image;
    const sign = new THREE.Mesh(new THREE.PlaneGeometry(6, 0.95), new THREE.MeshBasicMaterial({ map: this.sign, transparent: true })); sign.position.set(0, 2.45, 22.0); sign.rotation.y = Math.PI; s.add(sign);
    // overhead neon arches
    for (let i = 0; i < 6; i++) { const arch = new THREE.Mesh(new THREE.TorusGeometry(3.2, 0.04, 6, 40, Math.PI), new THREE.MeshBasicMaterial({ color: [0xff4d6d, 0xffd43b, 0x4dabff, 0xcc5de8, 0x51cf66, 0x22d3ee][i] })); arch.position.set(0, 0, 3 + i * 3.5); s.add(arch); (this.arches = this.arches || []).push(arch); }
    const spot = new THREE.PointLight(0xffe8cc, 1.4, 10); spot.position.set(0, 3, 19.5); s.add(spot);
    // pin geometry (lathe from a real pin profile)
    const prof = [[0, 0], [0.026, 0], [0.04, 0.02], [0.058, 0.09], [0.06, 0.13], [0.052, 0.19], [0.032, 0.25], [0.024, 0.28], [0.03, 0.32], [0.032, 0.35], [0.024, 0.375], [0, 0.383]].map(p => new THREE.Vector2(p[0] * 1.06, p[1]));
    this.pinGeo = new THREE.LatheGeometry(prof, 20); this.pinMat = mat(0xffffff, { roughness: 0.25 }); this.stripeMat = mat(0xe03131);
    this.pinMeshes = [];
    for (let i = 0; i < 10; i++) {
      const grp = new THREE.Group(), body = new THREE.Mesh(this.pinGeo, this.pinMat); grp.add(body);
      for (const y of [0.262, 0.282]) { const st = new THREE.Mesh(new THREE.CylinderGeometry(0.027, 0.027, 0.012, 16), this.stripeMat); st.position.y = y; grp.add(st); }
      shadowAll(grp); s.add(grp); this.pinMeshes.push(grp);
    }
    this.ballM = new THREE.Mesh(new THREE.SphereGeometry(0.11, 24, 18), mat(0xffffff, { roughness: 0.15, metalness: 0.35 })); this.ballM.castShadow = true; s.add(this.ballM);
    for (const [x, y] of [[0.03, 0.06], [-0.03, 0.06], [0, 0.0]]) { const h = new THREE.Mesh(new THREE.CircleGeometry(0.016, 10), mat(0x111111)); h.position.set(x, y, 0.111); this.ballM.add(h); }
    this.trail = new Trail3D(s, 14);
    // aim guide: arrow + dotted line in the bowler's color
    this.guide = new THREE.Group(); s.add(this.guide);
    const arrow = new THREE.Mesh(new THREE.ShapeGeometry(new THREE.Shape([new THREE.Vector2(0, 0.45), new THREE.Vector2(-0.18, 0), new THREE.Vector2(-0.06, 0), new THREE.Vector2(-0.06, -0.3), new THREE.Vector2(0.06, -0.3), new THREE.Vector2(0.06, 0), new THREE.Vector2(0.18, 0)])), new THREE.MeshBasicMaterial({ color: 0xffd43b, transparent: true, opacity: 0.9, side: THREE.DoubleSide }));
    arrow.rotation.x = -Math.PI / 2; arrow.position.set(0, 0.01, 1.2); this.guide.add(arrow); this.arrow = arrow;
    this.dots = []; for (let i = 0; i < 26; i++) { const d = new THREE.Mesh(new THREE.CircleGeometry(0.06, 14), new THREE.MeshBasicMaterial({ color: 0xffd43b, transparent: true, side: THREE.DoubleSide, depthWrite: false })); d.rotation.x = -Math.PI / 2; this.guide.add(d); this.dots.push(d); }
    this.bowler = makePerson('#888', null); s.add(this.bowler.root);
  }
  setPins() { this.pins = []; for (let r = 0; r < 4; r++) for (let i = 0; i <= r; i++) this.pins.push({ x: (i - r / 2) * 0.305, z: 18.3 + r * 0.264, vx: 0, vz: 0, down: false, fall: 0, dir: 0, gone: false }); }
  nextTurn() {
    this.ti++; if (this.ti >= this.turns.length) return this.end();
    this.cur = this.turns[this.ti]; this.roll = 0; this.setPins(); this.frameNo = Math.floor(this.ti / this.ps.length) + 1;
    if (this.is3d) { this.scene.remove(this.bowler.root); this.bowler = makePerson(this.cur.color, null); this.scene.add(this.bowler.root); this.guide.children.forEach(m => m.material.color.set(this.cur.color)); this.ballM.material.color.set(this.cur.color); }
    this.aimPhase();
  }
  aimPhase() {
    this.phase = 'aim'; this.wait = this.cur.bot ? rnd(1.8, 2.8) : 15; this.ball = null; this.swingAnim = 0; if (this.trail) this.trail.clear();
    this.camZ = -2.8;
    for (const p of this.ps) if (!p.bot) this.c.setLayout(p, p === this.cur ? SWING('bowling', { label: 'ROLL!', hint: 'Tilt or drag to move the 🎯 line on the TV, then swing!' }) : { name: 'msg', emoji: '🎳', text: `${this.cur.name} is bowling`, sub: 'Watch the TV!' });
    this.pop.add(`${this.cur.name} · ${this.roll === 0 ? `FRAME ${this.frameNo}/${this.frames}` : '2ND BALL'}`, 800, 190, this.cur.color, 52, 1.6);
  }
  onAct(p, m) { if (m && m.a === 'swing' && p === this.cur) this.release(clamp(+m.p || 0.6, 0.2, 1)); }
  onButton(p, b) { if (b === 'a' && p === this.cur && !p.bot) this.release(0.6); }
  release(power) {
    if (this.phase !== 'aim') return;
    const aim = this.cur.bot ? this.aim : this.aimT.at(this.cur, this.t);
    const x = -clamp(aim, -1, 1) * 0.42, spd = 7 + power * 9;
    this.ball = { x, z: 0.3, vx: -x * 0.09 + rnd(-0.02, 0.02), vz: spd, gutter: false, rot: 0, y: 0.11 };
    this.phase = 'roll'; this.rollT = 0; this.swingAnim = 1; this.c.sfx.whoosh(); this.c.shake(4);
    if (!this.cur.bot) this.c.setLayout(this.cur, { name: 'msg', emoji: '🎳', text: 'Rolling…', sub: power > 0.8 ? 'Monster throw! 💪' : 'Fingers crossed!' });
  }
  update(dt) {
    this.t += dt; this.pop.update(dt); this.flash = Math.max(0, this.flash - dt); this.excite = Math.max(0, this.excite - dt * 0.4); this.swingAnim = Math.max(0, (this.swingAnim || 0) - dt * 2);
    for (const p of this.ps) if (!p.bot) this.aimT.push(p, this.t);
    this.tick3d(dt * this.ts);
    if (this.over) return;
    if (this.phase === 'aim') {
      this.aim = this.cur.bot ? Math.sin(this.t * 1.6) * 0.5 : this.aim + (clamp((this.cur.input && this.cur.input.x) || 0) - this.aim) * Math.min(1, dt * 12);
      this.wait -= dt; if (this.wait <= 0) this.release(this.cur.bot ? rnd(0.5, 1) : 0.5);
      return;
    }
    if (this.phase === 'roll') {
      const sdt = dt * this.ts, b = this.ball; this.rollT += dt;
      b.z += b.vz * sdt; b.x += b.vx * sdt; b.rot += b.vz * sdt / 0.11;
      if (!b.gutter && b.z < 17.6 && Math.abs(b.x) > 0.53) { b.gutter = true; b.x = Math.sign(b.x) * 0.65; b.vx = 0; b.y = 0.06; this.pop.add('GUTTER!', 800, 300, '#adb5bd', 70, 1.2); this.c.sfx.buzz(); }
      if (b.z > 17.6 && Math.abs(b.x) > 0.62) { b.x = Math.sign(b.x) * 0.62; b.vx = 0; }
      if (b.z > 21.5) b.y -= sdt * 2;
      for (const pn of this.pins) {
        if (pn.gone) continue;
        if (!b.gutter && b.z < 21) { const dx = pn.x - b.x, dz = pn.z - b.z, d = Math.hypot(dx, dz); if (d < 0.17 && d > 0) { const k = Math.min(6, b.vz * 0.45); pn.vx += dx / d * k + b.vx * 0.3; pn.vz += dz / d * k + b.vz * 0.25; b.vx -= dx / d * 0.35; b.vz *= 0.96; this.knock(pn); } }
        if (Math.hypot(pn.vx, pn.vz) > 0.05) for (const o of this.pins) { if (o === pn || o.gone) continue; const dx = o.x - pn.x, dz = o.z - pn.z, d = Math.hypot(dx, dz); if (d < 0.13 && d > 0) { const sp = Math.min(5, Math.hypot(pn.vx, pn.vz) * 0.75); o.vx += dx / d * sp; o.vz += dz / d * sp; pn.vx *= 0.7; pn.vz *= 0.7; this.knock(o); } }
      }
      for (const pn of this.pins) {
        if (pn.gone) continue;
        pn.x += pn.vx * sdt; pn.z += pn.vz * sdt;
        if (Math.abs(pn.x) > 0.7) { pn.x = Math.sign(pn.x) * 0.7; pn.vx *= -0.4; }
        if (pn.z > 21.1) { pn.z = 21.1; pn.vz = 0; pn.vx *= 0.5; }
        const k = Math.pow(0.12, sdt); pn.vx *= k; pn.vz *= k;
        if (pn.down) { pn.fall = Math.min(1, pn.fall + sdt * 5); if (Math.hypot(pn.vx, pn.vz) > 0.2) pn.dir = Math.atan2(pn.vx, pn.vz); }
      }
      const nearPins = b.z > 16.5 && b.z < 19.5 && !b.gutter && this.roll === 0;
      this.ts += ((nearPins ? 0.3 : 1) - this.ts) * Math.min(1, dt * 8);
      const moving = this.pins.some(p => !p.gone && Math.hypot(p.vx, p.vz) > 0.06);
      if ((b.z > 21 && !moving) || this.rollT > 7) this.score();
    }
    if (this.phase === 'show') { this.wait -= dt; if (this.wait <= 0) { if (this.again) this.aimPhase(); else this.nextTurn(); } }
  }
  knock(pn) { if (pn.down) return; pn.down = true; pn.dir = Math.atan2(pn.vx, pn.vz); this.c.sfx.bonk(); this.burst(pn.x, 0.25, pn.z, 10, 0xffffff, 3, 0.4, 3); }
  score() {
    this.ts = 1;
    const newly = this.pins.filter(p => p.down && !p.gone).length, before = this.roll === 0 ? 0 : this.firstDown, down = before + newly;
    let gain = newly, msg = `${newly} PIN${newly === 1 ? '' : 'S'}`, color = '#fff';
    this.again = false;
    if (this.roll === 0 && down === 10) { gain = 20; msg = '💥 STRIKE!! 💥'; color = '#ffd43b'; this.strikes.set(this.cur, this.strikes.get(this.cur) + 1); this.flash = 0.5; this.excite = 1; this.c.shake(30); this.c.sfx.boom(); for (let i = 0; i < 6; i++) this.burst(rnd(-0.6, 0.6), rnd(0.3, 1.5), rnd(18, 20), 60, pick([0xff4d6d, 0xffd43b, 0x4dabff, 0x51cf66]), 6, 1.3, 3); }
    else if (this.roll === 1 && down === 10) { gain = 15 - before; msg = '✨ SPARE! ✨'; color = '#51cf66'; this.excite = 0.8; this.burst(0, 1, 19, 80, 0x51cf66, 5, 1.2, 3); this.c.sfx.win(); }
    else if (this.roll === 0) { this.again = true; this.firstDown = down; this.c.sfx.ding(); if (down === 0) msg = 'OOPS… 0'; }
    this.pts.set(this.cur, this.pts.get(this.cur) + gain);
    this.pop.add(msg, 800, 300, color, down === 10 ? 110 : 70, 2);
    if (gain) this.pop.add(`+${gain}`, 800, 420, this.cur.color, 60, 2);
    if (this.again) { this.pins.forEach(p => { if (p.down) p.gone = true; p.vx = p.vz = 0; }); this.roll = 1; }
    this.phase = 'show'; this.wait = down === 10 ? 3 : 2.2;
  }
  end() { if (this.done) return; this.done = this.over = true; this.c.finish(rankBy(this.ps, p => this.pts.get(p) * 10 + this.strikes.get(p), p => `${this.pts.get(p)} pts${this.strikes.get(p) ? ` · ${this.strikes.get(p)}× strike` : ''}`)); }
  sync() {
    // neon sign
    const sg = this.signCv.getContext('2d'); sg.clearRect(0, 0, 1024, 160); sg.font = '110px Bungee, Impact'; sg.textAlign = 'center'; sg.textBaseline = 'middle'; sg.shadowColor = `hsl(${(this.t * 90) % 360},100%,60%)`; sg.shadowBlur = 30; sg.fillStyle = `hsl(${(this.t * 90) % 360},100%,72%)`; sg.fillText('STRIKE ZONE', 512, 85); this.sign.needsUpdate = true;
    (this.arches || []).forEach((a, i) => a.material.color.setHSL(((this.t * 0.2 + i / 6) % 1), 1, 0.55 + this.excite * 0.2));
    // pins
    this.pins.forEach((pn, i) => { const m = this.pinMeshes[i]; m.visible = !pn.gone; m.position.set(pn.x, 0, pn.z); m.rotation.set(0, 0, 0); if (pn.down) { m.rotateY(pn.dir); m.rotateX(pn.fall * 1.45); } });
    for (let i = this.pins.length; i < 10; i++) this.pinMeshes[i].visible = false;
    // ball + bowler + guide
    const aimX = -clamp(this.aim, -1, 1) * 0.42, b = this.ball;
    if (b) { this.ballM.position.set(b.x, b.y, b.z); this.ballM.rotation.set(b.rot, 0, 0); this.trail.push(b.x, b.y, b.z); }
    else { this.ballM.position.set(aimX, 0.11, 0.35); this.ballM.rotation.set(0, 0, 0); }
    this.trail.draw(false, 0.25, new THREE.Color(this.cur.color));
    this.bowler.root.position.set(1.55, 0, 1.4); this.bowler.root.rotation.y = -0.6; this.bowler.root.visible = !b || b.z < 4; // stands beside the lane, cheering
    posePerson(this.bowler, this.t, this.swingAnim, 0, 'bowl');
    this.guide.visible = this.phase === 'aim';
    if (this.guide.visible) { this.arrow.position.x = aimX; this.dots.forEach((d, i) => { const z = 1.8 + i * 0.66; d.position.set(aimX - aimX * 0.09 * (z / 17) * 0, 0.006, z); d.material.opacity = 0.9 * (1 - i / 30) * (0.6 + 0.4 * Math.sin(this.t * 8 - i * 0.6)); }); }
    // camera: low behind the ball, chases it down the lane
    const tz = b ? Math.min(b.z - 3.0, 14.8) : -3.4; this.camZ += (tz - this.camZ) * 0.08;
    this.cam.position.set(b ? b.x * 0.4 : 0, b ? 1.0 : 2.0, this.camZ); this.cam.lookAt(b ? b.x * 0.3 : 0, 0.1, this.camZ + 13);
  }
  draw(g) {
    if (this.is3d) this.sync();
    this.render(g);
    board(g, rankBy(this.ps, p => this.pts.get(p), () => '').map(r => ({ name: r.p.name, color: r.p.color, val: this.pts.get(r.p) })), 20, 20, `FRAME ${Math.min(this.frameNo || 1, this.frames)}/${this.frames}`);
    if (this.phase === 'aim') txt(g, this.cur.bot ? `${this.cur.name} is aiming…` : `${this.cur.name}: tilt / drag to aim 🎯 then SWING!`, 800, 840, 40, this.cur.color);
    this.overlays(g);
  }
}

/* =====================================================================
   ⚾ HOME RUN DERBY — everyone bats at the SAME pitch. Tilt / drag to aim
   left or right field (your 🎯 shows in the outfield), swing on time.
   ===================================================================== */
class HomeRunDerby extends Sport3D {
  constructor(c) { super(c); this.N = 8; this.n = 0; }
  start() {
    this.ps = this.c.roster(1); this.dist = new Map(this.ps.map(p => [p, 0])); this.hr = new Map(this.ps.map(p => [p, 0])); this.sw = new Map(this.ps.map(p => [p, 0]));
    this.c.setLayout('all', SWING('bat', { hint: 'Tilt/drag to aim your 🎯 · swing when the ring closes!' }));
    this.cd = new Countdown(this.c); this.phase = 'cd'; this.flyT = 0;
    this.build();
  }
  bx(i) { return (i - (this.ps.length - 1) / 2) * 1.7; }
  build() {
    if (!this.is3d) return;
    this.initScene(0x0a0a2a); const s = this.scene; this.shadowBox(0, 10, 24, 50);
    const field = new THREE.Mesh(new THREE.CircleGeometry(150, 64), mat(0xffffff, { roughness: 1, map: canvasTex(1024, 1024, (g, w, h) => {
      const c = w / 2, k = w / 300; // 1 m = k px, home plate at (c, c + 40*k)... field centered on plate
      for (let i = 0; i < 30; i++) { g.fillStyle = i % 2 ? '#2b8a3e' : '#2f9e44'; g.fillRect(0, i * h / 30, w, h / 30 + 1); }
      const hx = c, hy = c; g.save(); g.translate(hx, hy); g.rotate(Math.PI / 4);
      g.fillStyle = '#c47a3a'; g.fillRect(-2 * k, -2 * k, 30 * k, 30 * k); g.fillStyle = '#2f9e44'; g.fillRect(2 * k, 2 * k, 22 * k, 22 * k); g.restore();
      g.fillStyle = '#c47a3a'; g.beginPath(); g.arc(hx, hy, 5 * k, 0, TAU); g.fill();
      g.strokeStyle = '#fff'; g.lineWidth = 3; g.beginPath(); g.moveTo(hx, hy); g.lineTo(hx + 150 * k * Math.sin(Math.PI / 4), hy - 150 * k * Math.cos(Math.PI / 4)); g.moveTo(hx, hy); g.lineTo(hx - 150 * k * Math.sin(Math.PI / 4), hy - 150 * k * Math.cos(Math.PI / 4)); g.stroke();
      g.fillStyle = '#c47a3a'; g.beginPath(); g.arc(hx, hy - 18.4 * k, 3 * k, 0, TAU); g.fill();
    }) }));
    field.rotation.x = -Math.PI / 2; field.rotation.z = Math.PI; field.receiveShadow = true; s.add(field);
    // home plate
    const plate = new THREE.Mesh(new THREE.ShapeGeometry(new THREE.Shape([new THREE.Vector2(-0.22, 0), new THREE.Vector2(0.22, 0), new THREE.Vector2(0.22, 0.22), new THREE.Vector2(0, 0.44), new THREE.Vector2(-0.22, 0.22)])), mat(0xffffff)); plate.rotation.x = -Math.PI / 2; plate.position.y = 0.01; s.add(plate);
    const mound = new THREE.Mesh(new THREE.CylinderGeometry(2.6, 2.9, 0.25, 32), mat(0xc47a3a, { roughness: 1 })); mound.position.set(0, 0.12, 18.4); mound.receiveShadow = true; s.add(mound);
    // outfield fence + stands behind it
    const fence = new THREE.Mesh(new THREE.CylinderGeometry(115, 115, 4.5, 80, 1, true, -Math.PI * 0.27, Math.PI * 0.54), mat(0x0b3d6e, { side: THREE.DoubleSide })); fence.rotation.y = 0; fence.position.y = 2.25; s.add(fence);
    const fy = new THREE.Mesh(new THREE.CylinderGeometry(115.1, 115.1, 0.4, 80, 1, true, -Math.PI * 0.27, Math.PI * 0.54), new THREE.MeshBasicMaterial({ color: 0xffd43b, side: THREE.DoubleSide })); fy.position.y = 4.6; s.add(fy);
    this.stadium = buildStadium(s, { x0: -60, x1: 60, z0: -22, z1: 118, rows: 9, sides: ['f', 'l', 'r', 'b'] });
    // pitcher
    this.pitcher = makePerson('#adb5bd', null); this.pitcher.root.position.set(0, 0.25, 18.4); this.pitcher.root.rotation.y = Math.PI; s.add(this.pitcher.root);
    // batters lined up at the plate
    this.bat = new Map(); this.ps.forEach((p, i) => { const P = makePerson(p.color, 'bat'); P.root.position.set(this.bx(i) - 0.5, 0, -0.3); P.root.rotation.y = -Math.PI / 2 + 0.25; s.add(P.root); this.bat.set(p, P); });
    // pitch ball + timing ring
    this.ballM = new THREE.Mesh(new THREE.SphereGeometry(0.1, 16, 12), mat(0xffffff, { emissive: 0x666666 })); this.ballM.castShadow = true; s.add(this.ballM);
    this.ptrail = new Trail3D(s, 12);
    this.ring = new THREE.Mesh(new THREE.TorusGeometry(1, 0.05, 8, 48), new THREE.MeshBasicMaterial({ color: 0xffd43b })); this.ring.position.set(0, 1, 0.5); s.add(this.ring);
    // flying balls (one per player) + aim markers in the outfield
    this.fly = new Map(); this.aimM = new Map();
    for (const p of this.ps) {
      const m = new THREE.Mesh(new THREE.SphereGeometry(1.2, 18, 14), mat(p.color, { emissive: p.color, emissiveIntensity: 0.8 })); m.visible = false; m.castShadow = true; s.add(m);
      this.fly.set(p, { m, trail: new Trail3D(s, 16) });
      if (!p.bot) { const a = new THREE.Group(); const ring = new THREE.Mesh(new THREE.RingGeometry(3, 4.2, 40), new THREE.MeshBasicMaterial({ color: p.color, side: THREE.DoubleSide, transparent: true, opacity: 0.85 })); ring.rotation.x = -Math.PI / 2; a.add(ring); const pole = new THREE.Mesh(new THREE.CylinderGeometry(0.4, 0.4, 14, 8), new THREE.MeshBasicMaterial({ color: p.color, transparent: true, opacity: 0.55 })); pole.position.y = 7; a.add(pole); a.position.y = 0.05; s.add(a); this.aimM.set(p, a); }
    }
    this.camP = new THREE.Vector3(0, 2.3, -6.5); this.camL = new THREE.Vector3(0, 1.3, 18);
  }
  nextPitch() {
    this.n++; if (this.n > this.N) return this.end();
    const kind = this.n === this.N ? 'CRAZY BALL' : pick(['FASTBALL', 'CHANGE-UP', 'CURVEBALL', 'FASTBALL', 'SLIDER']);
    const T = { FASTBALL: 0.72, 'CHANGE-UP': 1.15, CURVEBALL: 0.95, SLIDER: 0.85, 'CRAZY BALL': 1.0 }[kind];
    this.pitch = { kind, T, t: -1.1, swings: new Map(), res: [] }; this.phase = 'pitch';
    for (const p of this.ps) if (p.bot) { const r = Math.random(); this.pitch.swings.set(p, { at: T + (r < 0.15 ? rnd(-0.6, 0.5) : rnd(-0.13, 0.13)), pow: rnd(0.5, 1), aim: rnd(-1, 1), bot: true }); }
    if (this.ptrail) this.ptrail.clear();
    this.pop.add(`PITCH ${this.n}/${this.N}: ${kind}`, 800, 190, kind === 'CRAZY BALL' ? '#ff4d6d' : '#ffd43b', 46, 1.4);
  }
  ballAt(t) {
    const P = this.pitch, u = clamp(t / P.T, 0, 1.3);
    let x = 0, y = lerp(1.9, 0.95, u), z = lerp(17.6, 0, u);
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
    this.pitch.swings.set(p, { at: this.pitch.t, pow, aim: this.aimT.at(p, this.t) });
  }
  resolve() {
    const P = this.pitch; P.res = [];
    for (const p of this.ps) {
      const s = P.swings.get(p); let d = 0, type = 'MISS';
      if (s) {
        const e = Math.abs(s.at - P.T);
        if (e < 0.17) { const q = 1 - e / 0.17; d = Math.round((q * 0.72 + s.pow * 0.28) * 150 + rnd(-6, 6)); type = d >= 115 ? 'HOME RUN' : e > 0.13 ? 'FOUL' : 'HIT'; if (type === 'FOUL') d = 0; }
        else type = s.at < P.T ? 'TOO EARLY' : 'TOO LATE';
      }
      this.dist.set(p, this.dist.get(p) + d); if (type === 'HOME RUN') this.hr.set(p, this.hr.get(p) + 1);
      const aim = s ? s.aim : 0, ang = type === 'FOUL' ? (Math.random() < 0.5 ? -1 : 1) * rnd(0.85, 1.1) : clamp(-aim * 0.7 + rnd(-0.08, 0.08), -0.78, 0.78);
      P.res.push({ p, d, type, ang, fd: type === 'FOUL' ? rnd(30, 60) : d, h: rnd(10, 16) + d * 0.08 });
      if (!p.bot) this.c.send(p, { t: 'toast', text: type === 'HOME RUN' ? `💥 HOME RUN! ${d} m` : d ? `${type}! ${d} m` : type });
    }
    const hrs = P.res.filter(r => r.type === 'HOME RUN');
    if (hrs.length) { this.pop.add(hrs.length > 1 ? `💥 ${hrs.length}× HOME RUN!! 💥` : `💥 HOME RUN ${hrs[0].p.name}! 💥`, 800, 230, '#ffd43b', 80, 2.2); this.flash = 0.45; this.c.shake(24); this.c.sfx.boom(); this.excite = 1; }
    else if (P.res.some(r => r.d)) { this.c.sfx.hit(); this.c.shake(8); this.excite = 0.6; }
    else { this.pop.add('STRIKE!', 800, 300, '#ff4d6d', 90, 1.4); this.c.sfx.buzz(); }
    for (const r of P.res) if (r.d || r.type === 'FOUL') { const i = this.ps.indexOf(r.p); this.burst(this.bx(i), 1, 0.3, 30, 0xffffff, 6, 0.5, 2); }
    this.phase = 'fly'; this.flyT = 0; this.fired = new Set();
  }
  update(dt) {
    this.t += dt; this.pop.update(dt); this.flash = Math.max(0, this.flash - dt); this.excite = Math.max(0, this.excite - dt * 0.4);
    for (const p of this.ps) { this.sw.set(p, Math.max(0, this.sw.get(p) - dt * 2.5)); if (!p.bot) this.aimT.push(p, this.t); }
    this.tick3d(dt);
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
      for (const r of this.pitch.res) if (r.type === 'HOME RUN' && this.flyT > 2.3 && !this.fired.has(r.p)) { this.fired.add(r.p); for (let i = 0; i < 3; i++) this.firework(Math.sin(r.ang) * 120 + rnd(-15, 15), rnd(25, 45), Math.cos(r.ang) * 120); }
      if (this.flyT > 4) this.nextPitch();
    }
  }
  end() { if (this.done) return; this.done = this.over = true; this.c.finish(rankBy(this.ps, p => this.dist.get(p) * 10 + this.hr.get(p), p => `${this.dist.get(p)} m${this.hr.get(p) ? ` · ${this.hr.get(p)} HR` : ''}`)); }
  sync() {
    const P = this.pitch;
    // pitcher wind-up
    const wind = P && this.phase === 'pitch' ? clamp((P.t + 1.1) / 1.1, 0, 1) : 0;
    posePerson(this.pitcher, this.t, wind > 0 && wind < 1 ? 1 - wind : 0, 0, 'bowl');
    this.ps.forEach((p, i) => posePerson(this.bat.get(p), this.t, this.sw.get(p), 0, 'bat'));
    const pitching = this.phase === 'pitch' && P && P.t >= 0;
    this.ballM.visible = pitching; this.ring.visible = this.phase === 'pitch' && P && P.t > -0.5;
    if (pitching) { const b = this.ballAt(P.t); this.ballM.position.set(b.x, b.y, b.z); this.ptrail.push(b.x, b.y, b.z); }
    this.ptrail.draw(P && P.kind === 'CRAZY BALL' && pitching, 0.22);
    if (this.ring.visible) { const left = P.T - P.t, k = clamp(left / 1.0, 0, 1); this.ring.scale.setScalar(0.45 + k * 2.6); this.ring.material.color.set(Math.abs(left) < 0.07 ? 0x51cf66 : 0xffd43b); this.ring.lookAt(this.cam.position); }
    // aim markers in the outfield (live)
    for (const [p, a] of this.aimM) { const ang = -clamp((p.input && p.input.x) || 0) * 0.7, r = 95; a.position.x += (Math.sin(ang) * r - a.position.x) * 0.25; a.position.z += (Math.cos(ang) * r - a.position.z) * 0.25; a.visible = this.phase === 'pitch' || this.phase === 'cd'; }
    // flying balls
    const u = clamp(this.flyT / 2.4, 0, 1);
    for (const p of this.ps) { const f = this.fly.get(p); f.m.visible = false; if (this.phase !== 'fly') { f.trail.clear(); } }
    if (this.phase === 'fly') for (const r of this.pitch.res) {
      if (!r.fd) continue; const f = this.fly.get(r.p), i = this.ps.indexOf(r.p);
      const x = this.bx(i) * (1 - u) + Math.sin(r.ang) * r.fd * u, z = Math.cos(r.ang) * r.fd * u, y = 1 + 4 * r.h * u * (1 - u);
      f.m.visible = true; f.m.position.set(x, y, z); f.trail.push(x, y, z); f.trail.draw(r.type === 'HOME RUN', 1.3, new THREE.Color(r.p.color));
    }
    // camera: behind the plate during the pitch, flies up to watch the ball
    if (this.phase === 'fly') {
      // chase cam: fly right behind the longest hit
      const lead = this.pitch.res.filter(r => r.fd).sort((a, b) => b.fd - a.fd)[0];
      if (lead) {
        const i = this.ps.indexOf(lead.p), x = this.bx(i) * (1 - u) + Math.sin(lead.ang) * lead.fd * u, z = Math.cos(lead.ang) * lead.fd * u, y = 1 + 4 * lead.h * u * (1 - u);
        const back = u < 1 ? 16 : 30; this.camP.set(x - Math.sin(lead.ang) * back, y + (u < 1 ? 4 : 12), z - Math.cos(lead.ang) * back); this.camL.set(x, y, z);
      } else { this.camP.set(0, 6, -10); this.camL.set(0, 1, 18); }
    } else { this.camP.set(0, 2.3, -6.5); this.camL.set(0, 1.3, 18); }
    this.cam.position.lerp(this.camP, this.phase === 'fly' ? 0.14 : 0.2); const look = this.camLook || (this.camLook = this.camL.clone()); look.lerp(this.camL, 0.15); this.cam.lookAt(look);
  }
  draw(g) {
    if (this.is3d) this.sync();
    this.render(g);
    if (this.is3d && (this.phase === 'pitch' || this.phase === 'cd')) for (const [p, a] of this.aimM) { const s = this.scr(a.position.x, 15, a.position.z); if (s.vis) txt(g, `🎯 ${p.name}`, s.x, s.y, 20, p.color, 'center', 'Nunito'); }
    if (this.is3d) this.ps.forEach((p, i) => this.nameTag(g, p, this.bx(i) - 0.5, 2.6, -0.3));
    if (this.phase === 'fly') {
      const u = clamp(this.flyT / 2.4, 0, 1);
      if (this.is3d) for (const r of this.pitch.res) if (r.fd) { const i = this.ps.indexOf(r.p), x = this.bx(i) * (1 - u) + Math.sin(r.ang) * r.fd * u, z = Math.cos(r.ang) * r.fd * u, y = 1 + 4 * r.h * u * (1 - u); const s = this.scr(x, y + 3, z); if (s.vis) txt(g, `${r.p.name} ${r.type === 'FOUL' ? 'FOUL' : Math.round(r.d * u) + ' m'}`, s.x, s.y, 26, r.p.color, 'center', 'Nunito'); }
      const rows = [...this.pitch.res].sort((a, b) => b.d - a.d);
      g.fillStyle = 'rgba(10,6,30,.78)'; rr(g, 1250, 120, 330, rows.length * 38 + 20, 18); g.fill();
      rows.forEach((r, i) => { const y = 148 + i * 38; g.fillStyle = r.p.color; g.beginPath(); g.arc(1276, y, 9, 0, TAU); g.fill(); txt(g, r.p.name.slice(0, 10), 1294, y, 19, '#fff', 'left', 'Nunito'); txt(g, r.d ? `${r.d} m` : r.type, 1564, y, 18, r.type === 'HOME RUN' ? '#ffd43b' : r.d ? '#fff' : '#ff8787', 'right'); });
    }
    strip(g, rankBy(this.ps, p => this.dist.get(p), () => '').map(r => ({ name: r.p.name, color: r.p.color, val: this.dist.get(r.p) + 'm' })));
    txt(g, `PITCH ${Math.min(this.n, this.N)}/${this.N}`, 1500, 110, 22, '#adb5bd');
    this.overlays(g);
    if (this.phase === 'cd') { this.cd.draw(g); txt(g, 'Tilt / drag your phone to move your 🎯 in the outfield', 800, 700, 34, '#fff', 'center', 'Nunito'); }
  }
}
