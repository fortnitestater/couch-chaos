// SKY BRAWL — 3D dogfight over a tropical archipelago (raylib).
// Simulation runs on a flat 1600x900 field (same feel as before); everything is drawn in 3D.
#include "cc.h"
#include <map>

namespace {
constexpr float ALT = 6.0f;            // flight altitude (world units)
inline Vector3 W3(float x, float y, float alt = ALT) { return { (x - 800) / 20.f, alt, (y - 450) / 20.f }; }

enum Pow { NONE, TRIPLE, MISSILE, SHIELD, MEGA, SPEED, NPOW };
struct ItemDef { Color c; const char* s; const char* n; };
const ItemDef ITEMS[NPOW] = { {}, { Hex(0xffd43b), "3x", "TRIPLE SHOT" }, { Hex(0xff6b6b), "🚀", "HOMING MISSILES" }, { Hex(0x66d9e8), "🛡️", "SHIELD" }, { Hex(0xcc5de8), "XL", "MEGA PLANE" }, { Hex(0x51cf66), "⚡", "TURBO" } };
enum Ev { EV_NONE, METEORS, REVERSE, FRENZY, TINY };
const char* EV_NAME[] = { "", "☄️ METEOR SHOWER!", "🙃 REVERSE CONTROLS!", "🔥 BULLET FRENZY!", "🐜 TINY PLANES!" };

struct Plane {
  int pid; float x, y, ang, hp, inv, cd, mcd, energy, speed, powT, bank, flash, trail, respawn, prop = 0, roll = 0;
  bool alive; Pow pow; int kills = 0, deaths = 0;
};
struct Shot { float x, y, vx, vy, life, r; int owner, dmg; bool homing; Color c; };
struct Crate { float x, y, t; Pow k; bool dead = false; };
struct Rock { float x, y, vx, vy, r, a, va; int hp; };
struct Island { Vector3 p; float r, h; int palms; float seed; };
struct Cloud { Vector3 p; float r, s; };
struct Feed { std::string s; float t; };

struct SkyBrawl : Game {
  std::vector<Plane> pl; std::vector<Shot> shots; std::vector<Crate> items; std::vector<Rock> rocks;
  std::vector<Island> isles; std::vector<Cloud> clouds; std::vector<Feed> feed;
  Particles3D fx; Camera3D cam{};
  float T = 120, itemT = 3, eventT, evLeft = 0, rockT = 0, time = 0, camShake = 0; Ev ev = EV_NONE; bool done = false;

  Player& P(int pid) { return (*ps)[pid]; }
  float Scale(const Plane& p) { return 1.0f * (p.pow == MEGA ? 1.8f : 1) * (ev == TINY ? 0.55f : 1); }
  void Feed_(const std::string& s) { feed.insert(feed.begin(), { s, 4 }); if (feed.size() > 5) feed.resize(5); }

  void Start() override {
    eventT = frand(16, 22);
    int n = (int)ps->size();
    for (int i = 0; i < n; i++) { Plane p{}; p.pid = i; Spawn(p, (float)i / n); pl.push_back(p); }
    // archipelago
    for (int i = 0; i < 40 && isles.size() < 7; i++) { Vector3 c = { frand(-44, 44), 0, frand(-26, 26) }; bool ok = true; for (auto& o : isles) if (Vector3Distance(c, o.p) < 16) ok = false; if (ok) isles.push_back({ c, frand(2.8f, 5.5f), frand(1.2f, 3.5f), (int)frand(1, 4.9f), frand(0, 100) }); }
    for (int i = 0; i < 8; i++) clouds.push_back({ { frand(-60, 60), frand(1.0f, 2.5f), frand(-30, 30) }, frand(1.6f, 3.0f), frand(0.6f, 1.6f) });
    Layout(-1, R"({"name":"pad","stick":"x","a":"FIRE","b":"BOOST","hint":"Tilt, drag, or ← → to steer"})");
    Banner("✈️ SKY BRAWL!", 1600, "#ffffff");
    cam.fovy = 46; cam.up = { 0, 1, 0 }; cam.projection = CAMERA_PERSPECTIVE;
  }
  void Spawn(Plane& p, float f = -1) {
    float a = (f < 0 ? frand(0, 1) : f) * 2 * PI;
    p.x = 800 + cosf(a) * 340; p.y = 450 + sinf(a) * 300; p.ang = a + PI + 0.4f;
    p.hp = 3; p.alive = true; p.inv = 2; p.cd = 0; p.mcd = 0; p.energy = 1; p.speed = 300; p.pow = NONE; p.powT = 0; p.bank = 0; p.flash = 0; p.trail = 0;
  }
  Plane* Nearest(float x, float y, int notPid) {
    Plane* best = nullptr; float bd = 1e9f;
    for (auto& o : pl) { if (o.pid == notPid || !o.alive) continue; float d = hypotf(o.x - x, o.y - y); if (d < bd) { bd = d; best = &o; } }
    return best;
  }

