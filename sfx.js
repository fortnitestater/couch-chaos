/* Synthesized sound effects — no audio files needed */
const SFX = (() => {
  let ac = null, master = null, crowd = null;
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
    // ---- sports / stadium ----
    pok(power = 0.7) { if (!ok('pk', 40)) return; tone(320 + power * 180, 0.06, 'triangle', 0.5); noise(0.07, 0.5 + power * 0.3, 4200); }, // racket hits ball
    smash() { noise(0.25, 0.9, 6000); tone(180, 0.25, 'sawtooth', 0.3, 0.4); tone(90, 0.4, 'sine', 0.5, 0.5); },
    bounce() { ok('bo', 60) && tone(150, 0.07, 'sine', 0.5, 0.6); },
    netHit() { noise(0.25, 0.4, 900); tone(90, 0.2, 'sine', 0.3); },
    swish() { ok('sw', 120) && bandNoise(0.28, 0.25, 1800, 4); },
    // crowd: a looping murmur, plus cheers / "ooh" / applause swells on top
    crowdOn(level = 0.12) { if (!ac) return; if (!crowd) crowd = loopNoise(); crowd.g.gain.setTargetAtTime(level, ac.currentTime, 0.4); },
    crowdOff() { if (crowd && ac) crowd.g.gain.setTargetAtTime(0, ac.currentTime, 0.4); },
    cheer(k = 1) { bandNoise(2.2 * k + 0.6, 0.55 * k, 1100, 0.7, 0.35); bandNoise(1.6 * k, 0.3 * k, 2600, 1.2, 0.2); },
    ooh() { if (!ac) return; for (let i = 0; i < 6; i++) tone(180 + Math.random() * 120, 0.9, 'sine', 0.05, 0.75, i * 0.02); bandNoise(0.9, 0.18, 500, 1.5, 0.2); },
    applause(k = 1) { if (!ac) return; for (let i = 0; i < 40 * k; i++) noiseClap(Math.random() * 2.2 * k); },
    // announcer: the device's built-in voice (free). Replaceable by recorded clips later.
    say(text, o = {}) {
      try {
        if (!('speechSynthesis' in window) || SFX.muteVoice) return;
        const u = new SpeechSynthesisUtterance(text); u.rate = o.rate || 1.05; u.pitch = o.pitch || 0.9; u.volume = 1;
        const vs = speechSynthesis.getVoices(), v = vs.find(x => /en-GB|en_GB/.test(x.lang) && /male|daniel|george|arthur/i.test(x.name)) || vs.find(x => /^en/.test(x.lang));
        if (v) u.voice = v;
        if (o.interrupt !== false) speechSynthesis.cancel();
        speechSynthesis.speak(u);
      } catch (e) {}
    },
  };
  function noiseBuf(sec) { const n = Math.floor(ac.sampleRate * sec), b = ac.createBuffer(1, n, ac.sampleRate), d = b.getChannelData(0); let l = 0; for (let i = 0; i < n; i++) { l = l * 0.97 + (Math.random() * 2 - 1) * 0.03; d[i] = (Math.random() * 2 - 1) * 0.6 + l * 8; } return b; }
  function bandNoise(dur, vol, freq, q = 1, attack = 0.02) {
    if (!ac) return; const t = ac.currentTime, src = ac.createBufferSource(); src.buffer = noiseBuf(dur + 0.1);
    const f = ac.createBiquadFilter(); f.type = 'bandpass'; f.frequency.value = freq; f.Q.value = q;
    const g = ac.createGain(); g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(vol, t + attack); g.gain.exponentialRampToValueAtTime(0.001, t + dur);
    src.connect(f); f.connect(g); g.connect(master); src.start(t); src.stop(t + dur + 0.1);
  }
  function noiseClap(delay) { if (!ac) return; const t = ac.currentTime + delay, src = ac.createBufferSource(); src.buffer = noiseBuf(0.05); const f = ac.createBiquadFilter(); f.type = 'bandpass'; f.frequency.value = 1200 + Math.random() * 1800; f.Q.value = 2; const g = ac.createGain(); g.gain.setValueAtTime(0.12 + Math.random() * 0.1, t); g.gain.exponentialRampToValueAtTime(0.001, t + 0.05); src.connect(f); f.connect(g); g.connect(master); src.start(t); }
  function loopNoise() { // stadium murmur: band-limited noise, slowly breathing
    const src = ac.createBufferSource(); src.buffer = noiseBuf(4); src.loop = true;
    const f = ac.createBiquadFilter(); f.type = 'bandpass'; f.frequency.value = 650; f.Q.value = 0.6;
    const g = ac.createGain(); g.gain.value = 0; const lfo = ac.createOscillator(), lg = ac.createGain(); lfo.frequency.value = 0.15; lg.gain.value = 0.03; lfo.connect(lg); lg.connect(g.gain); lfo.start();
    src.connect(f); f.connect(g); g.connect(master); src.start(); return { src, g };
  }
})();
