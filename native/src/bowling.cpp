// STRIKE BOWLING, in a real bowling alley: eleven lanes, other people bowling on theirs, a birthday party on one lane and
// a buffet on another. You throw from your phone (slide, flick, curve the flick for a hook, nudge it while it rolls).
// The physics is real-ish: a ball on an oiled lane that skids and then grips (the hook comes late), and ten pins that are
// separate bodies: struck, they tip over an edge or wobble back, slide, and knock each other down.
#include "classic.h"
#include "kit.h"
#include "mesh.h"
#include "rlgl.h"
#include <cstring>

namespace {
using kit::I;
constexpr float PITCH = 1.55f, HEAD_Z = 18.29f, PIT_Z = 19.5f, BACK_Z = 21.1f;
constexpr float PIN_R = 0.0605f, PIN_M = 1.55f, BALL_R = 0.1085f, BALL_M = 6.35f;
constexpr int LMIN = -5, LMAX = 5, BIRTHDAY = -2, FOOD = 2;
constexpr float AMAX = 0.9f;   // m/s^2 of sideways grip on the dry boards at full hook
inline float LaneX(int k) { return k * PITCH; }
// the oil: slick for the first 11 m, then the back end grabs
inline float Prof(float z) { float t = Clamp((z - 11.5f) / 4.0f, 0.f, 1.f); return 0.12f + 0.88f * t * t * (3 - 2 * t); }
// sideways displacement from hooking, at depth z, for hook h and a ball doing vz m/s:  AMAX * h * HookI(z) / vz^2
float HookI(float z) { float s = 0; const float st = 0.1f; for (float u = 0; u < z; u += st) s += (z - u - st * 0.5f) * Prof(u + st * 0.5f) * st; return s; }

struct Ball { float x = 0, z = 0, vx = 0, vz = 0, y = BALL_R, rot = 0, ax = 0; bool on = false, bump = true, bumped = false; };
struct PinB { float x = 0, z = 0, vx = 0, vz = 0, tilt = 0, w = 0, dx = 0, dz = 1, y = 0, vy = 0, wobA = 0, wobT = 0, pitT = 0; int st = 0; bool inPit = false; };   // st: 0 standing, 1 falling, 2 lying, 3 gone

struct PinSet {
  PinB p[10]; int hits = 0; float bigHit = 0; bool thump = false;
  static void Home(int i, float& x, float& z) { int r = 0, k = i; while (k > r) { r++; k -= r; } x = (k - r / 2.f) * 0.3048f; z = HEAD_Z + r * 0.264f; }
  void Reset(int mask = 1023) { for (int i = 0; i < 10; i++) { PinB q; Home(i, q.x, q.z); q.st = ((mask >> i) & 1) ? 0 : 3; p[i] = q; } }
  bool Up(int i) const { const PinB& q = p[i]; return q.st == 0 && !q.inPit && q.tilt < 0.14f && fabsf(q.x) < 0.56f && q.z < PIT_Z - 0.03f; }
  int Mask() const { int m = 0; for (int i = 0; i < 10; i++) if (Up(i)) m |= 1 << i; return m; }
  int NotYetHit() const { int c = 0; for (auto& q : p) if (q.st == 0 && !q.inPit) c++; return c; }
  bool Moving() const {
    for (auto& q : p) { if (q.st == 3) continue; if (q.st == 1) return true; if (q.inPit && q.pitT < 1.6f) return true; if (q.vx * q.vx + q.vz * q.vz > 0.004f) return true; if (q.wobT > 0.3f) return true; }
    return false;
  }
  // a standing pin is one circle; a pin that is going over (or lying) is a row of circles along its length
  static int Circles(const PinB& a, float* cx, float* cz, float* cr) {
    if (a.st == 0) { cx[0] = a.x; cz[0] = a.z; cr[0] = PIN_R; return 1; }
    float L = 0.38f * sinf(Clamp(a.tilt, 0.f, 1.5708f));
    for (int k = 0; k < 3; k++) { float f = 0.2f + 0.3f * k; cx[k] = a.x + a.dx * L * f; cz[k] = a.z + a.dz * L * f; cr[k] = 0.052f; }
    return 3;
  }
  // an impulse from the side at 'h' metres up: spin it. Hard enough and it goes over; softer and it wobbles and settles
  void Nudge(PinB& q, float dvx, float dvz, float h) {
    if (q.st != 0) return; float s = hypotf(dvx, dvz); if (s < 0.03f) return;
    float w0 = 3.25f * s * (h / 0.11f); q.dx = dvx / s; q.dz = dvz / s; if (w0 > q.w) q.w = w0;
    if (q.w > 2.4f) { q.st = 1; q.tilt = 0.03f; } else { q.wobT = 1.4f; q.wobA = fminf(0.2f, q.w * 0.06f); }
  }
  void BallHit(Ball& b) {
    if (!b.on || b.z < HEAD_Z - 0.5f || b.z > PIT_Z + 0.2f) return;
    for (auto& q : p) {
      if (q.st == 3 || q.inPit) continue; float cx[3], cz[3], cr[3]; int n = Circles(q, cx, cz, cr);
      for (int k = 0; k < n; k++) {
        float dx = cx[k] - b.x, dz = cz[k] - b.z, d = hypotf(dx, dz), R = BALL_R + cr[k]; if (d >= R || d < 1e-5f) continue;
        float nx = dx / d, nz = dz / d, vrel = (q.vx - b.vx) * nx + (q.vz - b.vz) * nz;
        if (vrel < 0) { float j = -(1 + 0.62f) * vrel / (1 / BALL_M + 1 / PIN_M); b.vx -= nx * j / BALL_M; b.vz -= nz * j / BALL_M; q.vx += nx * j / PIN_M; q.vz += nz * j / PIN_M; Nudge(q, nx * j / PIN_M, nz * j / PIN_M, 0.11f); hits++; bigHit = fmaxf(bigHit, -vrel); }
        float pen = R - d; q.x += nx * pen; q.z += nz * pen;
      }
    }
  }
  void PinPin() {
    for (int i = 0; i < 10; i++) for (int j = i + 1; j < 10; j++) {
      PinB &A = p[i], &B = p[j]; if (A.st == 3 || B.st == 3 || A.inPit || B.inPit) continue;
      bool mov = A.st == 1 || B.st == 1 || A.vx * A.vx + A.vz * A.vz > 0.003f || B.vx * B.vx + B.vz * B.vz > 0.003f; if (!mov) continue;
      float ax[3], az[3], ar[3], bx[3], bz[3], br[3]; int na = Circles(A, ax, az, ar), nb = Circles(B, bx, bz, br);
      for (int x = 0; x < na; x++) for (int y = 0; y < nb; y++) {
        float dx = bx[y] - ax[x], dz = bz[y] - az[x], d = hypotf(dx, dz), R = ar[x] + br[y]; if (d >= R || d < 1e-5f) continue;
        float nx = dx / d, nz = dz / d, vrel = (B.vx - A.vx) * nx + (B.vz - A.vz) * nz;
        if (vrel < 0) {   // an inelastic-ish bounce with a little scatter, so no two racks fall the same way
          float dv = -(1 + 0.62f) * vrel / 2 * (1 + frand(-0.1f, 0.1f)), sj = frand(-0.12f, 0.12f) * dv, tx = -nz, tz = nx;
          A.vx -= nx * dv + tx * sj; A.vz -= nz * dv + tz * sj; B.vx += nx * dv + tx * sj; B.vz += nz * dv + tz * sj;
          Nudge(A, -nx * dv, -nz * dv, 0.12f); Nudge(B, nx * dv, nz * dv, 0.12f); hits++; bigHit = fmaxf(bigHit, -vrel); }
        float pen = (R - d) * 0.5f; A.x -= nx * pen; A.z -= nz * pen; B.x += nx * pen; B.z += nz * pen;
      }
    }
  }
  void Integrate(float h) {
    for (auto& q : p) {
      if (q.st == 3) continue;
      if (q.inPit) {   // fallen off the back of the deck into the pit
        q.vy -= 9.8f * h; q.y += q.vy * h; q.x += q.vx * h; q.z += q.vz * h; q.pitT += h; q.tilt = fminf(1.5708f, q.tilt + 3 * h);
        if (q.z > BACK_Z - 0.08f) { q.z = BACK_Z - 0.08f; q.vz *= -0.15f; } if (q.y < -0.35f) { q.y = -0.35f; q.vy = 0; q.vx *= 0.92f; q.vz *= 0.92f; }
        if (q.pitT > 2.4f) q.st = 3; continue;
      }
      float sp = hypotf(q.vx, q.vz), fr = q.st == 0 ? 2.2f : q.st == 1 ? 0.8f : 2.3f;
      if (sp > 1e-4f) { float k = fmaxf(0.f, sp - fr * h) / sp; q.vx *= k; q.vz *= k; }
      q.x += q.vx * h; q.z += q.vz * h;
      if (q.st == 1) { q.tilt += q.w * h; q.w += 40.6f * sinf(q.tilt) * h; if (q.tilt >= 1.5708f) { q.tilt = 1.5708f; q.st = 2; q.w = 0; thump = true; } }
      else if (q.st == 0 && q.wobT > 0) q.wobT -= h;
      float ax = fabsf(q.x);   // the sides: past the deck a pin drops into the gutter, past the dividers it bounces off
      if (ax > 0.55f) { q.y += (-0.1f - q.y) * fminf(1.f, 12 * h); if (q.st == 0) { q.st = 1; q.tilt = 0.4f; q.w = 2; q.dx = copysignf(1.f, q.x); q.dz = 0; } }
      if (ax > 0.76f) { q.x = copysignf(0.76f, q.x); q.vx *= -0.25f; }
      if (q.z > PIT_Z) { q.inPit = true; q.vy = -0.4f; q.vz = fmaxf(q.vz, 0.8f); }
    }
  }
  void Step(float h) { PinPin(); Integrate(h); }
};
void StepPins(PinSet& ps, float dt) { int n = std::max(1, std::min(16, (int)ceilf(dt * 240.f))); for (int i = 0; i < n; i++) ps.Step(dt / n); }
void RollStep(Ball& b, PinSet& ps, float dt, float steer) {
  int n = std::max(1, std::min(16, (int)ceilf(dt * 240.f))); float h = dt / n;
  for (int i = 0; i < n; i++) {
    if (b.z < HEAD_Z) { b.vx += b.ax * Prof(b.z) * h; if (b.z < 15.5f) b.vx += -steer * 0.12f * h; }
    b.x += b.vx * h; b.z += b.vz * h; b.rot += b.vz * h / BALL_R;
    if (b.bump && b.z < 17.9f && fabsf(b.x) > 0.5f) { b.x = copysignf(0.5f, b.x); b.vx *= -0.5f; b.vz *= 0.97f; b.bumped = true; }
    ps.BallHit(b); ps.Step(h);
    if (b.z > PIT_Z) { b.y = fmaxf(BALL_R - 0.35f, b.y - 2.5f * h); b.vz *= fmaxf(0.f, 1 - 5 * h); if (b.z > BACK_Z) { b.z = BACK_Z; b.vz = 0; } }
  }
}

// ----- the people in the alley -----
struct Lane {
  int k = 0, kind = 0;   // kind: 0 a regular lane, 1 the buffet, 2 the birthday party
  PinSet pins; Ball ball;
  // the bowler on this lane
  int ph = 0; float t = 0, wait = 2, sx = 0, nz = -3.9f, slope = 0, hookN = 0, vz = 8; bool rel = false, cheer = false; float cheerT = 0, sfxT = 0; int downN = 0;
  unsigned hsh = 0; Color shirt = WHITE; Color ballCol = WHITE;
};

struct Bowling : Classic {
  std::vector<Lane> lanes; Texture2D carpet{}, banner{}, neon{}, ads{};
  // the phone's plan for this ball: where it starts (bxn, -1..1 screen-right), its slope (baimN), its late curve (bhookN), how hard (bpowN)
  float bxn = 0, bxs = 0, baimN = 0, bhookN = 0, bpowN = 0.7f, sendT = 0; bool aimLive = false; std::string res;
  std::vector<int> order, pts, strikes; int ti = -1, cur = 0, roll = 0, firstDown = 0, frames = 3, frameNo = 1, startMask = 1023;
  std::string phase = "aim", pop; Color popC = WHITE; float wait = 0, rollT = 0, aim = 0, camZ = -3, ts = 1, popT = 0, partyT = 3; bool again = false, fin = false, shot = false, crash = false;
  Lane& Mine() { return lanes[-LMIN]; }
  ~Bowling() { for (Texture2D* t : { &carpet, &banner, &neon, &ads }) if (t->id) UnloadTexture(*t); }

