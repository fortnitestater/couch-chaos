/* Couch Chaos — 💣 Bombs & party games + 🧠 brain games. Needs engine.js */

/* ---------------------------------------------------------------- 💣 BOMB SQUAD — read the manual, cut the right wire */
const WIRE = { RED: '#e03131', BLUE: '#1c7ed6', YELLOW: '#fcc419', GREEN: '#2f9e44', WHITE: '#f1f3f5', BLACK: '#343a40' };
const ORD = ['first', 'second', 'third', 'fourth', 'fifth', 'sixth'];
class BombSquad extends QuizGame {
  constructor(c) { super(c, { rounds: 8, time: 14, title: 'BOMB SQUAD', bg: ['#3d1f1f', '#0d0606'], botSkill: 0.6 }); }
  makeRound(n) {
    const count = Math.min(6, 3 + Math.floor(n / 2)), names = Object.keys(WIRE);
    let wires, rule, ans;
    for (let tries = 0; tries < 200; tries++) {
      wires = Array.from({ length: count }, () => pick(names));
      const kind = n <= 2 ? irnd(0, 1) : irnd(0, 5);
      const has = c => wires.includes(c), cnt = c => wires.filter(w => w === c).length;
      if (kind === 0) { const k = irnd(0, count - 1); rule = `Cut the ${ORD[k]} wire.`; ans = k; }
      else if (kind === 1) { const c = pick(wires); rule = `Cut the LAST ${c} wire.`; ans = wires.lastIndexOf(c); }
      else if (kind === 2) { rule = 'If there is more than one RED wire, cut the last RED wire. Otherwise, cut the second wire.'; ans = cnt('RED') > 1 ? wires.lastIndexOf('RED') : 1; }
      else if (kind === 3) { const uniq = wires.filter(w => cnt(w) === 1); if (uniq.length !== 1) continue; rule = 'Cut the only wire whose color appears just ONCE.'; ans = wires.indexOf(uniq[0]); }
      else if (kind === 4) { const i = wires.indexOf('BLUE'); if (i < 0 || i === count - 1) continue; rule = 'Cut the wire right AFTER the first BLUE wire.'; ans = i + 1; }
      else { rule = has('YELLOW') ? 'If there is a YELLOW wire, cut the first wire. Otherwise, cut the last wire.' : 'If there is a YELLOW wire, cut the first wire. Otherwise, cut the last wire.'; ans = has('YELLOW') ? 0 : count - 1; }
      break;
    }
    const self = this;
    return {
      text: rule, phone: '📖 Read the rule on the TV and CUT the right wire!', opts: wires.map((w, i) => `${i + 1} ${w}`), colors: wires.map(w => WIRE[w]), cols: count > 4 ? 3 : 2, correct: ans,
      revealText: `Correct wire: #${ans + 1} ${wires[ans]}`, hideOpts: true,
      resolve(answers, gm) {
        const m = gm.defaultResolve();
        const wrong = [...answers.entries()].filter(([, a]) => a.i !== ans);
        if (wrong.length) { gm.fx.burst(800, 420, 70, '#ff922b', 520, 1.2, 12); gm.fx.burst(800, 420, 40, '#ffd43b', 380, 1, 9); gm.c.sfx.boom(); gm.c.shake(18); for (const [p] of wrong) gm.c.send(p, { t: 'buzz', ms: 600 }); }
        else { gm.fx.burst(800, 420, 40, '#51cf66', 300, 0.8, 7); }
        return m;
      },
      draw(g, gm) {
        para(g, rule, 800, 220, 1300, 46, '#ffe066');
        // bomb body
        g.fillStyle = 'rgba(0,0,0,.4)'; rr(g, 310, 330, 1000, 470, 30); g.fill();
        g.fillStyle = '#495057'; rr(g, 300, 320, 1000, 470, 30); g.fill(); g.lineWidth = 6; g.strokeStyle = '#212529'; g.stroke();
        g.fillStyle = '#111'; rr(g, 650, 345, 300, 80, 12); g.fill();
        const secs = gm.phase === 'ask' ? Math.max(0, gm.t) : 0;
        txt(g, `00:${String(Math.ceil(secs)).padStart(2, '0')}`, 800, 387, 50, secs < 4 && ((secs * 4) | 0) % 2 ? '#ff6b6b' : '#ff2222');
        wires.forEach((w, i) => {
          const y = 470 + i * (300 / count), cut = gm.phase === 'reveal' && i === ans;
          g.lineCap = 'round'; g.lineWidth = 18; g.strokeStyle = '#212529';
          g.beginPath(); g.moveTo(380, y); g.bezierCurveTo(600, y + 40, 1000, y - 40, 1220, y); g.stroke();
          g.lineWidth = 12; g.strokeStyle = WIRE[w];
          if (cut) { g.beginPath(); g.moveTo(380, y); g.bezierCurveTo(500, y + 30, 700, y + 20, 770, y + 30); g.stroke(); g.beginPath(); g.moveTo(830, y + 30); g.bezierCurveTo(900, y + 20, 1100, y - 30, 1220, y); g.stroke(); }
          else { g.beginPath(); g.moveTo(380, y); g.bezierCurveTo(600, y + 40, 1000, y - 40, 1220, y); g.stroke(); }
          txt(g, String(i + 1), 345, y, 30, '#fff');
          g.fillStyle = '#adb5bd'; g.beginPath(); g.arc(1240, y, 12, 0, TAU); g.fill();
        });
        if (gm.phase === 'reveal') {
          let k = 0; for (const [p, a] of gm.answers) { const y = 470 + a.i * (300 / count); g.fillStyle = p.color; g.beginPath(); g.arc(1290 + (k % 3) * 0, y - 14 + (k % 3) * 12, 10, 0, TAU); g.fill(); k++; }
        }
      },
    };
  }
}