  struct In { float x; bool a, b; };
  In Bot(Plane& p) {
    Plane* t = Nearest(p.x, p.y, p.pid);
    float tx = 0, ty = 0, td = 1e9f; bool has = false;
    if (t) { tx = t->x; ty = t->y; td = hypotf(tx - p.x, ty - p.y); has = true; }
    for (auto& it : items) { float d = hypotf(it.x - p.x, it.y - p.y); if (d < 350 && d < td) { tx = it.x; ty = it.y; td = d; has = true; } }
    if (!has) return { sinf(T + p.y) * 0.5f, false, false };
    float da = angDiff(p.ang, atan2f(ty - p.y, tx - p.x));
    float aim = t ? fabsf(angDiff(p.ang, atan2f(t->y - p.y, t->x - p.x))) : 9;
    return { Clamp(da * 2.2f, -1, 1) * 0.8f * (ev == REVERSE ? -1 : 1), aim < 0.3f && td < 650, td > 700 && p.energy > 0.5f };
  }

  void Fire(Plane& p) {
    p.cd = ev == FRENZY ? 0.06f : 0.16f;
    float sc = Scale(p); bool mega = p.pow == MEGA;
    auto mk = [&](float da, bool homing) {
      float a = p.ang + da, sp = homing ? 520.f : 980.f;
      shots.push_back({ p.x + cosf(a) * 30 * sc, p.y + sinf(a) * 30 * sc, cosf(a) * sp, sinf(a) * sp, homing ? 2.4f : 0.85f, mega ? 11.f : 5.f, p.pid, homing || mega ? 2 : 1, homing, P(p.pid).color });
    };
    if (p.pow == TRIPLE) { mk(-0.2f, false); mk(0, false); mk(0.2f, false); } else mk(0, false);
    if (p.pow == MISSILE && p.mcd <= 0) { mk(0, true); p.mcd = 0.55f; }
    // muzzle flash
    fx.Burst(W3(p.x + cosf(p.ang) * 34 * sc, p.y + sinf(p.ang) * 34 * sc), 2, Hex(0xfff3bf), 4, 0.08f, 0.35f);
    Sfx("shoot");
  }
  void Hit(Plane& p, int by, int dmg) {
    Vector3 at = W3(p.x, p.y);
    if (p.pow == SHIELD) { fx.Burst(at, 10, Hex(0x66d9e8), 8, 0.4f, 0.3f); Sfx("tink"); return; }
    p.hp -= dmg; p.flash = 0.12f;
    fx.Burst(at, 8, Hex(0xffd43b), 9, 0.35f, 0.3f); Sfx("hit"); Buzz(p.pid, 60);
    if (p.hp <= 0) {
      p.alive = false; p.respawn = 2.5f; p.deaths++;
      if (by >= 0 && by != p.pid) pl[by].kills++;
      fx.Burst(at, 46, P(p.pid).color, 16, 1.2f, 0.55f, 9); fx.Burst(at, 34, Hex(0xff922b), 12, 0.9f, 0.8f, -1); fx.Burst(at, 20, Hex(0x495057), 6, 2.0f, 1.1f, -2);
      Sfx("boom"); Shake(16); camShake = 0.6f; Buzz(p.pid, 400);
      Feed_(by >= 0 ? P(by).name + "  💥  " + P(p.pid).name : "☄️ bonked " + P(p.pid).name);
    }
  }
  void RockBoom(Rock& r) { Vector3 at = W3(r.x, r.y); fx.Burst(at, 22, Hex(0x8d6e63), 12, 0.7f, 0.5f, 6); fx.Burst(at, 14, Hex(0xff922b), 10, 0.5f, 0.6f); Sfx("boom"); Shake(5); camShake = fmaxf(camShake, 0.25f); }

