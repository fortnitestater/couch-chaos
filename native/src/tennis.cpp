// SMASH TENNIS in C++: a real-size court in a full stadium, singles or doubles (players rotate in on bigger groups).
// Players run to the ball by themselves — you only SWING (phone motion, the AI camera, or a tap) and tilt to aim.
// A ring on the ground closes on the ball: swing when it's smallest. Ball flies with gravity + bounce, lands in.
#include "classic.h"
#include "kit.h"
#include "mesh.h"
#include "rlgl.h"

namespace {
using kit::I;
constexpr float HL = 11.885f, HW = 4.115f, DW = 5.485f, NETH = 0.914f, SVC = 6.4f, G = 9.81f, GSPD = 0.8f;  // GSPD: game speed (slower than real = easier)

struct Tennis : Classic {
  struct Pl { int pid; int team; Vector3 pos, goal, vel{}, vs{}; float sw = 0, swDir = 1, cool = 0, yaw = NAN, ph = 0; bool warn = false; };   // vs: smoothed velocity (for the animation) · yaw: which way the body faces · ph: the stride
  std::vector<int> teamQ[2]; std::vector<Pl> on;  // players on court
  // difficulty (anyone can change it with the button on their phone). EASY is the default and is built to be won; HARD is the old, tougher game.
  //   the bots: how often they return a ball that is in reach, how close their racket has to get, how fast they run, how often they hit long, how often they smash
  //   the people: how far away a swing still counts, how close for a "good" one; EASY also steers your shots to the empty side and keeps them in the court
  int level = 0;
  static constexpr float AI_RET[3] = { 0.55f, 0.72f, 0.93f }, AI_REACH[3] = { 1.5f, 1.7f, 2.1f }, AI_RUN[3] = { 4.8f, 5.8f, 7.0f }, AI_ERR[3] = { 0.30f, 0.12f, 0.0f }, AI_SMASH[3] = { 0.0f, 0.15f, 0.3f };
  static constexpr float H_REACH[3] = { 7.0f, 5.4f, 4.6f }, H_GOOD[3] = { 4.5f, 3.2f, 2.4f };
  // the match: EASY = first to 3 points, NORMAL = first to 5 points (plain counting, no 15-30-40), HARD = the classic game (deuce) twice
  bool Quick() const { return level < 2; }
  int PointsToWin() const { return level == 0 ? 5 : 7; }   // the big points (ace, smash, fireball, bullseye) count double, so a match is still short
  int GamesToWin() const { return level < 2 ? 1 : 2; }
  // two people playing each other would never miss (every ball is in reach and stays in), so the rallies would go on for ever: against a person
  // the rally speeds up the longer it lasts, shots can go out, and the match has a clock (when it runs out, whoever leads wins; if it is level, the next point decides)
  bool Pvp() { for (auto& q : on) if (q.pid < 0 || P(q.pid).bot) return false; return !on.empty(); }
  float Reach() { return Pvp() ? fminf(H_REACH[level], 5.4f) : H_REACH[level]; }
  float Good() { return Pvp() ? fminf(H_GOOD[level], 3.2f) : H_GOOD[level]; }
  float matchT = 0; bool timeUp = false;
  // ----- stamina, the POWER meter (a full one makes the next hit an unstoppable FIREBALL) and the target rings (land a ball in one = BULLSEYE) -----
  std::vector<float> stamina, heat;   // by player id; the last slot is the practice machine
  float& Stam(const Pl& p) { return stamina[p.pid >= 0 ? p.pid : N()]; }
  float& Heat(const Pl& p) { return heat[p.pid >= 0 ? p.pid : N()]; }
  static float Fat(float s) { return s >= 0.4f ? 1.f : 0.55f + 0.45f * (s / 0.4f); }   // under 40% stamina you run slower and reach less
  struct Tgt { Vector3 p{}; } tgt[2];   // tgt[team] = a ring on that team's side of the court
  bool fireLive = false; float hitDue = 0;
  // ----- the shots: every one beats something and loses to something (DRIVE tires / LOB beats the net man / DROP beats the deep player / SLICE is safe) -----
  float cReach = 1, cRet = 1; bool cVolley = false, cSmash = false, cNoPower = false, volleyLive = false;   // what the last shot did to whoever must return it
  void ResetCounters() { cReach = cRet = 1; cVolley = cSmash = cNoPower = false; }
  static float NetNess(const Pl& q) { return Clamp((HL - fabsf(q.pos.z)) / (HL - 3.5f), 0.f, 1.f); }   // 0 = on the baseline, 1 = at the net
  const Pl* Opp(int team) { const Pl* o = nullptr; for (auto& q : on) if (q.team != team && (!o || fabsf(q.pos.z) < fabsf(o->pos.z))) o = &q; return o; }   // the opponent closest to the net   // hitDue: the moment the ball reaches the receiver (they run to be there on time)
  void NewTargets() { float w = Doubles() ? DW : HW; for (int tm = 0; tm < 2; tm++) tgt[tm].p = { frand(-0.75f, 0.75f) * (w - 0.7f), 0, Side(tm) * frand(3.4f, HL - 1.8f) }; }
  // ball kids: they wait by the net; after each point the nearest one runs out, picks the ball up and carries it back
  struct Kid { Vector3 pos, home; int mode = 0; float yaw = 0; };   // 0 waiting, 1 fetching, 2 carrying it back
  std::vector<Kid> kids; bool kidSent = false;
  void UpdateKids(float dt) {
    if (kids.empty()) { const Vector3 H[4] = { { DW + 1.4f, 0, 3.2f }, { DW + 1.4f, 0, -3.2f }, { -DW - 1.5f, 0, 5.5f }, { -DW - 1.5f, 0, -5.5f } }; for (auto& h : H) { Kid k; k.pos = k.home = h; k.yaw = atan2f(-h.x, -h.z); kids.push_back(k); } }
    if (phase != "point") kidSent = false;
    if (phase == "point" && !kidSent) { kidSent = true; int b = 0; float bd = 1e9f; for (int i = 0; i < (int)kids.size(); i++) { float d = Vector3Distance(kids[i].pos, bp); if (d < bd) { bd = d; b = i; } } kids[b].mode = 1; }
    for (auto& k : kids) {
      Vector3 goal = k.mode == 1 ? Vector3{ Clamp(bp.x, -DW - 1.2f, DW + 1.2f), 0, Clamp(bp.z, -HL - 2.5f, HL + 2.5f) } : k.home, d = Vector3Subtract(goal, k.pos); d.y = 0; float l = Vector3Length(d);
      if (k.mode == 1 && l < 0.5f) k.mode = 2;
      if (k.mode == 2 && l < 0.15f) { k.mode = 0; k.yaw = atan2f(-k.home.x, -k.home.z); }
      if (l > 0.12f && k.mode != 0) { float sp = k.mode == 1 ? 6.0f : 3.2f; k.pos = Vector3Add(k.pos, Vector3Scale(d, fminf(sp * dt, l) / l)); k.yaw = atan2f(d.x, d.z); }
    }
  }
  void DrawKids() {
    int n = 0; for (auto& k : kids) {
      PersonPose q; q.pos = k.pos; q.yaw = k.yaw; q.c = Hex(0xff922b); q.hsh = 7 + n * 5; q.height = 1.32f; q.clip = k.mode == 1 ? "Run" : k.mode == 2 ? "Walk" : "Idle_Neutral"; q.t = time * (k.mode == 1 ? 1.3f : 1.f) + n; Matrix wm = MatrixIdentity(); q.wrist = &wm; n++;
      if (DrawPersonPose(q) && k.mode == 2) lit.Draw(prim.sphere, TRS({ wm.m12, wm.m13 - 0.04f, wm.m14 }, { 0, 0, 0 }, { 0.067f, 0.067f, 0.067f }), Hex(0xd6f03c));   // the ball in their hand
    }
  }
  float freezeT = 0, superT = 0; int cheatTeam = -1;   // owner cheats: the other team stands frozen / your team runs and reaches far
  float RM(int team) const { return superT > 0 && team == cheatTeam ? 1.8f : 1.f; }
  void Cheat(int pid, const std::string& n) override {
    int tm = -1; for (auto& q : on) if (q.pid == pid) tm = q.team; if (tm < 0) return;
    if (n == "point") { if (phase == "rally" || phase == "serve") { live = false; Point(tm, "WINNER!"); } }
    else if (n == "freeze") { freezeT = 25; cheatTeam = tm; }
    else if (n == "super") { superT = 30; cheatTeam = tm; }
  }
  float TimeLimit() const { return level == 0 ? 150.f : level == 1 ? 240.f : 0.f; }
  Vector3 bp{}, bv{}; bool live = false, bounced = false, smashLive = false; int lastHitTeam = -1, bounces = 0; float spin = 0;
  int pts[2] = { 0, 0 }, games[2] = { 0, 0 }, server = 0, serveTeam = 0, GAMES = 2, ptTeam = -1;
  std::string phase = "intro", call, radar; float t = 2.5f, callT = 0, radarT = 0, hitAt = -1, tossT = -1; int recv = -1; Vector3 hitPt{};
  std::vector<Vector4> crowd; bool fin = false; int rally = 0; std::vector<int> hits;
  std::vector<Vector3> trail; Mesh crowdMesh{}; int camShot = -1; float shotT = 0;
  // presentation: LED ads, officials who talk, ball feel, Hawk-Eye reviews
  Texture2D adTex{}, scrTex{};
  float spinA = 0, spinR = 30, squash = 0, squeakT = 0; Vector3 spinAx{ 1, 0, 0 }; float camSide = 0;   // spin: topspin in rad/s (negative = backspin)
  struct Mark { Vector3 p; float life, yaw; }; std::vector<Mark> marks;
  std::vector<Vector3> path;   // the ball's flight since the last hit (Hawk-Eye replays it)
  struct Var { bool on = false; float t = 0; Vector3 at{}; bool in = false, alongX = true; float cm = 0; int team = 0, stage = 0; std::vector<Vector3> path; } var;
  int sinceVar = 2;
  float talkT = 0; std::string talk; int talker = 0;   // who speaks: 0 the chair umpire, 1 / 2 the near / far line judge
  int judgeOut = -1; float judgeT = 0; bool calledPlay = false;
  ~Tennis() { if (crowdMesh.vertexCount) UnloadMesh(crowdMesh); if (adTex.id) UnloadTexture(adTex); if (scrTex.id) UnloadTexture(scrTex); }

