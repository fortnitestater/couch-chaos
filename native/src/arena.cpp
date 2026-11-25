// ARENA GAMES in 3D (joystick + one button): Hot Potato, Paint Wars, Coin Rush, Frozen Lake, Ice Hockey,
// Target Blast and Light Trails. Same characters, lighting and scenery as the classics.
// Made easier than the old versions: snappier steering, bigger touch ranges, longer warnings, aim assist.
#include "classic.h"
#include "kit.h"
#include "mesh.h"

namespace {
using kit::I;

struct Arena : Classic {
  struct K : Mover { bool alive = true; float dashT = 0, dcd = 0, stun = 0, inv = 0, fade = 1, score = 0, speedMul = 1, squish = 0, bt = 0; int team = 0; Vector2 goal{}; float goalT = 0; };
  std::vector<K> k; std::vector<int> outOrder; float T = 60, el = 0, overT = 0; bool over = false, clockOn = true, fin = false, collide = true;
  float X0 = 150, X1 = 1450, Y0 = 175, Y1 = 815, speed = 340, Rad = 30, groundY = 0; std::string boardTitle = "SCORE";
  virtual Vector2 Bot(int i, bool& dash) { dash = false; return { 0, 0 }; }
  virtual void OnBump(int a, int b, float imp) {}
  virtual void OnDash(int i) {}
  virtual void Rules(float dt) {}
  virtual bool IsOver() { return clockOn && T <= 0; }
  virtual std::vector<int> Rank() { return Sorted([&](int i) { return k[i].score; }); }
  virtual std::string Stat(int i) { return I((int)roundf(k[i].score)); }
  virtual std::string WinText() { auto o = Rank(); return P(o[0]).name + " WINS!"; }
  void Spawn(float rx = 300, float ry = 210) {
    k.clear();
    for (int i = 0; i < N(); i++) { float a = (float)i / N() * 2 * PI - PI / 2; K m; m.x = (X0 + X1) / 2 + cosf(a) * rx; m.y = (Y0 + Y1) / 2 + sinf(a) * ry; m.face = a + PI; k.push_back(m); }
  }
  int Alive() { int n = 0; for (auto& m : k) n += m.alive; return n; }
  void Eliminate(int i, const char* why) {
    if (!k[i].alive) return;
    k[i].alive = false; outOrder.push_back(i);
    fx.Burst(W3(k[i].x, k[i].y, 1), 30, P(i).color, 8, 0.9f, 0.22f, 3); Sfx("boom"); Shake(8); Buzz(i, 400);
    Banner(P(i).name + " — " + why, 1200, "#ff6b6b");
  }
  // survivors first (by score), then by who lasted longest
  std::vector<int> SurvivorRank() {
    std::vector<int> o; for (int i = 0; i < N(); i++) if (k[i].alive) o.push_back(i);
    std::stable_sort(o.begin(), o.end(), [&](int a, int b) { return k[a].score > k[b].score; });
    for (int j = (int)outOrder.size() - 1; j >= 0; j--) o.push_back(outOrder[j]);
    return o;
  }
  Vector2 BotSeek(int i, float tx, float ty, bool flee = false) {
    K& m = k[i]; Vector2 d = Toward(m.x, m.y, tx, ty);
    if (flee) { d = { -d.x, -d.y }; float cx = (X0 + X1) / 2 - m.x, cy = (Y0 + Y1) / 2 - m.y, cl = fmaxf(1, hypotf(cx, cy)); d.x += cx / cl * 0.7f; d.y += cy / cl * 0.7f; }
    float dist = hypotf(tx - m.x, ty - m.y); if (!flee && dist < 30) { d.x *= dist / 30; d.y *= dist / 30; }
    return d;
  }
  void Update(float dt) override {
    time += dt; fx.Update(dt); conf.Update(dt);
    if (Counting(dt, "GO!")) return;
    if (over) {
      overT -= dt; for (auto& m : k) { m.vx *= 0.9f; m.vy *= 0.9f; }
      if (overT <= 0 && !fin) { fin = true; auto o = Rank(); FinishOrder(o, [&](int i) { return Stat(i); }); }
      return;
    }
    el += dt; if (clockOn) T -= dt;
    for (int i = 0; i < N(); i++) {
      K& m = k[i]; if (!m.alive) { m.fade = fmaxf(0, m.fade - dt * 1.5f); continue; }
      Player& p = P(i); Vector2 in = p.stick; bool dash = (p.a && !p.pa) || (p.b && !p.pb);
      if (p.bot) in = Bot(i, dash);
      m.inv -= dt; m.squish = fmaxf(0, m.squish - dt * 3);
      if (m.stun > 0) { m.stun -= dt; in = { 0, 0 }; dash = false; }
      if (dash && m.dcd <= 0) { m.dashT = 0.3f; m.dcd = 1.0f; Sfx("dash"); fx.Burst(W3(m.x, m.y, 0.5f), 10, p.color, 5, 0.4f, 0.15f); OnDash(i); }
      m.dashT -= dt; m.dcd -= dt;
      Steer(m, in, speed * m.speedMul * (m.dashT > 0 ? 2.0f : 1), dt, m.dashT > 0 ? 14 : 9);
      if (m.x < X0 + Rad) { m.x = X0 + Rad; m.vx = fabsf(m.vx) * 0.4f; } if (m.x > X1 - Rad) { m.x = X1 - Rad; m.vx = -fabsf(m.vx) * 0.4f; }
      if (m.y < Y0 + Rad) { m.y = Y0 + Rad; m.vy = fabsf(m.vy) * 0.4f; } if (m.y > Y1 - Rad) { m.y = Y1 - Rad; m.vy = -fabsf(m.vy) * 0.4f; }
    }
    if (collide)
      for (int i = 0; i < N(); i++) for (int j = i + 1; j < N(); j++) {
        K& a = k[i]; K& b = k[j]; if (!a.alive || !b.alive) continue;
        float dx = b.x - a.x, dy = b.y - a.y, d = hypotf(dx, dy), mn = Rad * 2;
        if (d < mn && d > 0.01f) {
          float nx = dx / d, ny = dy / d, ov = (mn - d) / 2; a.x -= nx * ov; a.y -= ny * ov; b.x += nx * ov; b.y += ny * ov;
          float rv = (b.vx - a.vx) * nx + (b.vy - a.vy) * ny;
          if (rv < 0) { float j2 = -rv * 0.9f; a.vx -= j2 * nx; a.vy -= j2 * ny; b.vx += j2 * nx; b.vy += j2 * ny; if (j2 > 260) { a.squish = b.squish = fminf(1, j2 / 900); Sfx("bonk"); } }
          OnBump(i, j, rv < 0 ? -rv : 0);
        }
      }
    Rules(dt);
    if (IsOver() && !over) {
      over = true; overT = 3.0f; auto o = Rank();
      conf.Spawn(W3(k[o[0]].x, k[o[0]].y, 3), 140, 4, 7); Sfx("win"); Banner(WinText(), 2400, kit::Hx(P(o[0]).color).c_str());
    }
  }
  // ---- drawing helpers ----
  virtual bool Sticks() { return false; }  // players carry a hockey stick
  void Toons(std::function<Color(int)> ring = nullptr, std::function<float(int)> cheerF = nullptr) {
    auto o = over ? Rank() : std::vector<int>{};
    for (int i = 0; i < N(); i++) {
      K& m = k[i]; if (!m.alive && m.fade <= 0) continue;
      Vector3 pos = W3(m.x, m.y); float sp = hypotf(m.vx, m.vy);
      if (ring) { Color c = ring(i); if (c.a) lit.Draw(prim.cyl, TRS({ pos.x, 0.04f, pos.z }, { 0, 0, 0 }, { 1.05f, 0.03f, 1.05f }), c); }
      bool flash = m.inv > 0 && (int)(time * 12) % 2;
      float cheer = over && !o.empty() && o[0] == i ? 1.f : cheerF ? cheerF(i) : 0;
      float s = 1.3f * (m.alive ? 1 : m.fade);
      Vector3 hl = { -0.46f, 0.72f, 0.36f }; if (Sticks()) { g_hold.on = true; g_hold.hand = hl; }
      float hop = m.stun > 0 ? 0 : fabsf(sinf(time * 13 + i)) * 0.28f * fminf(1, sp / 200) + (cheer > 0 ? fabsf(sinf(time * 8)) * 0.5f : 0);
      Vector3 tp = { pos.x, groundY + (m.alive ? 0 : (1 - m.fade) * -1.5f), pos.z }; float yaw = PI / 2 - m.face;
      DrawToon(tp, yaw, flash ? WHITE : P(i).color, time + i, hop, cheer, s);
      if (Sticks()) { g_hold.on = false; float swing = m.dashT > 0 ? sinf(m.dashT * 10) * 0.6f : 0;
        Vector3 h = ToonPoint(tp, yaw, s, hop, hl), tip = ToonPoint(tp, yaw, s, 0, { -0.2f + swing, 0.02f, 1.25f }), fc = ToonPoint(tp, yaw, s, 0, { -1.2f, 0.72f, 0.36f });
        DrawProp(props.hockeyStick, RacketXf(h, Vector3Subtract(tip, h), Vector3Subtract(fc, h), Vector3Distance(tip, h) / 1.45f)); }
    }
  }
  void Names() { for (int i = 0; i < N(); i++) if (k[i].alive) NameTag({ W3(k[i].x, k[i].y).x, 3.4f, W3(k[i].x, k[i].y).z }, i, 18); }
  void Fence(Color post = WHITE) {
    int nx = 26, nz = 14;
    for (int i = 0; i <= nx; i++) for (float y : { Y0 - 30, Y1 + 30 }) lit.Draw(prim.cube, TRS(W3(X0 - 30 + (X1 - X0 + 60) * i / nx, y, 0.35f), { 0, 0, 0 }, { 0.15f, 0.7f, 0.15f }), post);
    for (int i = 0; i <= nz; i++) for (float x : { X0 - 30, X1 + 30 }) lit.Draw(prim.cube, TRS(W3(x, Y0 - 30 + (Y1 - Y0 + 60) * i / nz, 0.35f), { 0, 0, 0 }, { 0.15f, 0.7f, 0.15f }), post);
    for (float y : { Y0 - 30, Y1 + 30 }) lit.Draw(prim.cube, TRS(W3(800, y, 0.6f), { 0, 0, 0 }, { (X1 - X0 + 60) * SC, 0.1f, 0.1f }), post);
    for (float x : { X0 - 30, X1 + 30 }) lit.Draw(prim.cube, TRS(W3(x, (Y0 + Y1) / 2, 0.6f), { 0, 0, 0 }, { 0.1f, 0.1f, (Y1 - Y0 + 60) * SC }), post);
  }
  void Floor(Color c, int pat) { lit.Pattern(pat); lit.Draw(prim.cube, TRS(W3(800, (Y0 + Y1) / 2, 0.01f), { 0, 0, 0 }, { (X1 - X0 + 60) * SC, 0.02f, (Y1 - Y0 + 60) * SC }), c); lit.Pattern(PAT_NONE); }
  void ArenaCam(float h = 24, float z = 17.5f) { cam.position = { sinf(time * 0.1f) * 1.0f, h, z }; cam.target = { 0, 0, 1.0f }; }
  void HUD() { Board(boardTitle, Rank(), [&](int i) { return Stat(i); }); if (clockOn) Clock(T); }
};

// ===================================================================================== HOT POTATO
struct Potato : Arena {
  int holder = -1; float fuse = 0, passCd = 0, tickT = 0, wait = 0; Vector3 boomAt{};
  void Start() override {
    InitCam(44); sc.Init(30, 20, false); clockOn = false; speed = 330; boardTitle = "STILL STANDING"; Spawn();
    Layout(-1, kit::Pad("xy", "DASH", "", "Got the bomb? Touch someone to pass it on before it blows!"));
    NewBomb();
  }
  void NewBomb() {
    std::vector<int> al; for (int i = 0; i < N(); i++) if (k[i].alive) al.push_back(i);
    if (al.size() < 2) return;
    holder = kit::Pick(al); fuse = frand(10, 16); passCd = 1.0f; tickT = 0;
    Banner("💣 " + P(holder).name + " has the bomb!", 1400, "#ff922b"); Buzz(holder, 300);
  }
  void Rules(float dt) override {
    for (int i = 0; i < N(); i++) { k[i].speedMul = i == holder ? 1.2f : 1; k[i].score = k[i].alive ? el : k[i].score; }
    if (wait > 0) { wait -= dt; if (wait <= 0) NewBomb(); return; }
    if (holder < 0) return;
    passCd -= dt; fuse -= dt; tickT -= dt;
    if (tickT <= 0) { tickT = fuse < 3 ? 0.22f : fuse < 6 ? 0.45f : 0.8f; Sfx("tick"); }
    if (fuse <= 0) {
      int h = holder; holder = -1; boomAt = W3(k[h].x, k[h].y, 1);
      fx.Burst(boomAt, 60, Hex(0xff922b), 10, 1.2f, 0.3f, 3); fx.Burst(boomAt, 30, Hex(0xffd43b), 7, 0.9f, 0.25f, 2); Shake(18);
      Eliminate(h, "BOOM!");
      for (int i = 0; i < N(); i++) if (k[i].alive) { float dx = k[i].x - k[h].x, dy = k[i].y - k[h].y, d = fmaxf(1, hypotf(dx, dy)); if (d < 300) { k[i].vx += dx / d * 800; k[i].vy += dy / d * 800; } }
      wait = 2.0f;
    }
  }
  void OnBump(int a, int b, float) override {
    if (passCd > 0 || holder < 0) return;
    int to = a == holder ? b : b == holder ? a : -1; if (to < 0) return;
    int from = holder; holder = to; passCd = 0.8f; k[to].stun = 0.25f; Sfx("pickup"); Buzz(to, 200);
    fx.Burst(W3(k[to].x, k[to].y, 2.5f), 14, Hex(0xff922b), 5, 0.5f, 0.15f);
    (void)from;
  }
  bool IsOver() override { return Alive() <= 1 && wait <= 0.1f; }
  std::vector<int> Rank() override { return SurvivorRank(); }
  std::string Stat(int i) override { return k[i].alive ? "survived!" : "💥 " + I((int)k[i].score) + "s"; }
  Vector2 Bot(int i, bool& dash) override {
    dash = false; if (holder < 0) return { sinf(time + i) * 0.5f, cosf(time * 1.3f + i) * 0.5f };
    if (i == holder) { int best = -1; float bd = 1e9f; for (int j = 0; j < N(); j++) if (j != i && k[j].alive) { float d = hypotf(k[j].x - k[i].x, k[j].y - k[i].y); if (d < bd) { bd = d; best = j; } } if (best < 0) return { 0, 0 }; dash = bd < 200 && frand(0, 1) < 0.05f; return BotSeek(i, k[best].x, k[best].y); }
    float d = hypotf(k[holder].x - k[i].x, k[holder].y - k[i].y);
    dash = d < 140 && frand(0, 1) < 0.05f;
    Vector2 v = BotSeek(i, k[holder].x, k[holder].y, true); return { v.x * (d < 420 ? 1 : 0.35f), v.y * (d < 420 ? 1 : 0.35f) };
  }
  void Draw() override {
    ArenaCam(); sc.snow = false; sc.Begin(cam);
    lit.Pattern(PAT_TILE);
    lit.Draw(prim.cube, TRS(W3(800, (Y0 + Y1) / 2, 0.01f), { 0, 0, 0 }, { (X1 - X0 + 60) * SC, 0.02f, (Y1 - Y0 + 60) * SC }), Hex(0xffe0b8));
    lit.Pattern(PAT_NONE);
    Fence(Hex(0xff922b)); sc.Trees(time);
    Toons([&](int i) { return i == holder ? Hex(0xff6b3c, 200) : BLANK; });
    if (holder >= 0) {
      Vector3 p = W3(k[holder].x, k[holder].y, 3.2f + 0.1f * sinf(time * 6)); float pulse = 1 + sinf(time * (fuse < 3 ? 22 : 8)) * 0.1f;
      lit.Draw(prim.sphere, TRS(p, { 0, 0, 0 }, { 0.75f * pulse, 0.75f * pulse, 0.75f * pulse }), fuse < 3 && (int)(time * 10) % 2 ? Hex(0xc92a2a) : Hex(0x25262b));
      lit.Draw(prim.cyl, TRS({ p.x + 0.25f, p.y + 0.6f, p.z }, { 0, 0, -0.5f }, { 0.12f, 0.4f, 0.12f }), Hex(0xa0a4ab));
      if ((int)(time * 30) % 2) fx.Puff({ p.x + 0.5f, p.y + 0.95f, p.z }, frand(0, 1) < 0.5f ? Hex(0xffd43b) : Hex(0xff922b), 0.12f, 0.3f, { frand(-1, 1), 1.5f, frand(-1, 1) });
    }
    fx.Draw(); conf.Draw();
    EndMode3D();
    if (holder >= 0 && fuse < 3) DrawRectangle(0, 0, W, H, Fade(RED, 0.08f + 0.06f * sinf(time * 20)));
    Names();
    if (holder >= 0) { Vector2 s = GetWorldToScreen(W3(k[holder].x, k[holder].y, 4.6f), cam); Text(s.x, s.y - 20, 34, Hex(0xff922b), "💣 PASS IT!", 0, TS_DISPLAY | TS_SHADOW); }
    HUD();
    Hint(holder >= 0 ? "💣 " + P(holder).name + " has the bomb — run into someone to pass it!" : "Next bomb coming…", Hex(0xffd8a8));
    DrawCount();
  }
};

// ===================================================================================== PAINT WARS
struct Paint : Arena {
  static constexpr int C = 36, Rw = 18; std::vector<int> grid; struct PB { float x, y, t; }; std::vector<PB> bombs; float bombT = 3, cntT = 0;
  float CW() { return (X1 - X0) / C; } float CH() { return (Y1 - Y0) / Rw; }
  void Start() override {
    InitCam(44); sc.Init(30, 20, false); T = 60; speed = 350; boardTitle = "FLOOR PAINTED"; Spawn(); grid.assign(C * Rw, -1);
    Layout(-1, kit::Pad("xy", "DASH", "", "Run around to paint the floor your colour! Grab the paint bombs!"));
  }
  void PaintAt(float x, float y, float rad, int who) {
    int c0 = std::max(0, (int)((x - rad - X0) / CW())), c1 = std::min(C - 1, (int)((x + rad - X0) / CW()));
    int r0 = std::max(0, (int)((y - rad - Y0) / CH())), r1 = std::min(Rw - 1, (int)((y + rad - Y0) / CH()));
    for (int r = r0; r <= r1; r++) for (int c = c0; c <= c1; c++) { float cx = X0 + (c + 0.5f) * CW(), cy = Y0 + (r + 0.5f) * CH(); if ((cx - x) * (cx - x) + (cy - y) * (cy - y) < rad * rad) grid[r * C + c] = who; }
  }
  void Rules(float dt) override {
    for (int i = 0; i < N(); i++) PaintAt(k[i].x, k[i].y, Rad * 1.5f, i);
    bombT -= dt;
    if (bombT <= 0 && bombs.size() < 3) { bombT = frand(3, 5.5f); bombs.push_back({ frand(X0 + 80, X1 - 80), frand(Y0 + 80, Y1 - 80), 0 }); }
    for (size_t b = 0; b < bombs.size(); b++) {
      bombs[b].t += dt;
      for (int i = 0; i < N(); i++) if (hypotf(bombs[b].x - k[i].x, bombs[b].y - k[i].y) < Rad + 34) {
        PaintAt(bombs[b].x, bombs[b].y, 190, i); fx.Burst(W3(bombs[b].x, bombs[b].y, 0.5f), 50, P(i).color, 9, 0.9f, 0.3f, 2); Sfx("splash"); Shake(6);
        Banner("🎨 SPLAT! " + P(i).name, 900, kit::Hx(P(i).color).c_str());
        bombs.erase(bombs.begin() + b); b--; break;
      }
    }
    cntT -= dt;
    if (cntT <= 0) { cntT = 0.25f; std::vector<int> cnt(N(), 0); for (int v : grid) if (v >= 0) cnt[v]++; for (int i = 0; i < N(); i++) k[i].score = 100.f * cnt[i] / grid.size(); }
  }
  std::string Stat(int i) override { return I((int)roundf(k[i].score)) + "%"; }
  Vector2 Bot(int i, bool& dash) override {
    dash = frand(0, 1) < 0.01f; K& m = k[i];
    for (auto& b : bombs) if (hypotf(b.x - m.x, b.y - m.y) < 420) return BotSeek(i, b.x, b.y);
    if (m.goalT < el || hypotf(m.goal.x - m.x, m.goal.y - m.y) < 40) {
      for (int t = 0; t < 14; t++) { int c = rand() % (C * Rw); if (grid[c] != i) { m.goal = { X0 + (c % C + 0.5f) * CW(), Y0 + (c / C + 0.5f) * CH() }; break; } }
      m.goalT = el + 1.6f;
    }
    return BotSeek(i, m.goal.x, m.goal.y);
  }
  void Draw() override {
    ArenaCam(); sc.Begin(cam);
    Floor(Hex(0xf1f3f5), PAT_TILE);
    // painted floor, merged into runs so it stays fast
    for (int r = 0; r < Rw; r++) {
      int c = 0;
      while (c < C) {
        int v = grid[r * C + c], e = c; while (e + 1 < C && grid[r * C + e + 1] == v) e++;
        if (v >= 0) { float x0 = X0 + c * CW(), x1 = X0 + (e + 1) * CW(); lit.Draw(prim.cube, TRS(W3((x0 + x1) / 2, Y0 + (r + 0.5f) * CH(), 0.03f), { 0, 0, 0 }, { (x1 - x0) * SC, 0.03f, CH() * SC }), Shade(P(v).color, 0.05f)); }
        c = e + 1;
      }
    }
    Fence(Hex(0x845ef7)); sc.Trees(time);
    for (auto& b : bombs) {
      Vector3 p = W3(b.x, b.y, 0.9f + 0.2f * sinf(b.t * 5)); float s = 1 + 0.08f * sinf(b.t * 9);
      lit.Draw(prim.cyl, TRS({ p.x, p.y - 0.5f, p.z }, { 0, b.t, 0 }, { 0.55f * s, 1.0f, 0.55f * s }), Hex(0xdee2e6));
      lit.Draw(prim.cyl, TRS({ p.x, p.y - 0.1f, p.z }, { 0, b.t, 0 }, { 0.57f * s, 0.35f, 0.57f * s }), Hex(0xff6bd6 - ((int)(b.t * 3) % 3) * 0x202000));
      lit.Draw(prim.sphere, TRS({ p.x, p.y + 0.55f, p.z }, { 0, 0, 0 }, { 0.3f, 0.2f, 0.3f }), Hex(0xadb5bd));
    }
    Toons();
    fx.Draw(); conf.Draw(); EndMode3D();
    Names(); HUD(); Hint("🎨 Paint the floor your colour — paint cans splash a huge area!", Hex(0xe5dbff)); DrawCount();
  }
};

// ===================================================================================== COIN RUSH
struct Coins : Arena {
  struct Coin { float x, y, vx, vy, t; int v; }; struct Bm { float x, y, t; }; std::vector<Coin> coins; std::vector<Bm> bombs; float bombT = 4;
  void AddCoin(float x, float y, int v, float vx = 0, float vy = 0) { coins.push_back({ x, y, vx, vy, frand(0, 6), v }); }
  void Start() override {
    InitCam(44); sc.Init(30, 20, false); T = 60; speed = 350; boardTitle = "COINS"; Spawn();
    for (int i = 0; i < 12; i++) AddCoin(frand(X0 + 40, X1 - 40), frand(Y0 + 40, Y1 - 40), 1);
    Layout(-1, kit::Pad("xy", "DASH", "", "Grab coins! DASH into people to steal one. Run from the red circles!"));
  }
  void Rules(float dt) override {
    if (coins.size() < 16 && frand(0, 1) < dt * 3.5f) AddCoin(frand(X0 + 40, X1 - 40), frand(Y0 + 40, Y1 - 40), frand(0, 1) < 0.1f ? 5 : 1);
    for (auto& c : coins) { c.t += dt; c.x = Clamp(c.x + c.vx * dt, X0 + 20, X1 - 20); c.y = Clamp(c.y + c.vy * dt, Y0 + 20, Y1 - 20); c.vx *= 0.92f; c.vy *= 0.92f; }
    for (size_t c = 0; c < coins.size(); c++) for (int i = 0; i < N(); i++) if (k[i].stun <= 0 && hypotf(coins[c].x - k[i].x, coins[c].y - k[i].y) < Rad + 26) {
      k[i].score += coins[c].v; fx.Burst(W3(coins[c].x, coins[c].y, 0.8f), 10, Hex(0xffd43b), 4, 0.4f, 0.1f); Sfx("pickup");
      coins.erase(coins.begin() + c); c--; break;
    }
    bombT -= dt;
    if (bombT <= 0) { bombT = frand(2.2f, 4.0f) * (T > 30 ? 1 : 0.7f); bombs.push_back({ frand(X0 + 120, X1 - 120), frand(Y0 + 120, Y1 - 120), 2.2f }); }
    for (size_t b = 0; b < bombs.size(); b++) {
      bombs[b].t -= dt; if (bombs[b].t > 0) continue;
      Bm bm = bombs[b]; bombs.erase(bombs.begin() + b); b--;
      fx.Burst(W3(bm.x, bm.y, 1), 50, Hex(0xff922b), 9, 0.9f, 0.25f, 2); Sfx("boom"); Shake(9);
      for (int i = 0; i < N(); i++) {
        float d = hypotf(k[i].x - bm.x, k[i].y - bm.y); if (d > 140) continue;
        int lose = std::min((int)k[i].score, 2); k[i].score -= lose; k[i].stun = 0.8f;
        k[i].vx += (k[i].x - bm.x) / fmaxf(1, d) * 700; k[i].vy += (k[i].y - bm.y) / fmaxf(1, d) * 700;
        for (int q = 0; q < lose; q++) { float a = frand(0, 2 * PI); AddCoin(k[i].x, k[i].y, 1, cosf(a) * 500, sinf(a) * 500); }
        Buzz(i, 300);
      }
    }
  }
  void OnBump(int a, int b, float) override {
    int pr[2][2] = { { a, b }, { b, a } };
    for (auto& q : pr) { K& att = k[q[0]]; K& vic = k[q[1]]; if (att.dashT > 0 && vic.inv <= 0 && vic.score > 0) { vic.score--; att.score++; vic.inv = 0.9f; Sfx("ding"); fx.Burst(W3(vic.x, vic.y, 1.5f), 8, Hex(0xffd43b), 4, 0.4f, 0.1f); } }
  }
  Vector2 Bot(int i, bool& dash) override {
    dash = false; K& m = k[i];
    for (auto& b : bombs) if (hypotf(b.x - m.x, b.y - m.y) < 175) return BotSeek(i, b.x, b.y, true);
    float bd = 1e9f; Vector2 t = { m.x, m.y };
    for (auto& c : coins) { float d = hypotf(c.x - m.x, c.y - m.y) / c.v; if (d < bd) { bd = d; t = { c.x, c.y }; } }
    return BotSeek(i, t.x, t.y);
  }
  void Draw() override {
    ArenaCam(); sc.Begin(cam);
    Floor(Hex(0xffe9a8), PAT_TILE);
    Fence(Hex(0xf59f00)); sc.Trees(time);
    for (auto& b : bombs) {
      Vector3 p = W3(b.x, b.y); float k2 = 1 - b.t / 2.2f;
      lit.Draw(prim.cyl, TRS({ p.x, 0.04f, p.z }, { 0, 0, 0 }, { 140 * SC, 0.02f, 140 * SC }), Hex(0xff4d4d, (unsigned char)(90 + 80 * fabsf(sinf(b.t * 10)))));
      lit.Draw(prim.cyl, TRS({ p.x, 0.06f, p.z }, { 0, 0, 0 }, { 140 * SC * k2, 0.02f, 140 * SC * k2 }), Hex(0xc92a2a, 200));
      lit.Draw(prim.sphere, TRS({ p.x, 0.6f + (1 - k2) * 5, p.z }, { 0, 0, 0 }, { 0.6f, 0.6f, 0.6f }), Hex(0x25262b));
    }
    for (auto& c : coins) {
      float s = c.v > 1 ? 1.4f : 1; Vector3 p = W3(c.x, c.y, 0.55f + 0.12f * sinf(c.t * 3));
      lit.Draw(prim.cyl, TRS({ p.x, 0.04f, p.z }, { 0, 0, 0 }, { 0.55f * s, 0.02f, 0.55f * s }), Hex(0xffd43b, 120));
      lit.Draw(prim.cyl, TRS({ p.x, p.y + 0.25f, p.z }, { PI / 2, c.t * 3, 0 }, { 0.62f * s, 0.16f, 0.62f * s }), c.v > 1 ? Hex(0xff922b) : Hex(0xfcc419));
    }
    Toons();
    fx.Draw(); conf.Draw(); EndMode3D();
    Names(); HUD(); Hint("🪙 Grab coins · DASH into someone to steal · stay out of red circles!", Hex(0xfff3bf)); DrawCount();
  }
};

// ===================================================================================== FROZEN LAKE
struct Frozen : Arena {
  static constexpr int C = 10, Rw = 6; struct Tl { int s = 0; float t = 0; }; std::vector<Tl> tl; float waveT = 3;
  float TW() { return (X1 - X0) / C; } float TH() { return (Y1 - Y0) / Rw; }
  Tl* At(float x, float y) { int c = (int)((x - X0) / TW()), r = (int)((y - Y0) / TH()); if (c < 0 || r < 0 || c >= C || r >= Rw) return nullptr; return &tl[r * C + c]; }
  void Start() override {
    InitCam(44); sc.Init(30, 20, true); clockOn = false; speed = 330; collide = true; groundY = 0.29f; boardTitle = "ON THE ICE"; Spawn(); tl.assign(C * Rw, Tl{});
    Layout(-1, kit::Pad("xy", "DASH", "", "Red cracking ice is about to break — get off it!"));
  }
  void Rules(float dt) override {
    waveT -= dt;
    if (waveT <= 0) {
      waveT = fmaxf(1.6f, 3.2f - el / 30);
      std::vector<int> ok; for (int i = 0; i < C * Rw; i++) if (tl[i].s == 0) ok.push_back(i);
      kit::Shuffle(ok); int n = std::min((int)ok.size() - 4, 3 + (int)(el / 9));
      for (int j = 0; j < n; j++) { tl[ok[j]].s = 1; tl[ok[j]].t = 2.0f; }
      Sfx("tick");
    }
    bool crack = false;
    for (auto& t : tl) {
      if (t.s == 1) { t.t -= dt; if (t.t <= 0) { t.s = 2; t.t = fmaxf(3, 6 - el / 20); crack = true; } }
      else if (t.s == 2) { t.t -= dt; if (t.t <= 0) t.s = 0; }
    }
    if (crack) Sfx("splash");
    for (int i = 0; i < N(); i++) { if (!k[i].alive) continue; Tl* t = At(k[i].x, k[i].y); if (t && t->s == 2) { fx.Burst(W3(k[i].x, k[i].y, 0.2f), 30, Hex(0x74c0fc), 6, 0.8f, 0.2f, 4); Eliminate(i, "SPLASH!"); } else k[i].score = el; }
  }
  bool IsOver() override { return Alive() <= (N() > 1 ? 1 : 0); }
  std::vector<int> Rank() override { return SurvivorRank(); }
  std::string Stat(int i) override { return I((int)k[i].score) + "s on the ice"; }
  Vector2 Bot(int i, bool& dash) override {
    dash = false; Tl* here = At(k[i].x, k[i].y);
    if (here && here->s == 0 && frand(0, 1) < 0.98f) return { 0, 0 };
    float bd = 1e9f; Vector2 best = { k[i].x, k[i].y };
    for (int t = 0; t < C * Rw; t++) if (tl[t].s == 0) { float x = X0 + (t % C + 0.5f) * TW(), y = Y0 + (t / C + 0.5f) * TH(), d = hypotf(x - k[i].x, y - k[i].y); if (d < bd) { bd = d; best = { x, y }; } }
    return BotSeek(i, best.x, best.y);
  }
  void Draw() override {
    ArenaCam(); sc.Begin(cam);
    lit.Pattern(PAT_WATER);
    lit.Draw(prim.cube, TRS(W3(800, (Y0 + Y1) / 2, 0.03f), { 0, 0, 0 }, { (X1 - X0 + 120) * SC, 0.04f, (Y1 - Y0 + 120) * SC }), Hex(0x1864ab));
    lit.Pattern(PAT_ICE);
    for (int t = 0; t < C * Rw; t++) {
      if (tl[t].s == 2) continue;
      float x = X0 + (t % C + 0.5f) * TW(), y = Y0 + (t / C + 0.5f) * TH(), sh = tl[t].s == 1 ? sinf(time * 50 + t) * 0.06f : 0;
      bool red = tl[t].s == 1 && sinf(time * 16) > -0.3f;
      lit.Draw(prim.cube, TRS(W3(x, y, 0.16f), { 0, 0, sh }, { TW() * SC * 0.94f, 0.26f, TH() * SC * 0.94f }), red ? Hex(0xff8787) : Hex(0xd6ecff));
    }
    lit.Pattern(PAT_NONE);
    sc.Trees(time);
    Toons();
    fx.Draw(); conf.Draw(); EndMode3D();
    Names(); HUD(); Hint("❄️ Red ice is cracking — move! Last one dry wins", Hex(0xd0ebff)); DrawCount();
  }
};

// ===================================================================================== ICE HOCKEY (teams)
struct IceHockey : Arena {
  float px = 800, py = 495, pvx = 0, pvy = 0, pause = 0; int goals[2] = { 0, 0 }, last = -1; std::vector<int> indiv;
  float GH = 150;
  void Start() override {
    InitCam(44); sc.Init(30, 20, true); T = 120; speed = 330; Rad = 30; boardTitle = "GOALS";
    Spawn(); indiv.assign(N(), 0);
    for (int i = 0; i < N(); i++) k[i].team = i % 2;
    Reset();
    Layout(-1, kit::Pad("xy", "SHOOT", "", "Red vs Blue! Run into the puck. SHOOT near it = blast it at their goal"));
  }
  void Reset() {
    float cy = (Y0 + Y1) / 2; int c[2] = { 0, 0 }, n[2] = { 0, 0 }; for (auto& m : k) n[m.team]++;
    for (auto& m : k) { m.x = m.team == 0 ? X0 + (X1 - X0) * 0.28f : X0 + (X1 - X0) * 0.72f; m.y = cy + (c[m.team]++ - (n[m.team] - 1) / 2.f) * 120; m.vx = m.vy = 0; m.face = m.team ? PI : 0; }
    px = 800; py = cy; pvx = pvy = 0; pause = 1.2f;
  }
  void OnDash(int i) override {  // aim assist: a shot near the puck flies at the other goal
    float d = hypotf(px - k[i].x, py - k[i].y); if (d > Rad + 70) return;
    float gx = k[i].team == 0 ? X1 + 20 : X0 - 20, gy = (Y0 + Y1) / 2 + frand(-60, 60);
    Vector2 dir = Toward(px, py, gx, gy); pvx = dir.x * 1150; pvy = dir.y * 1150; last = i; Sfx("bat"); Shake(4);
  }
  void Rules(float dt) override {
    if (pause > 0) { pause -= dt; return; }
    float cy = (Y0 + Y1) / 2, r = 16, kf = expf(-0.5f * dt);
    pvx *= kf; pvy *= kf; float sp = hypotf(pvx, pvy); if (sp > 1300) { pvx *= 1300 / sp; pvy *= 1300 / sp; }
    px += pvx * dt; py += pvy * dt;
    if (py < Y0 + r) { py = Y0 + r; pvy = fabsf(pvy) * 0.9f; } if (py > Y1 - r) { py = Y1 - r; pvy = -fabsf(pvy) * 0.9f; }
    bool inGoal = fabsf(py - cy) < GH;
    if (px < X0 + r && !inGoal) { px = X0 + r; pvx = fabsf(pvx) * 0.9f; } if (px > X1 - r && !inGoal) { px = X1 - r; pvx = -fabsf(pvx) * 0.9f; }
    for (int i = 0; i < N(); i++) {
      float dx = px - k[i].x, dy = py - k[i].y, d = hypotf(dx, dy), mn = Rad + r;
      if (d < mn && d > 0.01f) {
        float nx = dx / d, ny = dy / d; px = k[i].x + nx * mn; py = k[i].y + ny * mn;
        float rv = (pvx - k[i].vx) * nx + (pvy - k[i].vy) * ny;
        if (rv < 0) { pvx -= rv * nx * 1.3f; pvy -= rv * ny * 1.3f; pvx += k[i].vx * 0.35f; pvy += k[i].vy * 0.35f; Sfx("tink"); }
        last = i;
      }
    }
    if (px < X0 - 12 || px > X1 + 12) {
      int team = px < X0 ? 1 : 0; goals[team]++;
      if (last >= 0 && k[last].team == team) indiv[last]++;
      for (auto& m : k) m.score = (float)goals[m.team];
      Banner(std::string("GOAL! ") + (team ? "BLUE" : "RED") + (last >= 0 && k[last].team == team ? " · " + P(last).name : ""), 1800, team ? "#4dabff" : "#ff4d6d");
      fx.Burst(W3(px, py, 1), 60, team ? Hex(0x4dabff) : Hex(0xff4d6d), 9, 1.1f, 0.25f, 3); conf.Spawn(W3(px, py, 2), 60, 2, 5); Sfx("win"); Shake(10);
      Reset();
    }
  }
  std::vector<int> Rank() override {
    int win = goals[0] == goals[1] ? -1 : goals[0] > goals[1] ? 0 : 1; std::vector<int> o; for (int i = 0; i < N(); i++) o.push_back(i);
    std::stable_sort(o.begin(), o.end(), [&](int a, int b) { int wa = k[a].team == win, wb = k[b].team == win; if (wa != wb) return wa > wb; return indiv[a] > indiv[b]; });
    return o;
  }
  std::string Stat(int i) override { int t = k[i].team; return std::string(t ? "Blue " : "Red ") + I(goals[t]) + "-" + I(goals[1 - t]) + " · " + I(indiv[i]) + " goals"; }
  bool Sticks() override { return true; }
  std::string WinText() override { return goals[0] == goals[1] ? "IT'S A DRAW!" : goals[0] > goals[1] ? "RED TEAM WINS!" : "BLUE TEAM WINS!"; }
  Vector2 Bot(int i, bool& dash) override {
    float gx = k[i].team == 0 ? X1 : X0, gy = (Y0 + Y1) / 2, dx = px - gx, dy = py - gy, d = fmaxf(1, hypotf(dx, dy));
    float tx = px + dx / d * 40, ty = py + dy / d * 40;
    dash = hypotf(px - k[i].x, py - k[i].y) < Rad + 50 && frand(0, 1) < 0.15f;
    return BotSeek(i, tx, ty);
  }
  void Draw() override {
    ArenaCam(); sc.Begin(cam);
    float cy = (Y0 + Y1) / 2;
    lit.Pattern(PAT_ICE);
    lit.Draw(prim.cube, TRS(W3(800, cy, 0.01f), { 0, 0, 0 }, { (X1 - X0) * SC, 0.02f, (Y1 - Y0) * SC }), Hex(0xf1f8ff));
    lit.Pattern(PAT_NONE);
    lit.Draw(prim.cube, TRS(W3(800, cy, 0.03f), { 0, 0, 0 }, { 0.15f, 0.01f, (Y1 - Y0) * SC }), Hex(0xff6b6b));
    for (float fx2 : { 0.33f, 0.67f }) lit.Draw(prim.cube, TRS(W3(X0 + (X1 - X0) * fx2, cy, 0.03f), { 0, 0, 0 }, { 0.12f, 0.01f, (Y1 - Y0) * SC }), Hex(0x4dabff));
    // boards
    for (float y : { Y0 - 10, Y1 + 10 }) lit.Draw(prim.cube, TRS(W3(800, y, 0.35f), { 0, 0, 0 }, { (X1 - X0 + 40) * SC, 0.7f, 0.25f }), Hex(0xffd43b));
    for (int s = 0; s < 2; s++) {
      float x = s ? X1 + 10 : X0 - 10;
      for (float y : { cy - GH - (cy - GH - Y0) / 2, cy + GH + (Y1 - cy - GH) / 2 }) lit.Draw(prim.cube, TRS(W3(x, y, 0.35f), { 0, 0, 0 }, { 0.25f, 0.7f, (cy - GH - Y0) * SC }), Hex(0xffd43b));
      // goal net
      Vector3 gp = W3(s ? X1 + 8 : X0 - 8, cy, 0);  // a real goal: red pipe frame + netting, mouth facing the rink
      DrawProp(props.hockeyGoal, MatrixMultiply(MatrixMultiply(MatrixScale(1.8f, 1.5f, GH * 2 * SC), MatrixRotateY(s ? PI : 0)), MatrixTranslate(gp.x, 0, gp.z)));
    }
    sc.Trees(time);
    Toons([&](int i) { return k[i].team ? Hex(0x4dabff, 220) : Hex(0xff4d6d, 220); });
    lit.Draw(prim.cyl, TRS(W3(px, py, 0.02f), { 0, 0, 0 }, { 0.45f, 0.22f, 0.45f }), Hex(0x15161a));
    lit.Caster(W3(px, py), 0.4f);
    fx.Draw(); conf.Draw(); EndMode3D();
    Names();
    kit::Glass(560, 14, 480, 64, 22);
    Text(690, 47, 40, Hex(0xff6b81), I(goals[0]), 0, TS_DISPLAY | TS_SHADOW); Text(800, 47, 18, WHITE, "RED  ·  BLUE", 0, TS_HEAVY | TS_PLAIN); Text(910, 47, 40, Hex(0x74c0fc), I(goals[1]), 0, TS_DISPLAY | TS_SHADOW);
    Clock(T, 1500);
    Hint("🏒 Run into the puck · press SHOOT next to it to blast it at their goal", Hex(0xd0ebff)); DrawCount();
  }
};

// ===================================================================================== TARGET BLAST
struct Targets : Classic {
  struct B { Vector3 p; float vx, vy, w, r; int kind; Color c; bool dead = false; };
  struct X { float x, y, vx = 0, vy = 0, cd = 0, kick = 0; int score = 0; };
  std::vector<B> bl; std::vector<X> cr; float T = 60, spawnT = 0; bool over = false, fin = false; float overT = 0;
  void Start() override {
    cam = {}; InitCam(52); cam.position = { 0, 3.5f, 16 }; cam.target = { 0, 8.5f, 0 };
    for (int i = 0; i < N(); i++) cr.push_back({ 300.f + i * 150, 500 });
    Layout(-1, kit::Pad("xy", "SHOOT", "", "Move your crosshair onto balloons and SHOOT. Not the black bombs!"));
  }
  Vector2 Scr(const B& b) { return GetWorldToScreen(b.p, cam); }
  void Shoot(int i) {
    X& c = cr[i]; if (c.cd > 0) return; c.cd = 0.2f; c.kick = 1; Sfx("shoot");
    int best = -1; float bd = 1e9f;
    for (size_t j = 0; j < bl.size(); j++) { if (bl[j].dead) continue; Vector2 s = Scr(bl[j]); float d = hypotf(s.x - c.x, s.y - c.y); float rr = bl[j].r * 52 + 22; if (d < rr && d < bd) { bd = d; best = (int)j; } }
    if (best < 0) return;
    B& b = bl[best]; b.dead = true; int v = b.kind == 1 ? 5 : b.kind == 2 ? -3 : 1; c.score += v;
    fx.Burst(b.p, b.kind == 2 ? 40 : 22, b.kind == 2 ? Hex(0xff922b) : b.c, 6, 0.7f, 0.2f, 3);
    if (b.kind == 2) { Sfx("boom"); Shake(8); Buzz(i, 300); } else Sfx("pickup");
  }
  void Update(float dt) override {
    time += dt; fx.Update(dt); conf.Update(dt);
    for (int i = 0; i < N(); i++) {
      X& c = cr[i]; Player& p = P(i); Vector2 in = p.stick; bool fire = p.a;
      if (p.bot) {
        int best = -1; float bd = 1e9f;
        for (size_t j = 0; j < bl.size(); j++) if (!bl[j].dead && bl[j].kind != 2) { Vector2 s = Scr(bl[j]); if (s.y > 870) continue; float d = hypotf(s.x - c.x, s.y - c.y) / (bl[j].kind == 1 ? 2 : 1); if (d < bd) { bd = d; best = (int)j; } }
        in = { 0, 0 }; fire = false;
        if (best >= 0) { Vector2 s = Scr(bl[best]); in = { Clamp((s.x - c.x) / 120, -1, 1) * 0.75f, Clamp((s.y - c.y) / 120, -1, 1) * 0.75f }; fire = hypotf(s.x - c.x, s.y - c.y) < 40 && frand(0, 1) < 0.25f; }
      }
      // aim assist: gentle pull toward the nearest good balloon
      Vector2 pull = { 0, 0 }; float bd = 140;
      for (auto& b : bl) if (!b.dead && b.kind != 2) { Vector2 s = Scr(b); float d = hypotf(s.x - c.x, s.y - c.y); if (d < bd) { bd = d; pull = { (s.x - c.x) * 2.2f, (s.y - c.y) * 2.2f }; } }
      c.vx += (in.x * 1150 - c.vx) * fminf(1, dt * 10); c.vy += (in.y * 1150 - c.vy) * fminf(1, dt * 10);
      float pk = hypotf(in.x, in.y) < 0.3f ? 1.f : 0.35f;
      c.x = Clamp(c.x + (c.vx + pull.x * pk) * dt, 20, 1580); c.y = Clamp(c.y + (c.vy + pull.y * pk) * dt, 100, 880);
      c.cd -= dt; c.kick = fmaxf(0, c.kick - dt * 6);
      if (fire && cd <= 0 && !over) Shoot(i);
    }
    if (Counting(dt, "GO!")) return;
    if (over) { overT -= dt; if (overT <= 0 && !fin) { fin = true; FinishOrder(Sorted([&](int i) { return (float)cr[i].score; }), [&](int i) { return I(cr[i].score) + " pts"; }); } return; }
    T -= dt;
    if (T <= 0) { over = true; overT = 2.5f; auto o = Sorted([&](int i) { return (float)cr[i].score; }); Sfx("win"); conf.Spawn({ 0, 8, 0 }, 140, 8, 6); Banner(P(o[0]).name + " WINS!", 2400, kit::Hx(P(o[0]).color).c_str()); return; }
    spawnT -= dt;
    if (spawnT <= 0) {
      spawnT = frand(0.25f, 0.5f); float r = frand(0, 1); int kind = r < 0.1f ? 1 : r < 0.2f ? 2 : 0;
      const unsigned C[] = { 0xff4d6d, 0x4dabff, 0x51cf66, 0xcc5de8, 0xff922b };
      bl.push_back({ { frand(-12, 12), -1.0f, frand(-3, 1) }, frand(-0.4f, 0.4f), frand(1.4f, 2.4f) * (kind == 1 ? 1.5f : 1), frand(0, 6), kind == 1 ? 0.75f : 1.0f, kind, Hex(C[rand() % 5]) });
    }
    for (auto& b : bl) { b.w += dt * 2; b.p.x += (b.vx + sinf(b.w) * 0.4f) * dt; b.p.y += b.vy * dt; }
    bl.erase(std::remove_if(bl.begin(), bl.end(), [](const B& b) { return b.dead || b.p.y > 20; }), bl.end());
  }
  void Draw() override {
    UI::Sky(Hex(0x5b3cc4), Hex(0xffb3d9));
    lit.Begin(cam, { 0.9f, 0.75f, 0.95f }, 0.004f);
    BeginMode3D(cam);
    lit.Pattern(PAT_GRASS); lit.Draw(prim.plane, TRS({ 0, -2.6f, 0 }, { 0, 0, 0 }, { 200, 1, 200 }), Hex(0x69b35a)); lit.Pattern(PAT_NONE);
    for (int q = 0; q < 7; q++) {  // fluffy clouds
      float cx = -26 + q * 9 + sinf(time * 0.05f + q) * 2, cy = 13 + (q % 3) * 3.5f, cz = -22 - (q % 2) * 6;
      for (int j = 0; j < 4; j++) DrawSphere({ cx + j * 1.6f - 2.4f, cy + (j % 2) * 0.6f, cz }, 1.5f + (j % 2) * 0.4f, Hex(0xfdf6ff));
    }
    // carnival stalls at the bottom
    for (int s = -3; s <= 3; s++) {
      float x = s * 4.2f; Color c = s % 2 ? Hex(0xff6b6b) : Hex(0xffffff);
      lit.Draw(prim.cube, TRS({ x, -1.6f, 3 }, { 0, 0, 0 }, { 4.0f, 2.0f, 1.2f }), Hex(0x8a5a35));
      for (int k2 = 0; k2 < 4; k2++) lit.Draw(prim.cube, TRS({ x - 1.5f + k2, -0.3f, 3.3f }, { 0.4f, 0, 0 }, { 1.0f, 0.15f, 1.4f }), k2 % 2 ? c : Hex(0xffd43b));
    }
    for (auto& b : bl) {
      Color c = b.kind == 2 ? Hex(0x25262b) : b.kind == 1 ? Hex(0xfcc419) : b.c;
      lit.Draw(prim.sphere, TRS(b.p, { 0, 0, sinf(b.w) * 0.1f }, { b.r * 0.86f, b.r, b.r * 0.86f }), c);
      lit.Draw(prim.cone, TRS({ b.p.x, b.p.y - b.r * 1.0f, b.p.z }, { PI, 0, 0 }, { 0.15f, 0.2f, 0.15f }), c);
      Limb({ b.p.x, b.p.y - b.r * 1.1f, b.p.z }, { b.p.x + sinf(b.w) * 0.2f, b.p.y - b.r - 1.6f, b.p.z }, 0.015f, Hex(0xf8f9fa));
      if (b.kind == 2) lit.Draw(prim.cyl, TRS({ b.p.x, b.p.y + b.r * 0.95f, b.p.z }, { 0, 0, 0.3f }, { 0.1f, 0.35f, 0.1f }), Hex(0xadb5bd));
    }
    fx.Draw(); conf.Draw(); EndMode3D();
    for (auto& b : bl) if (b.kind == 1 || b.kind == 2) { Vector2 s = GetWorldToScreen(b.p, cam); Text(s.x, s.y, 30, WHITE, b.kind == 1 ? "★" : "💣", 0, TS_DISPLAY | TS_SHADOW); }
    for (int i = 0; i < N(); i++) {
      X& c = cr[i]; float r = 30 + c.kick * 10; Color col = P(i).color;
      DrawRing({ c.x, c.y }, r - 3, r + 3, 0, 360, 40, Fade(BLACK, 0.35f));
      DrawRing({ c.x, c.y }, r - 2.5f, r + 2.5f, 0, 360, 40, col);
      for (int q = 0; q < 4; q++) { float a = q * PI / 2; DrawLineEx({ c.x + cosf(a) * (r - 10), c.y + sinf(a) * (r - 10) }, { c.x + cosf(a) * (r + 14), c.y + sinf(a) * (r + 14) }, 5, col); }
      DrawCircleV({ c.x, c.y }, 4, WHITE);
      Text(c.x, c.y + r + 22, 17, col, P(i).name, 0, TS_HEAVY);
    }
    Board("POINTS", Sorted([&](int i) { return (float)cr[i].score; }), [&](int i) { return I(cr[i].score); });
    Clock(T);
    Hint("🎯 Balloon +1 · ★ gold +5 · 💣 bomb −3 — the crosshair helps you aim", Hex(0xf3d9fa)); DrawCount();
  }
};

// ===================================================================================== LIGHT TRAILS
struct Trails : Classic {
  static constexpr int GW = 160, GHt = 84; const float CELL = 8.75f, TOP = 110;
  struct Rd { float x, y, ang, energy = 1; bool alive = true; int cell = -1; std::vector<Vector2> pts; };
  std::vector<Rd> rd; std::vector<short> grid; std::vector<float> stamp; std::vector<int> pts, out; int round = 0, R = 3; float el = 0, overT = 0; bool between = false, fin = false;
  float LX(float x) { return x; }
  void Start() override { InitCam(46); pts.assign(N(), 0); Layout(-1, kit::Pad("x", "", "BOOST", "Steer left / right. Never touch a light wall!")); Next(); }
  void Next() {
    round++;
    if (round > R) { if (!fin) { fin = true; FinishOrder(Sorted([&](int i) { return (float)pts[i]; }), [&](int i) { return I(pts[i]) + " pts"; }); } return; }
    grid.assign(GW * GHt, -1); stamp.assign(GW * GHt, 0); rd.clear(); out.clear(); el = 0; between = false;
    for (int i = 0; i < N(); i++) { float a = (float)i / N() * 2 * PI; Rd r; r.x = 800 - cosf(a) * 540; r.y = TOP + 368 - sinf(a) * 280; r.ang = atan2f(TOP + 368 - r.y, 800 - r.x); r.pts.push_back({ r.x, r.y }); rd.push_back(r); }
    Recount();
  }
  int Cell(float x, float y) { int cx = (int)floorf((x - 100) / CELL), cy = (int)floorf((y - TOP) / CELL); if (cx < 0 || cy < 0 || cx >= GW || cy >= GHt) return -2; return cy * GW + cx; }
  bool Free(float x, float y, int i) { int c = Cell(x, y); if (c == -2) return false; int o = grid[c]; return o == -1 || (o == i && el - stamp[c] < 0.3f); }
  void Crash(int i) { Rd& r = rd[i]; r.alive = false; out.push_back(i); fx.Burst(W3(r.x, r.y, 0.6f), 40, P(i).color, 8, 1, 0.2f, 2); Sfx("boom"); Shake(8); Buzz(i, 400); }
  void Update(float dt) override {
    time += dt; fx.Update(dt); conf.Update(dt);
    if (fin) return;
    if (Counting(dt, "GO!")) return;
    if (between) { overT -= dt; if (overT <= 0) Next(); return; }
    el += dt;
    for (int i = 0; i < N(); i++) {
      Rd& r = rd[i]; if (!r.alive) continue;
      float turn = P(i).stick.x; bool boost = P(i).b || P(i).a;
      if (P(i).bot) {
        float best = 0, bs = -1;
        for (float da : { -0.6f, 0.f, 0.6f }) { float s = 0; for (float d = 12; d < 220; d += 8) { if (!Free(r.x + cosf(r.ang + da) * d, r.y + sinf(r.ang + da) * d, i)) break; s = d; } s += da == 0 ? 10 : frand(0, 8); if (s > bs) { bs = s; best = da; } }
        turn = Clamp(best * 2.5f, -1, 1); boost = bs > 200 && frand(0, 1) < 0.02f;
      }
      r.ang += Clamp(turn, -1, 1) * 3.0f * dt;
      boost = boost && r.energy > 0.02f; r.energy = boost ? fmaxf(0, r.energy - dt * 0.6f) : fminf(1, r.energy + dt * 0.15f);
      float dist = (boost ? 330.f : 210.f) * dt;
      while (dist > 0 && r.alive) {
        float st = fminf(4, dist); dist -= st; r.x += cosf(r.ang) * st; r.y += sinf(r.ang) * st;
        int c = Cell(r.x, r.y);
        if (c != r.cell) { if (c == -2 || (grid[c] != -1 && !(grid[c] == i && el - stamp[c] < 0.3f))) { Crash(i); break; } grid[c] = (short)i; stamp[c] = el; r.cell = c; }
      }
      Vector2 l = r.pts.back(); float dd = hypotf(l.x - r.x, l.y - r.y);
      if (dd > 34 || (dd > 10 && r.pts.size() > 1 && fabsf(angDiff(atan2f(l.y - r.pts[r.pts.size() - 2].y, l.x - r.pts[r.pts.size() - 2].x), r.ang)) > 0.12f)) r.pts.push_back({ r.x, r.y });
    }
    int al = 0, last = -1; for (int i = 0; i < N(); i++) if (rd[i].alive) { al++; last = i; }
    if (al <= (N() > 1 ? 1 : 0)) {
      between = true; overT = 3;
      for (size_t j = 0; j < out.size(); j++) pts[out[j]] += (int)j;
      if (last >= 0) { pts[last] += N(); Banner(P(last).name + " WINS THE ROUND!", 2000, kit::Hx(P(last).color).c_str()); Sfx("win"); conf.Spawn(W3(rd[last].x, rd[last].y, 2), 90, 2, 5); }
      else Banner("DOUBLE CRASH!", 1800);
    }
  }
  void Wall(Vector2 a, Vector2 b, Color c, float alpha) {
    Vector3 A = W3(a.x, a.y), Bv = W3(b.x, b.y); float len = hypotf(Bv.x - A.x, Bv.z - A.z); if (len < 0.01f) return;
    float yaw = atan2f(Bv.x - A.x, Bv.z - A.z);
    lit.Draw(prim.cube, TRS({ (A.x + Bv.x) / 2, 0.45f, (A.z + Bv.z) / 2 }, { 0, yaw, 0 }, { 0.12f, 0.9f, len + 0.12f }), Fade(c, alpha));
  }
  void Draw() override {
    cam.position = { 0, 25, 15.5f }; cam.target = { 0, 0, 0.6f };
    UI::Sky(Hex(0x05030f), Hex(0x140a30));
    lit.Begin(cam, { 0.02f, 0.01f, 0.06f }, 0.01f);
    BeginMode3D(cam);
    float x0 = 100, x1 = 100 + GW * CELL, y1 = TOP + GHt * CELL;
    lit.Draw(prim.cube, TRS(W3((x0 + x1) / 2, (TOP + y1) / 2, -0.02f), { 0, 0, 0 }, { (x1 - x0) * SC, 0.02f, (y1 - TOP) * SC }), Hex(0x0b0a24));
    for (float x = x0; x <= x1 + 1; x += 70) lit.Draw(prim.cube, TRS(W3(x, (TOP + y1) / 2, 0), { 0, 0, 0 }, { 0.04f, 0.01f, (y1 - TOP) * SC }), Hex(0x1c3d7a));
    for (float y = TOP; y <= y1 + 1; y += 70) lit.Draw(prim.cube, TRS(W3((x0 + x1) / 2, y, 0), { 0, 0, 0 }, { (x1 - x0) * SC, 0.01f, 0.04f }), Hex(0x1c3d7a));
    for (float y : { TOP, y1 }) lit.Draw(prim.cube, TRS(W3((x0 + x1) / 2, y, 0.3f), { 0, 0, 0 }, { (x1 - x0) * SC, 0.6f, 0.15f }), Hex(0x22d3ee));
    for (float x : { x0, x1 }) lit.Draw(prim.cube, TRS(W3(x, (TOP + y1) / 2, 0.3f), { 0, 0, 0 }, { 0.15f, 0.6f, (y1 - TOP) * SC }), Hex(0x22d3ee));
    for (int i = 0; i < N(); i++) {
      Rd& r = rd[i]; Color c = Shade(P(i).color, 0.25f); float a = r.alive ? 0.95f : 0.4f;
      for (size_t j = 1; j < r.pts.size(); j++) Wall(r.pts[j - 1], r.pts[j], c, a);
      Wall(r.pts.back(), { r.x, r.y }, c, a);
      if (r.alive) {
        Vector3 p = W3(r.x, r.y); float yaw = atan2f(cosf(r.ang), sinf(r.ang));
        lit.Draw(prim.cube, TRS({ p.x, 0.35f, p.z }, { 0, yaw, 0 }, { 0.42f, 0.45f, 1.1f }), P(i).color);
        lit.Draw(prim.sphere, TRS({ p.x + cosf(r.ang) * 0.4f * 0, 0.7f, p.z }, { 0, 0, 0 }, { 0.38f, 0.38f, 0.38f }), Hex(0xf8f9fa));
        for (int s = -1; s <= 1; s += 2) lit.Draw(prim.cyl, TRS({ p.x + cosf(r.ang) * 0.45f * s, 0.25f, p.z + sinf(r.ang) * 0.45f * s }, { 0, yaw, PI / 2 }, { 0.25f, 0.12f, 0.25f }), Hex(0x111111));
      }
    }
    fx.Draw(); conf.Draw();
    EndMode3D();
    for (int i = 0; i < N(); i++) if (rd[i].alive) NameTag(W3(rd[i].x, rd[i].y, 1.8f), i, 17);
    Board("ROUND " + I(std::min(round, R)) + " / " + I(R), Sorted([&](int i) { return (float)pts[i]; }), [&](int i) { return I(pts[i]); });
    Hint("🏍️ Steer left / right · BOOST for speed · never touch a wall", Hex(0xc5f6fa)); DrawCount();
  }
};
}  // namespace

static RegGame g1("potato", [] { return std::unique_ptr<Game>(new Potato); });
static RegGame g2("paint", [] { return std::unique_ptr<Game>(new Paint); });
static RegGame g3("coins", [] { return std::unique_ptr<Game>(new Coins); });
static RegGame g4("frozen", [] { return std::unique_ptr<Game>(new Frozen); });
static RegGame g5("hockey", [] { return std::unique_ptr<Game>(new IceHockey); });
static RegGame g6("targets", [] { return std::unique_ptr<Game>(new Targets); });
static RegGame g7("trails", [] { return std::unique_ptr<Game>(new Trails); });
