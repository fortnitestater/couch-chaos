/* Couch Chaos — bridge to the native engine (C++ / raylib compiled to WebAssembly, native-games.wasm).
   C++ draws the console shell and the games into its own WebGL canvas; we copy that onto the TV canvas,
   then draw C++'s text requests with the site fonts (emoji + names look right). */
const NATIVE = (() => {
  const src = (document.currentScript && document.currentScript.src) || '';
  return { mod: null, loading: null, cv: null, base: src.replace(/[?#].*$/, '').replace(/[^/]*$/, ''), v: (src.match(/[?&]v=(\w+)/) || [])[1] || Date.now() };
})();
const _mctx = document.createElement('canvas').getContext('2d');
function nativeFont(size, style) {
  const w = style & 16 ? 800 : style & 8 ? 500 : 700;
  return style & 1 ? `700 ${size}px Fredoka, Inter, system-ui, sans-serif` : `${w} ${size}px Inter, system-ui, sans-serif`;
}
function nativeLoad() {
  if (NATIVE.loading) return NATIVE.loading;
  const cv = NATIVE.cv = document.createElement('canvas');
  cv.id = 'ccNative'; cv.width = 1600; cv.height = 900;
  cv.style.cssText = 'position:fixed;left:-20000px;top:0;width:1600px;height:900px;pointer-events:none';
  document.body.appendChild(cv);
  NATIVE.loading = new Promise((ok, bad) => {
    const s = document.createElement('script'); s.src = NATIVE.base + 'native-games.js?v=' + NATIVE.v;
    s.onload = () => CCNative({ canvas: cv, print() {}, printErr: e => console.warn('[native]', e), locateFile: f => NATIVE.base + f + '?v=' + NATIVE.v })
      .then(m => {
        m.measure = (s, size, style) => { _mctx.font = nativeFont(size, style); return _mctx.measureText(s).width; };
        NATIVE.mod = m; m._cc_init(); ok(m);
      }, bad);
    s.onerror = bad; document.head.appendChild(s);
  });
  NATIVE.loading.catch(() => { NATIVE.loading = null; });
  return NATIVE.loading;
}
function nativeWrap(g, s, maxW) { const out = []; let line = ''; for (const w of String(s).split(' ')) { const t = line ? line + ' ' + w : w; if (g.measureText(t).width > maxW && line) { out.push(line); line = w; } else line = t; } if (line) out.push(line); return out; }
// draws the native frame + its text list onto a 2D canvas (in 1600x900 space)
function nativeBlit(g, texts) {
  g.drawImage(NATIVE.cv, 0, 0, 1600, 900);
  let clipped = false;
  for (const t of texts) {
    if (t[0] === 'clip') {
      if (clipped) { g.restore(); clipped = false; }
      if (t[3] > 0) { g.save(); g.beginPath(); g.rect(t[1], t[2], t[3], t[4]); g.clip(); clipped = true; }
      continue;
    }
    const [x, y, s, c, str, al, style = 0, w = 0] = t;
    const col = `rgba(${c >>> 24},${(c >> 16) & 255},${(c >> 8) & 255},${(c & 255) / 255})`;
    if (!style) { txt(g, str, x, y, s, col, al < 0 ? 'left' : al > 0 ? 'right' : 'center', 'Nunito'); continue; }
    g.save();
    g.font = nativeFont(s, style); g.textAlign = al < 0 ? 'left' : al > 0 ? 'right' : 'center'; g.textBaseline = 'middle';
    const lines = w > 0 ? nativeWrap(g, str, w) : [str], lh = s * 1.3;
    if (style & 4) { g.shadowColor = 'rgba(0,0,0,.45)'; g.shadowBlur = s * 0.35; g.shadowOffsetY = s * 0.08; }
    if (!(style & 2)) { g.lineJoin = 'round'; g.lineWidth = Math.max(3, s / 6); g.strokeStyle = 'rgba(14,10,36,.8)'; }
    g.fillStyle = col;
    lines.forEach((ln, i) => { const yy = y + i * lh; if (!(style & 2)) g.strokeText(ln, x, yy); g.fillText(ln, x, yy); });
    g.restore();
  }
  if (clipped) g.restore();
}

// ---------- the console shell ----------
const NativeShell = {
  texts: [], last: '',
  ready: () => !!NATIVE.mod,
  frame(dt, snapshot, g) {
    const m = NATIVE.mod; if (!m) return false;
    const js = JSON.stringify(snapshot);
    m.game = this;
    if (js !== this.last) { this.last = js; m.ccall('cc_shell', null, ['string'], [js]); }
    this.texts.length = 0;
    m._cc_shell_frame(dt);
    nativeBlit(g, this.texts);
    return true;
  },
  pick(x, y) { return NATIVE.mod ? NATIVE.mod._cc_shell_pick(x, y) : 0; },
};

// ---------- a game running in C++ ----------
class NativeGame {
  constructor(c, id, min = 2) { this.c = c; this.id = id; this.min = min; this.texts = []; this.ready = false; this.t = 0; }
  start() {
    this.ps = this.c.roster(this.min);
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
    nativeBlit(g, this.texts);
  }
  onPump(p, n) { const i = this.ps.indexOf(p); if (i >= 0 && this.ready) NATIVE.mod.ccall('cc_act', null, ['number', 'string', 'number', 'number'], [i, 'pump', Math.max(1, +n || 1), 0]); }
  onAct(p, m) {
    const i = this.ps.indexOf(p); if (i < 0 || !this.ready) return;
    const kind = String(m.a || '') + (m.id ? ':' + m.id : ''), n = m.s ? ['♠', '♥', '♦', '♣'].indexOf(m.s) : +m.i || 0;
    NATIVE.mod.ccall('cc_act', null, ['number', 'string', 'number', 'number'], [i, kind, n, +m.p || 0]);
  }
  resync(p) { const i = this.ps.indexOf(p); if (i >= 0 && this.sent && this.sent[i]) this.c.send(p, this.sent[i]); }
  // ---- called from C++ ----
  layout(pid, json) { const L = JSON.parse(json); if (pid < 0) this.c.setLayout('all', L); else if (this.ps[pid]) this.c.setLayout(this.ps[pid], L); }
  sfx(n) { const f = this.c.sfx[n]; if (typeof f === 'function') f.call(this.c.sfx); }
  banner(s, ms, col) { this.c.banner(s, ms, col); }
  sendTo(pid, json) { const p = this.ps[pid]; if (!p || p.bot) return; const m = JSON.parse(json); (this.sent = this.sent || {})[pid] = m; this.c.send(p, m); }
  buzz(pid, ms) { const p = this.ps[pid]; if (p && !p.bot) this.c.send(p, { t: 'buzz', ms }); }
  finish(s) {
    if (this.dead) return; this.dead = true;
    const r = s.split(';').filter(Boolean).map(x => { const k = x.indexOf(':'); return { p: this.ps[+x.slice(0, k)], stat: x.slice(k + 1) }; }).filter(r => r.p);
    if (NATIVE.mod) setTimeout(() => { if (NATIVE.mod.game === this) NATIVE.mod._cc_stop(); }, 0); // not mid-frame
    this.c.finish(r);
  }
}
