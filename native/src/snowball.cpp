// SNOWBALL FIGHT — arcing snowballs, snow forts to hide behind, most hits in 75 s wins.
#include "cc.h"

namespace {
constexpr float SC = 1 / 40.f;
const Rectangle BOX = { 60, 100, 1480, 770 };
struct Kid { int pid; float x, y, vx = 0, vy = 0, r = 32, face, aim, cd = 0, tcd = 0, stun = 0, inv = 0, wind = 0; int hits = 0, bonks = 0; float bt = 0; Vector2 bi{}; bool ba = false, bb = false; };
struct Ball { float x, y, h, vx, vy, vh, life; int owner; };
struct Fort { float x, y, r; };

struct Snowball : Game {
  std::vector<Kid> k; std::vector<Ball> balls; std::vector<Fort> forts; std::vector<Vector4> trees;
  Particles3D fx; Confetti conf; Camera3D cam{}; WinterBG bg;
  float T = 75, cdT = 3.5f, el = 0, time = 0, lastTick = 4, overT = 0; bool over = false, done = false;
  Player& P(int i) { return (*ps)[i]; }
  Vector3 W3(float x, float y, float h = 0) { return { (x - 800) * SC, h, (y - 485) * SC }; }

  void Start() override {
    int n = (int)ps->size();
    for (int i = 0; i < n; i++) { float a = (float)i / n * 2 * PI - PI / 2, rad = 770 * 0.32f; Kid b; b.pid = i; b.x = 800 + cosf(a) * rad * 1.5f; b.y = 485 + sinf(a) * rad; b.face = b.aim = a + PI; k.push_back(b); }
    for (int tries = 0; tries < 200 && forts.size() < 6; tries++) {
      Fort f = { frand(BOX.x + 180, BOX.x + BOX.width - 180), frand(BOX.y + 140, BOX.y + BOX.height - 140), frand(45, 70) };
      bool ok = true; for (auto& o : forts) if (hypotf(o.x - f.x, o.y - f.y) < o.r + f.r + 90) ok = false;
      for (auto& b : k) if (hypotf(b.x - f.x, b.y - f.y) < f.r + 80) ok = false;
      if (ok) forts.push_back(f);
    }
    for (int i = 0; i < 70; i++) { float a = frand(0, 2 * PI); float x = cosf(a) * frand(23, 40), z = sinf(a) * frand(13.5f, 26); trees.push_back({ x, z, frand(1.4f, 2.8f), frand(0, 6) }); }
    Layout(-1, R"({"name":"pad","stick":"xy","a":"THROW","b":"DASH","hint":"Move, then THROW where you face. Hide behind forts!"})");
    cam.up = { 0, 1, 0 }; cam.fovy = 44; cam.projection = CAMERA_PERSPECTIVE;
    bg.Init();
  }
  struct In { float x, y; bool a, b; };
  In Input(Kid& b) {
    Player& p = P(b.pid);
    if (!p.bot) return { p.stick.x, p.stick.y, p.a, p.b };
    if (el - b.bt > 0.15f) {
      b.bt = el; Kid* t = nullptr; float bd = 1e9f;
      for (auto& o : k) if (&o != &b) { float d = hypotf(o.x - b.x, o.y - b.y); if (d < bd) { bd = d; t = &o; } }
      if (!t) { b.bi = { 0, 0 }; b.ba = false; }
      else {
        float dx = t->x - b.x, dy = t->y - b.y, d = fmaxf(1, hypotf(dx, dy)), keep = d < 260 ? -1.f : 1.f;
        b.bi = { dx / d * keep * 0.8f + sinf(el * 2 + b.x) * 0.3f, dy / d * keep * 0.8f + cosf(el * 2 + b.y) * 0.3f };
        b.ba = d < 600 && frand(0, 1) < 0.5f; b.aim = atan2f(dy, dx);
      }
    }
    return { b.bi.x, b.bi.y, b.ba, false };
  }
  void Update(float dt) override {
    time += dt; fx.Update(dt); conf.Update(dt); bg.Update(dt);
    if (done) return;
    if (cdT > 0) { cdT -= dt; if (ceilf(cdT) < lastTick && cdT > 0) { lastTick = ceilf(cdT); Sfx("tick"); } if (cdT <= 0) { Sfx("go"); Banner("THROW!", 900, "#4dabff"); } return; }
    if (over) { overT -= dt; if (overT <= 0) End(); }
    else { el += dt; T -= dt; if (T <= 0) { over = true; overT = 2.5f; Banner("TIME!", 1500, "#ffd43b"); Sfx("whistle"); } }
    for (auto& b : k) {
      In in = over ? In{ 0, 0, false, false } : Input(b);
      b.stun = fmaxf(0, b.stun - dt); b.inv = fmaxf(0, b.inv - dt); b.cd -= dt; b.tcd -= dt; b.wind = fmaxf(0, b.wind - dt * 4);
      float ax = in.x, ay = in.y, l = hypotf(ax, ay); if (l > 1) { ax /= l; ay /= l; }
      if (b.stun > 0) { ax = ay = 0; }
      b.vx += ax * 1400 * dt; b.vy += ay * 1400 * dt;
      if (in.b && b.cd <= 0 && b.stun <= 0) { float d = fmaxf(0.2f, hypotf(ax, ay)); float dx = l > 0.2f ? ax / d : cosf(b.face), dy = l > 0.2f ? ay / d : sinf(b.face); b.vx += dx * 900; b.vy += dy * 900; b.cd = 1.1f; Sfx("dash"); fx.Burst(W3(b.x, b.y, 0.3f), 14, WHITE, 5, 0.5f, 0.2f); }
      float f = expf(-3.2f * dt); b.vx *= f; b.vy *= f; b.x += b.vx * dt; b.y += b.vy * dt;
      if (hypotf(b.vx, b.vy) > 40) b.face = atan2f(b.vy, b.vx);
      if (l > 0.3f && !P(b.pid).bot) b.aim = atan2f(ay, ax);
      b.x = Clamp(b.x, BOX.x + b.r, BOX.x + BOX.width - b.r); b.y = Clamp(b.y, BOX.y + b.r, BOX.y + BOX.height - b.r);
      for (auto& ft : forts) { float dx = b.x - ft.x, dy = b.y - ft.y, d = fmaxf(1, hypotf(dx, dy)), mn = ft.r + b.r; if (d < mn) { b.x = ft.x + dx / d * mn; b.y = ft.y + dy / d * mn; float vn = (b.vx * dx + b.vy * dy) / d; if (vn < 0) { b.vx -= vn * dx / d; b.vy -= vn * dy / d; } } }
      if (hypotf(b.vx, b.vy) > 250 && frand(0, 1) < 0.4f) fx.Puff(W3(b.x, b.y, 0.1f), Hex(0xffffff, 200), 0.15f, 0.5f, { 0, 0.5f, 0 });
      if (in.a && b.tcd <= 0 && b.stun <= 0 && !over) {
        float a = b.aim; b.tcd = 0.45f; b.wind = 1;
        // lob: flight ~0.75 s over ~600 px, peaks ~1.6 m
        balls.push_back({ b.x + cosf(a) * (b.r + 12), b.y + sinf(a) * (b.r + 12), 1.5f, cosf(a) * 760 + b.vx * 0.3f, sinf(a) * 760 + b.vy * 0.3f, 3.2f, 1.1f, b.pid });
        Sfx("whoosh");
      }
    }
    for (size_t i = 0; i < k.size(); i++) for (size_t j = i + 1; j < k.size(); j++) {
      Kid &a = k[i], &b = k[j]; float dx = b.x - a.x, dy = b.y - a.y, d = hypotf(dx, dy), mn = a.r + b.r;
      if (d < mn && d > 0.01f) { float nx = dx / d, ny = dy / d, ov = (mn - d) / 2; a.x -= nx * ov; a.y -= ny * ov; b.x += nx * ov; b.y += ny * ov; float rv = (b.vx - a.vx) * nx + (b.vy - a.vy) * ny; if (rv < 0) { float jj = -1.8f * rv / 2; a.vx -= jj * nx; a.vy -= jj * ny; b.vx += jj * nx; b.vy += jj * ny; } }
    }
    for (auto& s : balls) {
      s.x += s.vx * dt; s.y += s.vy * dt; s.vh -= 9.0f * dt; s.h = fmaxf(0, s.h + s.vh * dt); s.life -= dt;
      if (s.h <= 0.05f) { s.life = 0; fx.Burst(W3(s.x, s.y, 0.1f), 10, WHITE, 4, 0.5f, 0.15f, 6); }
      if (s.x < BOX.x || s.x > BOX.x + BOX.width || s.y < BOX.y || s.y > BOX.y + BOX.height) s.life = 0;
      for (auto& f : forts) if (s.life > 0 && hypotf(s.x - f.x, s.y - f.y) < f.r && s.h < f.r * SC * 1.1f) { s.life = 0; fx.Burst(W3(s.x, s.y, s.h), 14, WHITE, 6, 0.5f, 0.17f, 6); Sfx("tink"); }
      for (auto& b : k) {
        if (s.life <= 0 || b.pid == s.owner || b.inv > 0) continue;
        if (hypotf(s.x - b.x, s.y - b.y) < b.r + 10 && s.h < 2.2f) {
          s.life = 0; b.stun = 0.6f; b.inv = 0.8f; b.vx += s.vx * 0.5f; b.vy += s.vy * 0.5f; b.bonks++;
          k[s.owner].hits++;
          fx.Burst(W3(s.x, s.y, s.h), 28, WHITE, 8, 0.6f, 0.2f, 7); fx.Burst(W3(s.x, s.y, s.h), 6, P(s.owner).color, 5, 0.5f, 0.2f);
          Sfx("bonk"); Shake(5); Buzz(b.pid, 120);
        }
      }
    }
    balls.erase(std::remove_if(balls.begin(), balls.end(), [](const Ball& b) { return b.life <= 0; }), balls.end());
  }
  void End() {
    if (done) return; done = true;
    std::vector<int> o; for (auto& b : k) o.push_back(b.pid);
    std::sort(o.begin(), o.end(), [&](int a, int b) { return k[a].hits != k[b].hits ? k[a].hits > k[b].hits : k[a].bonks < k[b].bonks; });
    std::string r; for (int i : o) r += std::to_string(i) + ":" + std::to_string(k[i].hits) + " hits · got bonked " + std::to_string(k[i].bonks) + ";";
    Finish(r);
  }
  void Draw() override {
    cam.position = { sinf(time * 0.15f) * 1.2f, 21, 17 }; cam.target = { 0, 0, 1.3f };
    UI::Sky(Hex(0x9cc8f5), Hex(0xe3f1ff));
    lit.Begin(cam, { 0.86f, 0.92f, 1.0f }, 0.009f);
    BeginMode3D(cam);
    lit.Draw(prim.plane, TRS({ 0, 0, 0 }, { 0, 0, 0 }, { 300, 1, 300 }), Hex(0xeef5ff));
    lit.Draw(prim.cube, TRS({ 0, 0.03f, 0 }, { 0, 0, 0 }, { BOX.width * SC, 0.06f, BOX.height * SC }), Hex(0xffffff));
    // fence posts
    for (int i = 0; i <= 36; i++) { float x = BOX.x + BOX.width * i / 36; for (float y : { BOX.y, BOX.y + BOX.height }) lit.Draw(prim.cyl, TRS(W3(x, y, 0), { 0, 0, 0 }, { 0.12f, 0.6f, 0.12f }), Hex(0x8d6e63)); }
    for (int i = 0; i <= 20; i++) { float y = BOX.y + BOX.height * i / 20; for (float x : { BOX.x, BOX.x + BOX.width }) lit.Draw(prim.cyl, TRS(W3(x, y, 0), { 0, 0, 0 }, { 0.12f, 0.6f, 0.12f }), Hex(0x8d6e63)); }
    for (auto& tr : trees) {
      float s = tr.z;
      lit.Draw(prim.cyl, TRS({ tr.x, 0, tr.y }, { 0, 0, 0 }, { 0.2f * s, 0.5f * s, 0.2f * s }), Hex(0x5c3d2e));
      for (int i = 0; i < 3; i++) { float y = 0.4f * s + i * 0.45f * s, r = (1.1f - i * 0.28f) * s; lit.Draw(prim.cone, TRS({ tr.x, y, tr.y }, { 0, 0, 0 }, { r, 0.75f * s, r }), Hex(0x2b8a3e)); lit.Draw(prim.cone, TRS({ tr.x, y + 0.42f * s, tr.y }, { 0, 0, 0 }, { r * 0.5f, 0.34f * s, r * 0.5f }), WHITE); }
    }
    for (auto& f : forts) {
      Vector3 c = W3(f.x, f.y); float r = f.r * SC;
      lit.Draw(prim.sphere, TRS(c, { 0, 0, 0 }, { r * 2, r * 1.3f, r * 2 }), Hex(0xf8fbff));
      for (int i = 0; i < 8; i++) { float a = i * PI / 4; lit.Draw(prim.sphere, TRS({ c.x + cosf(a) * r * 0.95f, 0.15f, c.z + sinf(a) * r * 0.95f }, { 0, 0, 0 }, { 0.6f, 0.45f, 0.6f }), Hex(0xe7f5ff)); }
    }
    for (auto& b : k) {
      Vector3 pos = W3(b.x, b.y); Player& p = P(b.pid);
      lit.Draw(prim.cyl, TRS({ pos.x + 0.1f, 0.08f, pos.z + 0.1f }, { 0, 0, 0 }, { 1.4f, 0.01f, 1.4f }), Hex(0x4a6fa5, 60));
      float wob = b.stun > 0 ? sinf(time * 30) * 0.3f : 0;
      DrawToon(pos, PI / 2 - b.aim + wob, (b.inv > 0 && (int)(b.inv * 12) % 2) ? WHITE : p.color, time + b.pid, over ? fabsf(sinf(time * 7 + b.pid)) * 0.5f : 0, b.wind, 1.45f);
      // aim arrow on the snow
      Vector3 a0 = W3(b.x + cosf(b.aim) * 46, b.y + sinf(b.aim) * 46, 0.04f), a1 = W3(b.x + cosf(b.aim) * 80, b.y + sinf(b.aim) * 80, 0.04f);
      DrawCylinderEx(a0, a1, 0.12f, 0.02f, 6, Fade(p.color, 0.85f));
      if (b.stun > 0) for (int i = 0; i < 3; i++) { float a = time * 6 + i * 2.1f; DrawSphere({ pos.x + cosf(a) * 0.5f, 2.6f, pos.z + sinf(a) * 0.5f }, 0.1f, Hex(0xffd43b)); }
    }
    for (auto& s : balls) {
      lit.Draw(prim.sphere, TRS(W3(s.x, s.y, s.h + 0.2f), { 0, 0, 0 }, { 0.42f, 0.42f, 0.42f }), WHITE);
      lit.Draw(prim.cyl, TRS(W3(s.x, s.y, 0.08f), { 0, 0, 0 }, { 0.35f, 0.01f, 0.35f }), Hex(0x4a6fa5, 70));
    }
    fx.Draw(); conf.Draw();
    for (auto& f : bg.snow) DrawCube({ f.p.x, f.p.y * 0.6f, f.p.z * 0.9f + 4 }, 0.08f, 0.08f, 0.08f, WHITE);
    EndMode3D();
    for (auto& b : k) { Vector2 s = GetWorldToScreen(W3(b.x, b.y, 3.6f), cam); Text(s.x, s.y, 19, P(b.pid).color, P(b.pid).name); }
    std::vector<int> o; for (auto& b : k) o.push_back(b.pid);
    std::sort(o.begin(), o.end(), [&](int a, int b) { return k[a].hits > k[b].hits; });
    UI::Box({ 18, 18, 250, 48 + o.size() * 34.f }, 18, Hex(0x0b1020, 160));
    Text(36, 42, 18, Hex(0xadb5bd), "HITS", -1);
    for (size_t i = 0; i < o.size(); i++) { float y = 76 + i * 34.f; DrawCircle(44, (int)y, 8, P(o[i]).color); Text(60, y, 20, WHITE, P(o[i]).name, -1); Text(250, y, 22, Hex(0xffd43b), std::to_string(k[o[i]].hits), 1); }
    int m = (int)ceilf(fmaxf(0, T)); char buf[16]; snprintf(buf, sizeof buf, "%d:%02d", m / 60, m % 60);
    UI::Box({ 730, 14, 140, 58 }, 29, Hex(0x0b1020, 150)); Text(800, 44, 40, m <= 10 ? Hex(0xff4d6d) : WHITE, buf);
    if (cdT > 0 && cdT <= 3) { float f = cdT - floorf(cdT); Text(800, 420, 160 + 80 * f, Fade(WHITE, fminf(1, f * 2 + 0.3f)), std::to_string((int)ceilf(cdT)), 0, TS_DISPLAY | TS_SHADOW); }
  }
};
}  // namespace
std::unique_ptr<Game> MakeSnowball() { return std::make_unique<Snowball>(); }