  void Update(float dt) override {
    if (done) return;
    T -= dt; time += dt; fx.Update(dt); camShake = fmaxf(0, camShake - dt);
    if (T <= 0) { End(); return; }
    for (auto& c : clouds) { c.p.x += c.s * dt; if (c.p.x > 62) { c.p.x = -62; c.p.z = frand(-35, 35); } }
    for (auto& f : feed) f.t -= dt;
    feed.erase(std::remove_if(feed.begin(), feed.end(), [](const Feed& f) { return f.t <= 0; }), feed.end());
    if (ev == EV_NONE) {
      eventT -= dt;
      if (eventT <= 0 && T > 12) { ev = (Ev)(1 + rand() % 4); evLeft = 10; Banner(EV_NAME[ev], 2200, "#ff922b"); Sfx("alarm"); }
    } else { evLeft -= dt; if (evLeft <= 0) { ev = EV_NONE; eventT = frand(14, 22); } }
    itemT -= dt;
    if (itemT <= 0 && items.size() < 3) { items.push_back({ frand(120, 1480), frand(130, 800), 0, (Pow)(1 + rand() % (NPOW - 1)) }); itemT = frand(4, 7); }
    for (auto& it : items) it.t += dt;
    if (ev == METEORS) {
      rockT -= dt;
      if (rockT <= 0) { rockT = 0.3f; rocks.push_back({ frand(-150, 1450), -60, frand(60, 180), frand(260, 420), frand(20, 38), 0, frand(-3, 3), 2 }); }
    }
    for (auto& r : rocks) {
      r.x += r.vx * dt; r.y += r.vy * dt; r.a += r.va * dt;
      if (frand(0, 1) < 0.7f) fx.Puff(W3(r.x, r.y), frand(0, 1) < 0.5f ? Hex(0xff922b, 200) : Hex(0xffd43b, 200), r.r / 40.f, 0.45f, { 0, 1.5f, 0 });
    }
    for (auto& p : pl) {
      if (!p.alive) { p.respawn -= dt; if (p.respawn <= 0) Spawn(p); continue; }
      Player& who = P(p.pid);
      In in = who.bot ? Bot(p) : In{ who.stick.x, who.a, who.b };
      float turn = Clamp(in.x, -1, 1); if (ev == REVERSE) turn = -turn;
      // soft arena edge: gently turn back toward the middle instead of teleporting
      float ox = fmaxf(0, fabsf(p.x - 800) - 720), oy = fmaxf(0, fabsf(p.y - 450) - 390);
      if (ox > 0 || oy > 0) { float home = angDiff(p.ang, atan2f(450 - p.y, 800 - p.x)); turn = Clamp(turn + Clamp(home * 2, -1, 1) * fminf(1, (ox + oy) / 80), -1, 1); }
      p.bank += (turn - p.bank) * fminf(1, dt * 7);
      p.ang += p.bank * 2.8f * dt;
      bool boost = in.b && p.energy > 0.02f;
      p.energy = boost ? fmaxf(0, p.energy - dt * 0.7f) : fminf(1, p.energy + dt * 0.22f);
      float tgt = (boost ? 560.f : 310.f) * (p.pow == SPEED ? 1.45f : 1);
      p.speed += (tgt - p.speed) * fminf(1, dt * 3);
      p.x += cosf(p.ang) * p.speed * dt; p.y += sinf(p.ang) * p.speed * dt;
      p.inv = fmaxf(0, p.inv - dt); p.flash = fmaxf(0, p.flash - dt); p.mcd -= dt; p.prop += dt * (boost ? 60 : 38);
      if (p.pow) { p.powT -= dt; if (p.powT <= 0) p.pow = NONE; }
      float sc = Scale(p);
      p.trail -= dt;
      if (p.trail <= 0) {
        p.trail = 0.025f;
        float bx = p.x - cosf(p.ang) * 30 * sc, by = p.y - sinf(p.ang) * 30 * sc;
        if (boost) fx.Puff(W3(bx, by), frand(0, 1) < 0.5f ? Hex(0xffa94d, 230) : Hex(0xffe066, 230), 0.55f * sc, 0.35f);
        // wingtip vapour
        float wx = -sinf(p.ang) * 34 * sc, wy = cosf(p.ang) * 34 * sc;
        fx.Puff(W3(p.x - cosf(p.ang) * 6 + wx, p.y - sinf(p.ang) * 6 + wy, ALT + p.bank * 0.9f), Hex(0xffffff, 150), 0.16f * sc, 0.55f);
        fx.Puff(W3(p.x - cosf(p.ang) * 6 - wx, p.y - sinf(p.ang) * 6 - wy, ALT - p.bank * 0.9f), Hex(0xffffff, 150), 0.16f * sc, 0.55f);
        if (p.hp == 1) fx.Puff(W3(bx, by), Hex(0x343a40, 190), 0.5f, 0.8f, { 0, 1.2f, 0 });
      }
      p.cd -= dt;
      if (in.a && p.cd <= 0) Fire(p);
      for (auto& it : items) {
        if (!it.dead && hypotf(it.x - p.x, it.y - p.y) < 50 * sc) {
          it.dead = true; p.pow = it.k; p.powT = it.k == SHIELD ? 7 : 9;
          fx.Burst(W3(it.x, it.y), 26, ITEMS[it.k].c, 12, 0.6f, 0.35f); Sfx("pickup");
          Feed_(who.name + " got " + ITEMS[it.k].n); Buzz(p.pid, 40);
        }
      }
      for (auto& r : rocks)
        if (r.hp > 0 && p.alive && p.inv <= 0 && hypotf(r.x - p.x, r.y - p.y) < r.r + 20 * sc) { r.hp = 0; RockBoom(r); Hit(p, -1, 2); }
    }
    items.erase(std::remove_if(items.begin(), items.end(), [](const Crate& c) { return c.dead; }), items.end());
    for (auto& s : shots) {
      if (s.homing) {
        Plane* t = Nearest(s.x, s.y, s.owner);
        if (t) {
          float cur = atan2f(s.vy, s.vx), da = angDiff(cur, atan2f(t->y - s.y, t->x - s.x));
          float a = cur + Clamp(da, -4 * dt, 4 * dt), sp = hypotf(s.vx, s.vy);
          s.vx = cosf(a) * sp; s.vy = sinf(a) * sp;
        }
        fx.Puff(W3(s.x, s.y), Hex(0xf1f3f5, 200), 0.3f, 0.5f);
      }
      s.x += s.vx * dt; s.y += s.vy * dt; s.life -= dt;
      for (auto& p : pl) {
        if (p.pid == s.owner || !p.alive || p.inv > 0) continue;
        float r = 22 * Scale(p) + s.r;
        if ((p.x - s.x) * (p.x - s.x) + (p.y - s.y) * (p.y - s.y) < r * r) { s.life = 0; Hit(p, s.owner, s.dmg); break; }
      }
      for (auto& r : rocks)
        if (s.life > 0 && r.hp > 0 && hypotf(r.x - s.x, r.y - s.y) < r.r + s.r) { s.life = 0; if (--r.hp <= 0) RockBoom(r); }
    }
    shots.erase(std::remove_if(shots.begin(), shots.end(), [](const Shot& s) { return s.life <= 0; }), shots.end());
    rocks.erase(std::remove_if(rocks.begin(), rocks.end(), [](const Rock& r) { return r.hp <= 0 || r.y > 980; }), rocks.end());
  }