/* ---------------------------------------------------------------- 🥔 HOT POTATO BOMB (arena) */
class HotPotato extends ArenaGame {
  constructor(c) { super(c, { time: 999, clock: false, b: 'DASH', hint: 'Touch someone to pass the bomb before it blows!', fric: 2.8 }); this.wait = 0; }
  setup() { this.newBomb(); }
  newBomb() {
    const al = this.blobs.filter(b => b.alive); if (al.length < 2) return;
    this.holder = pick(al); this.fuse = rnd(8, 15); this.passCd = 0.8; this.tickT = 0;
    this.c.banner(`💣 ${this.holder.p.name} has it!`, 1300, '#ff922b');
  }
  tick(dt) {
    if (this.wait > 0) { this.wait -= dt; if (this.wait <= 0) this.newBomb(); return; }
    if (!this.holder) return;
    for (const b of this.blobs) b.speedMul = b === this.holder ? 1.18 : 1;
    this.passCd -= dt; this.fuse -= dt; this.tickT -= dt;
    if (this.tickT <= 0) { this.tickT = this.fuse < 3 ? 0.2 : this.fuse < 6 ? 0.4 : 0.7; this.c.sfx.tick(); }
    if (this.fuse <= 0) {
      const h = this.holder; this.holder = null;
      this.fx.burst(h.x, h.y, 60, '#ff922b', 520, 1.2, 12); this.fx.burst(h.x, h.y, 30, '#ffd43b', 380, 0.9, 9);
      this.eliminate(h, 'BOOM!'); this.c.shake(20);
      for (const b of this.blobs) if (b.alive) { const dx = b.x - h.x, dy = b.y - h.y, d = Math.hypot(dx, dy) || 1; if (d < 300) { b.vx += dx / d * 900; b.vy += dy / d * 900; } }
      this.wait = 1.8;
    }
  }
  onBump(a, b) {
    if (this.passCd > 0 || !this.holder) return;
    const to = a === this.holder ? b : b === this.holder ? a : null;
    if (!to) return;
    this.holder = to; this.passCd = 0.7; to.stun = 0.3; this.c.sfx.blip(); this.fx.text(to.x, to.y - 50, 'TAG!', '#ff922b');
    this.c.send(to.p, { t: 'buzz', ms: 200 });
  }
  isOver() { return this.blobs.filter(b => b.alive).length <= 1; }
  rank() { return survivorRank(this.ps, p => this.blobOf(p).alive, this.out); }
  botTarget(b) {
    if (!this.holder) return null;
    if (b === this.holder) { let t = null, bd = 1e9; for (const o of this.blobs) if (o !== b && o.alive) { const d = Math.hypot(o.x - b.x, o.y - b.y); if (d < bd) { bd = d; t = o; } } return t ? { x: t.x, y: t.y, dash: true, dashRange: 220 } : null; }
    return { x: this.holder.x, y: this.holder.y, flee: true };
  }
  drawArena(g) {
    bgGrad(g, '#5c2a0a', '#1a0a03');
    const bx = this.box; g.fillStyle = '#ffe8cc'; rr(g, bx.x, bx.y, bx.w, bx.h, 30); g.fill(); g.lineWidth = 8; g.strokeStyle = '#d9480f'; g.stroke();
    g.fillStyle = 'rgba(217,72,15,.08)'; for (let i = 0; i < 16; i++) for (let j = 0; j < 8; j++) if ((i + j) % 2) g.fillRect(bx.x + i * bx.w / 16, bx.y + j * bx.h / 8, bx.w / 16, bx.h / 8);
  }
  drawOver(g) {
    const h = this.holder; if (!h || !h.alive) return;
    const pulse = 1 + Math.sin(performance.now() / (this.fuse < 3 ? 40 : 120)) * 0.12, x = h.x, y = h.y - h.r - 30;
    g.fillStyle = '#212529'; g.beginPath(); g.arc(x, y, 22 * pulse, 0, TAU); g.fill();
    g.strokeStyle = '#868e96'; g.lineWidth = 4; g.beginPath(); g.moveTo(x + 10, y - 18); g.quadraticCurveTo(x + 22, y - 34, x + 30, y - 30); g.stroke();
    this.fx.puff(x + 30, y - 30, Math.random() < 0.5 ? '#ffd43b' : '#ff922b', 4, 0.25, rnd(-40, 40), rnd(-80, -20));
    if (this.fuse < 3) { g.fillStyle = `rgba(255,0,0,${0.12 + Math.sin(performance.now() / 60) * 0.08})`; g.fillRect(0, 0, 1600, 900); }
  }
  drawHUD(g) { strip(g, this.ps.map(p => ({ name: p.name, color: p.color, val: !this.blobOf(p).alive ? '💥' : this.holder && this.holder.p === p ? '💣' : '🙂', dim: !this.blobOf(p).alive, hi: this.holder && this.holder.p === p }))); }
}

