// Couch Chaos native core: window, bridge, lighting shader, primitives, particles, game registry.
#include "cc.h"
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

Color ParseColor(const std::string& hex) { unsigned v = 0x888888; if (hex.size() >= 7 && hex[0] == '#') v = (unsigned)strtoul(hex.substr(1, 6).c_str(), nullptr, 16); return Hex(v); }

// ---------- lighting: sun + ambient + specular + distance fog (GLSL ES 1.0 for WebGL) ----------
static const char* VS = R"(#version 100
attribute vec3 vertexPosition; attribute vec3 vertexNormal; attribute vec2 vertexTexCoord;
uniform mat4 mvp; uniform mat4 matModel;
varying vec3 vN; varying vec3 vW; varying vec2 vUV;
void main() { vW = (matModel * vec4(vertexPosition, 1.0)).xyz; vN = normalize((matModel * vec4(vertexNormal, 0.0)).xyz); vUV = vertexTexCoord; gl_Position = mvp * vec4(vertexPosition, 1.0); })";
static const char* FS = R"(#version 100
precision mediump float;
varying vec3 vN; varying vec3 vW; varying vec2 vUV;
uniform sampler2D texture0; uniform vec4 colDiffuse; uniform vec3 lightDir; uniform vec3 camPos; uniform vec3 fogColor; uniform float fogDensity; uniform float ambient;
void main() {
  vec4 tc = colDiffuse; // untextured: never depends on whatever texture is bound
  vec3 n = normalize(vN); vec3 L = normalize(-lightDir);
  float d = max(dot(n, L), 0.0);
  vec3 V = normalize(camPos - vW); vec3 Hh = normalize(V + L);
  float s = pow(max(dot(n, Hh), 0.0), 40.0) * 0.45;
  float rim = pow(1.0 - max(dot(n, V), 0.0), 3.0) * 0.18;
  vec3 c = tc.rgb * (ambient + 0.85 * d) + vec3(s + rim);
  float dist = length(camPos - vW); float f = 1.0 - exp(-pow(dist * fogDensity, 2.0));
  gl_FragColor = vec4(mix(c, fogColor, clamp(f, 0.0, 1.0)), tc.a);
})";
Lit lit; Prims prim;
void Lit::Init() {
  shader = LoadShaderFromMemory(VS, FS);
  shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");
  locLight = GetShaderLocation(shader, "lightDir"); locCam = GetShaderLocation(shader, "camPos"); locFog = GetShaderLocation(shader, "fogColor"); locFogD = GetShaderLocation(shader, "fogDensity"); locAmb = GetShaderLocation(shader, "ambient");
  mat = LoadMaterialDefault(); mat.shader = shader;
}
void Lit::Begin(const Camera3D& cam, Vector3 fogColor, float fogDensity) {
  Vector3 L = Vector3Normalize({ -0.45f, -1.0f, -0.35f }); float amb = 0.42f;
  SetShaderValue(shader, locLight, &L, SHADER_UNIFORM_VEC3); SetShaderValue(shader, locCam, &cam.position, SHADER_UNIFORM_VEC3);
  SetShaderValue(shader, locFog, &fogColor, SHADER_UNIFORM_VEC3); SetShaderValue(shader, locFogD, &fogDensity, SHADER_UNIFORM_FLOAT); SetShaderValue(shader, locAmb, &amb, SHADER_UNIFORM_FLOAT);
}
void Lit::Draw(const Mesh& m, Matrix t, Color c) { mat.maps[MATERIAL_MAP_DIFFUSE].color = c; rlDisableBackfaceCulling(); DrawMesh(m, mat, t); }
void Prims::Init() { cube = GenMeshCube(1, 1, 1); sphere = GenMeshSphere(0.5f, 14, 18); cyl = GenMeshCylinder(0.5f, 1, 18); cone = GenMeshCone(0.5f, 1, 18); plane = GenMeshPlane(1, 1, 1, 1); }
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
static std::vector<Player> players;
static std::unique_ptr<Game> game;

int g_dbg = 0;
extern "C" {
EMSCRIPTEN_KEEPALIVE void cc_dbg(int m) { g_dbg = m; }
EMSCRIPTEN_KEEPALIVE void cc_init() {
  SetConfigFlags(FLAG_MSAA_4X_HINT);
  InitWindow(W, H, "Couch Chaos");
  lit.Init(); prim.Init(); UI::Init();
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
  game.reset();
  if (g == "skybrawl") game = MakeSkyBrawl();
  else if (g == "sumo") game = MakeSumo();
  else if (g == "snowball") game = MakeSnowball();
  if (game) { game->ps = &players; game->Start(); }
}
EMSCRIPTEN_KEEPALIVE void cc_input(int pid, float x, float y, int a, int b) {
  if (pid < 0 || pid >= (int)players.size()) return;
  Player& p = players[pid]; p.stick = { x, y }; p.a = a; p.b = b;
}
EMSCRIPTEN_KEEPALIVE void cc_act(int pid, const char* kind, int i, float pw) { if (game) game->Act(pid, kind, i, pw); }
EMSCRIPTEN_KEEPALIVE void cc_frame(float dt) {
  if (!game) return;
  dt = fminf(dt, 0.05f);
  game->Update(dt);
  for (auto& p : players) { p.pa = p.a; p.pb = p.b; }
  BeginDrawing();
  rlEnableDepthMask(); ClearBackground(BLACK); // every frame starts clean (color + depth)
  game->Draw();
  EndDrawing();
}
EMSCRIPTEN_KEEPALIVE void cc_stop() { game.reset(); }
}

int main() { return 0; }
