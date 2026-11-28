// Couch Chaos native core: window, bridge, lighting shader, primitives, particles, game registry.
#include "cc.h"
#include "mesh.h"
#include <emscripten/emscripten.h>
#include <sstream>
#include "rlgl.h"
#include <GLES2/gl2.h>

// ---------- JS side of the bridge (Module.game is the NativeGame adapter in tv.js) ----------
EM_JS(void, js_text, (float x, float y, float size, int rgba, const char* s, int align, int style, float w), {
  if (Module.game) Module.game.texts.push([x, y, size, rgba >>> 0, UTF8ToString(s), align, style, w]);
});
EM_JS(void, js_clip, (float x, float y, float w, float h), { if (Module.game) Module.game.texts.push(['clip', x, y, w, h]); });
EM_JS(float, js_measure, (const char* s, float size, int style), { return Module.measure ? Module.measure(UTF8ToString(s), size, style) : UTF8ToString(s).length * size * 0.55; });
EM_JS(void, js_layout, (int pid, const char* json), { if (Module.game) Module.game.layout(pid, UTF8ToString(json)); });
EM_JS(void, js_sfx, (const char* s), { if (Module.game) Module.game.sfx(UTF8ToString(s)); });
EM_JS(void, js_shake, (float a), { if (Module.game) Module.game.c.shake(a); });
EM_JS(void, js_banner, (const char* s, int ms, const char* c), { if (Module.game) Module.game.banner(UTF8ToString(s), ms, UTF8ToString(c)); });
EM_JS(void, js_buzz, (int pid, int ms), { if (Module.game) Module.game.buzz(pid, ms); });
EM_JS(void, js_send, (int pid, const char* s), { if (Module.game && Module.game.sendTo) Module.game.sendTo(pid, UTF8ToString(s)); });
EM_JS(void, js_img, (const char* k, float x, float y, float w, float h, float r, float a, float pan, float zoom), {
  if (Module.game) Module.game.texts.push(['img', UTF8ToString(k), x, y, w, h, r, a, pan, zoom]);
});
EM_JS(void, js_rr, (float x, float y, float w, float h, float r, int f1, int f2, int st, float lw, int grad, int gold), {
  if (Module.game) Module.game.texts.push(['rr', x, y, w, h, r, f1 >>> 0, f2 >>> 0, st >>> 0, lw, grad, gold]);
});
EM_JS(void, js_finish, (const char* s), { if (Module.game) Module.game.finish(UTF8ToString(s)); });

static inline Color HudA(Color c) { if (g_hudA < 0.999f) c.a = (unsigned char)(c.a * g_hudA); return c; }
void Text(float x, float y, float size, Color c, const std::string& s, int align, int style, float w) { c = HudA(c); if (c.a == 0 || s.empty()) return; js_text(x, y, size, (int)(((unsigned)c.r << 24) | ((unsigned)c.g << 16) | ((unsigned)c.b << 8) | c.a), s.c_str(), align, style, w); }
float Measure(const std::string& s, float size, int style) { return js_measure(s.c_str(), size, style); }
void TextClip(float x, float y, float w, float h) { js_clip(x, y, w, h); }
void TextUnclip() { js_clip(-1, -1, -1, -1); }
void Layout(int pid, const std::string& json) { js_layout(pid, json.c_str()); }
EM_JS(int, js_fresh, (const char* key, int n), {
  var k = 'cc_seen_' + UTF8ToString(key), M = Module.seen || (Module.seen = {}), u = M[k];
  if (!u) { u = []; try { var s = localStorage.getItem(k); if (s) u = s.split(',').map(Number).filter(function (v) { return v >= 0 && v < n; }); } catch (e) {} M[k] = u; }
  var last = u.length ? u[u.length - 1] : -1;
  if (u.length >= n) u.length = 0;
  var left = [];
  for (var i = 0; i < n; i++) if (u.indexOf(i) < 0 && (i !== last || n === 1)) left.push(i);
  if (!left.length) left.push(Math.max(0, last));
  var v = left[Math.floor(Math.random() * left.length)]; u.push(v);
  try { localStorage.setItem(k, u.join(',')); } catch (e) {}
  return v;
});
int Fresh(const char* key, int n) { return n > 0 ? js_fresh(key, n) : 0; }
void Sfx(const char* name) { js_sfx(name); }
void Shake(float a) { js_shake(a); }
void Finish(const std::string& r) { js_finish(r.c_str()); }
void Banner(const std::string& s, int ms, const char* c) { js_banner(s.c_str(), ms, c); }
void Buzz(int pid, int ms) { js_buzz(pid, ms); }
void Send(int pid, const std::string& json) { js_send(pid, json.c_str()); }
static int RGBA(Color c) { return (int)(((unsigned)c.r << 24) | ((unsigned)c.g << 16) | ((unsigned)c.b << 8) | c.a); }
void Img(const std::string& k, float x, float y, float w, float h, float r, float a, float pan, float zoom) { a *= g_hudA; if (a > 0.005f) js_img(k.c_str(), x, y, w, h, r, a, pan, zoom); }
void RRect(float x, float y, float w, float h, float r, Color f1, Color f2, Color st, float lw, bool grad, bool gold) { f1 = HudA(f1); f2 = HudA(f2); st = HudA(st); if (!f1.a && !f2.a && !st.a) return; js_rr(x, y, w, h, r, RGBA(f1), RGBA(f2), RGBA(st), lw, grad, gold); }

Color ParseColor(const std::string& hex) { unsigned v = 0x888888; if (hex.size() >= 7 && hex[0] == '#') v = (unsigned)strtoul(hex.substr(1, 6).c_str(), nullptr, 16); return Hex(v); }

// ---------- lighting: sun + ambient + specular + distance fog (GLSL ES 1.0 for WebGL) ----------
static const char* VS = R"(#version 100
attribute vec3 vertexPosition; attribute vec3 vertexNormal; attribute vec2 vertexTexCoord; attribute vec4 vertexColor;
uniform mat4 mvp; uniform mat4 matModel;
varying vec3 vN; varying vec3 vW; varying vec2 vUV; varying vec4 vC;
void main() { vC = vertexColor; vW = (matModel * vec4(vertexPosition, 1.0)).xyz; vN = normalize((matModel * vec4(vertexNormal, 0.0)).xyz); vUV = vertexTexCoord; gl_Position = mvp * vec4(vertexPosition, 1.0); })";
static const char* FS = R"(#version 100
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
varying vec3 vN; varying vec3 vW; varying vec2 vUV; varying vec4 vC;
uniform sampler2D texture0; uniform vec4 uvR;
uniform vec4 colDiffuse; uniform vec3 lightDir; uniform vec3 camPos; uniform vec3 fogColor; uniform float fogDensity; uniform float ambient;
uniform float pattern; uniform vec4 casters[24]; uniform float nCast; uniform float time; uniform vec3 sunCol; uniform vec3 skyCol;
uniform sampler2D shadowMap; uniform mat4 lightVP; uniform float shOn; uniform float shTex; uniform vec4 flashP; uniform vec3 flashC;
uniform vec4 plP[6]; uniform vec4 plC[6]; uniform float nPl; uniform float rough; uniform float camFillK;
float unpackD(vec4 c) { return dot(c, vec4(1.0, 1.0 / 255.0, 1.0 / 65025.0, 1.0 / 16581375.0)); }
float sunShadow(vec3 p, vec3 n, float nd) {
  if (shOn < 0.5) return 1.0;
  vec4 lp = lightVP * vec4(p + n * (0.035 + 0.075 * (1.0 - clamp(nd, 0.0, 1.0))), 1.0);
  vec3 q = lp.xyz * 0.5 + 0.5;
  if (q.x < 0.0 || q.x > 1.0 || q.y < 0.0 || q.y > 1.0 || q.z > 1.0) return 1.0;
  float bias = 0.0006 + 0.0022 * (1.0 - clamp(nd, 0.0, 1.0));
  float s = 0.0;
  for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) {
    vec2 o = vec2(float(x), float(y)) * shTex * 1.5;
    s += step(q.z - bias, unpackD(texture2D(shadowMap, q.xy + o)));
  }
  s /= 9.0;
  // fade out at the edge of the shadow map
  vec2 e = min(q.xy, 1.0 - q.xy); float edge = smoothstep(0.0, 0.06, min(e.x, e.y));
  return mix(1.0, s, edge);
}
float h21(vec2 p) { p = fract(p * vec2(123.34, 456.21)); p += dot(p, p + 45.32); return fract(p.x * p.y); }
float vn(vec2 p) { vec2 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);
  return mix(mix(h21(i), h21(i + vec2(1.0, 0.0)), f.x), mix(h21(i + vec2(0.0, 1.0)), h21(i + vec2(1.0, 1.0)), f.x), f.y); }
