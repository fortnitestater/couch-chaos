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

constexpr int W = 1600, H = 900;

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
struct Player { int id; std::string name; Color color; bool bot; Vector2 stick{}; bool a = false, b = false, pa = false, pb = false; };

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
struct Spring { float x = 0, v = 0; void To(float t, float dt, float k = 190, float d = 21) { int n = 4; float h = dt / n; for (int i = 0; i < n; i++) { v += (k * (t - x) - d * v) * h; x += v * h; } } };
inline float EaseOut(float t) { t = Clamp(t, 0, 1); return 1 - (1 - t) * (1 - t) * (1 - t); }
inline float EaseBack(float t) { t = Clamp(t, 0, 1); float c = 1.7f; return 1 + (c + 1) * powf(t - 1, 3) + c * powf(t - 1, 2); }
// cartoon character: hop = jump height, cheer 0..1 raises arms
void DrawToon(Vector3 pos, float yaw, Color c, float t, float hop = 0, float cheer = 0, float scale = 1);
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
