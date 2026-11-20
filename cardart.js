/* Couch Chaos — card art for the TV canvas. Real-looking cards: card stock with a soft sheen,
   chunky 3D numerals, corner indices, a patterned back, and a little physics in how they move.
   Loaded after the games, so drawCard / drawStormCard here replace the older flat versions. */

const CARD_INK = { R: '#e8333f', B: '#1f7ae0', G: '#22a35a', Y: '#f6b21a' };
function shadeHex(hex, k) { // k < 0 darkens, k > 0 lightens
  const n = parseInt(hex.slice(1), 16), r = n >> 16, g = (n >> 8) & 255, b = n & 255, f = c => Math.round(k < 0 ? c * (1 + k) : c + (255 - c) * k);
  return `rgb(${f(r)},${f(g)},${f(b)})`;
}
function cardPaper(g, w, h) {
  const r = w * 0.09;
  g.shadowColor = 'rgba(0,0,0,.38)'; g.shadowBlur = w * 0.16; g.shadowOffsetY = w * 0.06;
  const gr = g.createLinearGradient(-w / 2, -h / 2, w / 2, h / 2); gr.addColorStop(0, '#ffffff'); gr.addColorStop(0.6, '#f7f5f0'); gr.addColorStop(1, '#ebe7dc');
  g.fillStyle = gr; rr(g, -w / 2, -h / 2, w, h, r); g.fill();
  g.shadowColor = 'transparent';
  g.lineWidth = Math.max(1, w * 0.012); g.strokeStyle = 'rgba(0,0,0,.12)'; g.stroke();
}
// chunky 3D numeral: extruded edge + dark outline + colored face + top sheen
function cardNumeral(g, s, size, ink, x = 0, y = 0) {
  const dark = shadeHex(ink, -0.55);
  g.font = `700 ${size}px Fredoka, Inter, system-ui, sans-serif`; g.textAlign = 'center'; g.textBaseline = 'middle'; g.lineJoin = 'round';
  for (let k = 7; k >= 1; k--) { g.fillStyle = dark; g.fillText(s, x + k * size * 0.007, y + k * size * 0.011); }
  g.lineWidth = size * 0.1; g.strokeStyle = dark; g.strokeText(s, x, y);
  g.fillStyle = ink; g.fillText(s, x, y);
  g.save(); g.beginPath(); g.rect(x - size * 1.5, y - size * 0.62, size * 3, size * 0.42); g.clip(); g.globalAlpha = 0.3; g.fillStyle = '#fff'; g.fillText(s, x, y); g.restore();
}
function cornerIndex(g, s, w, h, ink, extra) {
  for (const flip of [1, -1]) {
    g.save(); if (flip < 0) g.rotate(Math.PI);
    g.font = `700 ${w * 0.17}px Fredoka, Inter, sans-serif`; g.textAlign = 'center'; g.textBaseline = 'middle'; g.fillStyle = ink;
    g.fillText(s, -w / 2 + w * 0.15, -h / 2 + w * 0.17);
    if (extra) { g.font = `${w * 0.13}px serif`; g.fillText(extra, -w / 2 + w * 0.15, -h / 2 + w * 0.34); }
    g.restore();
  }
}
/* the back: deep navy with a woven diamond pattern and an emblem */
let _backPat = null;
function backPattern(g) {
  if (_backPat) return _backPat;
  const c = document.createElement('canvas'); c.width = c.height = 24; const p = c.getContext('2d');
  p.fillStyle = '#1b2a6b'; p.fillRect(0, 0, 24, 24); p.strokeStyle = 'rgba(255,209,102,.55)'; p.lineWidth = 1.4;
  p.beginPath(); p.moveTo(12, 0); p.lineTo(24, 12); p.lineTo(12, 24); p.lineTo(0, 12); p.closePath(); p.stroke();
  p.fillStyle = 'rgba(92,225,230,.5)'; p.beginPath(); p.arc(12, 12, 2, 0, TAU); p.fill();
  return (_backPat = g.createPattern(c, 'repeat'));
}
function drawCardBack(g, w, h) {
  cardPaper(g, w, h);
  const m = w * 0.07, r = w * 0.07;
  g.save(); rr(g, -w / 2 + m, -h / 2 + m, w - m * 2, h - m * 2, r); g.clip();
  g.fillStyle = (typeof document !== 'undefined') ? backPattern(g) : '#1b2a6b'; g.translate(-w / 2, -h / 2); g.fillRect(0, 0, w, h); g.translate(w / 2, h / 2);
  const gr = g.createRadialGradient(0, 0, 0, 0, 0, h * 0.6); gr.addColorStop(0, 'rgba(61,139,255,.35)'); gr.addColorStop(1, 'rgba(7,8,15,.45)'); g.fillStyle = gr; g.fillRect(-w, -h, w * 2, h * 2);
  g.restore();
  g.fillStyle = '#0b0d17'; g.beginPath(); g.ellipse(0, 0, w * 0.27, h * 0.2, 0, 0, TAU); g.fill();
  g.lineWidth = w * 0.025; g.strokeStyle = '#ffd166'; g.stroke();
  g.font = `700 ${w * 0.2}px Fredoka, Inter, sans-serif`; g.textAlign = 'center'; g.textBaseline = 'middle'; g.fillStyle = '#ffd166'; g.fillText('CC', 0, 1);
}

