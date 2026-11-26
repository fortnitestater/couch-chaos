// Couch Chaos — native (C++ / raylib) game engine, compiled to WebAssembly.
// The website (tv.js) owns rooms, phones and menus; these games own the gameplay + graphics.
#pragma once
#include "raylib.h"
#include "raymath.h"
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <cstdlib>
#include <algorithm>

constexpr int W = 1600, H = 900;  // the logical screen every game draws in
// the real render resolution is W*g_rs x H*g_rs (matches the TV: 1.2 = 1920x1080, 2 = 3200x1800); games never need to care
extern float g_rs;
extern int g_dbg;  // debug switches (7 = tennis close-up camera)
// world → screen in LOGICAL 1600x900 space (overlay text + 2D drawing use logical coordinates)
// cinematic camera (main.cpp): every 3D view goes through it, and so do labels pinned to 3D things
Camera3D CineCam(Camera3D c);
void CC_BeginMode3D(Camera3D c);
void CC_EndMode3D();
#define BeginMode3D(c) CC_BeginMode3D(c)
#define EndMode3D() CC_EndMode3D()
inline Vector2 CC_WorldToScreen(Vector3 p, Camera3D c) { return GetWorldToScreenEx(p, CineCam(c), W, H); }
#define GetWorldToScreen(...) CC_WorldToScreen(__VA_ARGS__)
inline void BeginScissorL(float x, float y, float w, float h) { BeginScissorMode((int)(x * g_rs), (int)(y * g_rs), (int)(w * g_rs), (int)(h * g_rs)); }

// ---------- bridge to the website ----------
// Text is drawn by the website with real fonts (emoji, names). align: 0 center, -1 left, 1 right. wrapW > 0 wraps lines.
enum { TS_DISPLAY = 1, TS_PLAIN = 2, TS_SHADOW = 4, TS_LIGHT = 8, TS_HEAVY = 16, TS_GOLD = 32, TS_SPACED = 64, TS_SILVER = 128 };
void Text(float x, float y, float size, Color c, const std::string& s, int align = 0, int style = 0, float wrapW = 0);
float Measure(const std::string& s, float size, int style = 0);
void TextClip(float x, float y, float w, float h);
void TextUnclip();
void Layout(int pid, const std::string& json);  // pid -1 = everyone: what each phone shows
void Sfx(const char* name);
void Shake(float amount);
void Finish(const std::string& ranking);
void Banner(const std::string& text, int ms = 1600, const char* color = "#ffffff");
void Buzz(int pid, int ms);
void Send(int pid, const std::string& json);
// overlay drawing (website canvas, above the GL frame): cover art by key, rounded rects with gradient / gold rim
void Img(const std::string& key, float x, float y, float w, float h, float radius, float alpha, float pan = 0, float zoom = 1);
void RRect(float x, float y, float w, float h, float radius, Color fill, Color fill2, Color stroke, float lw, bool gradient = false, bool goldStroke = false);  // any message straight to one phone        // "pid:stat;pid:stat" best first

// ---------- players ----------
struct Player { int id; std::string name; Color color; bool bot; Vector2 stick{}; bool a = false, b = false, pa = false, pb = false, touch = false, ptouch = false; };

// ---------- small helpers ----------
inline float frand(float a, float b) { return a + (b - a) * (float)rand() / (float)RAND_MAX; }
inline float angDiff(float a, float b) { float d = fmodf(b - a + PI * 3, PI * 2) - PI; return d; }
inline Color Hex(unsigned v, unsigned char a = 255) { return Color{ (unsigned char)(v >> 16), (unsigned char)(v >> 8), (unsigned char)v, a }; }
inline Color Shade(Color c, float k) { auto f = [&](unsigned char v) { return (unsigned char)Clamp(k < 0 ? v * (1 + k) : v + (255 - v) * k, 0, 255); }; return Color{ f(c.r), f(c.g), f(c.b), c.a }; }
Color ParseColor(const std::string& hex);