/* ---------------------------------------------------------------- 🧝 ODD ELF OUT — social deduction, talk out loud! */
const ELF_WORDS = {
  'Winter things': ['Snowman', 'Igloo', 'Sled', 'Mittens', 'Icicle', 'Snowball', 'Ski Lift', 'Hot Cocoa'],
  'Animals': ['Penguin', 'Polar Bear', 'Reindeer', 'Owl', 'Walrus', 'Husky', 'Fox', 'Seal'],
  'Food': ['Pizza', 'Pancakes', 'Soup', 'Gingerbread', 'Popcorn', 'Tacos', 'Ice Cream', 'Spaghetti'],
  'Places': ['Beach', 'Airport', 'School', 'Hospital', 'Castle', 'Zoo', 'Supermarket', 'Library'],
  'Jobs': ['Doctor', 'Pilot', 'Chef', 'Firefighter', 'Teacher', 'Astronaut', 'Farmer', 'Magician'],
  'Sports': ['Soccer', 'Ice Hockey', 'Skiing', 'Tennis', 'Basketball', 'Swimming', 'Bowling', 'Golf'],
};
class OddElfOut {
  constructor(c) { this.c = c; this.fx = new Particles(); this.round = 0; this.pts = new Map(); }
  start() { this.ps = this.c.roster(3); this.ps.forEach(p => this.pts.set(p, 0)); this.R = Math.min(3, Math.max(2, this.ps.length - 1)); this.next(); }
  next() {
    this.round++; if (this.round > this.R) return this.end();
    this.cat = pick(Object.keys(ELF_WORDS)); this.word = pick(ELF_WORDS[this.cat]);
    const humans = this.ps.filter(p => !p.bot); this.elf = pick(humans.length >= 3 ? humans : this.ps);
    this.order = shuffle(this.ps.slice()); this.phase = 'clue'; this.t = 60; this.ready = new Set(); this.votes = new Map();
    for (const p of this.ps) if (!p.bot) this.c.setLayout(p, {
      name: 'choice', prompt: p === this.elf ? `🧝 YOU are the ODD ELF! Category: ${this.cat}. Blend in — fake a clue!` : `Secret word: ${this.word.toUpperCase()}  (category: ${this.cat})`,
      options: [{ label: '✅ Ready to vote', color: '#51cf66' }], cols: 1, secret: true });
    this.c.sfx.whoosh();
  }
  onAct(p, m) {
    if (!this.pts.has(p) || m.a !== 'choice') return;
    if (this.phase === 'clue') { this.ready.add(p); if (!p.bot) this.c.setLayout(p, { name: 'msg', emoji: '⏳', text: 'Waiting for others…', sub: p === this.elf ? 'Shh… you are the elf 🧝' : `Word: ${this.word}` }); if (this.ready.size >= Math.ceil(this.ps.filter(q => !q.bot).length * 0.6) && this.ps.some(q => !q.bot)) this.t = Math.min(this.t, 1); }
    else if (this.phase === 'vote' && !this.votes.has(p)) { const t = this.ps[m.i | 0]; if (!t || t === p) return; this.votes.set(p, t); if (!p.bot) this.c.setLayout(p, { name: 'msg', emoji: '🗳️', text: `You voted ${t.name}`, sub: '' }); if (this.votes.size === this.ps.length) this.t = Math.min(this.t, 0.5); }
    else if (this.phase === 'guess' && p === this.elf) this.elfGuess(this.guessOpts[m.i | 0]);
  }
  startVote() {
    this.phase = 'vote'; this.t = 25; this.c.sfx.alarm(); this.c.banner('🗳️ VOTE: WHO IS THE ODD ELF?', 1800, '#ffd43b');
    for (const p of this.ps) if (!p.bot) this.c.setLayout(p, { name: 'choice', prompt: 'Who is the Odd Elf?', cols: 2, options: this.ps.map((q, i) => ({ label: q.name, color: q.color, disabled: q === p })) });
    for (const b of this.ps) if (b.bot) { const others = this.ps.filter(q => q !== b); setTimeout(() => this.onAct(b, { a: 'choice', i: this.ps.indexOf(Math.random() < 0.35 && b !== this.elf ? this.elf : pick(others)) }), rnd(1500, 6000)); }
  }
  tally() {
    const cnt = new Map(); for (const t of this.votes.values()) cnt.set(t, (cnt.get(t) || 0) + 1);
    let max = 0; for (const v of cnt.values()) max = Math.max(max, v);
    const top = [...cnt.entries()].filter(([, v]) => v === max).map(([k]) => k);
    this.cnt = cnt; this.accused = top.length === 1 ? top[0] : null;
    if (this.accused === this.elf) {
      this.phase = 'guess'; this.t = 15;
      this.guessOpts = shuffle([this.word, ...shuffle(ELF_WORDS[this.cat].filter(w => w !== this.word)).slice(0, 5)]);
      this.c.banner(`CAUGHT! ${this.elf.name} WAS THE ELF!`, 2200, this.elf.color); this.c.sfx.ding();
      for (const p of this.ps) if (!p.bot) this.c.setLayout(p, p === this.elf ? { name: 'choice', prompt: 'Last chance! Guess the secret word to steal the win:', cols: 2, options: this.guessOpts.map((w, i) => ({ label: w, color: OPT_COLORS[i] })) } : { name: 'msg', emoji: '🧝', text: `${this.elf.name} is guessing the word…`, sub: '' });
      if (this.elf.bot) setTimeout(() => this.elfGuess(Math.random() < 0.3 ? this.word : pick(this.guessOpts)), 2500);
    } else {
      this.pts.set(this.elf, this.pts.get(this.elf) + 3);
      this.c.banner(`THE ELF ESCAPED! It was ${this.elf.name}`, 2600, this.elf.color); this.c.sfx.buzz();
      this.result(`${this.elf.name} fooled everyone! (+3)`);
    }
  }
  elfGuess(w) {
    if (this.phase !== 'guess') return;
    if (w === this.word) { this.pts.set(this.elf, this.pts.get(this.elf) + 3); this.result(`${this.elf.name} guessed "${this.word}" and steals it! (+3)`); this.c.sfx.win(); }
    else { for (const p of this.ps) if (p !== this.elf) this.pts.set(p, this.pts.get(p) + (this.votes.get(p) === this.elf ? 2 : 1)); this.result(`The word was "${this.word}". Elf hunters win! (+2)`); this.fx.confetti(100); this.c.sfx.win(); }
  }
  result(text) {
    this.phase = 'result'; this.t = 5; this.resultText = text;
    for (const p of this.ps) if (!p.bot) this.c.setLayout(p, { name: 'msg', emoji: '🧝', text, sub: `Secret word: ${this.word}` });
  }
  update(dt) {
    this.fx.update(dt); this.t -= dt;
    if (this.phase === 'clue') { for (const b of this.ps) if (b.bot && !this.ready.has(b) && this.t < 50) this.ready.add(b); if (this.t <= 0) this.startVote(); }
    else if (this.phase === 'vote' && this.t <= 0) this.tally();
    else if (this.phase === 'guess' && this.t <= 0) this.elfGuess(null);
    else if (this.phase === 'result' && this.t <= 0) this.next();
  }
  end() { if (this.done) return; this.done = true; this.c.finish(rankBy(this.ps, p => this.pts.get(p), p => `${this.pts.get(p)} pts`)); }
  draw(g) {
    bgGrad(g, '#2b8a3e', '#05200d');
    for (let i = 0; i < 30; i++) { const x = (i * 157) % 1600, y = (i * 97 + performance.now() / 30) % 900; g.fillStyle = 'rgba(255,255,255,.5)'; g.beginPath(); g.arc(x, y, 3, 0, TAU); g.fill(); }
    txt(g, `🧝 ODD ELF OUT · ROUND ${Math.min(this.round, this.R)}/${this.R}`, 800, 110, 30, '#d3f9d8');
    if (this.phase === 'clue') {
      txt(g, `Category: ${this.cat}`, 800, 200, 56, '#ffd43b');
      para(g, 'Everyone checks their phone. One of you is the ODD ELF and does NOT know the word. Go around in this order and say ONE word clue out loud — then vote!', 800, 300, 1300, 30, '#fff');
      this.order.forEach((p, i) => { const x = 800 + (i - (this.order.length - 1) / 2) * 170; drawBlob(g, x, 520, 44, p.color, Math.PI / 2, { name: p.name, hat: '#e03131' }); txt(g, String(i + 1), x, 600, 30, '#ffd43b'); if (this.ready.has(p)) txt(g, '✅', x + 40, 480, 26); });
      timeBar(g, this.t / 60, 700); txt(g, 'Press "Ready to vote" on your phone when done', 800, 750, 24, '#d3f9d8', 'center', 'Nunito');
    } else if (this.phase === 'vote') {
      txt(g, 'WHO IS THE ODD ELF?', 800, 230, 70, '#ffd43b');
      this.ps.forEach((p, i) => { const x = 800 + (i - (this.ps.length - 1) / 2) * 170; drawBlob(g, x, 480, 50, p.color, Math.PI / 2, { name: p.name }); if ([...this.votes.keys()].includes(p)) txt(g, '🗳️', x, 560, 30); });
      timeBar(g, this.t / 25, 700);
    } else {
      this.ps.forEach((p, i) => { const x = 800 + (i - (this.ps.length - 1) / 2) * 170, v = (this.cnt && this.cnt.get(p)) || 0; drawBlob(g, x, 480, 50, p.color, Math.PI / 2, { name: p.name, hat: p === this.elf ? '#2f9e44' : null, crown: false }); txt(g, `${v} vote${v === 1 ? '' : 's'}`, x, 560, 22, '#fff', 'center', 'Nunito'); if (p === this.elf) txt(g, '🧝 ELF', x, 360, 26, '#51cf66'); });
      txt(g, this.phase === 'guess' ? `${this.elf.name} is guessing the word…` : (this.resultText || ''), 800, 230, 38, '#ffd43b', 'center', 'Nunito');
      if (this.phase === 'result') txt(g, `The word was: ${this.word}`, 800, 680, 44, '#fff');
    }
    strip(g, this.ps.map(p => ({ name: p.name, color: p.color, val: this.pts.get(p) })));
    this.fx.draw(g);
  }
}

