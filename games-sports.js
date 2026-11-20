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
  for (const s of [1, -1]) { // arms[0] = left (+x), arms[1] = right (-x) when facing +z
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
  let tool1 = null;
  if (gear) { tool1 = makeTool(gear === 'bowl' ? 'ball' : gear, color); tool1.position.y = -0.7; tool1.quaternion.setFromEuler(new THREE.Euler(-0.95, 0, -0.6)).invert(); tool1.visible = false; arm.add(tool1); }
  shadowAll(root);
  return { root, body, arms, legs, head, tool, tool1 };
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
  } else if (kind === 'fh' || kind === 'bh') { // forehand: racket from the right side across; backhand: from the left
    const s = kind === 'fh' ? 1 : -1, k = sw > 0 ? Math.min(1, e * 1.6) : 0;
    R.rotation.set(-1.3, s * lerp(-2.0, 1.8, k), 0.2 * s); P.body.rotation.y = s * lerp(-0.7, 0.6, k);
    if (kind === 'bh') P.arms[0].rotation.set(-1.2, lerp(1.5, -1.0, k), 0);
  } else if (kind === 'serve') { // racket behind the head, up and over
    const k = sw > 0 ? Math.min(1, e * 1.7) : 0;
    R.rotation.set(lerp(-3.4, -0.3, k), 0, lerp(0.6, 0.1, k)); P.arms[0].rotation.set(lerp(-2.8, -0.6, k), 0, -0.2); P.body.rotation.y = lerp(-0.4, 0.4, k);
  } else if (kind === 'toss') { R.rotation.set(-2.5, 0, 0.8); P.arms[0].rotation.set(-3.0, 0, -0.1); P.body.rotation.y = -0.3;
  } else if (kind === 'hold') { P.body.rotation.y = 0; P.arms[0].rotation.set(-0.5, 0, -0.3);
  } else { R.rotation.set(Math.sin(run) * 0.5, 0, 0.2); }
}
/* ---------- 1:1 phone orientation ----------
   The phone streams its rotation since "center" (held upright, screen facing the player).
   In that pose the BACK of the phone points at the TV = the player's forward (+z in the player's frame). */
