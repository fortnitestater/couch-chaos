// SUMO BLOBS — shove everyone off a shrinking ice floe into the freezing sea (3 rounds: normal, ice, mega bounce).
#include "cc.h"
#include "touch.h"

namespace {
constexpr float SC = 1 / 40.f;   // field px -> world units
struct Blob { int pid; float x, y, vx = 0, vy = 0, r = 36, m = 1, cd = 0, fall = 0, face, squish = 0, sink = 0; bool alive = true, heavy = false; };

struct Sumo : Game {
  std::vector<Blob> bl; std::vector<int> pts, outOrder; Particles3D fx; Confetti conf; Camera3D cam{};
  int round = 0, mod = 0; float R = 420, t = 0, el = 0, time = 0, lastTick = 4, bannerT = 0; std::string phase;
  Player& P(int i) { return (*ps)[i]; }
  Vector3 W3(float x, float y, float h = 0) { return { (x - 800) * SC, h, (y - 480) * SC }; }
  TouchBox TB{ 350, 30, 1250, 930 }; SnapTimer snap;
  void Snap(float dt) {
    if (!snap.Tick(dt)) return;
    for (size_t i = 0; i < ps->size(); i++) {
      if (P((int)i).bot) continue; std::vector<TouchObj> o; Vector2 c = TB.P(800, 480);
      o.push_back({ c.x, c.y, TB.R(R), Hex(0xe7f5ff), 11 });
      for (auto& b : bl) if (b.alive) { Vector2 q = TB.P(b.x, b.y); o.push_back({ q.x, q.y, TB.R(b.r), b.heavy ? Shade(P(b.pid).color, -0.35f) : P(b.pid).color, b.pid == (int)i ? 1 : 3 }); }
      bool me = false; for (auto& b : bl) if (b.pid == (int)i) me = b.alive;
      SendSnap((int)i, o, "ROUND " + std::to_string(round) + " / 3 · " + (me ? "push them into the sea!" : "splash! wait for the next round"));
    }
  }