  void Start() override {
    InitCam(58); g_cineK = 0; frames = N() > 4 ? 2 : 3; pts.assign(N(), 0); strikes.assign(N(), 0);
    for (int f = 0; f < frames; f++) for (int i = 0; i < N(); i++) order.push_back(i);
    for (int k = LMIN; k <= LMAX; k++) {
      Lane L; L.k = k; L.kind = k == BIRTHDAY ? 2 : k == FOOD ? 1 : 0; L.pins.Reset(); L.hsh = 5 + (unsigned)(k + 9) * 7; L.wait = frand(0.5f, 6.0f);
      const unsigned SH[] = { 0xe03131, 0x1c7ed6, 0x2f9e44, 0xf08c00, 0x9c36b5, 0x0b7285, 0xd6336c, 0x495057 }; L.shirt = Hex(SH[(k + 9) % 8]); L.ballCol = Hex(SH[(k + 13) % 8]);
      lanes.push_back(L);
    }
    Layout(-1, kit::Msg("🎳", "Bowling!", "Wait for your turn"));
    Sfx("crowdOn"); NextTurn();
  }
  void Pop(const std::string& s, Color c) { pop = s; popC = c; popT = 2.2f; }
  void NextTurn() {
    ti++;
    if (ti >= (int)order.size()) {
      phase = "end"; wait = 3.9f; auto o = Sorted([&](int i) { return pts[i] * 10.f + strikes[i]; }); Sfx("win"); conf.Spawn({ 0, 2, 3 }, 140, 3, 5); Banner(P(o[0]).name + " WINS!", 2400, kit::Hx(P(o[0]).color).c_str());
      cur = o[0]; Mine().ball.on = false; Mine().pins.Reset();
      cine::Shot s; s.kind = cine::WIN; s.at = { 0, 0.95f, -1.7f }; s.size = 0.85f; s.c = P(o[0]).color; s.face = { 0.3f, 0, 1 }; s.floorY = 0; s.hero = true; cine::Play(s);
      return;
    }
    cur = order[ti]; roll = 0; Mine().pins.Reset(); frameNo = ti / N() + 1; AimPhase();
  }
  void AimPhase() {
    phase = "aim"; wait = P(cur).bot ? frand(1.8f, 2.8f) : 20; Mine().ball.on = false; camZ = -3.4f; shot = false; crash = false; startMask = Mine().pins.Mask();
    bxn = bxs = baimN = bhookN = 0; bpowN = 0.7f; aimLive = false; aim = 0;
    for (int i = 0; i < N(); i++) if (!P(i).bot) Layout(i, BowlL(i == cur));
    Pop(P(cur).name + (roll == 0 ? " · FRAME " + I(frameNo) + "/" + I(frames) : " · 2ND BALL"), P(cur).color);
  }
  std::string BowlL(bool mine) {
    char b[320]; snprintf(b, sizeof b, "{\"name\":\"bowl\",\"mine\":%s,\"who\":%s,\"frame\":%d,\"frames\":%d,\"ball\":%d,\"pins\":%d,\"ph\":\"aim\"}", mine ? "true" : "false", kit::Q(P(cur).name).c_str(), std::min(frameNo, frames), frames, roll + 1, Mine().pins.Mask());
    return b;
  }
  void SendBowl(const char* ph, const std::string& msg = "") {   // every phone gets the lane live
    Lane& L = Mine(); char b[260]; snprintf(b, sizeof b, "{\"t\":\"bowl\",\"ph\":\"%s\",\"x\":%.3f,\"z\":%.2f,\"pins\":%d,\"msg\":%s}", ph, -L.ball.x, L.ball.on ? L.ball.z : 0.3f, L.pins.Mask(), kit::Q(msg).c_str());
    for (int i = 0; i < N(); i++) if (!P(i).bot) Send(i, b);
  }
  // The throw. xn: where on the foul line (-1..1, screen-right +) · aimN: the straight part of the line · hookN: how late it curves · power 0..1
  void Throw(float xn, float aimN, float hookN, float power) {
    if (phase != "aim") return;
    Lane& L = Mine(); const float Ln = HEAD_Z;
    float x = -Clamp(xn, -1.f, 1.f) * 0.42f, vz = 6.5f + power * 4.5f, slope = -Clamp(aimN, -1.f, 1.f) * 0.035f, hk = -Clamp(hookN, -1.f, 1.f);
    float xi = x + slope * Ln + AMAX * hk * HookI(Ln) / (vz * vz);
    // a gentle assist: a ball that would arrive near the head pin is nudged toward the nearest pocket
    if (fabsf(xi) < 0.26f) { float tgt = copysignf(0.075f, xi == 0 ? 1.f : xi); float xi2 = xi + (tgt - xi) * 0.4f; slope += (xi2 - xi) / Ln; }
    L.ball = Ball(); L.ball.x = x; L.ball.z = 0.3f; L.ball.vx = slope * vz; L.ball.vz = vz; L.ball.on = true; L.ball.ax = AMAX * hk;
    phase = "roll"; rollT = 0; Sfx("whoosh"); Shake(3); sendT = 0; SendBowl("roll");
  }
  void BotThrow() {
    float xn = Clamp(bxn, -0.6f, 0.6f), x = -xn * 0.42f, tgt = frand(0, 1) < 0.5f ? -0.075f : 0.075f;
    float aimN = -((tgt - x) / HEAD_Z) / 0.035f + frand(-0.22f, 0.22f);
    Throw(xn, aimN, frand(-0.25f, 0.25f), frand(0.4f, 1));
  }
  void Release(float power) { Throw(Clamp(aim, -1.f, 1.f), 0, 0, power); }
  void Act(int p, const std::string& k, int, float pw) override {
    if (p != cur) return;
    float a = 0, b = 0, c = 0;
    if (k.rfind("bowlaim:", 0) == 0) { if (sscanf(k.c_str() + 8, "%f|%f|%f", &a, &b, &c) == 3) { bxn = Clamp(a, -1.f, 1.f); baimN = Clamp(b, -1.f, 1.f); bhookN = Clamp(c, -1.f, 1.f); bpowN = pw; aimLive = true; } return; }
    if (k.rfind("bowl:", 0) == 0) { if (sscanf(k.c_str() + 5, "%f|%f|%f", &a, &b, &c) == 3) Throw(a, b, c, Clamp(pw, 0.18f, 1.f)); return; }
    if (k.rfind("swing", 0) == 0) Release(Clamp(pw > 0 ? pw : 0.6f, 0.25f, 1.f));
  }
  void StrikeShot(bool st) { cine::Shot s; s.kind = cine::STRIKE; s.at = { 0, 0.25f, 19.0f }; s.size = 0.55f; s.c = st ? Hex(0xffd43b) : Hex(0x51cf66); s.caption = st ? "STRIKE!" : "SPARE!"; s.sub = P(cur).name; s.floorY = 0; s.face = { 0, 0, 1 }; s.elev = 0.2f; cine::Play(s); }
  void DebugShot(int k) override {   // debug: a ball straight into the pocket
    if (k != cine::STRIKE) return;
    Lane& L = Mine(); phase = "roll"; roll = 0; shot = false; crash = false; rollT = 0; startMask = 1023; firstDown = 0; L.pins.Reset(); L.ball = Ball(); L.ball.x = 0.07f; L.ball.z = 15.5f; L.ball.vz = 9; L.ball.on = true;
  }
  void Score() {
    Lane& L = Mine(); ts = 1; int endMask = L.pins.Mask(), newly = 0; for (int i = 0; i < 10; i++) if (((startMask >> i) & 1) && !((endMask >> i) & 1)) newly++;
    int before = roll == 0 ? 0 : firstDown, down = before + newly, gain = newly; again = false; res = "";
    if (roll == 0 && down == 10) { res = "STRIKE!"; gain = 20; Pop("STRIKE  +20", Hex(0xffd43b)); strikes[cur]++; Sfx("cheer"); conf.Spawn({ 0, 1.5f, 19 }, 120, 1.5f, 4); Shake(10); }
    else if (roll == 1 && down == 10) { res = "SPARE!"; gain = 15 - before; Pop("SPARE  +" + I(gain), Hex(0x51cf66)); Sfx("applause"); fx.Burst({ 0, 1, 19 }, 60, Hex(0x51cf66), 5, 1, 0.05f, 3); }
    else { res = newly == 0 ? "MISS" : I(newly) + (newly == 1 ? " PIN" : " PINS"); Pop(down == 0 && roll == 0 ? "OOPS… 0" : I(newly) + " PIN" + (newly == 1 ? "" : "S"), WHITE); if (roll == 0) { again = true; firstDown = down; } Sfx("ding"); }
    if (g_dbg == 36) fprintf(stderr, "BOWL ball=%d newly=%d down=%d%c", roll, newly, down, 10);   // debug: print every result
    pts[cur] += gain; if (again) roll = 1;
    phase = "show"; wait = down == 10 ? 3 : 2.4f; SendBowl("show", res);
  }

