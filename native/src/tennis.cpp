// SMASH TENNIS in C++: a real-size court in a full stadium, singles or doubles (players rotate in on bigger groups).
// Players run to the ball by themselves — you only SWING (phone motion, the AI camera, or a tap) and tilt to aim.
// A ring on the ground closes on the ball: swing when it's smallest. Ball flies with gravity + bounce, lands in.
#include "classic.h"
#include "kit.h"
#include "mesh.h"

namespace {
using kit::I;
constexpr float HL = 11.885f, HW = 4.115f, DW = 5.485f, NETH = 0.914f, SVC = 6.4f, G = 9.81f, GSPD = 0.72f;  // GSPD: game speed (slower than real = easier)

struct Tennis : Classic {
  struct Pl { int pid; int team; Vector3 pos, goal, vel{}; float sw = 0, swDir = 1, cool = 0; };
  std::vector<int> teamQ[2]; std::vector<Pl> on;  // players on court
  Vector3 bp{}, bv{}; bool live = false, bounced = false; int lastHitTeam = -1, bounces = 0; float spin = 0;
  int pts[2] = { 0, 0 }, games[2] = { 0, 0 }, server = 0, serveTeam = 0, GAMES = 2;
  std::string phase = "intro", call, radar; float t = 2.5f, callT = 0, radarT = 0, hitAt = -1, tossT = -1; int recv = -1; Vector3 hitPt{};
  std::vector<Vector4> crowd; bool fin = false; int rally = 0; std::vector<int> hits;
  std::vector<Vector3> trail; Mesh crowdMesh{};
  ~Tennis() { if (crowdMesh.vertexCount) UnloadMesh(crowdMesh); }