// ---------- lit 3D rendering ----------
// surface patterns for Lit::Draw (set with lit.Pattern(...); every lit.Begin resets it to PAT_NONE)
enum { PAT_NONE = 0, PAT_GRASS, PAT_WOOD, PAT_FELT, PAT_SNOW, PAT_ICE, PAT_TILE, PAT_WATER, PAT_RUG, PAT_STONE };
struct Lit {
  Shader shader{}; Material mat{}; int locLight, locCam, locFog, locFogD, locAmb, locPat, locCast, locNCast, locTime, locSun, locSky;
  std::vector<Vector4> cast, castPrev;  // soft contact shadows (x, groundY, z, radius), drawn one frame late
  float time = 0;
  // real sun shadows: every opaque lit.Draw is recorded, and next frame the scene is rendered from the sun into a depth map
  struct Rec { Mesh m; Matrix t; };
  std::vector<Rec> rec; Camera3D lastCam{}; bool haveCam = false; float shOn = 0; Matrix lightVP{};
  RenderTexture2D shRT{}; Shader shSh{}; Material shMat{}; int locLVP, locShOn, locShTex;
  void ShadowPass();
  void Reset() { rec.clear(); cast.clear(); castPrev.clear(); haveCam = false; shOn = 0; }
  void Init();
  void Begin(const Camera3D& cam, Vector3 fogColor, float fogDensity);
  void Draw(const Mesh& m, Matrix t, Color c);
  void Pattern(int p);
  void Caster(Vector3 ground, float radius) { if (cast.size() < 24) cast.push_back({ ground.x, ground.y, ground.z, radius }); }
  void EndFrame(float dt) { castPrev.swap(cast); cast.clear(); time += dt; }
};
// whole-frame post processing: bloom, grade, vignette, FXAA
void PostBegin();
void PostEnd();
extern Lit lit;
struct Prims { Mesh cube, sphere, cyl, cone, plane, torus, hemi; void Init(); };
// a rounded limb from a to b (cylinder + ball ends)
void Limb(Vector3 a, Vector3 b, float r, Color c);
extern Prims prim;
Matrix TRS(Vector3 pos, Vector3 rotEuler, Vector3 scale);

// ---------- 3D particles ----------
struct Particle { Vector3 p, v; float life, max, size; Color c; float drag; float grav; };
struct Particles3D {
  std::vector<Particle> l;
  void Burst(Vector3 at, int n, Color c, float speed, float life, float size, float grav = 0);
  void Puff(Vector3 at, Color c, float size, float life, Vector3 v = { 0, 0, 0 });
  void Update(float dt);
  void Draw();
};

