// WINTER 3D: Ski Slalom (steer through gates), Sled Dash (shake to race) and Snow Curling (aim, power, sweep).
// Easier than before: wider gates + steering help, a catch-up boost for sleds, slower curling meters.
#include "classic.h"
#include "kit.h"
#include "mesh.h"

namespace {
using kit::I;

// ===================================================================================== SKI SLALOM
struct Ski : Classic {
  struct S { float x = 0, vx = 0, crash = 0, lean = 0; int gates = 0, crashes = 0; long lastGate = -1; bool tuck = false; };
  struct O { int type; float x, z, w; long id; Color c; bool hit = false; };  // type 0 gate, 1 tree
  std::vector<S> sk; std::vector<O> obs; float T = 60, dist = 0, nextZ = 14, spd = 9; bool over = false, fin = false; float overT = 0; long nid = 0;
  std::vector<Vector4> side;  // trees along the edges
  int Score(int i) { return sk[i].gates * 10 - sk[i].crashes * 3; }
  SnapTimer snap;
  // the phone shows the slope ahead of you: 18 m wide, 4 m behind to 20 m ahead
  Vector2 PP(float x, float z) { return { (x + 9) / 18, 1 - (z - (dist - 4)) / 24 }; }
  void Snap(float dt) {
    if (!snap.Tick(dt)) return;
    std::vector<TouchObj> base;
    for (auto& o : obs) {
      if (o.z < dist - 4 || o.z > dist + 20) continue; Vector2 q = PP(o.x, o.z);
      if (o.type == 0) base.push_back({ q.x, q.y, o.w / 2 / 18, o.c, 8 });
      else base.push_back({ q.x, q.y, (o.w * 0.55f + 0.2f) / 18, o.hit ? Hex(0x8ca88f) : Hex(0x2f9e44), 9 });
    }
    for (int i = 0; i < N(); i++) {
      if (P(i).bot) continue; std::vector<TouchObj> o = base;
      for (int j = 0; j < N(); j++) { Vector2 q = PP(sk[j].x, dist); o.push_back({ q.x, q.y, 0.5f / 18, P(j).color, j == i ? 1 : 3 }); }
      SendSnap(i, o, "⛷️ YOU: " + I(sk[i].gates) + " gates" + (sk[i].crash > 0 ? " · OUCH!" : ""));
    }
  }
  void Start() override {
    InitCam(52);
    for (int i = 0; i < N(); i++) { S s; s.x = (i - (N() - 1) / 2.f) * 1.6f; sk.push_back(s); }
    for (float z = -10; z < 80; z += frand(1.5f, 3)) for (int sd = -1; sd <= 1; sd += 2) side.push_back({ sd * frand(10.5f, 16), z, frand(1.2f, 2.4f), frand(0, 6) });
    Layout(-1, TouchLayout("slope", 18.f / 24.f, "steer", "", "Hold your finger where you want to ski — go between the flags, dodge the trees!", -1, "TUCK"));
  }
  void Spawn() {
    while (nextZ < dist + 45) {
      if (frand(0, 1) < 0.6f) obs.push_back({ 0, frand(-6, 6), nextZ, frand(4.5f, 6.0f), nid++, frand(0, 1) < 0.5f ? Hex(0xff4d6d) : Hex(0x3d8bff) });
      else { int n = kit::irand(1, 2); for (int k = 0; k < n; k++) obs.push_back({ 1, frand(-7.5f, 7.5f), nextZ + frand(-0.8f, 0.8f), frand(0.7f, 1.0f), nid++, BLANK }); }
      nextZ += frand(6.5f, 8.5f) * (1 - fminf(0.3f, (60 - T) / 200));
    }
  }
  void Update(float dt) override {
    time += dt; fx.Update(dt); conf.Update(dt);
    Spawn(); Snap(dt);
    if (Counting(dt, "GO!")) return;
    if (over) { overT -= dt; if (overT <= 0 && !fin) { fin = true; FinishOrder(Sorted([&](int i) { return (float)Score(i); }), [&](int i) { return I(sk[i].gates) + " gates · " + I(sk[i].crashes) + " crashes"; }); } return; }
    T -= dt;
    if (T <= 0) { over = true; overT = 2.6f; auto o = Sorted([&](int i) { return (float)Score(i); }); Sfx("win"); conf.Spawn({ sk[o[0]].x, 3, dist + 2 }, 120, 3, 6); Banner(P(o[0]).name + " WINS!", 2400, kit::Hx(P(o[0]).color).c_str()); return; }
    spd = 9 + (60 - T) * 0.08f;
    bool anyTuck = false;
    for (int i = 0; i < N(); i++) {
      S& s = sk[i]; float in = P(i).stick.x; s.tuck = P(i).b || P(i).a;
      // next gate ahead (for steering help + bots)
      const O* g = nullptr; for (auto& o : obs) if (o.type == 0 && o.z > dist + 0.5f && o.id != s.lastGate && (!g || o.z < g->z)) g = &o;
      if (P(i).bot) {
        float want = (g ? g->x : 0) + (i - (N() - 1) / 2.f) * 0.7f;
        for (auto& o : obs) if (o.type == 1 && o.z > dist && o.z < dist + 5 && fabsf(o.x - s.x) < 1.8f) want = s.x + (s.x < o.x ? -3.5f : 3.5f);
        in = Clamp((want - s.x) / 2.5f, -1, 1) * 0.9f; s.tuck = false;
      } else if (g && fabsf(in) < 0.15f) in += Clamp((g->x - s.x) / 6, -0.35f, 0.35f);  // gentle steering help when you let go
      if (!P(i).bot && P(i).touch) { float tx = -9 + TouchAt(P(i)).x * 18; in = Clamp((tx - s.x) / 1.2f, -1, 1); }
      if (s.tuck) anyTuck = true;
      if (s.crash > 0) { s.crash -= dt; s.vx *= 0.9f; continue; }
      s.vx += (in * 13 - s.vx) * fminf(1, dt * 6); s.x = Clamp(s.x + s.vx * dt, -8.5f, 8.5f); s.lean = Clamp(s.vx / 13, -1, 1);
      if (frand(0, 1) < 0.6f) fx.Puff({ s.x + frand(-0.2f, 0.2f), 0.1f, dist - 0.5f }, Hex(0xffffff), 0.18f, 0.5f, { -s.vx * 0.1f, 0.6f, -2 });
      for (auto& o : obs) {
        if (o.type == 1 && !o.hit && fabsf(o.z - dist) < 0.6f && fabsf(o.x - s.x) < o.w * 0.55f + 0.35f) {
          o.hit = true; s.crash = 0.8f; s.crashes++; fx.Burst({ s.x, 1, dist }, 25, WHITE, 6, 0.8f, 0.2f, 4); Sfx("bonk"); Shake(5); Buzz(i, 250);
        }
        if (o.type == 0 && s.lastGate != o.id && o.z <= dist && o.z > dist - spd * dt * 2 - 0.2f) {
          s.lastGate = o.id;
          if (fabsf(s.x - o.x) < o.w / 2) { s.gates++; Sfx("ding"); fx.Burst({ s.x, 1.2f, dist }, 14, o.c, 5, 0.5f, 0.12f); }
        }
      }
    }
    dist += spd * (anyTuck ? 1.25f : 1) * dt;
    obs.erase(std::remove_if(obs.begin(), obs.end(), [&](const O& o) { return o.z < dist - 6; }), obs.end());
    for (auto& t : side) if (t.y < dist - 12) t.y += 90;
  }
  void Draw() override {
    cam.position = { 0, 9.5f, dist - 11.5f }; cam.target = { 0, 0, dist + 10 };
    UI::Sky(Hex(0x8ec5ff), Hex(0xe7f5ff));
    lit.Begin(cam, { 0.88f, 0.94f, 1.0f }, 0.012f);
    BeginMode3D(cam);
    lit.Pattern(PAT_SNOW); lit.Draw(prim.plane, TRS({ 0, 0, dist + 30 }, { 0, 0, 0 }, { 120, 1, 120 }), Hex(0xf3f8ff)); lit.Pattern(PAT_NONE);
    for (int sd = -1; sd <= 1; sd += 2) for (float z = floorf((dist - 8) / 3) * 3; z < dist + 60; z += 3) lit.Draw(prim.cube, TRS({ sd * 9.6f, 0.15f, z }, { 0, 0, 0 }, { 0.12f, 0.3f, 1.6f }), Hex(0xff922b));
    for (auto& t : side) {
      float s = t.z;
      lit.Draw(prim.cyl, TRS({ t.x, 0, t.y }, { 0, 0, 0 }, { 0.22f * s, 0.5f * s, 0.22f * s }), Hex(0x5c3d2e));
      for (int k = 0; k < 3; k++) { float y = 0.4f * s + k * 0.45f * s, r = (1.1f - k * 0.28f) * s; lit.Draw(prim.cone, TRS({ t.x, y, t.y }, { 0, 0, 0 }, { r, 0.75f * s, r }), Hex(0x2b8a3e)); lit.Draw(prim.cone, TRS({ t.x, y + 0.42f * s, t.y }, { 0, 0, 0 }, { r * 0.5f, 0.34f * s, r * 0.5f }), WHITE); }
    }
    for (auto& o : obs) {
      if (o.type == 0) {
        for (int sd = -1; sd <= 1; sd += 2) {
          float x = o.x + sd * o.w / 2;
          lit.Draw(prim.cyl, TRS({ x, 0, o.z }, { 0, 0, 0 }, { 0.07f, 2.0f, 0.07f }), Hex(0x495057));
          lit.Draw(prim.cube, TRS({ x - sd * 0.45f, 1.65f + sinf(time * 6 + o.z) * 0.03f, o.z }, { 0, 0, 0 }, { 0.9f, 0.6f, 0.04f }), o.c);
        }
        lit.Draw(prim.cube, TRS({ o.x, 0.02f, o.z }, { 0, 0, 0 }, { o.w, 0.01f, 0.12f }), Fade(o.c, 0.35f));
      } else {
        float s = o.w * 1.6f;
        lit.Draw(prim.cyl, TRS({ o.x, 0, o.z }, { 0, 0, o.hit ? 0.5f : 0 }, { 0.22f * s, 0.5f * s, 0.22f * s }), Hex(0x5c3d2e));
        for (int k = 0; k < 3; k++) { float y = 0.4f * s + k * 0.45f * s, r = (1.0f - k * 0.27f) * s; lit.Draw(prim.cone, TRS({ o.x, y, o.z }, { 0, 0, o.hit ? 0.4f : 0 }, { r, 0.75f * s, r }), Hex(0x2f9e44)); }
      }
    }
    for (int i = 0; i < N(); i++) {
      S& s = sk[i]; float y = s.crash > 0 ? 0 : 0; float roll = s.crash > 0 ? 1.2f : s.lean * 0.3f;
      Vector3 p = { s.x, y, dist };
      for (int sd = -1; sd <= 1; sd += 2) lit.Draw(prim.cube, TRS({ p.x + sd * 0.22f, 0.04f, p.z + 0.2f }, { 0, s.lean * 0.3f, 0 }, { 0.12f, 0.05f, 1.9f }), Hex(0x212529));
      DrawToon({ p.x, 0.08f, p.z }, s.lean * 0.4f, P(i).color, time + i, 0, s.crash > 0 ? 0 : 0.0f, s.tuck ? 1.05f : 1.2f);
      (void)roll;
      for (int sd = -1; sd <= 1; sd += 2) Limb({ p.x + sd * 0.55f, 1.05f, p.z + 0.2f }, { p.x + sd * 0.7f, 0, p.z - 0.6f }, 0.025f, Hex(0xadb5bd));
    }
    fx.Draw(); conf.Draw();
    EndMode3D();
    for (int i = 0; i < N(); i++) { Vector2 s = GetWorldToScreen({ sk[i].x, 2.6f, dist }, cam); Text(s.x, s.y, 18, P(i).color, P(i).name, 0, TS_HEAVY); if (sk[i].crash > 0) Text(s.x, s.y - 30, 26, Hex(0xff6b6b), "OUCH!", 0, TS_DISPLAY | TS_SHADOW); }
    Board("GATES", Sorted([&](int i) { return (float)Score(i); }), [&](int i) { return I(sk[i].gates); });
    Clock(T);
    Hint("⛷️ Hold your finger where to ski · flags +10 · trees knock you over · TUCK for speed", Hex(0xd0ebff));
    DrawCount();
  }
};

// ===================================================================================== SLED DASH
struct Sled : Classic {
  struct S { float prog = 0, heat = 0, acc = 0, rate = 0, last = -1; int place = 0; };
  std::vector<S> st; std::vector<int> fin; float el = 0, zapT = 7, endT = -1, boltT = 0; int boltI = -1; bool done = false;
  float LZ(int i) { return (i - (N() - 1) / 2.f) * fminf(2.4f, 13.f / N()); }
  float PX(float prog) { return -15 + prog * 0.3f; }
  void Start() override {
    InitCam(42); sc.Init(30, 20, true);
    for (int i = 0; i < N(); i++) { S s; s.rate = frand(5.5f, 7.0f); st.push_back(s); }
    Layout(-1, "{\"name\":\"shake\",\"hint\":\"Shake your phone (or tap really fast) to pull your sled! Avalanches hit the leader.\"}");
  }
  float Leader() { float m = 0; for (auto& s : st) m = fmaxf(m, s.prog); return m; }
  void Add(int i, float v) {
    S& s = st[i]; if (s.prog >= 100) return;
    float behind = Leader() - s.prog;
    s.prog = fminf(100, s.prog + v * (1 + fminf(0.5f, behind / 40)));  // catch-up: the further behind, the more each shake counts
    s.heat = fminf(1, s.heat + 0.15f);
    if (s.prog >= 100) {
      fin.push_back(i); s.place = (int)fin.size(); Sfx("win"); fx.Burst({ PX(100), 1, LZ(i) }, 40, P(i).color, 7, 1, 0.2f, 3);
      if (fin.size() == 1) { Banner(P(i).name + " WINS!", 2200, kit::Hx(P(i).color).c_str()); endT = 6; conf.Spawn({ PX(100), 3, 0 }, 140, 4, 6); }
    }
  }
  void Act(int p, const std::string& k, int n, float) override { if (k == "pump" && cd <= 0 && p >= 0 && p < N() && !P(p).bot) { if (time - st[p].last < 0.06f) return; st[p].last = time; Add(p, 1.9f); } }
  void Update(float dt) override {
    time += dt; fx.Update(dt); conf.Update(dt); boltT -= dt;
    if (Counting(dt, "MUSH!")) return;
    if (done) return;
    el += dt;
    for (int i = 0; i < N(); i++) {
      S& s = st[i]; s.heat = fmaxf(0, s.heat - dt * 0.8f);
      if (P(i).a && !P(i).pa) Add(i, 1.9f);  // keyboard / button fallback
      if (P(i).bot && s.prog < 100) { s.acc += dt * s.rate * frand(0.6f, 1.4f); while (s.acc >= 1) { s.acc -= 1; Add(i, 1.5f); } }
      if (s.prog < 100 && s.heat > 0.05f && frand(0, 1) < 0.7f) fx.Puff({ PX(s.prog) - 1.2f, 0.2f, LZ(i) + frand(-0.3f, 0.3f) }, WHITE, 0.25f + s.heat * 0.3f, 0.5f, { -3, 0.8f, 0 });
    }
    zapT -= dt;
    if (zapT <= 0) {
      zapT = frand(6, 9); int lead = -1; for (int i = 0; i < N(); i++) if (st[i].prog < 100 && (lead < 0 || st[i].prog > st[lead].prog)) lead = i;
      int racing = 0; for (auto& s : st) racing += s.prog < 100;
      if (lead >= 0 && racing > 1 && st[lead].prog > 15) {
        st[lead].prog = fmaxf(0, st[lead].prog - 7); boltI = lead; boltT = 0.8f;
        fx.Burst({ PX(st[lead].prog), 1.5f, LZ(lead) }, 40, WHITE, 6, 0.9f, 0.3f, 5); Sfx("snow"); Shake(8); Buzz(lead, 300);
        Banner("❄️ AVALANCHE on " + P(lead).name + "!", 1300, "#a5d8ff");
      }
    }
    if (endT > 0) endT -= dt;
    if ((int)fin.size() == N() || (endT <= 0 && endT > -0.5f && !fin.empty()) || el > 70) {
      done = true; std::vector<int> o = fin; std::vector<int> rest; for (int i = 0; i < N(); i++) if (st[i].prog < 100) rest.push_back(i);
      std::sort(rest.begin(), rest.end(), [&](int a, int b) { return st[a].prog > st[b].prog; }); o.insert(o.end(), rest.begin(), rest.end());
      FinishOrder(o, [&](int i) { return st[i].prog >= 100 ? std::string("finished!") : I((int)st[i].prog) + "%"; });
    }
  }
  void Draw() override {
    cam.position = { 0, 14, 22 }; cam.target = { 0, 0, 1.0f };
    sc.Begin(cam);
    float zh = fminf(2.4f, 13.f / N()) * N() / 2 + 1;
    lit.Pattern(PAT_ICE); lit.Draw(prim.cube, TRS({ 0, 0.02f, 0 }, { 0, 0, 0 }, { 34, 0.04f, zh * 2 }), Hex(0x5f9bd6)); lit.Pattern(PAT_NONE);
    for (int i = 0; i <= N(); i++) { float z = (i - N() / 2.f) * fminf(2.4f, 13.f / N()); lit.Draw(prim.cube, TRS({ 0, 0.06f, z }, { 0, 0, 0 }, { 32, 0.02f, 0.06f }), Hex(0xa5d8ff)); }
    for (int r = 0; r < (int)(zh * 4); r++) for (int c = 0; c < 2; c++) lit.Draw(prim.cube, TRS({ PX(100) + c * 0.5f, 0.06f, -zh + r * 0.5f + 0.25f }, { 0, 0, 0 }, { 0.5f, 0.03f, 0.5f }), (r + c) % 2 ? Hex(0x111111) : WHITE);
    for (int s = -1; s <= 1; s += 2) { lit.Draw(prim.cyl, TRS({ PX(100) + 0.5f, 0, s * zh }, { 0, 0, 0 }, { 0.15f, 3.2f, 0.15f }), Hex(0xff4d6d)); }
    lit.Draw(prim.cube, TRS({ PX(100) + 0.5f, 3.2f, 0 }, { 0, 0, 0 }, { 0.3f, 0.6f, zh * 2 }), Hex(0xff4d6d));
    sc.Trees(time);
    for (int i = 0; i < N(); i++) {
      S& s = st[i]; float x = PX(s.prog), z = LZ(i), jig = s.heat * 0.05f * sinf(time * 40 + i);
      lit.Draw(prim.cube, TRS({ x, 0.32f, z + jig }, { 0, 0, 0 }, { 1.8f, 0.18f, 0.9f }), Hex(0xc0703a));
      for (int sd = -1; sd <= 1; sd += 2) { lit.Draw(prim.cube, TRS({ x, 0.08f, z + sd * 0.4f }, { 0, 0, 0 }, { 2.1f, 0.06f, 0.08f }), Hex(0x7a4a1e)); lit.Draw(prim.cyl, TRS({ x + 1.05f, 0.08f, z + sd * 0.4f }, { PI / 2, 0, 0 }, { 0.14f, 0.08f, 0.14f }), Hex(0x7a4a1e)); }
      DrawToon({ x - 0.2f, 0.4f, z + jig }, PI / 2, P(i).color, time + i, s.heat > 0.3f ? fabsf(sinf(time * 14 + i)) * 0.12f : 0, s.place == 1 ? 1.f : s.heat * 0.6f, 1.05f);
      if (boltI == i && boltT > 0) for (int k = 0; k < 5; k++) lit.Draw(prim.sphere, TRS({ x + frand(-0.6f, 0.6f), 1 + boltT * 4 + k * 0.6f, z + frand(-0.4f, 0.4f) }, { 0, 0, 0 }, { 0.6f, 0.6f, 0.6f }), WHITE);
    }
    fx.Draw(); conf.Draw(); EndMode3D();
    for (int i = 0; i < N(); i++) {
      Vector2 p = GetWorldToScreen({ PX(st[i].prog), 2.4f, LZ(i) }, cam); Text(p.x, p.y, 18, P(i).color, P(i).name, 0, TS_HEAVY);
      Vector2 e = GetWorldToScreen({ PX(100) + 2.2f, 0.3f, LZ(i) }, cam); Text(e.x, e.y, 24, st[i].place ? Hex(0xffd43b) : WHITE, st[i].place ? "#" + I(st[i].place) : I((int)st[i].prog) + "%", 0, TS_DISPLAY | TS_SHADOW);
    }
    Text(800, 52, 44, WHITE, cd > 0 ? "GET READY TO SHAKE!" : "🛷 SLED DASH", 0, TS_DISPLAY | TS_SHADOW | TS_GOLD);
    Hint("📳 Shake the phone (or tap fast) · falling behind? Every shake counts extra!", Hex(0xd0ebff));
    DrawCount();
  }
};

// ===================================================================================== SNOW CURLING
struct Curling : Classic {
  struct St { int who; float x, z, vx, vz, spin = 0; bool out = false; };
  std::vector<St> stones; std::vector<int> order; int turn = -1, cur = -1; std::string phase = "intro"; float t = 2, osc = 0, osc2 = 0, sweep = 0, botAt = 1, waitT = 0, lockedAim = 0;
  const float HX = 10, HZ = 0, R = 0.55f, X0 = -10, Z0 = -4.2f, Z1 = 4.2f; bool fin = false; St* live = nullptr; float camX = -4;
  void Start() override {
    InitCam(40); sc.Init(30, 20, true);
    for (int k = 0; k < 2; k++) for (int i = 0; i < N(); i++) order.push_back(i);
    Layout(-1, kit::Msg("🥌", "Snow Curling", "Wait for your turn — slide closest to the target!"));
  }
  float AimV() { return sinf(osc * 1.6f) * 0.16f; }
  float PowV() { return 0.5f - cosf(osc2 * 2.0f) * 0.5f; }
  void Next() {
    turn++; live = nullptr;
    if (turn >= (int)order.size()) { phase = "done"; t = 3.5f; auto o = Rank(); Banner(P(o[0]).name + " IS CLOSEST!", 2400, kit::Hx(P(o[0]).color).c_str()); Sfx("win"); conf.Spawn({ HX, 3, HZ }, 120, 3, 6); return; }
    cur = order[turn]; phase = "aim"; t = 10; osc = 0; osc2 = 0; sweep = 0; botAt = frand(1.0f, 2.4f);
    Banner(P(cur).name + "'s throw", 1200, kit::Hx(P(cur).color).c_str());
    for (int i = 0; i < N(); i++) if (!P(i).bot) Layout(i, i == cur ? "{\"name\":\"curl\"}" : kit::Msg("👀", P(cur).name + " is throwing", "Get ready to cheer!"));
  }
  void Press() {
    if (phase == "aim") { lockedAim = AimV(); phase = "power"; t = 10; osc2 = 0; Sfx("tick"); botAt = frand(0.8f, 2.0f); if (!P(cur).bot) Layout(cur, "{\"name\":\"button\",\"label\":\"THROW!\",\"hint\":\"Tap when the bar is in the green zone\"}"); }
    else if (phase == "power") {
      float pw = PowV(), sp = 7 + pw * 10;
      stones.push_back({ cur, X0, 0, cosf(lockedAim) * sp, sinf(lockedAim) * sp }); live = &stones.back();
      phase = "slide"; Sfx("whoosh");
      if (!P(cur).bot) Layout(cur, "{\"name\":\"curl\",\"sweep\":true}");
    } else if (phase == "slide") sweep = fminf(1, sweep + 0.14f);
  }
  void Act(int p, const std::string& k, int i, float pw) override {
    if (p != cur) return;
    if (k == "curl" && (phase == "aim" || phase == "power")) { lockedAim = Clamp(i / 1000.f, -1, 1) * 0.16f; phase = "power"; osc2 = acosf(Clamp(1 - 2 * (0.15f + Clamp(pw, 0, 1) * 0.6f), -1, 1)) / 2.0f; Press(); }
    else if (k == "sweep" && phase == "slide") Press();
  }
  float Dist(const St& s) { return hypotf(s.x - HX, s.z - HZ); }
  float Best(int p) { float b = 1e9f; for (auto& s : stones) if (s.who == p && !s.out) b = fminf(b, Dist(s)); return b; }
  std::vector<int> Rank() { return Sorted([&](int i) { return -Best(i); }); }
  void Update(float dt) override {
    time += dt; fx.Update(dt); conf.Update(dt); osc += dt; osc2 += dt; t -= dt;
    if (phase == "intro") { if (t <= 0) Next(); return; }
    if (phase == "done") { if (t <= 0 && !fin) { fin = true; FinishOrder(Rank(), [&](int i) { float b = Best(i); return b > 1e8f ? std::string("no stone left") : I((int)(b * 30)) + " cm away"; }); } return; }
    if (cur >= 0 && P(cur).a && !P(cur).pa) Press();
    if (phase == "aim" || phase == "power") {
      if (P(cur).bot) { botAt -= dt; bool good = phase == "aim" ? fabsf(AimV()) < 0.04f : fabsf(PowV() - 0.42f) < 0.08f; if (botAt <= 0 && (good || botAt < -3)) Press(); }
      if (t <= 0) Press();
    }
    bool moving = false;
    for (auto& s : stones) {
      if (s.out) continue;
      float sp = hypotf(s.vx, s.vz);
      if (sp > 0.05f) {
        moving = true; float fr = (&s == live ? 3.4f - sweep * 2.0f : 3.4f) * dt, ns = fmaxf(0, sp - fr);
        s.vx *= ns / sp; s.vz *= ns / sp; s.x += s.vx * dt; s.z += s.vz * dt; s.spin += sp * dt * 0.4f;
        if (s.z < Z0 + R || s.z > Z1 - R) { s.vz = -s.vz * 0.6f; s.z = Clamp(s.z, Z0 + R, Z1 - R); }
        if (s.x > HX + 4.5f) { s.out = true; fx.Burst({ s.x, 0.4f, s.z }, 15, WHITE, 4, 0.6f, 0.1f, 3); }
      } else s.vx = s.vz = 0;
    }
    if (live && sweep > 0) { sweep = fmaxf(0, sweep - dt * 0.8f); if (frand(0, 1) < 0.5f) fx.Puff({ live->x + 1.2f, 0.2f, live->z + frand(-0.4f, 0.4f) }, WHITE, 0.2f, 0.4f, { 1, 0.6f, 0 }); }
    if (cur >= 0 && P(cur).bot && phase == "slide" && frand(0, 1) < dt * 4) Press();
    for (size_t i = 0; i < stones.size(); i++) for (size_t j = i + 1; j < stones.size(); j++) {
      St& a = stones[i]; St& b = stones[j]; if (a.out || b.out) continue;
      float dx = b.x - a.x, dz = b.z - a.z, d = hypotf(dx, dz), mn = R * 2;
      if (d < mn && d > 0.001f) {
        float nx = dx / d, nz = dz / d, ov = (mn - d) / 2; a.x -= nx * ov; a.z -= nz * ov; b.x += nx * ov; b.z += nz * ov;
        float rv = (b.vx - a.vx) * nx + (b.vz - a.vz) * nz;
        if (rv < 0) { float j2 = -rv * 0.95f; a.vx -= j2 * nx; a.vz -= j2 * nz; b.vx += j2 * nx; b.vz += j2 * nz; Sfx("bonk"); fx.Burst({ a.x + nx * R, 0.4f, a.z + nz * R }, 8, WHITE, 3, 0.4f, 0.1f); }
      }
    }
    if (phase == "slide" && !moving) { phase = "wait"; waitT = 1.2f; }
    if (phase == "wait") { waitT -= dt; if (waitT <= 0) Next(); }
  }
  void Stone(const St& s) {
    Color c = P(s.who).color;
    Matrix m = MatrixMultiply(MatrixMultiply(MatrixScale(R / 0.145f, R / 0.145f, R / 0.145f), MatrixRotateY(s.spin)), MatrixTranslate(s.x, 0.01f, s.z));
    DrawProp(props.stone, m); DrawProp(props.stoneHandle, m, c);  // polished granite + coloured handle, real proportions
    lit.Caster({ s.x, 0, s.z }, R);
  }
  void Draw() override {
    cam.position = { -19, 10, 0 }; cam.target = { 4, 0, 0 }; cam.fovy = 38;
    sc.Begin(cam);
    lit.Pattern(PAT_ICE); lit.Draw(prim.cube, TRS({ 1, 0.01f, 0 }, { 0, 0, 0 }, { 28, 0.02f, Z1 - Z0 }), Hex(0xf1f8ff)); lit.Pattern(PAT_NONE);
    for (float z : { Z0, Z1 }) lit.Draw(prim.cube, TRS({ 1, 0.2f, z }, { 0, 0, 0 }, { 28, 0.4f, 0.2f }), Hex(0x4dabff));
    const unsigned RC[] = { 0x4dabff, 0xffffff, 0xff6b6b, 0xffffff }; const float RR[] = { 3.2f, 2.3f, 1.4f, 0.5f };
    for (int k = 0; k < 4; k++) lit.Draw(prim.cyl, TRS({ HX, 0.025f + k * 0.004f, HZ }, { 0, 0, 0 }, { RR[k], 0.01f, RR[k] }), Hex(RC[k]));
    lit.Draw(prim.cube, TRS({ 1, 0.035f, 0 }, { 0, 0, 0 }, { 28, 0.005f, 0.05f }), Hex(0xadb5bd));
    sc.Trees(time);
    for (auto& s : stones) if (!s.out) Stone(s);
    if (phase == "aim" || phase == "power") {
      float a = phase == "aim" ? AimV() : lockedAim; Color c = P(cur).color;
      for (int k = 1; k < 12; k++) { float d = k * 1.8f; lit.Draw(prim.sphere, TRS({ X0 + cosf(a) * d, 0.1f, sinf(a) * d }, { 0, 0, 0 }, { 0.18f, 0.06f, 0.18f }), k % 2 ? c : WHITE); }
      DrawToon({ X0 - 1.2f, 0, 0 }, PI / 2, c, time, 0, 0, 1.3f);
      Stone({ cur, X0, 0, 0, 0 });
    } else if (cur >= 0 && phase != "done") DrawToon({ X0 - 1.2f, 0, 0 }, PI / 2, P(cur).color, time, 0, 0.4f, 1.3f);
    fx.Draw(); conf.Draw(); EndMode3D();
    if (phase == "power") {
      float pw = PowV(); kit::Glass(560, 790, 480, 52, 20);
      DrawRectangleRounded({ 576, 806, 448, 20 }, 1, 8, Hex(0x1b2550));
      DrawRectangleRounded({ 576 + 448 * 0.32f, 806, 448 * 0.22f, 20 }, 1, 8, Hex(0x34c77b));
      DrawRectangleRounded({ 576, 806, 448 * pw, 20 }, 1, 8, Fade(pw > 0.75f ? Hex(0xff4d6d) : Hex(0xffd43b), 0.85f));
      Text(800, 776, 20, WHITE, "POWER — tap THROW in the green zone", 0, TS_HEAVY | TS_SHADOW);
    }
    if (live && sweep > 0.05f && phase == "slide") { Vector2 s = GetWorldToScreen({ live->x, 1.4f, live->z }, cam); Text(s.x, s.y, 28, WHITE, "SWEEP! SWEEP!", 0, TS_DISPLAY | TS_SHADOW); }
    Board("CLOSEST (cm)", Rank(), [&](int i) { float b = Best(i); return b > 1e8f ? std::string("-") : I((int)(b * 30)); });
    int left = (int)order.size() - std::max(0, turn);
    Hint(phase == "done" ? "FINAL!" : phase == "aim" ? "🥌 " + P(cur).name + ": LOCK the aim when the arrow points at the target" : phase == "power" ? "🥌 " + P(cur).name + ": THROW in the green zone" : I(left) + " throws left · closest to the middle wins", Hex(0xd0ebff));
  }
};
}  // namespace

static RegGame w1("ski", [] { return std::unique_ptr<Game>(new Ski); });
static RegGame w2("sled", [] { return std::unique_ptr<Game>(new Sled); });
static RegGame w3("curling", [] { return std::unique_ptr<Game>(new Curling); });
