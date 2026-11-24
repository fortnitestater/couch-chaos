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
void Text(float x, float y, float size, Color c, const std::string& s, int align = 0); // align: 0 center, -1 left, 1 right
void Layout(int pid, const std::string& json);  // pid -1 = everyone: what each phone shows
void Sfx(const char* name);
void Shake(float amount);
void Finish(const std::string& ranking);
void Banner(const std::string& text, int ms = 1600, const char* color = "#ffffff");
void Buzz(int pid, int ms);        // "pid:stat;pid:stat" best first

// ---------- players ----------
struct Player { int id; std::string name; Color color; bool bot; Vector2 stick{}; bool a = false, b = false, pa = false, pb = false; };

// ---------- small helpers ----------
inline float frand(float a, float b) { return a + (b - a) * (float)rand() / (float)RAND_MAX; }
inline float angDiff(float a, float b) { float d = fmodf(b - a + PI * 3, PI * 2) - PI; return d; }
inline Color Hex(unsigned v, unsigned char a = 255) { return Color{ (unsigned char)(v >> 16), (unsigned char)(v >> 8), (unsigned char)v, a }; }
inline Color Shade(Color c, float k) { auto f = [&](unsigned char v) { return (unsigned char)Clamp(k < 0 ? v * (1 + k) : v + (255 - v) * k, 0, 255); }; return Color{ f(c.r), f(c.g), f(c.b), c.a }; }
Color ParseColor(const std::string& hex);

// ---------- lit 3D rendering ----------
struct Lit {
  Shader shader{}; Material mat{}; int locLight, locCam, locFog, locFogD, locAmb;
  void Init();
  void Begin(const Camera3D& cam, Vector3 fogColor, float fogDensity);
  void Draw(const Mesh& m, Matrix t, Color c);
};
extern Lit lit;
struct Prims { Mesh cube, sphere, cyl, cone, plane; void Init(); };
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