let _QC = null, _A0 = null; const _Y = () => new THREE.Vector3(0, 1, 0);
function phoneQ(raw) {
  if (!raw || !has3D()) return null;
  if (!_QC) _QC = new THREE.Quaternion().setFromAxisAngle(_Y(), Math.PI);
  const q = new THREE.Quaternion(raw[0], raw[1], raw[2], raw[3]).normalize();
  return _QC.clone().multiply(q).multiply(_QC.clone().invert()); // phone frame -> player frame
}
const livePhone = p => (p && !p.bot && p.or && Date.now() - p.orT < 1500 ? p.or : null);
/* which way the racket face / bat / hand points, in WORLD space (yaw = direction the player faces) */
function faceDir(q, yaw) { return new THREE.Vector3(0, 0, 1).applyQuaternion(q).applyAxisAngle(_Y(), yaw); }
/* pose the right arm + 1:1 tool from the phone: the arm rotates from the shoulder exactly like the phone */
function holdTool(P, q, yaw) {
  if (!_A0) _A0 = new THREE.Quaternion().setFromEuler(new THREE.Euler(-0.95, 0, -0.6));
  P.body.rotation.y = 0;
  const user = new THREE.Quaternion().setFromAxisAngle(_Y(), yaw).multiply(q); // tool rotation in world
  const inv = P.root.quaternion.clone().invert();
  P.arms[1].quaternion.copy(inv.multiply(user).multiply(_A0));
  if (P.tool) P.tool.visible = false; if (P.tool1) P.tool1.visible = true;
}
/* aim angle (radians) scaled to the player's calibrated comfortable turn: their max turn = full aim */
function aimYaw(p, n, fwd = 1) { const k = 32 / ((p && p.calYaw) || 32); return clamp(Math.atan2(n.x * fwd, Math.max(0.2, n.z * fwd)) * k, -1.1, 1.1) * fwd; }
function freeTool(P) { if (P.tool) P.tool.visible = true; if (P.tool1) P.tool1.visible = false; }
/* tools built UPRIGHT (grip at origin, head up +y, face normal +z) for 1:1 holding */
function makeTool(kind, color) {
  const g = new THREE.Group();
  if (kind === 'racket') {
    const handle = new THREE.Mesh(new THREE.CylinderGeometry(0.03, 0.025, 0.34, 8), mat(0x111111)); handle.position.y = 0.12; g.add(handle);
    const ring = new THREE.Mesh(new THREE.TorusGeometry(0.21, 0.028, 8, 28), mat(color, { metalness: 0.4, roughness: 0.3 })); ring.position.y = 0.55; ring.scale.y = 1.25; g.add(ring);
    const str = new THREE.Mesh(new THREE.CircleGeometry(0.2, 22), new THREE.MeshStandardMaterial({ color: 0xffffff, transparent: true, opacity: 0.4, side: THREE.DoubleSide })); str.position.y = 0.55; str.scale.y = 1.25; g.add(str);
  } else if (kind === 'bat') {
    const bat = new THREE.Mesh(new THREE.CylinderGeometry(0.058, 0.025, 1.0, 12), mat(0xc08552, { roughness: 0.35 })); bat.position.y = 0.42; g.add(bat);
    const knob = new THREE.Mesh(new THREE.SphereGeometry(0.04, 8, 6), mat(0x5c3a1e)); knob.position.y = -0.08; g.add(knob);
  } else if (kind === 'ball') {
    const b = new THREE.Mesh(new THREE.SphereGeometry(0.11, 20, 14), mat(color, { roughness: 0.15, metalness: 0.35 })); b.position.set(0, 0, 0.06); g.add(b);
  }
  shadowAll(g); return g;
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


/* ---------- TENNIS BALL PHYSICS (metres, seconds) ----------
   Simulated, not faked: gravity + air drag + Magnus force from spin, a real bounce (spin changes the kick),
   and the net. Real tennis-ball numbers: m = 57 g, r = 3.3 cm, drag Cd ≈ 0.55, lift from spin up to CL ≈ 0.3.
   GAME_SPEED slows real shot speeds a little so people in a living room can react. */
const TB = { g: 9.81, kD: 0.0196, kL: 0.035, r: 0.033, e: 0.75, NET_H: 0.914, dt: 1 / 240, GAME_SPEED: 0.82 };
function tennisSim(p, v, spin, maxT = 4) {
  let x = p.x, y = p.y, z = p.z, vx = v.x, vy = v.y, vz = v.z, s = spin, t = 0, k = 0;
  const pts = [[0, x, y, z]], bounces = []; let net = null;
  while (t < maxT) {
    const sp = Math.hypot(vx, vy, vz) || 1, hl = Math.hypot(vx, vz) || 1, wx = vz / hl, wz = -vx / hl; // spin axis ⟂ to travel
    const cl = 0.3 * s; // topspin (+) pushes down, slice (−) lifts
    const ax = -TB.kD * sp * vx + TB.kL * cl * sp * (-wz * vy);
    const ay = -TB.g - TB.kD * sp * vy + TB.kL * cl * sp * (wz * vx - wx * vz);
    const az = -TB.kD * sp * vz + TB.kL * cl * sp * (wx * vy);
    const pz = z;
    vx += ax * TB.dt; vy += ay * TB.dt; vz += az * TB.dt; x += vx * TB.dt; y += vy * TB.dt; z += vz * TB.dt; t += TB.dt;
    // the net (0.914 m in the middle, 1.07 m at the posts)
    if ((pz - 12) * (z - 12) <= 0 && y < TB.NET_H + Math.min(1, Math.abs(x) / 5.5) * 0.155 + TB.r) { net = { t, x, y: Math.max(TB.r, y), z: 12 + Math.sign(pz - 12) * 0.08 }; pts.push([t, net.x, net.y, net.z]); break; }
    if (y < TB.r && vy < 0) { // bounce: topspin kicks forward and high, slice skids and stays low
      y = TB.r; vy = -vy * (TB.e + 0.05 * s); const f = clamp(0.68 + 0.12 * s, 0.5, 0.85); vx *= f; vz *= f; s *= 0.55;
      bounces.push({ t, x, z }); if (bounces.length >= 2) { pts.push([t, x, y, z]); break; }
    }
    if (++k % 2 === 0) pts.push([t, x, y, z]);
  }
  return { pts, bounces, net, end: pts[pts.length - 1][0] };
}
/* aim a shot: find the launch angle that lands the ball on the target with this speed and spin.
   If the speed is too high for that distance the ball just goes long — like real tennis (use topspin!). */
function solveShot(S, T, speed, spin, kind, noLift) {
  const dx = T.x - S.x, dz = T.z - S.z, dist = Math.hypot(dx, dz), yaw = Math.atan2(dx, dz);
  const shoot = th => { const c = Math.cos(th), v = { x: speed * c * Math.sin(yaw), y: speed * Math.sin(th), z: speed * c * Math.cos(yaw) }; const sim = tennisSim(S, v, spin); const b = sim.bounces[0]; return { sim, th, d: b ? Math.hypot(b.x - S.x, b.z - S.z) : sim.net ? 0 : 99 }; };
  const lob = kind === 'lob';
  let lo = lob ? 0.62 : kind === 'smash' || kind === 'serve' ? -0.4 : -0.15, hi = lob ? 1.2 : 0.7, r = null;
  for (let i = 0; i < 18; i++) { const mid = (lo + hi) / 2; r = shoot(mid); const tooShort = r.d < dist; if (lob ? !tooShort : tooShort) lo = mid; else hi = mid; }
  // players aim to clear the net: if this one clips it, lift it a little (it may then land long)
  for (let i = 0; r.sim.net && !noLift && i < 6 && kind !== 'lob'; i++) r = shoot(r.th + 0.025);
  return r;
}
const gauss = () => (Math.random() + Math.random() + Math.random() - 1.5) * 0.8;
/* =====================================================================
   🎾 SMASH TENNIS — real tennis, 1:1.
   SERVE: lift the phone (or raise your hand on camera) to TOSS, then swing to hit
          at the top of the toss. Two faults = double fault.
   RALLY: your player runs to the ball; swing when the ring closes. Swing TOWARD
          where you want the ball. Forehand / backhand picked by which side it comes.
   Real scoring (15-30-40-deuce-advantage), first to 2 games. 1v1 = split screen.
   ===================================================================== */
const CALLS = ['love', 'fifteen', 'thirty', 'forty'], SHOW = ['0', '15', '30', '40'];
class SmashTennis extends Sport3D {
  constructor(c) { super(c); this.pts = [0, 0]; this.games = [0, 0]; this.GAMES = 2; this.T = 300; this.faults = 0; this.serveIdx = [0, 0]; }
  start() {
    this.ps = this.c.roster(2);
    const hs = this.ps.filter(p => !p.bot), bs = this.ps.filter(p => p.bot), order = [...shuffle(hs), ...bs];
    this.teams = [[], []]; order.forEach((p, i) => this.teams[i % 2].push(p));
    this.split = this.teams[0].length === 1 && this.teams[1].length === 1 && !this.teams[0][0].bot && !this.teams[1][0].bot;
    this.pl = new Map(); this.hits = new Map(this.ps.map(p => [p, 0])); this.aces = new Map(this.ps.map(p => [p, 0])); this.bests = new Map(); this.fastest = null; this.radarShow = null;
    this.teams.forEach((tm, ti) => tm.forEach((p, i) => { const lx = -3.4 + (i + 0.5) * (6.8 / tm.length); this.pl.set(p, { p, team: ti, home: lx, x: lx, homeZ: ti === 0 ? -1.2 : 25.2, z: ti === 0 ? -1.2 : 25.2, sw: 0, cool: 0, botAt: null, run: 0, stroke: 'racket' }); }));
    this.c.setLayout('all', SWING('tennis', { hint: 'Serve: swing · Rally: swing when the ring closes · purple ring = SMASH' }));
    for (const p of this.ps) if (!p.bot) this.c.send(p, { t: 'toast', text: this.pl.get(p).team === 0 ? '🔴 You are RED' : '🔵 You are BLUE' });
    this.cd = new Countdown(this.c); this.phase = 'cd'; this.serveTeam = 0;
    this.build();
    this.c.sfx.crowdOn && this.c.sfx.crowdOn(0.1);
  }
  tn(team) { return this.split ? this.teams[team][0].name : team === 0 ? 'Red' : 'Blue'; }
  say(s) { this.c.sfx.say && this.c.sfx.say(s); }
  build() {
    if (!this.is3d) return;
    this.initScene(0x090523); const s = this.scene; this.shadowBox(0, 12, 22);
    const W = 11, L = 24;
    const grass = new THREE.Mesh(new THREE.PlaneGeometry(70, 80), mat(0x1c6b3f, { roughness: 1 })); grass.rotation.x = -Math.PI / 2; grass.position.set(0, -0.01, 12); grass.receiveShadow = true; s.add(grass);
    const court = new THREE.Mesh(new THREE.PlaneGeometry(W + 3, L + 6), mat(0xffffff, { roughness: 0.85, map: canvasTex(560, 1200, (g, w, h) => {
      g.fillStyle = '#2f9e44'; g.fillRect(0, 0, w, h); const m = 60; g.fillStyle = '#3b5bdb'; g.fillRect(m, m, w - m * 2, h - m * 2);
      g.strokeStyle = '#fff'; g.lineWidth = 8; const cx = w / 2, sx = (w - m * 2) / W; g.strokeRect(m, m, w - m * 2, h - m * 2);
      const sl = (z) => m + (z / L) * (h - m * 2); g.lineWidth = 5; [5.6, 18.4].forEach(z => { g.beginPath(); g.moveTo(m + 1.37 * sx, sl(z)); g.lineTo(w - m - 1.37 * sx, sl(z)); g.stroke(); });
      g.beginPath(); g.moveTo(m + 1.37 * sx, m); g.lineTo(m + 1.37 * sx, h - m); g.moveTo(w - m - 1.37 * sx, m); g.lineTo(w - m - 1.37 * sx, h - m); g.moveTo(cx, sl(5.6)); g.lineTo(cx, sl(18.4)); g.stroke();
    }) }));
    court.rotation.x = -Math.PI / 2; court.position.set(0, 0, 12); court.receiveShadow = true; s.add(court);
    const net = new THREE.Mesh(new THREE.PlaneGeometry(W + 1, 0.95), new THREE.MeshStandardMaterial({ transparent: true, side: THREE.DoubleSide, map: canvasTex(256, 32, (g, w, h) => { g.clearRect(0, 0, w, h); g.strokeStyle = 'rgba(20,20,30,.85)'; g.lineWidth = 1.5; for (let x = 0; x < w; x += 6) { g.beginPath(); g.moveTo(x, 0); g.lineTo(x, h); g.stroke(); } for (let y = 0; y < h; y += 6) { g.beginPath(); g.moveTo(0, y); g.lineTo(w, y); g.stroke(); } }) }));
    net.position.set(0, 0.48, 12); s.add(net); this.netM = net;
    const band = new THREE.Mesh(new THREE.BoxGeometry(W + 1, 0.07, 0.05), mat(0xffffff)); band.position.set(0, 0.97, 12); s.add(band);
    for (const x of [-(W / 2 + 0.6), W / 2 + 0.6]) { const post = new THREE.Mesh(new THREE.CylinderGeometry(0.06, 0.06, 1.1, 8), mat(0xced4da, { metalness: 0.6 })); post.position.set(x, 0.55, 12); post.castShadow = true; s.add(post); }
    this.stadium = buildStadium(s, { x0: -12, x1: 12, z0: -8, z1: 32, rows: 7 });
    for (const pl of this.pl.values()) { pl.P = makePerson(pl.p.color, 'racket'); pl.P.root.rotation.y = pl.team === 0 ? 0 : Math.PI; s.add(pl.P.root); }
    this.ballM = new THREE.Mesh(new THREE.SphereGeometry(0.13, 18, 14), mat(0xe8ff3b, { emissive: 0x99aa00, emissiveIntensity: 0.6, roughness: 0.4 })); this.ballM.castShadow = true; s.add(this.ballM);
    this.trail = new Trail3D(s, 18);
    const ringMat = () => new THREE.MeshBasicMaterial({ color: 0xffd43b, transparent: true, opacity: 0.9, side: THREE.DoubleSide, depthWrite: false });
    this.tRing = new THREE.Mesh(new THREE.RingGeometry(0.92, 1, 48), ringMat()); this.tRing.rotation.x = -Math.PI / 2; s.add(this.tRing);
    this.tCore = new THREE.Mesh(new THREE.RingGeometry(0.85, 1, 48), ringMat()); this.tCore.rotation.x = -Math.PI / 2; this.tCore.material.color.set(0xffffff); this.tCore.material.opacity = 0.5; s.add(this.tCore);
    // toss meter: a ring at the top of the toss, glows green at the perfect moment
    this.markM = [0, 1, 2].map(() => { const m = new THREE.Mesh(new THREE.CircleGeometry(0.14, 20), new THREE.MeshBasicMaterial({ color: 0x51cf66, transparent: true, side: THREE.DoubleSide, depthWrite: false })); m.rotation.x = -Math.PI / 2; m.scale.y = 1.6; m.visible = false; s.add(m); return m; });
    this.tossRing = new THREE.Mesh(new THREE.TorusGeometry(0.35, 0.04, 8, 32), new THREE.MeshBasicMaterial({ color: 0xffd43b })); this.tossRing.visible = false; s.add(this.tossRing);
    this.aims = new Map();
    for (const p of this.ps) if (!p.bot) {
      const grp = new THREE.Group(); const ring = new THREE.Mesh(new THREE.RingGeometry(0.55, 0.75, 40), new THREE.MeshBasicMaterial({ color: p.color, transparent: true, opacity: 0.95, side: THREE.DoubleSide, depthWrite: false })); ring.rotation.x = -Math.PI / 2; grp.add(ring);
      const dot = new THREE.Mesh(new THREE.CircleGeometry(0.18, 20), new THREE.MeshBasicMaterial({ color: p.color, side: THREE.DoubleSide })); dot.rotation.x = -Math.PI / 2; grp.add(dot);
      const beam = new THREE.Mesh(new THREE.CylinderGeometry(0.05, 0.05, 3, 6), new THREE.MeshBasicMaterial({ color: p.color, transparent: true, opacity: 0.5 })); beam.position.y = 1.5; grp.add(beam);
      grp.position.y = 0.03; s.add(grp); this.aims.set(p, grp);
    }
    this.cam.position.set(0, 7.2, -12.5);
    if (this.split) { this.camB = new THREE.PerspectiveCamera(50, 640 / 720, 0.1, 900); this.camB.position.set(0, 6.4, 34); this.cam.fov = 50; }
  }
  lanes(team) { return this.teams[team].map(p => this.pl.get(p)); }
  // which world direction is "right" for this player's screen (split screen: blue looks the other way)
  rightX(pl) { return this.split && pl.team === 1 ? 1 : -1; }
  bodyRight(pl) { return pl.team === 0 ? -1 : 1; }
  newServe() {
    const tm = this.lanes(this.serveTeam); this.server = tm[this.serveIdx[this.serveTeam] % tm.length];
    this.phase = 'serve'; this.tossed = false; this.tossT = 0; this.server.auto = false; this.rally = 0;
    if (!this.faults && Math.random() < 0.25 && this.pts[0] + this.pts[1] > 0) setTimeout(() => this.say('Quiet, please.'), 900); this.wait = this.server.p.bot ? rnd(0.8, 1.3) : 9;
    this.ball = null; if (this.trail) this.trail.clear();
    // server stands at the baseline on the side of the score (deuce / ad court)
    const side = (this.pts[0] + this.pts[1]) % 2 === 0 ? 1 : -1; this.server.serveX = side * 1.4 * (this.server.team === 0 ? -1 : 1);
    this.pop.add(this.faults ? `2ND SERVE — ${this.server.p.name}` : `${this.server.p.name} SERVES`, 800, 230, this.server.p.color, 50, 1.6);
    if (!this.server.p.bot) this.c.send(this.server.p, { t: 'toast', text: '🎾 Your serve — SWING!' });
  }
  toss(pl) {
    if (this.phase !== 'serve' || pl !== this.server || this.tossed) return;
    this.tossed = true; this.tossT = 0; pl.stroke = 'toss'; this.c.sfx.swish && this.c.sfx.swish();
  }
  tossPos() { const s = this.server, u = this.tossT / 1.0; return { x: s.x - 0.35 * this.bodyRight(s), y: 1.7 + 1.9 * Math.sin(Math.PI * clamp(u, 0, 1)), z: s.z + (s.team === 0 ? 0.3 : -0.3), u }; }
  // ---------- REAL BALL PHYSICS ----------
  // kind: 'drive' | 'lob' | 'smash' | 'serve'. The ball flies with gravity, clears the net (unless it's a mishit),
  // bounces with energy loss, and we work out exactly where and when it reaches the receiver.
  halfW() { return this.teams[0].length === 1 && this.teams[1].length === 1 ? 4.115 : 5.485; } // singles / doubles sidelines
  // REAL SHOT: intended target from aim + timing, speed from the measured swing, spin from the swing path
  launch(from, power, aim, n, opt = {}) {
    const kind = opt.kind || 'drive', other = 1 - from.team, fwd = from.team === 0 ? 1 : -1, opp = this.lanes(other), hw = this.halfW();
    const S = opt.start || { x: from.x + 0.45 * this.bodyRight(from), y: kind === 'smash' ? 2.5 : 1.0, z: from.z + fwd * 0.35 };
    const kmh = opt.kmh || 60 + power * 90, err = opt.err || 0;
    let spin = opt.spin != null ? clamp(opt.spin) : kind === 'lob' ? 0.2 : kind === 'smash' ? 0.05 : kind === 'serve' ? 0.15 : 0.4;
    let tx, tz;
    if (kind === 'serve') { // cross-court into the diagonal service box: aim chooses wide or down the T
      const side = Math.sign(from.serveX || 1), box = -side, w = clamp(((aim ?? rnd(-1, 1)) * box * this.rightX(from) * -1) * 0.5 + 0.5, 0, 1);
      tx = box * lerp(0.5, 3.5, w); tz = 12 + fwd * rnd(4.6, 5.8); this.serveBox = { sign: box, fwd };
    } else {
      tx = aim != null ? aim * 3.4 * this.rightX(from) : n ? from.x + Math.tan(aimYaw(from.p, n, fwd)) * 6 : rnd(-2, 2);
      tz = 12 + fwd * (kind === 'lob' ? 10 : kind === 'smash' ? rnd(5, 9) : opt.attack ? rnd(6.2, 8.4) : rnd(8.6, 10.2));
      if (opt.attack) tx *= 1.18;
    }
    // timing error: early/late pulls the ball sideways and makes the length less precise (that's where errors come from)
    // mistiming turns the racket face: being 0.1 s early/late pulls the ball ~2.5 m off line (a bit gentler for phone players,
    // whose timing also includes sensor noise)
    const ek = from.p.bot ? 25 : 15;
    tx += err * ek * this.bodyRight(from) + gauss() * Math.abs(err) * ek * 0.4; tz += fwd * gauss() * Math.abs(err) * ek * 0.6;
    tx = clamp(tx, -hw - 1.5, hw + 1.5);
    // real speed × game speed, with a playable minimum (a soft swing still gets the ball back, like a real rally ball)
    const raw = (kmh / 3.6) * TB.GAME_SPEED;
    const speed = kind === 'lob' ? clamp(raw * 0.7, 16, 19) : kind === 'smash' ? Math.max(22, raw) : kind === 'serve' ? Math.max(18, raw) : Math.max(14.5, raw);
    // mistimed or very low balls often don't get lifted enough → into the net (like real)
    const noLift = kind === 'drive' && ((Math.abs(err) > 0.07 && Math.random() < 0.6) || (S.y < 0.65 && Math.random() < 0.45));
    if (noLift) tz -= fwd * rnd(3, 6);
    const shot = solveShot(S, { x: tx, z: tz }, speed, spin, kind, noLift), sim = shot.sim, b1 = sim.bounces[0];
    // in or out? (lines are in — the ball only has to touch them)
    let inside = false;
    if (b1 && !sim.net) {
      if (kind === 'serve') inside = Math.sign(b1.x) === this.serveBox.sign || Math.abs(b1.x) < TB.r ? Math.abs(b1.x) <= 4.115 + TB.r && (fwd > 0 ? b1.z >= 12 && b1.z <= 18.4 + TB.r : b1.z <= 12 && b1.z >= 5.6 - TB.r) : false;
      else inside = Math.abs(b1.x) <= hw + TB.r && (fwd > 0 ? b1.z >= 12 && b1.z <= 24 + TB.r : b1.z <= 12 && b1.z >= -TB.r);
    }
    // where the receiver can meet it: after the bounce, at a comfortable height, or at their baseline
    let c = null;
    if (inside) {
      for (const [t, x, y, z] of sim.pts) {
        if (t <= b1.t + 0.05) continue;
        const behind = fwd > 0 ? z > 24.4 : z < -0.4, idx = sim.pts.findIndex(q => q[0] === t), prev = sim.pts[Math.max(0, idx - 1)], falling = y < prev[2];
        if ((falling && y <= 1.25) || behind || (sim.bounces[1] && t >= sim.bounces[1].t - 0.04)) { c = { x, y, z, t }; break; }
      }
      if (!c) { const l = sim.pts[sim.pts.length - 1]; c = { t: l[0], x: l[1], y: l[2], z: l[3] }; }
    }
    const to = opp.reduce((a, q) => (Math.abs(q.x - (c ? c.x : tx)) < Math.abs(a.x - (c ? c.x : tx)) ? q : a));
    // can they get there in time? real players: ~0.25 s to react, ~5.5 m/s sprint, ~1.3 m reach with the racket
    const reach = c ? Math.hypot(c.x - to.x, c.z - to.z) <= 5.3 * Math.max(0, c.t - 0.3) + 1.25 : false;
    this.ball = { sim, t: 0, t1: b1 ? b1.t : 99, bx: b1 ? b1.x : tx, bz: b1 ? b1.z : tz, A: c ? c.t : sim.end, c: c || { x: tx, y: 1, z: to.z, t: sim.end }, rcv: to, by: from, kind, spin, kmh: Math.round(kmh),
      smash: kind === 'smash' || !!opt.fire, bounced: false, done: false, out: !!b1 && !sim.net && !inside, netT: sim.net ? sim.net.t : null, serve: kind === 'serve', reach, prevZ: S.z };
    // bots: they try to hit it on time, but like people they are sometimes a bit early / late
    const hard = clamp((kmh - 90) / 70, 0, 1) + (c && (c.y > 1.9 || c.y < 0.45) ? 0.5 : 0); // fast or awkward balls are harder
    const e = gauss() * (this.ball.smash ? 0.13 : 0.09) * (1 + hard);
    const beaten = kind === 'serve' && kmh > 160 && Math.random() < (kmh - 158) / 230; // a big serve can beat the returner (ace)
    to.botAt = to.p.bot && c && reach && !beaten && !this.ball.out && this.ball.netT == null ? c.t + e : null; to.botErr = e;
    to.botAim = -Math.sign(from.x || rnd(-1, 1)) * this.rightX(to) * rnd(0.4, 1); // aim for the open court, toward the lines
    if (!to.p.bot && c && reach && c.y > 2) { const s = this.scr(c.x, 3.2, c.z); this.pop.add('⬆ HIGH BALL — SMASH IT!', s.x, s.y, '#cc5de8', 34, 1.2); }
    this.phase = 'rally';
  }
  ballPos() {
    const b = this.ball, pts = b.sim.pts, t = b.t;
    if (b.netT != null && t > b.netT) { const n = b.sim.net, u = t - b.netT; return { x: n.x, y: Math.max(TB.r, n.y - 0.5 * TB.g * u * u), z: n.z }; }
    const i = Math.min(pts.length - 2, Math.max(0, Math.floor(t * 120)));
    if (t >= b.sim.end) { const l = pts[pts.length - 1], u = t - b.sim.end; return { x: l[1], y: Math.max(TB.r, l[2]), z: l[3] + (b.by.team === 0 ? 1 : -1) * u * 4 }; }
    const a = pts[i], q = pts[i + 1], f = clamp((t - a[0]) / ((q[0] - a[0]) || 1), 0, 1);
    return { x: lerp(a[1], q[1], f), y: lerp(a[2], q[2], f), z: lerp(a[3], q[3], f) };
  }
  onAct(p, m) {
    const pl = this.pl.get(p); if (!pl || !m) return;
    if (m.a === 'toss') return this.toss(pl);
    if (m.a === 'swing') { pl.spinIn = typeof m.spin === 'number' ? clamp(m.spin) : null; this.swing(pl, clamp(+m.p || 0.6, 0.2, 1), m.q, typeof m.aim === 'number' ? clamp(m.aim) : null, +m.lag || 0, +m.kmh || 0); }
  }
  onButton(p, b) {
    const pl = this.pl.get(p); if (!pl || b !== 'a') return;
    if (this.phase === 'serve' && pl === this.server && !this.tossed) return this.swing(pl, 0.7); // hand up on camera / Space = serve
    if (!p.cam && !p.bot) this.swing(pl, 0.6);
  }
  faceOf(pl, raw) { const q = phoneQ(raw || livePhone(pl.p)); return q ? faceDir(q, pl.team === 0 ? 0 : Math.PI) : null; }
  aimOf(pl) { return pl.p.bot ? (pl.botAim ?? rnd(-1, 1)) : this.aimT.at(pl.p, this.t); }
  // how late a swing reaches the TV: the phone's measuring delay + half the network round trip (+ camera processing)
  latency(pl, lag) { return lag + ((pl.p.rtt || 90) / 2000) + (pl.p.cam ? 0.12 : 0); }
  // shot speed (km/h, what the radar shows): measured from the swing when we have it, otherwise estimated
  speedOf(power, kmh, kind) { const k = kmh || Math.round(55 + power * 95 + rnd(-6, 6)); return Math.round(kind === 'smash' ? Math.max(k, 120) * 1.15 : kind === 'lob' ? k * 0.55 : kind === 'serve' ? k * 1.15 : k); }
  radar(pl, kmh, kind) {
    pl.p.best = Math.max(pl.p.best || 0, kmh); this.bests.set(pl.p, Math.max(this.bests.get(pl.p) || 0, kmh));
    if (!this.fastest || kmh > this.fastest.kmh) { const first = !!this.fastest; this.fastest = { p: pl.p, kmh }; if (first && kmh > 120) this.pop.add(`⚡ NEW FASTEST: ${kmh} km/h!`, 800, 760, '#ffd43b', 44, 1.6); }
    this.radarShow = { kmh, kind, color: pl.p.color, name: pl.p.name, t: 2.2 };
  }
  swing(pl, power, raw, aimIn = null, lag = 0, kmhIn = 0) {
    if (!pl || pl.cool > 0 || this.over) return;
    pl.sw = 1; pl.cool = 0.25; this.c.sfx.swish ? this.c.sfx.swish() : this.c.sfx.whoosh();
    const n = this.faceOf(pl, raw), aim = aimIn != null ? aimIn : n ? null : this.aimOf(pl);
    if (this.phase === 'serve' && pl === this.server) {
      // ONE swing serves: the ball goes up and is hit at the top automatically, with your swing's power + direction
      if (this.tossed) return;
      this.toss(pl); pl.servePow = power; pl.serveKmh = kmhIn; pl.serveAim = aim; pl.serveN = aimIn != null ? null : n; pl.serveAt = 0.5;
      return;
    }
    if (this.phase !== 'rally' || !this.ball || this.ball.done || this.ball.rcv !== pl || this.ball.netT != null || this.ball.out) return;
    const b = this.ball, dt = b.t - this.latency(pl, lag) - b.A;
    if (dt < -0.38) { const s = this.scr(pl.x, 2.6, pl.z); this.pop.add('TOO EARLY!', s.x, s.y, '#ff922b', 36, 0.7); pl.cool = 0.15; return; }
    if (dt > 0.3) return; // too late — the ball is past you
    if (!b.reach) { const s = this.scr(pl.x, 2.6, pl.z); this.pop.add('TOO FAR!', s.x, s.y, '#ff922b', 34, 0.7); return; }
    const perfect = Math.abs(dt) < 0.09, good = Math.abs(dt) < 0.2, high = b.c.y > 2.0;
    this.hits.set(pl.p, this.hits.get(pl.p) + 1); this.rally = (this.rally || 0) + 1;
    if (this.rally === 8) { this.say('Unbelievable!'); this.c.sfx.cheer && this.c.sfx.cheer(0.5); } else if (perfect && Math.random() < 0.12) this.say('What a shot!');
    const bxRel = (b.c.x - pl.x) * this.bodyRight(pl);
    let kind = 'drive', fire = false, boost = 1;
    if (high && power >= 0.45) { kind = 'smash'; pl.stroke = 'serve'; } // overhead smash on a high ball
    else if (n && n.y > 0.45) { kind = 'lob'; pl.stroke = bxRel > -0.1 ? 'fh' : 'bh'; } // racket face tilted up = lob
    else { pl.stroke = bxRel > -0.1 ? 'fh' : 'bh'; if (power >= 0.78 && Math.abs(dt) < 0.13) { fire = true; boost = 1.35; } }
    if (!good) power *= 0.7; // mistimed = weaker
    const s = this.scr(pl.x, 3, pl.z);
    this.pop.add(kind === 'smash' ? 'SMASH!' : kind === 'lob' ? 'LOB' : pl.stroke === 'fh' ? 'FOREHAND' : 'BACKHAND', s.x, s.y, kind === 'smash' ? '#ff922b' : '#fff', kind === 'smash' ? 40 : 26, 0.7);
    this.hitFx(pl, kind === 'smash' || fire, perfect, power, kind === 'smash' ? 'smash' : 'power');
    this.freeze = kind === 'smash' || fire ? 0.12 : 0.05; // hit-stop: a tiny freeze sells the impact
    const kmh = this.speedOf(power, good ? kmhIn : kmhIn * 0.75, kind); this.radar(pl, kmh, kind);
    if (kmh > 175 && kind !== 'smash' && Math.random() < 0.35) this.say('What a shot!');
    const spin = kind === 'smash' ? 0.05 : kind === 'lob' ? 0.2 : pl.spinIn != null ? pl.spinIn : pl.p.bot ? rnd(0.2, 0.7) : 0.4; pl.spinIn = null;
    if (this.radarShow) this.radarShow.spin = spin;
    this.launch(pl, power, aim, n, { kind, fire: fire || kmh > 150, kmh: kmh * (good ? 1 : 0.85), spin, attack: pl.p.bot && pl.attack, err: pl.p.bot ? pl.botErr || 0 : dt, start: { x: b.c.x, y: b.c.y, z: b.c.z } }); pl.attack = false;
  }
  fault(why) {
    this.faults++; this.ball = null; this.tossed = false;
    this.c.sfx.ooh && this.c.sfx.ooh();
    if (this.faults >= 2) { this.faults = 0; return this.point(1 - this.serveTeam, 'DOUBLE FAULT!', 'Double fault.'); }
    this.pop.add(`FAULT! ${why}`, 800, 330, '#ff922b', 60, 1.4); this.say('Fault.');
    this.phase = 'between'; this.wait = 1.6;
  }
  // Hawk-Eye: a mark where the ball landed (green in, red out) and a close-call label
  mark(x, z, out) {
    const hw = this.halfW(), lineD = Math.min(Math.abs(Math.abs(x) - hw), Math.abs(z), Math.abs(z - 24));
    this.marks = (this.marks || []).concat([{ x, z, out, life: 3 }]).slice(-3);
    if (lineD < 0.3) { const s = this.scr(x, 0.5, z); this.pop.add(out ? 'OUT ✋ (close!)' : 'IN ✔ (on the line!)', s.x, s.y - 40, out ? '#ff4d6d' : '#51cf66', 30, 1.6); }
  }
  hitFx(pl, smash, perfect, power = 0.7, label = 'power') {
    const p = this.ball && !this.ball.done ? this.ballPos() : this.phase === 'serve' && this.tossed ? this.tossPos() : { x: pl.x, y: 1.2, z: pl.z };
    this.burst(p.x, p.y, p.z, smash ? 120 : 25, smash ? 0xff922b : 0xffffff, smash ? 12 : 5, 0.7, 3);
    if (smash) { if (label === 'smash') this.say('Smash! What power!'); else if (Math.random() < 0.06) this.say('What a shot!'); this.pop.add(label === 'smash' ? '🔥 SMASH!! 🔥' : '💥 POWER SHOT!', 800, 330, '#ff922b', label === 'smash' ? 120 : 80, 1.2); this.c.shake(label === 'smash' ? 26 : 12); this.c.sfx.smash ? this.c.sfx.smash() : this.c.sfx.boom(); this.flash = 0.35; this.excite = 1; this.c.sfx.cheer && this.c.sfx.cheer(0.6); }
    else { if (perfect) { const s = this.scr(pl.x, 2.8, pl.z); this.pop.add('PERFECT!', s.x, s.y, '#51cf66', 44, 0.9); } this.c.shake(5); this.c.sfx.pok ? this.c.sfx.pok(power) : this.c.sfx.hit(); }
  }
  callScore() {
    const s = this.serveTeam, a = this.pts[s], b = this.pts[1 - s];
    if (a >= 3 && b >= 3) return a === b ? 'Deuce' : `Advantage ${this.tn(a > b ? s : 1 - s)}`;
    return a === b ? `${CALLS[a]} all` : `${CALLS[a]}, ${CALLS[b]}`;
  }
  showPts(team) { const a = this.pts[team], b = this.pts[1 - team]; if (a >= 3 && b >= 3) return a > b ? 'AD' : '40'; return SHOW[Math.min(3, a)]; }
  point(team, why, spoken) {
    if (this.phase === 'point' || this.over) return;
    this.pts[team]++; this.phase = 'point'; this.wait = 2.6; this.excite = 1; this.ts = 1; this.faults = 0;
    const o = 1 - team; let call;
    if (this.pts[team] >= 4 && this.pts[team] - this.pts[o] >= 2) {
      this.games[team]++; this.pts = [0, 0];
      this.serveIdx[this.serveTeam]++; this.serveTeam = 1 - this.serveTeam; // serve changes every game
      if (this.games[team] >= this.GAMES) { this.winTeam = team; this.wait = 4; call = `Game, set and match, ${this.tn(team)}!`; for (let i = 0; i < 10; i++) this.firework(rnd(-16, 16), rnd(12, 24), rnd(15, 35)); this.c.sfx.applause && this.c.sfx.applause(2); }
      else call = `Game, ${this.tn(team)}!`;
      this.pop.add(this.winTeam != null ? '🏆 MATCH!' : 'GAME!', 800, 440, team === 0 ? '#ff4d6d' : '#4dabff', 90, 2);
    } else call = this.callScore();
    this.pop.add(why, 800, 330, '#ff4d6d', 64, 1.6);
    this.say(`${spoken || ''} ${call}`.trim());
    for (let i = 0; i < 4; i++) this.firework(rnd(-14, 14), rnd(14, 22), rnd(20, 34));
    this.c.sfx.cheer ? this.c.sfx.cheer(1) : this.c.sfx.win();
  }
  update(dt) {
    this.t += dt; this.pop.update(dt); this.flash = Math.max(0, this.flash - dt); this.excite = Math.max(0, this.excite - dt * 0.5);
    for (const p of this.ps) if (!p.bot) this.aimT.push(p, this.t);
    for (const pl of this.pl.values()) { pl.sw = Math.max(0, pl.sw - dt * 2.4); pl.cool -= dt; if (pl.sw === 0 && pl.stroke !== 'toss') pl.stroke = 'racket'; }
    this.ts = 1; this.tick3d(dt);
    if (this.over) return;
    if (this.phase === 'cd') { if (this.cd.update(dt)) { this.newServe(); this.say(`${this.tn(this.serveTeam)} to serve. Play!`); } return; }
    this.T -= dt;
    for (const pl of this.pl.values()) {
      // run to where the ball will be met (ball on the forehand side), otherwise back to your spot
      let tx = pl.home, tz = pl.homeZ;
      const b = this.ball;
      if (b && !b.done && b.rcv === pl && b.netT == null && !b.out) { tx = b.c.x - 0.55 * this.bodyRight(pl); tz = b.c.z - (pl.team === 0 ? 0.35 : -0.35); }
      if ((this.phase === 'serve' || this.phase === 'between') && pl === this.server) { tx = pl.serveX ?? pl.home; tz = pl.homeZ; }
      else if ((this.phase === 'serve' || this.phase === 'between') && this.server && pl.team !== this.server.team) { tx = -Math.sign(this.server.serveX || 1) * 2.3 + (pl.home * 0.3); tz = pl.homeZ; } // returner stands behind the box
      tz = pl.team === 0 ? clamp(tz, -3, 10.5) : clamp(tz, 13.5, 27);
      const mx = clamp(tx - pl.x, -5.8 * dt, 5.8 * dt), mz = clamp(tz - pl.z, -5 * dt, 5 * dt); // human sprint speeds
      pl.x += mx; pl.z += mz; pl.run += Math.min(0.5, Math.hypot(mx, mz) * 3);
    }
    if (this.phase === 'between') { this.wait -= dt; if (this.wait <= 0) this.newServe(); return; }
    if (this.phase === 'serve') {
      const sv = this.server;
      // bots serve by themselves; a human who does nothing for 9 s gets an automatic soft serve
      if (!this.tossed) { this.wait -= dt; if (this.wait <= 0) { sv.cool = 0; this.swing(sv, sv.p.bot ? rnd(0.55, 0.95) : 0.55, null, null, 0, sv.p.bot ? Math.round(this.faults ? rnd(115, 145) : rnd(140, 190)) : 0); } return; }
      this.tossT += dt;
      if (this.tossT >= (sv.serveAt || 0.5)) { // hit at the top of the toss
        const p = sv.servePow || 0.6; sv.stroke = 'serve'; sv.sw = 1;
        if (p < 0.4 && Math.random() < 0.35) return this.fault('Too soft — into the net!');
        const ace = p > 0.85; this.hitFx(sv, ace, ace, p); this.freeze = 0.06;
        if (ace) this.pop.add('⚡ BIG SERVE!', 800, 300, '#ffd43b', 80, 1.2);
        const kmh = this.speedOf(p, sv.serveKmh, 'serve'); this.radar(sv, kmh, 'serve');
        const se = gauss() * 0.022 * (1 + Math.max(0, (kmh - 130) / 35)) * (this.faults ? 0.6 : 1); // 2nd serves are safer
        this.launch(sv, p, sv.serveAim, sv.serveN, { kind: 'serve', ace: ace || kmh > 185, fire: ace, kmh, err: se, start: this.tossPos() });
      }
      return;
    }
    if (this.phase === 'point') { this.wait -= dt; if (this.wait <= 0) { if (this.winTeam != null || this.T <= 0) return this.end(); this.newServe(); } return; }
    if (this.phase === 'rally' && this.ball) {
      const b = this.ball;
      if (this.freeze > 0) { this.freeze -= dt; return; }
      b.t += dt;
      if (b.netT != null && b.t >= b.netT && !b.netDone) { b.netDone = true; this.c.sfx.netHit && this.c.sfx.netHit(); this.c.sfx.ooh && this.c.sfx.ooh(); }
      if (b.netT != null) { if (b.t > b.netT + 0.9) { b.done = true; if (b.serve) this.fault('Net!'); else this.point(1 - b.by.team, `NET! ${b.by.p.name}`, 'Net.'); } return; }
      if (!b.bounced && b.t >= b.t1) {
        b.bounced = true; this.burst(b.bx, 0.1, b.bz, 14, 0xffffff, 3, 0.4, 2); this.c.sfx.bounce ? this.c.sfx.bounce() : this.c.sfx.tink();
        this.mark(b.bx, b.bz, b.out);
        if (b.out) { b.done = true; this.c.sfx.ooh && this.c.sfx.ooh(); if (b.serve) return this.fault('Out!'); return this.point(1 - b.by.team, `OUT! ${b.by.p.name}`, 'Out!'); }
      }
      if (b.rcv.p.bot && b.rcv.botAt != null && b.t >= b.rcv.botAt) { b.rcv.botAt = null; b.rcv.attack = Math.random() < 0.22; this.swing(b.rcv, rnd(0.5, 0.9), null, null, 0, Math.round(rnd(85, 135) + (b.rcv.attack ? 18 : 0))); }
      if (b.t > b.A + 0.3 + this.latency(b.rcv, 0.11) && this.ball === b) {
        b.done = true;
        if (b.serve) { this.aces.set(b.by.p, this.aces.get(b.by.p) + 1); this.point(b.by.team, `ACE! ${b.by.p.name}`, 'Ace!'); }
        else this.point(b.by.team, b.reach ? `${b.rcv.p.name} MISSED!` : `WINNER! ${b.by.p.name}`, b.reach ? '' : 'What a shot!');
      }
    }
    if (this.T <= 0 && this.phase !== 'point') this.end();
  }
  end() {
    if (this.done) return; this.done = this.over = true;
    this.c.sfx.crowdOff && this.c.sfx.crowdOff();
    const sc = t => this.games[t] * 10 + this.pts[t], win = sc(0) === sc(1) ? -1 : sc(0) > sc(1) ? 0 : 1;
    this.c.finish(rankBy(this.ps, p => (this.pl.get(p).team === win ? 1000 : 0) + this.hits.get(p) + this.aces.get(p) * 3, p => `${this.pl.get(p).team === win ? '🏆 ' : ''}${this.hits.get(p)} hits${this.aces.get(p) ? ` · ${this.aces.get(p)} aces` : ''}${this.bests.get(p) ? ` · ⚡${this.bests.get(p)} km/h` : ''}`));
  }
  sync() {
    for (const pl of this.pl.values()) {
      pl.P.root.position.set(pl.x, 0, pl.z);
      const q = phoneQ(livePhone(pl.p));
      if (q) { posePerson(pl.P, this.t, 0, pl.run, 'hold'); if (!pl.qs) pl.qs = q.clone(); pl.qs.slerp(q, 0.6); holdTool(pl.P, pl.qs, pl.team === 0 ? 0 : Math.PI); }
      else { freeTool(pl.P); posePerson(pl.P, this.t, pl.sw, pl.run, pl === this.server && this.phase === 'serve' && this.tossed && pl.sw === 0 ? 'toss' : pl.stroke); }
    }
    const tossing = this.phase === 'serve' && this.tossed, live = this.ball && !this.ball.done;
    this.ballM.visible = !!live || tossing;
    let bp = null;
    if (tossing) bp = this.tossPos(); else if (live) bp = this.ballPos();
    if (bp) { this.ballM.position.set(bp.x, bp.y, bp.z); if (live) this.trail.push(bp.x, bp.y, bp.z); this.ballM.material.emissive.set(live && this.ball.smash ? 0xff5500 : 0x99aa00); }
    this.trail.draw(live && this.ball.smash, 0.32, 0xeaffa0);
    // toss timing ring: around the top of the toss, green at the sweet spot
    this.tossRing.visible = tossing;
    if (tossing) { const s = this.server, u = this.tossT / 1.25; this.tossRing.position.set(s.x - 0.35 * this.bodyRight(s), 3.6, s.z + (s.team === 0 ? 0.3 : -0.3)); this.tossRing.lookAt(this.cam.position); this.tossRing.material.color.set(0x51cf66); this.tossRing.scale.setScalar(0.8 + u); }
    (this.marks || []).forEach(m => (m.life -= 1 / 60)); this.marks = (this.marks || []).filter(m => m.life > 0);
    this.markM.forEach((m, i) => { const k = this.marks[i]; m.visible = !!k; if (k) { m.position.set(k.x, 0.012, k.z); m.material.color.set(k.out ? 0xff4d6d : 0x51cf66); m.material.opacity = Math.min(1, k.life); } });
    const R = live && this.ball.netT == null && !this.ball.out && this.ball.reach ? this.ball.rcv : null;
    this.tRing.visible = this.tCore.visible = !!R;
    if (R) {
      // the ring sits where you'll meet the ball and closes exactly when you should swing; purple = high ball, SMASH it
      const b = this.ball, left = b.A - b.t, k = clamp(left / 1.2, 0, 1), sc = 0.9 + k * 3;
      this.tRing.position.set(b.c.x, 0.04, b.c.z); this.tRing.scale.setScalar(sc); this.tCore.position.set(b.c.x, 0.035, b.c.z); this.tCore.scale.setScalar(0.9);
      this.tRing.material.color.set(b.c.y > 2 ? 0xcc5de8 : Math.abs(left) < 0.12 ? 0x51cf66 : 0xffd43b);
    }
    for (const [p, grp] of this.aims) {
      const pl = this.pl.get(p), mine = (R === pl) || (this.phase === 'serve' && this.server === pl);
      grp.visible = mine && this.phase !== 'cd' && this.phase !== 'point';
      const n = this.faceOf(pl), fw = pl.team === 0 ? 1 : -1;
      const ax = n ? clamp(pl.x + Math.tan(aimYaw(p, n, fw)) * 6, -4.6, 4.6) : clamp(((p.input && p.input.x) || 0) * 4.3 * this.rightX(pl), -4.6, 4.6);
      grp.position.x += (ax - grp.position.x) * 0.35; grp.position.z = pl.team === 0 ? 18.5 : 5.5;
      grp.scale.setScalar(1.4 + Math.sin(this.t * 10) * 0.1);
    }
    const bx = bp ? bp.x : 0, punch = this.flash * 4;
    this.cam.position.x += (bx * 0.35 - this.cam.position.x) * 0.06; this.cam.position.y = 7.2 - punch * 0.5; this.cam.position.z = -12.5 + punch;
    this.cam.lookAt(this.cam.position.x * 0.5, 0, 12);
    if (this.camB) { this.camB.position.x += (bx * 0.35 - this.camB.position.x) * 0.06; this.camB.position.y = 7.2 - punch * 0.5; this.camB.position.z = 36.5 - punch; this.camB.lookAt(this.camB.position.x * 0.5, 0, 12); }
  }
  // split screen for 1v1: left = behind red, right = behind blue
  render(g) {
    if (!this.split || !this.is3d) return super.render(g);
    const r = S3.renderer(); r.setScissorTest(true);
    [[0, this.cam], [1, this.camB]].forEach(([i, cam]) => { cam.aspect = 640 / 720; cam.updateProjectionMatrix(); r.setViewport(i * 640, 0, 640, 720); r.setScissor(i * 640, 0, 640, 720); r.render(this.scene, cam); });
    r.setScissorTest(false); r.setViewport(0, 0, 1280, 720);
    g.drawImage(r.domElement, 0, 0, 1600, 900);
    g.fillStyle = '#0a0620'; g.fillRect(796, 0, 8, 900);
  }
  scrIn(i, x, y, z) { const cam = i ? this.camB : this.cam, v = new THREE.Vector3(x, y, z).project(cam); return { x: i * 800 + (v.x + 1) * 400, y: (1 - v.y) * 450, vis: v.z < 1 && v.z > -1 }; }
  draw(g) {
    if (this.is3d) this.sync();
    this.render(g);
    if (this.is3d) {
      const views = this.split ? [0, 1] : [null];
      for (const vi of views) {
        const S = (x, y, z) => vi == null ? this.scr(x, y, z) : this.scrIn(vi, x, y, z);
        for (const pl of this.pl.values()) { const s = S(pl.x, 2.75, pl.z); if (s.vis && (vi == null || s.x > vi * 800 && s.x < vi * 800 + 800)) txt(g, pl.p.name, s.x, s.y, 22, pl.p.color, 'center', 'Nunito'); }
        for (const [p, grp] of this.aims) if (grp.visible) { const s = S(grp.position.x, 3.3, grp.position.z); if (s.vis) txt(g, `🎯 ${p.name}`, s.x, s.y, 22, p.color, 'center', 'Nunito'); }
      }
      if (this.split) { txt(g, `🔴 ${this.tn(0)}`, 400, 870, 30, '#ff4d6d'); txt(g, `🔵 ${this.tn(1)}`, 1200, 870, 30, '#4dabff'); }
    }
    // scoreboard: games + points, like on TV
    g.fillStyle = 'rgba(10,6,30,.86)'; rr(g, 520, 14, 560, 104, 22); g.fill(); g.lineWidth = 3; g.strokeStyle = 'rgba(255,255,255,.15)'; g.stroke();
    [0, 1].forEach(tm => {
      const y = 44 + tm * 44, col = tm === 0 ? '#ff4d6d' : '#4dabff';
      if (this.serveTeam === tm && this.phase !== 'cd') { g.fillStyle = '#e8ff3b'; g.beginPath(); g.arc(545, y, 7, 0, TAU); g.fill(); }
      txt(g, this.tn(tm).slice(0, 10).toUpperCase(), 562, y, 24, col, 'left');
      txt(g, String(this.games[tm]), 900, y, 30, '#fff');
      g.fillStyle = 'rgba(255,255,255,.12)'; rr(g, 950, y - 18, 100, 36, 10); g.fill();
      txt(g, this.showPts(tm), 1000, y, 28, '#ffd43b');
    });
    txt(g, 'GAMES', 900, 22, 12, '#adb5bd', 'center', 'Nunito'); txt(g, 'POINTS', 1000, 22, 12, '#adb5bd', 'center', 'Nunito');
    clock(g, this.T);
    if (this.radarShow && this.radarShow.t > 0) {
      const r = this.radarShow, a = Math.min(1, r.t * 2); this.radarShow.t -= 1 / 60;
      g.save(); g.globalAlpha = a; g.fillStyle = 'rgba(10,6,30,.88)'; rr(g, 1300, 120, 280, 120, 18); g.fill(); g.lineWidth = 3; g.strokeStyle = r.color; g.stroke();
      txt(g, r.kind === 'serve' ? 'SERVE SPEED' : r.kind === 'smash' ? 'SMASH SPEED' : 'SHOT SPEED', 1440, 145, 16, '#adb5bd', 'center', 'Nunito');
      txt(g, `${r.kmh}`, 1410, 192, 54, r.kmh > 160 ? '#ff922b' : '#fff'); txt(g, 'km/h', 1520, 200, 22, '#ffd43b');
      txt(g, `${r.name}${r.spin != null ? ` · ${r.spin > 0.25 ? 'TOPSPIN' : r.spin < -0.15 ? 'SLICE' : 'FLAT'} ${Math.round(Math.abs(r.spin) * 3000 + 300)} rpm` : ''}`, 1440, 228, 15, r.color, 'center', 'Nunito'); g.restore();
    }
    if (this.fastest) txt(g, `⚡ FASTEST: ${this.fastest.p.name} ${this.fastest.kmh} km/h`, 1580, 880, 20, '#ffd43b', 'right', 'Nunito');
    this.overlays(g);
    if (this.phase === 'serve' && !this.tossed) txt(g, this.server.p.bot ? '' : `${this.server.p.name}: SWING to serve!`, 800, 800, 34, this.server.p.color, 'center', 'Nunito');
    if (this.winTeam != null && this.phase === 'point') txt(g, `🏆 ${this.tn(this.winTeam).toUpperCase()} WINS!`, 800, 600, 92, this.winTeam === 0 ? '#ff4d6d' : '#4dabff');
    if (this.phase === 'cd') { this.cd.draw(g); txt(g, 'Swing when the ring closes · swing TOWARD where you want it · purple = SMASH', 800, 700, 30, '#fff', 'center', 'Nunito'); }
  }
}

/* =====================================================================
   🎳 STRIKE BOWLING — take turns. Tilt / drag to slide the ball and its
   glowing guide line, swing to roll. Strike = 20, spare = 15.
   ===================================================================== */
class StrikeBowling extends Sport3D {
  constructor(c) { super(c); this.aim = 0; }
  start() {
    this.ps = this.c.roster(1); this.frames = this.ps.length > 4 ? 2 : 3; this.c.sfx.crowdOn && this.c.sfx.crowdOn(0.06);
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
    if (this.is3d) { this.scene.remove(this.bowler.root); this.bowler = makePerson(this.cur.color, 'bowl'); this.scene.add(this.bowler.root); this.guide.children.forEach(m => m.material.color.set(this.cur.color)); this.ballM.material.color.set(this.cur.color); }
    this.aimPhase();
  }
  aimPhase() {
    this.phase = 'aim'; this.wait = this.cur.bot ? rnd(1.8, 2.8) : 15; this.ball = null; this.swingAnim = 0; if (this.trail) this.trail.clear();
    this.camZ = -2.8;
    for (const p of this.ps) if (!p.bot) this.c.setLayout(p, p === this.cur ? SWING('bowling', { label: 'ROLL!', hint: 'Tilt or drag to move the 🎯 line on the TV, then swing!' }) : { name: 'msg', emoji: '🎳', text: `${this.cur.name} is bowling`, sub: 'Watch the TV!' });
    this.pop.add(`${this.cur.name} · ${this.roll === 0 ? `FRAME ${this.frameNo}/${this.frames}` : '2ND BALL'}`, 800, 190, this.cur.color, 52, 1.6);
  }
  onAct(p, m) { if (m && m.a === 'swing' && p === this.cur) this.release(clamp(+m.p || 0.6, 0.2, 1), m.q); }
  // 1:1: angle down the lane = where the phone's back points (yaw); null without a phone
  yawOf(raw) { const q = phoneQ(raw || livePhone(this.cur)); if (!q) return null; const n = faceDir(q, 0); return aimYaw(this.cur, n); }
  onButton(p, b) { if (b === 'a' && p === this.cur && !p.bot) this.release(0.6); }
  release(power, raw) {
    if (this.phase !== 'aim') return;
    const aim = this.cur.bot ? this.aim : this.aimT.at(this.cur, this.t);
    const spd = 7 + power * 9, yaw = this.yawOf(raw);
    const x = yaw != null ? 0 : -clamp(aim, -1, 1) * 0.42;
    this.ball = { x, z: 0.3, vx: yaw != null ? Math.sin(clamp(yaw * 0.12, -0.035, 0.035)) * spd : -x * 0.09 + rnd(-0.02, 0.02), vz: spd, gutter: false, rot: 0, y: 0.11 };
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
      if (!b.gutter && b.z < 17.6 && Math.abs(b.x) > 0.53) { b.gutter = true; b.x = Math.sign(b.x) * 0.65; b.vx = 0; b.y = 0.06; this.pop.add('GUTTER!', 800, 300, '#adb5bd', 70, 1.2); this.c.sfx.buzz(); this.c.sfx.say && this.c.sfx.say('Gutter ball.'); this.c.sfx.ooh && this.c.sfx.ooh(); }
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
  knock(pn) { if (pn.down) return; if (!this.pins.some(p => p.down && !p.gone)) this.c.sfx.strike && this.c.sfx.strike(); pn.down = true; pn.dir = Math.atan2(pn.vx, pn.vz); this.c.sfx.bonk(); this.burst(pn.x, 0.25, pn.z, 10, 0xffffff, 3, 0.4, 3); }
  score() {
    this.ts = 1;
    const newly = this.pins.filter(p => p.down && !p.gone).length, before = this.roll === 0 ? 0 : this.firstDown, down = before + newly;
    let gain = newly, msg = `${newly} PIN${newly === 1 ? '' : 'S'}`, color = '#fff';
    this.again = false;
    if (this.roll === 0 && down === 10) { gain = 20; msg = '💥 STRIKE!! 💥'; color = '#ffd43b'; this.c.sfx.say && this.c.sfx.say('Strike!'); this.c.sfx.cheer && this.c.sfx.cheer(1); this.strikes.set(this.cur, this.strikes.get(this.cur) + 1); this.flash = 0.5; this.excite = 1; this.c.shake(30); this.c.sfx.boom(); for (let i = 0; i < 6; i++) this.burst(rnd(-0.6, 0.6), rnd(0.3, 1.5), rnd(18, 20), 60, pick([0xff4d6d, 0xffd43b, 0x4dabff, 0x51cf66]), 6, 1.3, 3); }
    else if (this.roll === 1 && down === 10) { gain = 15 - before; msg = '✨ SPARE! ✨'; color = '#51cf66'; this.c.sfx.say && this.c.sfx.say('Spare!'); this.c.sfx.applause && this.c.sfx.applause(1); this.excite = 0.8; this.burst(0, 1, 19, 80, 0x51cf66, 5, 1.2, 3); this.c.sfx.win(); }
    else if (this.roll === 0) { this.again = true; this.firstDown = down; this.c.sfx.ding(); if (down === 0) msg = 'OOPS… 0'; }
    this.pts.set(this.cur, this.pts.get(this.cur) + gain);
    this.pop.add(msg, 800, 300, color, down === 10 ? 110 : 70, 2);
    if (gain) this.pop.add(`+${gain}`, 800, 420, this.cur.color, 60, 2);
    if (this.again) { this.pins.forEach(p => { if (p.down) p.gone = true; p.vx = p.vz = 0; }); this.roll = 1; }
    this.phase = 'show'; this.wait = down === 10 ? 3 : 2.2;
  }
  end() { if (this.done) return; this.done = this.over = true; this.c.sfx.crowdOff && this.c.sfx.crowdOff(); this.c.finish(rankBy(this.ps, p => this.pts.get(p) * 10 + this.strikes.get(p), p => `${this.pts.get(p)} pts${this.strikes.get(p) ? ` · ${this.strikes.get(p)}× strike` : ''}`)); }
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
    else { this.ballM.position.set(this.cur.bot || !livePhone(this.cur) ? aimX : 0, 0.11, 0.35); this.ballM.rotation.set(0, 0, 0); }
    this.trail.draw(false, 0.25, new THREE.Color(this.cur.color));
    this.bowler.root.position.set(1.55, 0, 1.4); this.bowler.root.rotation.y = -0.6; this.bowler.root.visible = !b || b.z < 4; // stands beside the lane, cheering
    const bq = !this.ball && phoneQ(livePhone(this.cur));
    if (bq) { posePerson(this.bowler, this.t, 0, 0, 'hold'); holdTool(this.bowler, bq, 0); }
    else { freeTool(this.bowler); posePerson(this.bowler, this.t, this.swingAnim, 0, 'bowl'); }
    this.ballM.visible = !bq || !!this.ball;
    this.guide.visible = this.phase === 'aim';
    const ly = this.phase === 'aim' && !this.cur.bot ? this.yawOf() : null;
    if (this.guide.visible && ly != null) { // live 1:1 guide: a line fanning out from the foul line where you point
      const ang = clamp(ly * 0.12, -0.035, 0.035); this.arrow.position.x = 0; this.arrow.rotation.z = -ang * 6;
      this.dots.forEach((d, i) => { const z = 1.8 + i * 0.66; d.position.set(Math.tan(ang) * z, 0.006, z); d.material.opacity = 0.9 * (1 - i / 30) * (0.6 + 0.4 * Math.sin(this.t * 8 - i * 0.6)); });
    } else if (this.guide.visible) { this.arrow.rotation.z = 0; this.arrow.position.x = aimX; this.dots.forEach((d, i) => { const z = 1.8 + i * 0.66; d.position.set(aimX - aimX * 0.09 * (z / 17) * 0, 0.006, z); d.material.opacity = 0.9 * (1 - i / 30) * (0.6 + 0.4 * Math.sin(this.t * 8 - i * 0.6)); }); }
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
    this.c.sfx.crowdOn && this.c.sfx.crowdOn(0.1);
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
  onAct(p, m) { if (m && m.a === 'swing') { if (m.kmh) p.swKmh = +m.kmh; this.swing(p, clamp(+m.p || 0.6, 0.2, 1), m.q); } }
  dirOf(p, raw) { const q = phoneQ(raw || livePhone(p)); if (!q) return null; const n = faceDir(q, 0); return { ang: clamp(aimYaw(p, n), -0.78, 0.78), up: clamp(n.y, -0.4, 0.9) }; }
  onButton(p, b) { if (b === 'a' && !p.bot) this.swing(p, 0.6); }
  swing(p, pow, raw) {
    this.sw.set(p, 1); this.c.sfx.whoosh();
    if (this.phase !== 'pitch' || this.pitch.swings.has(p) || this.pitch.t < -0.2) return;
    this.pitch.swings.set(p, { at: this.pitch.t, pow, kmh: p.swKmh || 0, aim: this.aimT.at(p, this.t), dir: this.dirOf(p, raw) }); p.swKmh = 0;
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
      const aim = s ? s.aim : 0, dir = s && s.dir, ang = type === 'FOUL' ? (Math.random() < 0.5 ? -1 : 1) * rnd(0.85, 1.1) : dir ? clamp(dir.ang + rnd(-0.05, 0.05), -0.78, 0.78) : clamp(-aim * 0.7 + rnd(-0.08, 0.08), -0.78, 0.78);
      const exit = s && d ? Math.round((s.kmh || 70 + s.pow * 80) * (0.8 + (d / 150) * 0.5)) : 0; // exit velocity
      if (exit) this.exitBest = Math.max(this.exitBest || 0, exit);
      P.res.push({ p, d, type, ang, exit, fd: type === 'FOUL' ? rnd(30, 60) : d, h: rnd(10, 16) + d * 0.08 });
      if (!p.bot) this.c.send(p, { t: 'toast', text: type === 'HOME RUN' ? `💥 HOME RUN! ${d} m` : d ? `${type}! ${d} m` : type });
    }
    const hrs = P.res.filter(r => r.type === 'HOME RUN');
    if (hrs.length) { this.pop.add(hrs.length > 1 ? `💥 ${hrs.length}× HOME RUN!! 💥` : `💥 HOME RUN ${hrs[0].p.name}! 💥`, 800, 230, '#ffd43b', 80, 2.2); this.flash = 0.45; this.c.shake(24); this.c.sfx.boom(); this.excite = 1; this.c.sfx.say && this.c.sfx.say('Home run!'); this.c.sfx.bat && this.c.sfx.bat(); this.c.sfx.cheer && this.c.sfx.cheer(1.3); }
    else if (P.res.some(r => r.d)) { this.c.sfx.bat ? this.c.sfx.bat() : this.c.sfx.hit(); this.c.shake(8); this.excite = 0.6; this.c.sfx.cheer && this.c.sfx.cheer(0.5); }
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
  end() { if (this.done) return; this.done = this.over = true; this.c.sfx.crowdOff && this.c.sfx.crowdOff(); this.c.finish(rankBy(this.ps, p => this.dist.get(p) * 10 + this.hr.get(p), p => `${this.dist.get(p)} m${this.hr.get(p) ? ` · ${this.hr.get(p)} HR` : ''}`)); }
  sync() {
    const P = this.pitch;
    // pitcher wind-up
    const wind = P && this.phase === 'pitch' ? clamp((P.t + 1.1) / 1.1, 0, 1) : 0;
    posePerson(this.pitcher, this.t, wind > 0 && wind < 1 ? 1 - wind : 0, 0, 'bowl');
    this.ps.forEach((p, i) => { const P = this.bat.get(p), q = phoneQ(livePhone(p)); if (q) { posePerson(P, this.t, 0, 0, 'hold'); if (!P.qs) P.qs = q.clone(); P.qs.slerp(q, 0.6); holdTool(P, P.qs, 0); } else { freeTool(P); posePerson(P, this.t, this.sw.get(p), 0, 'bat'); } });
    const pitching = this.phase === 'pitch' && P && P.t >= 0;
    this.ballM.visible = pitching; this.ring.visible = this.phase === 'pitch' && P && P.t > -0.5;
    if (pitching) { const b = this.ballAt(P.t); this.ballM.position.set(b.x, b.y, b.z); this.ptrail.push(b.x, b.y, b.z); }
    this.ptrail.draw(P && P.kind === 'CRAZY BALL' && pitching, 0.22);
    if (this.ring.visible) { const left = P.T - P.t, k = clamp(left / 1.0, 0, 1); this.ring.scale.setScalar(0.45 + k * 2.6); this.ring.material.color.set(Math.abs(left) < 0.07 ? 0x51cf66 : 0xffd43b); this.ring.lookAt(this.cam.position); }
    // aim markers in the outfield (live)
    for (const [p, a] of this.aimM) { const dd = this.dirOf(p), ang = dd ? dd.ang : -clamp((p.input && p.input.x) || 0) * 0.7, r = 95; a.position.x += (Math.sin(ang) * r - a.position.x) * 0.25; a.position.z += (Math.cos(ang) * r - a.position.z) * 0.25; a.visible = this.phase === 'pitch' || this.phase === 'cd'; }
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
      rows.forEach((r, i) => { const y = 148 + i * 38; g.fillStyle = r.p.color; g.beginPath(); g.arc(1276, y, 9, 0, TAU); g.fill(); txt(g, r.p.name.slice(0, 10), 1294, y, 19, '#fff', 'left', 'Nunito'); txt(g, r.d ? `${r.d} m${r.exit ? ` · ${r.exit}km/h` : ''}` : r.type, 1564, y, 18, r.type === 'HOME RUN' ? '#ffd43b' : r.d ? '#fff' : '#ff8787', 'right'); });
    }
    strip(g, rankBy(this.ps, p => this.dist.get(p), () => '').map(r => ({ name: r.p.name, color: r.p.color, val: this.dist.get(r.p) + 'm' })));
    txt(g, `PITCH ${Math.min(this.n, this.N)}/${this.N}`, 1500, 110, 22, '#adb5bd');
    this.overlays(g);
    if (this.phase === 'cd') { this.cd.draw(g); txt(g, 'Turn your phone — or point your arm 📷 — to aim your 🎯', 800, 700, 34, '#fff', 'center', 'Nunito'); }
  }
}