/* ---------------------------------------------------------------- 👉 MOST LIKELY TO */
const LIKELY = ['…eat the last cookie and blame the dog', '…fall asleep during a movie', '…get lost in their own neighborhood', '…win a snowball fight', '…laugh at their own joke', '…become famous', '…sing loudly in the car',
  '…forget their own birthday', '…build the biggest snowman', '…survive a zombie apocalypse', '…cry at a cartoon', '…burn the toast', '…talk to animals', '…take 100 selfies a day', '…be late to their own party',
  '…eat dessert first', '…win a dance battle', '…trip over nothing', '…become a superhero', '…lose their phone while holding it', '…start a food fight', '…wear socks with sandals', '…know every song lyric', '…cheat at board games',
  '…go to space', '…adopt 10 cats', '…sleep until noon', '…invent something crazy', '…scream at a spider', '…open presents early'];
class MostLikely extends QuizGame {
  constructor(c) { super(c, { rounds: 7, time: 15, min: 3, title: 'MOST LIKELY TO', bg: ['#c2255c', '#2b0716'] }); this.prompts = shuffle(LIKELY.slice()); }
  makeRound() {
    const pr = this.prompts.pop(), ps = this.ps;
    return {
      text: `Who is most likely to ${pr}`, phone: `Most likely to ${pr}`, opts: ps.map(p => p.name), colors: ps.map(p => p.color), cols: ps.length > 4 ? 3 : 2,
      botAnswer: () => irnd(0, ps.length - 1),
      resolve(answers, gm) {
        const cnt = new Map(); for (const a of answers.values()) cnt.set(a.i, (cnt.get(a.i) || 0) + 1);
        const max = Math.max(0, ...cnt.values()), top = [...cnt.entries()].filter(([, v]) => v === max).map(([k]) => k);
        gm.q.goodSet = new Set(top); gm.q.revealText = top.length ? `${top.map(i => ps[i].name).join(' & ')}! (${max} vote${max === 1 ? '' : 's'})` : 'Nobody voted!';
        const m = new Map(); for (const [p, a] of answers) if (top.includes(a.i)) m.set(p, 300);
        for (const i of top) m.set(ps[i], (m.get(ps[i]) || 0) + 100);
        return m;
      },
    };
  }
}

/* ---------------------------------------------------------------- 1️⃣ LOWEST UNIQUE */
class LowestUnique extends QuizGame {
  constructor(c) { super(c, { rounds: 6, time: 10, min: 2, title: 'LOWEST UNIQUE NUMBER', bg: ['#0b7285', '#03222a'] }); }
  makeRound() {
    return {
      text: 'Pick the LOWEST number that NOBODY else picks!', opts: ['1', '2', '3', '4', '5', '6', '7', '8', '9', '10'], cols: 5, big: true,
      botAnswer: () => Math.min(9, Math.floor(Math.random() ** 1.6 * 10)),
      resolve(answers, gm) {
        const cnt = new Map(); for (const a of answers.values()) cnt.set(a.i, (cnt.get(a.i) || 0) + 1);
        const uniq = [...cnt.entries()].filter(([, v]) => v === 1).map(([k]) => k).sort((a, b) => a - b);
        const m = new Map();
        if (uniq.length) { const w = [...answers.entries()].find(([, a]) => a.i === uniq[0])[0]; m.set(w, 1000); gm.q.goodSet = new Set([uniq[0]]); gm.q.revealText = `${w.name} wins with ${uniq[0] + 1}!`; }
        else gm.q.revealText = 'Everyone clashed! Nobody wins.';
        return m;
      },
    };
  }
}