// ---------- UI kit (art.cpp) ----------
namespace UI {
  void Init();
  void Box(Rectangle r, float rad, Color c1, Color c2, float angleDeg = 90, Color border = BLANK, float bw = 0);
  inline void Box(Rectangle r, float rad, Color c) { Box(r, rad, c, c); }
  void Shadow(Rectangle r, float rad, float blur, Color c, float dy = 0);
  void Sky(Color top, Color bottom);
}
// ---------- couch-readable HUD pieces shared by every game (navy glass + gold rim, big type) ----------
inline float EaseOutQ(float t) { t = t < 0 ? 0 : t > 1 ? 1 : t; return 1 - (1 - t) * (1 - t); }
namespace hud {
// per-channel look of every HUD panel (set when a game starts)
struct Theme { Color top, bot, rim, accent, title; bool gold; };
inline Theme& TH() { static Theme t{ { 0x16, 0x24, 0x5a, 235 }, { 0x0a, 0x12, 0x33, 235 }, { 255, 255, 255, 200 }, { 0xff, 0xd4, 0x3b, 255 }, { 0xf2, 0xc6, 0x6d, 255 }, true }; return t; }
inline void SetTheme(const std::string& id) {
  auto in = [&](std::initializer_list<const char*> l) { for (auto s : l) if (id == s) return true; return false; };
  Theme& t = TH();
  if (in({ "song" })) t = { { 0x9a, 0x16, 0x66, 240 }, { 0x3a, 0x0a, 0x45, 240 }, { 0xff, 0xff, 0xff, 210 }, { 0x5f, 0xf2, 0xff, 255 }, { 0x9b, 0xf6, 0xff, 255 }, false };
  else if (in({ "trivia", "likely", "elf", "doodle", "bomb", "lowest", "reflex" })) t = { { 0x6a, 0x1f, 0xb0, 240 }, { 0x2a, 0x0b, 0x4f, 240 }, { 255, 255, 255, 200 }, { 0xff, 0xd4, 0x3b, 255 }, { 0xff, 0xd4, 0x3b, 255 }, true };
  else if (in({ "blackjack", "storm", "avalanche" })) t = { { 0x0e, 0x6b, 0x4f, 240 }, { 0x05, 0x33, 0x26, 240 }, { 255, 255, 255, 200 }, { 0xf2, 0xc6, 0x6d, 255 }, { 0xf2, 0xc6, 0x6d, 255 }, true };
  else if (in({ "four", "tycoon" })) t = { { 0xb3, 0x3d, 0x16, 240 }, { 0x5c, 0x12, 0x10, 240 }, { 255, 255, 255, 200 }, { 0xff, 0xe0, 0x8a, 255 }, { 0xff, 0xe0, 0x8a, 255 }, true };
  else if (in({ "tennis", "bowling", "airhockey", "derby" })) t = { { 0x1d, 0x52, 0xd6, 240 }, { 0x07, 0x1c, 0x4d, 240 }, { 255, 255, 255, 230 }, { 0xc6, 0xff, 0x3d, 255 }, { 0xc6, 0xff, 0x3d, 255 }, false };
  else t = { { 0x16, 0x24, 0x5a, 235 }, { 0x0a, 0x12, 0x33, 235 }, { 255, 255, 255, 200 }, { 0xff, 0xd4, 0x3b, 255 }, { 0xf2, 0xc6, 0x6d, 255 }, true };
}
struct Row { Color c; std::string name, val; bool hot; };
// TV hints are one short line: no emoji prefix, only the first idea
inline std::string ShortHint(const std::string& s) {
  size_t a = 0; while (a < s.size() && ((unsigned char)s[a] >= 0x80 || s[a] == ' ')) a++;
  std::string r = s.substr(a); size_t cut = std::string::npos;
  for (const char* sep : { " · ", " — ", " – ", " (", ". ", "! " }) { size_t p = r.find(sep); if (p != std::string::npos && p > 8 && p < cut) cut = p + (sep[0] == '!' ? 1 : 0); }
  if (cut != std::string::npos) r = r.substr(0, cut);
  return r.empty() ? s : r;
}
inline std::vector<Rectangle>& TagRects() { static std::vector<Rectangle> v; return v; }
inline void TagReset() { TagRects().clear(); }
inline void Panel(float x, float y, float w, float h, float r, bool gold = true) {
  RRect(x, y + 6, w, h, r, Color{ 0, 0, 0, 70 }, Color{ 0, 0, 0, 0 }, BLANK, 0, true);
  const Theme& t = TH();
  RRect(x, y, w, h, r, t.top, t.bot, t.rim, gold ? 3.5f : 2, true, gold && t.gold);
  RRect(x + 6, y + 4, w - 12, fminf(h * 0.42f, 34), fminf(r, 17), Color{ 255, 255, 255, 30 }, Color{ 255, 255, 255, 0 }, BLANK, 0, true);   // top sheen
}
inline void Board(const std::string& title, const std::vector<Row>& rows, float x = 24, float bw = 400, bool bottom = false) {
  float rh = rows.size() > 5 ? 48.f : 58.f, bh = 70 + rows.size() * rh, y0 = bottom ? 786 - bh : 24;
  Panel(x, y0, bw, bh, 26);
  Text(x + 28, y0 + 34, 22, TH().title, title, -1, TS_PLAIN | TS_HEAVY | TS_SPACED);
  for (size_t i = 0; i < rows.size(); i++) {
    float y = y0 + 88 + i * rh; const Row& r = rows[i];
    if (r.hot) RRect(x + 12, y - rh / 2 + 3, bw - 24, rh - 6, 16, Color{ 255, 255, 255, 30 }, Color{ 255, 255, 255, 30 }, BLANK, 0);
    Color d = { (unsigned char)(r.c.r * 0.72f), (unsigned char)(r.c.g * 0.72f), (unsigned char)(r.c.b * 0.72f), 255 };
    RRect(x + 24, y - 16, 32, 32, 16, r.c, d, Color{ 255, 255, 255, 230 }, 3, true);
    Text(x + 70, y + 1, rh > 50 ? 33 : 28, r.hot ? Color{ 0xff, 0xe0, 0x66, 255 } : WHITE, r.name, -1, TS_HEAVY | TS_SHADOW);
    Text(x + bw - 26, y + 1, r.val.size() > 6 ? 22 : rh > 50 ? 36 : 30, TH().accent, r.val, 1, TS_DISPLAY | TS_SHADOW);
  }
}
inline void Clock(float T, float x = 800) {
  int m = (int)ceilf(fmaxf(0, T)); char b[16]; snprintf(b, sizeof b, "%d:%02d", m / 60, m % 60); bool low = m <= 10;
  RRect(x - 100, 24, 200, 84, 42, Color{ 0, 0, 0, 70 }, Color{ 0, 0, 0, 0 }, BLANK, 0, true);
  RRect(x - 100, 18, 200, 84, 42, TH().top, TH().bot, low ? Color{ 0xff, 0x4d, 0x6d, 255 } : TH().rim, 3, true, !low && TH().gold);
  Text(x, 62, 58, low ? Color{ 0xff, 0x6b, 0x81, 255 } : WHITE, b, 0, TS_DISPLAY | TS_SHADOW);
}
inline void Hint(const std::string& s0, Color c = WHITE, float y = 841) {
  std::string s = ShortHint(s0);
  static std::string last; static double since = 0; double now = GetTime();
  if (s != last) { last = s; since = now; }
  float age = (float)(now - since), shown = 4.5f;
  if (age > shown + 0.4f) return;
  float k = age < 0.25f ? 1 - EaseOutQ(age / 0.25f) : age > shown ? (age - shown) / 0.4f : 0;   // slide up in, slide down out
  y += 140 * k * k;
  float fs = 42, w = Measure(s, fs, TS_DISPLAY) + 100; if (w > 1500) { fs *= 1500 / w; w = 1500; }
  y -= 4;
  Panel(800 - w / 2, y - 40, w, 80, 40);
  Text(800, y + 1, fs, c, s, 0, TS_DISPLAY | TS_SHADOW);
}
// a name plate in the player's colour
inline void Tag(float x, float y, Color col, const std::string& name, float size = 28) {
  float w = Measure(name, size, TS_HEAVY) + 30, h = size + 14;
  // stack instead of overlapping when players bunch up
  for (int tries = 0; tries < 6; tries++) {
    bool hit = false;
    for (auto& r : TagRects()) if (x - w / 2 < r.x + r.width && x + w / 2 > r.x && y - h / 2 < r.y + r.height && y + h / 2 > r.y) { y = r.y - h / 2 - 4; hit = true; }
    if (!hit) break;
  }
  TagRects().push_back({ x - w / 2, y - h / 2, w, h });
  Color d = { (unsigned char)(col.r * 0.7f), (unsigned char)(col.g * 0.7f), (unsigned char)(col.b * 0.7f), 255 };
  RRect(x - w / 2, y - h / 2 + 3, w, h, h / 2, Color{ 0, 0, 0, 80 }, Color{ 0, 0, 0, 80 }, BLANK, 0);
  RRect(x - w / 2, y - h / 2, w, h, h / 2, col, d, Color{ 255, 255, 255, 235 }, 3, true);
  Text(x, y + 1, size, WHITE, name, 0, TS_HEAVY | TS_SHADOW);
}
}  // namespace hud
struct Spring { float x = 0, v = 0; void To(float t, float dt, float k = 190, float d = 21) { int n = 4; float h = dt / n; for (int i = 0; i < n; i++) { v += (k * (t - x) - d * v) * h; x += v * h; } } };
inline float EaseOut(float t) { t = Clamp(t, 0, 1); return 1 - (1 - t) * (1 - t) * (1 - t); }
inline float EaseBack(float t) { t = Clamp(t, 0, 1); float c = 1.7f; return 1 + (c + 1) * powf(t - 1, 3) + c * powf(t - 1, 2); }
// cartoon character: hop = jump height, cheer 0..1 raises arms
void DrawToon(Vector3 pos, float yaw, Color c, float t, float hop = 0, float cheer = 0, float scale = 1);
// rigged character models (people.cpp)
bool DrawPerson(Vector3 pos, float yaw, Color c, float t, float hop, float cheer, float sc, float k, unsigned hsh);
void InstallFileLoader();
bool DrawModel(const char* file, Vector3 pos, float yaw, float height, Color tint = WHITE, Color force = BLANK);
bool HaveFile(const std::string& n);
// make the next DrawToon hold something: its right hand goes to 'hand' (toon-local space, before scale)
struct ToonHold { bool on = false; Vector3 hand{}; };
extern ToonHold g_hold;
extern float g_toonK;  // people are drawn this much bigger than their nominal scale (1 = life size; party games use 1.4)
Vector3 ToonPoint(Vector3 pos, float yaw, float sc, float hop, Vector3 local);  // toon-local point → world
struct WinterBG {
  struct Flake { Vector3 p; float s, w; };
  std::vector<Flake> snow; std::vector<Vector4> trees, hills; float t = 0;
  void Init(); void Update(float dt); void Draw(const Camera3D& cam, bool sky = true);
};
struct Confetti {
  struct C { Vector3 p, v, r, w; Color c; float life; };
  std::vector<C> l;
  void Spawn(Vector3 at, int n, float spread, float up);
  void Update(float dt); void Draw();
};

