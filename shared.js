/* Couch Chaos — shared bits (TV + controller) */
const PREFIX = 'couchchaos-v1-';
// Every game belongs to a "channel": its own colours, background pattern and HUD styling on the TV and the phones.
const CHANNELS = {
  show:   { name: 'Showtime',     c1: '#ff3d8b', c2: '#6a1fd6', deep: '#2a0b4f', accent: '#ffd43b', pat: 'dots' },
  music:  { name: 'Music Hall',   c1: '#ff2e93', c2: '#00a6e0', deep: '#2b0a3d', accent: '#5ff2ff', pat: 'waves' },
  cards:  { name: 'Card Club',    c1: '#13b07a', c2: '#065a42', deep: '#062e23', accent: '#f2c66d', pat: 'suits' },
  board:  { name: 'Board Night',  c1: '#ff8a1f', c2: '#c2255c', deep: '#3d0d12', accent: '#ffe08a', pat: 'grid' },
  sports: { name: 'Sports Arena', c1: '#2f7bff', c2: '#00b4d8', deep: '#071c4d', accent: '#c6ff3d', pat: 'stripes' },
};
const GAME_CH = { song: 'music', trivia: 'show', likely: 'show', elf: 'show', doodle: 'show', bomb: 'show', lowest: 'show', reflex: 'show',
  blackjack: 'cards', storm: 'cards', avalanche: 'cards', four: 'board', tycoon: 'board', tennis: 'sports', bowling: 'sports', airhockey: 'sports' };
const chOf = id => CHANNELS[GAME_CH[id]] || null;
const CODE_LETTERS = 'ABCDEFGHJKMNPQRSTUVWXYZ';
function esc(s) {
  return String(s).replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
}
const store = {
  get(k) { try { return localStorage.getItem(k); } catch (e) { return null; } },
  set(k, v) { try { localStorage.setItem(k, v); } catch (e) {} },
};

// WebRTC servers: STUN only. (The free public TURN relays were tested dead and one stalled connecting
// for ~8s, so they're left out.) To add your own TURN server, set window.CC_TURN = {urls, username, credential}
// in a <script> before shared.js on tv.html and play.html.
const PEER_OPTS = {
  debug: 0,
  config: {
    iceServers: [
      { urls: ['stun:stun.l.google.com:19302', 'stun:stun1.l.google.com:19302'] },
      { urls: 'stun:stun.cloudflare.com:3478' },
    ].concat(typeof window !== 'undefined' && window.CC_TURN ? [window.CC_TURN] : []),
  },
};

// Fetch relay credentials (if turn-config.js has a link) before the first connection. Never blocks more than 3s.
const ICE_READY = (async () => {
  const url = typeof window !== 'undefined' && window.CC_TURN_URL; if (!url) return;
  try {
    const ctl = new AbortController(), t = setTimeout(() => ctl.abort(), 3000);
    const list = await (await fetch(url, { signal: ctl.signal })).json(); clearTimeout(t);
    if (Array.isArray(list) && list.length) PEER_OPTS.config.iceServers.push(...list);
  } catch (e) { console.warn('relay config failed', e); }
})();