  // ---------------------------------------------------------------- update
  void Update(float dt) override {
    time += dt; fx.Update(dt); conf.Update(dt); popT -= dt;
    UpdateWorld(dt);
    if (phase == "end") { wait -= dt; if (wait <= 0 && !fin) { fin = true; FinishOrder(Sorted([&](int i) { return pts[i] * 10.f + strikes[i]; }), [&](int i) { return I(pts[i]) + " pts" + (strikes[i] ? " · " + I(strikes[i]) + " strikes" : ""); }); } return; }
    Lane& L = Mine();
    if (phase == "aim") {
      if (P(cur).bot) bxn += (sinf(time * 1.3f) * 0.5f - bxn) * fminf(1.f, dt * 3);
      else if (!aimLive) { aim += (Clamp(P(cur).stick.x, -1.f, 1.f) - aim) * fminf(1.f, dt * 12); bxn = aim; }
      bxs += (bxn - bxs) * fminf(1.f, dt * 14);
      if (!P(cur).bot && P(cur).a && !P(cur).pa) Release(0.6f);
      wait -= dt; if (wait <= 0) { if (P(cur).bot) BotThrow(); else Throw(bxn, baimN, bhookN, 0.55f); }
      return;
    }
    if (phase == "roll") {
      rollT += dt; float sdt = dt * ts; Ball& b = L.ball;
      RollStep(b, L.pins, sdt, P(cur).bot ? 0.f : P(cur).stick.x);
      if (b.bumped) { b.bumped = false; Sfx("tink"); }
      if (L.pins.hits > 0) { if (!crash && L.pins.bigHit > 1.2f) { crash = true; Sfx("strike"); Shake(6); } else Sfx("bonk"); if (L.pins.hits > 1 && frand(0, 1) < 0.5f) fx.Burst({ LaneX(0) + frand(-0.3f, 0.3f), 0.3f, 18.8f }, 3, WHITE, 2, 0.4f, 0.03f, 3); L.pins.hits = 0; L.pins.bigHit = 0; }
      if (!shot && crash && L.pins.NotYetHit() == 0) { shot = true; StrikeShot(roll == 0); }
      sendT -= dt; if (sendT <= 0) { sendT = 0.07f; SendBowl("roll"); }
      bool near = b.z > 16.5f && b.z < 19.5f && roll == 0; ts += ((near ? 0.4f : 1.f) - ts) * fminf(1.f, dt * 8);
      if ((b.z > PIT_Z + 0.4f && !L.pins.Moving()) || rollT > 9) Score();
    }
    if (phase == "show") { wait -= dt; if (wait <= 0) { if (again) AimPhase(); else NextTurn(); } }
  }
  // the whole alley carries on: the pins on every lane, and the other bowlers
  void UpdateWorld(float dt) {
    for (auto& L : lanes) {
      if (std::abs(L.k) == 5) continue;   // the far lanes are just scenery
      if (L.k == 0) { if (L.ball.on && phase != "roll") RollStep(L.ball, L.pins, dt, 0); else if (phase != "roll" && L.pins.Moving()) StepPins(L.pins, dt); continue; }
      if (L.ball.on) RollStep(L.ball, L.pins, dt, 0); else if (L.pins.Moving()) StepPins(L.pins, dt);
      L.sfxT -= dt; if (L.pins.hits > 0) { if (L.sfxT <= 0 && L.pins.bigHit > 1.2f) { Sfx("pinsFar"); L.sfxT = 0.45f; } L.pins.hits = 0; L.pins.bigHit = 0; }
      L.cheerT -= dt; UpdateBowler(L, dt);
    }
    partyT -= dt; if (partyT <= 0) { partyT = frand(4, 8); conf.Spawn({ LaneX(BIRTHDAY) + frand(-0.6f, 0.6f), 3.2f, frand(-1.5f, 1.0f) }, 36, 1.5f, 2); }
  }
  void UpdateBowler(Lane& L, float dt) {
    if (L.kind == 1) return;   // the buffet lane has no bowler
    L.t += dt;
    switch (L.ph) {
      case 0: if (L.t > L.wait) { L.ph = 1; L.t = 0; L.sx = frand(-0.3f, 0.3f); L.rel = false; L.vz = frand(6.8f, 9.6f); L.hookN = frand(-0.55f, 0.55f);
          float tgt = frand(0, 1) < 0.5f ? -0.075f : 0.075f; L.slope = (tgt - L.sx - AMAX * L.hookN * HookI(HEAD_Z) / (L.vz * L.vz)) / HEAD_Z + frand(-0.007f, 0.007f); } break;
      case 1: L.nz = Lerp(-3.9f, -0.7f, Clamp(L.t / 1.7f, 0.f, 1.f)); if (L.t > 1.7f) { L.ph = 2; L.t = 0; } break;
      case 2:
        if (!L.rel && L.t > 0.3f) { L.rel = true; L.ball = Ball(); L.ball.x = L.sx; L.ball.z = -0.1f; L.ball.vx = L.slope * L.vz; L.ball.vz = L.vz; L.ball.on = true; L.ball.ax = AMAX * L.hookN; }
        if (L.t > 0.7f) { L.ph = 3; L.t = 0; } break;
      case 3:
        if ((L.ball.z > PIT_Z + 0.4f && !L.pins.Moving()) || L.t > 8) {
          L.downN = 10 - __builtin_popcount(L.pins.Mask()); L.ph = 4; L.t = 0; L.ball.on = false;
          if (L.downN >= 8) { L.cheer = true; L.cheerT = 2.2f; if (L.kind == 2) conf.Spawn({ LaneX(L.k), 2.6f, 0 }, 60, 1.6f, 3); } else L.cheer = false;
        } break;
      case 4: if (L.t > 2.4f) { L.pins.Reset(); L.ph = 5; L.t = 0; } break;
      case 5: L.nz = Lerp(-0.7f, -3.9f, Clamp(L.t / 1.8f, 0.f, 1.f)); if (L.t > 1.8f) { L.ph = 0; L.t = 0; L.wait = frand(1.5f, 6.0f); L.cheer = false; } break;
    }
  }