/* 🌪️ Color Storm card */
function drawStormCard(g, x, y, w, h, c, back, rot = 0) {
  g.save(); g.translate(x, y); g.rotate(rot);
  if (back || !c) { drawCardBack(g, w, h); g.restore(); return; }
  cardPaper(g, w, h);
  const ink = c.c ? CARD_INK[c.c] : null, t = stormText(c);
  if (ink) {
    // a soft color wash in an oval, like a printed card
    g.save(); g.beginPath(); g.ellipse(0, 0, w * 0.4, h * 0.36, -0.45, 0, TAU); g.fillStyle = shadeHex(ink, 0.82); g.fill(); g.restore();
    cardNumeral(g, t, t.length > 1 ? w * 0.42 : w * 0.66, ink, 0, h * 0.02);
    cornerIndex(g, t, w, h, ink);
  } else { // wild: four-color pinwheel
    const R = w * 0.33;
    ['R', 'Y', 'G', 'B'].forEach((k, i) => { g.beginPath(); g.moveTo(0, 0); g.arc(0, 0, R, i * Math.PI / 2 - Math.PI / 4, (i + 1) * Math.PI / 2 - Math.PI / 4); g.closePath(); g.fillStyle = CARD_INK[k]; g.fill(); });
    g.lineWidth = w * 0.03; g.strokeStyle = '#fff'; g.beginPath(); g.arc(0, 0, R, 0, TAU); g.stroke();
    cardNumeral(g, c.v === 'BLIZZARD' ? '+4' : c.v === 'SWAP' ? '⇄' : '★', w * 0.32, '#1a1d29', 0, 0);
    g.font = `700 ${w * 0.11}px Inter, sans-serif`; g.textAlign = 'center'; g.fillStyle = '#1a1d29'; g.fillText(c.v, 0, h * 0.4);
    cornerIndex(g, c.v === 'BLIZZARD' ? '+4' : c.v === 'SWAP' ? '⇄' : 'W', w, h, '#1a1d29');
  }
  g.restore();
}

/* 🃏 standard playing card (Wild Eights, Higher or Lower) */
function drawCard(g, x, y, w, h, card, rot = 0) {
  g.save(); g.translate(x, y); g.rotate(rot);
  if (!card) { drawCardBack(g, w, h); g.restore(); return; }
  cardPaper(g, w, h);
  const r = cRank(card), s = cSuit(card), red = '♥♦'.includes(s), ink = red ? '#d62839' : '#1a1d29', tag = typeof CARD_TAG !== 'undefined' && CARD_TAG[r];
  cornerIndex(g, r, w, h, ink, s);
  if ('JQK'.includes(r)) { // court card: framed panel + big letter
    g.lineWidth = w * 0.02; g.strokeStyle = shadeHex(red ? '#d62839' : '#3d4a8a', 0.2); rr(g, -w * 0.3, -h * 0.3, w * 0.6, h * 0.6, w * 0.05); g.stroke();
    g.fillStyle = red ? 'rgba(214,40,57,.08)' : 'rgba(61,74,138,.08)'; g.fill();
    cardNumeral(g, r, w * 0.42, red ? '#d62839' : '#3d4a8a', 0, -h * 0.03);
    g.font = `${w * 0.18}px serif`; g.textAlign = 'center'; g.fillStyle = ink; g.fillText(s, 0, h * 0.2);
  } else { g.font = `${w * 0.5}px serif`; g.textAlign = 'center'; g.textBaseline = 'middle'; g.fillStyle = ink; g.fillText(s, 0, h * 0.02); }
  if (tag) { g.fillStyle = '#3d8bff'; rr(g, -w * 0.3, h * 0.31, w * 0.6, h * 0.1, h * 0.05); g.fill(); g.font = `700 ${w * 0.085}px Inter, sans-serif`; g.fillStyle = '#fff'; g.textBaseline = 'middle'; g.fillText(tag, 0, h * 0.36); }
  g.restore();
}

/* 💣 Avalanche! action card */
function drawAvCard(g, x, y, w, h, key, rot = 0) {
  g.save(); g.translate(x, y); g.rotate(rot);
  if (!key) { drawCardBack(g, w, h); g.restore(); return; }
  const [icon, label, col] = AV[key];
  cardPaper(g, w, h);
  g.save(); rr(g, -w / 2, -h / 2, w, h, w * 0.09); g.clip(); g.fillStyle = col; g.fillRect(-w / 2, -h / 2, w, h * 0.24); g.restore();
  g.font = `700 ${w * 0.13}px Fredoka, Inter, sans-serif`; g.textAlign = 'center'; g.textBaseline = 'middle'; g.fillStyle = '#fff'; g.fillText(label.toUpperCase(), 0, -h / 2 + h * 0.12);
  g.font = `${w * 0.46}px serif`; g.fillText(icon, 0, h * 0.08);
  g.restore();
}

/* card motion: cards travel on an arc, spin a little, and settle (critically damped) */
class CardFlights {
  constructor() { this.l = []; }
  add(o) { this.l.push(Object.assign({ t: 0, dur: 0.45, delay: 0, rot0: 0, rot1: rnd(-0.12, 0.12), arc: -60, w: 110, h: 160 }, o)); }
  update(dt) { for (const f of this.l) { if (f.delay > 0) { f.delay -= dt; continue; } f.t += dt / f.dur; if (f.t >= 1 && !f.landed) { f.landed = true; f.onLand && f.onLand(); } } this.l = this.l.filter(f => f.t < 1); }
  draw(g, painter) {
    for (const f of this.l) {
      if (f.delay > 0) continue;
      const u = clamp(f.t, 0, 1), e = 1 - Math.pow(1 - u, 3);
      const x = lerp(f.from.x, f.to.x, e), y = lerp(f.from.y, f.to.y, e) + Math.sin(u * Math.PI) * f.arc, sc = 1 + Math.sin(u * Math.PI) * 0.12;
      painter(g, x, y, f.w * sc, f.h * sc, f.card, !f.card, lerp(f.rot0, f.rot1, e) + Math.sin(u * Math.PI) * 0.25);
    }
  }
}
