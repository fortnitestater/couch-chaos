// Couch Chaos native core: window, bridge, lighting shader, primitives, particles, game registry.
#include "cc.h"
#include "mesh.h"
#include <emscripten/emscripten.h>
#include <sstream>
#include "rlgl.h"

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

void Text(float x, float y, float size, Color c, const std::string& s, int align, int style, float w) { if (c.a == 0 || s.empty()) return; js_text(x, y, size, (int)(((unsigned)c.r << 24) | ((unsigned)c.g << 16) | ((unsigned)c.b << 8) | c.a), s.c_str(), align, style, w); }
float Measure(const std::string& s, float size, int style) { return js_measure(s.c_str(), size, style); }
void TextClip(float x, float y, float w, float h) { js_clip(x, y, w, h); }
void TextUnclip() { js_clip(-1, -1, -1, -1); }
void Layout(int pid, const std::string& json) { js_layout(pid, json.c_str()); }
void Sfx(const char* name) { js_sfx(name); }
void Shake(float a) { js_shake(a); }
void Finish(const std::string& r) { js_finish(r.c_str()); }
void Banner(const std::string& s, int ms, const char* c) { js_banner(s.c_str(), ms, c); }
void Buzz(int pid, int ms) { js_buzz(pid, ms); }
void Send(int pid, const std::string& json) { js_send(pid, json.c_str()); }
static int RGBA(Color c) { return (int)(((unsigned)c.r << 24) | ((unsigned)c.g << 16) | ((unsigned)c.b << 8) | c.a); }
void Img(const std::string& k, float x, float y, float w, float h, float r, float a, float pan, float zoom) { if (a > 0.005f) js_img(k.c_str(), x, y, w, h, r, a, pan, zoom); }
void RRect(float x, float y, float w, float h, float r, Color f1, Color f2, Color st, float lw, bool grad, bool gold) { js_rr(x, y, w, h, r, RGBA(f1), RGBA(f2), RGBA(st), lw, grad, gold); }

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
uniform vec4 colDiffuse; uniform vec3 lightDir; uniform vec3 camPos; uniform vec3 fogColor; uniform float fogDensity; uniform float ambient;
uniform float pattern; uniform vec4 casters[24]; uniform float nCast; uniform float time; uniform vec3 sunCol; uniform vec3 skyCol;
uniform sampler2D shadowMap; uniform mat4 lightVP; uniform float shOn; uniform float shTex;
float unpackD(vec4 c) { return dot(c, vec4(1.0, 1.0 / 255.0, 1.0 / 65025.0, 1.0 / 16581375.0)); }
float sunShadow(vec3 p, vec3 n, float nd) {
  if (shOn < 0.5) return 1.0;
  vec4 lp = lightVP * vec4(p + n * 0.035, 1.0);
  vec3 q = lp.xyz * 0.5 + 0.5;
  if (q.x < 0.0 || q.x > 1.0 || q.y < 0.0 || q.y > 1.0 || q.z > 1.0) return 1.0;
  float bias = 0.0005 + 0.0016 * (1.0 - clamp(nd, 0.0, 1.0));
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
  vec3 base = colDiffuse.rgb * vC.rgb;  // per-vertex colour (white unless a merged mesh brings its own)
  vec3 n = normalize(vN); vec3 L = normalize(-lightDir); vec3 V = normalize(camPos - vW);
  float spec = 0.35, gloss = 40.0;
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
      spec = 0.25; gloss = 60.0;
    } else if (pattern < 3.5) { // felt
      base *= 0.9 + 0.12 * vn(q * 30.0) + 0.06 * fbm(q * 2.0);
      spec = 0.02;
    } else if (pattern < 4.5) { // snow: soft drifts + glitter
      base *= 0.93 + 0.09 * fbm(q * 0.35);
      float gl = step(0.985, h21(floor(q * 18.0))) * pow(max(dot(reflect(-L, n), V), 0.0), 6.0);
      base += gl * 0.9; spec = 0.1;
    } else if (pattern < 5.5) { // ice: cracks + sheen
      float c = abs(vn(q * 1.3) - 0.5); base = mix(base, vec3(1.0), (1.0 - smoothstep(0.0, 0.03, c)) * 0.45);
      base *= 0.95 + 0.08 * fbm(q * 0.5); spec = 0.9; gloss = 90.0;
    } else if (pattern < 6.5) { // tiles
      vec2 t = fract(q * 0.5); float gr = smoothstep(0.0, 0.04, min(min(t.x, t.y), min(1.0 - t.x, 1.0 - t.y)));
      base *= (0.75 + 0.25 * gr) * (0.95 + 0.08 * h21(floor(q * 0.5))); spec = 0.5;
    } else if (pattern < 7.5) { // water: moving ripples
      float r = fbm(q * 0.5 + vec2(time * 0.25, time * 0.18)) + fbm(q * 0.9 - vec2(time * 0.2, -time * 0.1));
      n = normalize(n + vec3(r - 0.9, 0.0, vn(q * 0.7 + time * 0.3) - 0.5) * 0.5);
      base *= 0.85 + 0.25 * r; spec = 0.6; gloss = 80.0;
    } else if (pattern < 8.5) { // rug: woven rings
      float d = length(q - vec2(0.0, 0.5)); base *= 0.85 + 0.15 * step(0.5, fract(d * 0.6)) + 0.05 * vn(q * 25.0); spec = 0.02;
    } else {                    // stone
      base *= 0.8 + 0.3 * fbm(q * 1.5 + vW.y); spec = 0.15;
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
      sh *= 1.0 - 0.6 * (1.0 - smoothstep(0.25, 1.0, d));
    }
  }
  float ssh = sunShadow(vW, n, nd);
  sh *= mix(0.22, 1.0, ssh);
  // cheap ambient occlusion: creases at the ground and under things get darker
  float ao = mix(0.72, 1.0, smoothstep(-0.6, 0.5, n.y)) * mix(0.85, 1.0, ssh);
  vec3 hemi = mix(vec3(0.5, 0.44, 0.4) * ambient, skyCol * (ambient + 0.12), n.y * 0.5 + 0.5) * ao;
  vec3 Hh = normalize(V + L);
  float s = pow(max(dot(n, Hh), 0.0), gloss) * spec * step(0.0, nd);
  float fres = pow(1.0 - max(dot(n, V), 0.0), 3.0);
  // key (sun) + sky fill + a soft fill light from the camera so faces and fronts are never in the dark
  float camFill = max(dot(n, V), 0.0) * 0.16;
  vec3 col = base * (hemi * mix(0.8, 1.0, sh) + sunCol * 1.02 * diff * sh + vec3(camFill)) + sunCol * s * sh * ssh + skyCol * fres * 0.25;
  float dist = length(camPos - vW); float f = 1.0 - exp(-pow(dist * fogDensity, 2.0));
  gl_FragColor = vec4(mix(col, fogColor, clamp(f, 0.0, 1.0)), colDiffuse.a);
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
Lit lit; Prims prim;
void Lit::Init() {
  shader = LoadShaderFromMemory(VS, FS);
  shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");
  locLight = GetShaderLocation(shader, "lightDir"); locCam = GetShaderLocation(shader, "camPos"); locFog = GetShaderLocation(shader, "fogColor"); locFogD = GetShaderLocation(shader, "fogDensity"); locAmb = GetShaderLocation(shader, "ambient");
  locPat = GetShaderLocation(shader, "pattern"); locCast = GetShaderLocation(shader, "casters"); locNCast = GetShaderLocation(shader, "nCast"); locTime = GetShaderLocation(shader, "time");
  locSun = GetShaderLocation(shader, "sunCol"); locSky = GetShaderLocation(shader, "skyCol");
  mat = LoadMaterialDefault(); mat.shader = shader;
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
  Vector3 L = Vector3Normalize({ -0.45f, -1.0f, -0.35f });
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
  Vector3 L = Vector3Normalize({ -0.45f, -1.0f, -0.35f }); float amb = 0.52f;
  // sky fill tinted by the scene's fog colour; dark scenes (night, rooms) get a cooler, dimmer fill
  float lum = fogColor.x * 0.3f + fogColor.y * 0.55f + fogColor.z * 0.15f;
  Vector3 sky = Vector3Lerp({ 0.75f, 0.82f, 1.0f }, fogColor, 0.35f);
  if (lum < 0.3f) sky = Vector3Lerp({ 0.78f, 0.8f, 0.95f }, fogColor, 0.15f);  // indoor / night scenes still get a bright fill
  Vector3 sun = { 1.0f, 0.95f, 0.86f };
  SetShaderValue(shader, locLight, &L, SHADER_UNIFORM_VEC3); SetShaderValue(shader, locCam, &cam.position, SHADER_UNIFORM_VEC3);
  SetShaderValue(shader, locFog, &fogColor, SHADER_UNIFORM_VEC3); SetShaderValue(shader, locFogD, &fogDensity, SHADER_UNIFORM_FLOAT); SetShaderValue(shader, locAmb, &amb, SHADER_UNIFORM_FLOAT);
  SetShaderValue(shader, locSun, &sun, SHADER_UNIFORM_VEC3); SetShaderValue(shader, locSky, &sky, SHADER_UNIFORM_VEC3); SetShaderValue(shader, locTime, &time, SHADER_UNIFORM_FLOAT);
  float nc = (float)castPrev.size(); SetShaderValue(shader, locNCast, &nc, SHADER_UNIFORM_FLOAT);
  if (!castPrev.empty()) SetShaderValueV(shader, locCast, castPrev.data(), SHADER_UNIFORM_VEC4, (int)castPrev.size());
  if (!haveCam) { lastCam = cam; haveCam = true; }
  SetShaderValue(shader, locShOn, &shOn, SHADER_UNIFORM_FLOAT);
  float tex = 1.f / SHADOW_RES; SetShaderValue(shader, locShTex, &tex, SHADER_UNIFORM_FLOAT);
  SetShaderValueMatrix(shader, locLVP, lightVP);
  Pattern(PAT_NONE);
}
void Lit::Pattern(int p) { float f = (float)p; SetShaderValue(shader, locPat, &f, SHADER_UNIFORM_FLOAT); }
void Lit::Draw(const Mesh& m, Matrix t, Color c) {
  mat.maps[MATERIAL_MAP_DIFFUSE].color = c; rlDisableBackfaceCulling(); DrawMesh(m, mat, t);
  // opaque, not-huge things cast sun shadows (floors, walls and sky domes only receive them)
  if (c.a == 255 && rec.size() < 6000) {
    float sx = t.m0 * t.m0 + t.m1 * t.m1 + t.m2 * t.m2, sy = t.m4 * t.m4 + t.m5 * t.m5 + t.m6 * t.m6, sz = t.m8 * t.m8 + t.m9 * t.m9 + t.m10 * t.m10;
    if (fmaxf(sx, fmaxf(sy, sz)) < 30 * 30) rec.push_back({ m, t });
  }
}
void Prims::Init() {
  cube = GenMeshCube(1, 1, 1); sphere = GenMeshSphere(0.5f, 22, 30); cyl = GenMeshCylinder(0.5f, 1, 24); cone = GenMeshCone(0.5f, 1, 24); plane = GenMeshPlane(1, 1, 1, 1);
  torus = GenMeshTorus(0.25f, 1, 16, 24); hemi = GenMeshHemiSphere(0.5f, 16, 24);
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
  gl_FragColor = vec4(c * smoothstep(0.78, 1.0, l), 1.0);
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
precision mediump float; varying vec2 uv; uniform sampler2D texture0; uniform sampler2D bloom; uniform vec2 px;
vec3 S(vec2 p) { return texture2D(texture0, p).rgb; }
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
    // gentle contrast-adaptive sharpening: crisper textures and outlines without halos
    vec3 nb = (cN + cS + cE + cW) * 0.25; float amt = 0.35 * (1.0 - smoothstep(0.02, 0.25, mx - mn));
    c = clamp(cM + (cM - nb) * amt, min(min(cN, cS), min(cE, cW)), max(max(cN, cS), max(cE, cW)));
  }
  c += texture2D(bloom, uv).rgb * 0.38;
  c = mix(c, c * c * (3.0 - 2.0 * c), 0.15);
  float l = dot(c, lw); c = mix(vec3(l), c, 1.12);
  c += vec3(0.012, 0.0, -0.01) * smoothstep(0.4, 1.0, l) + vec3(-0.008, 0.0, 0.014) * (1.0 - smoothstep(0.0, 0.4, l));
  vec2 v = uv - 0.5; c *= 1.0 - dot(v, v) * 0.22;
  gl_FragColor = vec4(clamp(c, 0.0, 1.0), 1.0);
})";
static RenderTexture2D rtMain, rtA, rtB; static Shader shBright, shBlur, shFinal; static int lBrightPx, lBlurDir, lFinalPx, lFinalBloom; static bool postOk = false;
float g_rs = 1; static int FW = W, FH = H;
static void LoadPostShaders();
static void PostInit() {
  if (rtMain.id) { UnloadRenderTexture(rtMain); UnloadRenderTexture(rtA); UnloadRenderTexture(rtB); }
  rtMain = LoadRenderTexture(FW, FH); rtA = LoadRenderTexture(FW / 4, FH / 4); rtB = LoadRenderTexture(FW / 4, FH / 4);
  SetTextureFilter(rtMain.texture, TEXTURE_FILTER_BILINEAR); SetTextureFilter(rtA.texture, TEXTURE_FILTER_BILINEAR); SetTextureFilter(rtB.texture, TEXTURE_FILTER_BILINEAR);
  if (!shFinal.id) LoadPostShaders();
  if (!shFinal.id || shFinal.id == rlGetShaderIdDefault()) postOk = false; else postOk = rtMain.id > 0;
}
static void LoadPostShaders() {
  shBright = LoadShaderFromMemory(PVS, BRIGHT_FS); shBlur = LoadShaderFromMemory(PVS, BLUR_FS); shFinal = LoadShaderFromMemory(PVS, FINAL_FS);
  lBrightPx = GetShaderLocation(shBright, "px"); lBlurDir = GetShaderLocation(shBlur, "dir"); lFinalPx = GetShaderLocation(shFinal, "px"); lFinalBloom = GetShaderLocation(shFinal, "bloom");
}
// draw in logical 1600x900 coordinates whatever the real framebuffer size is
static void LogicalOrtho() { rlDrawRenderBatchActive(); rlMatrixMode(RL_PROJECTION); rlLoadIdentity(); rlOrtho(0, W, H, 0, 0.0, 1.0); rlMatrixMode(RL_MODELVIEW); rlLoadIdentity(); }
static void Blit(Texture2D src, RenderTexture2D dst, Shader sh) {
  BeginTextureMode(dst); ClearBackground(BLACK); BeginShaderMode(sh);
  DrawTexturePro(src, { 0, 0, (float)src.width, (float)-src.height }, { 0, 0, (float)dst.texture.width, (float)dst.texture.height }, { 0, 0 }, 0, WHITE);
  EndShaderMode(); EndTextureMode();
}
extern int g_post;
void PostBegin() { if (!postOk || !g_post) { BeginDrawing(); LogicalOrtho(); return; } BeginTextureMode(rtMain); LogicalOrtho(); }
void PostEnd() {
  if (!postOk || !g_post) { EndDrawing(); return; }
  EndTextureMode();
  Vector2 px = { 1.f / FW, 1.f / FH }; SetShaderValue(shBright, lBrightPx, &px, SHADER_UNIFORM_VEC2);
  Blit(rtMain.texture, rtA, shBright);
  for (int k = 0; k < 2; k++) {
    Vector2 d = { 4.f / FW, 0 }; SetShaderValue(shBlur, lBlurDir, &d, SHADER_UNIFORM_VEC2); Blit(rtA.texture, rtB, shBlur);
    d = { 0, 4.f / FH }; SetShaderValue(shBlur, lBlurDir, &d, SHADER_UNIFORM_VEC2); Blit(rtB.texture, rtA, shBlur);
  }
  BeginDrawing(); ClearBackground(BLACK);
  BeginShaderMode(shFinal);
  SetShaderValue(shFinal, lFinalPx, &px, SHADER_UNIFORM_VEC2);
  SetShaderValueTexture(shFinal, lFinalBloom, rtA.texture);
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
static std::unique_ptr<Game> game;

int g_dbg = 0; int g_post = 1;
extern "C" {
EMSCRIPTEN_KEEPALIVE void cc_dbg(int m) { if (m >= 100) g_post = m - 100; else g_dbg = m; }
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
  game.reset(); lit.Reset();
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
EMSCRIPTEN_KEEPALIVE void cc_act(int pid, const char* kind, int i, float pw) { if (game) game->Act(pid, kind, i, pw); }
EMSCRIPTEN_KEEPALIVE void cc_frame(float dt) {
  if (!game) return;
  dt = fminf(dt, 0.05f);
  game->Update(dt);
  for (auto& p : players) { p.pa = p.a; p.pb = p.b; }
  lit.ShadowPass();
  PostBegin();
  rlEnableDepthMask(); ClearBackground(BLACK); // every frame starts clean (color + depth)
  game->Draw();
  PostEnd();
  lit.EndFrame(dt);
}
EMSCRIPTEN_KEEPALIVE void cc_stop() { game.reset(); lit.Reset(); }
}

int main() { return 0; }