  void Start() override {
    pts.assign(ps->size(), 0);
    Layout(-1, TouchLayout("sea", 1, "go", "HEAVY", "Tap where to slide. DASH = shove, hold HEAVY = rock.", -1, "DASH"));
    cam.up = { 0, 1, 0 }; cam.fovy = 42; cam.projection = CAMERA_PERSPECTIVE;
    Next();
  }
  void Next() {
    round++; mod = round - 1; R = 420; phase = "count"; t = 3.5f; lastTick = 4; el = 0; outOrder.clear(); bl.clear();
    int n = (int)ps->size();
    for (int i = 0; i < n; i++) { float a = (float)i / n * 2 * PI - PI / 2; Blob b; b.pid = i; b.x = 800 + cosf(a) * 270; b.y = 480 + sinf(a) * 270; b.face = a + PI; bl.push_back(b); }
    const char* names[] = { "", " · ICE!", " · MEGA BOUNCE!" };
    Banner("ROUND " + std::to_string(round) + names[mod], 2200, "#ffd43b");
  }
  struct In { float x, y; bool a, b; };
  In Bot(Blob& b) {
    Blob* t = nullptr; float bd = 1e9f;
    for (auto& o : bl) { if (&o == &b || !o.alive) continue; float d = hypotf(o.x - b.x, o.y - b.y); if (d < bd) { bd = d; t = &o; } }
    float dx = 0, dy = 0; if (t) { dx = (t->x - b.x) / bd; dy = (t->y - b.y) / bd; }
    float ex = b.x - 800, ey = b.y - 480, ed = fmaxf(1, hypotf(ex, ey));
    if (ed > R * 0.55f) { float k = 2 * ed / fmaxf(1, R); dx -= ex / ed * k; dy -= ey / ed * k; }
    return { dx, dy, false, t && bd < 140 && frand(0, 1) < 0.04f };
  }
  void Update(float dt) override {
    time += dt; fx.Update(dt); conf.Update(dt); Snap(dt);
    for (auto& b : bl) if (!b.alive) { b.sink += dt; b.x += b.vx * dt * 0.3f; b.y += b.vy * dt * 0.3f; }
    if (phase == "count") {
      t -= dt;
      if (ceilf(t) < lastTick && t > 0) { lastTick = ceilf(t); Sfx("tick"); }
      if (t <= 0) { phase = "fight"; Sfx("go"); Banner("PUSH!", 900, "#ff4d6d"); }
      return;
    }
    if (phase == "over") { t -= dt; if (t <= 0) { if (round >= 3) End(); else Next(); } return; }
    el += dt;
    R = fmaxf(0, R - dt * (el > 30 ? 14 : 6));
    float fr = mod == 1 ? 0.45f : 2.4f, e = mod == 2 ? 1.7f : 0.9f;
    for (auto& b : bl) {
      if (!b.alive) continue;
      Player& p = P(b.pid);
      In in = p.bot ? Bot(b) : In{ p.stick.x, p.stick.y, p.a, p.b };
      if (!p.bot && p.touch) {  // slide toward the finger: steer the velocity so you stop on the spot
        Vector2 t = TB.W(TouchAt(p)); float dx = t.x - b.x, dy = t.y - b.y, d = hypotf(dx, dy);
        float want = d < 10 ? 0 : fminf(1, d / 140) * 430, wx = d > 0.1f ? dx / d * want : 0, wy = d > 0.1f ? dy / d * want : 0;
        float ix = (wx - b.vx) / 260, iy = (wy - b.vy) / 260, il = hypotf(ix, iy); if (il > 1) { ix /= il; iy /= il; }
        in.x = ix; in.y = iy;
        if (in.b && d > 10) { in.x = dx / d; in.y = dy / d; }  // DASH goes where you point
      }
      b.heavy = in.a; b.m = b.heavy ? 3.5f : 1;
      float ax = in.x, ay = in.y, l = hypotf(ax, ay); if (l > 1) { ax /= l; ay /= l; }
      float acc = (b.heavy ? 450.f : 1500.f) * (mod == 1 ? 0.6f : 1);
      b.vx += ax * acc * dt; b.vy += ay * acc * dt; b.cd -= dt;
      if (in.b && b.cd <= 0 && !b.heavy) {
        float dx = ax, dy = ay;
        if (hypotf(dx, dy) < 0.2f) { float v = hypotf(b.vx, b.vy); if (v > 1) { dx = b.vx / v; dy = b.vy / v; } else { dx = cosf(b.face); dy = sinf(b.face); } }
        float dl = fmaxf(0.001f, hypotf(dx, dy)); b.vx += dx / dl * 950; b.vy += dy / dl * 950; b.cd = 1.2f;
        fx.Burst(W3(b.x, b.y, 0.6f), 16, p.color, 6, 0.5f, 0.18f); Sfx("dash");
      }
      float k = expf(-fr * dt); b.vx *= k; b.vy *= k; b.x += b.vx * dt; b.y += b.vy * dt;
      if (hypotf(b.vx, b.vy) > 40) b.face = atan2f(b.vy, b.vx);
      b.squish = fmaxf(0, b.squish - dt * 3);
      if (hypotf(b.vx, b.vy) > 260 && frand(0, 1) < 0.5f) fx.Puff(W3(b.x, b.y, 0.1f), mod == 1 ? Hex(0xd0ebff, 200) : Hex(0xffffff, 170), 0.18f, 0.5f, { 0, 0.6f, 0 });
    }
    for (size_t i = 0; i < bl.size(); i++) for (size_t j = i + 1; j < bl.size(); j++) {
      Blob &a = bl[i], &b = bl[j]; if (!a.alive || !b.alive) continue;
      float dx = b.x - a.x, dy = b.y - a.y, d = hypotf(dx, dy), mn = a.r + b.r;
      if (d < mn && d > 0.01f) {
        float nx = dx / d, ny = dy / d, ia = 1 / a.m, ib = 1 / b.m, ov = mn - d;
        a.x -= nx * ov * ia / (ia + ib); a.y -= ny * ov * ia / (ia + ib); b.x += nx * ov * ib / (ia + ib); b.y += ny * ov * ib / (ia + ib);
        float rv = (b.vx - a.vx) * nx + (b.vy - a.vy) * ny;
        if (rv < 0) {
          float jj = -(1 + e) * rv / (ia + ib);
          a.vx -= jj * ia * nx; a.vy -= jj * ia * ny; b.vx += jj * ib * nx; b.vy += jj * ib * ny;
          if (jj > 250) {
            a.squish = b.squish = fminf(1, jj / 1200);
            Sfx("bonk"); fx.Burst(W3(a.x + nx * a.r, a.y + ny * a.r, 0.9f), 12, WHITE, 7, 0.4f, 0.16f); fx.Burst(W3(a.x + nx * a.r, a.y + ny * a.r, 0.9f), 6, Hex(0xffd43b), 5, 0.35f, 0.2f);
            Shake(fminf(12, jj / 150)); Buzz(a.pid, 50); Buzz(b.pid, 50);
          }
        }
      }
    }
    for (auto& b : bl) {
      if (b.alive && hypotf(b.x - 800, b.y - 480) > R + b.r * 0.2f) {
        b.alive = false; b.sink = 0; outOrder.push_back(b.pid);
        fx.Burst(W3(b.x, b.y, 0), 40, Hex(0xa5d8ff), 9, 0.9f, 0.22f, 9); fx.Burst(W3(b.x, b.y, 0), 18, WHITE, 6, 0.7f, 0.3f, 6);
        Sfx("splash"); Shake(10); Buzz(b.pid, 500);
      }
    }
    int left = 0; Blob* last = nullptr; for (auto& b : bl) if (b.alive) { left++; last = &b; }
    if (left <= 1) {
      phase = "over"; t = 3;
      for (size_t i = 0; i < outOrder.size(); i++) pts[outOrder[i]] += (int)i;
      if (last) { pts[last->pid] += (int)bl.size(); Banner(P(last->pid).name + " WINS ROUND!", 2200, "#ffd43b"); conf.Spawn(W3(last->x, last->y, 4), 160, 2.5f, 4); Sfx("win"); }
      else Banner("DOUBLE SPLASH!", 2000, "#ffd43b");
    }
  }
  void End() {
    std::vector<int> o; for (size_t i = 0; i < ps->size(); i++) o.push_back((int)i);
    std::sort(o.begin(), o.end(), [&](int a, int b) { return pts[a] > pts[b]; });
    std::string r; for (int i : o) r += std::to_string(i) + ":" + std::to_string(pts[i]) + " pts;";
    phase = "done"; Finish(r);
  }