  // ---------------------------------------------------------------- drawing helpers
  void LoadTex() {
    if (!carpet.id && HaveFile("carpet.png")) { carpet = LoadTexture("carpet.png"); if (carpet.id) { GenTextureMipmaps(&carpet); SetTextureFilter(carpet, TEXTURE_FILTER_TRILINEAR); SetTextureWrap(carpet, TEXTURE_WRAP_REPEAT); } }
    if (!banner.id && HaveFile("banner.png")) { banner = LoadTexture("banner.png"); if (banner.id) { SetTextureFilter(banner, TEXTURE_FILTER_BILINEAR); } }
    if (!neon.id && HaveFile("neon.png")) { neon = LoadTexture("neon.png"); if (neon.id) SetTextureFilter(neon, TEXTURE_FILTER_BILINEAR); }
    if (!ads.id && HaveFile("ads.png")) { ads = LoadTexture("ads.png"); if (ads.id) { GenTextureMipmaps(&ads); SetTextureFilter(ads, TEXTURE_FILTER_TRILINEAR); } }
  }
  // a textured panel standing on the line a→b (a = its left end as seen from the front), or lying flat when flat is set
  void Quad(Texture2D tx, Vector3 a, Vector3 b, float y0, float y1, float u0 = 0, float u1 = 1, float v0 = 0, float v1 = 1, Color tint = WHITE) {
    rlSetTexture(tx.id); rlBegin(RL_QUADS); rlColor4ub(tint.r, tint.g, tint.b, tint.a);
    rlTexCoord2f(u0, v1); rlVertex3f(a.x, y0, a.z); rlTexCoord2f(u1, v1); rlVertex3f(b.x, y0, b.z); rlTexCoord2f(u1, v0); rlVertex3f(b.x, y1, b.z); rlTexCoord2f(u0, v0); rlVertex3f(a.x, y1, a.z);
    rlEnd(); rlSetTexture(0);
  }
  void Floor(Texture2D tx, float x0, float x1, float z0, float z1, float y, float tile, Color tint) {
    rlSetTexture(tx.id); rlBegin(RL_QUADS); rlColor4ub(tint.r, tint.g, tint.b, 255);
    rlTexCoord2f(0, 0); rlVertex3f(x0, y, z0); rlTexCoord2f(0, (z1 - z0) / tile); rlVertex3f(x0, y, z1); rlTexCoord2f((x1 - x0) / tile, (z1 - z0) / tile); rlVertex3f(x1, y, z1); rlTexCoord2f((x1 - x0) / tile, 0); rlVertex3f(x1, y, z0);
    rlEnd(); rlSetTexture(0);
  }
  void DrawPins(const PinSet& ps, float lx, bool casters) {
    for (int i = 0; i < 10; i++) {
      const PinB& q = ps.p[i]; if (q.st == 3) continue;
      float wob = 0; if (q.st == 0 && q.wobT > 0) { float a = 1.4f - q.wobT; wob = q.wobA * sinf(40 * a) * expf(-2.2f * a); }
      float tl = q.st == 0 ? wob : q.tilt; Vector3 ax = { q.dz, 0, -q.dx }; if (q.st == 0 && q.wobT <= 0) ax = { 1, 0, 0 };
      Matrix m = MatrixMultiply(MatrixRotate(ax, tl), MatrixTranslate(lx + q.x, q.y + (q.st == 0 ? 0.f : 0.058f * sinf(q.tilt)), q.z));
      DrawProp(props.pin, m);
      if (casters && q.st == 0) lit.Caster({ lx + q.x, 0, q.z }, 0.07f);
    }
  }
  void DrawBall(const Ball& b, float lx, Color c, bool caster) {
    Matrix bm = MatrixMultiply(MatrixRotateX(b.rot), MatrixTranslate(lx + b.x, b.y, b.z));
    lit.Draw(prim.sphere, MatrixMultiply(MatrixScale(BALL_R * 2, BALL_R * 2, BALL_R * 2), bm), c);
    const Vector3 H[3] = { { 0.03f, 0.06f, -0.085f }, { -0.03f, 0.06f, -0.085f }, { 0, 0.0f, -0.1f } };
    for (auto& h : H) { Vector3 w = Vector3Transform(h, bm); lit.Draw(prim.sphere, TRS(w, { 0, 0, 0 }, { 0.03f, 0.03f, 0.03f }), Hex(0x111111)); }
    if (caster) lit.Caster({ lx + b.x, 0, b.z }, 0.1f);
  }
  int slotN = 4;   // every person in the alley gets their own mesh, in the order they are drawn
  void Person(float x, float z, float yaw, const char* clip, float t, unsigned hsh, Color shirt, const char* clip2 = nullptr, float t2 = 0, float mix = 0, bool scrub2 = false, Color hat = BLANK, const Ball* carry = nullptr, Color ballCol = WHITE, bool scrub = false) {
    if (g_dbg == 37) return;   // debug: no people (to time them)
    PersonPose q; q.slot = std::min(slotN++, 63); q.lowRate = true; q.pos = { x, 0, z }; q.yaw = yaw; q.c = shirt; q.hsh = hsh; q.height = 1.72f + (hsh % 5) * 0.025f; q.clip = clip; q.t = t; q.scrub = scrub; q.clip2 = clip2; q.t2 = t2; q.scrub2 = scrub2; q.mix = mix;
    Matrix wm = MatrixIdentity(), hm = MatrixIdentity(); q.wrist = &wm; q.headM = &hm;
    if (!DrawPersonPose(q)) return;
    if (hat.a) { Vector3 o = { hm.m12, hm.m13, hm.m14 }; lit.Draw(prim.cone, TRS({ o.x, o.y + 0.07f, o.z }, { 0, 0, 0 }, { 0.15f, 0.27f, 0.15f }), hat); lit.Draw(prim.sphere, TRS({ o.x, o.y + 0.35f, o.z }, { 0, 0, 0 }, { 0.05f, 0.05f, 0.05f }), WHITE); }
    if (carry) { Vector3 o = { wm.m12, wm.m13 - 0.1f, wm.m14 }; lit.Draw(prim.sphere, TRS(o, { 0, 0, 0 }, { BALL_R * 2, BALL_R * 2, BALL_R * 2 }), ballCol); }
  }
  void Balloons(float x, float z, int n, float seed) {
    const unsigned C[] = { 0xff4d6d, 0xffd43b, 0x4dabf7, 0x51cf66, 0xcc5de8, 0xff922b };
    for (int i = 0; i < n; i++) {
      float a = seed + i * 1.7f, bx = x + sinf(a * 2.1f) * 0.45f, bz = z + cosf(a * 1.3f) * 0.3f, by = 2.05f + (i % 3) * 0.28f + 0.05f * sinf(time * 1.3f + a);
      lit.Draw(prim.sphere, TRS({ bx, by, bz }, { 0, 0, 0 }, { 0.27f, 0.33f, 0.27f }), Hex(C[i % 6]));
      lit.Draw(prim.cone, TRS({ bx, by - 0.2f, bz }, { PI, 0, 0 }, { 0.05f, 0.05f, 0.05f }), Hex(C[i % 6]));
      Limb({ bx, by - 0.2f, bz }, { x + sinf(a) * 0.08f, 0.62f, z }, 0.004f, Hex(0xe9ecef));
    }
  }