/* ---------------------------------------------------------------- ✊ ROCK PAPER SCISSORS ROYALE */
class RPSRoyale extends QuizGame {
  constructor(c) { super(c, { rounds: 6, time: 7, min: 2, title: 'RPS ROYALE', bg: ['#5f3dc4', '#140a33'] }); }
  makeRound() {
    return {
      text: 'Everyone vs everyone! Beat as many as you can.', opts: ['✊ Rock', '✋ Paper', '✌️ Scissors'], cols: 3, big: true,
      resolve(answers, gm) {
        const m = new Map(), arr = [...answers.entries()];
        for (const [p, a] of arr) { let w = 0; for (const [q, b] of arr) if (q !== p && (a.i - b.i + 3) % 3 === 1) w++; if (w) m.set(p, w * 100); }
        const c = [0, 1, 2].map(i => arr.filter(([, a]) => a.i === i).length); gm.q.revealText = `✊ ${c[0]}   ✋ ${c[1]}   ✌️ ${c[2]}`;
        return m;
      },
    };
  }
}

/* ---------------------------------------------------------------- ❓ TRIVIA BLITZ */
const TRIVIA = [
  ['Which animal is the largest on Earth?', 'Blue whale', 'Elephant', 'Giraffe', 'Great white shark'], ['How many legs does a spider have?', '8', '6', '10', '12'],
  ['What do bees make?', 'Honey', 'Milk', 'Silk', 'Wax candles'], ['What is frozen water called?', 'Ice', 'Steam', 'Fog', 'Dew'],
  ['Which planet is known as the Red Planet?', 'Mars', 'Venus', 'Jupiter', 'Saturn'], ['How many days are in a leap year?', '366', '365', '364', '360'],
  ['What is the capital of France?', 'Paris', 'Rome', 'Madrid', 'Berlin'], ['Which bird is famous for not being able to fly and living in Antarctica?', 'Penguin', 'Ostrich', 'Puffin', 'Seagull'],
  ['What color do you get mixing blue and yellow?', 'Green', 'Purple', 'Orange', 'Brown'], ['How many minutes are in an hour?', '60', '100', '30', '90'],
  ['Which is the coldest continent?', 'Antarctica', 'Europe', 'Asia', 'North America'], ['What do caterpillars turn into?', 'Butterflies', 'Bees', 'Beetles', 'Spiders'],
  ['Which ocean is the biggest?', 'Pacific', 'Atlantic', 'Indian', 'Arctic'], ['What is the tallest animal?', 'Giraffe', 'Elephant', 'Horse', 'Camel'],
  ['How many sides does a hexagon have?', '6', '5', '8', '7'], ['What gas do plants breathe in?', 'Carbon dioxide', 'Oxygen', 'Helium', 'Nitrogen'],
  ['Which instrument has 88 keys?', 'Piano', 'Guitar', 'Violin', 'Drums'], ['What is the fastest land animal?', 'Cheetah', 'Lion', 'Horse', 'Greyhound'],
  ['In which season do bears usually hibernate?', 'Winter', 'Summer', 'Spring', 'Autumn'], ['What shape is a snowflake usually?', '6-sided', '4-sided', 'Round', '8-sided'],
  ['What is the main ingredient in guacamole?', 'Avocado', 'Tomato', 'Pea', 'Cucumber'], ['Which country has the most people?', 'India', 'USA', 'Brazil', 'Russia'],
  ['How many continents are there?', '7', '5', '6', '8'], ['What is the hardest natural material?', 'Diamond', 'Gold', 'Iron', 'Granite'],
  ['Which sport uses a puck?', 'Ice hockey', 'Curling', 'Golf', 'Tennis'], ['What do you call a baby kangaroo?', 'Joey', 'Cub', 'Kid', 'Calf'],
  ['Which is a mammal?', 'Dolphin', 'Shark', 'Salmon', 'Octopus'], ['How many players on a soccer team on the field?', '11', '9', '10', '12'],
  ['Where is the Great Pyramid of Giza?', 'Egypt', 'Mexico', 'Peru', 'Greece'], ['What is the boiling point of water in °C?', '100', '90', '120', '80'],
  ['Which planet has the most famous rings?', 'Saturn', 'Mars', 'Earth', 'Mercury'], ['What do reindeer eat in winter?', 'Lichen / moss', 'Fish', 'Pine cones', 'Snow only'],
  ['How many hearts does an octopus have?', '3', '1', '2', '8'], ['What is the largest planet?', 'Jupiter', 'Saturn', 'Neptune', 'Earth'],
  ['Which month has the fewest days?', 'February', 'April', 'June', 'November'], ['What is a group of wolves called?', 'Pack', 'Herd', 'Flock', 'School'],
  ['What language has the most native speakers?', 'Mandarin Chinese', 'English', 'Spanish', 'Hindi'], ['Which metal is liquid at room temperature?', 'Mercury', 'Iron', 'Silver', 'Aluminum'],
  ['Where do polar bears live?', 'Arctic', 'Antarctica', 'Alps', 'Sahara'], ['What do you call a word that reads the same backwards?', 'Palindrome', 'Synonym', 'Acronym', 'Homonym'],
  ['How many strings does a standard guitar have?', '6', '4', '5', '8'], ['Which vitamin do we get from sunlight?', 'Vitamin D', 'Vitamin C', 'Vitamin A', 'Vitamin B12'],
  ['What is the closest star to Earth?', 'The Sun', 'Sirius', 'Polaris', 'Alpha Centauri'], ['What is the largest organ of the human body?', 'Skin', 'Liver', 'Heart', 'Brain'],
  ['Which famous winter sport event happens every 4 years?', 'Winter Olympics', 'World Series', 'Super Bowl', 'Tour de France'], ['What temperature does water freeze at in °C?', '0', '10', '-10', '32'],
  ['Which animal changes color to hide?', 'Chameleon', 'Zebra', 'Panda', 'Flamingo'], ['What is a baby cat called?', 'Kitten', 'Puppy', 'Foal', 'Chick'],
  ['How many colors are in a rainbow?', '7', '5', '6', '9'], ['Which is the longest river in the world?', 'Nile', 'Thames', 'Danube', 'Mississippi'],
];
class TriviaBlitz extends QuizGame {
  constructor(c) { super(c, { rounds: 8, time: 14, title: 'TRIVIA BLITZ', bg: ['#1c3d8f', '#060f2b'] }); this.bank = shuffle(TRIVIA.slice()); }
  makeRound() { const [q, ...a] = this.bank.pop(), opts = shuffle(a.slice()); return { text: q, opts, correct: opts.indexOf(a[0]), revealText: `Answer: ${a[0]}` }; }
}