  void Start() override {
    InitCam(40); hits.assign(N(), 0);
    for (int i = 0; i < N(); i++) teamQ[i % 2].push_back(i);
    if (N() == 1) { /* solo: a bot-free practice — the far side is played by an AI ball machine */ }
    for (int s = 0; s < 4; s++) for (int r = 0; r < 7; r++) for (int k = 0; k < 34; k++) {
      float u = (k + 0.5f) / 34; Vector3 p;
      if (s == 0) p = { -18 + u * 36, 1.2f + r * 1.1f, HL + 7 + r * 1.3f }; else if (s == 1) p = { -18 + u * 36, 1.2f + r * 1.1f, -HL - 8 - r * 1.3f };
      else if (s == 2) p = { -11.5f - r * 1.3f, 1.2f + r * 1.1f, -HL - 4 + u * (2 * HL + 8) }; else p = { 11.5f + r * 1.3f, 1.2f + r * 1.1f, -HL - 4 + u * (2 * HL + 8) };
      crowd.push_back({ p.x, p.y, p.z, (float)(rand() % 8) });
    }
    Layout(-1, kit::Msg("🎾", "Smash Tennis", "Get ready…"));
    SetupPoint();
  }
  bool Doubles() { return teamQ[0].size() >= 2 && teamQ[1].size() >= 2; }
  float Side(int team) { return team == 0 ? -1.f : 1.f; }  // team 0 near the camera (negative z)
  void SetupPoint() {
    on.clear();
    for (int tm = 0; tm < 2; tm++) {
      int n = std::min<int>(Doubles() ? 2 : 1, teamQ[tm].size());
      for (int k = 0; k < n; k++) { Pl p; p.pid = teamQ[tm][k]; p.team = tm; float x = n == 2 ? (k ? 2.2f : -2.2f) : 0; p.pos = { x, 0, Side(tm) * (HL + 0.8f) }; if (n == 2 && k == 1) p.pos.z = Side(tm) * 3.5f; p.goal = p.pos; on.push_back(p); }
    }
    if (teamQ[1].empty()) { Pl p; p.pid = -1; p.team = 1; p.pos = { 0, 0, HL + 0.8f }; p.goal = p.pos; on.push_back(p); }  // practice machine
    // server: first player of the serving team stands behind the baseline, to the right
    int sidx = -1; for (size_t i = 0; i < on.size(); i++) if (on[i].team == serveTeam) { sidx = (int)i; break; }
    server = sidx; float sx = ((pts[0] + pts[1]) % 2 ? -1.f : 1.f) * 1.4f * -Side(serveTeam);
    on[server].pos = on[server].goal = { sx, 0, Side(serveTeam) * (HL + 0.5f) };
    for (auto& p : on) if (p.team != serveTeam && p.pos.z * Side(p.team) > 8) { p.pos.x = p.goal.x = -sx * 0.8f; }
    live = false; bounced = false; bounces = 0; trail.clear(); phase = "serve"; t = 7; tossT = -1; recv = -1; hitAt = -1; rally = 0;
    for (int i = 0; i < N(); i++) if (!P(i).bot) {
      bool srv = on[server].pid == i; bool playing = false; for (auto& p : on) playing |= p.pid == i;
      Layout(i, playing ? std::string("{\"name\":\"swing\",\"sport\":\"tennis\",\"label\":\"SWING!\",\"aim\":true,\"hint\":") + kit::Q(srv ? "Your serve! Swing to toss + hit · tilt to aim" : "Swing when the ring around you is smallest · tilt to aim left/right") + "}"
                        : kit::Msg("🎾", "Next up soon!", "You rotate in after this point"));
    }
  }
  // launch the ball from 'from' to land at target, at speed v (m/s), clearing the net
  void Launch(Vector3 from, Vector3 target, float speed) {
    Vector3 d = { target.x - from.x, 0, target.z - from.z }; float dist = hypotf(d.x, d.z);
    float T = fmaxf(0.55f, dist / (speed * GSPD));
    for (int k = 0; k < 12; k++) {  // make sure it clears the net with room to spare
      float vy = (0 - from.y + 0.5f * G * T * T) / T, tn = fabsf(from.z) / fmaxf(0.01f, fabsf(d.z)) * T;
      float hn = from.y + vy * tn - 0.5f * G * tn * tn;
      if (hn > NETH + 0.35f) break; T *= 1.08f;
    }
    bv = { d.x / T, (0 - from.y + 0.5f * G * T * T) / T, d.z / T }; bp = from; live = true; bounced = false; bounces = 0;
  }
  // where (and when) the receiver should hit: after the bounce, when the ball comes down to ~1 m
  void Predict() {
    Vector3 p = bp, v = bv; bool b = bounced; float tt = 0; const float back = HL + 1.0f;
    for (int k = 0; k < 900; k++) {
      float dt = 1 / 120.f; Vector3 pv = v; v.y -= G * dt; p = Vector3Add(p, Vector3Scale(v, dt)); tt += dt;
      if (p.y < 0.033f && v.y < 0) { p.y = 0.033f; v.y = -v.y * 0.72f; v.x *= 0.88f; v.z *= 0.88f; if (b) break; b = true; continue; }
      if (!b) continue;
      // after the bounce: take it at the top of the bounce if that's a nice height, else on the way down at ~1.2 m,
      // and never let the player be pushed far behind the baseline
      if (pv.y > 0 && v.y <= 0 && p.y < 1.6f) { hitPt = p; hitAt = tt; return; }
      if (v.y < 0 && p.y < 1.2f) { hitPt = p; hitAt = tt; return; }
      if (fabsf(p.z) > back) { hitPt = p; hitAt = tt; return; }
    }
    hitPt = p; hitAt = tt;
  }
  // the racket is in the right hand: stand beside the ball, not on it (near side faces +z → right hand is at -x)
  float HandSide(int team) { return team == 0 ? -0.75f : 0.75f; }
  Vector3 RacketPt(const Pl& p) { return { p.pos.x + HandSide(p.team), 1.0f, p.pos.z }; }
  int Receiver() {
    int team = 1 - lastHitTeam, best = -1; float bd = 1e9f;
    for (size_t i = 0; i < on.size(); i++) if (on[i].team == team) { float d = hypotf(on[i].pos.x - hitPt.x, on[i].pos.z - hitPt.z); if (d < bd) { bd = d; best = (int)i; } }
    return best;
  }
  void Hit(int i, float power, float aim, bool good) {
    Pl& p = on[i]; lastHitTeam = p.team; rally++; if (p.pid >= 0) hits[p.pid]++;
    float tz = -Side(p.team) * (HL - 2.2f - frand(0, 2.5f) * (good ? 0.6f : 1)), tx = Clamp(aim, -1, 1) * (Doubles() ? DW : HW) * 0.78f + frand(-0.4f, 0.4f);
    bool smash = bp.y > 1.9f && power > 0.5f;
    float spd = (smash ? 34 : 20 + power * 13) * (good ? 1 : 0.82f);
    Launch(bp, { tx, 0, tz }, spd);
    int kmh = (int)(spd * 3.6f * 1.1f); radar = I(kmh) + " km/h"; radarT = 1.6f;
    Sfx(smash ? "smash" : "pok"); Shake(smash ? 10 : 2); fx.Burst(bp, smash ? 40 : 12, smash ? Hex(0xff922b) : WHITE, smash ? 6 : 3, 0.5f, 0.05f, 2);
    if (smash) { call = "🔥 SMASH!"; callT = 1.2f; } else if (good && power > 0.8f) { call = "💥 POWER SHOT!"; callT = 1; }
    p.sw = 1; p.swDir = bp.x > p.pos.x ? 1.f : -1.f;
    if (rally == 8) Sfx("cheer");
    Predict(); recv = Receiver();
  }
  void Swing(int pid, float power) {
    for (size_t i = 0; i < on.size(); i++) {
      Pl& p = on[i]; if (p.pid != pid || p.cool > 0) continue;
      p.cool = 0.3f; p.sw = 1; Sfx("swish");
      if (phase == "serve" && (int)i == server && tossT < 0) { tossT = 0; servePow = power; serveAim = P(pid).stick.x; return; }
      if (phase != "rally" || !live || (int)i != recv) return;
      // generous: anywhere near the ball counts; closer to the ideal moment = better shot
      Vector3 rk = RacketPt(p); float d = Vector3Distance(rk, bp);
      if (d < 3.2f && bp.z * Side(p.team) > -0.5f) { Hit((int)i, power, P(pid).stick.x, d < 1.6f); if (d < 1.1f) { call = "PERFECT!"; callT = 0.8f; } return; }
      call = d < 6 ? "TOO EARLY!" : "WAIT FOR IT…"; callT = 0.6f;
    }
  }
  float servePow = 0.7f, serveAim = 0;
  void Act(int p, const std::string& k, int, float pw) override { if (k.rfind("swing", 0) == 0) Swing(p, Clamp(pw > 0 ? pw : 0.65f, 0.2f, 1)); }
  void Point(int team, const std::string& why) {
    if (phase == "point") return;
    pts[team]++; phase = "point"; t = 2.6f; live = false; call = why; callT = 2.2f; Sfx(why.find("ACE") != std::string::npos ? "cheer" : "applause");
    int a = pts[team], b = pts[1 - team];
    if (a >= 4 && a - b >= 2) {
      games[team]++; pts[0] = pts[1] = 0; call = (team ? TeamName(1) : TeamName(0)) + " WIN THE GAME!"; callT = 2.4f; Sfx("cheer");
      serveTeam = 1 - serveTeam;
      if (games[team] >= GAMES) { phase = "end"; t = 4; conf.Spawn({ 0, 4, Side(team) * 6 }, 160, 5, 6); Sfx("win"); Banner(TeamName(team) + " WIN THE MATCH!", 2600, team ? "#4dabff" : "#ff6b6b"); return; }
    }
    // rotate players in for big groups
    for (int tm = 0; tm < 2; tm++) if ((int)teamQ[tm].size() > (Doubles() ? 2 : 1)) { int f = teamQ[tm][0]; teamQ[tm].erase(teamQ[tm].begin()); teamQ[tm].push_back(f); }
  }
  std::string TeamName(int tm) { if (teamQ[tm].empty()) return "MACHINE"; if (teamQ[tm].size() == 1) return P(teamQ[tm][0]).name; return tm ? "BLUE TEAM" : "RED TEAM"; }
  std::string Score(int tm) { int a = pts[tm], b = pts[1 - tm]; if (a >= 3 && b >= 3) return a == b ? "40" : a > b ? "AD" : "40"; const char* S[] = { "0", "15", "30", "40" }; return S[std::min(3, a)]; }
  void Update(float dt) override {
    time += dt; fx.Update(dt); conf.Update(dt); callT -= dt; radarT -= dt; t -= dt;
    for (int i = 0; i < N(); i++) if (!P(i).bot && P(i).a && !P(i).pa) Swing(i, 0.65f);
    for (auto& p : on) { p.sw = fmaxf(0, p.sw - dt * 3); p.cool -= dt; }
    if (phase == "intro") { if (t <= 0) phase = "serve"; return; }
    if (phase == "end") { if (t <= 0 && !fin) { fin = true; std::vector<int> o; int w = games[0] > games[1] ? 0 : 1; for (int i : teamQ[w]) o.push_back(i); for (int i : teamQ[1 - w]) o.push_back(i);
        std::stable_sort(o.begin(), o.begin() + teamQ[w].size(), [&](int a, int b) { return hits[a] > hits[b]; });
        FinishOrder(o, [&](int i) { int tm = i % 2; return std::string(tm == w ? "won " : "lost ") + I(games[tm]) + "-" + I(games[1 - tm]) + " · " + I(hits[i]) + " hits"; }); } return; }
    if (phase == "point") { if (t <= 0) SetupPoint(); return; }
    // movement: the receiver runs to stand beside the ball (racket side); everyone else recovers to their ready spot.
    // Real-ish footwork: accelerate up to ~7 m/s, brake smoothly, always facing the net.
    for (size_t i = 0; i < on.size(); i++) {
      Pl& p = on[i]; bool netMan = Doubles() && fabsf(p.pos.z) < 6 && (int)i != recv;
      if (phase == "rally" && (int)i == recv) p.goal = { Clamp(hitPt.x - HandSide(p.team), -DW - 1.2f, DW + 1.2f), 0, Clamp(hitPt.z, -HL - 1.5f, HL + 1.5f) };
      else if (phase == "rally") { float bx2 = live ? bp.x : 0; p.goal = { Clamp(bx2 * 0.35f + (netMan ? (p.goal.x > 0 ? 1.2f : -1.2f) : 0), -3, 3), 0, Side(p.team) * (netMan ? 3.5f : HL + 0.4f) }; }
      Vector3 d = Vector3Subtract(p.goal, p.pos); d.y = 0; float l = Vector3Length(d);
      Vector3 want = l > 0.02f ? Vector3Scale(d, fminf(7.0f, l * 4.0f) / l) : Vector3{ 0, 0, 0 };
      Vector3 dv = Vector3Subtract(want, p.vel); float dl = Vector3Length(dv), acc = 28 * dt;
      if (dl > acc) dv = Vector3Scale(dv, acc / dl);
      p.vel = Vector3Add(p.vel, dv); p.pos = Vector3Add(p.pos, Vector3Scale(p.vel, dt));
    }
    if (phase == "serve") {
      Pl& s = on[server];
      if (tossT < 0) { if ((s.pid < 0 || P(s.pid).bot) && t < 5.6f) { tossT = 0; servePow = frand(0.6f, 1); serveAim = frand(-0.7f, 0.7f); s.sw = 0; } if (t <= 0) { tossT = 0; servePow = 0.6f; serveAim = 0; } return; }
      tossT += dt; bp = { s.pos.x + 0.3f, 1.2f + 6.0f * tossT - 0.5f * G * tossT * tossT * 1.0f, s.pos.z }; live = false;
      if (tossT >= 0.55f) {  // hit at the top of the toss
        float sx = s.pos.x > 0 ? -1.f : 1.f;
        Vector3 target = { sx * frand(0.6f, HW - 0.5f) * 0.9f + serveAim * 0.6f, 0, -Side(s.team) * frand(SVC - 2.2f, SVC - 0.5f) };
        target.x = Clamp(target.x, -HW + 0.3f, HW - 0.3f); if (target.x * sx < 0.2f) target.x = sx * 0.6f;
        bp.y = 2.7f; lastHitTeam = s.team; float spd = 20 + servePow * 11; Launch(bp, target, spd);
        radar = I((int)(spd * 3.6f * 1.15f)) + " km/h"; radarT = 1.6f; Sfx("smash"); s.sw = 1; phase = "rally"; Predict(); recv = Receiver();
      }
      return;
    }
    if (phase == "rally" && live) {
      int steps = 4; float h = dt / steps;
      for (int k = 0; k < steps && live; k++) {
        float zb = bp.z; bv.y -= G * h; bp = Vector3Add(bp, Vector3Scale(bv, h));
        if ((zb < 0) != (bp.z < 0) && bp.y < NETH) { live = false; Sfx("netHit"); Point(1 - lastHitTeam, "NET!"); return; }
        if (bp.y < 0.033f && bv.y < 0) {
          bp.y = 0.033f; bv.y = -bv.y * 0.72f; bv.x *= 0.88f; bv.z *= 0.88f; bounces++; Sfx("bounce");
          float w = Doubles() ? DW : HW; bool onRecvSide = bp.z * Side(lastHitTeam) < 0;
          if (bounces == 1 && (!onRecvSide || fabsf(bp.x) > w + 0.05f || fabsf(bp.z) > HL + 0.05f)) { live = false; Point(1 - lastHitTeam, "OUT!"); return; }
          if (bounces >= 2) { live = false; Point(lastHitTeam, rally <= 1 ? "ACE! 🎾" : "WINNER!"); return; }
        }
      }
      trail.push_back(bp); if (trail.size() > 16) trail.erase(trail.begin());
      // the AI (bots + the practice machine) hits on time, most of the time
      if (recv >= 0) { Pl& r = on[recv]; bool ai = r.pid < 0 || P(r.pid).bot; Vector3 rk = RacketPt(r);
        if (ai && Vector3Distance(rk, bp) < 1.9f && bounces == 1) { if (frand(0, 1) < (r.pid < 0 ? 0.9f : 0.8f)) Hit(recv, frand(0.45f, 0.9f), frand(-0.8f, 0.8f), true); else { recv = -1; } } }
      if (fabsf(bp.z) > HL + 9 || fabsf(bp.x) > 14) { live = false; Point(bounces >= 1 ? lastHitTeam : 1 - lastHitTeam, bounces >= 1 ? "WINNER!" : "OUT!"); }
    }
  }
  // the swing arc in the character's own space: the right arm sweeps from behind, across the front
  Vector3 HandLocal(const Pl& p, Vector3* dir, Vector3* face) {
    float sw = p.sw, ph = sw > 0 ? -1.7f + (1 - sw) * 3.4f : -0.35f;  // back → across
    Vector3 sh = { -0.2f, 1.41f, 0 };
    float reach = 0.5f, h = sw > 0 ? 1.12f + 0.12f * sinf((1 - sw) * PI) : 1.02f;
    Vector3 hand = { sh.x - reach * cosf(ph), h, reach * sinf(ph) };
    if (dir) *dir = Vector3Normalize({ -cosf(ph) * 0.8f, sw > 0 ? 0.35f : 0.9f, sinf(ph) * 0.8f });
    if (face) *face = { sinf(ph), 0, cosf(ph) };
    return hand;
  }
  void Racket(const Pl& p, Color c) {
    float yaw = p.team == 0 ? 0 : PI, sc = 1.05f; Vector3 d, f; Vector3 hl = HandLocal(p, &d, &f);
    Vector3 hand = ToonPoint(p.pos, yaw, sc, 0, hl);
    Vector3 tip = ToonPoint(p.pos, yaw, sc, 0, Vector3Add(hl, d)), fw = ToonPoint(p.pos, yaw, sc, 0, Vector3Add(hl, f));
    Matrix xf = RacketXf(Vector3Subtract(hand, Vector3Scale(Vector3Normalize(Vector3Subtract(tip, hand)), 0.06f)), Vector3Subtract(tip, hand), Vector3Subtract(fw, hand), 1.4f);
    DrawProp(props.racketFrame, xf, c); DrawProp(props.racketRest, xf, WHITE);
  }
  void Draw() override { g_toonK = 1; DrawCourt(); g_toonK = 1.4f; }
  void DrawCourt() {
    // broadcast camera: high behind the near baseline, drifting gently with the ball
    float bx = live ? Clamp(bp.x, -4, 4) * 0.25f : 0;
    cam.position = Vector3Lerp(cam.position.y == 0 ? Vector3{ 0, 10.5f, -26 } : cam.position, { bx, 10.5f, -26.5f }, 0.05f);
    cam.target = Vector3Lerp(cam.target, { bx * 0.6f, 0, 0.5f }, 0.05f); cam.fovy = 43;
    if (g_dbg == 7 && !on.empty()) { Pl& q = on[0]; cam.position = { q.pos.x + 2.2f, 1.6f, q.pos.z + (q.team ? -2.6f : 2.6f) }; cam.target = { q.pos.x + 0.3f, 1.0f, q.pos.z }; cam.fovy = 40; }
    UI::Sky(Hex(0x4a90e2), Hex(0xcfe8ff));
    lit.Begin(cam, { 0.75f, 0.85f, 0.95f }, 0.004f);
    BeginMode3D(cam);
    lit.Pattern(PAT_GRASS); lit.Draw(prim.plane, TRS({ 0, -0.02f, 0 }, { 0, 0, 0 }, { 120, 1, 120 }), Hex(0x2f8a4a)); lit.Pattern(PAT_NONE);
    lit.Draw(prim.cube, TRS({ 0, -0.01f, 0 }, { 0, 0, 0 }, { 2 * DW + 7, 0.02f, 2 * HL + 12 }), Hex(0x2b6a52));
    lit.Draw(prim.cube, TRS({ 0, 0, 0 }, { 0, 0, 0 }, { 2 * DW, 0.02f, 2 * HL }), Hex(0x2a6fb0));
    auto line = [&](float x0, float z0, float x1, float z1) { lit.Draw(prim.cube, TRS({ (x0 + x1) / 2, 0.015f, (z0 + z1) / 2 }, { 0, 0, 0 }, { fmaxf(0.05f, fabsf(x1 - x0)), 0.01f, fmaxf(0.05f, fabsf(z1 - z0)) }), WHITE); };
    for (float z : { -HL, HL }) line(-DW, z, DW, z);
    for (float x : { -DW, DW, -HW, HW }) line(x, -HL, x, HL);
    for (float z : { -SVC, SVC }) line(-HW, z, HW, z);
    line(0, -SVC, 0, SVC); line(0, -HL, 0, -HL + 0.2f); line(0, HL - 0.2f, 0, HL);
    // net + posts
    DrawProp(props.tennisNet, MatrixIdentity());  // woven net, headband, centre strap, posts with cranks (real ITF sizes)
    for (int s = -1; s <= 1; s += 2) lit.Draw(prim.cyl, TRS({ s * (HW + 0.914f), 0, 0 }, { 0, 0, 0 }, { 0.03f, 1.07f, 0.03f }), Hex(0xf1f3f5));  // singles sticks
    // umpire chair
    for (int s = -1; s <= 1; s += 2) lit.Draw(prim.cyl, TRS({ -DW - 1.6f + s * 0.25f, 0, 0 }, { 0, 0, 0 }, { 0.05f, 2.0f, 0.05f }), Hex(0xdee2e6));
    lit.Draw(prim.cube, TRS({ -DW - 1.6f, 2.0f, 0 }, { 0, 0, 0 }, { 0.8f, 0.12f, 0.8f }), Hex(0x2f9e44));
    lit.Draw(prim.cube, TRS({ -DW - 1.95f, 2.45f, 0 }, { 0, 0, 0 }, { 0.1f, 0.9f, 0.8f }), Hex(0x2f9e44));
    DrawToon({ -DW - 1.6f, 2.0f, 0 }, PI / 2, Hex(0xe9ecef), time, 0, 0, 0.8f);
    // player benches beside the umpire: two chairs, a cooler and a towel each
    for (int s = -1; s <= 1; s += 2) {
      float z = s * 1.6f, x = -DW - 1.9f;
      for (int k = 0; k < 2; k++) {
        float zz = z + s * k * 0.7f;
        lit.Draw(prim.cube, TRS({ x, 0.45f, zz }, { 0, 0, 0 }, { 0.5f, 0.06f, 0.5f }), Hex(0x1e6b46));
        lit.Draw(prim.cube, TRS({ x - 0.24f, 0.75f, zz }, { 0, 0, 0 }, { 0.05f, 0.6f, 0.5f }), Hex(0x1e6b46));
        for (int q = -1; q <= 1; q += 2) lit.Draw(prim.cyl, TRS({ x, 0, zz + q * 0.2f }, { 0, 0, 0 }, { 0.04f, 0.45f, 0.04f }), Hex(0xced4da));
      }
      lit.Draw(prim.cube, TRS({ x + 0.1f, 0.2f, z + s * 1.5f }, { 0, 0, 0 }, { 0.45f, 0.4f, 0.35f }), Hex(0x1971c2));
      lit.Draw(prim.cube, TRS({ x + 0.1f, 0.42f, z + s * 1.5f }, { 0, 0, 0 }, { 0.47f, 0.05f, 0.37f }), WHITE);
      lit.Draw(prim.cube, TRS({ x - 0.2f, 0.62f, z }, { 0, 0, 0 }, { 0.08f, 0.35f, 0.4f }), WHITE);
    }
    // stands: concrete terraces down to the ground, padded front walls, ad boards
    lit.Pattern(PAT_STONE);
    for (int s = 0; s < 4; s++) for (int r = 0; r < 7; r++) {
      float top = 1.15f + r * 1.1f; Color cc = r % 2 ? Hex(0x737b87) : Hex(0x7f8793);
      if (s == 0) lit.Draw(prim.cube, TRS({ 0, top / 2, HL + 7 + r * 1.3f }, { 0, 0, 0 }, { 38, top, 1.3f }), cc);
      else if (s == 1) lit.Draw(prim.cube, TRS({ 0, top / 2, -HL - 8 - r * 1.3f }, { 0, 0, 0 }, { 38, top, 1.3f }), cc);
      else for (int q = -1; q <= 1; q += 2) lit.Draw(prim.cube, TRS({ (s == 2 ? -1 : 1) * (11.5f + r * 1.3f), top / 2, q * (HL + 4) / 2 }, { 0, 0, 0 }, { 1.3f, top, HL + 4 }), cc);
    }
    lit.Pattern(PAT_NONE);
    // back walls behind the top rows
    for (int s = -1; s <= 1; s += 2) {
      lit.Draw(prim.cube, TRS({ s * (11.5f + 7 * 1.3f), 5.2f, -(HL + 4) / 2 }, { 0, 0, 0 }, { 0.4f, 10.4f, HL + 4 }), Hex(0x2c3e66));
      lit.Draw(prim.cube, TRS({ s * (11.5f + 7 * 1.3f), 5.2f, (HL + 4) / 2 }, { 0, 0, 0 }, { 0.4f, 10.4f, HL + 4 }), Hex(0x2c3e66));
    }
    for (int q = -1; q <= 1; q += 2) lit.Draw(prim.cube, TRS({ q * 9.5f, 5.2f, HL + 7 + 7 * 1.3f }, { 0, 0, 0 }, { 19, 10.4f, 0.4f }), Hex(0x2c3e66));
    // side stand front walls (dark green padding with a white cap)
    for (int s = -1; s <= 1; s += 2) for (int q = -1; q <= 1; q += 2) {
      lit.Draw(prim.cube, TRS({ s * 10.75f, 0.6f, q * (HL + 4) / 2 }, { 0, 0, 0 }, { 0.2f, 1.2f, HL + 4 }), Hex(0x1d4d3a));
      lit.Draw(prim.cube, TRS({ s * 10.75f, 1.22f, q * (HL + 4) / 2 }, { 0, 0, 0 }, { 0.26f, 0.06f, HL + 4 }), Hex(0xf1f3f5));
    }
    // roofs over the side stands and the far stand, on steel columns (in panels so they cast shadows)
    for (int s = -1; s <= 1; s += 2) {
      for (int k = 0; k < 4; k++) {
        float z = -HL - 4 + (k + 0.5f) * (2 * HL + 8) / 4;
        lit.Draw(prim.cube, TRS({ s * 17.2f, 11.2f, z }, { 0, 0, s * 0.12f }, { 11.5f, 0.3f, (2 * HL + 8) / 4 + 0.02f }), Hex(0xe9ecef));
        lit.Draw(prim.cube, TRS({ s * 17.2f, 11.0f, z }, { 0, 0, s * 0.12f }, { 11.4f, 0.12f, (2 * HL + 8) / 4 }), Hex(0x495057));
      }
      for (int k = 0; k < 5; k++) lit.Draw(prim.cyl, TRS({ s * 20.9f, 0, -HL - 4 + k * (2 * HL + 8) / 4 }, { 0, 0, 0 }, { 0.35f, 11.6f, 0.35f }), Hex(0xadb5bd));
    }
    for (int k = 0; k < 3; k++) lit.Draw(prim.cube, TRS({ -12.7f + k * 12.7f, 11.2f, HL + 12 }, { -0.12f, 0, 0 }, { 12.72f, 0.3f, 10.5f }), Hex(0xe9ecef));
    // floodlight towers on the corners: steel masts with banks of lamps
    for (int sx = -1; sx <= 1; sx += 2) for (int sz = -1; sz <= 1; sz += 2) {
      Vector3 b0 = { sx * 22.5f, 0, (float)sz * (HL + 17) };
      lit.Draw(prim.cyl, TRS(b0, { 0, 0, 0 }, { 0.5f, 19, 0.5f }), Hex(0x868e96));
      float yaw = atan2f(-b0.x, -b0.z);
      lit.Draw(prim.cube, TRS({ b0.x, 19.6f, b0.z }, { 0.5f, yaw, 0 }, { 4.2f, 2.6f, 0.4f }), Hex(0x343a40));
    }
    // big screen above the far stand
    lit.Draw(prim.cube, TRS({ 0, 13.8f, HL + 17.5f }, { 0, 0, 0 }, { 11, 5.4f, 0.6f }), Hex(0x212529));
    for (int k = -1; k <= 1; k += 2) lit.Draw(prim.cyl, TRS({ k * 4.f, 0, HL + 17.8f }, { 0, 0, 0 }, { 0.4f, 11.2f, 0.4f }), Hex(0x868e96));
    // distant clouds
    for (int k = 0; k < 7; k++) { float a = k * 0.9f - 2.7f; lit.Draw(prim.sphere, TRS({ sinf(a) * 140, 38.f + (k % 3) * 6, cosf(a) * 140 + 40 }, { 0, a, 0 }, { 46, 9, 20 }), WHITE); }
    float ex = phase == "point" ? 1.f : 0.25f;
    if (!crowdMesh.vertexCount) { std::vector<Vector4> seats; for (auto& c : crowd) seats.push_back({ c.x, c.y - 0.05f, c.z, atan2f(-c.x, -c.z) }); crowdMesh = BuildCrowd(seats, 7); }
    DrawProp(crowdMesh, MatrixTranslate(0, fabsf(sinf(time * 7)) * 0.05f * ex, 0));
    // back-stop ad boards: alternating panels
    for (int s = -1; s <= 1; s += 2) for (int k = 0; k < 6; k++) {
      float w = (2 * DW + 6) / 6;
      lit.Draw(prim.cube, TRS({ -DW - 3 + w * (k + 0.5f), 0.5f, s * (HL + 5.4f) }, { 0, 0, 0 }, { w - 0.04f, 1.0f, 0.15f }), k % 2 ? Hex(0x0b3d6e) : Hex(0x2f6f4f));
      lit.Draw(prim.cube, TRS({ -DW - 3 + w * (k + 0.5f), 0.5f, s * (HL + 5.4f) - s * 0.08f }, { 0, 0, 0 }, { w * 0.6f, 0.16f, 0.02f }), Hex(0xf1f3f5));
    }
    // emissive bits: lamp banks and the big screen glow (unlit, so they bloom)
    for (int sx = -1; sx <= 1; sx += 2) for (int sz = -1; sz <= 1; sz += 2) {
      Vector3 b0 = { sx * 22.5f, 0, (float)sz * (HL + 17) }; Vector3 d = Vector3Normalize({ -b0.x, 0, -b0.z });
      for (int i = 0; i < 3; i++) for (int j = 0; j < 2; j++) {
        Vector3 rt = { -d.z, 0, d.x }; Vector3 p = Vector3Add({ b0.x, 19.0f + j * 1.1f, b0.z }, Vector3Add(Vector3Scale(rt, (i - 1) * 1.3f), Vector3Scale(d, 0.3f)));
        DrawSphere(p, 0.42f, Hex(0xfffbe8));
      }
    }
    DrawCube({ 0, 13.8f, HL + 17.15f }, 10.2f, 4.7f, 0.05f, Hex(0x1c7ed6));
    DrawCube({ 0, 14.9f, HL + 17.1f }, 9.0f, 1.1f, 0.05f, Hex(0x74c0fc));
    DrawCube({ -2.4f, 13.0f, HL + 17.1f }, 4.0f, 1.2f, 0.05f, Hex(0xffd43b));
    DrawCube({ 2.4f, 13.0f, HL + 17.1f }, 4.0f, 1.2f, 0.05f, Hex(0xff8787));
    // players
    for (auto& p : on) {
      Color c = p.pid >= 0 ? P(p.pid).color : Hex(0xadb5bd);
      float yaw = p.team == 0 ? 0 : PI; bool moving = Vector3Length(p.vel) > 0.6f;
      g_hold.on = true; g_hold.hand = HandLocal(p, nullptr, nullptr);
      DrawToon(p.pos, yaw, c, time + p.pid, moving ? 0.12f : 0, 0, 1.05f);
      g_hold.on = false;
      Racket(p, c);
    }
    // timing ring: closes around the receiver as the ball arrives
    if (phase == "rally" && live && recv >= 0 && on[recv].pid >= 0 && !P(on[recv].pid).bot) {
      Pl& r = on[recv]; Vector3 rp = RacketPt(r); float d = Vector3Distance(rp, bp), k = Clamp((d - 1.0f) / 9.0f, 0, 1);
      Color rc = d < 3.2f ? (d < 1.6f ? Hex(0x51cf66) : Hex(0xffd43b)) : Hex(0xffffff);
      DrawCircle3D({ rp.x, 0.03f, rp.z }, 0.7f + k * 3.2f, { 1, 0, 0 }, 90, Fade(rc, 0.95f));
      DrawCircle3D({ rp.x, 0.035f, rp.z }, 0.72f + k * 3.2f, { 1, 0, 0 }, 90, Fade(rc, 0.95f));
      DrawCircle3D({ rp.x, 0.03f, rp.z }, 0.7f, { 1, 0, 0 }, 90, Fade(WHITE, 0.6f));
    }
    // ball + trail + shadow
    if (live || phase == "serve") {
      if (phase != "serve" || tossT >= 0) {
        DrawProp(props.tennisBall, MatrixMultiply(MatrixMultiply(MatrixScale(0.05f, 0.05f, 0.05f), MatrixRotateXYZ({ time * 20, time * 13, 0 })), MatrixTranslate(bp.x, bp.y, bp.z))); lit.Caster({ bp.x, 0, bp.z }, 0.08f);
        for (size_t k = 1; k < trail.size(); k++) Limb(trail[k - 1], trail[k], 0.03f * k / trail.size(), Fade(Hex(0xf4ffb0), 0.6f));
      }
    }
    fx.Draw(); conf.Draw();
    EndMode3D();
    // scoreboard
    kit::Glass(520, 14, 560, 70, 22);
    Text(560, 37, 22, Hex(0xff8787), TeamName(0), -1, TS_HEAVY | TS_PLAIN); Text(560, 64, 22, Hex(0x74c0fc), TeamName(1), -1, TS_HEAVY | TS_PLAIN);
    Text(930, 37, 22, Hex(0xffe066), I(games[0]), 0, TS_DISPLAY); Text(930, 64, 22, Hex(0xffe066), I(games[1]), 0, TS_DISPLAY);
    Text(1010, 37, 22, WHITE, Score(0), 0, TS_DISPLAY); Text(1010, 64, 22, WHITE, Score(1), 0, TS_DISPLAY);
    Text(930, 20, 11, Hex(0xc9d3f2), "GAMES", 0, TS_PLAIN | TS_SPACED); Text(1010, 20, 11, Hex(0xc9d3f2), "PTS", 0, TS_PLAIN | TS_SPACED);
    for (auto& p : on) if (p.pid >= 0) { Vector2 s = GetWorldToScreen({ p.pos.x, 2.2f, p.pos.z }, cam); Text(s.x, s.y, 18, P(p.pid).color, P(p.pid).name, 0, TS_HEAVY); }
    if (phase == "serve" && server >= 0 && tossT < 0) { Pl& s = on[server]; Vector2 sp = GetWorldToScreen({ s.pos.x, 2.7f, s.pos.z }, cam); Text(sp.x, sp.y, 26, Hex(0xffe066), s.pid >= 0 && !P(s.pid).bot ? "SWING TO SERVE!" : "serving…", 0, TS_DISPLAY | TS_SHADOW); }
    if (callT > 0) Text(800, 330, 70 + 10 * EaseBack(fminf(1, (2.2f - callT) * 4)), Fade(WHITE, fminf(1, callT * 2)), call, 0, TS_DISPLAY | TS_SHADOW | TS_GOLD);
    if (radarT > 0) { kit::Glass(1330, 110, 240, 60, 20); Text(1450, 140, 30, Hex(0xffe066), radar, 0, TS_DISPLAY | TS_SHADOW); }
    Hint("🎾 Swing when the ring around you is smallest (green = perfect) · tilt to aim · you run to the ball by yourself", Hex(0xd8f5a2));
  }
};
}  // namespace

static RegGame t1("tennis", [] { return std::unique_ptr<Game>(new Tennis); });
