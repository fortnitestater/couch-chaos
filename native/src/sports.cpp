// SWING SPORTS in C++: Strike Bowling (tilt to aim, swing to roll — with bumpers, so no gutter balls)
// and Home Run Derby (everyone bats at the same pitch; a generous timing window and a closing ring to time it).
#include "classic.h"
#include "kit.h"
#include "mesh.h"

namespace {
using kit::I;
std::string SwingL(const char* sport, const char* label, const char* hint) { return std::string("{\"name\":\"swing\",\"sport\":\"") + sport + "\",\"label\":\"" + label + "\",\"aim\":true,\"hint\":" + kit::Q(hint) + "}"; }


// ===================================================================================== HOME RUN DERBY
struct Derby : Classic {
  struct Sw { float at = 0, pow = 0, aim = 0; bool done = false; };
  struct Fly { bool on = false; Vector3 p, v; float d = 0; int type = 0; std::vector<Vector3> trail; };
  std::vector<Sw> sw; std::vector<Fly> fly; std::vector<int> hr; std::vector<float> dist, aimS; std::vector<std::string> res;
  int n = 0, NP = 8; std::string kind, phase = "cd"; float pt = -1.1f, T = 0.9f, flyT = 0, swingA[8] = {}; bool fin = false;
  void Start() override {
    InitCam(45); hr.assign(N(), 0); dist.assign(N(), 0); aimS.assign(N(), 0); fly.assign(N(), Fly{}); res.assign(N(), "");
    Layout(-1, SwingL("bat", "SWING!", "Tilt / drag to aim left or right field · swing when the ring closes!"));
  }
  float BX(int i) { return (i - (N() - 1) / 2.f) * 1.7f; }
  void NextPitch() {
    n++; if (n > NP) { phase = "end"; flyT = 3; auto o = Sorted([&](int i) { return dist[i]; }); Sfx("win"); conf.Spawn({ 0, 3, 2 }, 150, 4, 6); Banner(P(o[0]).name + " WINS!", 2400, kit::Hx(P(o[0]).color).c_str()); return; }
    const char* K[] = { "FASTBALL", "CHANGE-UP", "CURVEBALL", "FASTBALL", "SLIDER" }; kind = n == NP ? "CRAZY BALL" : K[rand() % 5];
    T = kind == "FASTBALL" ? 0.85f : kind == "CHANGE-UP" ? 1.25f : kind == "CURVEBALL" ? 1.05f : kind == "SLIDER" ? 0.95f : 1.1f;  // a little slower than real: easier to time
    pt = -1.2f; sw.assign(N(), Sw{}); phase = "pitch";
    for (int i = 0; i < N(); i++) { res[i] = ""; if (P(i).bot) { float r = frand(0, 1); sw[i] = { T + (r < 0.15f ? frand(-0.6f, 0.5f) : frand(-0.12f, 0.12f)), frand(0.5f, 1), frand(-1, 1), true }; } }
    Banner("PITCH " + I(n) + "/" + I(NP) + ": " + kind, 1300, kind == "CRAZY BALL" ? "#ff4d6d" : "#ffd43b");
  }
  Vector3 BallAt(float t) {
    float u = Clamp(t / T, 0, 1.3f), x = 0, y = 1.9f + (0.95f - 1.9f) * u, z = 17.6f * (1 - u);
    if (kind == "CURVEBALL") { y -= sinf(u * PI / 2) * 0.35f * u; x = sinf(u * 2) * 0.25f; }
    if (kind == "SLIDER") x = u * u * 0.45f;
    if (kind == "CRAZY BALL") { x = sinf(u * 14) * 0.6f * (1 - u * 0.7f); y += cosf(u * 11) * 0.35f * (1 - u * 0.6f); }
    return { x, y, z };
  }
  void Swing(int i, float pow) {
    swingA[i % 8] = 1; Sfx("whoosh");
    if (phase != "pitch" || sw[i].done || pt < -0.2f) return;
    sw[i] = { pt, pow, Clamp(P(i).stick.x, -1, 1), true };
  }
  void Act(int p, const std::string& k, int, float pw) override { if (p >= 0 && p < N() && k.rfind("swing", 0) == 0) Swing(p, Clamp(pw > 0 ? pw : 0.6f, 0.2f, 1)); }
  void Resolve() {
    for (int i = 0; i < N(); i++) {
      Sw& s = sw[i]; float d = 0; int type = 0; std::string r = "MISS";
      if (s.done) {
        float e = fabsf(s.at - T);
        if (e < 0.24f) { float q = 1 - e / 0.24f; d = roundf((q * 0.72f + s.pow * 0.28f) * 150 + frand(-6, 6)); type = d >= 105 ? 2 : 1; r = type == 2 ? "HOME RUN!" : I((int)d) + " m"; }
        else r = s.at < T ? "TOO EARLY" : "TOO LATE";
      }
      res[i] = r; dist[i] += d; if (type == 2) { hr[i]++; Sfx("bat"); }
      if (d > 0) { float ang = s.aim * 0.7f; Fly& f = fly[i]; f.on = true; f.p = { BX(i), 1, 0.4f }; float tf = 2.2f; f.v = { -sinf(ang) * d / tf, 9.81f * tf / 2 + (type == 2 ? 2 : 0), cosf(ang) * d / tf }; f.d = d; f.type = type; f.trail.clear(); if (!P(i).bot) Buzz(i, type == 2 ? 300 : 80); }
    }
    bool anyHR = false; for (auto& f : fly) anyHR |= f.on && f.type == 2;
    if (anyHR) { Sfx("cheer"); Shake(6); } else Sfx("ding");
    phase = "fly"; flyT = 3.2f;
  }
  void Update(float dt) override {
    time += dt; fx.Update(dt); conf.Update(dt); for (auto& a : swingA) a = fmaxf(0, a - dt * 3);
    for (int i = 0; i < N(); i++) if (!P(i).bot && P(i).a && !P(i).pa) Swing(i, 0.6f);
    for (auto& f : fly) if (f.on) { f.v.y -= 9.81f * dt; f.p = Vector3Add(f.p, Vector3Scale(f.v, dt)); if (f.p.y < 0.5f) { f.p.y = 0.5f; f.v = Vector3Scale(f.v, 0.3f); f.v.y = fabsf(f.v.y); } f.trail.push_back(f.p); if (f.trail.size() > 40) f.trail.erase(f.trail.begin()); }
    if (phase == "cd") { if (Counting(dt, "PLAY BALL!")) return; NextPitch(); return; }
    if (phase == "end") { flyT -= dt; if (flyT <= 0 && !fin) { fin = true; FinishOrder(Sorted([&](int i) { return dist[i]; }), [&](int i) { return I((int)dist[i]) + " m · " + I(hr[i]) + " HR"; }); } return; }
    if (phase == "pitch") {
      pt += dt;
      for (int i = 0; i < N(); i++) if (P(i).bot && !sw[i].done) {}  // bots pre-decided
      for (int i = 0; i < N(); i++) if (P(i).bot && sw[i].done && fabsf(pt - sw[i].at) < dt) swingA[i % 8] = 1;
      if (pt > T + 0.45f) Resolve();
    } else if (phase == "fly") { flyT -= dt; if (flyT <= 0) { for (auto& f : fly) f.on = false; NextPitch(); } }
  }
  void Draw() override { g_toonK = 1; DrawScene(); g_toonK = 1.4f; }
  void DrawScene() {
    bool flying = phase == "fly" && flyT < 2.9f;
    Vector3 cp = flying ? Vector3{ 0, 26, -22 } : Vector3{ 0, 4.6f, -10.5f }, ct = flying ? Vector3{ 0, 0, 60 } : Vector3{ 0, 0.9f, 12 };
    cam.position = Vector3Lerp(cam.position.z == 0 && cam.position.y == 0 ? cp : cam.position, cp, 0.06f); cam.target = Vector3Lerp(cam.target, ct, 0.06f);
    UI::Sky(Hex(0x0a0a2a), Hex(0x2a2a6a));
    lit.Begin(cam, { 0.05f, 0.05f, 0.15f }, 0.004f);
    BeginMode3D(cam);
    lit.Pattern(PAT_GRASS); lit.Draw(prim.plane, TRS({ 0, 0, 50 }, { 0, 0, 0 }, { 300, 1, 300 }), Hex(0x2f9e44)); lit.Pattern(PAT_NONE);
    lit.Draw(prim.cube, TRS({ 0, 0.01f, 13 }, { 0, PI / 4, 0 }, { 19, 0.01f, 19 }), Hex(0xc47a3a));
    lit.Draw(prim.cube, TRS({ 0, 0.02f, 13 }, { 0, PI / 4, 0 }, { 16, 0.01f, 16 }), Hex(0x37b24d));
    lit.Draw(prim.cyl, TRS({ 0, 0, 18.4f }, { 0, 0, 0 }, { 2.7f, 0.25f, 2.7f }), Hex(0xc47a3a));
    lit.Draw(prim.cube, TRS({ 0, 0.03f, 0 }, { 0, 0, 0 }, { 0.44f, 0.01f, 0.44f }), WHITE);
    for (int k = 0; k <= 40; k++) { float a = -0.85f + 1.7f * k / 40.f; lit.Draw(prim.cube, TRS({ sinf(a) * 115, 2.25f, cosf(a) * 115 }, { 0, a, 0 }, { 9.2f, 4.5f, 0.4f }), Hex(0x0b3d6e)); lit.Draw(prim.cube, TRS({ sinf(a) * 115, 4.6f, cosf(a) * 115 }, { 0, a, 0 }, { 9.2f, 0.3f, 0.45f }), Hex(0xffd43b));
      for (int r = 0; r < 4; r++) lit.Draw(prim.cube, TRS({ sinf(a) * (119 + r * 3), 5 + r * 2.2f, cosf(a) * (119 + r * 3) }, { 0, a, 0 }, { 9.2f, 1.6f, 2.6f }), Hex(r % 2 ? 0x364fc7 : 0x4263eb)); }
    for (int q = -2; q <= 2; q++) {   // floodlight towers
      float a = q * 0.36f; Vector3 tp = { sinf(a) * 136, 0, cosf(a) * 136 };
      lit.Draw(prim.cube, TRS({ tp.x, 18, tp.z }, { 0, a, 0 }, { 1.0f, 36, 1.0f }), Hex(0x495057));
      lit.Draw(prim.cube, TRS({ tp.x, 36.5f, tp.z }, { 0, a, 0 }, { 12, 5.5f, 0.8f }), Hex(0x343a40));
      for (int r = 0; r < 3; r++) for (int u = 0; u < 6; u++) DrawSphere({ tp.x + cosf(a) * (u - 2.5f) * 1.7f, 35 + r * 1.6f, tp.z - sinf(a) * (u - 2.5f) * 1.7f - 0.7f }, 0.6f, Hex(0xfff8e0));
    }
    for (int q = 0; q <= 40; q++) {   // the crowd: rows of fans in team colours, bobbing
      float a = -0.85f + 1.7f * q / 40.f;
      for (int r = 0; r < 4; r++) for (int u = 0; u < 7; u++) {
        unsigned hh = (unsigned)(q * 131 + r * 17 + u * 7); static const unsigned cc[] = { 0xff6b6b, 0xffd43b, 0xf8f9fa, 0x74c0fc, 0x69db7c, 0xffa94d };
        float off = (u - 3) * 1.25f, rad = 118.2f + r * 3;
        lit.Draw(prim.cube, TRS({ sinf(a) * rad + cosf(a) * off, 6.3f + r * 2.2f + 0.14f * sinf(time * 3 + (float)(hh % 13)), cosf(a) * rad - sinf(a) * off }, { 0, a, 0 }, { 0.85f, 1.1f, 0.6f }), Hex(cc[hh % 6]));
      }
    }
    DrawToon({ 0, 0.25f, 18.4f }, PI, Hex(0xadb5bd), time, 0, phase == "pitch" && pt < 0 ? Clamp(-pt, 0, 1) : 0, 1.2f);
    for (int i = 0; i < N(); i++) {
      DrawToon({ BX(i) - 0.5f, 0, -0.3f }, PI / 2 - 0.25f, P(i).color, time + i, 0, 0, 1.0f);
      float s = swingA[i % 8], a = -1.2f + (1 - s) * 0 + s * 2.6f;
      Vector3 h = { BX(i) - 0.25f, 1.05f, -0.2f };
      Vector3 bd = s > 0 ? Vector3{ cosf(a), 0.12f, sinf(a) } : Vector3{ 0.15f, 1, -0.2f };  // bat on the shoulder, or swinging through
      DrawProp(props.bat, RacketXf(h, bd, { 0, 0, 1 }, 1.1f));
    }
    if (phase == "pitch" && pt > 0) { Vector3 b = BallAt(pt); DrawProp(props.baseball, MatrixMultiply(MatrixMultiply(MatrixScale(0.06f, 0.06f, 0.06f), MatrixRotateXYZ({ time * 30, time * 7, 0 })), MatrixTranslate(b.x, b.y, b.z))); lit.Caster({ b.x, 0, b.z }, 0.08f); }
    for (int i = 0; i < N(); i++) if (fly[i].on) { auto& f = fly[i]; lit.Draw(prim.sphere, TRS(f.p, { 0, 0, 0 }, { 1.2f, 1.2f, 1.2f }), Shade(P(i).color, 0.3f)); for (size_t k = 1; k < f.trail.size(); k++) Limb(f.trail[k - 1], f.trail[k], 0.25f * k / f.trail.size(), Fade(P(i).color, 0.7f)); }
    // timing ring: closes on the plate exactly when you should swing
    if (phase == "pitch" && pt > -0.6f) { float k = Clamp((T - pt) / 1.2f, 0, 1); DrawCircle3D({ 0, 1, 0.5f }, 0.35f + k * 1.6f, { 0, 1, 0 }, 0, Fade(Hex(0xffd43b), 0.9f)); DrawCircle3D({ 0, 1, 0.5f }, 0.36f + k * 1.6f, { 0, 1, 0 }, 0, Fade(Hex(0xffd43b), 0.9f)); }
    fx.Draw(); conf.Draw();
    EndMode3D();
    if (phase == "pitch" && pt > -0.6f && !flying) { float k = Clamp((T - pt) / 1.2f, 0, 1); Vector2 c = GetWorldToScreen({ 0, 1, 0.5f }, cam); DrawRing(c, 34 + k * 160, 42 + k * 160, 0, 360, 64, Fade(Hex(0xffd43b), 0.85f)); DrawRing(c, 30, 36, 0, 360, 48, Fade(WHITE, 0.6f)); }
    for (int i = 0; i < N(); i++) {
      Vector2 s = GetWorldToScreen({ BX(i) - 0.5f, 2.3f, -0.3f }, cam);
      if (!flying) { Text(s.x, s.y, 18, P(i).color, P(i).name, 0, TS_HEAVY); if (!res[i].empty()) Text(s.x, s.y - 30, 24, res[i][0] == 'H' ? Hex(0xffd43b) : res[i][0] == 'T' || res[i][0] == 'M' ? Hex(0xff8787) : WHITE, res[i], 0, TS_DISPLAY | TS_SHADOW); }
      if (fly[i].on && flying) { Vector2 f = GetWorldToScreen(fly[i].p, cam); Text(f.x, f.y - 30, 22, P(i).color, P(i).name + (fly[i].type == 2 ? " 💥 HR!" : ""), 0, TS_HEAVY | TS_SHADOW); }
    }
    Board("DISTANCE (m)", Sorted([&](int i) { return dist[i]; }), [&](int i) { return I((int)dist[i]); });
    Text(800, 52, 40, WHITE, n > 0 && n <= NP ? "PITCH " + I(n) + "/" + I(NP) + " · " + kind : "HOME RUN DERBY", 0, TS_DISPLAY | TS_SHADOW | TS_GOLD);
    Hint("⚾ Swing when the gold ring shrinks onto the plate · tilt to aim left / right field", Hex(0xffe066));
    DrawCount();
  }
};
}  // namespace

static RegGame s2("derby", [] { return std::unique_ptr<Game>(new Derby); });