float fbm(vec2 p) { return vn(p) * 0.55 + vn(p * 2.03 + 7.1) * 0.3 + vn(p * 4.1 + 3.7) * 0.15; }
void main() {
  vec4 tx = texture2D(texture0, uvR.xy + vUV * uvR.zw);
  vec3 base = colDiffuse.rgb * vC.rgb * tx.rgb;  // per-vertex colour (white unless a merged mesh brings its own) x a printed texture (white unless Tex)
  vec3 n = normalize(vN); vec3 L = normalize(-lightDir); vec3 V = normalize(camPos - vW);
  float gl0 = 1.0 - rough; float spec = mix(0.03, 0.8, gl0 * gl0), gloss = mix(8.0, 150.0, gl0 * gl0);   // roughness: matte people, glossy lanes
  // ---- surface patterns ----
  if (pattern > 0.5) {
    vec2 q = vW.xz;
    if (pattern < 1.5) {        // grass: mowed stripes + clumps
      float st = step(0.5, fract(q.x * 0.12 + 0.02 * vn(q * 0.3)));
      base *= 0.86 + 0.1 * st + 0.18 * fbm(q * 0.9) - 0.08 * vn(q * 6.0);
      spec = 0.05;
    } else if (pattern < 2.5) { // wood planks with grain
      vec2 w = vW.xz + vec2(0.0, vW.y);
      float plank = floor(w.x * 0.9);
      float g = fract(w.y * 0.15 + h21(vec2(plank, 1.0)) * 4.0 + fbm(vec2(w.x * 6.0, w.y * 0.4)) * 0.8);
      base *= 0.82 + 0.16 * h21(vec2(plank, 2.0)) + 0.1 * smoothstep(0.3, 0.7, g) - 0.18 * (1.0 - smoothstep(0.0, 0.06, fract(w.x * 0.9)));
      spec = max(spec, 0.25); gloss = max(gloss, 60.0);
    } else if (pattern < 3.5) { // felt
      base *= 0.9 + 0.12 * vn(q * 30.0) + 0.06 * fbm(q * 2.0);
      spec = 0.02;
    } else if (pattern < 4.5) { // snow: soft drifts + glitter
      float dr = fbm(q * 0.22), fine = fbm(q * 1.7 + 3.1), grain = vn(q * 9.0);
      float hollow = smoothstep(0.62, 0.3, dr);
      base *= 0.8 + 0.2 * dr + 0.07 * (fine - 0.5) + 0.03 * (grain - 0.5);
      base = mix(base, base * vec3(0.72, 0.84, 1.02), hollow * 0.6);
      // wind ridges catch the sun
      float ridge = smoothstep(0.52, 0.6, fbm(vec2(q.x * 0.5 + q.y * 0.2, q.y * 1.6)));
      base += vec3(0.05, 0.05, 0.04) * ridge;
      float gl = step(0.975, h21(floor(q * 22.0))) * pow(max(dot(reflect(-L, n), V), 0.0), 5.0);
      base += gl * 1.4; spec = 0.12; gloss = 24.0;
    } else if (pattern < 5.5) { // ice: cracks + sheen
      float c = abs(vn(q * 1.3) - 0.5); base = mix(base, vec3(0.95, 0.98, 1.0), (1.0 - smoothstep(0.0, 0.03, c)) * 0.4);
      float deep = fbm(q * 0.3 + 4.0); base *= 0.78 + 0.26 * deep;
      base = mix(base, base * vec3(0.7, 0.86, 1.05), smoothstep(0.55, 0.25, deep) * 0.5);
      // skate scratches
      float sc1 = smoothstep(0.97, 1.0, vn(vec2(q.x * 14.0 + q.y * 3.0, q.y * 0.6))) + smoothstep(0.975, 1.0, vn(vec2(q.y * 12.0 - q.x * 4.0, q.x * 0.5)));
      base += vec3(0.1) * sc1; spec = 0.9; gloss = 110.0;
    } else if (pattern < 6.5) { // tiles
      vec2 t = fract(q * 0.5); float gr = smoothstep(0.0, 0.04, min(min(t.x, t.y), min(1.0 - t.x, 1.0 - t.y)));
      base *= (0.75 + 0.25 * gr) * (0.95 + 0.08 * h21(floor(q * 0.5))); spec = 0.5;
    } else if (pattern < 7.5) { // water: moving ripples
      float r = fbm(q * 0.5 + vec2(time * 0.25, time * 0.18)) + fbm(q * 0.9 - vec2(time * 0.2, -time * 0.1));
      n = normalize(n + vec3(r - 0.9, 0.0, vn(q * 0.7 + time * 0.3) - 0.5) * 0.5);
      base *= 0.85 + 0.25 * r; spec = 0.6; gloss = 80.0;
    } else if (pattern < 8.5) { // rug: woven rings
      float d = length(q - vec2(0.0, 0.5)); base *= 0.85 + 0.15 * step(0.5, fract(d * 0.6)) + 0.05 * vn(q * 25.0); spec = 0.02;
    } else if (pattern < 9.5) { // stone
      base *= 0.9 + 0.14 * fbm(q * 1.5 + vW.y); spec = 0.15;
    } else {                    // a bowling lane: 39 narrow maple/pine boards running down the lane under a clear coat
      float bd = floor(q.x * 37.0), bf = fract(q.x * 37.0);
      float grain = fbm(vec2(q.x * 60.0, q.y * 0.9 + h21(vec2(bd, 3.0)) * 9.0));
      base *= 0.86 + 0.12 * h21(vec2(bd, 5.0)) + 0.1 * (grain - 0.5) - 0.07 * (1.0 - smoothstep(0.0, 0.08, min(bf, 1.0 - bf)));
      base = mix(base, base * vec3(0.78, 0.6, 0.45), smoothstep(16.5, 18.5, q.y) * 0.0 + step(0.5, h21(vec2(floor(q.y * 0.25), bd))) * 0.06);
    }
  }
  // ---- light: warm sun with a soft cel band, hemisphere sky/ground fill, fresnel rim ----
  float nd = dot(n, L);
  float wrap = clamp((nd + 0.3) / 1.3, 0.0, 1.0);
  float diff = mix(wrap, smoothstep(0.32, 0.5, wrap), 0.35);
  // contact shadows from characters (soft blobs on upward faces near their feet)
  float sh = 1.0;
  for (int i = 0; i < 24; i++) {
    if (float(i) >= nCast) break;
    vec4 c = casters[i];
    float hgt = vW.y - c.y;
    if (n.y > 0.4 && hgt < 0.3 && hgt > -0.6) {
      vec2 off = vW.xz - (c.xz - L.xz * 0.12);
      float d = length(off) / c.w;
      sh *= 1.0 - 0.5 * (1.0 - smoothstep(0.2, 1.25, d));
      sh *= 1.0 - 0.45 * (1.0 - smoothstep(0.0, 0.55, d));
    }
  }
  float ssh = sunShadow(vW, n, nd);
  sh *= mix(0.16, 1.0, ssh);
  // cheap ambient occlusion: creases at the ground and under things get darker
  float ao = mix(0.72, 1.0, smoothstep(-0.6, 0.5, n.y)) * mix(0.85, 1.0, ssh);
  vec3 hemi = mix(vec3(0.5, 0.44, 0.4) * ambient, skyCol * (ambient + 0.12), n.y * 0.5 + 0.5) * ao;
  vec3 Hh = normalize(V + L);
  float s = pow(max(dot(n, Hh), 0.0), gloss) * spec * step(0.0, nd);
  float fres = pow(1.0 - max(dot(n, V), 0.0), 3.0);
  // key (sun) + sky fill + a soft fill light from the camera so faces and fronts are never in the dark
  float camFill = max(dot(n, V), 0.0) * camFillK;
  vec3 shadeTint = mix(vec3(0.62, 0.74, 1.08), vec3(1.0), sh);   // shadows go cool, never grey
  float rimW = pow(1.0 - max(dot(n, V), 0.0), 2.6) * (1.0 - abs(n.y)) * smoothstep(-0.3, 0.4, dot(normalize(vec3(L.x, 0.0, L.z) * -1.0 + V * 0.2), n) + 0.5);
  vec3 col = base * (hemi * mix(0.7, 1.0, sh) * shadeTint + sunCol * 1.35 * diff * sh + vec3(camFill)) + sunCol * s * sh * ssh + skyCol * fres * 0.22 + vec3(1.0, 0.82, 0.62) * rimW * 0.42;
  // lamps, spotlight pools and neon: warm and cool light that falls off with distance (what gives a room its mood)
  vec3 loc = vec3(0.0), locS = vec3(0.0);
  float shiny = smoothstep(0.55, 0.95, gl0) * step(0.5, n.y);   // only polished floors mirror the lamps
  float F = mix(0.12, 1.0, pow(1.0 - max(dot(n, V), 0.0), 4.0));
  vec3 Rr = reflect(-V, n);
  for (int i = 0; i < 6; i++) {
    if (float(i) >= nPl) break;
    vec3 dl = plP[i].xyz - vW; float dd = length(dl); vec3 l = dl / max(dd, 0.001); float r = plP[i].w;
    float att = 1.0 / (1.0 + dd * dd / (r * r)); att *= 1.0 - smoothstep(r * 2.0, r * 3.2, dd);
    float ndl = dot(n, l); float w = clamp((ndl + 0.2) / 1.2, 0.0, 1.0);
    loc += plC[i].rgb * att * w * ao;
    locS += plC[i].rgb * att * pow(max(dot(n, normalize(V + l)), 0.0), gloss) * spec * step(0.0, ndl);
    if (shiny > 0.0) {   // the lamp itself, seen in the floor: a bright blob that stretches into a streak toward you
      float al = dot(dl, Rr);
      if (al > 0.0) { float pp = length(dl - Rr * al), sz = 0.22 + al * (0.03 + rough * 0.35);
        locS += plC[i].rgb * (1.0 - smoothstep(0.0, sz, pp)) * (0.35 + 0.65 * F) * shiny * 0.55; }
    }
  }
  col = mix(col, skyCol * 0.18, F * shiny * 0.55);   // the floor mirrors the dark room above it: deeper, richer colour
  col += base * loc + locS;
  // an explosion / goal / strike flash lights up everything around it
  if (flashC.r + flashC.g + flashC.b > 0.001) { vec3 fl = flashP.xyz - vW; float fd = length(fl); col += base * flashC * (max(dot(n, fl / max(fd, 0.001)), 0.0) * 0.75 + 0.25) / (1.0 + fd * fd / (flashP.w * flashP.w)); }
  // ACES filmic curve: bright surfaces roll off instead of clipping to pure white
  col *= 0.6;
  col = clamp((col * (2.51 * col + 0.03)) / (col * (2.43 * col + 0.59) + 0.14), 0.0, 1.0);
  float dist = length(camPos - vW); float f = 1.0 - exp(-pow(dist * fogDensity, 2.0));
  f = max(f, (1.0 - exp(-dist * 0.006)) * exp(-max(vW.y + 3.0, 0.0) * 0.12) * 0.32);   // low-lying haze adds depth
  gl_FragColor = vec4(mix(col, fogColor, clamp(f, 0.0, 1.0)), colDiffuse.a * tx.a);
})";
static const char* SH_VS = R"(#version 100
attribute vec3 vertexPosition; uniform mat4 mvp; varying float vD;
void main() { gl_Position = mvp * vec4(vertexPosition, 1.0); vD = gl_Position.z * 0.5 + 0.5; })";
static const char* SH_FS = R"(#version 100
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
varying float vD;
void main() { float d = clamp(vD, 0.0, 0.99999); vec4 e = fract(d * vec4(1.0, 255.0, 65025.0, 16581375.0)); e -= e.yzww * vec4(1.0 / 255.0, 1.0 / 255.0, 1.0 / 255.0, 0.0); gl_FragColor = e; })";
static const int SHADOW_RES = 2048;
Lit lit; Prims prim; static int locFlP = -1, locFlC = -1;
void Lit::Init() {
  shader = LoadShaderFromMemory(VS, FS);
  shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");
  locLight = GetShaderLocation(shader, "lightDir"); locCam = GetShaderLocation(shader, "camPos"); locFog = GetShaderLocation(shader, "fogColor"); locFogD = GetShaderLocation(shader, "fogDensity"); locAmb = GetShaderLocation(shader, "ambient");
  locPat = GetShaderLocation(shader, "pattern"); locCast = GetShaderLocation(shader, "casters"); locNCast = GetShaderLocation(shader, "nCast"); locTime = GetShaderLocation(shader, "time");
  locSun = GetShaderLocation(shader, "sunCol"); locSky = GetShaderLocation(shader, "skyCol");
  locPlP = GetShaderLocation(shader, "plP"); locPlC = GetShaderLocation(shader, "plC"); locNPl = GetShaderLocation(shader, "nPl"); locRough = GetShaderLocation(shader, "rough"); locCamFill = GetShaderLocation(shader, "camFillK");
  mat = LoadMaterialDefault(); mat.shader = shader;
  locFlP = GetShaderLocation(shader, "flashP"); locFlC = GetShaderLocation(shader, "flashC");
  locUV = GetShaderLocation(shader, "uvR"); NoTex();
  locLVP = GetShaderLocation(shader, "lightVP"); locShOn = GetShaderLocation(shader, "shOn"); locShTex = GetShaderLocation(shader, "shTex");
  shSh = LoadShaderFromMemory(SH_VS, SH_FS); shMat = LoadMaterialDefault(); shMat.shader = shSh;
  shRT = LoadRenderTexture(SHADOW_RES, SHADOW_RES);
  SetTextureFilter(shRT.texture, TEXTURE_FILTER_POINT); SetTextureWrap(shRT.texture, TEXTURE_WRAP_CLAMP);
  shader.locs[SHADER_LOC_MAP_NORMAL] = GetShaderLocation(shader, "shadowMap");
  mat.maps[MATERIAL_MAP_NORMAL].texture = shRT.texture;
}
extern int g_post;
// render last frame's opaque draws from the sun into the shadow map (before this frame draws anything)
void Lit::ShadowPass() {
  shOn = 0;
  if (rec.empty() || !haveCam || !shRT.id || !g_post) { rec.clear(); haveCam = false; return; }
  Vector3 L = Vector3Normalize(sunDir);
  // cover what the camera looks at: centre on the ground point it aims at, sized by how far away it is
  Vector3 c = lastCam.target; Vector3 fwd = Vector3Subtract(lastCam.target, lastCam.position);
  float dist = Vector3Length(fwd);
  float R = Clamp(dist * 0.85f, 6.0f, 34.0f); R = ceilf(R);
  float tx = 2 * R / SHADOW_RES; c.x = roundf(c.x / tx) * tx; c.z = roundf(c.z / tx) * tx; c.y = roundf(c.y / tx) * tx;
  Matrix view = MatrixLookAt(Vector3Subtract(c, Vector3Scale(L, 60)), c, { 0, 0, -1 });
  Matrix proj = MatrixOrtho(-R, R, -R, R, 1, 140);
  BeginTextureMode(shRT); ClearBackground(WHITE);
  rlDrawRenderBatchActive();
  rlMatrixMode(RL_PROJECTION); rlLoadIdentity(); rlMultMatrixf(MatrixToFloat(proj));
  rlMatrixMode(RL_MODELVIEW); rlLoadIdentity(); rlMultMatrixf(MatrixToFloat(view));
  rlEnableDepthTest(); rlEnableDepthMask(); rlDisableBackfaceCulling(); rlDisableColorBlend();
  for (auto& r : rec) DrawMesh(r.m, shMat, r.t);
  rlEnableColorBlend();
  EndTextureMode();
  lightVP = MatrixMultiply(view, proj); shOn = 1;
  rec.clear(); haveCam = false;
}
void Lit::Begin(const Camera3D& cam, Vector3 fogColor, float fogDensity) {
  NoTex(); sunDir = { -0.45f, -1.0f, -0.35f }; sunCol = { 1.0f, 0.92f, 0.78f }; amb = 0.4f; camFill = 0.08f; roughV = 0.5f; plP.clear(); plC.clear();   // the defaults; a scene changes them with Mood / Light right after
  Vector3 L = Vector3Normalize(sunDir);
  // sky fill tinted by the scene's fog colour; dark scenes (night, rooms) get a cooler, dimmer fill
  float lum = fogColor.x * 0.3f + fogColor.y * 0.55f + fogColor.z * 0.15f;
  Vector3 sky = Vector3Lerp({ 0.75f, 0.82f, 1.0f }, fogColor, 0.35f);
  if (lum < 0.3f) sky = Vector3Lerp({ 0.78f, 0.8f, 0.95f }, fogColor, 0.15f);  // indoor / night scenes still get a bright fill
  Vector3 sun = sunCol;
  Vector3 eye = CineCam(cam).position;   // the real (cinematic) eye, for highlights and fog
  SetShaderValue(shader, locLight, &L, SHADER_UNIFORM_VEC3); SetShaderValue(shader, locCam, &eye, SHADER_UNIFORM_VEC3);
  { Vector4 fp; Vector3 fc; cine::Light(fp, fc); SetShaderValue(shader, locFlP, &fp, SHADER_UNIFORM_VEC4); SetShaderValue(shader, locFlC, &fc, SHADER_UNIFORM_VEC3); }
  SetShaderValue(shader, locFog, &fogColor, SHADER_UNIFORM_VEC3); SetShaderValue(shader, locFogD, &fogDensity, SHADER_UNIFORM_FLOAT); SetShaderValue(shader, locAmb, &amb, SHADER_UNIFORM_FLOAT);
  SetShaderValue(shader, locSun, &sun, SHADER_UNIFORM_VEC3); SetShaderValue(shader, locSky, &sky, SHADER_UNIFORM_VEC3); SetShaderValue(shader, locTime, &time, SHADER_UNIFORM_FLOAT);
  { float z = 0; SetShaderValue(shader, locNPl, &z, SHADER_UNIFORM_FLOAT); SetShaderValue(shader, locRough, &roughV, SHADER_UNIFORM_FLOAT); SetShaderValue(shader, locCamFill, &camFill, SHADER_UNIFORM_FLOAT); }
  float nc = (float)castPrev.size(); SetShaderValue(shader, locNCast, &nc, SHADER_UNIFORM_FLOAT);
  if (!castPrev.empty()) SetShaderValueV(shader, locCast, castPrev.data(), SHADER_UNIFORM_VEC4, (int)castPrev.size());
  if (!haveCam) { lastCam = cam; haveCam = true; }
  SetShaderValue(shader, locShOn, &shOn, SHADER_UNIFORM_FLOAT);
  float tex = 1.f / SHADOW_RES; SetShaderValue(shader, locShTex, &tex, SHADER_UNIFORM_FLOAT);
  SetShaderValueMatrix(shader, locLVP, lightVP);
  Pattern(PAT_NONE);
}
void Lit::Pattern(int p) { float f = (float)p; SetShaderValue(shader, locPat, &f, SHADER_UNIFORM_FLOAT); }
void Lit::Mood(float ambient, Vector3 c, Vector3 dir, float cf) {
  amb = ambient; sunCol = c; sunDir = dir; camFill = cf; Vector3 L = Vector3Normalize(dir);
  SetShaderValue(shader, locAmb, &amb, SHADER_UNIFORM_FLOAT); SetShaderValue(shader, locSun, &sunCol, SHADER_UNIFORM_VEC3); SetShaderValue(shader, locLight, &L, SHADER_UNIFORM_VEC3); SetShaderValue(shader, locCamFill, &camFill, SHADER_UNIFORM_FLOAT);
}
void Lit::Sky(Vector3 c) { SetShaderValue(shader, locSky, &c, SHADER_UNIFORM_VEC3); }
void Lit::Light(Vector3 p, Vector3 c, float r) {
  if (plP.size() >= 6) return; plP.push_back({ p.x, p.y, p.z, r }); plC.push_back({ c.x, c.y, c.z, 1 });
  float n = (float)plP.size(); SetShaderValue(shader, locNPl, &n, SHADER_UNIFORM_FLOAT);
  SetShaderValueV(shader, locPlP, plP.data(), SHADER_UNIFORM_VEC4, (int)plP.size()); SetShaderValueV(shader, locPlC, plC.data(), SHADER_UNIFORM_VEC4, (int)plC.size());
}
void Lit::Tex(Texture2D t, Vector4 r) { mat.maps[MATERIAL_MAP_DIFFUSE].texture = t; uvR = r; SetShaderValue(shader, locUV, &uvR, SHADER_UNIFORM_VEC4); }
void Lit::NoTex() { mat.maps[MATERIAL_MAP_DIFFUSE].texture = Texture2D{ rlGetTextureIdDefault(), 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 }; uvR = { 0, 0, 1, 1 }; SetShaderValue(shader, locUV, &uvR, SHADER_UNIFORM_VEC4); }
void Lit::Rough(float r) { if (r == roughV) return; rlDrawRenderBatchActive(); roughV = r; SetShaderValue(shader, locRough, &roughV, SHADER_UNIFORM_FLOAT); }
void Lit::MirrorBegin(float y, float a) { rlDrawRenderBatchActive(); mirror = true; mirrorY = y; mirrorA = a; rlDisableDepthTest(); rlDisableDepthMask(); rlSetCullFace(RL_CULL_FACE_FRONT); }
void Lit::MirrorEnd() { rlDrawRenderBatchActive(); mirror = false; rlEnableDepthTest(); rlEnableDepthMask(); rlSetCullFace(RL_CULL_FACE_BACK); }
void Lit::Draw(const Mesh& m, Matrix t, Color c) {
  if (mirror) {   // the reflection: flipped about the floor, faded, never casting a shadow
    t = MatrixMultiply(t, MatrixMultiply(MatrixMultiply(MatrixTranslate(0, -mirrorY, 0), MatrixScale(1, -1, 1)), MatrixTranslate(0, mirrorY, 0)));
    c.a = (unsigned char)(c.a * mirrorA);
    mat.maps[MATERIAL_MAP_DIFFUSE].color = c; rlEnableBackfaceCulling(); DrawMesh(m, mat, t); rlDisableBackfaceCulling(); return;
  }
  mat.maps[MATERIAL_MAP_DIFFUSE].color = c; rlDisableBackfaceCulling(); DrawMesh(m, mat, t);
  // opaque, not-huge things cast sun shadows (floors, walls and sky domes only receive them)
  if (!noRec && c.a == 255 && rec.size() < 6000) {
    float sx = t.m0 * t.m0 + t.m1 * t.m1 + t.m2 * t.m2, sy = t.m4 * t.m4 + t.m5 * t.m5 + t.m6 * t.m6, sz = t.m8 * t.m8 + t.m9 * t.m9 + t.m10 * t.m10;
    if (fmaxf(sx, fmaxf(sy, sz)) < 30 * 30) rec.push_back({ m, t });
  }
}
// a unit cube (-0.5..0.5) whose edges and corners are rounded with radius r: each face is a grid that is dense in the
// rounded band, and every grid point is pushed out from the inner box onto the rounded surface
static Mesh GenRoundBox(float r, int m) {
  const float k = r * 2;   // in the -1..1 cube
  std::vector<float> g; for (int i = 0; i < m; i++) g.push_back(-1 + k * i / m); g.push_back(-1 + k); g.push_back(0); g.push_back(1 - k); for (int i = 1; i <= m; i++) g.push_back(1 - k + k * i / m);
  const int n = (int)g.size();
  const Vector3 FN[6][3] = { { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } }, { { -1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 } }, { { 0, 1, 0 }, { 0, 0, 1 }, { 1, 0, 0 } },
                             { { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, 1 } }, { { 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 } }, { { 0, 0, -1 }, { 0, 1, 0 }, { 1, 0, 0 } } };
  std::vector<float> v, nn, tc; std::vector<unsigned short> id;
  for (int f = 0; f < 6; f++) {
    unsigned short base = (unsigned short)(v.size() / 3);
    for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) {
      Vector3 p = Vector3Add(FN[f][0], Vector3Add(Vector3Scale(FN[f][1], g[i]), Vector3Scale(FN[f][2], g[j])));
      Vector3 in = { Clamp(p.x, -1 + k, 1 - k), Clamp(p.y, -1 + k, 1 - k), Clamp(p.z, -1 + k, 1 - k) }, d = Vector3Subtract(p, in);
      Vector3 nr = Vector3Length(d) > 1e-5f ? Vector3Normalize(d) : FN[f][0], q = Vector3Scale(Vector3Add(in, Vector3Scale(nr, k)), 0.5f);
      v.insert(v.end(), { q.x, q.y, q.z }); nn.insert(nn.end(), { nr.x, nr.y, nr.z }); tc.insert(tc.end(), { (g[i] + 1) / 2, (g[j] + 1) / 2 });
    }
    for (int i = 0; i + 1 < n; i++) for (int j = 0; j + 1 < n; j++) {
      unsigned short a = base + i * n + j, b = base + (i + 1) * n + j, c = base + (i + 1) * n + j + 1, e = base + i * n + j + 1;
      id.insert(id.end(), { a, b, c, a, c, e });
    }
  }
  Mesh ms{}; ms.vertexCount = (int)v.size() / 3; ms.triangleCount = (int)id.size() / 3;
  ms.vertices = (float*)MemAlloc((unsigned)(v.size() * sizeof(float))); memcpy(ms.vertices, v.data(), v.size() * sizeof(float));
  ms.normals = (float*)MemAlloc((unsigned)(nn.size() * sizeof(float))); memcpy(ms.normals, nn.data(), nn.size() * sizeof(float));
  ms.texcoords = (float*)MemAlloc((unsigned)(tc.size() * sizeof(float))); memcpy(ms.texcoords, tc.data(), tc.size() * sizeof(float));
  ms.indices = (unsigned short*)MemAlloc((unsigned)(id.size() * sizeof(unsigned short))); memcpy(ms.indices, id.data(), id.size() * sizeof(unsigned short));
  UploadMesh(&ms, false); return ms;
}
// smooth round shapes (raylib's are 24-sided and flat-shaded: every round table showed its facets).
// Same layout as GenMeshCylinder / GenMeshCone: radius 0.5, base at y = 0, top (or tip) at y = 1.
static Mesh GenRound(int seg, bool cone) {
  std::vector<float> v, nn, tc; std::vector<unsigned short> id;
  auto V = [&](float x, float y, float z, float a, float b, float c, float u, float w) { v.insert(v.end(), { x, y, z }); nn.insert(nn.end(), { a, b, c }); tc.insert(tc.end(), { u, w }); return (unsigned short)(v.size() / 3 - 1); };
  const float sl = cone ? 0.5f : 0.f;   // a cone's side normal leans up: radius 0.5 over height 1
  for (int i = 0; i <= seg; i++) {
    float a = 2 * PI * i / seg, c = cosf(a), s = sinf(a); Vector3 n = Vector3Normalize({ c, sl, s });
    V(c * 0.5f, 0, s * 0.5f, n.x, n.y, n.z, (float)i / seg, 1); V(cone ? 0 : c * 0.5f, 1, cone ? 0 : s * 0.5f, n.x, n.y, n.z, (float)i / seg, 0);
  }
  for (int i = 0; i < seg; i++) { unsigned short a = i * 2, b = a + 1, c = a + 2, d = a + 3; id.insert(id.end(), { a, b, c, c, b, d }); }
  for (int cap = 0; cap < (cone ? 1 : 2); cap++) {
    float y = (float)cap, ny = cap ? 1.f : -1.f; unsigned short ctr = V(0, y, 0, 0, ny, 0, 0.5f, 0.5f), first = (unsigned short)(v.size() / 3);
    for (int i = 0; i <= seg; i++) { float a = 2 * PI * i / seg; V(cosf(a) * 0.5f, y, sinf(a) * 0.5f, 0, ny, 0, 0.5f + cosf(a) * 0.5f, 0.5f + sinf(a) * 0.5f); }
    for (int i = 0; i < seg; i++) { unsigned short p = first + i, q = first + i + 1; if (cap) id.insert(id.end(), { ctr, q, p }); else id.insert(id.end(), { ctr, p, q }); }
  }
  Mesh ms{}; ms.vertexCount = (int)v.size() / 3; ms.triangleCount = (int)id.size() / 3;
  ms.vertices = (float*)MemAlloc((unsigned)(v.size() * sizeof(float))); memcpy(ms.vertices, v.data(), v.size() * sizeof(float));
  ms.normals = (float*)MemAlloc((unsigned)(nn.size() * sizeof(float))); memcpy(ms.normals, nn.data(), nn.size() * sizeof(float));
  ms.texcoords = (float*)MemAlloc((unsigned)(tc.size() * sizeof(float))); memcpy(ms.texcoords, tc.data(), tc.size() * sizeof(float));
  ms.indices = (unsigned short*)MemAlloc((unsigned)(id.size() * sizeof(unsigned short))); memcpy(ms.indices, id.data(), id.size() * sizeof(unsigned short));
  UploadMesh(&ms, false); return ms;
}
void Prims::Init() {
  cube = GenMeshCube(1, 1, 1); sphere = GenMeshSphere(0.5f, 22, 30); cyl = GenRound(72, false); cone = GenRound(48, true); plane = GenMeshPlane(1, 1, 1, 1);
  torus = GenMeshTorus(0.25f, 1, 16, 24); hemi = GenMeshHemiSphere(0.5f, 16, 24); rbox = GenRoundBox(0.12f, 4); rcard = GenRoundBox(0.06f, 3);
}
void Limb(Vector3 a, Vector3 b, float r, Color c) {
  Vector3 d = Vector3Subtract(b, a); float len = Vector3Length(d); if (len < 1e-4f) return;
  Vector3 u = Vector3Scale(d, 1 / len), y = { 0, 1, 0 };
  Vector3 ax = Vector3CrossProduct(y, u); float s = Vector3Length(ax), ang = acosf(Clamp(Vector3DotProduct(y, u), -1, 1));
  Matrix R = s < 1e-4f ? (u.y > 0 ? MatrixIdentity() : MatrixRotateX(PI)) : MatrixRotate(Vector3Scale(ax, 1 / s), ang);
  lit.Draw(prim.cyl, MatrixMultiply(MatrixMultiply(MatrixScale(r * 2, len, r * 2), R), MatrixTranslate(a.x, a.y, a.z)), c);
  lit.Draw(prim.sphere, MatrixMultiply(MatrixScale(r * 2, r * 2, r * 2), MatrixTranslate(b.x, b.y, b.z)), c);
  lit.Draw(prim.sphere, MatrixMultiply(MatrixScale(r * 2, r * 2, r * 2), MatrixTranslate(a.x, a.y, a.z)), c);
}

