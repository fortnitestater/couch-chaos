/* Couch Chaos — shared bits (TV + controller) */
const PREFIX = 'couchchaos-v1-';
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
