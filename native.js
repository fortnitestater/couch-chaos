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
// render the 3D at the screen's real resolution (1600x900 is only the layout size): 1080p TV = 1.2x, 4K = 2x (max)
function nativeScale() {
  const q = (location.search.match(/[?&]res=([\d.]+)/) || [])[1]; if (q) return Math.max(1, Math.min(2, +q));
  const px = Math.max(innerWidth, innerHeight * 16 / 9) * (window.devicePixelRatio || 1);
  return Math.max(1, Math.min(2, Math.round(px / 1600 * 20) / 20));
}
function nativeSetScale(rs) { if (NATIVE.mod && NATIVE.mod._cc_set_scale && Math.abs(rs - NATIVE.rs) > 0.01) { NATIVE.rs = rs; NATIVE.mod._cc_set_scale(rs); } }
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
        NATIVE.mod = m; NATIVE.rs = nativeScale(); m._cc_init(NATIVE.rs); ok(m);
        // rigged character models: fetched in the background, handed to the engine as they arrive
        ['p0.glb', 'p1.glb', 'p2.glb', 'p3.glb', 'sofa.glb', 'armchair.glb', 'loveseat.glb', 'fireplace.glb', 'snakeplant.glb', 'monstera.glb', 'floorlamp.glb', 'sidetable.glb', 'chair.glb', 'ads.png', 'screen.png', 'banner.png', 'carpet.png', 'neon.png', 'pizza.glb', 'pizzabox.glb', 'burger.glb', 'fries.glb', 'cake.glb'].forEach(f => fetch(NATIVE.base + 'models/' + f + '?v=' + NATIVE.v).then(r => r.ok ? r.arrayBuffer() : null).then(b => {
          if (!b) return; const u = new Uint8Array(b), p = m._malloc(u.length); m.HEAPU8.set(u, p);
          m.ccall('cc_put_file', null, ['string', 'number', 'number'], [f, p, u.length]); m._free(p);
        }).catch(() => {}));
      }, bad);
    s.onerror = bad; document.head.appendChild(s);
  });
  NATIVE.loading.catch(() => { NATIVE.loading = null; });
  return NATIVE.loading;
}
const NIMG = {};
function nativeImg(k) {
  let im = NIMG[k];
  if (!im) { im = NIMG[k] = new Image(); im.decoding = 'async'; im.src = NATIVE.base + 'covers/' + k.replace(/^_/, 'q-') + '.jpg?v=' + NATIVE.v; im.onerror = () => { im.bad = true; }; }
  return im.complete && im.naturalWidth && !im.bad ? im : null;
}
function nRgba(c) { return `rgba(${c >>> 24},${(c >> 16) & 255},${(c >> 8) & 255},${(c & 255) / 255})`; }
function nGold(g, y0, y1, a = 1) {
  const gr = g.createLinearGradient(0, y0, 0, y1);
  gr.addColorStop(0, `rgba(255,243,206,${a})`); gr.addColorStop(0.45, `rgba(242,198,109,${a})`); gr.addColorStop(0.75, `rgba(196,140,58,${a})`); gr.addColorStop(1, `rgba(246,215,139,${a})`);
  return gr;
}
function nativeWrap(g, s, maxW) { const out = []; let line = ''; for (const w of String(s).split(' ')) { const t = line ? line + ' ' + w : w; if (g.measureText(t).width > maxW && line) { out.push(line); line = w; } else line = t; } if (line) out.push(line); return out; }
// draws the native frame + its text list onto a 2D canvas (in 1600x900 space)
function nativeBlit(g, texts) {
  g.drawImage(NATIVE.cv, 0, 0, 1600, 900);
  let clipped = false;
  for (const t of texts) {
    if (t[0] === 'img') {
      const [, k, x, y, w, h, r, a, pan, zoom] = t, im = nativeImg(k); if (!im) continue;
      g.save(); g.globalAlpha = Math.min(1, a); g.beginPath(); g.roundRect(x, y, w, h, r); g.clip();
      const sc = Math.max(w / im.naturalWidth, h / im.naturalHeight) * (zoom || 1), dw = im.naturalWidth * sc, dh = im.naturalHeight * sc;
      g.drawImage(im, x - (dw - w) * (0.5 + (pan || 0) * 0.5), y - (dh - h) * 0.42, dw, dh);
      g.restore(); continue;
    }
    if (t[0] === 'rr') {
      const [, x, y, w, h, r, f1, f2, st, lw, grad, gold] = t;
      g.save(); g.beginPath(); g.roundRect(x, y, w, h, r);
      if (f1 || f2) {
        if (grad) { const gr = g.createLinearGradient(0, y, 0, y + h); gr.addColorStop(0, nRgba(f1)); gr.addColorStop(1, nRgba(f2)); g.fillStyle = gr; } else g.fillStyle = nRgba(f1);
        g.fill();
      }
      if (lw > 0 && (st & 255)) { g.lineWidth = lw; g.strokeStyle = gold ? nGold(g, y, y + h, (st & 255) / 255) : nRgba(st); g.beginPath(); g.roundRect(x + lw / 2, y + lw / 2, w - lw, h - lw, Math.max(0, r - lw / 2)); g.stroke(); }
      g.restore(); continue;
    }
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
    if (style & 64) g.letterSpacing = (s * 0.14).toFixed(1) + 'px';
    const lines = w > 0 ? nativeWrap(g, str, w) : [str], lh = s * (style & 1 ? 1.05 : 1.3);
    if (style & 4) { g.shadowColor = `rgba(0,0,0,${(0.45 * (c & 255) / 255).toFixed(3)})`; g.shadowBlur = s * 0.35; g.shadowOffsetY = s * 0.08; }
    if (!(style & 2)) { g.lineJoin = 'round'; g.lineWidth = Math.max(3, s / 6); g.strokeStyle = `rgba(14,10,36,${(0.8 * (c & 255) / 255).toFixed(3)})`; }
    const al1 = (c & 255) / 255;
    if (style & 32) { g.fillStyle = nGold(g, y - s * 0.5, y + s * 0.5 + (lines.length - 1) * lh, al1); if (style & 4) { g.shadowColor = `rgba(242,180,80,${0.35 * al1})`; g.shadowBlur = s * 0.45; g.shadowOffsetY = 0; } }
    else if (style & 128) { const gr = g.createLinearGradient(0, y - s * 0.5, 0, y + s * 0.5); gr.addColorStop(0, `rgba(255,255,255,${al1})`); gr.addColorStop(0.6, `rgba(214,222,240,${al1})`); gr.addColorStop(1, `rgba(160,172,204,${al1})`); g.fillStyle = gr; }
    else g.fillStyle = col;
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
    this.ps.forEach((p, i) => { if (!p.bot) { const n = p.input || {}; m._cc_input(i, +n.x || 0, +n.y || 0, (n.a ? 1 : 0) | (n.touch ? 2 : 0), n.b ? 1 : 0); } });
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
    if (m.a === 'stroke' && Array.isArray(m.pts)) {  // doodles: "stroke:id|#color|width|x,y;x,y;..."
      const pts = m.pts.slice(0, 200).filter(Array.isArray).map(q => (+q[0] || 0).toFixed(3) + ',' + (+q[1] || 0).toFixed(3)).join(';');
      NATIVE.mod.ccall('cc_act', null, ['number', 'string', 'number', 'number'], [i, `stroke:${String(m.id || '').replace(/[|:]/g, '')}|${String(m.c || '#000').slice(0, 9)}|${+m.w || 6}|${pts}`, 0, 0]);
      return;
    }
    const kind = String(m.a || '') + (m.id ? ':' + m.id : ''), n = m.s ? ['♠', '♥', '♦', '♣'].indexOf(m.s) : +m.i || 0;
    NATIVE.mod.ccall('cc_act', null, ['number', 'string', 'number', 'number'], [i, kind, n, +m.p || 0]);
  }
  resync(p) { const i = this.ps.indexOf(p); if (i >= 0 && this.sent && this.sent[i]) this.c.send(p, this.sent[i]); }
  // ---- called from C++ ----
  layout(pid, json) { const L = JSON.parse(json); if (pid < 0) this.c.setLayout('all', L); else if (this.ps[pid]) this.c.setLayout(this.ps[pid], L); }
  sfx(n) {
    if (n.startsWith('say:')) { const s = this.c.sfx.say; if (s) s.call(this.c.sfx, n.slice(4)); return; }   // an official speaks (recorded umpire calls)
    const f = this.c.sfx[n]; if (typeof f === 'function') f.call(this.c.sfx);
    // the host reacts to reveals now and then
    const H = this.c.sfx.host; if (!H) return;
    if (n === 'ding' && Math.random() < 0.55) H.call(this.c.sfx, Math.random() < 0.6 ? 'correct' : 'what a play', { gap: 6 });
    else if (n === 'buzz' && Math.random() < 0.5) H.call(this.c.sfx, Math.random() < 0.5 ? 'nobody' : 'so close', { gap: 6 });
    else if (n === 'boom') H.call(this.c.sfx, 'boom', { gap: 4 });
  }
  banner(s, ms, col) {
    this.c.banner(s, ms, col);
    const H = this.c.sfx.host, t = String(s).toUpperCase(); if (!H) return;
    if (/FINAL ROUND|LAST ROUND/.test(t)) H.call(this.c.sfx, 'final round', { force: true });
    else if (/ROUND 1\b|ROUND ONE|HAND 1\b/.test(t)) H.call(this.c.sfx, 'round one', { gap: 2 });
    else if (/ROUND \d|NEXT ROUND|HAND \d/.test(t)) H.call(this.c.sfx, 'next round', { gap: 5 });
    else if (/TIME'?S UP|TIME UP/.test(t)) H.call(this.c.sfx, 'times up', { gap: 2 });
    else if (/\bTIE\b|DRAW!/.test(t)) H.call(this.c.sfx, 'tie', { force: true });
    else if (/WINS|WINNER|CHAMPION/.test(t)) H.call(this.c.sfx, 'what a game', { force: true });
    else if (/YOUR TURN|'S TURN/.test(t)) H.call(this.c.sfx, 'your turn', { gap: 8 });
    else if (/BOOM|EXPLOD|BUST/.test(t)) H.call(this.c.sfx, 'ouch', { gap: 4 });
  }
  sendTo(pid, json) { const p = this.ps[pid]; if (!p || p.bot) return; const m = JSON.parse(json); if (m.t !== 'snap' && m.t !== 'paint') (this.sent = this.sent || {})[pid] = m; this.c.send(p, m); }
  buzz(pid, ms) { const p = this.ps[pid]; if (p && !p.bot) this.c.send(p, { t: 'buzz', ms }); }
  finish(s) {
    if (this.dead) return; this.dead = true;
    const r = s.split(';').filter(Boolean).map(x => { const k = x.indexOf(':'); return { p: this.ps[+x.slice(0, k)], stat: x.slice(k + 1) }; }).filter(r => r.p);
    if (NATIVE.mod) setTimeout(() => { if (NATIVE.mod.game === this) NATIVE.mod._cc_stop(); }, 0); // not mid-frame
    this.c.finish(r);
  }
}
