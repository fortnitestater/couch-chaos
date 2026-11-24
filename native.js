/* Couch Chaos — bridge to the native games (C++ / raylib compiled to WebAssembly, native-games.wasm).
   The C++ side draws the game into its own WebGL canvas; we copy it onto the TV canvas each frame,
   then draw the C++ text requests with the site fonts (so emoji + names look right). */
const NATIVE = (() => { const src = (document.currentScript && document.currentScript.src) || ''; return { mod: null, loading: null, cv: null, base: src.replace(/[?#].*$/, '').replace(/[^/]*$/, ''), v: (src.match(/[?&]v=(\w+)/) || [])[1] || Date.now() }; })();
function nativeLoad() {
  if (NATIVE.loading) return NATIVE.loading;
  const cv = NATIVE.cv = document.createElement('canvas');
  cv.id = 'ccNative'; cv.width = 1600; cv.height = 900;
  cv.style.cssText = 'position:fixed;left:-20000px;top:0;width:1600px;height:900px;pointer-events:none';
  document.body.appendChild(cv);
  NATIVE.loading = new Promise((ok, bad) => {
    const s = document.createElement('script'); s.src = NATIVE.base + 'native-games.js?v=' + NATIVE.v;
    s.onload = () => CCNative({ canvas: cv, print() {}, printErr: e => console.warn('[native]', e), locateFile: f => NATIVE.base + f + '?v=' + NATIVE.v })
      .then(m => { NATIVE.mod = m; m._cc_init(); ok(m); }, bad);
    s.onerror = bad; document.head.appendChild(s);
  });
  NATIVE.loading.catch(() => { NATIVE.loading = null; });
  return NATIVE.loading;
}

class NativeGame {
  constructor(c, id) { this.c = c; this.id = id; this.texts = []; this.ready = false; this.t = 0; }
  start() {
    this.ps = this.c.roster(2);
    nativeLoad().then(m => {
      if (this.dead) return;
      m.game = this;
      const list = this.ps.map(p => `${String(p.name).replace(/[|;]/g, '')}|${p.color}|${p.bot ? 1 : 0}`).join(';');
      m.ccall('cc_start', null, ['string', 'string'], [this.id, list]);
      this.ready = true;
    }).catch(e => { console.error(e); this.c.banner('Could not load this game', 2200, '#ff4d6d'); });
  }
  update(dt) {
    this.t += dt;
    if (!this.ready || this.dead) return;
    const m = NATIVE.mod; m.game = this;
    this.ps.forEach((p, i) => { if (!p.bot) { const n = p.input || {}; m._cc_input(i, +n.x || 0, +n.y || 0, n.a ? 1 : 0, n.b ? 1 : 0); } });
    this.texts.length = 0;
    m._cc_frame(dt);
  }
  draw(g) {
    if (!this.ready) {
      g.fillStyle = '#0b1020'; g.fillRect(0, 0, 1600, 900);
      txt(g, 'Loading…', 800, 450, 44, '#fff');
      g.strokeStyle = '#4dabff'; g.lineWidth = 6; g.lineCap = 'round';
      g.beginPath(); g.arc(800, 540, 26, this.t * 5, this.t * 5 + 4); g.stroke();
      return;
    }
    g.drawImage(NATIVE.cv, 0, 0, 1600, 900);
    for (const [x, y, s, c, str, al] of this.texts)
      txt(g, str, x, y, s, `rgba(${c >>> 24},${(c >> 16) & 255},${(c >> 8) & 255},${(c & 255) / 255})`, al < 0 ? 'left' : al > 0 ? 'right' : 'center', 'Nunito');
  }
  onAct(p, m) { const i = this.ps.indexOf(p); if (i >= 0 && this.ready) NATIVE.mod.ccall('cc_act', null, ['number', 'string', 'number', 'number'], [i, String(m.a || ''), +m.i || 0, +m.p || 0]); }
  // ---- called from C++ ----
  layout(pid, json) { const L = JSON.parse(json); if (pid < 0) this.c.setLayout('all', L); else if (this.ps[pid]) this.c.setLayout(this.ps[pid], L); }
  sfx(n) { const f = this.c.sfx[n]; if (typeof f === 'function') f.call(this.c.sfx); }
  banner(s, ms, col) { this.c.banner(s, ms, col); }
  buzz(pid, ms) { const p = this.ps[pid]; if (p && !p.bot) this.c.send(p, { t: 'buzz', ms }); }
  finish(s) {
    if (this.dead) return; this.dead = true;
    const r = s.split(';').filter(Boolean).map(x => { const k = x.indexOf(':'); return { p: this.ps[+x.slice(0, k)], stat: x.slice(k + 1) }; }).filter(r => r.p);
    if (NATIVE.mod) setTimeout(() => { if (NATIVE.mod.game === this) NATIVE.mod._cc_stop(); }, 0); // not mid-frame
    this.c.finish(r);
  }
}