  void Draw() override {
    cam.position = { sinf(time * 0.2f) * 1.5f, 19, 15.5f }; cam.target = { 0, 0, 0.8f };
    UI::Sky(Hex(0x0c2a5b), Hex(0x2c7be5));
    Vector3 fog = { 0.07f, 0.2f, 0.42f };
    lit.Begin(cam, fog, 0.012f);
    BeginMode3D(cam);
    // freezing sea with drifting ice chunks
    lit.Pattern(PAT_WATER); lit.Draw(prim.plane, TRS({ 0, -0.6f, 0 }, { 0, 0, 0 }, { 300, 1, 300 }), Hex(0x1864ab)); lit.Pattern(PAT_NONE);
    for (int i = 0; i < 24; i++) {
      float a = i * 2.4f + time * 0.05f * (i % 2 ? 1 : -1), d = 14 + (i * 37 % 13);
      lit.Draw(prim.cyl, TRS({ cosf(a) * d, -0.62f + 0.05f * sinf(time + i), sinf(a) * d * 0.7f }, { 0, a, 0 }, { 1.2f + i % 3, 0.25f, 0.9f + i % 2 }), Hex(0xe7f5ff));
    }
    for (int i = 0; i < 40; i++) { float a = i * 0.157f + time * 0.1f; float rr = R * SC + 0.4f + 0.15f * sinf(time * 3 + i); lit.Draw(prim.sphere, TRS({ cosf(a) * rr, -0.55f, sinf(a) * rr }, { 0, 0, 0 }, { 0.5f, 0.12f, 0.5f }), Hex(0xffffff, 200)); }
    // floe: thick ice disc that shrinks
    float rr = fmaxf(0.01f, R * SC);
    Color top = mod == 1 ? Hex(0xd0f0ff) : mod == 2 ? Hex(0xffc9e9) : Hex(0xf1f7ff);
    lit.Draw(prim.cyl, TRS({ 0, -1.0f, 0 }, { 0, 0, 0 }, { rr * 2, 0.88f, rr * 2 }), Hex(0x74c0fc));
    lit.Pattern(PAT_ICE); lit.Draw(prim.cyl, TRS({ 0, -0.1f, 0 }, { 0, 0, 0 }, { rr * 2, 0.1f, rr * 2 }), top); lit.Pattern(PAT_NONE);
    lit.Draw(prim.cyl, TRS({ 0, -0.1f, 0 }, { 0, 0, 0 }, { rr * 1.1f, 0.14f, rr * 1.1f }), mod == 2 ? Hex(0xfcc2d7) : Hex(0xd0ebff));
    // warning ring when it shrinks fast
    if (el > 30) DrawCircle3D({ 0, 0.03f, 0 }, rr - 0.3f, { 1, 0, 0 }, 90, Fade(RED, 0.4f + 0.4f * sinf(time * 8)));
    for (auto& b : bl) {
      Player& p = P(b.pid); float s = b.r / 36.f * 1.55f;
      Vector3 pos = W3(b.x, b.y, 0);
      if (!b.alive) { pos.y = -b.sink * 2.2f; if (b.sink > 1.2f) continue; }
      Color c = b.heavy ? Hex(0x868e96) : p.color;
      lit.Draw(prim.cyl, TRS({ pos.x + 0.15f, 0.06f, pos.z + 0.1f }, { 0, 0, 0 }, { 1.3f * s, 0.01f, 1.3f * s }), Hex(0x0b3d66, 70));
      float hop = phase == "over" && b.alive ? fabsf(sinf(time * 8)) * 0.6f : b.squish * 0.2f;
      DrawToon(pos, -b.face + PI / 2, c, time + b.pid, hop, phase == "over" && b.alive ? 1 : b.alive ? 0 : 1, s * (1 + b.squish * 0.15f));
      if (b.heavy) DrawSphereWires({ pos.x, pos.y + 0.9f * s, pos.z }, 0.85f * s, 6, 8, Fade(Hex(0xced4da), 0.5f));
    }
    fx.Draw();
    conf.Draw();
    EndMode3D();
    // HUD
    for (auto& b : bl) if (b.alive) {
      Vector2 s = GetWorldToScreen(W3(b.x, b.y, 3.4f), cam);
      Text(s.x, s.y, 20, P(b.pid).color, P(b.pid).name);
      if (b.cd > 0) { UI::Box({ s.x - 20, s.y + 16, 40, 6 }, 3, Hex(0x000000, 90)); UI::Box({ s.x - 20, s.y + 16, 40 * (1 - b.cd / 1.2f), 6 }, 3, Hex(0xffffff, 220)); }
    }
    std::vector<int> o; for (size_t i = 0; i < ps->size(); i++) o.push_back((int)i);
    std::sort(o.begin(), o.end(), [&](int a, int b) { return pts[a] > pts[b]; });
    UI::Box({ 18, 18, 250, 48 + o.size() * 34.f }, 18, Hex(0x0b1020, 160));
    Text(36, 42, 18, Hex(0xadb5bd), "ROUND " + std::to_string(round) + " / 3", -1);
    for (size_t i = 0; i < o.size(); i++) { float y = 76 + i * 34.f; DrawCircle(44, (int)y, 8, P(o[i]).color); Text(60, y, 20, WHITE, P(o[i]).name, -1); Text(250, y, 22, Hex(0xffd43b), std::to_string(pts[o[i]]), 1); }
    if (phase == "count" && t > 0 && t <= 3) { float f = t - floorf(t); Text(800, 420, 160 + 80 * f, Fade(WHITE, fminf(1, f * 2 + 0.3f)), std::to_string((int)ceilf(t)), 0, TS_DISPLAY | TS_SHADOW); }
    if (el > 30 && phase == "fight") Text(800, 860, 24, Hex(0xff8787), "⚠️ The ice is melting fast!");
  }
};
}  // namespace
std::unique_ptr<Game> MakeSumo() { return std::make_unique<Sumo>(); }
