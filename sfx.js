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
  // bowling alley effects and the STRIKE NIGHT intro (real recordings)
  for (const [k, f] of Object.entries({ b_release: 'bowl_release', b_strike: 'bowl_strike', b_strike2: 'bowl_strike2', b_pins: 'bowl_pins', b_bumper: 'bowl_bumper', b_return: 'bowl_return', b_pin: 'bowl_pin',
    i_drums: 'intro_drums', i_snare: 'intro_snare', i_riser: 'intro_riser', i_boom: 'intro_boom', i_brass: 'intro_brass', i_horn: 'intro_horn', i_crowd: 'intro_crowd' })) SAMPLE_FILES[k] = f;
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
      // everything goes through a limiter first, so a loud moment (a strike on top of the crowd and the announcer) never clips
      const lim = ac.createDynamicsCompressor(); lim.threshold.value = -4; lim.knee.value = 3; lim.ratio.value = 20; lim.attack.value = 0.002; lim.release.value = 0.15; lim.connect(ac.destination);
      master = ac.createGain(); master.gain.value = 0.35; master.connect(lim);
      voiceOut = ac.createGain(); voiceOut.gain.value = 1.0; voiceOut.connect(lim);
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
  // ===== bowling and the STRIKE NIGHT intro: real recorded sound effects (ElevenLabs), played from the part of the file we want =====
  let introSrc = [], introVoice = null, rollSrc = null, gutSrc = null, ambSrc = null, ambLoading = false; const MASTER = 0.35;
  const keep = s => { introSrc.push(s); return s; };
  // play part of a recording: off/dur pick the part (seconds into the file), delay schedules it, rate changes the pitch, short fades keep it click-free
  function samp(k, o = {}) {
    if (!ac || !S[k]) return null;
    const rate = o.rate || 1, t = ac.currentTime + (o.delay || 0), off = o.off || 0, dur = Math.max(0.05, Math.min(o.dur || 99, S[k].duration - off)), v = o.vol == null ? 1 : o.vol;
    const fi = o.fi == null ? 0.004 : o.fi, len = dur / rate, fo = Math.min(o.fo == null ? 0.12 : o.fo, len / 2);
    const src = ac.createBufferSource(), g = ac.createGain(); src.buffer = S[k]; src.playbackRate.value = rate;
    g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(v, t + fi); g.gain.setValueAtTime(v, t + len - fo); g.gain.linearRampToValueAtTime(0.0001, t + len);
    src.connect(g); g.connect(master); src.start(t, off, dur); src.stop(t + len + 0.05); if (o.keep) keep(src); return { src, g };
  }
  // a looped part of a recording that can be faded in and out (the ball rolling, the ball in the gutter, the alley's background noise)
  function loopStart(k, a, b, vol, fade) {
    if (!ac || !S[k]) return null; const t = ac.currentTime, src = ac.createBufferSource(), g = ac.createGain();
    src.buffer = S[k]; src.loop = true; src.loopStart = a; src.loopEnd = Math.min(b, S[k].duration); g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(vol, t + fade);
    src.connect(g); g.connect(master); src.start(t, a); return { src, g };
  }
  function loopStop(h, fade) { if (!h) return; const t = ac.currentTime; try { h.g.gain.cancelScheduledValues(t); h.g.gain.setValueAtTime(Math.max(0.0001, h.g.gain.value), t); h.g.gain.linearRampToValueAtTime(0.0001, t + fade); h.src.stop(t + fade + 0.05); } catch (e) {} }
  // The ball rolling is mostly a low rumble that wobbles at the rate the ball turns (~13 turns a second at 9 m/s). Synthesized from filtered noise, because the
  // generated recordings of it were almost silent between the first thud and the end. kind 'wood': deep rumble and a little grain; 'gutter': hollow and metallic.
  let rumbleBuf = null;
  function rumbleOn(kind, vol) {
    if (!ac) return null; const t = ac.currentTime;
    if (!rumbleBuf) { const n = ac.sampleRate * 3; rumbleBuf = ac.createBuffer(1, n, ac.sampleRate); const d = rumbleBuf.getChannelData(0); for (let i = 0; i < n; i++) d[i] = Math.random() * 2 - 1; }
    const src = ac.createBufferSource(); src.buffer = rumbleBuf; src.loop = true; src.loopStart = Math.random() * 1.5;
    const out = ac.createGain(); out.gain.setValueAtTime(0.0001, t); out.gain.linearRampToValueAtTime(vol, t + 0.25); out.connect(master);
    const wob = ac.createGain(); wob.gain.value = 0.78; wob.connect(out);   // the wobble: amplitude modulated at the turning rate
    const lfo = ac.createOscillator(), lg = ac.createGain(); lfo.frequency.value = kind === 'wood' ? 13 : 10; lg.gain.value = 0.22; lfo.connect(lg); lg.connect(wob.gain); lfo.start(t);
    const bands = kind === 'wood' ? [['lowpass', 170, 0.7, 2.6], ['bandpass', 520, 0.8, 0.55], ['bandpass', 1500, 1.2, 0.12]] : [['lowpass', 220, 0.7, 1.6], ['bandpass', 430, 5, 1.5], ['bandpass', 1100, 3, 0.5]];
    for (const [type, f, q, g] of bands) { const fl = ac.createBiquadFilter(); fl.type = type; fl.frequency.value = f; fl.Q.value = q; const gg = ac.createGain(); gg.gain.value = g; src.connect(fl); fl.connect(gg); gg.connect(wob); }
    src.start(t, Math.random() * 1.5); return { src, g: out, lfo };
  }
  function rumbleOff(h, fade) { if (!h) return; const t = ac.currentTime; try { h.g.gain.cancelScheduledValues(t); h.g.gain.setValueAtTime(Math.max(0.0001, h.g.gain.value), t); h.g.gain.linearRampToValueAtTime(0.0001, t + fade); h.src.stop(t + fade + 0.05); h.lfo.stop(t + fade + 0.05); } catch (e) {} }
  function stopIntro() { const t = ac.currentTime; for (const s of introSrc) { try { s.stop(t); } catch (e) {} } introSrc = []; if (introVoice) { try { introVoice.stop(t); } catch (e) {} introVoice = null; } }
  const ok = (k, ms) => { const n = performance.now(); if (last[k] && n - last[k] < ms) return false; last[k] = n; return true; };
  return {
    init,
    shoot() { ok('s', 50) && tone(880, 0.07, 'square', 0.06, 0.5); },
    hit() { ok('h', 40) && tone(220, 0.1, 'sawtooth', 0.15, 0.6); },
    fPunch() { ok('fp', 40) && (noise(0.12, 0.6, 2600), tone(135, 0.15, 'sine', 0.6, 0.4), tone(900, 0.04, 'square', 0.12, 0.3)); },
    fKick() { ok('fk', 40) && (noise(0.22, 0.75, 1800), tone(85, 0.24, 'sine', 0.85, 0.35), tone(230, 0.08, 'sawtooth', 0.16, 0.4)); },
    fBlock() { ok('fb', 40) && (noise(0.07, 0.35, 5000), tone(430, 0.07, 'triangle', 0.25, 0.6)); },
    fBell() { for (let i = 0; i < 3; i++) { tone(1320, 0.9, 'sine', 0.22, 0.99, i * 0.3); tone(1980, 0.7, 'sine', 0.1, 0.99, i * 0.3); } },
    fKO() { noise(0.5, 0.85, 900); tone(60, 0.8, 'sine', 0.95, 0.5); tone(120, 0.4, 'sine', 0.5, 0.4, 0.05); },
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
    // ---- bowling: every sound is a real recording ----
    bowlRelease() { samp('b_release', { vol: 1.5, off: 2.85, dur: 1.7, fo: 0.5 }); },                                      // the ball lands on the wood
    bowlRollOn() { if (!ac) return; SFX.bowlRollOff(true); rollSrc = rumbleOn('wood', 0.5); },                                // the rumble of the ball on the boards
    bowlRollOff(quick) { rumbleOff(rollSrc, quick ? 0.03 : 0.3); rollSrc = null; },
    bowlGutterOn() { if (!ac) return; SFX.bowlRollOff(true); rumbleOff(gutSrc, 0.05); gutSrc = rumbleOn('gutter', 0.55); samp('b_bumper', { vol: 1.0, off: 0.25, dur: 1.0, rate: 1.15, fo: 0.3 }); },   // it dropped into the gutter
    bowlGutterOff() { rumbleOff(gutSrc, 0.4); gutSrc = null; },
    bowlBumper() { if (ok('bb', 110)) samp('b_bumper', { vol: 1.3, off: 0.25, dur: 1.2, rate: 0.92 + Math.random() * 0.16, fo: 0.3 }); },   // a thump off the padded rail
    bowlCrash(vm = 1) { if (!S.b_strike) { play('strike', 2, 1, true); return; } const a = Math.random() < 0.5; samp(a ? 'b_strike' : 'b_strike2', { vol: (a ? 2.1 : 2.6) * vm, off: a ? 0.5 : 0.8, dur: 2.7, fo: 0.5 }); },   // the pins go down
    bowlPin() { if (!ok('bp', 85) || !S.b_pin) return; if (Math.random() < 0.3 && S.b_pins) samp('b_pins', { vol: 1.1, off: 0.0, dur: 1.6, rate: 0.9 + Math.random() * 0.2, fo: 0.4 }); else samp('b_pin', { vol: 1.2, off: 0.05, dur: 0.9, rate: 0.88 + Math.random() * 0.3, fo: 0.3 }); },   // a pin knocking another
    bowlReturn() { samp('b_return', { vol: 0.9, off: 2.7, dur: 2.6, delay: 1.2, fo: 0.9 }); },                              // the ball comes back up the return
    bowlSweep() { samp('b_pins', { vol: 1.3, off: 0.0, dur: 2.2, delay: 0.5, rate: 0.85, fo: 0.7 }); },                                   // the sweep clears the deck into the pit
    bowlRack() { samp('b_pin', { vol: 1.1, off: 0.05, dur: 0.8, rate: 0.8, fo: 0.3 }); samp('b_pins', { vol: 0.5, off: 0.0, dur: 0.9, rate: 1.25, fo: 0.4 }); },   // a new rack of pins is set down
    bowlAmbienceOff() { SFX.bowlAmbience(false); },
    bowlAmbience(on = true) {   // the sound of the alley behind everything: far pins, voices, a bit of music
      if (!ac) return; if (!on) { loopStop(ambSrc, 1.0); ambSrc = null; return; }
      if (ambSrc) return;
      if (S.b_amb) { ambSrc = loopStart('b_amb', 1.0, 29.0, 0.75, 1.5); return; }
      if (ambLoading) return; ambLoading = true;
      fetch('audio/bowl_ambience.mp3').then(r => (r.ok ? r.arrayBuffer() : Promise.reject())).then(b => ac.decodeAudioData(b)).then(buf => { S.b_amb = buf; ambLoading = false; if (!ambSrc) SFX.bowlAmbience(true); }).catch(() => { ambLoading = false; });
    },
    // ---- the STRIKE NIGHT intro: war drums, a snare roll, a riser, brass, an air horn and a roaring crowd, all real recordings ----
    introRise() {   // the announcer and the build-up, aimed at the moment of impact (4.9 s)
      if (!ac) return; stopIntro(); const t0 = ac.currentTime + 0.06; master.gain.cancelScheduledValues(t0); master.gain.setValueAtTime(MASTER, t0);
      if (S.bowl_intro && !SFX.muteVoice) { const s = ac.createBufferSource(); s.buffer = S.bowl_intro; s.connect(voiceOut); s.start(t0 + 0.1); introVoice = s; }
      samp('i_brass', { delay: 0.06, vol: 1.5, dur: 1.6, fo: 0.5, keep: true });                                       // the opening hit
      samp('i_drums', { delay: 0.5, vol: 1.1, off: 0.0, dur: 4.4, fo: 0.3, keep: true });                              // war drums under the announcer
      samp('i_brass', { delay: 2.9, vol: 1.7, dur: 1.6, fo: 0.5, keep: true });                                        // "STRIKE NIGHT!"
      samp('i_snare', { delay: 0.0, off: 0.1, dur: 6.0, vol: 1.7, fo: 0.25, keep: true });                             // a snare roll that peaks about 5 s in
      samp('i_riser', { delay: 3.4, off: 0.6, dur: 2.6, vol: 1.5, fo: 0.05, keep: true });                             // a riser that peaks as the ball hits
    },
    introWhoosh() { X('whoosh', 0.9, 0.9, 0.12); },
    introThrow() { SFX.bowlRelease(); X('whoosh', 1.0, 0.9, 0.05); },
    introImpact() {   // the ball hits: crash, boom, the crowd gasps, a pin screams toward the lens
      if (!ac) return; samp('i_boom', { vol: 1.0, dur: 3.0, fo: 0.8 }); SFX.bowlCrash(0.5); play('ooh', 0.5); X('whoosh', 1.3, 0.9, 0);
    },
    introBlack() {   // the pin hits the lens: everything cuts to silence
      if (!ac) return; const t = ac.currentTime; for (const s of introSrc) { try { s.stop(t); } catch (e) {} } introSrc = []; SFX.bowlRollOff(true);
      master.gain.cancelScheduledValues(t); master.gain.setTargetAtTime(0.0, t, 0.01); master.gain.setTargetAtTime(MASTER, t + 1.4, 0.05);
    },
    introSwell() {   // lights up: boom, brass, an air horn and the whole stadium on its feet
      if (!ac) return; const t = ac.currentTime; master.gain.cancelScheduledValues(t); master.gain.setValueAtTime(MASTER, t);
      samp('i_boom', { vol: 1.1, dur: 3.0, fo: 0.8 }); samp('i_brass', { vol: 1.2, dur: 1.8, fo: 0.5 }); samp('i_horn', { vol: 1.1, dur: 2.2, fo: 0.4 });
      samp('i_crowd', { vol: 1.5, off: 0.2, dur: 7.0, fo: 1.8 }); play('applause', 0.6);
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
    pinsFar() { if (!ac || !ok('pf', 350)) return; if (!samp('b_strike', { vol: 0.28, off: 0.5, dur: 1.8, rate: 0.8 + Math.random() * 0.4, fo: 0.5 })) play('strike', 0.22, 0.85 + Math.random() * 0.35, true); },   // other people's pins crashing, far away
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
