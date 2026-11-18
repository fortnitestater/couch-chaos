/* Synthesized sound effects — no audio files needed */
const SFX = (() => {
  let ac = null, master = null;
  const last = {};
  function init() {
    if (ac) return;
    try {
      ac = new (window.AudioContext || window.webkitAudioContext)();
      master = ac.createGain(); master.gain.value = 0.35; master.connect(ac.destination);
    } catch (e) { ac = null; }
  }
  function tone(f, dur, type = 'square', vol = 0.3, slide = 0, delay = 0) {
    if (!ac) return;
    const t = ac.currentTime + delay;
    const o = ac.createOscillator(), g = ac.createGain();
    o.type = type; o.frequency.setValueAtTime(f, t);
    if (slide) o.frequency.exponentialRampToValueAtTime(Math.max(20, f * slide), t + dur);
    g.gain.setValueAtTime(vol, t); g.gain.exponentialRampToValueAtTime(0.001, t + dur);
    o.connect(g); g.connect(master); o.start(t); o.stop(t + dur + 0.02);
  }
  function noise(dur, vol = 0.4, freq = 800) {
    if (!ac) return;
    const t = ac.currentTime, n = Math.floor(ac.sampleRate * dur);
    const buf = ac.createBuffer(1, n, ac.sampleRate), d = buf.getChannelData(0);
    for (let i = 0; i < n; i++) d[i] = Math.random() * 2 - 1;
    const src = ac.createBufferSource(); src.buffer = buf;
    const f = ac.createBiquadFilter(); f.type = 'lowpass'; f.frequency.setValueAtTime(freq, t); f.frequency.exponentialRampToValueAtTime(60, t + dur);
    const g = ac.createGain(); g.gain.setValueAtTime(vol, t); g.gain.exponentialRampToValueAtTime(0.001, t + dur);
    src.connect(f); f.connect(g); g.connect(master); src.start(t);
  }
  const ok = (k, ms) => { const n = performance.now(); if (last[k] && n - last[k] < ms) return false; last[k] = n; return true; };
  return {
    init,
    shoot() { ok('s', 50) && tone(880, 0.07, 'square', 0.06, 0.5); },
    hit() { ok('h', 40) && tone(220, 0.1, 'sawtooth', 0.15, 0.6); },
    boom() { ok('bm', 60) && noise(0.8, 0.7, 1400); },
    tink() { ok('t', 50) && tone(1800, 0.08, 'triangle', 0.15); },
    pickup() { tone(660, 0.08, 'square', 0.15); tone(990, 0.14, 'square', 0.15, 1, 0.08); },
    dash() { ok('d', 60) && tone(300, 0.15, 'sawtooth', 0.12, 2.5); },
    bonk() { ok('b', 70) && tone(160, 0.14, 'sine', 0.5, 0.5); },
    splash() { noise(0.6, 0.5, 700); tone(500, 0.5, 'sine', 0.2, 0.2); },
    card() { noise(0.09, 0.3, 3500); },
    blip() { tone(520, 0.06, 'square', 0.12); },
    tick() { tone(700, 0.06, 'square', 0.12); },
    go() { tone(523, 0.1, 'square', 0.2); tone(784, 0.3, 'square', 0.2, 1, 0.1); },
    ding() { tone(1046, 0.18, 'triangle', 0.3); },
    buzz() { tone(110, 0.35, 'sawtooth', 0.25); },
    alarm() { for (let i = 0; i < 3; i++) tone(880, 0.12, 'square', 0.14, 0.7, i * 0.18); },
    win() { [523, 659, 784, 1046].forEach((f, i) => tone(f, 0.2, 'triangle', 0.25, 1, i * 0.12)); },
    zap() { noise(0.3, 0.4, 5000); tone(1500, 0.3, 'sawtooth', 0.15, 0.1); },
    join() { tone(660, 0.08, 'triangle', 0.2); tone(880, 0.12, 'triangle', 0.2, 1, 0.08); },
    whoosh() { noise(0.5, 0.35, 2500); },
  };
})();