/* ---------------------------------------------------------------- 🎨 COLOR CLASH */
const CC = [['RED', '#ff4d6d'], ['BLUE', '#4dabff'], ['GREEN', '#51cf66'], ['YELLOW', '#ffd43b'], ['PURPLE', '#cc5de8'], ['ORANGE', '#ff922b']];
class ColorClash extends QuizGame {
  constructor(c) { super(c, { rounds: 10, time: 6, title: 'COLOR CLASH', bg: ['#212529', '#000'] }); }
  makeRound(n) {
    const word = pick(CC); let ink; do { ink = pick(CC); } while (ink === word);
    const mode = n % 3 === 0 ? 'WORD' : 'INK', target = mode === 'INK' ? ink : word;
    const opts = shuffle([word, ink, ...shuffle(CC.filter(c => c !== word && c !== ink)).slice(0, 2)]);
    return {
      text: '', phone: mode === 'INK' ? 'Tap the INK color!' : 'Tap what the WORD says!', opts: opts.map(o => o[0]), colors: shuffle(CC.map(c => c[1])).slice(0, 4), correct: opts.indexOf(target), big: true,
      revealText: `It was ${target[0]}`,
      draw(g) { txt(g, mode === 'INK' ? 'TAP THE INK COLOR' : 'TAP WHAT THE WORD SAYS', 800, 190, 34, '#adb5bd'); txt(g, word[0], 800, 340, 170, ink[1]); },
    };
  }
}

/* ---------------------------------------------------------------- 🔢 COUNT IT */
const COUNT_SETS = [['🐧', '🦭'], ['⛄', '🎄'], ['❄️', '⭐'], ['🍪', '🧁'], ['🦌', '🐻‍❄️'], ['🎁', '🧦'], ['🐟', '🐠']];
class CountIt extends QuizGame {
  constructor(c) { super(c, { rounds: 7, time: 10, title: 'COUNT IT!', bg: ['#0c8599', '#022c33'] }); }
  makeRound(n) {
    const [a, b] = pick(COUNT_SETS), N = irnd(6, 9 + n * 3), M = irnd(3, 6 + n * 2), pts = [];
    const place = e => { for (let t = 0; t < 50; t++) { const x = rnd(140, 1460), y = rnd(170, 820); if (pts.every(p => Math.hypot(p.x - x, p.y - y) > 64)) { pts.push({ x, y, e, r: rnd(-0.4, 0.4) }); return; } } };
    for (let i = 0; i < N; i++) place(a); for (let i = 0; i < M; i++) place(b);
    const real = pts.filter(p => p.e === a).length, set = new Set([real]); while (set.size < 4) set.add(Math.max(1, real + irnd(-4, 4)));
    const opts = shuffle([...set]), SHOW = 3.5;
    return {
      text: `How many ${a}?`, opts, correct: opts.indexOf(real), big: true, revealText: `There were ${real} ${a}`, hideOpts: true,
      draw(g, gm) {
        const showing = gm.phase === 'reveal' || gm.askAt - gm.t < SHOW;
        if (showing) { g.font = '58px serif'; g.textAlign = 'center'; g.textBaseline = 'middle'; for (const p of pts) { g.save(); g.translate(p.x, p.y); g.rotate(p.r); g.globalAlpha = gm.phase === 'reveal' && p.e !== a ? 0.25 : 1; g.fillText(p.e, 0, 0); g.restore(); } g.globalAlpha = 1; }
        else { txt(g, `How many ${a} were there?`, 800, 420, 64); txt(g, 'Answer on your phone!', 800, 520, 30, '#c5f6fa', 'center', 'Nunito'); }
        if (showing && gm.phase === 'ask') txt(g, `Count the ${a}!  ${Math.ceil(SHOW - (gm.askAt - gm.t))}`, 800, 150, 40, '#ffd43b');
        if (gm.phase === 'reveal') txt(g, `${real} ${a}`, 800, 470, 120, '#ffd43b');
      },
    };
  }
}

/* ---------------------------------------------------------------- ➕ QUICK MATH */
class QuickMath extends QuizGame {
  constructor(c) { super(c, { rounds: 8, time: 10, title: 'QUICK MATH', bg: ['#2f9e44', '#05200d'] }); }
  makeRound(n) {
    let a, b, op, ans;
    if (n <= 3) { a = irnd(2, 20); b = irnd(2, 20); op = pick(['+', '−']); if (op === '−' && b > a) [a, b] = [b, a]; ans = op === '+' ? a + b : a - b; }
    else if (n <= 6) { a = irnd(2, 12); b = irnd(2, 10); op = '×'; ans = a * b; }
    else { b = irnd(2, 9); ans = irnd(2, 12); a = b * ans; op = '÷'; }
    const set = new Set([ans]); while (set.size < 4) set.add(Math.max(0, ans + pick([-10, -2, -1, 1, 2, 10, 3, -3])));
    const opts = shuffle([...set]);
    return { text: '', opts, correct: opts.indexOf(ans), big: true, revealText: `${a} ${op} ${b} = ${ans}`, phone: `${a} ${op} ${b} = ?`, draw(g) { txt(g, `${a} ${op} ${b} = ?`, 800, 320, 150, '#fff'); } };
  }
}

