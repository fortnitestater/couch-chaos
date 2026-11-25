/* Couch Chaos — real buttons. Turns every <button> into a physical keycap (see buttons.css):
   sinks the instant your finger lands, springs back when you let go, with a soft click + buzz. */
(() => {
  const SKIP = '.linkBtn,.tcard,.card,.die,.noRb';
  let ac = null;
  function click(kind) {
    try {
      ac = ac || new (window.AudioContext || window.webkitAudioContext)();
      if (ac.state === 'suspended') ac.resume();
      const t = ac.currentTime, o = ac.createOscillator(), g = ac.createGain(), f = ac.createBiquadFilter();
      // a short, warm "thock": a low sine dropping in pitch through a soft low-pass
      o.type = 'triangle'; o.frequency.setValueAtTime(kind ? 210 : 320, t); o.frequency.exponentialRampToValueAtTime(kind ? 90 : 140, t + 0.05);
      f.type = 'lowpass'; f.frequency.value = 1800;
      g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(kind ? 0.11 : 0.07, t + 0.004); g.gain.exponentialRampToValueAtTime(0.0001, t + (kind ? 0.06 : 0.045));
      o.connect(f); f.connect(g); g.connect(ac.destination); o.start(t); o.stop(t + 0.08);
    } catch (e) {}
  }
  function deco(b) {
    if (b.__rb || b.matches(SKIP)) return; b.__rb = 1;
    b.classList.add('rb');
    const c = b.style.backgroundColor || (b.style.background && !/gradient/.test(b.style.background) ? b.style.background : '');
    if (c) b.style.setProperty('--bc', c);
  }
  function scan(root) { if (root.matches && root.matches('button,a.bigCard')) deco(root); root.querySelectorAll && root.querySelectorAll('button,a.bigCard').forEach(deco); }
  const mo = new MutationObserver(ms => { for (const m of ms) for (const n of m.addedNodes) if (n.nodeType === 1) scan(n); });
  const go = () => { scan(document.body); mo.observe(document.body, { childList: true, subtree: true }); };
  if (document.body) go(); else addEventListener('DOMContentLoaded', go);
  const held = new Map();
  addEventListener('pointerdown', e => {
    const b = e.target.closest && e.target.closest('.rb'); if (!b || b.disabled) return;
    b.classList.add('press'); held.set(e.pointerId, b);
    click(b.classList.contains('pbtn') || b.classList.contains('tapBig') || b.classList.contains('readyBtn'));
    try { navigator.vibrate && navigator.vibrate(9); } catch (_) {}
  }, true);
  const up = e => { const b = held.get(e.pointerId); if (b) { held.delete(e.pointerId); setTimeout(() => b.classList.remove('press'), 40); } };
  addEventListener('pointerup', up, true); addEventListener('pointercancel', up, true);
})();