  void Start() override {
    InitCam(40); hits.assign(N(), 0); stamina.assign(N() + 1, 1.f); heat.assign(N() + 1, 0.f);
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
    Sfx("crowdOn");
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
    live = false; bounced = false; bounces = 0; trail.clear(); phase = "serve"; smashLive = false; fireLive = false; volleyLive = false; ResetCounters(); path.clear();
    NewTargets(); stamina[N()] = 1.f; for (auto& q : on) { Stam(q) = fminf(1.f, Stam(q) + 0.06f); q.yaw = q.team == 0 ? 0.f : PI; }
    if (!calledPlay) { calledPlay = true; Say(0, serveTeam ? "Blue to serve. Play!" : "Red to serve. Play!"); }
    else if (frand(0, 1) < 0.3f) Say(0, "Quiet, please."); t = 7; tossT = -1; recv = -1; hitAt = -1; rally = 0;
    for (int i = 0; i < N(); i++) if (!P(i).bot) {
      bool srv = on[server].pid == i; bool playing = false; for (auto& p : on) playing |= p.pid == i;
      Layout(i, playing ? std::string("{\"name\":\"tennis\",\"serve\":") + (srv ? "true" : "false") + ",\"lv\":" + std::to_string(level) + "}"
                        : kit::Msg("🎾", "Next up soon!", "You rotate in after this point"));
    }
  }
  // launch the ball from 'from' to land at target, at speed v (m/s), clearing the net
  // ----- the ball: gravity, air drag (it slows down), Magnus lift (topspin makes it dip, backspin makes it float), and a bounce that grips or skids -----
  static constexpr float KD = 0.0194f, KM = 7.5e-4f;
  static void Fly(Vector3& p, Vector3& v, float sp, float dt) {
    float s = Vector3Length(v), vh = hypotf(v.x, v.z);
    Vector3 a = { -KD * s * v.x, -G - KD * s * v.y, -KD * s * v.z };
    if (vh > 0.5f) { float m = KM * sp; a.y += -m * vh; a.x += m * v.y * v.x / vh; a.z += m * v.y * v.z / vh; }
    v = Vector3Add(v, Vector3Scale(a, dt)); p = Vector3Add(p, Vector3Scale(v, dt));
  }
  static void Bounce(Vector3& p, Vector3& v, float& sp) {
    const float e = 0.74f, r = 0.033f; p.y = 0.033f; float vy = -v.y; v.y = vy * e;
    float vh = hypotf(v.x, v.z); if (vh > 0.05f) {   // the court grips the bottom of the ball: topspin kicks it forward, backspin sets it up
      float slip = vh - sp * r, dv = -copysignf(fminf(fabsf(slip) * 0.4f, 0.6f * (1 + e) * vy), slip);
      v.x += dv * v.x / vh; v.z += dv * v.z / vh; sp = Clamp(sp - 1.5f * dv / r, -260.f, 260.f);
    }
  }
  // choose the launch velocity so that THIS physics lands the ball on the target, over the net
  void Launch(Vector3 from, Vector3 target, float speed, float tmul = 1) {
    Vector3 d = { target.x - from.x, 0, target.z - from.z }; float dist = hypotf(d.x, d.z);
    float T = fmaxf(0.55f, dist / (speed * GSPD)) * tmul; Vector3 v{};
    for (int tries = 0; tries < 6; tries++) {
      v = { d.x / T, (0 - from.y + 0.5f * G * T * T) / T, d.z / T };
      float clear = 9;
      for (int it = 0; it < 8; it++) {   // shoot, see where it lands, correct
        Vector3 p = from, w = v; float t = 0; clear = 9;
        while (t < 6) { float zb = p.z; Fly(p, w, spin, 1 / 240.f); t += 1 / 240.f; if ((zb < 0) != (p.z < 0)) clear = p.y; if (p.y <= 0.033f && w.y < 0) break; }
        v.x += (target.x - p.x) / t; v.z += (target.z - p.z) / t; v.y += (T - t) * G * 0.5f;
      }
      if (clear > NETH + 0.3f) break; T *= 1.08f;
    }
    bv = v; bp = from; live = true; bounced = false; bounces = 0; spinR = spin * 0.15f;
    path.clear(); path.push_back(from);
  }
  // where (and when) the receiver should hit: after the bounce, when the ball comes down to ~1 m
  void Predict() {
    Vector3 p = bp, v = bv; bool b = bounced; float tt = 0, sp = spin; const float back = HL + 1.0f;
    for (int k = 0; k < 1800; k++) {
      float dt = 1 / 240.f; Vector3 pv = v; Fly(p, v, sp, dt); tt += dt;
      if (p.y < 0.033f && v.y < 0) { Bounce(p, v, sp); if (b) break; b = true; continue; }
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
  // bullet time on the smash: low, close, from the net side of the hitter
  // slow motion, close on the ball: the camera circles it on a tilted ellipse as it flies, then it craters the court
  void SmashShot(const Pl& p, bool fire = false, bool volley = false) { cine::Shot s; s.kind = cine::SMASH; s.at = bp; s.size = 1.0f; s.c = fire ? Hex(0xff3b1f) : Hex(0xff922b); s.caption = fire ? "FIREBALL!" : volley ? "VOLLEY KILL!" : "SMASH!"; s.sub = radar; s.floorY = 0; s.face = { 0, 0, -Side(p.team) }; cine::Play(s); }
  void WinShot(int team) { for (auto& q : on) if (q.team == team) { cine::Shot s; s.kind = cine::WIN; s.at = Vector3Add(q.pos, { 0, 1.0f, 0 }); s.size = 1.0f; s.c = q.pid >= 0 ? P(q.pid).color : WHITE; s.face = { 0, 0, -Side(team) }; s.floorY = 0; s.hero = q.pid >= 0; cine::Play(s); break; } }
  void DebugShot(int k) override { if (on.empty()) return;
    if (k == cine::REVEAL || k == cine::GOAL) {   // debug: a Hawk-Eye review (REVEAL: just out · GOAL: in, but called out)
      bool in = k == cine::GOAL; Vector3 a = { 0.8f, 1.4f, -3 }, b = { 2.6f, 0.033f, in ? HL - 0.01f : HL + 0.09f };
      path.clear(); for (int i = 0; i <= 150; i++) { float u = i / 150.f; path.push_back({ Lerp(a.x, b.x, u), Lerp(a.y, b.y, u) + 1.6f * u * (1 - u), Lerp(a.z, b.z, u) }); }
      bp = b; lastHitTeam = 0; StartVar(in, in ? 0.01f : 0.04f, true); return;
    } if (k == cine::SMASH) {   // debug: smash the ball the server just tossed (or one at the first player's racket)
      bool toss = phase == "serve" && tossT >= 0 && server >= 0; int hi = toss ? server : 0;
      if (!toss) bp = Vector3Add(RacketPt(on[hi]), { 0.1f, 1.05f, 0 });
      phase = "rally"; tossT = 0; Hit(hi, 1.0f, 0.3f, true); } else if (k == cine::WIN) WinShot(on[0].team); }
  // ----- officials -----
  Vector3 JudgePos(int j) { return j == 0 ? Vector3{ DW + 1.1f, 0, -(HL + 3.0f) } : Vector3{ -(DW + 1.1f), 0, HL + 3.0f }; }
  void Say(int who, const std::string& shown, const std::string& spoken = "") {
    talker = who; talk = shown; talkT = 1.9f; std::string s = "say:" + (spoken.empty() ? shown : spoken); Sfx(s.c_str());
  }
  std::string Prefix(const std::string& why) {
    if (why.rfind("OUT", 0) == 0) return "Out. "; if (why.rfind("NET", 0) == 0) return "Net. "; if (why.rfind("ACE", 0) == 0) return "Ace. ";
    if (why.rfind("SMASH", 0) == 0) return "Smash, what power! "; if (why.rfind("FIREBALL", 0) == 0) return "Fireball! "; if (why.rfind("VOLLEY", 0) == 0) return "Volley winner! "; if (why.rfind("BULLSEYE", 0) == 0) return "Bullseye! "; return "";
  }
  std::string ScoreCall() {   // the server's score first, as an umpire says it
    static const char* NM[] = { "Love", "Fifteen", "Thirty", "Forty" };
    int a = pts[serveTeam], b = pts[1 - serveTeam];
    if (Quick()) { static const char* W[] = { "Love", "One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight" }; return a == b ? std::string(W[std::min(a, 8)]) + " all." : std::string(W[std::min(a, 8)]) + ", " + W[std::min(b, 8)] + "."; }
    if (a >= 3 && b >= 3) return a == b ? "Deuce." : a > b ? "Advantage server." : "Advantage receiver.";
    if (a == b) return std::string(NM[std::min(a, 3)]) + " all.";
    std::string s = std::string(NM[std::min(a, 3)]) + " " + NM[std::min(b, 3)] + "."; s[s.find(' ') + 1] = (char)tolower(s[s.find(' ') + 1]); return s;
  }
  void StartVar(bool in, float m, bool alongX) {
    live = false; phase = "var"; var = Var{}; var.on = true; var.in = in; var.cm = m * 100; var.at = bp; var.alongX = alongX;
    var.team = in ? lastHitTeam : 1 - lastHitTeam; var.path = path; sinceVar = 0;
    judgeOut = bp.z < 0 ? 0 : 1; judgeT = 2.4f; Say(1 + judgeOut, "OUT!", "Out."); Sfx("ooh");
  }
  // ----- LED boards -----
  void LoadTex() {
    if (!adTex.id && HaveFile("ads.png")) { adTex = LoadTexture("ads.png"); if (adTex.id) { GenTextureMipmaps(&adTex); SetTextureFilter(adTex, TEXTURE_FILTER_TRILINEAR); SetTextureWrap(adTex, TEXTURE_WRAP_REPEAT); } }
    if (!scrTex.id && HaveFile("screen.png")) { scrTex = LoadTexture("screen.png"); if (scrTex.id) { GenTextureMipmaps(&scrTex); SetTextureFilter(scrTex, TEXTURE_FILTER_TRILINEAR); } }
  }
  // a self-lit textured panel standing on the ground line a→b (a = its left end, as seen from the front)
  void Led(Texture2D tx, Vector3 a, Vector3 b, float y0, float y1, float u0, float u1, float v0 = 0, float v1 = 1, Color tint = Color{ 238, 238, 238, 255 }) {
    rlSetTexture(tx.id); rlBegin(RL_QUADS); rlColor4ub(tint.r, tint.g, tint.b, 255);
    rlTexCoord2f(u0, v1); rlVertex3f(a.x, y0, a.z); rlTexCoord2f(u1, v1); rlVertex3f(b.x, y0, b.z);
    rlTexCoord2f(u1, v0); rlVertex3f(b.x, y1, b.z); rlTexCoord2f(u0, v0); rlVertex3f(a.x, y1, a.z);
    rlEnd(); rlSetTexture(0);
  }
  // the racket in the right hand, following the wrist bone
  void RacketInHand(const Matrix& m, Color c) {
    Vector3 O = { m.m12, m.m13, m.m14 }, X = Vector3Normalize({ m.m0, m.m1, m.m2 }), Y = Vector3Normalize({ m.m4, m.m5, m.m6 }), Z = Vector3Normalize({ m.m8, m.m9, m.m10 });
    Vector3 D[4] = { X, Vector3Negate(X), Z, Vector3Negate(Z) };
    int k = g_racketAxis % 8; Vector3 dir = D[k % 4], face = k < 4 ? Y : Vector3CrossProduct(dir, Y);
    Vector3 fist = Vector3Add(O, Vector3Scale(Y, 0.075f));
    Matrix xf = RacketXf(Vector3Subtract(fist, Vector3Scale(dir, 0.1f)), dir, face, 1.4f);
    DrawProp(props.racketFrame, xf, c); DrawProp(props.racketRest, xf, WHITE);
  }
  int AiShot(const Pl& me) {   // the bots read the court too: lob the net man, drop the deep one, drive the tired one (EASY barely, HARD nearly always)
    const Pl* o = Opp(me.team); float r = frand(0, 1);
    if (o && frand(0, 1) < (level == 0 ? 0.12f : level == 1 ? 0.45f : 0.85f)) { if (Stam(*o) < 0.4f) return 0; float nn = NetNess(*o); if (nn > 0.5f) return 1; if (nn < 0.28f) return 2; }
    return r < 0.1f ? 1 : r < 0.22f ? 2 : r < 0.42f ? 3 : 0;
  }
  void Hit(int i, float power, float aim, bool good, int shot = 0) {
    Pl& p = on[i]; lastHitTeam = p.team; rally++; if (p.pid >= 0) hits[p.pid]++;
    bool aiP = p.pid < 0 || P(p.pid).bot;
    bool cv = cVolley, cs = cSmash, cn = cNoPower; ResetCounters(); volleyLive = false;   // what the ball that just arrived did to this hit
    static const float SCB[4] = { 0.045f, 0.03f, 0.022f, 0.012f }, SCP[4] = { 0.06f, 0.02f, 0.01f, 0.f };   // breath: a drive tires you out, a slice saves your legs
    Stam(p) = fmaxf(0.f, Stam(p) - (SCB[shot & 3] + SCP[shot & 3] * power)); float fat = Fat(Stam(p)); power *= 0.85f + 0.15f * fat;   // a tired player hits softer
    if (cn) power = fminf(power, 0.65f);   // a slice comes in with no pace: there is nothing to hit it back with
    if (!aiP && level < 2) {   // a little help for people: the shot drifts toward the open part of the court, and an easy-mode swing is always a good one
      float wl0 = Doubles() ? DW : HW, ox = 0; int nn = 0; for (auto& q : on) if (q.team != p.team) { ox += q.pos.x; nn++; }
      if (nn) { ox /= nn; float away = Clamp(-ox / wl0 * 1.6f, -0.95f, 0.95f); aim = Lerp(aim, away, level == 0 ? 0.6f : 0.3f); }
      if (level == 0) good = true;
    }
    // harder swings go deeper and closer to the lines: more winners, more close calls (and the odd one out)
    float risk = Clamp((power - 0.55f) / 0.45f, 0, 1), wl = Doubles() ? DW : HW;
    float tz = -Side(p.team) * Lerp(HL - 2.2f - frand(0, 2.5f) * (good ? 0.6f : 1), HL - frand(-0.25f, 0.9f), risk * 0.85f);
    float tx = Clamp(aim, -1, 1) * wl * (0.78f + 0.2f * risk) + frand(-0.4f, 0.4f) * (1 + risk * 0.4f);
    // a SMASH is rare: a high ball, a hard swing and good timing (the AI only sometimes goes for it) — and nobody can return it
    bool ai = p.pid < 0 || P(p.pid).bot;
    bool smash = bp.y > (ai || level > 0 ? 1.75f : 1.55f) && power >= (ai || level > 0 ? 0.75f : 0.55f) && good && (!ai || frand(0, 1) < AI_SMASH[level]);
    if (cn) smash = false;
    if (cs && good && (!ai || frand(0, 1) < 0.85f)) smash = true;   // a lob that is too short, or to a player on the baseline, is smashed
    if (cv && (good || level == 0)) { smash = true; volleyLive = true; }   // a drop shot to a player at the net is volleyed away for a winner
    float& hv = Heat(p); bool fire = hv >= 1.f && !cn && (!ai || (level > 0 && frand(0, 1) < (level == 1 ? 0.4f : 0.7f)));   // a full POWER meter: this hit is a FIREBALL
    if (fire) smash = true;
    if (smash) { float w = Doubles() ? DW : HW; tz = -Side(p.team) * frand(HL * 0.45f, HL * 0.72f); tx = Clamp(aim, -1, 1) * w * 0.55f; }
    float spd = (fire ? 38 : smash ? 34 : 20 + power * 13) * (good ? 1 : 0.82f) * (ai && level == 0 ? 0.85f : 1.f) * (0.9f + 0.1f * fat);
    if (Pvp() && !smash && rally > 4) spd = fminf(36.f, spd * fminf(1.7f, 1 + 0.06f * (rally - 4)));   // a long rally gets faster until somebody cannot keep up
    spin = smash ? 40.f : power > 0.7f ? 190.f : power < 0.45f ? -70.f : 130.f;   // hard shots carry topspin (they dip into the court), soft ones are sliced
    float tmul = 1;
    if (!smash && shot == 1) { tz = -Side(p.team) * (HL - 0.7f - frand(0, 1.3f)); spd = 13; tmul = 1.12f; spin = 80; }       // LOB: high and deep, over a player who has come in
    else if (!smash && shot == 2) { tz = -Side(p.team) * frand(2.4f, 4.2f); spd = 10; spin = -150; }                           // DROP: backspin, short and soft, drags the other player forward
    else if (!smash && shot == 3) { tz = -Side(p.team) * (HL - 1.7f - frand(0, 1.5f)); tx = Clamp(tx, -wl * 0.7f, wl * 0.7f); spd = 14.5f; tmul = 1.05f; spin = -210; }   // SLICE: low, slow, always in: it takes the pace out of the rally
    if (!smash && ai && shot == 0 && frand(0, 1) < AI_ERR[level]) tz = -Side(p.team) * (HL + frand(0.8f, 2.6f));   // the bot's mistake: it hits long
    if (!smash && shot == 0 && fat < 0.8f && frand(0, 1) < 0.5f * (1.f - fat / 0.8f) + 0.1f) { tz = -Side(p.team) * (HL + frand(0.8f, 2.4f)); if (!aiP) { call = "TIRED ARMS: IT GOES LONG!"; callT = 1.2f; } }   // a worn-out player's drive sails long
    if (!ai && level < 2 && !smash && !Pvp()) { float m = level == 0 ? 0.6f : 0.25f; if (fabsf(tz) > HL - m) tz = copysignf(HL - m, tz); tx = Clamp(tx, -wl + m, wl - m); }   // a person's ball stays in
    if (!smash) {   // the target ring pulls a good shot into it (people: a lot on EASY, a little on HARD; the bots only now and then)
      const Tgt& T = tgt[1 - p.team]; float as = aiP ? 0.f : (level == 0 ? 1.8f : level == 1 ? 1.2f : 0.6f);
      if (aiP && level > 0 && frand(0, 1) < (level == 1 ? 0.06f : 0.12f)) as = 99.f;
      if (hypotf(tx - T.p.x, tz - T.p.z) < as) { tx = T.p.x + frand(-0.2f, 0.2f); tz = T.p.z + frand(-0.2f, 0.2f); }
    }
    if (const Pl* rq = Opp(p.team)) {   // what this shot does to the player who has to return it
      float nn = NetNess(*rq), rs = Stam(*rq); std::string tag; bool good2 = false;
      if (smash) {}
      else if (shot == 0) { if (rs < 0.4f) { cReach = 0.7f; cRet = 0.6f; tag = "DRIVE! They are tired"; good2 = true; } else if (nn > 0.55f) { cReach = 1.2f; tag = "BAD DRIVE: the net man blocks it"; } else if (fabsf(rq->pos.x) > 2.6f) { cReach = 0.8f; cRet = 0.8f; tag = "DRIVE! They are out wide"; good2 = true; } }
      else if (shot == 1) { if (nn > 0.5f) { cReach = 0.5f; cRet = 0.4f; tag = "GREAT LOB over the net man!"; good2 = true; } else if (nn < 0.3f) { cSmash = true; cReach = 1.25f; tag = "BAD LOB: they will smash it"; } }
      else if (shot == 2) { if (nn < 0.3f) { cReach = 0.6f; cRet = 0.5f; Stam(*rq) = fmaxf(0.f, rs - 0.05f); tag = "GREAT DROP! They are deep"; good2 = true; } else if (nn > 0.55f) { cVolley = true; cReach = 1.3f; tag = "BAD DROP: volley winner"; } }
      else if (shot == 3) { cNoPower = true; tag = "SLICE: safe, no pace for them"; good2 = true; }
      if (!tag.empty()) { call = (good2 ? "GOOD: " : "MISTAKE: ") + tag; callT = 1.5f; }
    }
    Launch(bp, { tx, 0, tz }, spd, tmul); smashLive = smash; fireLive = fire;
    static const float HVK[4] = { 1.f, 0.5f, 0.6f, 0.25f };   // the aggressive shots charge POWER, the safe ones hardly do
    if (fire) hv = 0; else if (good) hv = fminf(1.f, hv + (power > 0.8f ? 0.26f : 0.16f) * HVK[shot & 3]);   // good hits charge the POWER meter
    int kmh = (int)(spd * 3.6f * 1.1f); radar = shot == 1 && !smash ? "LOB" : shot == 2 && !smash ? "DROP SHOT" : shot == 3 && !smash ? "SLICE" : I(kmh) + " km/h"; radarT = 1.6f;
    Sfx(smash ? "smash" : "pok"); Shake(smash ? 10 : 2); if (!smash) fx.Burst(bp, 12, WHITE, 3, 0.5f, 0.05f, 2);
    if (smash) SmashShot(p, fire, volleyLive); else if (good && power > 0.8f) { call = "💥 POWER SHOT!"; callT = 1; }
    if (fire) { call = "🔥 FIREBALL!"; callT = 1.4f; Sfx("boom"); } else if (hv >= 1.f && !aiP) { call = "🔥 POWER READY!"; callT = 1.3f; }
    p.sw = 1; p.swDir = bp.x > p.pos.x ? 1.f : -1.f;
    if (rally == 8) Sfx("cheer");
    Predict(); hitDue = time + hitAt; recv = Receiver();
  }
  void Swing(int pid, float power, float aim = NAN, int shot = 0) {
    for (size_t i = 0; i < on.size(); i++) {
      Pl& p = on[i]; if (p.pid != pid || p.cool > 0) continue;
      float aimW = std::isnan(aim) ? P(pid).stick.x : (p.team == 0 ? -aim : aim);   // a flick's "right" is the player's right on screen
      p.cool = 0.3f; p.sw = 1; Sfx("swish");
      if (phase == "serve" && (int)i == server && tossT < 0) { tossT = 0; servePow = power; serveAim = aimW; return; }
      if (phase != "rally" || !live || (int)i != recv) return;
      if (smashLive) { call = "TOO FAST!"; callT = 0.8f; return; }   // a smash can't be returned
      // generous: anywhere near the ball counts; closer to the ideal moment = better shot
      Vector3 rk = RacketPt(p); float d = Vector3Distance(rk, bp);
      if (d < Reach() * RM(p.team) * Fat(Stam(p)) * cReach && bp.z * Side(p.team) > -0.5f) { Hit((int)i, power, aimW, d < Good() * RM(p.team), shot); if (d < (level == 0 ? 2.2f : 1.3f)) { call = "PERFECT!"; callT = 0.8f; } return; }
      if (!smashLive) { qPid = pid; qT = 2.0f; qPow = power; qAim = aimW; qShot = shot; if (callT > 0 && call == "WAIT FOR IT…") callT = 0; return; }   // flick as soon as you see the ball coming: the swing is held and goes the moment it's in reach
      call = "WAIT FOR IT…"; callT = 0.6f;
    }
  }
  float servePow = 0.7f, serveAim = 0;
  void Act(int p, const std::string& k, int n, float pw) override {
    if (k == "tlevel") { level = (int)Clamp((float)n, 0, 2); static const char* NM[] = { "EASY", "NORMAL", "HARD" }; call = std::string("MODE: ") + NM[level]; callT = 1.6f; Sfx("tink"); SendTn(); return; }
    if (k.rfind("swing", 0) != 0) return;
    float aim = NAN; int shot = 0; size_t c = k.find(':');
    if (c != std::string::npos) { float a = 0; char sh = 'd'; if (sscanf(k.c_str() + c + 1, "%f|%c", &a, &sh) >= 1) { aim = Clamp(a, -1, 1); shot = sh == 'l' ? 1 : sh == 'p' ? 2 : sh == 's' ? 3 : 0; } }
    Swing(p, Clamp(pw > 0 ? pw : 0.65f, 0.2f, 1), aim, shot);
  }
  // every player's phone shows the court live: where everyone is, the ball, and the ring that closes on their racket
  float tnT = 0; int qPid = -1, qShot = 0; float qT = 0, qPow = 0.6f, qAim = 0;
  void SendTn() {
    for (size_t i = 0; i < on.size(); i++) {
      Pl& me = on[i]; if (me.pid < 0 || P(me.pid).bot) continue;
      int T = me.team; auto SX = [&](float x) { return T == 0 ? -x : x; }; auto SD = [&](float z) { return T == 0 ? z + HL : HL - z; };
      float d = Vector3Distance(RacketPt(me), bp); bool rm = phase == "rally" && live && (int)i == recv, showBall = live || (phase == "serve" && tossT >= 0);
      std::string pl; for (auto& q : on) { char b[64]; snprintf(b, sizeof b, "%s[%.2f,%.2f,%d]", pl.empty() ? "" : ",", SX(q.pos.x), SD(q.pos.z), &q == &me ? 1 : 0); pl += b; }
      float os = 0; int on2 = 0; for (auto& q : on) if (q.team != T) { os += Stam(q); on2++; } os = on2 ? os / on2 : 1.f; float ft = Fat(Stam(me)) * (rm ? cReach : 1.f);
      const Pl* oq = Opp(T); char ad = 's'; std::string am = "They are set: SLICE stays safe, DRIVE pushes them";
      if (oq) { float nn = NetNess(*oq); if (nn > 0.5f) { ad = 'l'; am = "They are at the NET: LOB over them!"; } else if (nn < 0.28f) { ad = 'p'; am = "They are deep: a DROP shot makes them run!"; } if (Stam(*oq) < 0.4f && ad != 'l') { ad = 'd'; am = "They are TIRED: DRIVE it hard!"; } }
      const Vector3 &ta = tgt[1 - T].p, &tb = tgt[T].p;
      char b[1100]; snprintf(b, sizeof b, "{\"t\":\"tn\",\"ph\":\"%s\",\"live\":%d,\"b\":[%.2f,%.2f,%.2f],\"k\":%.2f,\"z\":%d,\"r\":%d,\"sv\":%d,\"pl\":[%s],\"lv\":%d,\"sa\":%.2f,\"hv\":%.2f,\"os\":%.2f,\"tg\":[[%.2f,%.2f],[%.2f,%.2f]],\"fl\":%d,\"ad\":\"%c\",\"am\":%s,\"msg\":%s}", phase.c_str(), showBall ? 1 : 0, SX(bp.x), SD(bp.z), bp.y, Clamp((d - 1) / 9, 0, 1), d < Good() * ft ? 2 : d < Reach() * ft ? 1 : 0, rm ? 1 : 0, phase == "serve" && (int)i == server ? 1 : 0, pl.c_str(), level, Stam(me), Heat(me), os, SX(ta.x), SD(ta.z), SX(tb.x), SD(tb.z), fireLive ? 1 : 0, ad, kit::Q(am).c_str(), kit::Q(call).c_str());
      Send(me.pid, b);
    }
  }
  void EndMatch(int team, const std::string& pre) {
    phase = "end"; t = 4; live = false; conf.Spawn({ 0, 4, Side(team) * 6 }, 160, 5, 6); Sfx("win"); Banner(TeamName(team) + " WIN THE MATCH!", 2600, team ? "#4dabff" : "#ff6b6b");
    WinShot(team); Say(0, "Game, set and match", pre + "Game, set and match.");
  }
  void Point(int team, const std::string& why) {
    if (phase == "point") return;
    sinceVar++; std::string pre = Prefix(why);
    bool big = level < 2 && (why.find("ACE") != std::string::npos || why.find("SMASH") != std::string::npos || why.find("FIREBALL") != std::string::npos || why.find("BULLSEYE") != std::string::npos || why.find("VOLLEY") != std::string::npos);   // an ace, a smash, a fireball or a bullseye count double
    pts[team] += big ? 2 : 1; ptTeam = team; phase = "point"; t = 2.6f; live = false; call = why + (big ? "  +2" : ""); callT = 2.2f; Sfx(why.find("ACE") != std::string::npos ? "cheer" : "applause");
    for (auto& q : on) { float& hv = Heat(q); if (q.team == team) hv = fminf(1.f, hv + 0.2f); else hv *= 0.5f; }
    int a = pts[team], b = pts[1 - team];
    if (timeUp && a > b) { games[team] = std::max(games[team], games[1 - team] + 1); pts[0] = pts[1] = 0; EndMatch(team, pre); return; }   // overtime: the first to lead wins
    if (Quick() ? a >= PointsToWin() : (a >= 4 && a - b >= 2)) {
      games[team]++; pts[0] = pts[1] = 0; call = (team ? TeamName(1) : TeamName(0)) + " WIN THE GAME!"; callT = 2.4f; Sfx("cheer");
      serveTeam = 1 - serveTeam;
      if (games[team] >= GamesToWin()) { EndMatch(team, pre); return; }
      Say(0, "Game, " + TeamName(team), pre + "Game.");
    }
    if (pts[0] + pts[1] > 0) { std::string sc = ScoreCall(); Say(0, sc.substr(0, sc.size() - 1), pre + sc); }
    // rotate players in for big groups
    for (int tm = 0; tm < 2; tm++) if ((int)teamQ[tm].size() > (Doubles() ? 2 : 1)) { int f = teamQ[tm][0]; teamQ[tm].erase(teamQ[tm].begin()); teamQ[tm].push_back(f); }
  }
  std::string TeamName(int tm) { if (teamQ[tm].empty()) return "MACHINE"; if (teamQ[tm].size() == 1) return P(teamQ[tm][0]).name; return tm ? "BLUE TEAM" : "RED TEAM"; }
  std::string Score(int tm) { if (Quick()) return I(pts[tm]); int a = pts[tm], b = pts[1 - tm]; if (a >= 3 && b >= 3) return a == b ? "40" : a > b ? "AD" : "40"; const char* S[] = { "0", "15", "30", "40" }; return S[std::min(3, a)]; }
  void Update(float dt) override {
    time += dt; fx.Update(dt); conf.Update(dt); callT -= dt; radarT -= dt; t -= dt; freezeT -= dt; superT -= dt;
    tnT -= dt; if (tnT <= 0) { tnT = 0.033f; SendTn(); }   // 30 times a second: the court on the phone moves smoothly
    talkT -= dt; judgeT -= dt; squeakT -= dt; spinA += spinR * dt; squash = fmaxf(0, squash - dt * 9);
    for (auto& m : marks) m.life -= dt;
    for (int i = 0; i < N(); i++) if (!P(i).bot && P(i).a && !P(i).pa) Swing(i, 0.65f);
    for (auto& p : on) { p.sw = fmaxf(0, p.sw - dt * 3); p.cool -= dt; }
    UpdateKids(dt);
    if (phase == "intro") { if (t <= 0) phase = "serve"; return; }
    if (phase != "end") {
      matchT += dt; float lim = TimeLimit();
      if (lim > 0 && !timeUp && matchT >= lim) {   // time: whoever leads wins; if it is level, the next point decides
        timeUp = true; Sfx("alarm");
        if (pts[0] != pts[1] && (phase == "serve" || phase == "point")) { int ld = pts[0] > pts[1] ? 0 : 1; games[ld] = std::max(games[ld], games[1 - ld] + 1); EndMatch(ld, ""); return; }
        call = pts[0] == pts[1] ? "OVERTIME! NEXT POINT WINS" : "TIME! THE LEADER WINS"; callT = 2.6f;
      }
    }
    if (phase == "end") { if (t <= 0 && !fin) { fin = true; Sfx("crowdOff"); std::vector<int> o; int w = games[0] > games[1] ? 0 : 1; for (int i : teamQ[w]) o.push_back(i); for (int i : teamQ[1 - w]) o.push_back(i);
        std::stable_sort(o.begin(), o.begin() + teamQ[w].size(), [&](int a, int b) { return hits[a] > hits[b]; });
        FinishOrder(o, [&](int i) { int tm = i % 2; return std::string(tm == w ? "won " : "lost ") + I(games[tm]) + "-" + I(games[1 - tm]) + " · " + I(hits[i]) + " hits"; }); } return; }
    if (phase == "point" || phase == "var") for (auto& q : on) Stam(q) = fminf(1.f, Stam(q) + dt * 0.025f);   // a breather between points
    if (phase == "var") {   // Hawk-Eye: the line judge's call, the replay, the verdict
      var.t += dt;
      if (var.stage == 0 && var.t > 0.9f) { var.stage = 1; Sfx("varSting"); }
      if (var.stage == 1 && var.t > 3.4f) { var.stage = 2; Sfx(var.in ? "varIn" : "varOut"); Sfx(var.in ? "cheer" : "applause"); }
      if (var.t > 5.6f) { var.on = false; Point(var.team, var.in ? "IN! OVERRULED" : "OUT!"); }
      return;
    }
    if (phase == "point") { if (t <= 0) SetupPoint(); return; }
    // movement: the receiver runs to stand beside the ball (racket side); everyone else recovers to their ready spot.
    // Real-ish footwork: accelerate up to ~7 m/s, brake smoothly, always facing the net.
    for (size_t i = 0; i < on.size(); i++) {
      Pl& p = on[i]; bool netMan = Doubles() && fabsf(p.pos.z) < 6 && (int)i != recv; bool aiP = p.pid < 0 || P(p.pid).bot;
      float netYaw = p.team == 0 ? 0.f : PI; if (std::isnan(p.yaw)) p.yaw = netYaw;
      if (freezeT > 0 && p.team != cheatTeam) { p.vel = { 0, 0, 0 }; continue; }
      float mul = (superT > 0 && p.team == cheatTeam) ? 2.2f : 1.f; float& sm = Stam(p); float fat = Fat(sm);
      if (phase == "rally" && (int)i == recv) p.goal = { Clamp(hitPt.x - HandSide(p.team), -DW - 1.2f, DW + 1.2f), 0, Clamp(hitPt.z, -HL - 1.5f, HL + 1.5f) };
      else if (phase == "rally") { float bx2 = live ? bp.x : 0; p.goal = { Clamp(bx2 * 0.35f + (netMan ? (p.goal.x > 0 ? 1.2f : -1.2f) : 0), -3, 3), 0, Side(p.team) * (netMan ? 3.5f : HL + 0.4f) }; }
      Vector3 d = Vector3Subtract(p.goal, p.pos); d.y = 0; float l = Vector3Length(d);
      // run so as to be there when the ball is (no sprint-and-wait), jog back to the ready spot, never faster than a person can
      float top = (aiP ? AI_RUN[level] : 6.6f) * mul * fat;
      float sp = phase == "rally" && (int)i == recv ? Clamp(l / fmaxf(0.3f, hitDue - time) * 1.2f, 1.6f, top) : top * (phase == "rally" ? 0.55f : 0.8f);
      Vector3 want = l > 0.02f ? Vector3Scale(d, fminf(sp, l * 3.2f) / l) : Vector3{ 0, 0, 0 };
      Vector3 dv = Vector3Subtract(want, p.vel); float dl = Vector3Length(dv), acc = 19 * dt * mul * (0.6f + 0.4f * fat);
      if (dl > acc) dv = Vector3Scale(dv, acc / dl);
      if (dl > 4.5f && Vector3Length(p.vel) > 1.8f && squeakT <= 0) { Sfx("squeak"); squeakT = 0.4f; }   // shoes on the hard court
      p.vel = Vector3Add(p.vel, dv); p.pos = Vector3Add(p.pos, Vector3Scale(p.vel, dt));
      float v = Vector3Length(p.vel);
      sm = Clamp(sm - v * dt * ((int)i == recv ? 0.0082f : 0.004f) + (v < 0.8f ? dt * 0.016f : 0.f), 0.f, 1.f);   // running costs breath, standing still gives it back
      if (!aiP && sm < 0.22f && !p.warn) { p.warn = true; call = P(p.pid).name + " IS TIRED!"; callT = 1.3f; } if (sm > 0.4f) p.warn = false;
      // facing: the net when set, the direction of the run when sprinting (turning at a human speed); the stride follows the ground covered
      float wy = netYaw; if (v > 2.6f && p.sw < 0.05f) wy = netYaw + angDiff(netYaw, atan2f(p.vel.x, p.vel.z)) * Clamp((v - 2.6f) / 2.2f, 0.f, 1.f);
      p.yaw += Clamp(angDiff(p.yaw, wy), -9.f * dt, 9.f * dt); p.ph += dt * (0.8f + v * 0.17f);
      p.vs = Vector3Lerp(p.vs, p.vel, 1 - expf(-9 * dt));
    }
    if (phase == "serve") {
      Pl& s = on[server];
      if (tossT < 0) { if ((s.pid < 0 || P(s.pid).bot || g_dbg == 21 || g_dbg == 22) && t < 5.6f) { tossT = 0; servePow = level == 0 ? frand(0.4f, 0.65f) : frand(0.6f, 1); serveAim = frand(-0.7f, 0.7f); s.sw = 0; } if (t <= 0) { tossT = 0; servePow = 0.6f; serveAim = 0; } return; }
      tossT += dt; bp = { s.pos.x + 0.3f, 1.2f + 6.0f * tossT - 0.5f * G * tossT * tossT * 1.0f, s.pos.z }; live = false;
      if (tossT >= 0.55f) {  // hit at the top of the toss
        float sx = s.pos.x > 0 ? -1.f : 1.f;
        Vector3 target = { sx * frand(0.6f, HW - 0.5f) * 0.9f + serveAim * 0.6f, 0, -Side(s.team) * frand(SVC - 2.2f, SVC - 0.5f) };
        target.x = Clamp(target.x, -HW + 0.3f, HW - 0.3f); if (target.x * sx < 0.2f) target.x = sx * 0.6f;
        bp.y = 2.7f; lastHitTeam = s.team; float spd = 20 + servePow * 11; spin = 70 + servePow * 60; Launch(bp, target, spd);
        radar = I((int)(spd * 3.6f * 1.15f)) + " km/h"; radarT = 1.6f; Sfx("smash"); s.sw = 1; phase = "rally"; Predict(); hitDue = time + hitAt; recv = Receiver();
      }
      return;
    }
    if (phase == "rally" && live) {
      int steps = 4; float h = dt / steps;
      for (int k = 0; k < steps && live; k++) {
        float zb = bp.z; Fly(bp, bv, spin, h);
        path.push_back(bp); if (path.size() > 900) path.erase(path.begin(), path.begin() + 300);
        if ((zb < 0) != (bp.z < 0) && bp.y < NETH) { live = false; Sfx("netHit"); Point(1 - lastHitTeam, "NET!"); return; }
        if (bp.y < 0.033f && bv.y < 0) {
          float my = atan2f(bv.x, bv.z);
          Bounce(bp, bv, spin); bounces++; Sfx("bounce"); spinR = spin * 0.15f;
          squash = 1; marks.push_back({ bp, 3.0f, my }); if (marks.size() > 12) marks.erase(marks.begin());
          if (smashLive && bounces == 1) { cine::Impact(bp); Sfx("boom"); Shake(12); }
          float w = Doubles() ? DW : HW; bool onRecvSide = bp.z * Side(lastHitTeam) < 0;
          if (bounces == 1 && !smashLive && onRecvSide) {   // close to a line: the line judge calls it, Hawk-Eye decides
            float ox = fabsf(bp.x) - (w + 0.05f), oz = fabsf(bp.z) - (HL + 0.05f), m = fmaxf(ox, oz); bool alongX = oz > ox;
            if (m > 0 && m < 0.16f && sinceVar >= 2) { StartVar(false, m, alongX); return; }
            if (m <= 0 && m > -0.07f && sinceVar >= 2 && frand(0, 1) < 0.45f) { StartVar(true, -m, alongX); return; }
          }
          if (bounces == 1 && (!onRecvSide || fabsf(bp.x) > w + 0.05f || fabsf(bp.z) > HL + 0.05f)) { live = false; Point(1 - lastHitTeam, "OUT!"); return; }
          if (bounces == 1 && !smashLive) { const Tgt& T = tgt[1 - lastHitTeam]; if (hypotf(bp.x - T.p.x, bp.z - T.p.z) < 0.95f) { live = false; fx.Burst(Vector3{ T.p.x, 0.3f, T.p.z }, 40, Hex(0xffd43b), 6, 0.9f, 0.15f, 3); Point(lastHitTeam, "BULLSEYE! 🎯"); return; } }   // the ball lands in the ring
          if (bounces >= 2) { live = false; Point(lastHitTeam, smashLive ? (fireLive ? "FIREBALL!" : volleyLive ? "VOLLEY WINNER!" : "SMASH WINNER!") : rally <= 1 ? "ACE! 🎾" : "WINNER!"); return; }
        }
      }
      trail.push_back(bp); if (trail.size() > 16) trail.erase(trail.begin());
      if (fireLive) fx.Puff(bp, Hex(0xff7b1a, 200), 0.18f, 0.35f);   // the fireball burns
      if (qT > 0) {
        qT -= dt;
        if (recv >= 0 && on[recv].pid == qPid) { Vector3 rk = RacketPt(on[recv]); float d = Vector3Distance(rk, bp); if (d < (Pvp() ? 3.6f : level == 0 ? 5.0f : level == 1 ? 3.6f : 3.0f) * cReach && bp.z * Side(on[recv].team) > -0.5f) { qT = 0; Hit(recv, qPow, qAim, true, qShot); } }
        else qT = 0;
      }
      // the AI (bots + the practice machine) hits on time, most of the time
      if (recv >= 0) { Pl& r = on[recv]; bool ai = r.pid < 0 || P(r.pid).bot || g_dbg == 21 || g_dbg == 22; /* 21 = debug autoplay */ Vector3 rk = RacketPt(r);
        if (ai && !smashLive && Vector3Distance(rk, bp) < AI_REACH[level] * Fat(Stam(r)) * cReach && bounces == 1) { if (frand(0, 1) < (AI_RET[level] + (r.pid < 0 ? 0.05f : 0.f)) * (0.7f + 0.3f * Fat(Stam(r))) * cRet) Hit(recv, level == 0 ? frand(0.3f, 0.6f) : level == 1 ? frand(0.4f, 0.8f) : frand(0.45f, 0.9f), frand(-0.8f, 0.8f), true, AiShot(r)); else { recv = -1; } } }
      if (fabsf(bp.z) > HL + 9 || fabsf(bp.x) > 14) { live = false; Point(bounces >= 1 ? lastHitTeam : 1 - lastHitTeam, bounces >= 1 ? (smashLive ? (fireLive ? "FIREBALL!" : volleyLive ? "VOLLEY WINNER!" : "SMASH WINNER!") : "WINNER!") : "OUT!"); }
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
    LoadTex();
    if (live && hypotf(bv.x, bv.z) > 0.5f) spinAx = Vector3Normalize(Vector3CrossProduct({ 0, 1, 0 }, Vector3Normalize({ bv.x, 0, bv.z })));
    // broadcast camera: high behind the near baseline, drifting gently with the ball
    // ...holding a close-up on the server before the serve, and cutting in on the winner after the point
    float bx = live ? Clamp(bp.x, -4, 4) * 0.25f : 0;
    // (close-ups are cuts that slowly push in; the pull back to the wide shot on the toss is one smooth crane move)
    int shot = phase == "var" ? (var.t < 0.9f ? 3 : 4) : phase == "serve" && tossT < 0 && server >= 0 && server < (int)on.size() ? 1 : phase == "point" && ptTeam >= 0 ? 2 : 0;
    if (shot != camShot) { if (shot) rig.Cut(); camShot = shot; shotT = 0; }
    shotT += g_frameDt; float push = 1.3f - 0.3f * EaseOutQ(shotT / 2.4f);
    // the TV is behind the player who is serving, a little above head height (it cuts to the other end when the serve changes sides)
    float sd = Side(serveTeam); if (sd != camSide) { if (camSide != 0) rig.Cut(); camSide = sd; }
    Vector3 wp = { bx * 1.2f, 3.3f, sd * (HL + 6.6f) }, wt = { bx * 0.8f, 0.8f, -sd * 4.0f }; float wf = 46, k = 2.6f;
    const Pl* hero = nullptr;
    if (shot == 1) hero = &on[server]; else if (shot == 2) for (auto& o : on) if (o.team == ptTeam) { hero = &o; break; }
    if (hero) { float sd = -Side(hero->team); wt = { hero->pos.x + 0.2f * sd, 1.1f, hero->pos.z }; wp = Vector3Add(wt, Vector3Scale({ 2.8f * sd, 0.35f, 3.9f * sd }, push)); wf = 34; k = 3.0f; }
    if (shot == 3) { Vector3 j = JudgePos(judgeOut < 0 ? 0 : judgeOut); float sz = j.z < 0 ? 1.f : -1.f; wt = { j.x, 1.35f, j.z }; wp = Vector3Add(wt, Vector3Scale({ -1.4f * sz, 0.3f, 3.4f * sz }, push)); wf = 32; k = 3.0f; }
    if (shot == 4) {   // Hawk-Eye: high over the line, looking down at the mark
      float sx = var.at.x < 0 ? -1.f : 1.f, sz = var.at.z < 0 ? -1.f : 1.f; wt = { var.at.x, 0.05f, var.at.z };
      Vector3 off = var.alongX ? Vector3{ 0.9f, 2.2f, -2.1f * sz } : Vector3{ -2.1f * sx, 2.2f, 0.9f };
      wp = Vector3Add(wt, Vector3Scale(off, 1.25f + 0.35f * (1 - EaseOutQ(shotT / 3.0f)))); wf = 34; k = 3.2f;
    }
    { Camera3D rc = rig.Go(wp, wt, wf, k); cam.position = rc.position; cam.target = rc.target; cam.fovy = rc.fovy; }
    if (smashLive && live) cine::Follow(bp);
    if (g_dbg == 23) { cam.position = { 1.0f, 8.5f, -19.0f }; cam.target = { 0, 0, 1.0f }; cam.fovy = 55; }   // debug: the whole court, from above the baseline
    if ((g_dbg == 7 || g_dbg == 22) && !on.empty()) { Pl& q = on[0]; cam.position = { q.pos.x + 2.2f, 1.6f, q.pos.z + (q.team ? -2.6f : 2.6f) }; cam.target = { q.pos.x + 0.3f, 1.0f, q.pos.z }; cam.fovy = 40; }
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
    // the chair umpire watches the ball and nods as they call the score
    { Vector3 u = { -DW - 1.6f, 2.06f, 0 }; float yaw = PI / 2; Vector3 look = live ? bp : (phase == "var" ? var.at : Vector3{ 0, 0.8f, 0 });
      float la = angDiff(yaw, atan2f(look.x - u.x, look.z - u.z));
      PersonPose q; q.pos = u; q.yaw = yaw; q.c = Hex(0x1e3a5f); q.hsh = 3; q.height = 1.74f; q.clip = "Idle_Neutral"; q.t = time;
      q.headYaw = Clamp(la, -1.2f, 1.2f); q.headPitch = 0.22f + (talkT > 0 && talker == 0 ? 0.07f * sinf(time * 15) : 0);
      if (!DrawPersonPose(q)) DrawToon({ -DW - 1.6f, 2.0f, 0 }, PI / 2, Hex(0xe9ecef), time, 0, 0, 0.8f); }
    // line judges behind each baseline: eyes on the ball, an arm straight out when it's out
    for (int j = 0; j < 2; j++) {
      Vector3 jp = JudgePos(j); float yaw = j == 0 ? 0 : PI; Vector3 look = live ? bp : (phase == "var" ? var.at : Vector3{ 0, 0.5f, 0 });
      float la = angDiff(yaw, atan2f(look.x - jp.x, look.z - jp.z)); bool pointing = judgeOut == j && judgeT > 0;
      float turn = pointing ? Clamp(la, -1.2f, 1.2f) : 0;
      PersonPose q; q.pos = jp; q.c = Hex(0x1e3a5f); q.hsh = 7 + j * 5; q.height = 1.76f; q.clip = "Idle_Neutral"; q.t = time + j * 1.3f;
      q.yaw = yaw + turn; q.headYaw = Clamp(la - turn, -1.0f, 1.0f);
      if (pointing) { q.clip2 = "Idle_Gun_Pointing"; q.t2 = time; q.mix = Clamp(judgeT * 3, 0, 1); }
      DrawPersonPose(q);
    }
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
    // LED ad boards: the other Couch Chaos games, scrolling round the court like a real tour event
    if (adTex.id) {
      float bw = 2 * DW + 6, u = fmodf(time * 0.018f, 1.f);
      for (int s = -1; s <= 1; s += 2) {
        lit.Draw(prim.cube, TRS({ 0, 0.5f, s * (HL + 5.4f) }, { 0, 0, 0 }, { bw + 0.1f, 1.06f, 0.2f }), Hex(0x111418));
        float z = s * (HL + 5.4f) - s * 0.11f, xl = s > 0 ? DW + 3 : -DW - 3, u0 = u + (s > 0 ? 0.f : 0.5f);
        Led(adTex, { xl, 0, z }, { -xl, 0, z }, 0.05f, 0.97f, u0, u0 + 0.5f);
      }
      for (int s = -1; s <= 1; s += 2) {
        float x = s * (10.75f - 0.115f), zl = s > 0 ? -(HL + 4) : HL + 4, u0 = -u * 1.3f + (s > 0 ? 0.25f : 0.75f);
        Led(adTex, { x, 0, zl }, { x, 0, -zl }, 0.14f, 1.12f, u0, u0 + 0.875f);
      }
    }
    else for (int s = -1; s <= 1; s += 2) for (int k = 0; k < 6; k++) {
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
    if (scrTex.id) { float v0 = phase == "var" ? 0.5f : 0.f; Led(scrTex, { 5.1f, 0, HL + 17.13f }, { -5.1f, 0, HL + 17.13f }, 11.45f, 16.15f, 0, 1, v0, v0 + 0.5f); }   // the big screen (Hawk-Eye during a review)
    else {
      DrawCube({ 0, 13.8f, HL + 17.15f }, 10.2f, 4.7f, 0.05f, Hex(0x1c7ed6));
      DrawCube({ 0, 14.9f, HL + 17.1f }, 9.0f, 1.1f, 0.05f, Hex(0x74c0fc));
      DrawCube({ -2.4f, 13.0f, HL + 17.1f }, 4.0f, 1.2f, 0.05f, Hex(0xffd43b));
      DrawCube({ 2.4f, 13.0f, HL + 17.1f }, 4.0f, 1.2f, 0.05f, Hex(0xff8787));
    }
    if (phase == "serve" || phase == "rally") for (int tm = 0; tm < 2; tm++) {   // the glowing target rings: land the ball in one for a BULLSEYE (a double point)
      const Vector3& tp = tgt[tm].p; float pu = 0.5f + 0.5f * sinf(time * 4 + tm), rr = 1.0f;
      lit.Draw(prim.cyl, TRS({ tp.x, 0.014f, tp.z }, { 0, 0, 0 }, { rr * 2, 0.004f, rr * 2 }), Fade(Hex(0xffd43b), 0.22f + 0.18f * pu));
      lit.Draw(prim.cyl, TRS({ tp.x, 0.016f, tp.z }, { 0, 0, 0 }, { rr * 1.1f, 0.004f, rr * 1.1f }), Fade(Hex(0xff4d6d), 0.45f + 0.2f * pu));
      for (int k = 0; k < 3; k++) DrawCircle3D({ tp.x, 0.03f, tp.z }, rr * (0.96f + 0.03f * k), { 1, 0, 0 }, 90, Fade(Hex(0xfff3bf), 0.9f));
    }
    // players
    // real footwork (run, side-step, backpedal), a swing that blends over whatever the legs are doing,
    // eyes on the ball, and the racket in the right hand
    for (size_t pi = 0; pi < on.size(); pi++) {
      Pl& p = on[pi];
      Color c = p.pid >= 0 ? P(p.pid).color : Hex(0xadb5bd);
      float yaw = std::isnan(p.yaw) ? (p.team == 0 ? 0.f : PI) : p.yaw;
      Vector3 fw = { sinf(yaw), 0, cosf(yaw) }, rt = { -cosf(yaw), 0, sinf(yaw) };
      float spd = Vector3Length(p.vs), vf = Vector3DotProduct(p.vs, fw), vr = Vector3DotProduct(p.vs, rt);
      Vector3 look = live ? bp : Vector3{ 0, 1, 0 };
      float la = angDiff(yaw, atan2f(look.x - p.pos.x, look.z - p.pos.z)), body = Clamp(la, -0.7f, 0.7f) * 0.35f;
      PersonPose q; q.pos = p.pos; q.c = c; q.hsh = PersonHash(c); q.height = 1.8f; q.yaw = yaw + body; q.headYaw = Clamp(la - body, -1.1f, 1.1f);
      q.headPitch = live ? Clamp(-atan2f(bp.y - 1.6f, fmaxf(1.f, hypotf(bp.x - p.pos.x, bp.z - p.pos.z))), -0.5f, 0.4f) : 0;
      if (spd > 0.6f) { q.clip = fabsf(vr) > fabsf(vf) * 0.8f ? (vr > 0 ? "Run_Right" : "Run_Left") : vf < -0.3f ? "Run_Back" : "Run"; q.t = p.ph + p.pid * 0.37f; }
      else { q.clip = "Idle_Sword"; q.t = time + p.pid * 0.7f; }
      bool serving = phase == "serve" && (int)pi == server && tossT >= 0;
      if (p.sw > 0) { q.clip2 = "Sword_Slash"; q.scrub2 = true; q.t2 = 0.28f + (1 - p.sw) * 0.72f; q.mix = Clamp(p.sw * 3, 0, 1); }
      else if (serving) { q.clip2 = "Sword_Slash"; q.scrub2 = true; q.t2 = 0.05f + Clamp(tossT / 0.55f, 0, 1) * 0.22f; q.mix = 1; }
      if (phase == "point" && p.team == ptTeam) { q.clip = "Interact"; q.scrub = true; q.t = 0.5f + 0.5f * Clamp((2.6f - t) / 0.6f, 0, 1); q.clip2 = nullptr; q.mix = 0; }
      Matrix wm = MatrixIdentity(); q.wrist = &wm;
      if (DrawPersonPose(q)) RacketInHand(wm, c);
      else {
        g_hold.on = true; g_hold.hand = HandLocal(p, nullptr, nullptr);
        DrawToon(p.pos, yaw, c, time + p.pid, spd > 0.6f ? 0.12f : 0, phase == "point" && p.team == ptTeam ? 1.f : 0, 1.05f);
        g_hold.on = false; Racket(p, c);
      }
    }
    // timing ring: closes around the receiver as the ball arrives
    if (phase == "rally" && live && recv >= 0 && on[recv].pid >= 0 && !P(on[recv].pid).bot) {
      Pl& r = on[recv]; Vector3 rp = RacketPt(r); float d = Vector3Distance(rp, bp), k = Clamp((d - 1.0f) / 9.0f, 0, 1);
      Color rc = d < 4.6f ? (d < 2.4f ? Hex(0x51cf66) : Hex(0xffd43b)) : Hex(0xffffff);
      DrawCircle3D({ rp.x, 0.03f, rp.z }, 0.7f + k * 3.2f, { 1, 0, 0 }, 90, Fade(rc, 0.95f));
      DrawCircle3D({ rp.x, 0.035f, rp.z }, 0.72f + k * 3.2f, { 1, 0, 0 }, 90, Fade(rc, 0.95f));
      DrawCircle3D({ rp.x, 0.03f, rp.z }, 0.7f, { 1, 0, 0 }, 90, Fade(WHITE, 0.6f));
    }
    // ball + trail + shadow
    if (live || phase == "serve") {
      if (phase != "serve" || tossT >= 0) {
        float bs = smashLive && cine::Busy() ? 0.085f : 0.05f, sq = squash;   // the smash ball reads big in the close-up
        Matrix bm = MatrixMultiply(MatrixMultiply(MatrixRotate(spinAx, spinA), MatrixScale(bs * (1 + 0.25f * sq), bs * (1 - 0.35f * sq), bs * (1 + 0.25f * sq))), MatrixTranslate(bp.x, bp.y - 0.012f * sq, bp.z));
        DrawProp(props.tennisBall, bm); lit.Caster({ bp.x, 0, bp.z }, 0.08f);
        if (!smashLive) for (size_t k = 1; k < trail.size(); k++) Limb(trail[k - 1], trail[k], 0.03f * k / trail.size(), Fade(Hex(0xf4ffb0), 0.6f));
      }
    }
    for (auto& m : marks) if (m.life > 0) lit.Draw(prim.cyl, TRS({ m.p.x, 0.012f, m.p.z }, { 0, m.yaw, 0 }, { 0.075f, 0.002f, 0.15f }), Fade(Hex(0x173a5c), Clamp(m.life / 1.5f, 0, 1) * 0.5f));
    if (phase == "var" && var.t > 0.9f && !var.path.empty()) {   // Hawk-Eye replay: the ball's last half second, then the mark it left
      int np = (int)var.path.size(), K = std::min(np, 140), s0 = np - K; float rt = Clamp((var.t - 1.1f) / 2.1f, 0, 1);
      int idx = s0 + (int)(EaseOutQ(rt) * (K - 1));
      for (int q = s0; q + 3 <= idx; q += 3) {
        Vector3 a = var.path[q], b = var.path[q + 3];
        Limb({ a.x, 0.014f, a.z }, { b.x, 0.014f, b.z }, 0.03f, Fade(Hex(0x0b2a44), 0.45f));   // its shadow on the court
        Limb(a, b, 0.03f, Fade(Hex(0x5ce1ff), 0.85f));
      }
      rlDrawRenderBatchActive(); BeginBlendMode(BLEND_ADDITIVE);
      for (int q = s0; q + 3 <= idx; q += 3) DrawCylinderEx(var.path[q], var.path[q + 3], 0.06f, 0.06f, 6, Fade(Hex(0x2bb8ff), 0.35f));
      rlDrawRenderBatchActive(); EndBlendMode(); BeginBlendMode(BLEND_ALPHA);
      Vector3 g = var.path[idx];
      DrawProp(props.tennisBall, MatrixMultiply(MatrixScale(0.05f, 0.05f, 0.05f), MatrixTranslate(g.x, g.y, g.z)));
      if (rt >= 1) {
        float pr = fmodf(var.t * 1.4f, 1.f);
        DrawCircle3D({ var.at.x, 0.03f, var.at.z }, 0.12f + pr * 0.5f, { 1, 0, 0 }, 90, Fade(Hex(0x5ce1ff), 1 - pr));
        lit.Draw(prim.cyl, TRS({ var.at.x, 0.017f, var.at.z }, { 0, 0, 0 }, { 0.07f, 0.003f, 0.1f }), Hex(0xe9ecef));
        Color lc = var.in ? Hex(0x51cf66) : Hex(0xff4d6d);
        if (var.alongX) lit.Draw(prim.cube, TRS({ var.at.x, 0.02f, copysignf(HL, var.at.z) }, { 0, 0, 0 }, { 1.6f, 0.006f, 0.055f }), lc);
        else lit.Draw(prim.cube, TRS({ copysignf(Doubles() ? DW : HW, var.at.x), 0.02f, var.at.z }, { 0, 0, 0 }, { 0.055f, 0.006f, 1.6f }), lc);
      }
    }
    DrawKids();
    fx.Draw(); conf.Draw();
    EndMode3D();
    // scoreboard
    kit::Glass(520, 14, 560, 70, 22);
    Text(560, 37, 22, Hex(0xff8787), TeamName(0), -1, TS_HEAVY | TS_PLAIN); Text(560, 64, 22, Hex(0x74c0fc), TeamName(1), -1, TS_HEAVY | TS_PLAIN);
    if (!Quick()) { Text(930, 37, 22, Hex(0xffe066), I(games[0]), 0, TS_DISPLAY); Text(930, 64, 22, Hex(0xffe066), I(games[1]), 0, TS_DISPLAY); }
    Text(1010, 37, 22, WHITE, Score(0), 0, TS_DISPLAY); Text(1010, 64, 22, WHITE, Score(1), 0, TS_DISPLAY);
    if (!Quick()) Text(930, 21, 13, Hex(0xdfe4f2), "GAMES", 0, TS_PLAIN | TS_SPACED); Text(1010, 21, 13, Hex(0xdfe4f2), "PTS", 0, TS_PLAIN | TS_SPACED);
    { static const char* LV[] = { "😊 EASY · first to 5 points", "🎾 NORMAL · first to 7 points", "🔥 HARD · classic: deuce, first to 2 games" }; float lw = Measure(LV[level], 20, TS_HEAVY | TS_PLAIN) + 38; RRect(800 - lw / 2, 86, lw, 32, 16.f, Color{ 8, 14, 36, 185 }, Color{ 8, 14, 36, 185 }, BLANK, 0); Text(800, 102, 20, Hex(level == 0 ? 0x8ce99a : level == 1 ? 0xffe066 : 0xff8787), LV[level], 0, TS_HEAVY | TS_PLAIN); }
    if (TimeLimit() > 0 && phase != "end" && phase != "intro") {   // the match clock
      float left = fmaxf(0.f, TimeLimit() - matchT); char tb[48]; if (timeUp) snprintf(tb, sizeof tb, "OVERTIME · first to lead wins"); else snprintf(tb, sizeof tb, "⏱ %d:%02d", (int)left / 60, (int)left % 60);
      float cw = Measure(tb, 20, TS_HEAVY | TS_PLAIN) + 34; RRect(800 - cw / 2, 121, cw, 30, 15, Color{ 8, 14, 36, 185 }, Color{ 8, 14, 36, 185 }, BLANK, 0);
      Text(800, 136, 20, Hex(timeUp || left < 20 ? 0xff8787 : 0xdfe4f2), tb, 0, TS_HEAVY | TS_PLAIN);
    }
    for (auto& p : on) if (p.pid >= 0) { Vector2 s = GetWorldToScreen({ p.pos.x, 2.2f, p.pos.z }, cam); if (s.y < 150 && s.x > 480 && s.x < 1120) continue; Text(s.x, s.y - 4, 23, P(p.pid).color, P(p.pid).name, 0, TS_HEAVY | TS_SHADOW);
      float sm = Stam(p), hv = Heat(p); Color bc = sm > 0.5f ? Hex(0x51cf66) : sm > 0.25f ? Hex(0xffd43b) : Hex(0xff4d6d), bg = Color{ 8, 14, 36, 200 };   // stamina (green > yellow > red) and the POWER meter
      RRect(s.x - 31, s.y + 13, 62, 9, 4.5f, bg, bg, BLANK, 0); RRect(s.x - 30, s.y + 14, fmaxf(7.f, 60 * sm), 7, 3.5f, bc, bc, BLANK, 0);
      RRect(s.x - 31, s.y + 24, 62, 7, 3.5f, bg, bg, BLANK, 0); RRect(s.x - 30, s.y + 25, fmaxf(0.f, 60 * hv), 5, 2.5f, hv >= 1.f ? Hex(0xffd43b) : Hex(0xff922b), hv >= 1.f ? Hex(0xff3b1f) : Hex(0xff7b1a), BLANK, 0);
      if (hv >= 1.f) Text(s.x + 46, s.y + 20, 22, Hex(0xff7b1a), "🔥", 0, TS_PLAIN); }
    if ((phase == "serve" || phase == "rally" || phase == "point") && matchT < 30.f) { std::string tip = "🎯 ring = 2 points  ·  🔥 full POWER = FIREBALL  ·  LOB beats the net  ·  DROP beats deep  ·  DRIVE tires  ·  SLICE is safe"; float tw = Measure(tip, 19, TS_HEAVY | TS_PLAIN) + 40; RRect(800 - tw / 2, 858, tw, 34, 17, Color{ 8, 14, 36, 175 }, Color{ 8, 14, 36, 175 }, BLANK, 0); Text(800, 875, 19, Hex(0xe9ecff), tip, 0, TS_HEAVY | TS_PLAIN); }
    if (phase == "serve" && server >= 0 && tossT < 0) { Pl& s = on[server]; Vector2 sp = GetWorldToScreen({ s.pos.x, 2.7f, s.pos.z }, cam); if (camShot == 1) sp = { 800, 760 }; /* the close-up: under the server, clear of the umpire's caption */ Text(sp.x, sp.y, 26, Hex(0xffe066), s.pid >= 0 && !P(s.pid).bot ? "SWING TO SERVE!" : "serving…", 0, TS_DISPLAY | TS_SHADOW); }
    if (talkT > 0 && !talk.empty()) {   // who is talking: a bubble over their head
      Vector3 hp = talker == 0 ? Vector3{ -DW - 1.6f, 4.25f, 0 } : Vector3Add(JudgePos(talker - 1), { 0, 2.05f, 0 });
      Camera3D cc = CineCam(cam); float a = Clamp(talkT * 3, 0, 1) * Clamp((1.9f - talkT) * 8, 0, 1);
      bool front = Vector3DotProduct(Vector3Subtract(hp, cc.position), Vector3Subtract(cc.target, cc.position)) > 0;
      Vector2 s = GetWorldToScreen(hp, cam); float fs = talker ? 52.f : 30.f, w = Measure(talk, fs, TS_DISPLAY) + 50, bh = fs + 26;
      if (front && s.x > 60 && s.x < 1540 && s.y > 120 && s.y < 860) {   // over their head
        s.x = Clamp(s.x, w / 2 + 20, 1580 - w / 2); s.y = Clamp(s.y, bh + 120, 840);
        RRect(s.x - w / 2, s.y - bh - 8, w, bh, bh / 2, Color{ 255, 255, 255, (unsigned char)(242 * a) }, Color{ 226, 232, 242, (unsigned char)(242 * a) }, Color{ 20, 33, 61, (unsigned char)(90 * a) }, 2, true);
        Text(s.x, s.y - 8 - bh / 2, fs, Fade(talker ? Hex(0xd6283c) : Hex(0x14213d), a), talk, 0, TS_DISPLAY | TS_PLAIN);
      } else {   // out of shot: a caption docked under the scoreboard
        std::string who = talker ? "LINE JUDGE" : "UMPIRE"; float tw = Measure(talk, 30, TS_DISPLAY), lw = Measure(who, 16, TS_PLAIN | TS_SPACED) + 30, W2 = lw + tw + 50;
        RRect(800 - W2 / 2, 96, W2, 50, 25, Color{ 255, 255, 255, (unsigned char)(238 * a) }, Color{ 226, 232, 242, (unsigned char)(238 * a) }, BLANK, 0, true);
        RRect(800 - W2 / 2 + 6, 102, lw, 38, 19, Fade(talker ? Hex(0xd6283c) : Hex(0x14213d), a), Fade(talker ? Hex(0xd6283c) : Hex(0x14213d), a), BLANK, 0);
        Text(800 - W2 / 2 + 6 + lw / 2, 121, 16, Fade(WHITE, a), who, 0, TS_PLAIN | TS_SPACED | TS_HEAVY);
        Text(800 - W2 / 2 + lw + 30 + tw / 2, 121, 30, Fade(Hex(0x14213d), a), talk, 0, TS_DISPLAY | TS_PLAIN);
      }
    }
    if (phase == "var" && var.t > 0.9f) {
      float k = EaseOutQ(Clamp((var.t - 0.9f) / 0.35f, 0, 1));
      RRect(800 - 270, 94 - 20 * (1 - k), 540, 62, 31, Color{ 6, 14, 34, (unsigned char)(232 * k) }, Color{ 10, 40, 74, (unsigned char)(232 * k) }, Color{ 92, 225, 255, (unsigned char)(255 * k) }, 3, true);
      Text(570, 125 - 20 * (1 - k), 24, Fade(Hex(0xff3b4f), k * (0.45f + 0.55f * (sinf(time * 8) * 0.5f + 0.5f))), "●", -1, TS_PLAIN);
      Text(816, 125 - 20 * (1 - k), 30, Fade(Hex(0x5ce1ff), k), "HAWK-EYE  ·  LIVE REVIEW", 0, TS_DISPLAY | TS_SHADOW);
      if (var.t > 3.2f) {   // the measurement, pinned to the mark
        Vector2 sp = GetWorldToScreen({ var.at.x, 0.35f, var.at.z }, cam); float a = Clamp((var.t - 3.2f) * 4, 0, 1);
        std::string ms = var.in ? "IN  ·  CLIPS THE LINE" : "OUT  ·  BY " + I((int)roundf(fmaxf(1, var.cm))) + " CM";
        float w = Measure(ms, 26, TS_DISPLAY) + 40; sp.x = Clamp(sp.x, w / 2 + 20, 1580 - w / 2); sp.y = Clamp(sp.y - 70, 200, 760);
        RRect(sp.x - w / 2, sp.y - 26, w, 52, 14, Color{ 6, 14, 34, (unsigned char)(225 * a) }, Color{ 6, 14, 34, (unsigned char)(225 * a) }, Fade(var.in ? Hex(0x51cf66) : Hex(0xff4d6d), a), 3);
        Text(sp.x, sp.y, 26, Fade(WHITE, a), ms, 0, TS_DISPLAY | TS_PLAIN);
      }
      if (var.t > 3.4f) {   // the verdict slams in
        float a = Clamp((var.t - 3.4f) * 5, 0, 1), s = 1 + 1.2f * powf(1 - Clamp((var.t - 3.4f) / 0.25f, 0, 1), 3);
        Text(800, 590, 150 * s, Fade(var.in ? Hex(0x51cf66) : Hex(0xff4d6d), a), var.in ? "IN" : "OUT", 0, TS_DISPLAY | TS_SHADOW | TS_HEAVY);
        Text(800, 680, 32, Fade(WHITE, a), var.in ? "CALL OVERRULED" : "THE CALL STANDS", 0, TS_DISPLAY | TS_SHADOW | TS_SPACED);
      }
    }
    if (callT > 0 && phase != "var") Text(800, 330, 70 + 10 * EaseBack(fminf(1, (2.2f - callT) * 4)), Fade(WHITE, fminf(1, callT * 2)), call, 0, TS_DISPLAY | TS_SHADOW | TS_GOLD);
    if (radarT > 0) { kit::Glass(1330, 110, 240, 60, 20); Text(1450, 140, 30, Hex(0xffe066), radar, 0, TS_DISPLAY | TS_SHADOW); }
    Hint("Flick up on your phone to hit · pick DRIVE, LOB or DROP · you run to the ball by yourself", Hex(0xd8f5a2));
  }
};
}  // namespace

static RegGame t1("tennis", [] { return std::unique_ptr<Game>(new Tennis); });