  // ---------------------------------------------------------------- the building
  void DrawHall(Color) {
    // the floor outside the lanes is carpet with neon shapes; the approaches are polished wood
    if (carpet.id) Floor(carpet, -24, 24, -16, 28, -0.04f, 3.5f, Color{ 150, 150, 175, 255 }); else lit.Draw(prim.plane, TRS({ 0, -0.04f, 6 }, { 0, 0, 0 }, { 48, 1, 44 }), Hex(0x1a1033));
    lit.Pattern(PAT_WOOD); lit.Draw(prim.cube, TRS({ 0, -0.02f, -3.3f }, { 0, 0, 0 }, { PITCH * 11 + 0.6f, 0.04f, 6.6f }), Hex(0xd9b27c)); lit.Pattern(PAT_NONE);
    for (float z : { -1.5f, -2.6f }) for (int k = LMIN; k <= LMAX; k++) for (int d = -1; d <= 1; d++) lit.Draw(prim.cyl, TRS({ LaneX(k) + d * 0.16f, 0.0f, z }, { 0, 0, 0 }, { 0.05f, 0.005f, 0.05f }), Hex(0x2b2b30));   // stance dots
    for (int k = LMIN; k <= LMAX; k++) {
      float lx = LaneX(k);
      lit.Pattern(PAT_WOOD); lit.Draw(prim.cube, TRS({ lx, -0.01f, 9.85f }, { 0, 0, 0 }, { 1.05f, 0.02f, 19.7f }), Hex(0xf2d29c)); lit.Pattern(PAT_NONE);
      for (int s = -1; s <= 1; s += 2) lit.Draw(prim.cube, TRS({ lx + s * 0.65f, -0.07f, 9.85f }, { 0, 0, 0 }, { 0.25f, 0.02f, 19.7f }), Hex(0x14141c));   // gutters
      lit.Draw(prim.cube, TRS({ lx, 0.003f, 0.0f }, { 0, 0, 0 }, { 1.05f, 0.012f, 0.05f }), Hex(0xd7263d));   // foul line
      for (int i = -3; i <= 3; i++) lit.Draw(prim.cone, TRS({ lx + i * 0.12f, 0.005f, 4.6f + fabsf((float)i) * 0.3f }, { PI / 2, 0, 0 }, { 0.04f, 0.12f, 0.01f }), Hex(0x5b2d7a));   // the arrows
      lit.Draw(prim.cube, TRS({ lx, -0.25f, 20.3f }, { 0, 0, 0 }, { 1.5f, 0.5f, 1.6f }), Hex(0x07070c));   // the pit
      lit.Draw(prim.cube, TRS({ lx, 1.2f, 21.2f }, { 0, 0, 0 }, { 1.5f, 2.6f, 0.2f }), Hex(0x15122b));      // kickback
      lit.Draw(prim.cube, TRS({ lx, 3.3f, 21.4f }, { 0, 0, 0 }, { 1.5f, 1.2f, 0.3f }), Hex(0x0f0c22));      // the masking unit over the pins
      lit.Draw(prim.cube, TRS({ lx, 3.7f, 3.02f }, { 0, 0, 0 }, { 0.98f, 0.56f, 0.04f }), Hex(0x2a2a36));   // overhead monitor frame
      if (k < LMAX) {   // a ball return between this lane and the next, with a few balls on it
        float bx = lx + PITCH / 2; lit.Draw(prim.cube, TRS({ bx, 0.28f, -1.6f }, { 0, 0, 0 }, { 0.5f, 0.56f, 1.7f }), Hex(0x1b1f3a)); lit.Draw(prim.cube, TRS({ bx, 0.58f, -1.6f }, { 0, 0, 0 }, { 0.52f, 0.05f, 1.72f }), Hex(0x2fd1ff));
        const unsigned BC[] = { 0xd6336c, 0x1c7ed6, 0x2f9e44, 0xf08c00, 0x9c36b5 }; for (int i = 0; i < 3; i++) lit.Draw(prim.sphere, TRS({ bx, 0.7f, -2.1f + i * 0.5f }, { 0, 0, 0 }, { 0.2f, 0.2f, 0.2f }), Hex(BC[(k + 7 + i) % 5]));
      }
    }
    // walls and ceiling
    lit.Draw(prim.cube, TRS({ 0, 5.3f, 6 }, { 0, 0, 0 }, { 50, 0.3f, 44 }), Hex(0x0b0918));
    lit.Draw(prim.cube, TRS({ 0, 3, 24.5f }, { 0, 0, 0 }, { 50, 7, 0.4f }), Hex(0x0c0a1e));
    for (int s = -1; s <= 1; s += 2) lit.Draw(prim.cube, TRS({ s * 13.5f, 3, 6 }, { 0, 0, 0 }, { 0.4f, 7, 44 }), Hex(0x100c26));
    lit.Draw(prim.cube, TRS({ 0, 3, -13.5f }, { 0, 0, 0 }, { 50, 7, 0.4f }), Hex(0x100c26));
  }
  void DrawGlow() {   // everything self-lit: neon, screens, signs
    rlDrawRenderBatchActive(); BeginBlendMode(BLEND_ADDITIVE);
    for (int i = 0; i < 6; i++) { float z = -2 + i * 4.6f; Color c = ColorFromHSV(fmodf(time * 12 + i * 55, 360), 0.75f, 1); DrawCube({ 0, 5.1f, z }, 26, 0.06f, 0.12f, Fade(c, 0.9f)); DrawCube({ 0, 5.1f, z + 0.5f }, 26, 0.04f, 0.04f, Fade(WHITE, 0.55f)); }
    for (int s = -1; s <= 1; s += 2) for (int i = 0; i < 9; i++) DrawCube({ s * 13.25f, 2.6f, -9 + i * 3.8f }, 0.06f, 4.2f, 0.12f, Fade(ColorFromHSV(fmodf(i * 40 + time * 20, 360), 0.8f, 1), 0.7f));
    for (int k = LMIN; k <= LMAX; k++) {
      float lx = LaneX(k); Color sc = k == 0 ? Hex(0x4dabff) : ColorFromHSV(fmodf(k * 61 + time * 6, 360), 0.45f, 0.9f);
      DrawCube({ lx, 3.7f, 3.0f }, 0.9f, 0.5f, 0.02f, Fade(sc, 0.22f));                        // the overhead score monitor
      DrawCube({ lx, 0.01f, 9.85f }, 1.0f, 0.002f, 19.6f, Color{ 255, 238, 200, 10 });         // a sheen of light down the boards
    }
    rlDrawRenderBatchActive(); EndBlendMode(); BeginBlendMode(BLEND_ALPHA);
    if (ads.id) for (int k = LMIN; k <= LMAX; k++) { float lx = LaneX(k), u0 = ((k + 5 + (int)time / 6) % 8) / 8.f; Quad(ads, { lx + 0.7f, 0, 21.2f }, { lx - 0.7f, 0, 21.2f }, 3.05f, 3.55f, u0, u0 + 0.125f, 0.05f, 0.95f, Color{ 235, 235, 235, 255 }); }
    if (neon.id) { rlDrawRenderBatchActive(); BeginBlendMode(BLEND_ADDITIVE); Quad(neon, { 4.2f, 0, 24.2f }, { -4.2f, 0, 24.2f }, 1.6f, 5.0f); rlDrawRenderBatchActive(); EndBlendMode(); BeginBlendMode(BLEND_ALPHA); }
  }
  void DrawParty(Lane& L) {
    float lx = LaneX(L.k);
    // banner over the lane, balloons by the ball return and floating down the lane, a table with the cake, presents
    if (banner.id) Quad(banner, { lx + 2.2f, 0, 8.5f }, { lx - 2.2f, 0, 8.5f }, 2.4f, 3.75f, 0, 1, 0, 1, WHITE);
    Balloons(lx + 0.85f, -1.6f, 7, 0.3f); Balloons(lx - 0.85f, -2.2f, 6, 2.1f); Balloons(lx + 0.4f, 5.0f, 4, 4.0f); Balloons(lx - 0.5f, 11.0f, 4, 5.5f);
    lit.Draw(prim.cube, TRS({ lx + 0.5f, 0.38f, -1.15f }, { 0, 0, 0 }, { 1.5f, 0.76f, 0.8f }), Hex(0xf1f3f5)); lit.Draw(prim.cube, TRS({ lx + 0.5f, 0.77f, -1.15f }, { 0, 0, 0 }, { 1.6f, 0.04f, 0.9f }), Hex(0xff6b9d));
    DrawModel("cake.glb", { lx + 0.5f, 0.79f, -1.15f }, 0.4f, 0.34f);
    const unsigned GC[] = { 0xff4d6d, 0x4dabf7, 0xffd43b, 0x51cf66 };
    for (int i = 0; i < 4; i++) { float gx = lx - 0.55f + i * 0.34f; lit.Draw(prim.cube, TRS({ gx, 0.14f, -0.55f }, { 0, 0.3f * i, 0 }, { 0.26f, 0.26f, 0.26f }), Hex(GC[i])); lit.Draw(prim.cube, TRS({ gx, 0.14f, -0.55f }, { 0, 0.3f * i, 0 }, { 0.04f, 0.27f, 0.27f }), WHITE); }
    rlDrawRenderBatchActive(); BeginBlendMode(BLEND_ADDITIVE); DrawSphere({ lx + 0.5f, 1.2f, -1.15f }, 0.3f + 0.03f * sinf(time * 14), Color{ 255, 190, 90, 40 }); rlDrawRenderBatchActive(); EndBlendMode(); BeginBlendMode(BLEND_ALPHA);
    // the friends: party hats, cheering when the birthday kid does well
    const unsigned HC[] = { 0xff4d6d, 0x4dabf7, 0xffd43b, 0x51cf66, 0xcc5de8 };
    struct G { float dx, z, yaw; unsigned h; };
    const G gs[] = { { 1.35f, -1.9f, -2.3f, 11 }, { 1.7f, -0.9f, -1.9f, 14 }, { -1.0f, -1.0f, 0.5f, 22 }, { -1.5f, -2.0f, 0.9f, 31 }, { 0.5f, -2.3f, 3.1f, 40 } };
    for (int i = 0; i < 5; i++) {
      bool hop = L.cheerT > 0; Color shirt = Hex(HC[(i + 2) % 5]);
      if (hop) Person(lx + gs[i].dx, gs[i].z, gs[i].yaw, "Wave", time * 1.1f + i * 0.4f, gs[i].h, shirt, nullptr, 0, 0, false, Hex(HC[i]));
      else Person(lx + gs[i].dx, gs[i].z, gs[i].yaw, "Idle_Neutral", time + i, gs[i].h, shirt, "Interact", fmodf(time * 0.35f + i * 0.2f, 1.f) * 0.5f, 0.55f, true, Hex(HC[i]));
    }
  }
  void DrawFood(Lane& L) {
    float lx = LaneX(L.k);
    lit.Draw(prim.cube, TRS({ lx, 0.4f, -1.5f }, { 0, 0, 0 }, { 2.1f, 0.8f, 0.95f }), Hex(0xf3ede2)); lit.Draw(prim.cube, TRS({ lx, 0.81f, -1.5f }, { 0, 0, 0 }, { 2.2f, 0.04f, 1.05f }), Hex(0xc92a2a));
    for (int i = -3; i <= 3; i++) lit.Draw(prim.cube, TRS({ lx + i * 0.3f, 0.835f, -1.5f }, { 0, 0, 0 }, { 0.15f, 0.005f, 1.05f }), WHITE);   // a checked cloth
    DrawModel("pizzabox.glb", { lx - 0.65f, 0.83f, -1.5f }, 0.2f, 0.3f); DrawModel("pizza.glb", { lx - 0.65f, 0.85f, -1.5f }, 0.2f, 0.035f); DrawModel("pizzabox.glb", { lx - 0.05f, 0.83f, -1.65f }, -0.3f, 0.3f); DrawModel("pizza.glb", { lx - 0.05f, 0.85f, -1.65f }, 0.5f, 0.035f);
    for (int i = 0; i < 3; i++) DrawModel("burger.glb", { lx + 0.45f + i * 0.22f, 0.83f, -1.3f - 0.1f * (i % 2) }, i, 0.15f);
    for (int i = 0; i < 3; i++) DrawModel("fries.glb", { lx + 0.55f + i * 0.2f, 0.83f, -1.75f }, 0.2f * i, 0.2f);
    const unsigned CC[] = { 0xe03131, 0x1c7ed6, 0xfab005, 0x37b24d };
    for (int i = 0; i < 4; i++) { Vector3 c = { lx - 0.9f + i * 0.28f, 0.83f, -1.15f }; lit.Draw(prim.cone, TRS({ c.x, c.y, c.z }, { 0, 0, 0 }, { 0.08f, 0.2f, 0.08f }), Hex(CC[i])); lit.Draw(prim.cyl, TRS({ c.x, c.y + 0.2f, c.z }, { 0, 0, 0 }, { 0.09f, 0.02f, 0.09f }), WHITE); lit.Draw(prim.cyl, TRS({ c.x + 0.01f, c.y + 0.2f, c.z }, { 0.2f, 0, 0 }, { 0.012f, 0.14f, 0.012f }), Hex(0xff6b6b)); }
    // four people eating: one on each side, one at the end
    struct G { float dx, z, yaw; unsigned h; };
    const G gs[] = { { -0.5f, -2.35f, 0.0f, 9 }, { 0.6f, -2.35f, 0.0f, 17 }, { -1.55f, -1.5f, 1.57f, 26 }, { 1.55f, -1.5f, -1.57f, 33 } };
    const unsigned SC[] = { 0x3b5bdb, 0xd9480f, 0x2b8a3e, 0x862e9c };
    for (int i = 0; i < 4; i++) Person(lx + gs[i].dx, gs[i].z, gs[i].yaw, "Idle_Neutral", time + i * 1.3f, gs[i].h, Hex(SC[i]), "Interact", fmodf(time * 0.28f + i * 0.31f, 1.f) * 0.45f + 0.1f, 0.6f, true);
  }
  void DrawBowlers() {
    for (auto& L : lanes) {
      if (L.k == 0 || L.kind == 1 || std::abs(L.k) == 5) continue; float lx = LaneX(L.k);
      if (L.kind == 0 && std::abs(L.k) == 1) {   // a friend watching
        Person(lx + 1.0f, -1.9f, -2.5f, "Idle_Neutral", time + L.k, L.hsh + 3, Hex(0x37474f), L.cheerT > 0 ? "Wave" : nullptr, time * 1.2f, L.cheerT > 0 ? 1.f : 0.f);
      }
      float bx = lx + L.sx; Color hat = L.kind == 2 ? Hex(0xffd43b) : BLANK;
      bool carry = L.ph <= 2 && !L.rel;
      if (L.ph == 0) Person(bx, L.nz, 0, "Idle_Neutral", time + L.k, L.hsh, L.shirt, nullptr, 0, 0, false, hat, &L.ball, L.ballCol);
      else if (L.ph == 1) Person(bx, L.nz, 0, "Walk", time * 1.15f, L.hsh, L.shirt, nullptr, 0, 0, false, hat, &L.ball, L.ballCol);
      else if (L.ph == 2) Person(bx, L.nz, 0, "Walk", 0.2f, L.hsh, L.shirt, "Sword_Slash", 0.22f + Clamp(L.t / 0.7f, 0.f, 1.f) * 0.5f, 1.f, true, hat, carry ? &L.ball : nullptr, L.ballCol);
      else if (L.ph == 3) Person(bx, L.nz, 0, "Idle_Neutral", time + L.k, L.hsh, L.shirt, nullptr, 0, 0, false, hat);
      else if (L.ph == 4) { if (L.cheer) Person(bx, L.nz, 0, "Wave", time * 1.3f, L.hsh, L.shirt, nullptr, 0, 0, false, hat); else Person(bx, L.nz, 0, "Idle_Neutral", time + 2, L.hsh, L.shirt, nullptr, 0, 0, false, hat); }
      else Person(bx, L.nz, PI, "Walk", time * 1.1f, L.hsh, L.shirt, nullptr, 0, 0, false, hat);
    }
  }

