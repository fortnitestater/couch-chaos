/* Owner cheats: the code is NOT in this file, only a salted, slow (PBKDF2) fingerprint of it, so reading the source does not reveal it.
   The TV is the one that checks the code and decides who is the owner; a phone cannot skip that check. */
const OWNER = { salt: '23359d01925b4df8cd87fa50930da574', iter: 600000, hash: '3b5e317b2710b8d53b6b65fa946efd2202494c25fdeaa76d065d9e09bb7ecdca' };
async function ownerCheck(code) {
  const norm = String(code || '').toLowerCase().replace(/[^a-z0-9]/g, '');
  if (norm.length < 8) return false;
  const key = await crypto.subtle.importKey('raw', new TextEncoder().encode(norm), 'PBKDF2', false, ['deriveBits']);
  const salt = new Uint8Array(OWNER.salt.match(/../g).map(h => parseInt(h, 16)));
  const bits = new Uint8Array(await crypto.subtle.deriveBits({ name: 'PBKDF2', salt, iterations: OWNER.iter, hash: 'SHA-256' }, key, 256));
  return Array.from(bits).map(b => b.toString(16).padStart(2, '0')).join('') === OWNER.hash;
}
