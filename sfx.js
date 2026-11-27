/* Sound: recorded ElevenLabs samples (crowd, cheers, racket, strike, music…) with synthesized fallbacks,
   so everything still works before the files load or offline. */
const SFX = (() => {
  let ac = null, master = null, crowd = null, voiceOut = null, voiceBuf = null, voiceSrc = [];
  let hostBuf = null, hostClips = [], hostSrc = null, hostLast = 0;
  // split the host recording into its lines at the pauses between them
  function findClips(buf) {
    const d = buf.getChannelData(0), sr = buf.sampleRate, win = Math.floor(sr * 0.02), segs = [];
    let on = -1, quiet = 0;
    for (let i = 0; i + win < d.length; i += win) {
      let e = 0; for (let j = i; j < i + win; j += 4) e += d[j] * d[j];
      const loud = Math.sqrt(e / (win / 4)) > 0.018, t = i / sr;
      if (loud) { if (on < 0) on = t; quiet = 0; }
      else if (on >= 0 && (quiet += 0.02) > 0.5) { segs.push([Math.max(0, on - 0.04), t - quiet + 0.1]); on = -1; quiet = 0; }
    }
    if (on >= 0) segs.push([on, d.length / sr]);
    return segs.filter(s => s[1] - s[0] > 0.18);
  }
  // recorded samples (ElevenLabs sound effects + music)
  const SAMPLE_FILES = { crowd: 'crowd', cheer: 'cheer', ooh: 'ooh', applause: 'applause', hit: 'hit', bounce: 'bounce', smash: 'smash', strike: 'strike', bat: 'bat', music: 'music', bowl_intro: 'bowl_intro' };
  // studio sound effects (ElevenLabs); each synth sound below falls back to its old beep if the file is missing
  for (const k of ['ui', 'ding', 'buzz', 'win', 'card', 'dice', 'pickup', 'whoosh', 'bonk', 'splash', 'boom', 'go', 'tick', 'join', 'snow']) SAMPLE_FILES['x_' + k] = 'sfx_' + k;
  // background music: one track per kind of game, loaded only when first needed
  const TRACKS = { menu: 'music', party: 'music_party', lounge: 'music_lounge', winter: 'music_winter' };
  const TRACK_VOL = { menu: 0.22, party: 0.13, lounge: 0.16, winter: 0.13 };
  const S = {}, onset = {}; let crowdS = null, musicS = null, musicWanted = false, musicMuted = false, track = 'menu';
  function loadTrack(name) {
    const k = 'm_' + name; if (S[k] || S[k + '_loading'] || !ac) return;
    S[k + '_loading'] = true;
    fetch(`audio/${TRACKS[name]}.mp3`).then(r => (r.ok ? r.arrayBuffer() : Promise.reject())).then(b => ac.decodeAudioData(b)).then(buf => { S[k] = buf; if (musicWanted && track === name) SFX.musicOn(); }).catch(() => {});
  }
  function loadSamples() {
    for (const [k, f] of Object.entries(SAMPLE_FILES)) {
      if (k === 'music') continue;
      fetch(`audio/${f}.mp3`).then(r => (r.ok ? r.arrayBuffer() : Promise.reject())).then(b => ac.decodeAudioData(b)).then(buf => {
        S[k] = buf;
        // where the sound actually starts (first point above 40% of the peak) — used to sync hits
        const d = buf.getChannelData(0); let pk = 0; for (let i = 0; i < d.length; i += 8) pk = Math.max(pk, Math.abs(d[i]));
        let i0 = 0; while (i0 < d.length && Math.abs(d[i0]) < pk * 0.4) i0 += 8; onset[k] = Math.max(0, i0 / buf.sampleRate - 0.03);
        if (k === 'crowd' && crowd && crowd.want) startCrowdLoop(crowd.level);
      }).catch(() => {});
    }
  }
  function play(k, vol = 1, rate = 1, fromOnset = false, maxDur = 0) {
    if (!ac || !S[k]) return false;
    const src = ac.createBufferSource(), g = ac.createGain(), t = ac.currentTime; src.buffer = S[k]; src.playbackRate.value = rate; g.gain.value = vol;
    src.connect(g); g.connect(master); src.start(t, fromOnset ? onset[k] || 0 : 0);
    if (maxDur > 0) { g.gain.setValueAtTime(vol, t + maxDur * 0.7); g.gain.linearRampToValueAtTime(0.0001, t + maxDur); src.stop(t + maxDur + 0.05); }
    return true;
  }
  // a recorded effect, trimmed, with a little pitch variety so repeats do not sound robotic
  const X = (k, vol = 1, maxDur = 0, vary = 0.06) => play('x_' + k, vol * 2.2, 1 + (Math.random() * 2 - 1) * vary, true, maxDur);
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
      if (typeof HOST !== 'undefined') fetch(HOST.url).then(r => r.arrayBuffer()).then(b => ac.decodeAudioData(b)).then(buf => { hostBuf = buf; hostClips = findClips(buf); }).catch(() => {});
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
  // ===== STRIKE NIGHT, the bowling intro: an NFL-style kickoff, built from war drums, a riser, brass stabs, cymbals, an air horn and a crowd going wild =====
  let introSrc = [], introVoice = null, bus = null; const MASTER = 0.35;
  const keep = s => { introSrc.push(s); return s; };
  const whiteBuf = sec => { const n = Math.floor(ac.sampleRate * sec), b = ac.createBuffer(1, n, ac.sampleRate), d = b.getChannelData(0); for (let i = 0; i < n; i++) d[i] = Math.random() * 2 - 1; return b; };
  // filtered noise that sweeps f0 -> f1; swells in over atk seconds, then dies away
  function nz(t, dur, vol, type, f0, f1, q = 1, atk = 0.01) {
    const src = ac.createBufferSource(); src.buffer = whiteBuf(dur + 0.05);
    const f = ac.createBiquadFilter(); f.type = type; f.Q.value = q; f.frequency.setValueAtTime(f0, t); f.frequency.exponentialRampToValueAtTime(Math.max(20, f1), t + dur);
    const g = ac.createGain(); g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(vol, t + atk); g.gain.exponentialRampToValueAtTime(0.0008, t + dur);
    src.connect(f); f.connect(g); g.connect(bus || master); src.start(t); src.stop(t + dur + 0.05); keep(src);
  }
  function osc(type, f0, f1, t, dur, vol, atk = 0.01) {
    const o = ac.createOscillator(), g = ac.createGain(); o.type = type; o.frequency.setValueAtTime(f0, t); if (f1 && f1 !== f0) o.frequency.exponentialRampToValueAtTime(f1, t + dur);
    g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(vol, t + atk); g.gain.exponentialRampToValueAtTime(0.0008, t + dur);
    o.connect(g); g.connect(bus || master); o.start(t); o.stop(t + dur + 0.05); keep(o);
  }
  const kick = (t, v = 1) => { osc('sine', 165, 42, t, 0.42, 0.95 * v, 0.004); nz(t, 0.035, 0.32 * v, 'highpass', 3000, 3000, 0.7, 0.002); };
  const snare = (t, v = 1) => { nz(t, 0.18, 0.5 * v, 'bandpass', 2200, 1700, 0.6, 0.002); osc('triangle', 230, 140, t, 0.1, 0.35 * v, 0.002); };
  const cymbal = (t, dur, v = 1) => { nz(t, dur, 0.4 * v, 'highpass', 7000, 6000, 0.5, 0.004); nz(t, dur * 0.6, 0.25 * v, 'bandpass', 9500, 8000, 1.2, 0.004); };
  // horns: detuned saw waves through a filter that snaps open and falls back
  function brass(t, notes, dur, v = 1) {
    const f = ac.createBiquadFilter(); f.type = 'lowpass'; f.Q.value = 2.5; f.frequency.setValueAtTime(350, t); f.frequency.exponentialRampToValueAtTime(3400, t + 0.07); f.frequency.exponentialRampToValueAtTime(700, t + dur);
    const g = ac.createGain(); g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(0.05 * v, t + 0.03); g.gain.setValueAtTime(0.05 * v, t + dur * 0.35); g.gain.exponentialRampToValueAtTime(0.001, t + dur);
    f.connect(g); g.connect(bus || master);
    for (const n of notes) for (const c of [-9, 0, 9]) { const o = ac.createOscillator(); o.type = 'sawtooth'; o.frequency.value = n; o.detune.value = c; o.connect(f); o.start(t); o.stop(t + dur + 0.05); keep(o); }
  }
  function airHorn(t, dur, v = 1) {   // the stadium air horn: a loud, slightly sour chord, a little sag at the end
    const f = ac.createBiquadFilter(); f.type = 'lowpass'; f.frequency.value = 2600; f.Q.value = 1.2;
    const g = ac.createGain(); g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(0.07 * v, t + 0.02); g.gain.setValueAtTime(0.07 * v, t + dur * 0.8); g.gain.exponentialRampToValueAtTime(0.001, t + dur);
    f.connect(g); g.connect(bus || master);
    for (const n of [392, 415, 494, 587]) { const o = ac.createOscillator(); o.type = 'square'; o.frequency.setValueAtTime(n, t); o.frequency.setValueAtTime(n, t + dur * 0.8); o.frequency.exponentialRampToValueAtTime(n * 0.93, t + dur); o.connect(f); o.start(t); o.stop(t + dur + 0.05); keep(o); }
  }
  // a riser: noise climbing through the spectrum and a saw sweeping up, both getting louder until the cut
  function riser(t, dur, v = 1) {
    const src = ac.createBufferSource(); src.buffer = whiteBuf(dur + 0.05); const f = ac.createBiquadFilter(); f.type = 'bandpass'; f.Q.value = 1.4; f.frequency.setValueAtTime(260, t); f.frequency.exponentialRampToValueAtTime(7500, t + dur);
    const g = ac.createGain(); g.gain.setValueAtTime(0.004, t); g.gain.exponentialRampToValueAtTime(0.42 * v, t + dur); g.gain.linearRampToValueAtTime(0.0001, t + dur + 0.04);
    src.connect(f); f.connect(g); g.connect(bus || master); src.start(t); src.stop(t + dur + 0.06); keep(src);
    const o = ac.createOscillator(), lp = ac.createBiquadFilter(), og = ac.createGain(); o.type = 'sawtooth'; o.frequency.setValueAtTime(70, t); o.frequency.exponentialRampToValueAtTime(950, t + dur);
    lp.type = 'lowpass'; lp.frequency.setValueAtTime(300, t); lp.frequency.exponentialRampToValueAtTime(5200, t + dur); og.gain.setValueAtTime(0.004, t); og.gain.exponentialRampToValueAtTime(0.1 * v, t + dur); og.gain.linearRampToValueAtTime(0.0001, t + dur + 0.04);
    o.connect(lp); lp.connect(og); og.connect(bus || master); o.start(t); o.stop(t + dur + 0.06); keep(o);
  }
  function stopIntro() { const t = ac.currentTime; for (const s of introSrc) { try { s.stop(t); } catch (e) {} } introSrc = []; if (introVoice) { try { introVoice.stop(t); } catch (e) {} introVoice = null; } }
  const ok = (k, ms) => { const n = performance.now(); if (last[k] && n - last[k] < ms) return false; last[k] = n; return true; };
  return {
    init,
    shoot() { ok('s', 50) && tone(880, 0.07, 'square', 0.06, 0.5); },
    hit() { ok('h', 40) && tone(220, 0.1, 'sawtooth', 0.15, 0.6); },
    boom() { ok('bm', 60) && (X('boom', 1, 1.8, 0.08) || noise(0.8, 0.7, 1400)); },
    tink() { ok('t', 50) && (X('ui', 0.7, 0.4, 0.15) || tone(1800, 0.08, 'triangle', 0.15)); },
    pickup() { if (X('pickup', 0.8, 1.2)) return; tone(660, 0.08, 'square', 0.15); tone(990, 0.14, 'square', 0.15, 1, 0.08); },
    dash() { ok('d', 60) && tone(300, 0.15, 'sawtooth', 0.12, 2.5); },
    bonk() { ok('b', 70) && (X('bonk', 0.9, 1.0) || tone(160, 0.14, 'sine', 0.5, 0.5)); },
    splash() { if (X('splash', 1, 1.6)) return; noise(0.6, 0.5, 700); tone(500, 0.5, 'sine', 0.2, 0.2); },
    card() { ok('c', 40) && (X('card', 0.9, 0.6, 0.1) || noise(0.09, 0.3, 3500)); },
    blip() { X('ui', 0.6, 0.4, 0.04) || tone(520, 0.06, 'square', 0.12); },
    tick() { X('tick', 0.9, 0.35, 0) || tone(700, 0.06, 'square', 0.12); },
    go() { if (X('go', 1, 1.5, 0)) return; tone(523, 0.1, 'square', 0.2); tone(784, 0.3, 'square', 0.2, 1, 0.1); },
    ding() { ok('dg', 60) && (X('ding', 0.8, 1.4, 0.02) || tone(1046, 0.18, 'triangle', 0.3)); },
    buzz() { ok('bz', 120) && (X('buzz', 0.8, 1.1, 0) || tone(110, 0.35, 'sawtooth', 0.25)); },
    whistle() { X('go', 1, 1.5, 0) || tone(1800, 0.4, 'sine', 0.2); },
    alarm() { for (let i = 0; i < 3; i++) tone(880, 0.12, 'square', 0.14, 0.7, i * 0.18); },
    win() { if (X('win', 1, 3, 0)) return; [523, 659, 784, 1046].forEach((f, i) => tone(f, 0.2, 'triangle', 0.25, 1, i * 0.12)); },
    zap() { noise(0.3, 0.4, 5000); tone(1500, 0.3, 'sawtooth', 0.15, 0.1); },
    join() { if (X('join', 0.9, 1.2, 0.03)) return; tone(660, 0.08, 'triangle', 0.2); tone(880, 0.12, 'triangle', 0.2, 1, 0.08); },
    whoosh() { ok('wh', 80) && (X('whoosh', 0.7, 0.8, 0.08) || noise(0.5, 0.35, 2500)); },
    dice() { ok('di', 150) && (X('dice', 0.9, 1.6, 0.05) || noise(0.3, 0.3, 2000)); },
    snow() { ok('sn', 50) && (X('snow', 0.9, 0.8, 0.12) || noise(0.2, 0.4, 1200)); },
    // ---- the bowling intro (STRIKE NIGHT) ----
    introRise() {   // the announcer and the build-up: opening hit, war drums, the title hit, a snare roll and a riser, all aimed at the moment of impact (~4.9 s)
      if (!ac) return; stopIntro(); const t = ac.currentTime + 0.06; master.gain.cancelScheduledValues(t); master.gain.setValueAtTime(MASTER, t);
      if (S.bowl_intro && !SFX.muteVoice) { const s = ac.createBufferSource(); s.buffer = S.bowl_intro; s.connect(voiceOut); s.start(t + 0.1); introVoice = s; }
      bus = ac.createGain(); bus.gain.value = 0.5; bus.connect(master);   // the build-up sits under the announcer
      brass(t, [110, 165, 220, 277], 1.2, 1); cymbal(t, 1.6, 0.8); kick(t, 1.3);
      for (let i = 0, u = 0.55; u < 2.75; i++, u += 0.4) { kick(t + u, 0.85); if (i % 2) snare(t + u, 0.6); }
      brass(t + 2.9, [147, 220, 294, 370], 1.0, 1.1); cymbal(t + 2.9, 1.2, 0.7); kick(t + 2.9, 1.3); kick(t + 3.02, 0.9);
      for (let u = 3.25, g = 0.12; u < 4.86; u += g, g = Math.max(0.028, g * 0.92)) snare(t + u, 0.22 + 0.55 * (u - 3.25) / 1.6);
      riser(t + 0.3, 4.55, 1); bus = null;
    },
    introWhoosh() { if (!ac) return; const t = ac.currentTime; nz(t, 0.55, 0.55, 'bandpass', 500, 4200, 1.2, 0.2); X('whoosh', 0.8, 0.9, 0.12); },
    introThrow() { if (!ac) return; const t = ac.currentTime; nz(t, 0.7, 0.5, 'bandpass', 400, 5000, 1.0, 0.3); osc('sawtooth', 180, 760, t, 0.6, 0.12, 0.05); kick(t, 0.9); },
    introImpact() {   // the ball hits: crash, boom, a cymbal, the crowd gasps, and a pin screams toward the lens
      if (!ac) return; const t = ac.currentTime; play('strike', 1.6, 1, true); X('boom', 1.0, 1.8, 0.05); osc('sine', 80, 28, t, 1.1, 0.8, 0.004); cymbal(t, 1.8, 0.6); play('ooh', 0.9);
      nz(t + 0.05, 0.62, 0.7, 'bandpass', 300, 6500, 1.8, 0.5); osc('sawtooth', 220, 1700, t + 0.05, 0.62, 0.18, 0.4);
    },
    introBlack() {   // the pin hits the lens: everything cuts to silence
      if (!ac) return; const t = ac.currentTime; for (const s of introSrc) { try { s.stop(t); } catch (e) {} } introSrc = [];
      master.gain.cancelScheduledValues(t); master.gain.setTargetAtTime(0.0, t, 0.01); master.gain.setTargetAtTime(MASTER, t + 1.4, 0.05);
    },
    introSwell() {   // lights up: horns, an air horn, a huge boom and the whole stadium on its feet
      if (!ac) return; const t = ac.currentTime; master.gain.cancelScheduledValues(t); master.gain.setValueAtTime(MASTER, t);
      brass(t, [110, 165, 220, 277, 330], 1.8, 1.0); airHorn(t, 1.3, 0.8); kick(t, 1.1); osc('sine', 70, 30, t, 1.3, 0.8, 0.004); cymbal(t, 2.4, 0.7);
      kick(t + 0.5, 0.7); kick(t + 0.75, 0.8); snare(t + 0.75, 0.6); nz(t, 2.3, 0.35, 'bandpass', 900, 1500, 0.8, 0.3);
      play('cheer', 1.5); play('applause', 1.0);
    },
    introStop() { if (!ac) return; stopIntro(); master.gain.cancelScheduledValues(ac.currentTime); master.gain.setTargetAtTime(MASTER, ac.currentTime, 0.05); },
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
    musicOn() {
      musicWanted = true; if (!ac || musicMuted) return;
      const buf = S['m_' + track]; if (!buf) { loadTrack(track); return; }
      if (musicS && musicS.name !== track) { const old = musicS; old.g.gain.setTargetAtTime(0, ac.currentTime, 0.35); setTimeout(() => { try { old.src.stop(); } catch (e) {} }, 2000); musicS = null; }
      if (!musicS) { const src = ac.createBufferSource(), g = ac.createGain(); src.buffer = buf; src.loop = true; g.gain.value = 0; src.connect(g); g.connect(ac.destination); src.start(); musicS = { src, g, name: track }; }
      musicS.g.gain.cancelScheduledValues(ac.currentTime); musicS.g.gain.setTargetAtTime(TRACK_VOL[track] || 0.18, ac.currentTime, 0.6);
    },
    // pick the soundtrack: 'menu' | 'party' | 'lounge' | 'winter' (preloads it)
    setTrack(name) { if (!TRACKS[name]) return; track = name; loadTrack(name); },
    musicStop() { musicWanted = false; if (musicS && ac) { musicS.g.gain.cancelScheduledValues(ac.currentTime); musicS.g.gain.setValueAtTime(0, ac.currentTime); } },
    audio() { if (!ac) init(); return ac; },
    musicOff() { musicWanted = false; if (musicS && ac) musicS.g.gain.setTargetAtTime(0, ac.currentTime, 0.4); },
    musicMuted() { return musicMuted; },
    toggleMusic() { musicMuted = !musicMuted; if (musicMuted) { const w = musicWanted; SFX.musicOff(); musicWanted = w; } else if (musicWanted) SFX.musicOn(); return !musicMuted; },
    cheer(k = 1) { if (play('cheer', 1.4 * Math.min(1.3, k + 0.3))) return; bandNoise(2.2 * k + 0.6, 0.55 * k, 1100, 0.7, 0.35); bandNoise(1.6 * k, 0.3 * k, 2600, 1.2, 0.2); },
    pinsFar() { if (!ac || !ok('pf', 350)) return; if (!play('strike', 0.22, 0.85 + Math.random() * 0.35, true)) bandNoise(0.5, 0.12, 1800, 1.2, 0.01); },   // other people's pins crashing, far away
    // shoe squeak on a hard court, Hawk-Eye stings, and the cinematic slow-motion / impact hits
    squeak() { if (!ac || !ok('sq', 160)) return; const f = 2300 + Math.random() * 900; tone(f, 0.06, 'sine', 0.05, 1.25); tone(f * 1.5, 0.05, 'triangle', 0.025, 0.8, 0.04); },
    varSting() { if (!ac) return; [988, 1319, 1760].forEach((f, i) => tone(f, 0.14, 'sine', 0.16, 1, i * 0.09)); bandNoise(0.5, 0.16, 3000, 2, 0.05); },
    varIn() { if (!ac) return; [784, 988, 1175, 1568].forEach((f, i) => tone(f, 0.5, 'triangle', 0.15, 1, i * 0.05)); },
    varOut() { if (!ac) return; tone(196, 0.35, 'sawtooth', 0.13, 0.9); tone(147, 0.5, 'sawtooth', 0.13, 0.9, 0.13); },
    slowmo() { if (!ac || !ok('slo', 600)) return; tone(240, 1.1, 'sine', 0.3, 0.25); bandNoise(1.0, 0.22, 500, 1.2, 0.05); },
    cinehit() { if (!ac || !ok('ch', 150)) return; tone(70, 0.7, 'sine', 0.55, 0.5); noise(0.3, 0.45, 700); },
    ooh() { if (!ac) return; if (play('ooh', 1.6)) return; for (let i = 0; i < 6; i++) tone(180 + Math.random() * 120, 0.9, 'sine', 0.05, 0.75, i * 0.02); bandNoise(0.9, 0.18, 500, 1.5, 0.2); },
    applause(k = 1) { if (!ac) return; if (play('applause', 1.3 * Math.min(1.4, k))) return; for (let i = 0; i < 40 * k; i++) noiseClap(Math.random() * 2.2 * k); },
    // announcer: the device's built-in voice (free). Replaceable by recorded clips later.
    voiceKeys: t => voiceKeys(t).map(k => (k >= 0 ? VOICE.lines[k] : '?')),
    // the show host: plays one recorded line (by key), never talking over itself; gap = minimum seconds since the last line
    host(key, o = {}) {
      if (SFX.muteVoice || !ac || !hostBuf || typeof HOST === 'undefined') return false;
      const i = HOST.lines.indexOf(key), c = hostClips[i]; if (i < 0 || !c) return false;
      const now = ac.currentTime; if (!o.force && now - hostLast < (o.gap || 3.2)) return false;
      if (hostSrc) { try { hostSrc.stop(); } catch (e) {} }
      const src = ac.createBufferSource(); src.buffer = hostBuf; src.connect(voiceOut); src.start(now + (o.delay || 0) + 0.02, c[0], c[1] - c[0]);
      hostSrc = src; hostLast = now + (o.delay || 0); return true;
    },
    hostClipCount() { return hostClips.length; },
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
