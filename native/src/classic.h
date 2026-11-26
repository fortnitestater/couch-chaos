// Shared bits for the joystick arena games: world mapping, movement helpers, scenery and the Classic base.
#pragma once
#include "cc.h"
#include "touch.h"
#include "mesh.h"
#include <functional>

namespace {
constexpr float SC = 1 / 40.f;
inline Vector3 W3(float x, float y, float h = 0) { return { (x - 800) * SC, h, (y - 480) * SC }; }
inline Vector2 Norm(Vector2 v) { float l = hypotf(v.x, v.y); return l > 1 ? Vector2{ v.x / l, v.y / l } : v; }
inline Vector2 Toward(float fx, float fy, float tx, float ty) { float dx = tx - fx, dy = ty - fy, d = hypotf(dx, dy); return d < 1 ? Vector2{ 0, 0 } : Vector2{ dx / d, dy / d }; }

struct Mover { float x = 0, y = 0, vx = 0, vy = 0, face = 0; };

void Steer(Mover& m, Vector2 in, float speed, float dt, float accel = 10) {
  in = Norm(in); float k = fminf(1, accel * dt);
  m.vx += (in.x * speed - m.vx) * k; m.vy += (in.y * speed - m.vy) * k;
  m.x += m.vx * dt; m.y += m.vy * dt;
  if (hypotf(m.vx, m.vy) > 30) m.face = atan2f(m.vy, m.vx);
}
void Separate(Mover& a, Mover& b, float r) {
  float dx = b.x - a.x, dy = b.y - a.y, d = hypotf(dx, dy);
  if (d < r && d > 0.01f) { float o = (r - d) / 2; a.x -= dx / d * o; a.y -= dy / d * o; b.x += dx / d * o; b.y += dy / d * o; }
}

// sky + ground + a ring of pine trees around the play area
struct Scenery {
  std::vector<Vector4> trees, props, peaks; bool snow = true;
  void Init(float rx, float rz, bool snowy, int n = 60) {
    snow = snowy; trees.clear(); props.clear(); peaks.clear();
    for (int i = 0; i < n; i++) { float a = frand(0, 2 * PI); trees.push_back({ cosf(a) * frand(rx, rx + 16), sinf(a) * frand(rz, rz + 12), frand(1.3f, 2.7f), frand(0, 6) }); }
    // rocks, bushes, snowmen / flowers scattered between the play area and the trees
    for (int i = 0; i < 46; i++) { float a = frand(0, 2 * PI); float k = frand(0.0f, 1.0f); props.push_back({ cosf(a) * (rx - 1 + k * 7), sinf(a) * (rz - 1 + k * 6), frand(0.5f, 1.2f), (float)(rand() % 4) }); }
    // a ring of far-off mountains for the horizon
    for (int i = 0; i < 30; i++) { float a = PI + 0.05f + i * (PI - 0.1f) / 29; float d = frand(78, 105); peaks.push_back({ cosf(a) * d, sinf(a) * d * 0.8f, frand(20, 36), frand(18, 34) }); }
    hills.clear(); far.clear();
    for (int i = 0; i < 26; i++) { float a = PI + 0.06f + i * (PI - 0.12f) / 25; float d = frand(rz + 26, rz + 44); hills.push_back({ cosf(a) * d * 1.25f, sinf(a) * d, frand(14, 26), frand(3.5f, 8) }); }
    for (int i = 0; i < 150; i++) { float a = PI + 0.05f + frand(0, PI - 0.1f); float d = frand(rz + 13, rz + 40); far.push_back({ cosf(a) * d * 1.3f, sinf(a) * d, frand(2.2f, 4.6f), frand(0, 6) }); }
  }
  std::vector<Vector4> hills, far;
  void Begin(Camera3D& cam) {
    if (snow) UI::Sky(Hex(0x5fa0e6), Hex(0xcfe4fa)); else UI::Sky(Hex(0x2f8fe8), Hex(0xbfe3ff));
    lit.Begin(cam, snow ? Vector3{ 0.74f, 0.84f, 0.97f } : Vector3{ 0.66f, 0.83f, 0.98f }, 0.0085f);
    BeginMode3D(cam);
    lit.Pattern(snow ? PAT_SNOW : PAT_GRASS);
    lit.Draw(prim.plane, TRS({ 0, 0, 0 }, { 0, 0, 0 }, { 300, 1, 300 }), snow ? Hex(0xe4eefb) : Hex(0x5fb04f));
    for (auto& h : hills) lit.Draw(prim.sphere, TRS({ h.x, -h.w * 0.35f, h.y }, { 0, h.x, 0 }, { h.z * 2.4f, h.w * 2, h.z * 1.7f }), snow ? Hex(0xdce8f8) : Hex(0x55a548));
    lit.Pattern(PAT_NONE);
    for (auto& tr : far) {   // dense, dark tree line that frames the arena
      float s = tr.z;
      DrawProp(snow ? ::props.pineSnow : ::props.pine, TRS({ tr.x, 0, tr.y }, { 0, tr.w, 0 }, { 1.7f * s, (2.1f + 0.5f * sinf(tr.w * 3)) * s, 1.7f * s }), Color{ 205, 220, 212, 255 });
    }
    for (auto& p : peaks) {
      lit.Draw(prim.cone, TRS({ p.x, -2, p.y }, { 0, p.x, 0 }, { p.z * 2, p.w, p.z * 2 }), snow ? Hex(0x9fb4dc) : Hex(0x6c8fb8));
      lit.Draw(prim.cone, TRS({ p.x, -2 + p.w * 0.58f, p.y }, { 0, p.x, 0 }, { p.z * 0.84f, p.w * 0.42f, p.z * 0.84f }), Hex(0xf8fbff));
    }
  }
  void Props(float t) {
    for (auto& p : props) {
      float s = p.z; int k = (int)p.w;
      if (k == 0) {  // mossy / snowy rock
        lit.Pattern(PAT_STONE);
        lit.Draw(prim.sphere, TRS({ p.x, 0.1f * s, p.y }, { 0.3f, p.x, 0.2f }, { 1.3f * s, 0.75f * s, s }), Hex(0x8d96a6));
        lit.Pattern(PAT_NONE);
        if (snow) lit.Draw(prim.sphere, TRS({ p.x, 0.36f * s, p.y }, { 0.3f, p.x, 0.2f }, { 1.0f * s, 0.32f * s, 0.8f * s }), WHITE);
      } else if (k == 1) {  // round bush
        Color g = snow ? Hex(0x2b8a3e) : Hex(0x37b24d);
        for (int j = 0; j < 3; j++) lit.Draw(prim.sphere, TRS({ p.x + (j - 1) * 0.35f * s, 0.35f * s, p.y + (j == 1 ? 0.15f : 0) * s }, { 0, 0, 0 }, { 0.8f * s, 0.7f * s, 0.8f * s }), Shade(g, j * 0.08f));
        if (snow) lit.Draw(prim.sphere, TRS({ p.x, 0.62f * s, p.y }, { 0, 0, 0 }, { 1.1f * s, 0.25f * s, 0.7f * s }), WHITE);
        else for (int j = 0; j < 4; j++) lit.Draw(prim.sphere, TRS({ p.x + sinf(j * 2.1f) * 0.4f * s, 0.55f * s, p.y + cosf(j * 2.1f) * 0.3f * s }, { 0, 0, 0 }, { 0.12f, 0.12f, 0.12f }), Hex(0xff6b6b));
      } else if (k == 2 && snow) {  // snowman
        float b = sinf(t * 1.5f + p.x) * 0.03f;
        lit.Draw(prim.sphere, TRS({ p.x, 0.45f, p.y }, { 0, 0, 0 }, { 1.0f, 0.9f, 1.0f }), Hex(0xf8fbff));
        lit.Draw(prim.sphere, TRS({ p.x, 1.15f, p.y }, { 0, 0, b }, { 0.7f, 0.66f, 0.7f }), Hex(0xf8fbff));
        lit.Draw(prim.sphere, TRS({ p.x, 1.68f, p.y }, { 0, 0, 0 }, { 0.5f, 0.48f, 0.5f }), Hex(0xf8fbff));
        lit.Draw(prim.cone, TRS({ p.x, 1.68f, p.y + 0.22f }, { PI / 2, 0, 0 }, { 0.1f, 0.3f, 0.1f }), Hex(0xff922b));
        lit.Draw(prim.cyl, TRS({ p.x, 1.88f, p.y }, { 0, 0, 0 }, { 0.42f, 0.36f, 0.42f }), Hex(0x212529));
        lit.Draw(prim.cyl, TRS({ p.x, 1.4f, p.y }, { 0, 0, 0 }, { 0.62f, 0.1f, 0.62f }), Hex(0xe03131));
      } else {  // flowers / pebbles
        for (int j = 0; j < 5; j++) {
          float ox = sinf(j * 2.4f + p.x) * 0.6f * s, oz = cosf(j * 1.7f + p.y) * 0.6f * s;
          if (snow) lit.Draw(prim.sphere, TRS({ p.x + ox, 0.05f, p.y + oz }, { 0, 0, 0 }, { 0.25f, 0.12f, 0.2f }), Hex(0xb0bccc));
          else {
            lit.Draw(prim.cyl, TRS({ p.x + ox, 0, p.y + oz }, { 0, 0, 0 }, { 0.04f, 0.35f, 0.04f }), Hex(0x2f9e44));
            const unsigned fc[] = { 0xffd43b, 0xff8787, 0xffffff, 0xda77f2, 0x74c0fc };
            lit.Draw(prim.sphere, TRS({ p.x + ox, 0.38f, p.y + oz }, { 0, 0, 0 }, { 0.18f, 0.1f, 0.18f }), Hex(fc[j]));
          }
        }
      }
    }
  }
  void Trees(float t) {
    Props(t);
    for (auto& tr : trees) {
      float s = tr.z, sw = sinf(t + tr.w) * 0.03f;
      DrawProp(snow ? ::props.pineSnow : ::props.pine, TRS({ tr.x, 0, tr.y }, { sw, tr.w, sw }, { 1.55f * s, (2.2f + 0.4f * sinf(tr.w * 5)) * s, 1.55f * s }));
    }
  }
};

struct Classic : Game {
  Camera3D cam{}; Scenery sc; Particles3D fx; Confetti conf; float time = 0, cd = 3.5f, lastTick = 4;
  Player& P(int i) { return (*ps)[i]; }
  int N() { return (int)ps->size(); }
  void InitCam(float fov) { cam.up = { 0, 1, 0 }; cam.fovy = fov; cam.projection = CAMERA_PERSPECTIVE; }
  bool Counting(float dt, const char* go) {  // 3-2-1, true while still counting
    if (cd <= 0) return false;
    cd -= dt;
    if (ceilf(cd) < lastTick && cd > 0) { lastTick = ceilf(cd); Sfx("tick"); }
    if (cd <= 0) { Sfx("go"); Banner(go, 1000, "#51cf66"); return false; }
    return true;
  }
  void Recount() { cd = 3.5f; lastTick = 4; }
  void DrawCount() { if (cd > 0 && cd <= 3) { float f = cd - floorf(cd); Text(800, 420, 160 + 80 * f, Fade(WHITE, fminf(1, f * 2 + 0.3f)), std::to_string((int)ceilf(cd)), 0, TS_DISPLAY | TS_SHADOW); } }
  void NameTag(Vector3 at, int pid, float size = 24) { Vector2 s = GetWorldToScreen(at, cam); hud::Tag(s.x, s.y - 16, P(pid).color, P(pid).name, fmaxf(size, 28)); }
  void Board(const std::string& title, const std::vector<int>& order, std::function<std::string(int)> val) {  // sized to read from the couch
    std::vector<hud::Row> rows; for (size_t i = 0; i < order.size(); i++) rows.push_back({ P(order[i]).color, P(order[i]).name, val(order[i]), i == 0 });
    hud::Board(title, rows);
  }
  void Clock(float T, float x = 800) { hud::Clock(T, x); }
  void Hint(const std::string& s, Color c = WHITE) { hud::Hint(s, c); }
  std::vector<int> Sorted(std::function<float(int)> key) { std::vector<int> o; for (int i = 0; i < N(); i++) o.push_back(i); std::stable_sort(o.begin(), o.end(), [&](int a, int b) { return key(a) > key(b); }); return o; }
  void FinishOrder(const std::vector<int>& o, std::function<std::string(int)> stat) { std::string r; for (int i : o) r += std::to_string(i) + ":" + stat(i) + ";"; Finish(r); }
  void Shadow(Vector3, float) {}  // characters cast soft contact shadows by themselves now
};
}  // namespace