/* ---------------------------------------------------------------- 🐱 COPY CAT — remember the color sequence */
const PADS = [['#ff4d6d', 'RED'], ['#4dabff', 'BLUE'], ['#ffd43b', 'YELLOW'], ['#51cf66', 'GREEN']];
class CopyCat {
  constructor(c) { this.c = c; this.fx = new Particles(); this.round = 0; this.R = 6; this.pts = new Map(); }
  start() { this.ps = this.c.roster(1); this.ps.forEach(p => this.pts.set(p, 0)); this.next(); }
  next() {
    this.round++; if (this.round > this.R) return this.end();
    this.len = this.round + 2; this.seq = Array.from({ length: this.len }, () => irnd(0, 3));
    this.phase = 'show'; this.t = 1; this.k = -1; this.lit = -1; this.entry = new Map(); this.res = new Map();
    this.c.setLayout('all', { name: 'msg', emoji: '👀', text: 'WATCH THE TV!', sub: `Remember ${this.len} colors` });
  }
  onAct(p, m) {
    if (this.phase !== 'input' || m.a !== 'choice' || !this.pts.has(p) || this.res.has(p)) return;
    const e = this.entry.get(p) || []; e.push(m.i | 0); this.entry.set(p, e);
    if (e[e.length - 1] !== this.seq[e.length - 1]) { this.res.set(p, 0); if (!p.bot) this.c.setLayout(p, { name: 'msg', emoji: '❌', text: 'Oops!', sub: 'Wrong color' }); this.c.sfx.buzz(); }
    else if (e.length === this.len) { const g = 100 * this.len + Math.round(this.t * 20); this.res.set(p, g); this.pts.set(p, this.pts.get(p) + g); if (!p.bot) this.c.setLayout(p, { name: 'msg', emoji: '✅', text: `+${g}`, sub: 'Perfect memory!' }); this.c.sfx.ding(); }
    if (this.res.size === this.ps.length) this.t = Math.min(this.t, 0.4);
  }
  update(dt) {
    this.fx.update(dt); this.t -= dt;
    if (this.phase === 'show') {
      if (this.t <= 0) {
        if (this.lit >= 0) { this.lit = -1; this.t = 0.18; return; }
        this.k++;
        if (this.k >= this.len) {
          this.phase = 'input'; this.t = 4 + this.len * 1.3; this.tMax = this.t;
          for (const p of this.ps) if (!p.bot) this.c.setLayout(p, { name: 'choice', prompt: `Tap the ${this.len} colors in order!`, cols: 2, multi: this.len, options: PADS.map(([c, n]) => ({ label: n, color: c })) });
          for (const b of this.ps) if (b.bot) { const ok = Math.random() < 1.1 - this.len * 0.09; this.seq.forEach((v, i) => setTimeout(() => this.onAct(b, { a: 'choice', i: ok || i < this.len - 1 ? v : (v + 1) % 4 }), 900 + i * 450)); }
          return;
        }
        this.lit = this.seq[this.k]; this.t = Math.max(0.35, 0.7 - this.round * 0.05); this.c.sfx.tone ? 0 : this.c.sfx.blip();
      }
    } else if (this.phase === 'input' && this.t <= 0) {
      for (const p of this.ps) if (!this.res.has(p)) this.res.set(p, 0);
      this.phase = 'reveal'; this.t = 2.5;
      for (const p of this.ps) if (!p.bot && !this.res.get(p)) this.c.setLayout(p, { name: 'msg', emoji: '⏰', text: 'Time up!', sub: '' });
    } else if (this.phase === 'reveal' && this.t <= 0) this.next();
  }
  end() { if (this.done) return; this.done = true; this.c.finish(rankBy(this.ps, p => this.pts.get(p), p => `${this.pts.get(p)} pts`)); }
  draw(g) {
    bgGrad(g, '#343a40', '#0b0c0f');
    txt(g, `COPY CAT · ${this.len} COLORS · ROUND ${Math.min(this.round, this.R)}/${this.R}`, 800, 110, 28, '#ced4da');
    PADS.forEach(([c], i) => {
      const x = 800 + (i % 2 ? 20 : -320), y = 170 + ((i / 2) | 0) * 330, on = this.lit === i || (this.phase === 'reveal' && false);
      g.globalAlpha = on ? 1 : 0.28; g.fillStyle = c; rr(g, x, y, 300, 300, 40); g.fill();
      if (on) { g.lineWidth = 10; g.strokeStyle = '#fff'; g.stroke(); }
      g.globalAlpha = 1;
    });
    if (this.phase === 'input') { timeBar(g, this.t / this.tMax, 840); txt(g, 'Now tap the sequence on your phone!', 800, 880, 24, '#fff', 'center', 'Nunito'); }
    if (this.phase === 'reveal') { txt(g, this.seq.map(i => PADS[i][1][0]).join(' '), 800, 500, 60, '#fff'); }
    strip(g, this.ps.map(p => ({ name: p.name, color: p.color, val: this.pts.get(p), hi: this.res && this.res.get(p) > 0 })));
    this.fx.draw(g);
  }
}

/* ---------------------------------------------------------------- ✏️ DOODLE DASH — draw on your phone, others guess */
const DOODLE_WORDS = ['Snowman', 'Cat', 'House', 'Sun', 'Pizza', 'Car', 'Tree', 'Fish', 'Rocket', 'Ice Cream', 'Penguin', 'Guitar', 'Bicycle', 'Rainbow', 'Dragon', 'Banana', 'Castle', 'Robot', 'Glasses', 'Umbrella',
  'Spider', 'Cake', 'Airplane', 'Moon', 'Flower', 'Dog', 'Mountain', 'Boat', 'Clock', 'Hat', 'Elephant', 'Ghost', 'Crown', 'Igloo', 'Sled', 'Mitten', 'Candle', 'Key', 'Lightbulb', 'Snail', 'Whale', 'Owl', 'Book', 'Shoe', 'Heart'];