  // ---------------------------------------------------------------- the frame
  void Draw() override { g_toonK = 1; DrawScene(); g_toonK = 1.4f; }
  void DrawScene() {
    LoadTex();
    Lane& M = Mine();
    // the camera is your eyes: standing at the back of the approach, looking down your lane; it follows the ball once it's thrown
    float bxw = -Clamp(bxs, -1.f, 1.f) * 0.42f; Vector3 wp, wt; float wf = 58, k = 3.0f;
    if (phase == "roll") { float tz = fminf(M.ball.z - 3.2f, 14.6f); camZ += (tz - camZ) * 0.08f; wp = { M.ball.x * 0.45f, 1.05f, camZ }; wt = { M.ball.x * 0.3f, 0.12f, camZ + 12 }; wf = 52; k = 5; }
    else if (phase == "show") { wp = { 1.3f, 1.2f, 16.0f }; wt = { -0.05f, 0.2f, 19.3f }; wf = 38; k = 2.4f; }
    else { wp = { bxw * 0.85f, 1.62f, -5.5f }; wt = { bxw * 0.35f, 0.5f, 15.0f }; wf = 56; }
    if (g_dbg == 31) { wp = { LaneX(BIRTHDAY) + 2.6f, 1.55f, -5.2f }; wt = { LaneX(BIRTHDAY) - 0.2f, 1.1f, -1.2f }; wf = 50; k = 9; }      // debug: the birthday lane
    if (g_dbg == 32) { wp = { LaneX(FOOD) - 2.6f, 1.55f, -5.2f }; wt = { LaneX(FOOD) + 0.2f, 1.0f, -1.5f }; wf = 50; k = 9; }               // debug: the buffet
    if (g_dbg == 33) { wp = { LaneX(1) + 0.9f, 1.55f, -5.5f }; wt = { LaneX(1), 1.0f, 2.0f }; wf = 50; k = 9; }                              // debug: a neighbour bowling
    { Camera3D rc = rig.Go(wp, wt, wf, k); cam.position = rc.position; cam.target = rc.target; cam.fovy = rc.fovy; }
    UI::Sky(Hex(0x0b0620), Hex(0x1a0b3a));
    lit.Begin(cam, { 0.05f, 0.04f, 0.11f }, 0.011f);
    BeginMode3D(cam);
    lit.noRec = true; DrawHall(WHITE);   // the alley around you is a backdrop: it doesn't need to cast shadows
    for (auto& L : lanes) { float lx = LaneX(L.k); lit.noRec = L.k != 0; DrawPins(L.pins, lx, L.k == 0); if (L.k != 0 && L.ball.on) DrawBall(L.ball, lx, L.ballCol, false); }
    lit.noRec = false;
    // your ball: on the foul line while you line it up, then rolling
    Color bc = P(cur).color;
    if (phase == "aim") { Ball b; b.x = bxw; b.z = 0.35f; b.y = BALL_R; DrawBall(b, 0, bc, true); }
    else if (M.ball.on || phase == "roll") DrawBall(M.ball, 0, bc, true);
    if (phase == "aim") for (int i = 0; i < 24; i++) {   // the plan: the line your ball will follow, curve and all
      float z = 0.9f + i * 0.74f, vz = 6.5f + bpowN * 4.5f, px = bxw - baimN * 0.035f * z - AMAX * bhookN * HookI(z) / (vz * vz) * 1.f;
      lit.Draw(prim.cyl, TRS({ px, 0.006f, z }, { 0, 0, 0 }, { 0.05f, 0.005f, 0.05f }), Fade(bc, 0.9f * (1 - i / 28.f) * (0.6f + 0.4f * sinf(time * 8 - i * 0.6f))));
    }
    slotN = 4; lit.noRec = true; DrawBowlers();
    for (auto& L : lanes) { if (L.kind == 2) DrawParty(L); if (L.kind == 1) DrawFood(L); }
    lit.noRec = false;
    // everyone else at your lane: your friends wait beside you, ball in hand
    { int n = 0; for (int i = 0; i < N(); i++) { if (i == cur && phase != "end") continue; float s = (n % 2) ? 1.f : -1.f; float x = s * (1.45f + 0.7f * (n / 2)); n++;
        Person(x, -6.6f, s > 0 ? -0.5f : 0.5f, "Idle_Neutral", time + i * 0.9f, PersonHash(P(i).color), P(i).color); } }
    if (phase == "end") DrawToon({ 0, 0, -1.7f }, 0, P(cur).color, time, 0, 1, 1.0f);
    fx.Draw(); conf.Draw();
    DrawGlow();
    EndMode3D();
    Board("FRAME " + I(std::min(frameNo, frames)) + "/" + I(frames), Sorted([&](int i) { return (float)pts[i]; }), [&](int i) { return I(pts[i]); });
    if (popT > 0) {
      float a = fminf(1.f, popT * 2), kk = EaseBack(fminf(1.f, (2.2f - popT) * 3)), fs = 44, w = Measure(pop, fs, TS_DISPLAY) + 90;
      UI::Box({ 800 - w / 2, 96 - 8 * (1 - kk), w, 76 }, 38, Hex(0x0b1020, (unsigned char)(200 * a)));
      DrawCircle((int)(800 - w / 2 + 40), (int)(134 - 8 * (1 - kk)), 12, Fade(popC, a));
      Text(800 + 14, 134 - 8 * (1 - kk), fs, Fade(WHITE, a), pop, 0, TS_DISPLAY | TS_SHADOW);
    }
    if (phase == "aim") Hint(P(cur).bot ? P(cur).name + " is aiming…" : P(cur).name + ": slide the ball, flick it up, curve the flick for a hook!", P(cur).color);
  }
};
}  // namespace

static RegGame s1("bowling", [] { return std::unique_ptr<Game>(new Bowling); });