  void End() {
    if (done) return; done = true;
    std::vector<Plane*> o; for (auto& p : pl) o.push_back(&p);
    std::sort(o.begin(), o.end(), [](Plane* a, Plane* b) { return a->kills != b->kills ? a->kills > b->kills : a->deaths < b->deaths; });
    std::string r;
    for (auto* p : o) r += std::to_string(p->pid) + ":" + std::to_string(p->kills) + " KOs · " + std::to_string(p->deaths) + " crashes;";
    Finish(r);
  }

  // ---------------- drawing ----------------
  void Part(const Mesh& m, Matrix base, Vector3 pos, Vector3 rot, Vector3 s, Color c) { lit.Draw(m, MatrixMultiply(TRS(pos, rot, s), base), c); }
  void DrawPlane(const Plane& p) {
    float sc = Scale(p) * 2.1f;
    Color body = p.flash > 0 ? WHITE : P(p.pid).color, dark = Shade(body, -0.35f), light = Shade(body, 0.35f);
    if (p.inv > 0 && ((int)(p.inv * 10)) % 2) { body.a = 110; dark.a = 110; light.a = 110; }
    Vector3 c = W3(p.x, p.y, ALT + sinf(time * 3 + p.pid) * 0.15f);
    Matrix base = MatrixMultiply(MatrixMultiply(MatrixScale(sc, sc, sc), MatrixRotateX(p.bank * 0.75f)), MatrixRotateY(-p.ang));
    base = MatrixMultiply(base, MatrixTranslate(c.x, c.y, c.z));
    const float Z = -PI / 2;
    Part(prim.cyl, base, { -1.5f, 0, 0 }, { 0, 0, Z }, { 0.62f, 2.9f, 0.62f }, body);                 // fuselage
    Part(prim.cone, base, { 1.4f, 0, 0 }, { 0, 0, Z }, { 0.62f, 0.55f, 0.62f }, light);                // nose
    Part(prim.cyl, base, { -2.05f, 0, 0 }, { 0, 0, Z }, { 0.5f, 0.6f, 0.5f }, dark);                    // tail cone
    Part(prim.cube, base, { 0.25f, -0.05f, 0 }, { 0, 0, 0 }, { 0.95f, 0.09f, 4.1f }, body);            // wing
    Part(prim.cube, base, { 0.25f, -0.05f, 2.0f }, { 0, 0, 0 }, { 0.97f, 0.11f, 0.25f }, dark);        // wingtips
    Part(prim.cube, base, { 0.25f, -0.05f, -2.0f }, { 0, 0, 0 }, { 0.97f, 0.11f, 0.25f }, dark);
    Part(prim.cube, base, { -1.85f, 0.05f, 0 }, { 0, 0, 0 }, { 0.55f, 0.07f, 1.6f }, body);            // tailplane
    Part(prim.cube, base, { -1.9f, 0.42f, 0 }, { 0, 0, 0.25f }, { 0.55f, 0.75f, 0.08f }, dark);        // fin
    Part(prim.sphere, base, { 0.45f, 0.3f, 0 }, { 0, 0, 0 }, { 0.85f, 0.5f, 0.46f }, Hex(0xcfe8ff, body.a)); // canopy
    Part(prim.sphere, base, { 1.95f, 0, 0 }, { 0, 0, 0 }, { 0.22f, 0.22f, 0.22f }, Hex(0x343a40));     // spinner
    Part(prim.cube, base, { 2.0f, 0, 0 }, { p.prop, 0, 0 }, { 0.04f, 1.7f, 0.14f }, Hex(0x212529, 170)); // propeller
    Part(prim.cube, base, { 2.0f, 0, 0 }, { p.prop + PI / 2, 0, 0 }, { 0.04f, 1.7f, 0.14f }, Hex(0x212529, 170));
    // ground shadow
    Vector3 sh = W3(p.x, p.y, 0.06f);
    lit.Draw(prim.sphere, MatrixMultiply(MatrixMultiply(MatrixScale(4.2f * sc / 1.5f * 1.5f, 0.02f, 3.0f * sc), MatrixRotateY(-p.ang)), MatrixTranslate(sh.x + 0.9f, sh.y, sh.z + 0.8f)), Hex(0x0b3d66, 80));
    if (p.pow == SHIELD) DrawSphereWires(c, 2.4f * sc, 8, 12, Fade(Hex(0x66d9e8), 0.55f + 0.3f * sinf(time * 10)));
  }
  void DrawIsland(const Island& is) {
    Matrix I = MatrixIdentity();
    float foam = 1.08f + 0.04f * sinf(time * 1.6f + is.seed);
    lit.Draw(prim.cyl, TRS({ is.p.x, 0.02f, is.p.z }, { 0, 0, 0 }, { is.r * 2 * foam * 1.12f, 0.05f, is.r * 2 * foam * 1.12f }), Hex(0x7fd3f5, 200));
    lit.Draw(prim.cyl, TRS({ is.p.x, 0.04f, is.p.z }, { 0, 0, 0 }, { is.r * 2 * foam, 0.06f, is.r * 2 * foam }), Hex(0xffffff, 200));
    lit.Draw(prim.cyl, TRS({ is.p.x, 0, is.p.z }, { 0, 0, 0 }, { is.r * 2, 0.35f, is.r * 2 }), Hex(0xf4d58d));          // beach
    lit.Draw(prim.cone, TRS({ is.p.x, 0.3f, is.p.z }, { 0, is.seed, 0 }, { is.r * 1.6f, is.h, is.r * 1.6f }), Hex(0x4caf50)); // hill
    lit.Draw(prim.cone, TRS({ is.p.x, 0.3f + is.h * 0.55f, is.p.z }, { 0, 0, 0 }, { is.r * 0.75f, is.h * 0.45f, is.r * 0.75f }), Hex(0x388e3c));
    for (int k = 0; k < is.palms; k++) {
      float a = is.seed + k * 2.1f, d = is.r * 0.82f; Vector3 b = { is.p.x + cosf(a) * d, 0.3f, is.p.z + sinf(a) * d };
      float sway = sinf(time * 1.3f + a) * 0.08f;
      lit.Draw(prim.cyl, TRS(b, { 0, 0, 0.18f + sway }, { 0.22f, 2.0f, 0.22f }), Hex(0x8d6e63));
      Vector3 top = { b.x - 0.36f, b.y + 2.0f, b.z };
      for (int f = 0; f < 5; f++) lit.Draw(prim.cube, TRS(top, { 0.5f + sway, f * 1.25f + a, 0 }, { 0.3f, 0.06f, 1.8f }), Hex(0x2f9e44));
    }
    (void)I;
  }
  void DrawCloud(const Cloud& c, unsigned char a) {
    for (int k = 0; k < 4; k++) {
      Vector3 q = { c.p.x + (k - 1.5f) * c.r * 0.6f, c.p.y + (k % 2) * c.r * 0.25f, c.p.z + ((k * 37) % 5 - 2) * 0.25f };
      lit.Draw(prim.sphere, TRS(q, { 0, 0, 0 }, { c.r * (k % 2 ? 1.5f : 1.15f), c.r * (k % 2 ? 1.0f : 0.8f), c.r * 1.1f }), Hex(0xffffff, a));
    }
  }