class DoodleDash {
  constructor(c) { this.c = c; this.fx = new Particles(); this.pts = new Map(); this.round = 0; this.words = shuffle(DOODLE_WORDS.slice()); }
  start() {
    this.ps = this.c.roster(2); this.ps.forEach(p => this.pts.set(p, 0));
    this.drawers = shuffle(this.ps.filter(p => !p.bot)).slice(0, 5); if (this.drawers.length === 1) this.drawers.push(this.drawers[0]);
    this.R = this.drawers.length; this.next();
  }
  next() {
    this.round++; if (this.round > this.R) return this.end();
    this.drawer = this.drawers[this.round - 1]; this.word = this.words.pop();
    this.opts = shuffle([this.word, ...shuffle(this.words.slice()).slice(0, 3)]);
    this.strokes = new Map(); this.order = []; this.guesses = new Map(); this.phase = 'draw'; this.t = this.tMax = 50;
    for (const p of this.ps) if (!p.bot) this.c.setLayout(p, p === this.drawer ? { name: 'draw', word: this.word } :
      { name: 'choice', prompt: `What is ${this.drawer.name} drawing?`, cols: 2, options: this.opts.map((w, i) => ({ label: w, color: OPT_COLORS[i] })) });
    this.c.banner(`✏️ ${this.drawer.name} is drawing!`, 1500, this.drawer.color);
    this.botT = new Map(this.ps.filter(p => p.bot && p !== this.drawer).map(p => [p, rnd(10, 30)]));
  }
  onAct(p, m) {
    if (this.phase !== 'draw') return;
    if (p === this.drawer) {
      if (m.a === 'stroke' && Array.isArray(m.pts)) { let s = this.strokes.get(m.id); if (!s) { s = { c: String(m.c || '#000').slice(0, 9), w: clamp(+m.w || 6, 2, 40), pts: [] }; this.strokes.set(m.id, s); this.order.push(s); } for (const q of m.pts.slice(0, 200)) if (Array.isArray(q)) s.pts.push([clamp(+q[0], 0, 1), clamp(+q[1], 0, 1)]); }
      else if (m.a === 'clear') { this.strokes = new Map(); this.order = []; }
      return;
    }
    if (m.a !== 'choice' || this.guesses.has(p) || !this.pts.has(p)) return;
    const ok = this.opts[m.i | 0] === this.word, gain = ok ? Math.round(300 + 700 * this.t / this.tMax) : 0;
    this.guesses.set(p, ok);
    if (ok) { this.pts.set(p, this.pts.get(p) + gain); this.pts.set(this.drawer, this.pts.get(this.drawer) + 150); this.c.sfx.ding(); this.fx.text(800, 160, `${p.name} got it!`, p.color, 34); }
    else this.c.sfx.buzz();
    if (!p.bot) this.c.setLayout(p, { name: 'msg', emoji: ok ? '✅' : '❌', text: ok ? `+${gain}` : 'Wrong!', sub: ok ? 'Nice eye!' : 'Better luck next round' });
    if (this.guesses.size >= this.ps.length - 1) this.t = Math.min(this.t, 0.5);
  }
  update(dt) {
    this.fx.update(dt); this.t -= dt;
    if (this.phase === 'draw') {
      for (const [b, at] of this.botT) if (this.tMax - this.t >= at) { this.botT.delete(b); this.onAct(b, { a: 'choice', i: Math.random() < 0.6 ? this.opts.indexOf(this.word) : irnd(0, 3) }); }
      if (this.t <= 0) { this.phase = 'reveal'; this.t = 3.5; this.c.sfx.win(); for (const p of this.ps) if (!p.bot && (p === this.drawer || !this.guesses.has(p))) this.c.setLayout(p, { name: 'msg', emoji: '✏️', text: `It was: ${this.word}`, sub: '' }); }
    } else if (this.phase === 'reveal' && this.t <= 0) this.next();
  }
  end() { if (this.done) return; this.done = true; this.c.finish(rankBy(this.ps, p => this.pts.get(p), p => `${this.pts.get(p)} pts`)); }
  draw(g) {
    bgGrad(g, '#e8590c', '#3d1402');
    const X = 250, Y = 150, W = 1100, H = 690;
    g.fillStyle = 'rgba(0,0,0,.3)'; rr(g, X + 8, Y + 12, W, H, 20); g.fill();
    g.fillStyle = '#fffdf7'; rr(g, X, Y, W, H, 20); g.fill();
    g.lineCap = 'round'; g.lineJoin = 'round';
    for (const s of this.order) { if (!s.pts.length) continue; g.strokeStyle = s.c; g.lineWidth = s.w * (W / 600); g.beginPath(); s.pts.forEach((q, i) => { const x = X + q[0] * W, y = Y + q[1] * H; i ? g.lineTo(x, y) : g.moveTo(x, y); }); if (s.pts.length === 1) g.lineTo(X + s.pts[0][0] * W + 0.1, Y + s.pts[0][1] * H); g.stroke(); }
    txt(g, `✏️ ${this.drawer ? this.drawer.name : ''} is drawing · ${Math.min(this.round, this.R)}/${this.R}`, 800, 110, 28, '#fff');
    if (this.phase === 'draw') { txt(g, this.word.replace(/[A-Za-z]/g, '_ ').trim(), 800, 870, 34, '#fff'); timeBar(g, this.t / this.tMax, 128); }
    else txt(g, this.word.toUpperCase(), 800, 495, 110, '#e8590c');
    strip(g, this.ps.map(p => ({ name: p.name, color: p.color, val: this.pts.get(p), hi: this.guesses && this.guesses.get(p) })), 46, 1600);
    this.fx.draw(g);
  }
}
