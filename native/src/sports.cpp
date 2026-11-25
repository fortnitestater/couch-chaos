// SWING SPORTS in C++: Strike Bowling (tilt to aim, swing to roll — with bumpers, so no gutter balls)
// and Home Run Derby (everyone bats at the same pitch; a generous timing window and a closing ring to time it).
#include "classic.h"
#include "kit.h"

namespace {
using kit::I;
std::string SwingL(const char* sport, const char* label, const char* hint) { return std::string("{\"name\":\"swing\",\"sport\":\"") + sport + "\",\"label\":\"" + label + "\",\"aim\":true,\"hint\":" + kit::Q(hint) + "}"; }

// ===================================================================================== STRIKE BOWLING
struct Bowling : Classic {
  struct Pin { float x, z, vx = 0, vz = 0, fall = 0, dir = 0; bool down = false, gone = false; };
  struct Ball { float x, z, vx, vz, y = 0.11f, rot = 0; bool on = false; } ball;
  std::vector<Pin> pins; std::vector<int> order, pts, strikes; int ti = -1, cur = 0, roll = 0, firstDown = 0, frames = 3, frameNo = 1;
  std::string phase = "aim", pop; Color popC = WHITE; float wait = 0, rollT = 0, aim = 0, camZ = -3, ts = 1, popT = 0, swingAnim = 0; bool again = false, fin = false;
  void Start() override {
    InitCam(50); frames = N() > 4 ? 2 : 3; pts.assign(N(), 0); strikes.assign(N(), 0);
    for (int f = 0; f < frames; f++) for (int i = 0; i < N(); i++) order.push_back(i);
    Layout(-1, kit::Msg("🎳", "Bowling!", "Wait for your turn"));
    NextTurn();
  }
  void SetPins() { pins.clear(); for (int r = 0; r < 4; r++) for (int i = 0; i <= r; i++) pins.push_back({ (i - r / 2.f) * 0.305f, 18.3f + r * 0.264f }); }
  void Pop(const std::string& s, Color c) { pop = s; popC = c; popT = 2.2f; }
  void NextTurn() {
    ti++;
    if (ti >= (int)order.size()) { phase = "end"; wait = 3; auto o = Sorted([&](int i) { return pts[i] * 10.f + strikes[i]; }); Sfx("win"); conf.Spawn({ 0, 2, 4 }, 140, 3, 5); Banner(P(o[0]).name + " WINS!", 2400, kit::Hx(P(o[0]).color).c_str()); return; }
    cur = order[ti]; roll = 0; SetPins(); frameNo = ti / N() + 1; AimPhase();
  }
  void AimPhase() {
    phase = "aim"; wait = P(cur).bot ? frand(1.8f, 2.8f) : 20; ball.on = false; camZ = -3.4f;
    for (int i = 0; i < N(); i++) if (!P(i).bot) Layout(i, i == cur ? SwingL("bowling", "ROLL!", "Tilt or drag to move the 🎯 line, then swing your arm (or tap ROLL)") : kit::Msg("🎳", P(cur).name + " is bowling", "Watch the TV!"));
    Pop(P(cur).name + (roll == 0 ? " · FRAME " + I(frameNo) + "/" + I(frames) : " · 2ND BALL"), P(cur).color);
  }
  void Release(float power) {
    if (phase != "aim") return;
    float x = -Clamp(aim, -1, 1) * 0.42f, spd = 7 + power * 8;
    // gentle aim assist: close to the middle → nudged toward the head pin
    float vx = -x * 0.09f + frand(-0.015f, 0.015f); if (fabsf(x) < 0.12f) vx = -x * 0.35f;
    ball = { x, 0.3f, vx, spd, 0.11f, 0, true };
    phase = "roll"; rollT = 0; swingAnim = 1; Sfx("whoosh"); Shake(3);
    if (!P(cur).bot) Layout(cur, kit::Msg("🎳", "Rolling…", power > 0.8f ? "Monster throw! 💪" : "Fingers crossed!"));
  }
  void Act(int p, const std::string& k, int, float pw) override { if (p == cur && k.rfind("swing", 0) == 0) Release(Clamp(pw > 0 ? pw : 0.6f, 0.25f, 1)); }
  void Knock(Pin& pn) { if (pn.down) return; bool first = true; for (auto& q : pins) if (q.down && !q.gone) first = false; if (first) Sfx("strike"); pn.down = true; pn.dir = atan2f(pn.vx, pn.vz); Sfx("bonk"); fx.Burst({ pn.x, 0.25f, pn.z }, 8, WHITE, 2.5f, 0.4f, 0.03f, 3); }
  void Score() {
    ts = 1; int newly = 0; for (auto& p : pins) newly += p.down && !p.gone;
    int before = roll == 0 ? 0 : firstDown, down = before + newly, gain = newly; again = false;
    if (roll == 0 && down == 10) { gain = 20; Pop("💥 STRIKE!! 💥", Hex(0xffd43b)); strikes[cur]++; Sfx("cheer"); conf.Spawn({ 0, 1.5f, 19 }, 120, 1.5f, 4); Shake(10); }
    else if (roll == 1 && down == 10) { gain = 15 - before; Pop("✨ SPARE! ✨", Hex(0x51cf66)); Sfx("applause"); fx.Burst({ 0, 1, 19 }, 60, Hex(0x51cf66), 5, 1, 0.05f, 3); }
    else { Pop(down == 0 && roll == 0 ? "OOPS… 0" : I(newly) + " PIN" + (newly == 1 ? "" : "S"), WHITE); if (roll == 0) { again = true; firstDown = down; } Sfx("ding"); }
    pts[cur] += gain;
    if (again) { for (auto& p : pins) { if (p.down) p.gone = true; p.vx = p.vz = 0; } roll = 1; }
    phase = "show"; wait = down == 10 ? 3 : 2.2f;
  }
  void Update(float dt) override {
    time += dt; fx.Update(dt); conf.Update(dt); popT -= dt; swingAnim = fmaxf(0, swingAnim - dt * 2);
    if (phase == "end") { wait -= dt; if (wait <= 0 && !fin) { fin = true; FinishOrder(Sorted([&](int i) { return pts[i] * 10.f + strikes[i]; }), [&](int i) { return I(pts[i]) + " pts" + (strikes[i] ? " · " + I(strikes[i]) + " strikes" : ""); }); } return; }
    if (phase == "aim") {
      aim = P(cur).bot ? sinf(time * 1.6f) * 0.35f : aim + (Clamp(P(cur).stick.x, -1, 1) - aim) * fminf(1, dt * 12);
      if (!P(cur).bot && P(cur).a && !P(cur).pa) Release(0.6f);
      wait -= dt; if (wait <= 0) Release(P(cur).bot ? frand(0.5f, 1) : 0.55f);
      return;
    }
    if (phase == "roll") {
      float sdt = dt * ts; Ball& b = ball; rollT += dt;
      b.z += b.vz * sdt; b.x += b.vx * sdt; b.rot += b.vz * sdt / 0.11f;
      if (b.z < 17.6f && fabsf(b.x) > 0.46f) { b.x = copysignf(0.46f, b.x); b.vx = -b.vx * 0.5f - copysignf(0.05f, b.x); Sfx("tink"); }  // bumpers!
      if (b.z > 21.5f) b.y -= sdt * 2;
      for (auto& pn : pins) {
        if (pn.gone) continue;
        if (b.z < 21) { float dx = pn.x - b.x, dz = pn.z - b.z, d = hypotf(dx, dz); if (d < 0.17f && d > 0) { float k = fminf(6, b.vz * 0.45f); pn.vx += dx / d * k + b.vx * 0.3f; pn.vz += dz / d * k + b.vz * 0.25f; b.vx -= dx / d * 0.35f; b.vz *= 0.93f; Knock(pn); } }
        if (hypotf(pn.vx, pn.vz) > 0.05f) for (auto& o : pins) { if (&o == &pn || o.gone) continue; float dx = o.x - pn.x, dz = o.z - pn.z, d = hypotf(dx, dz); if (d < 0.14f && d > 0) { float sp = fminf(5, hypotf(pn.vx, pn.vz) * 0.8f); o.vx += dx / d * sp; o.vz += dz / d * sp; Knock(o); } }
      }
      for (auto& pn : pins) {
        if (pn.gone) continue;
        pn.x += pn.vx * sdt; pn.z += pn.vz * sdt;
        if (fabsf(pn.x) > 0.7f) { pn.x = copysignf(0.7f, pn.x); pn.vx *= -0.4f; }
        if (pn.z > 21.1f) { pn.z = 21.1f; pn.vz = 0; pn.vx *= 0.5f; }
        float k = powf(0.12f, sdt); pn.vx *= k; pn.vz *= k;
        if (pn.down) { pn.fall = fminf(1, pn.fall + sdt * 5); if (hypotf(pn.vx, pn.vz) > 0.2f) pn.dir = atan2f(pn.vx, pn.vz); }
      }
      bool near = b.z > 16.5f && b.z < 19.5f && roll == 0;
      ts += ((near ? 0.35f : 1) - ts) * fminf(1, dt * 8);
      bool moving = false; for (auto& p : pins) if (!p.gone && hypotf(p.vx, p.vz) > 0.06f) moving = true;
      if ((b.z > 21 && !moving) || rollT > 7) Score();
    }
    if (phase == "show") { wait -= dt; if (wait <= 0) { if (again) AimPhase(); else NextTurn(); } }
  }
  void Draw() override {
    float tz = ball.on ? fminf(ball.z - 3.0f, 14.8f) : -3.4f; camZ += (tz - camZ) * 0.08f;
    cam.position = { ball.on ? ball.x * 0.4f : 0, ball.on ? 1.05f : 2.0f, camZ }; cam.target = { ball.on ? ball.x * 0.3f : 0, 0.1f, camZ + 13 };
    UI::Sky(Hex(0x0b0620), Hex(0x1a0b3a));
    lit.Begin(cam, { 0.04f, 0.02f, 0.1f }, 0.02f);
    BeginMode3D(cam);
    lit.Draw(prim.plane, TRS({ 0, -0.02f, 15 }, { 0, 0, 0 }, { 40, 1, 60 }), Hex(0x1a1033));
    lit.Pattern(PAT_WOOD);
    lit.Draw(prim.cube, TRS({ 0, -0.01f, 10.2f }, { 0, 0, 0 }, { 1.06f, 0.02f, 22.4f }), Hex(0xe0b27a));
    for (int sd = -1; sd <= 1; sd += 2) for (int k = 1; k <= 2; k++) lit.Draw(prim.cube, TRS({ sd * 1.9f * k, -0.01f, 10.2f }, { 0, 0, 0 }, { 1.06f, 0.02f, 22.4f }), Hex(0xb98250));
    lit.Pattern(PAT_NONE);
    for (int i = -3; i <= 3; i++) lit.Draw(prim.cone, TRS({ i * 0.12f, 0.005f, 4.6f + fabsf((float)i) * 0.3f }, { PI / 2, 0, 0 }, { 0.04f, 0.12f, 0.01f }), Hex(0x7b2cbf));
    lit.Draw(prim.cube, TRS({ 0, 0.005f, 1 }, { 0, 0, 0 }, { 1.06f, 0.01f, 0.05f }), Hex(0xe03131));
    for (int sd = -1; sd <= 1; sd += 2) {
      lit.Draw(prim.cube, TRS({ sd * 0.6f, 0.08f, 9.0f }, { 0, 0, 0 }, { 0.1f, 0.16f, 17.4f }), Hex(0xffd43b));  // bumper rails
      lit.Draw(prim.cube, TRS({ sd * 0.82f, 0.15f, 10.2f }, { 0, 0, 0 }, { 0.1f, 0.3f, 22.4f }), Hex(0x1c1440));
      lit.Draw(prim.cube, TRS({ sd * 0.82f, 0.32f, 10.2f }, { 0, 0, 0 }, { 0.05f, 0.05f, 22.4f }), sd < 0 ? Hex(0xff4d6d) : Hex(0x4dabff));
    }
    lit.Draw(prim.cube, TRS({ 0, 1.6f, 22.2f }, { 0, 0, 0 }, { 12, 3.2f, 0.3f }), Hex(0x0d0820));
    for (auto& pn : pins) {
      if (pn.gone) continue;
      Matrix m = MatrixMultiply(MatrixMultiply(MatrixRotateX(pn.fall * 1.45f), MatrixRotateY(pn.dir)), MatrixTranslate(pn.x, 0, pn.z));
      lit.Draw(prim.cyl, MatrixMultiply(TRS({ 0, 0, 0 }, { 0, 0, 0 }, { 0.055f, 0.26f, 0.055f }), m), WHITE);
      lit.Draw(prim.sphere, MatrixMultiply(TRS({ 0, 0.31f, 0 }, { 0, 0, 0 }, { 0.035f, 0.05f, 0.035f }), m), WHITE);
      lit.Draw(prim.cyl, MatrixMultiply(TRS({ 0, 0.24f, 0 }, { 0, 0, 0 }, { 0.042f, 0.025f, 0.042f }), m), Hex(0xe03131));
      if (!pn.down) lit.Caster({ pn.x, 0, pn.z }, 0.07f);
    }
    float ax = -Clamp(aim, -1, 1) * 0.42f;
    Vector3 bp = ball.on ? Vector3{ ball.x, ball.y, ball.z } : Vector3{ ax, 0.11f, 0.35f };
    lit.Draw(prim.sphere, TRS(bp, { ball.rot, 0, 0 }, { 0.11f, 0.11f, 0.11f }), P(cur).color); lit.Caster({ bp.x, 0, bp.z }, 0.1f);
    if (phase == "aim") for (int i = 0; i < 22; i++) { float z = 1.8f + i * 0.72f; lit.Draw(prim.cyl, TRS({ ax, 0.006f, z }, { 0, 0, 0 }, { 0.05f, 0.005f, 0.05f }), Fade(P(cur).color, 0.9f * (1 - i / 26.f) * (0.6f + 0.4f * sinf(time * 8 - i * 0.6f)))); }
    if (!ball.on || ball.z < 4) DrawToon({ 1.45f, 0, 1.3f }, -0.6f, P(cur).color, time, 0, swingAnim, 1.0f);
    fx.Draw(); conf.Draw();
    // neon arches
    BeginBlendMode(BLEND_ADDITIVE);
    for (int i = 0; i < 6; i++) { Color c = ColorFromHSV(fmodf(time * 70 + i * 60, 360), 0.8f, 1); for (int k = 0; k < 20; k++) { float a0 = PI * k / 20, a1 = PI * (k + 1) / 20; DrawCylinderEx({ cosf(a0) * 3.2f, sinf(a0) * 3.2f, 3 + i * 3.5f }, { cosf(a1) * 3.2f, sinf(a1) * 3.2f, 3 + i * 3.5f }, 0.04f, 0.04f, 5, Fade(c, 0.85f)); } }
    BeginBlendMode(BLEND_ALPHA);
    EndMode3D();
    Vector2 sg = GetWorldToScreen({ 0, 2.45f, 22 }, cam);
    if (sg.y > 0) Text(sg.x, sg.y, fminf(60, 900 / fmaxf(4, 22 - camZ)), ColorFromHSV(fmodf(time * 90, 360), 0.7f, 1), "STRIKE ZONE", 0, TS_DISPLAY | TS_SHADOW);
    Board("FRAME " + I(std::min(frameNo, frames)) + "/" + I(frames), Sorted([&](int i) { return (float)pts[i]; }), [&](int i) { return I(pts[i]); });
    if (popT > 0) Text(800, 300, 70 + 20 * EaseBack(fminf(1, (2.2f - popT) * 3)), Fade(popC, fminf(1, popT * 2)), pop, 0, TS_DISPLAY | TS_SHADOW);
    if (phase == "aim") Hint(P(cur).bot ? P(cur).name + " is aiming…" : "🎳 " + P(cur).name + ": tilt / drag to aim, then SWING! (bumpers are up)", P(cur).color);
  }
};

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
  void Draw() override {
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
    DrawToon({ 0, 0.25f, 18.4f }, PI, Hex(0xadb5bd), time, 0, phase == "pitch" && pt < 0 ? Clamp(-pt, 0, 1) : 0, 1.2f);
    for (int i = 0; i < N(); i++) {
      DrawToon({ BX(i) - 0.5f, 0, -0.3f }, PI / 2 - 0.25f, P(i).color, time + i, 0, 0, 1.0f);
      float s = swingA[i % 8], a = -1.2f + (1 - s) * 0 + s * 2.6f;
      Vector3 h = { BX(i) - 0.25f, 1.05f, -0.2f };
      Limb(h, { h.x + cosf(a) * 0.95f * (s > 0 ? 1 : 0.2f) + (s > 0 ? 0 : 0.1f), h.y + (s > 0 ? 0.1f : 0.8f), h.z + sinf(a) * 0.95f * (s > 0 ? 1 : 0.2f) }, 0.045f, Hex(0xc0703a));
    }
    if (phase == "pitch" && pt > 0) { Vector3 b = BallAt(pt); lit.Draw(prim.sphere, TRS(b, { 0, 0, 0 }, { 0.1f, 0.1f, 0.1f }), WHITE); lit.Caster({ b.x, 0, b.z }, 0.08f); }
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

static RegGame s1("bowling", [] { return std::unique_ptr<Game>(new Bowling); });
static RegGame s2("derby", [] { return std::unique_ptr<Game>(new Derby); });