  void Draw() override {
    bool met = ev == METEORS;
    Color sky = met ? Hex(0xff8a5c) : Hex(0x8fd0ff);
    ClearBackground(sky);
    float sk = camShake * camShake * 1.4f;
    cam.position = { sinf(time * 0.15f) * 1.5f + frand(-sk, sk), 66 + frand(-sk, sk), 27 };
    cam.target = { sinf(time * 0.15f) * 0.8f, 0, 1.2f };
    Vector3 fogC = { sky.r / 255.f, sky.g / 255.f, sky.b / 255.f };
    lit.Begin(cam, fogC, 0.0085f);
    BeginMode3D(cam);
    // ocean with gentle swell bands
    lit.Pattern(PAT_WATER); lit.Draw(prim.plane, TRS({ 0, 0, 0 }, { 0, 0, 0 }, { 400, 1, 400 }), met ? Hex(0xc0583a) : Hex(0x1c7ed6)); lit.Pattern(PAT_NONE);
    for (int i = -12; i <= 12; i++) {
      float z = i * 4.5f + fmodf(time * 0.8f, 4.5f);
      lit.Draw(prim.plane, TRS({ sinf(time * 0.3f + i) * 3, 0.01f, z }, { 0, 0.05f * sinf(i * 1.7f), 0 }, { 200, 1, 0.5f + 0.25f * sinf(time + i) }), met ? Hex(0xd9704f, 45) : Hex(0x74c0fc, 45));
    }
    for (auto& is : isles) DrawIsland(is);
    for (auto& c : clouds) DrawCloud(c, 170);
    // crates
    for (auto& it : items) {
      Vector3 c = W3(it.x, it.y, ALT + sinf(it.t * 3) * 0.4f);
      lit.Draw(prim.cube, TRS(c, { 0.3f, it.t * 1.6f, 0.2f }, { 1.9f, 1.9f, 1.9f }), ITEMS[it.k].c);
      lit.Draw(prim.cube, TRS(c, { 0.3f, it.t * 1.6f, 0.2f }, { 2.0f, 0.35f, 2.0f }), WHITE);
      DrawSphereEx(c, 1.9f + 0.25f * sinf(it.t * 6), 8, 10, Fade(ITEMS[it.k].c, 0.18f));
      Vector3 sh = W3(it.x, it.y, 0.07f);
      lit.Draw(prim.cyl, TRS(sh, { 0, 0, 0 }, { 2.2f, 0.02f, 2.2f }), Hex(0x0b3d66, 80));
    }
    // meteors
    for (auto& r : rocks) {
      Vector3 c = W3(r.x, r.y);
      lit.Draw(prim.sphere, TRS(c, { r.a, r.a * 0.7f, 0 }, { r.r / 11.f, r.r / 13.f, r.r / 12.f }), Hex(0x6d4c41));
      DrawSphereEx(c, r.r / 9.f, 8, 10, Fade(Hex(0xff922b), 0.35f));
    }
    for (auto& p : pl) if (p.alive) DrawPlane(p);
    // bullets: glowing tracers
    for (auto& s : shots) {
      Vector3 a = W3(s.x, s.y), b = W3(s.x - s.vx * 0.035f, s.y - s.vy * 0.035f);
      float r = s.r / 22.f;
      DrawCylinderEx(b, a, r * 0.5f, r, 6, Fade(s.c, 0.55f));
      DrawSphereEx(a, r * 1.1f, 6, 8, s.homing ? Hex(0xff6b6b) : Hex(0xfff9db));
    }
    BeginBlendMode(BLEND_ADDITIVE); fx.Draw(); EndBlendMode();
    EndMode3D();

    // ---------- HUD (2D) ----------
    for (auto& p : pl) {
      if (!p.alive) continue;
      Vector2 s = GetWorldToScreen(W3(p.x, p.y, ALT + 2.6f * Scale(p)), cam);
      Text(s.x, s.y - 16, 19, P(p.pid).color, P(p.pid).name);
      for (int i = 0; i < 3; i++) DrawRectangleRounded({ s.x - 24 + i * 17.f, s.y, 14, 6 }, 1, 4, i < p.hp ? Hex(0x51cf66) : Hex(0x000000, 90));
      DrawRectangleRounded({ s.x - 24, s.y + 9, 48, 4 }, 1, 4, Hex(0x000000, 70));
      DrawRectangleRounded({ s.x - 24, s.y + 9, 48 * p.energy, 4 }, 1, 4, Hex(0xffd43b));
      if (p.pow) Text(s.x + 40, s.y - 16, 20, WHITE, ITEMS[p.pow].s);
    }
    for (auto& it : items) { Vector2 s = GetWorldToScreen(W3(it.x, it.y, ALT + 2.2f), cam); Text(s.x, s.y, 20, WHITE, ITEMS[it.k].s); }
    int m = (int)ceilf(fmaxf(0, T));
    DrawRectangleRounded({ 730, 14, 140, 58 }, 0.5f, 8, Hex(0x0b1020, 150));
    char buf[16]; snprintf(buf, sizeof buf, "%d:%02d", m / 60, m % 60);
    Text(800, 44, 40, m <= 10 ? Hex(0xff4d6d) : WHITE, buf);
    // scoreboard
    std::vector<const Plane*> o; for (auto& p : pl) o.push_back(&p);
    std::sort(o.begin(), o.end(), [](const Plane* a, const Plane* b) { return a->kills > b->kills; });
    float bh = 46 + o.size() * 34.f;
    DrawRectangleRounded({ 18, 18, 250, bh }, 0.12f, 8, Hex(0x0b1020, 160));
    Text(36, 42, 18, Hex(0xadb5bd), "KOs", -1);
    for (size_t i = 0; i < o.size(); i++) {
      float y = 76 + i * 34.f; const Player& q = P(o[i]->pid);
      DrawCircle(44, (int)y, 8, q.color);
      Text(60, y, 20, WHITE, q.name, -1);
      Text(250, y, 22, Hex(0xffd43b), std::to_string(o[i]->kills), 1);
    }
    if (ev) { std::string s = std::string(EV_NAME[ev]) + " " + std::to_string((int)ceilf(evLeft)); Text(800, 100, 26, Hex(0xffd43b), s); }
    for (size_t i = 0; i < feed.size(); i++) Text(1580, 36 + i * 34.f, 20, Fade(WHITE, fminf(1, feed[i].t)), feed[i].s, 1);
    int k = 0;
    for (auto& p : pl) if (!p.alive && !P(p.pid).bot) Text(800, 860 - 30 * k++, 22, P(p.pid).color, P(p.pid).name + " respawning…");
  }
};
}  // namespace

std::unique_ptr<Game> MakeSkyBrawl() { return std::make_unique<SkyBrawl>(); }