// ---------- games ----------
struct Game {
  std::vector<Player>* ps = nullptr;
  float shake = 0;
  virtual ~Game() {}
  virtual void Start() = 0;
  virtual void Update(float dt) = 0;
  virtual void Draw() = 0;
  virtual void Act(int pid, const std::string& kind, int i, float p) {}
};
std::unique_ptr<Game> MakeSkyBrawl();
std::unique_ptr<Game> MakeSumo();
std::unique_ptr<Game> MakeSnowball();
std::unique_ptr<Game> MakeChairs();
std::unique_ptr<Game> MakeRedLight();
std::unique_ptr<Game> MakeSimon();
std::unique_ptr<Game> MakeTug();
std::unique_ptr<Game> MakeHockey();
std::unique_ptr<Game> MakeTag();
std::unique_ptr<Game> MakeEights();
std::unique_ptr<Game> MakeFour();
std::unique_ptr<Game> MakeSnakes();
std::unique_ptr<Game> MakeBlackjack();
std::unique_ptr<Game> MakeBingo();
std::unique_ptr<Game> MakeDice();
std::unique_ptr<Game> MakeMemory();
std::unique_ptr<Game> MakeHiLo();
std::unique_ptr<Game> MakeWord();
std::unique_ptr<Game> MakeTrivia();
std::unique_ptr<Game> MakeClash();
std::unique_ptr<Game> MakeCount();
std::unique_ptr<Game> MakeMath();
std::unique_ptr<Game> MakeLikely();
std::unique_ptr<Game> MakeLowest();
std::unique_ptr<Game> MakeRPS();
std::unique_ptr<Game> MakeBomb();
// newer games register themselves: static RegGame r("id", [] { return std::unique_ptr<Game>(new X); });
using GameMaker = std::unique_ptr<Game> (*)();
struct RegGame { RegGame(const char* id, GameMaker f); };
std::unique_ptr<Game> MakeRegistered(const std::string& id);
#include "json.h"
