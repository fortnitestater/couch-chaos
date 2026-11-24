/* Sound: recorded ElevenLabs samples (crowd, cheers, racket, strike, music…) with synthesized fallbacks,
   so everything still works before the files load or offline. */
const SFX = (() => {
  let ac = null, master = null, crowd = null, voiceOut = null, voiceBuf = null, voiceSrc = [];
  // recorded samples (ElevenLabs sound effects + music)
  const SAMPLE_FILES = { crowd: 'crowd', cheer: 'cheer', ooh: 'ooh', applause: 'applause', hit: 'hit', bounce: 'bounce', smash: 'smash', strike: 'strike', bat: 'bat', music: 'music' };
  const S = {}, onset = {}; let crowdS = null, musicS = null, musicWanted = false, musicMuted = false;
  function loadSamples() {
    for (const [k, f] of Object.entries(SAMPLE_FILES)) {
      fetch(`audio/${f}.mp3`).then(r => (r.ok ? r.arrayBuffer() : Promise.reject())).then(b => ac.decodeAudioData(b)).then(buf => {
        S[k] = buf;
        // where the sound actually starts (first point above 40% of the peak) — used to sync hits
        const d = buf.getChannelData(0); let pk = 0; for (let i = 0; i < d.length; i += 8) pk = Math.max(pk, Math.abs(d[i]));
        let i0 = 0; while (i0 < d.length && Math.abs(d[i0]) < pk * 0.4) i0 += 8; onset[k] = Math.max(0, i0 / buf.sampleRate - 0.03);
        if (k === 'crowd' && crowd && crowd.want) startCrowdLoop(crowd.level);
        if (k === 'music' && musicWanted) SFX.musicOn();
      }).catch(() => {});
    }
  }
  function play(k, vol = 1, rate = 1, fromOnset = false) {
    if (!ac || !S[k]) return false;
    const src = ac.createBufferSource(), g = ac.createGain(); src.buffer = S[k]; src.playbackRate.value = rate; g.gain.value = vol;
    src.connect(g); g.connect(master); src.start(ac.currentTime, fromOnset ? onset[k] || 0 : 0); return true;
  }
  function startCrowdLoop(level) {
    if (!S.crowd) return false;
    if (!crowdS) { const src = ac.createBufferSource(), g = ac.createGain(); src.buffer = S.crowd; src.loop = true; g.gain.value = 0; src.connect(g); g.connect(master); src.start(); crowdS = { src, g }; }
    crowdS.g.gain.setTargetAtTime(level * 9, ac.currentTime, 0.5); return true;
  }
  const last = {};
  function init() {
    if (ac) return;
    try {
      ac = new (window.AudioContext || window.webkitAudioContext)();
      master = ac.createGain(); master.gain.value = 0.35; master.connect(ac.destination);
      voiceOut = ac.createGain(); voiceOut.gain.value = 1.0; voiceOut.connect(ac.destination);
      loadSamples();
      if (typeof VOICE !== 'undefined') fetch(VOICE.url).then(r => r.arrayBuffer()).then(b => ac.decodeAudioData(b)).then(buf => { voiceBuf = buf; }).catch(() => {});
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
    pok(power = 0.7) { if (!ok('pk', 40)) return; if (play('hit', 1.4 + power, 0.9 + power * 0.25, true)) return; tone(320 + power * 180, 0.06, 'triangle', 0.5); noise(0.07, 0.5 + power * 0.3, 4200); }, // racket hits ball
    smash() { if (play('smash', 2.2, 1, true)) return; noise(0.25, 0.9, 6000); tone(180, 0.25, 'sawtooth', 0.3, 0.4); tone(90, 0.4, 'sine', 0.5, 0.5); },
    bounce() { if (!ok('bo', 60)) return; if (play('bounce', 1.6, 1, true)) return; tone(150, 0.07, 'sine', 0.5, 0.6); },
    strike() { play('strike', 2, 1, true) || noise(0.8, 0.7, 1400); },
    bat() { play('bat', 2.2, 1, true) || tone(220, 0.1, 'sawtooth', 0.15, 0.6); },
    netHit() { noise(0.25, 0.4, 900); tone(90, 0.2, 'sine', 0.3); },
    swish() { ok('sw', 120) && bandNoise(0.28, 0.25, 1800, 4); },
    // crowd: a looping murmur, plus cheers / "ooh" / applause swells on top
    crowdOn(level = 0.12) {
      if (!ac) return; SFX.musicOff();
      if (startCrowdLoop(level)) { if (crowd && crowd.g) crowd.g.gain.setTargetAtTime(0, ac.currentTime, 0.3); crowd = Object.assign(crowd || {}, { want: true, level }); return; }
      if (!crowd || !crowd.g) crowd = Object.assign(loopNoise(), { want: true, level }); else Object.assign(crowd, { want: true, level });
      crowd.g.gain.setTargetAtTime(level, ac.currentTime, 0.4);
    },
    crowdOff() { if (!ac) return; if (crowd) { crowd.want = false; if (crowd.g) crowd.g.gain.setTargetAtTime(0, ac.currentTime, 0.4); } if (crowdS) crowdS.g.gain.setTargetAtTime(0, ac.currentTime, 0.5); },
    // menu music (looping), M on the TV keyboard mutes it
    musicOn() { musicWanted = true; if (!ac || musicMuted || !S.music) return; if (!musicS) { const src = ac.createBufferSource(), g = ac.createGain(); src.buffer = S.music; src.loop = true; g.gain.value = 0; src.connect(g); g.connect(ac.destination); src.start(); musicS = { src, g }; } musicS.g.gain.setTargetAtTime(0.22, ac.currentTime, 0.6); },
    musicStop() { musicWanted = false; if (musicS && ac) { musicS.g.gain.cancelScheduledValues(ac.currentTime); musicS.g.gain.setValueAtTime(0, ac.currentTime); } },
    audio() { if (!ac) init(); return ac; },
    musicOff() { musicWanted = false; if (musicS && ac) musicS.g.gain.setTargetAtTime(0, ac.currentTime, 0.4); },
    toggleMusic() { musicMuted = !musicMuted; if (musicMuted) { const w = musicWanted; SFX.musicOff(); musicWanted = w; } else if (musicWanted) SFX.musicOn(); return !musicMuted; },
    cheer(k = 1) { if (play('cheer', 1.4 * Math.min(1.3, k + 0.3))) return; bandNoise(2.2 * k + 0.6, 0.55 * k, 1100, 0.7, 0.35); bandNoise(1.6 * k, 0.3 * k, 2600, 1.2, 0.2); },
    ooh() { if (!ac) return; if (play('ooh', 1.6)) return; for (let i = 0; i < 6; i++) tone(180 + Math.random() * 120, 0.9, 'sine', 0.05, 0.75, i * 0.02); bandNoise(0.9, 0.18, 500, 1.5, 0.2); },
    applause(k = 1) { if (!ac) return; if (play('applause', 1.3 * Math.min(1.4, k))) return; for (let i = 0; i < 40 * k; i++) noiseClap(Math.random() * 2.2 * k); },
    // announcer: the device's built-in voice (free). Replaceable by recorded clips later.
    voiceKeys: t => voiceKeys(t).map(k => (k >= 0 ? VOICE.lines[k] : '?')),
    say(text, o = {}) {
      if (SFX.muteVoice) return;
      // 1) the recorded ElevenLabs announcer, when every sentence is a known call
      const keys = voiceKeys(text);
      if (voiceBuf && keys.length && keys.every(k => k >= 0)) return playClips(keys, o.interrupt !== false);
      // 2) otherwise the device's built-in voice (names etc.)
      try {
        if (!('speechSynthesis' in window)) return;
        const u = new SpeechSynthesisUtterance(text); u.rate = o.rate || 1.05; u.pitch = o.pitch || 0.9; u.volume = 1;
        const vs = speechSynthesis.getVoices(), v = vs.find(x => /en-GB|en_GB/.test(x.lang) && /male|daniel|george|arthur/i.test(x.name)) || vs.find(x => /^en/.test(x.lang));
        if (v) u.voice = v;
        if (o.interrupt !== false) speechSynthesis.cancel();
        speechSynthesis.speak(u);
      } catch (e) {}
    },
  };
  function voiceKeys(text) { // whole line first ("Red to serve. Play!"), else sentence by sentence
    if (typeof VOICE === 'undefined') return [-1];
    const whole = voiceKey(text); if (whole >= 0) return [whole];
    return String(text).split(/(?<=[.!?])\s+/).map(voiceKey).filter(k => k !== -2);
  }
  function voiceKey(sentence) {
    const t = String(sentence).toLowerCase().replace(/[^a-z ]/g, ' ').replace(/\s+/g, ' ').trim();
    if (!t) return -2;
    let i = VOICE.lines.indexOf(t); if (i >= 0) return i;
    if (/^game set and match/.test(t)) return VOICE.lines.indexOf('game set and match');
    if (/^game /.test(t)) return VOICE.lines.indexOf('game');
    return -1;
  }
  function playClips(keys, interrupt) {
    if (interrupt) { voiceSrc.forEach(x => { try { x.stop(); } catch (e) {} }); voiceSrc = []; }
    let t = ac.currentTime + 0.03;
    for (const k of keys) {
      const [a, b] = VOICE.clips[k], src = ac.createBufferSource(); src.buffer = voiceBuf; src.connect(voiceOut);
      src.start(t, a, b - a); voiceSrc.push(src); t += b - a + 0.12;
    }
  }
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