// ---------- post processing: bloom (1/4 res), FXAA-lite, colour grade, vignette ----------
static const char* PVS = R"(#version 100
attribute vec3 vertexPosition; attribute vec2 vertexTexCoord; attribute vec4 vertexColor;
uniform mat4 mvp; varying vec2 uv;
void main() { uv = vertexTexCoord; gl_Position = mvp * vec4(vertexPosition, 1.0); })";
static const char* BRIGHT_FS = R"(#version 100
precision mediump float; varying vec2 uv; uniform sampler2D texture0; uniform vec2 px;
void main() {
  vec3 c = vec3(0.0);
  c += texture2D(texture0, uv + px * vec2(-1.0, -1.0)).rgb; c += texture2D(texture0, uv + px * vec2(1.0, -1.0)).rgb;
  c += texture2D(texture0, uv + px * vec2(-1.0, 1.0)).rgb; c += texture2D(texture0, uv + px * vec2(1.0, 1.0)).rgb;
  c *= 0.25; float l = dot(c, vec3(0.3, 0.59, 0.11));
  float sat = max(c.r, max(c.g, c.b)) - min(c.r, min(c.g, c.b));
  gl_FragColor = vec4(c * smoothstep(0.86, 1.0, l + sat * 0.2), 1.0);
})";
static const char* BLUR_FS = R"(#version 100
precision mediump float; varying vec2 uv; uniform sampler2D texture0; uniform vec2 dir;
void main() {
  vec3 c = texture2D(texture0, uv).rgb * 0.227;
  c += (texture2D(texture0, uv + dir * 1.38).rgb + texture2D(texture0, uv - dir * 1.38).rgb) * 0.316;
  c += (texture2D(texture0, uv + dir * 3.23).rgb + texture2D(texture0, uv - dir * 3.23).rgb) * 0.07;
  gl_FragColor = vec4(c, 1.0);
})";
static const char* FINAL_FS = R"(#version 100
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
varying vec2 uv; uniform sampler2D texture0; uniform sampler2D bloom; uniform sampler2D dofTex; uniform sampler2D depthTex;
uniform vec2 px; uniform float time; uniform float focus; uniform float dofOn; uniform float dofAmt; uniform sampler2D aoTex; uniform float aoOn;
uniform vec4 fxA; uniform vec4 fxB; uniform vec3 fxC;   // flash, colour split, zoom blur, speed lines | letterbox, impact frame, centre
vec3 S(vec2 p) { return texture2D(texture0, p).rgb; }
float linZ(float d) { float n = 0.3, f = 600.0; float z = d * 2.0 - 1.0; return 2.0 * n * f / (f + n - z * (f - n)); }
void main() {
  vec3 cM = S(uv), cN = S(uv + vec2(0.0, px.y)), cS = S(uv - vec2(0.0, px.y)), cE = S(uv + vec2(px.x, 0.0)), cW = S(uv - vec2(px.x, 0.0));
  vec3 lw = vec3(0.3, 0.59, 0.11);
  float lM = dot(cM, lw), lN = dot(cN, lw), lS = dot(cS, lw), lE = dot(cE, lw), lW = dot(cW, lw);
  float mn = min(lM, min(min(lN, lS), min(lE, lW))), mx = max(lM, max(max(lN, lS), max(lE, lW)));
  vec3 c = cM;
  if (mx - mn > max(0.06, mx * 0.12)) {
    vec2 d = normalize(vec2(-(lN - lS), lE - lW) + 1e-5) * px;
    c = (S(uv + d * 0.5) + S(uv - d * 0.5) + cM * 2.0) * 0.25;
  } else {
    vec3 nb = (cN + cS + cE + cW) * 0.25; float amt = 0.35 * (1.0 - smoothstep(0.02, 0.25, mx - mn));
    c = clamp(cM + (cM - nb) * amt, min(min(cN, cS), min(cE, cW)), max(max(cN, cS), max(cE, cW)));
  }
  // lens: a touch of colour fringing towards the corners
  vec2 dc = uv - 0.5; float ca = clamp(dot(dc, dc) * 3.2, 0.0, 1.0);
  c.r = mix(c.r, S(uv - dc * 0.0018).r, ca); c.b = mix(c.b, S(uv + dc * 0.0018).b, ca);   // (a strong fringe made everything look blurry)
  if (aoOn > 0.5) c *= mix(1.0, texture2D(aoTex, uv).r, 0.85);   // ambient occlusion: dark contact creases where things meet
  // depth of field: in focus around what the camera looks at, soft in front and behind (flat UI is never blurred)
  if (dofOn > 0.5) {
    float d = texture2D(depthTex, uv).r, coc = 0.0;
    if (d > 0.00002) { float z = linZ(d); coc = clamp((abs(z - focus) - focus * 0.55) / (focus * 1.8), 0.0, 1.0); }
    c = mix(c, texture2D(dofTex, uv).rgb, coc * dofAmt);
  }
  // cinematic moment: radial zoom blur with a colour split, out from what just happened
  vec2 ctr = fxB.zw;
  if (fxA.z > 0.001 || fxA.y > 0.001) {
    vec2 dv = (uv - ctr) * fxA.z * 0.16, ab = (uv - ctr) * fxA.y * 0.022; vec3 acc = vec3(0.0);
    for (int i = 0; i < 8; i++) { vec2 p = uv - dv * (float(i) / 7.0); acc += vec3(S(p + ab).r, S(p).g, S(p - ab).b); }
    c = mix(c, acc / 8.0, clamp(fxA.z * 2.0 + fxA.y, 0.0, 1.0));
  }
  c += texture2D(bloom, uv).rgb * (0.5 + fxA.x * 0.8);
  if (fxA.w > 0.001) {   // anime speed lines rushing out of the centre
    vec2 d = (uv - ctr) * vec2(px.y / px.x, 1.0); float r = length(d), a = atan(d.y, d.x);
    float s = (a + 3.14159) / 6.28318 * 110.0, k = floor(s), f = fract(s);
    float h = fract(sin((k + floor(time * 18.0) * 17.0) * 91.73) * 43758.55);
    float ln = step(0.6, h) * (1.0 - smoothstep(0.0, 0.08 + 0.3 * h, abs(f - 0.5))) * smoothstep(0.28 + 0.3 * h, 0.95, r);
    c = mix(c, vec3(1.0), ln * fxA.w * 0.75);
  }
  // film grade: gentle S-curve, rich colour, teal shadows / warm highlights
  c = mix(c, c * c * (3.0 - 2.0 * c), 0.42);
  float l = dot(c, lw); c = mix(vec3(l), c, 1.18);
  c += vec3(0.03, 0.01, -0.022) * smoothstep(0.45, 1.0, l) + vec3(-0.02, 0.006, 0.034) * (1.0 - smoothstep(0.0, 0.42, l));
  c *= 1.0 - dot(dc, dc) * 0.55;   // vignette
  float g = fract(sin(dot(floor(uv / px) + floor(time * 24.0) * 17.0, vec2(12.9898, 78.233))) * 43758.5453);
  c += (g - 0.5) * 0.04 * (1.0 - l * 0.6);   // film grain
  c = 1.0 - (1.0 - clamp(c, 0.0, 1.0)) * (1.0 - fxC * fxA.x * 0.7);   // flash
  if (fxB.y > 0.001) {   // impact frame: two frames of stark black / hot colour
    float l2 = dot(c, lw); vec3 imp = mix(fxC * 1.15, vec3(0.03, 0.0, 0.05), smoothstep(0.3, 0.52, l2));
    c = mix(c, imp, fxB.y);
  }
  float bh = fxB.x * 0.118; c *= smoothstep(bh, bh + 0.004, uv.y) * smoothstep(bh, bh + 0.004, 1.0 - uv.y);   // letterbox
  gl_FragColor = vec4(clamp(c, 0.0, 1.0), 1.0);
})";
// ambient occlusion: how buried each pixel is in the depth around it (under the cards, where a foot meets the floor, in the corners of a room)
static const char* AO_FS = R"(#version 100
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
varying vec2 uv; uniform sampler2D texture0; uniform vec2 px; uniform float aspect;
float linZ(float d) { float n = 0.3, f = 600.0; float z = d * 2.0 - 1.0; return 2.0 * n * f / (f + n - z * (f - n)); }
void main() {
  float d0 = texture2D(texture0, uv).r;
  if (d0 < 0.00002 || d0 > 0.99999) { gl_FragColor = vec4(1.0); return; }
  // pairs of samples on opposite sides: on a flat surface (a table, a card, a floor seen at a slant) the two average out to the
  // centre (1/z is linear across a plane), so only real creases and contacts - where the centre sits behind its neighbours - darken
  float z0 = linZ(d0), rad = clamp(0.3 / z0, 0.003, 0.035), occ = 0.0;
  float jit = fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453) * 6.283;
  for (int i = 0; i < 6; i++) {
    float a = float(i) * 1.0472 * 0.5 + float(i / 2) * 1.0 + jit, r = rad * (0.3 + 0.7 * fract(float(i) * 0.618 + 0.13));
    vec2 o = vec2(cos(a) / aspect, sin(a)) * r;
    float z1 = linZ(texture2D(texture0, uv + o).r), z2 = linZ(texture2D(texture0, uv - o).r);
    float zE = 2.0 / (1.0 / z1 + 1.0 / z2), dz = z0 - zE, near = z0 - min(z1, z2);
    occ += smoothstep(0.01 * z0, 0.04 * z0, dz) * (1.0 - smoothstep(0.35, 0.9, near));
  }
  gl_FragColor = vec4(vec3(clamp(1.0 - occ / 6.0 * 0.9, 0.0, 1.0)), 1.0);
})";
static Shader shAO; static int lAOPx = -1, lAOAsp = -1, lFinalAO = -1, lFinalAOOn = -1; static RenderTexture2D rtF, rtG;
static int lFxA = -1, lFxB = -1, lFxC = -1;
static RenderTexture2D rtMain, rtA, rtB, rtC, rtD, rtE; static Shader shBright, shBlur, shFinal; static int lBrightPx, lBlurDir, lFinalPx, lFinalBloom, lFinalDof, lFinalDepth, lFinalTime, lFinalFocus, lFinalDofOn; static bool postOk = false, dofOk = false;
float g_frameDt = 1 / 60.f; int g_racketAxis = 5;   // the racket handle runs along the thumb side of the fist (picked from a debug comparison)
float g_cineK = 1; float g_emoteT[8] = {}; int g_emoteK[8] = {}; Vector3 g_emotePos[8] = {}; bool g_emoteVis[8] = {};
float g_cineT = 0, g_focus = 15, g_dofAmt = 0.f; bool g_cineOn = false; static int lFinalDofAmt; extern int g_aoOn;
// the main scene target keeps its depth in a texture, so the final pass can focus by distance
static RenderTexture2D LoadSceneTarget(int w, int h) {
  RenderTexture2D t{}; t.id = rlLoadFramebuffer(); if (!t.id) return LoadRenderTexture(w, h);
  rlEnableFramebuffer(t.id);
  t.texture.id = rlLoadTexture(NULL, w, h, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1); t.texture.width = w; t.texture.height = h; t.texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8; t.texture.mipmaps = 1;
  t.depth.id = rlLoadTextureDepth(w, h, false); t.depth.width = w; t.depth.height = h; t.depth.format = 19; t.depth.mipmaps = 1;
  rlFramebufferAttach(t.id, t.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
  rlFramebufferAttach(t.id, t.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
  bool ok = rlFramebufferComplete(t.id); rlDisableFramebuffer();
  if (!ok) { rlUnloadFramebuffer(t.id); dofOk = false; return LoadRenderTexture(w, h); }
  dofOk = true; return t;
}
float g_rs = 1; static int FW = W, FH = H;
static void LoadPostShaders();
static void PostInit() {
  if (rtMain.id) { UnloadRenderTexture(rtMain); UnloadRenderTexture(rtA); UnloadRenderTexture(rtB); UnloadRenderTexture(rtC); UnloadRenderTexture(rtD); UnloadRenderTexture(rtE); }
  rtMain = LoadSceneTarget(FW, FH); rtA = LoadRenderTexture(FW / 4, FH / 4); rtB = LoadRenderTexture(FW / 4, FH / 4);
  rtC = LoadRenderTexture(FW / 4, FH / 4); rtD = LoadRenderTexture(FW / 4, FH / 4); rtE = LoadRenderTexture(FW / 4, FH / 4);
  if (rtF.id) { UnloadRenderTexture(rtF); UnloadRenderTexture(rtG); } rtF = LoadRenderTexture(FW / 2, FH / 2); rtG = LoadRenderTexture(FW / 2, FH / 2);
  SetTextureFilter(rtF.texture, TEXTURE_FILTER_BILINEAR); SetTextureFilter(rtG.texture, TEXTURE_FILTER_BILINEAR);
  for (auto* r : { &rtMain, &rtA, &rtB, &rtC, &rtD, &rtE }) SetTextureFilter(r->texture, TEXTURE_FILTER_BILINEAR);
  if (!shFinal.id) LoadPostShaders();
  if (!shFinal.id || shFinal.id == rlGetShaderIdDefault()) postOk = false; else postOk = rtMain.id > 0;
}
static void LoadPostShaders() {
  shBright = LoadShaderFromMemory(PVS, BRIGHT_FS); shBlur = LoadShaderFromMemory(PVS, BLUR_FS); shFinal = LoadShaderFromMemory(PVS, FINAL_FS);
  lBrightPx = GetShaderLocation(shBright, "px"); lBlurDir = GetShaderLocation(shBlur, "dir"); lFinalPx = GetShaderLocation(shFinal, "px"); lFinalBloom = GetShaderLocation(shFinal, "bloom");
  lFinalDof = GetShaderLocation(shFinal, "dofTex"); lFinalDepth = GetShaderLocation(shFinal, "depthTex"); lFinalTime = GetShaderLocation(shFinal, "time"); lFinalFocus = GetShaderLocation(shFinal, "focus"); lFinalDofOn = GetShaderLocation(shFinal, "dofOn"); lFinalDofAmt = GetShaderLocation(shFinal, "dofAmt");
  lFxA = GetShaderLocation(shFinal, "fxA"); lFxB = GetShaderLocation(shFinal, "fxB"); lFxC = GetShaderLocation(shFinal, "fxC");
  shAO = LoadShaderFromMemory(PVS, AO_FS); lAOPx = GetShaderLocation(shAO, "px"); lAOAsp = GetShaderLocation(shAO, "aspect"); lFinalAO = GetShaderLocation(shFinal, "aoTex"); lFinalAOOn = GetShaderLocation(shFinal, "aoOn");
}
// draw in logical 1600x900 coordinates whatever the real framebuffer size is
static void LogicalOrtho() { rlDrawRenderBatchActive(); rlMatrixMode(RL_PROJECTION); rlLoadIdentity(); rlOrtho(0, W, H, 0, 0.0, 1.0); rlMatrixMode(RL_MODELVIEW); rlLoadIdentity(); }
static void Blit(Texture2D src, RenderTexture2D dst, Shader sh) {
  BeginTextureMode(dst); ClearBackground(BLACK); BeginShaderMode(sh);
  DrawTexturePro(src, { 0, 0, (float)src.width, (float)-src.height }, { 0, 0, (float)dst.texture.width, (float)dst.texture.height }, { 0, 0 }, 0, WHITE);
  EndShaderMode(); EndTextureMode();
}
extern int g_post;
void PostBegin() { glDepthFunc(GL_LEQUAL); glDepthRangef(0, 1); if (!postOk || !g_post) { BeginDrawing(); LogicalOrtho(); return; } BeginTextureMode(rtMain); LogicalOrtho(); }
// ---------- cinematic camera: a slow establishing glide into every game, then a gentle handheld drift ----------
Camera3D CineCam(Camera3D c) {
  if (!g_cineOn) return c;
  float t = g_cineT;
  Vector3 d = Vector3Subtract(c.position, c.target); float L = Vector3Length(d); if (L < 1e-3f) return c;
  float k = Clamp(t / 4.0f, 0, 1); k = 1 - powf(1 - k, 2.2f); float in = (1 - k) * g_cineK;
  float ang = in * 0.18f;
  Vector3 dr = { d.x * cosf(ang) - d.z * sinf(ang), d.y, d.x * sinf(ang) + d.z * cosf(ang) };
  dr = Vector3Scale(dr, 1 + in * 0.28f); dr.y += in * L * 0.07f;
  float a = L * 0.0016f * g_cineK;
  Vector3 drift = { sinf(t * 0.37f) * a + sinf(t * 0.91f) * a * 0.3f, sinf(t * 0.29f + 1) * a * 0.6f, cosf(t * 0.33f) * a * 0.5f };
  c.position = Vector3Add(Vector3Add(c.target, dr), drift);
  c.target = Vector3Add(c.target, Vector3Scale(drift, 0.35f));
  return cine::Apply(c);
}
#undef BeginMode3D
#undef EndMode3D
Camera3D g_finalCam{}; static bool g_fxDrawn = false;
void CC_BeginMode3D(Camera3D c) { rlDrawRenderBatchActive(); glDepthFunc(GL_LEQUAL); glDepthRangef(0, 1); c = CineCam(c); g_finalCam = c; g_focus = Vector3Length(Vector3Subtract(c.position, c.target)); BeginMode3D(c); }
// flat drawing after the 3D scene writes depth 0, which the focus pass reads as "never blur"
void CC_EndMode3D() { if (g_cineOn && !g_fxDrawn) { g_fxDrawn = true; cine::Draw3D(); } EndMode3D(); if (dofOk && g_cineOn) { rlDrawRenderBatchActive(); rlEnableDepthTest(); glDepthFunc(GL_ALWAYS); glDepthRangef(0, 0); rlEnableDepthMask(); } }
void PostEnd() {
  rlDrawRenderBatchActive(); glDepthFunc(GL_LEQUAL); glDepthRangef(0, 1); rlDisableDepthTest();
  if (!postOk || !g_post) { EndDrawing(); return; }
  EndTextureMode();
  Vector2 px = { 1.f / FW, 1.f / FH }; SetShaderValue(shBright, lBrightPx, &px, SHADER_UNIFORM_VEC2);
  auto Dir = [&](float x, float y) { Vector2 d = { x / FW, y / FH }; SetShaderValue(shBlur, lBlurDir, &d, SHADER_UNIFORM_VEC2); };
  Blit(rtMain.texture, rtA, shBright);
  // anamorphic lens streaks: the bright parts stretched sideways (before the round bloom blur)
  Dir(6, 0); Blit(rtA.texture, rtE, shBlur); Dir(16, 0); Blit(rtE.texture, rtB, shBlur); Dir(40, 0); Blit(rtB.texture, rtE, shBlur);
  for (int k = 0; k < 2; k++) { Dir(4, 0); Blit(rtA.texture, rtB, shBlur); Dir(0, 4); Blit(rtB.texture, rtA, shBlur); }
  BeginTextureMode(rtA); BeginBlendMode(BLEND_ADDITIVE);
  DrawTexturePro(rtE.texture, { 0, 0, (float)rtE.texture.width, (float)-rtE.texture.height }, { 0, 0, (float)rtA.texture.width, (float)rtA.texture.height }, { 0, 0 }, 0, Color{ 70, 120, 255, 255 });
  EndBlendMode(); EndTextureMode();
  // ambient occlusion at half size from the scene's depth, softened twice
  bool ao = dofOk && g_cineOn && shAO.id && g_aoOn;
  if (ao) {
    BeginTextureMode(rtF); ClearBackground(WHITE); BeginShaderMode(shAO); float asp = (float)FW / FH; SetShaderValue(shAO, lAOAsp, &asp, SHADER_UNIFORM_FLOAT); SetShaderValue(shAO, lAOPx, &px, SHADER_UNIFORM_VEC2);
    DrawTexturePro(rtMain.depth, { 0, 0, (float)FW, (float)-FH }, { 0, 0, (float)rtF.texture.width, (float)rtF.texture.height }, { 0, 0 }, 0, WHITE); EndShaderMode(); EndTextureMode();
    Dir(1.5f, 0); Blit(rtF.texture, rtG, shBlur); Dir(0, 1.5f); Blit(rtG.texture, rtF, shBlur);
  }
  // depth-of-field source: a soft, quarter-size copy of the scene
  bool dof = dofOk && g_cineOn && (cine::Busy() || g_dofAmt > 0.01f);
  if (dof) { Dir(3, 0); Blit(rtMain.texture, rtC, shBlur); Dir(0, 6); Blit(rtC.texture, rtD, shBlur); Dir(10, 0); Blit(rtD.texture, rtC, shBlur); Dir(0, 10); Blit(rtC.texture, rtD, shBlur); }
  BeginDrawing(); ClearBackground(BLACK);
  BeginShaderMode(shFinal);
  SetShaderValue(shFinal, lFinalPx, &px, SHADER_UNIFORM_VEC2);
  SetShaderValueTexture(shFinal, lFinalBloom, rtA.texture);
  float tm = (float)GetTime(), don = dof ? 1.f : 0.f, da = cine::Busy() ? 0.3f : g_dofAmt;
  SetShaderValue(shFinal, lFinalTime, &tm, SHADER_UNIFORM_FLOAT); SetShaderValue(shFinal, lFinalFocus, &g_focus, SHADER_UNIFORM_FLOAT); SetShaderValue(shFinal, lFinalDofOn, &don, SHADER_UNIFORM_FLOAT); SetShaderValue(shFinal, lFinalDofAmt, &da, SHADER_UNIFORM_FLOAT);
  { cine::PostFx p = g_cineOn ? cine::Post() : cine::PostFx{}; Vector4 a = { p.flash, p.aber, p.zoom, p.speed }, b = { p.bars, p.impact, g_cineOn ? p.ctr.x : 0.5f, g_cineOn ? p.ctr.y : 0.5f }; Vector3 c = { p.fc.r / 255.f, p.fc.g / 255.f, p.fc.b / 255.f };
    SetShaderValue(shFinal, lFxA, &a, SHADER_UNIFORM_VEC4); SetShaderValue(shFinal, lFxB, &b, SHADER_UNIFORM_VEC4); SetShaderValue(shFinal, lFxC, &c, SHADER_UNIFORM_VEC3); }
  SetShaderValueTexture(shFinal, lFinalDof, dof ? rtD.texture : rtA.texture);
  if (dof) SetShaderValueTexture(shFinal, lFinalDepth, rtMain.depth);
  { float on = ao ? 1.f : 0.f; SetShaderValue(shFinal, lFinalAOOn, &on, SHADER_UNIFORM_FLOAT); SetShaderValueTexture(shFinal, lFinalAO, ao ? rtF.texture : rtA.texture); }
  DrawTexturePro(rtMain.texture, { 0, 0, (float)FW, (float)-FH }, { 0, 0, (float)FW, (float)FH }, { 0, 0 }, 0, WHITE);
  EndShaderMode();
  EndDrawing();
}
Matrix TRS(Vector3 pos, Vector3 r, Vector3 s) {
  Matrix m = MatrixScale(s.x, s.y, s.z);
  m = MatrixMultiply(m, MatrixRotateX(r.x)); m = MatrixMultiply(m, MatrixRotateZ(r.z)); m = MatrixMultiply(m, MatrixRotateY(r.y));
  return MatrixMultiply(m, MatrixTranslate(pos.x, pos.y, pos.z));
}

// ---------- particles ----------
void Particles3D::Burst(Vector3 at, int n, Color c, float sp, float life, float size, float grav) {
  for (int i = 0; i < n && l.size() < 700; i++) {
    Vector3 d = Vector3Normalize({ frand(-1, 1), frand(-1, 1), frand(-1, 1) });
    l.push_back({ at, Vector3Scale(d, sp * frand(0.25f, 1)), life * frand(0.6f, 1.1f), life, size * frand(0.6f, 1.3f), c, 2.2f, grav });
  }
}
void Particles3D::Puff(Vector3 at, Color c, float size, float life, Vector3 v) { if (l.size() < 700) l.push_back({ at, v, life, life, size, c, 1.0f, -0.6f }); }
void Particles3D::Update(float dt) {
  for (auto& p : l) { float k = expf(-p.drag * dt); p.v = Vector3Scale(p.v, k); p.v.y -= p.grav * dt; p.p = Vector3Add(p.p, Vector3Scale(p.v, dt)); p.life -= dt; }
  l.erase(std::remove_if(l.begin(), l.end(), [](const Particle& p) { return p.life <= 0; }), l.end());
}
void Particles3D::Draw() {
  for (auto& p : l) { float t = p.life / p.max; Color c = p.c; c.a = (unsigned char)(c.a * fminf(1, t * 1.6f)); float r = p.size * (0.4f + 0.6f * t) * 2; lit.Draw(prim.sphere, MatrixMultiply(MatrixScale(r, r, r), MatrixTranslate(p.p.x, p.p.y, p.p.z)), c); }
}

// ---------- registry + exported API ----------
static std::vector<std::pair<std::string, GameMaker>>& Registry() { static std::vector<std::pair<std::string, GameMaker>> r; return r; }
RegGame::RegGame(const char* id, GameMaker f) { Registry().push_back({ id, f }); }
std::unique_ptr<Game> MakeRegistered(const std::string& id) { for (auto& r : Registry()) if (r.first == id) return r.second(); return nullptr; }
static std::vector<Player> players;
int PidOfColor(Color c) { for (auto& p : players) if (p.color.r == c.r && p.color.g == c.g && p.color.b == c.b) return p.id; return -1; }
static const char* EMO[] = { "🔥", "👏", "😂", "😱", "😭", "🤯", "😎", "🎉" };
static void DrawEmotes() {
  for (int i = 0; i < 8; i++) {
    if (g_emoteT[i] <= 0 || !g_emoteVis[i]) continue;
    Camera3D c = g_finalCam; if (Vector3DotProduct(Vector3Subtract(g_emotePos[i], c.position), Vector3Subtract(c.target, c.position)) <= 0) continue;
    Vector2 s = GetWorldToScreenEx(g_emotePos[i], c, W, H); float age = 3.2f - g_emoteT[i];
    float sc = age < 0.28f ? EaseOutQ(age / 0.28f) * 1.18f : 1.f + 0.06f * sinf(age * 7), a = fminf(1.f, g_emoteT[i] * 2.4f), y = s.y - 34 - fminf(age, 0.5f) * 24;
    if (s.x < 40 || s.x > 1560 || y < 50 || y > 860) continue;
    float r = 50 * sc;
    RRect(s.x - r, y - r, r * 2, r * 2, r, Color{ 255, 255, 255, (unsigned char)(245 * a) }, Color{ 225, 230, 242, (unsigned char)(245 * a) }, Color{ 20, 33, 61, (unsigned char)(80 * a) }, 2, true);
    Text(s.x, y + 2, 62 * sc, Color{ 255, 255, 255, (unsigned char)(255 * a) }, EMO[g_emoteK[i] & 7], 0, TS_PLAIN);
  }
}
static std::unique_ptr<Game> game;

// ---- player portraits: each player's own character, head and shoulders, rendered once into a small picture for the HUD ----
EM_JS(void, js_portrait, (const char* k, const unsigned char* px, int w, int h), {
  const c = document.createElement('canvas'); c.width = w; c.height = h;
  c.getContext('2d').putImageData(new ImageData(new Uint8ClampedArray(HEAPU8.slice(px, px + w * h * 4).buffer), w, h), 0, 0);
  (window.NPORT || (window.NPORT = {}))[UTF8ToString(k)] = c;
});
bool g_portrait = false;   // people.cpp: no far-away level of detail while a portrait is drawn
static std::vector<unsigned> g_portQ; static std::vector<unsigned> g_portDone;
static unsigned PortraitId(Color c) { return ((unsigned)c.r << 16) | ((unsigned)c.g << 8) | c.b; }
std::string PortraitKey(Color c) { char b[16]; snprintf(b, sizeof b, "@%06x", PortraitId(c)); return b; }
bool Portrait(Color c) {
  unsigned k = PortraitId(c);
  if (std::find(g_portDone.begin(), g_portDone.end(), k) != g_portDone.end()) return true;
  if (std::find(g_portQ.begin(), g_portQ.end(), k) == g_portQ.end()) g_portQ.push_back(k);
  return false;
}
static void RenderPortraits() {   // one a frame, before the scene (so nothing of it leaks into the scene's shadows or contact shadows)
  if (g_portQ.empty() || !HaveFile("p0.glb")) return;
  unsigned k = g_portQ.front(); Color c = { (unsigned char)(k >> 16), (unsigned char)(k >> 8), (unsigned char)k, 255 };
  const int S = 160; static RenderTexture2D rt{}; if (!rt.id) rt = LoadRenderTexture(S, S);
  Camera3D cam{}; cam.position = { 0.34f, 1.6f, 1.12f }; cam.target = { 0.02f, 1.5f, 0 }; cam.up = { 0, 1, 0 }; cam.fovy = 30; cam.projection = CAMERA_PERSPECTIVE;
  auto savedPrev = lit.castPrev; lit.castPrev.clear(); size_t nCast = lit.cast.size(); bool nr = lit.noRec; lit.noRec = true; g_portrait = true;
  BeginTextureMode(rt); rlEnableDepthMask(); ClearBackground(Color{ 0, 0, 0, 0 });
  lit.Begin(cam, { 0.1f, 0.1f, 0.14f }, 0.0f);
  { float z = 0; SetShaderValue(lit.shader, lit.locShOn, &z, SHADER_UNIFORM_FLOAT); }
  lit.Mood(0.42f, { 1.15f, 1.05f, 0.95f }, { -0.35f, -0.6f, -0.7f }, 0.14f); lit.Sky({ 0.72f, 0.76f, 0.95f });
  lit.Light({ -0.9f, 2.3f, -1.0f }, { 0.9f, 1.0f, 1.6f }, 1.6f);   // a cool rim light from behind
  BeginMode3D(cam);
  PersonPose q; q.pos = { 0, 0, 0 }; q.yaw = 0.22f; q.c = c; q.hsh = PersonHash(c); q.height = 1.78f; q.clip = "Idle"; q.t = 0.4f;
  bool ok = DrawPersonPose(q);
  EndMode3D(); EndTextureMode();
  g_portrait = false; lit.noRec = nr; lit.cast.resize(nCast); lit.castPrev = savedPrev;
  if (!ok) return;   // the models are still arriving: try again next frame
  Image im = LoadImageFromTexture(rt.texture); ImageFlipVertical(&im); ImageFormat(&im, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
  js_portrait(PortraitKey(c).c_str(), (const unsigned char*)im.data, im.width, im.height); UnloadImage(im);
  g_portDone.push_back(k); g_portQ.erase(g_portQ.begin());
}

int g_dbg = 0; int g_post = 1; int g_aoOn = 1;   // g_aoOn: the TV turns ambient occlusion and reflections off when it is slow (native.js)
extern "C" {
EMSCRIPTEN_KEEPALIVE void cc_quality(int on) { g_aoOn = on ? 1 : 0; }
static int g_galPage = -1; static float g_galT = 0;   // debug pose gallery (see DrawGallery)
EMSCRIPTEN_KEEPALIVE void cc_dbg(int m) {
  if (m >= 40 && m < 50) {   // debug: play a test moment on whatever the camera looks at
    Camera3D c = g_finalCam; cine::Shot s; s.kind = m - 40; s.at = c.target; s.size = Vector3Distance(c.position, c.target) * 0.1f;
    s.c = Hex(0xffd43b); s.caption = "TEST"; s.sub = "debug"; s.floorY = c.target.y - s.size; s.hero = false; cine::Play(s); return;
  }
  if (m >= 50 && m < 60) { if (game) game->DebugShot(m - 50); return; }
  if (m >= 60 && m < 90) { g_racketAxis = m - 60; return; }
  if (m == 999) { g_galPage = -1; return; }
  if (m >= 1000 && m < 1500) { g_galPage = (m - 1000) / 100; g_galT = ((m - 1000) % 100) / 100.f; return; }
  if (m >= 100) g_post = m - 100; else g_dbg = m; }
static float ClampRS(float rs) { return rs < 1 ? 1 : rs > 2 ? 2 : rs; }
// change the render resolution on the fly (the website lowers it on slow TVs)
EMSCRIPTEN_KEEPALIVE void cc_set_scale(float rs) {
  g_rs = ClampRS(rs); FW = (int)roundf(W * g_rs); FH = (int)roundf(H * g_rs);
  SetWindowSize(FW, FH); PostInit();
}
EMSCRIPTEN_KEEPALIVE void cc_init(float rs) {
  SetConfigFlags(FLAG_MSAA_4X_HINT);
  g_rs = ClampRS(rs > 0 ? rs : 1); FW = (int)roundf(W * g_rs); FH = (int)roundf(H * g_rs);
  InitWindow(FW, FH, "Couch Chaos");
  InstallFileLoader();
  lit.Init(); prim.Init(); UI::Init(); PostInit(); props.Init();
  rlSetClipPlanes(0.3, 600); // better depth precision = no z-fighting
}
// players: "name|#rrggbb|bot;name|#rrggbb|bot;..."
EMSCRIPTEN_KEEPALIVE void cc_start(const char* id, const char* plist) {
  players.clear();
  std::stringstream ss(plist); std::string row; int i = 0;
  while (std::getline(ss, row, ';')) {
    if (row.empty()) continue;
    std::stringstream rs(row); std::string name, col, bot;
    std::getline(rs, name, '|'); std::getline(rs, col, '|'); std::getline(rs, bot, '|');
    players.push_back({ i++, name, ParseColor(col), bot == "1" });
  }
  std::string g = id;
  game.reset(); lit.Reset(); hud::SetTheme(g); g_cineT = 0; g_cineOn = true; cine::Reset();
  // how much each game blurs what is out of focus (game content on the back wall stays sharp)
  g_dofAmt = 0; g_cineK = 1; memset(g_emoteT, 0, sizeof g_emoteT);   // no blur while playing
  if (g == "skybrawl") game = MakeSkyBrawl();
  else if (g == "sumo") game = MakeSumo();
  else if (g == "snowball") game = MakeSnowball();
  else if (g == "chairs") game = MakeChairs();
  else if (g == "redlight") game = MakeRedLight();
  else if (g == "simon") game = MakeSimon();
  else if (g == "tug") game = MakeTug();
  else if (g == "airhockey") game = MakeHockey();
  else if (g == "tag") game = MakeTag();
  else if (g == "cards") game = MakeEights();
  else if (g == "four") game = MakeFour();
  else if (g == "snakes") game = MakeSnakes();
  else if (g == "blackjack") game = MakeBlackjack();
  else if (g == "bingo") game = MakeBingo();
  else if (g == "dice") game = MakeDice();
  else if (g == "memory") game = MakeMemory();
  else if (g == "hilo") game = MakeHiLo();
  else if (g == "word") game = MakeWord();
  else if (g == "trivia") game = MakeTrivia();
  else if (g == "colors") game = MakeClash();
  else if (g == "count") game = MakeCount();
  else if (g == "math") game = MakeMath();
  else if (g == "likely") game = MakeLikely();
  else if (g == "lowest") game = MakeLowest();
  else if (g == "rps") game = MakeRPS();
  else if (g == "bomb") game = MakeBomb();
  else game = MakeRegistered(g);
  if (game) { game->ps = &players; game->Start(); }
}
EMSCRIPTEN_KEEPALIVE void cc_input(int pid, float x, float y, int a, int b) {
  if (pid < 0 || pid >= (int)players.size()) return;
  Player& p = players[pid]; p.stick = { x, y }; p.a = (a & 1) != 0; p.b = b; p.touch = (a & 2) != 0;
}
EMSCRIPTEN_KEEPALIVE void cc_emote(int pid, int k) { if (pid >= 0 && pid < 8) { g_emoteT[pid] = 3.2f; g_emoteK[pid] = k & 7; } }
EMSCRIPTEN_KEEPALIVE void cc_act(int pid, const char* kind, int i, float pw) { if (!game) return; if (strncmp(kind, "cheat:", 6) == 0) game->Cheat(pid, kind + 6); else game->Act(pid, kind, i, pw); }
// ---- debug only: a pose gallery (cc_dbg 1000 + page*100 + time%): every animation clip large, from the front and the side ----
static void DrawGallery() {
  static const char* PAGES[5][8] = {
    { "Idle_Sword", "Punch_Left", "Punch_Right", "Kick_Left", "Kick_Right", "HitRecieve", "HitRecieve_2", "Death" },
    { "Idle", "Idle_Neutral", "Walk", "Run", "Run_Back", "Run_Left", "Run_Right", "Roll" },
    { "Sword_Slash", "Interact", "Wave", "Gun_Shoot", "Idle_Gun", "Idle_Gun_Pointing", "Idle_Gun_Shoot", "Run_Shoot" },
    { "Run", "Run", "Run_Left", "Idle_Sword", "Idle_Sword", "Walk", "Idle", "Idle" },   // blends as the games use them (clip2 below)
    { "Idle_Sword", "Idle_Sword", "Idle_Sword", "Idle_Sword", "Idle_Sword", "Idle_Sword", "Idle_Sword", "Idle_Sword" } };
  static const char* BL[8] = { "Sword_Slash", "Interact", "Sword_Slash", "HitRecieve", "Punch_Right", "Wave", "Interact", "Sword_Slash" };
  int pg = Clamp(g_galPage, 0, 4);
  Camera3D cam{}; cam.position = { 0, 1.9f, 9.0f }; cam.target = { 0, 0.85f, -1.6f }; cam.up = { 0, 1, 0 }; cam.fovy = 33; cam.projection = CAMERA_PERSPECTIVE;
  ClearBackground(Hex(0x8e9bb8));
  lit.Begin(cam, { 0.55f, 0.6f, 0.72f }, 0.0f);
  BeginMode3D(cam);
  lit.Draw(prim.plane, TRS({ 0, 0, -1.5f }, { 0, 0, 0 }, { 30, 1, 14 }), Hex(0xd5dae4));
  for (int row = 0; row < 2; row++) for (int i = 0; i < 8; i++) {
    PersonPose q; q.pos = { (i - 3.5f) * (row ? 1.55f : 1.3f), 0, row ? -3.2f : 0 }; q.yaw = row ? PI / 2 : 0; q.c = Hex(0xe03131); q.hsh = 1; q.height = 1.8f;
    q.clip = PAGES[pg][i]; q.t = g_galT; q.scrub = true;
    if (pg == 3) { q.clip2 = BL[i]; q.t2 = g_galT; q.scrub2 = true; q.mix = 1; q.t = g_galT * 0.7f; q.scrub = true; }
    if (pg == 4) { q.clip2 = i < 4 ? "HitRecieve" : "Punch_Right"; q.t2 = 0.06f + 0.06f * i; q.scrub2 = true; q.mix = i < 4 ? 0.65f : 1.f; }
    DrawPersonPose(q);
    Vector3 f = { sinf(q.yaw), 0, cosf(q.yaw) };   // the way the code thinks this person faces: a green bar on the floor
    lit.Draw(prim.cube, TRS(Vector3Add(q.pos, Vector3Add(Vector3Scale(f, 0.55f), { 0, 0.03f, 0 })), { 0, q.yaw, 0 }, { 0.08f, 0.06f, 0.5f }), Hex(0x2b8a3e));
  }
  EndMode3D();
  for (int i = 0; i < 8; i++) { Vector2 s = GetWorldToScreen({ (i - 3.5f) * 1.3f, 2.15f, 0 }, cam); Text(s.x, s.y, 15, WHITE, pg == 3 ? std::string(PAGES[pg][i]) + "+" + BL[i] : std::string(PAGES[pg][i]), 0, TS_HEAVY); }
  Text(800, 40, 26, WHITE, "pose gallery  page " + std::to_string(pg) + "  t=" + std::to_string((int)(g_galT * 100)) + "%", 0, TS_HEAVY);
}
EMSCRIPTEN_KEEPALIVE void cc_frame(float dt) {
  if (!game) return;
  dt = fminf(dt, 0.05f);
  g_frameDt = dt;
  g_cineT += dt;
  cine::Update(dt);
  float gdt = dt * cine::TimeScale();   // hit-stop and slow motion during a big moment
  game->Update(gdt);
  for (auto& p : players) { p.pa = p.a; p.pb = p.b; p.ptouch = p.touch; }
  RenderPortraits();
  lit.ShadowPass();
  PostBegin();
  rlEnableDepthMask(); ClearBackground(BLACK); // every frame starts clean (color + depth)
  hud::TagReset(); g_fxDrawn = false;
  for (int i = 0; i < 8; i++) { g_emoteVis[i] = false; g_emoteT[i] -= dt; }
  if (g_galPage >= 0) DrawGallery(); else game->Draw();
  DrawEmotes();
  cine::Draw2D();
  PostEnd();
  lit.EndFrame(gdt);
}
EMSCRIPTEN_KEEPALIVE void cc_stop() { game.reset(); lit.Reset(); g_cineOn = false; cine::Reset(); }
}

int main() { return 0; }
