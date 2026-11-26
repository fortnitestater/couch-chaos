// SMASH TENNIS in C++: a real-size court in a full stadium, singles or doubles (players rotate in on bigger groups).
// Players run to the ball by themselves — you only SWING (phone motion, the AI camera, or a tap) and tilt to aim.
// A ring on the ground closes on the ball: swing when it's smallest. Ball flies with gravity + bounce, lands in.
#include "classic.h"
#include "kit.h"
#include "mesh.h"
#include "rlgl.h"

namespace {
using kit::I;
constexpr float HL = 11.885f, HW = 4.115f, DW = 5.485f, NETH = 0.914f, SVC = 6.4f, G = 9.81f, GSPD = 0.72f;  // GSPD: game speed (slower than real = easier)

struct Tennis : Classic {
  struct Pl { int pid; int team; Vector3 pos, goal, vel{}; float sw = 0, swDir = 1, cool = 0; };
  std::vector<int> teamQ[2]; std::vector<Pl> on;  // players on court
  Vector3 bp{}, bv{}; bool live = false, bounced = false, smashLive = false; int lastHitTeam = -1, bounces = 0; float spin = 0;
  int pts[2] = { 0, 0 }, games[2] = { 0, 0 }, server = 0, serveTeam = 0, GAMES = 2, ptTeam = -1;
  std::string phase = "intro", call, radar; float t = 2.5f, callT = 0, radarT = 0, hitAt = -1, tossT = -1; int recv = -1; Vector3 hitPt{};
  std::vector<Vector4> crowd; bool fin = false; int rally = 0; std::vector<int> hits;
  std::vector<Vector3> trail; Mesh crowdMesh{}; int camShot = -1; float shotT = 0;
  // presentation: LED ads, officials who talk, ball feel, Hawk-Eye reviews
  Texture2D adTex{}, scrTex{};
  float spinA = 0, spinR = 30, squash = 0, gK = 1, squeakT = 0; Vector3 spinAx{ 1, 0, 0 };
  struct Mark { Vector3 p; float life, yaw; }; std::vector<Mark> marks;
  std::vector<Vector3> path;   // the ball's flight since the last hit (Hawk-Eye replays it)
  struct Var { bool on = false; float t = 0; Vector3 at{}; bool in = false, alongX = true; float cm = 0; int team = 0, stage = 0; std::vector<Vector3> path; } var;
  int sinceVar = 2;
  float talkT = 0; std::string talk; int talker = 0;   // who speaks: 0 the chair umpire, 1 / 2 the near / far line judge
  int judgeOut = -1; float judgeT = 0; bool calledPlay = false;
  ~Tennis() { if (crowdMesh.vertexCount) UnloadMesh(crowdMesh); if (adTex.id) UnloadTexture(adTex); if (scrTex.id) UnloadTexture(scrTex); }

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
    live = false; bounced = false; bounces = 0; trail.clear(); phase = "serve"; smashLive = false; path.clear();
    if (!calledPlay) { calledPlay = true; Say(0, serveTeam ? "Blue to serve. Play!" : "Red to serve. Play!"); }
    else if (frand(0, 1) < 0.3f) Say(0, "Quiet, please."); t = 7; tossT = -1; recv = -1; hitAt = -1; rally = 0;
    for (int i = 0; i < N(); i++) if (!P(i).bot) {
      bool srv = on[server].pid == i; bool playing = false; for (auto& p : on) playing |= p.pid == i;
      Layout(i, playing ? std::string("{\"name\":\"tennis\",\"serve\":") + (srv ? "true" : "false") + "}"
                        : kit::Msg("🎾", "Next up soon!", "You rotate in after this point"));
    }
  }
  // launch the ball from 'from' to land at target, at speed v (m/s), clearing the net
  void Launch(Vector3 from, Vector3 target, float speed, float tmul = 1) {
    Vector3 d = { target.x - from.x, 0, target.z - from.z }; float dist = hypotf(d.x, d.z);
    float T = fmaxf(0.55f, dist / (speed * GSPD)) * tmul, g = G * gK;
    for (int k = 0; k < 12; k++) {  // make sure it clears the net with room to spare
      float vy = (0 - from.y + 0.5f * g * T * T) / T, tn = fabsf(from.z) / fmaxf(0.01f, fabsf(d.z)) * T;
      float hn = from.y + vy * tn - 0.5f * g * tn * tn;
      if (hn > NETH + 0.35f) break; T *= 1.08f;
    }
    bv = { d.x / T, (0 - from.y + 0.5f * g * T * T) / T, d.z / T }; bp = from; live = true; bounced = false; bounces = 0;
    path.clear(); path.push_back(from);
  }
  // where (and when) the receiver should hit: after the bounce, when the ball comes down to ~1 m
  void Predict() {
    Vector3 p = bp, v = bv; bool b = bounced; float tt = 0; const float back = HL + 1.0f;
    for (int k = 0; k < 900; k++) {
      float dt = 1 / 120.f; Vector3 pv = v; v.y -= G * gK * dt; p = Vector3Add(p, Vector3Scale(v, dt)); tt += dt;
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
  // bullet time on the smash: low, close, from the net side of the hitter
  // slow motion, close on the ball: the camera circles it on a tilted ellipse as it flies, then it craters the court
  void SmashShot(const Pl& p) { cine::Shot s; s.kind = cine::SMASH; s.at = bp; s.size = 1.0f; s.c = Hex(0xff922b); s.caption = "SMASH!"; s.sub = radar; s.floorY = 0; s.face = { 0, 0, -Side(p.team) }; cine::Play(s); }
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
    if (why.rfind("SMASH", 0) == 0) return "Smash, what power! "; return "";
  }
  std::string ScoreCall() {   // the server's score first, as an umpire says it
    static const char* NM[] = { "Love", "Fifteen", "Thirty", "Forty" };
    int a = pts[serveTeam], b = pts[1 - serveTeam];
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
  int AiShot() { float r = frand(0, 1); return r < 0.14f ? 1 : r < 0.26f ? 2 : 0; }
  void Hit(int i, float power, float aim, bool good, int shot = 0) {
    Pl& p = on[i]; lastHitTeam = p.team; rally++; if (p.pid >= 0) hits[p.pid]++;
    // harder swings go deeper and closer to the lines: more winners, more close calls (and the odd one out)
    float risk = Clamp((power - 0.55f) / 0.45f, 0, 1), wl = Doubles() ? DW : HW;
    float tz = -Side(p.team) * Lerp(HL - 2.2f - frand(0, 2.5f) * (good ? 0.6f : 1), HL - frand(-0.25f, 0.9f), risk * 0.85f);
    float tx = Clamp(aim, -1, 1) * wl * (0.78f + 0.2f * risk) + frand(-0.4f, 0.4f) * (1 + risk * 0.4f);
    // a SMASH is rare: a high ball, a hard swing and good timing (the AI only sometimes goes for it) — and nobody can return it
    bool ai = p.pid < 0 || P(p.pid).bot;
    bool smash = bp.y > 1.75f && power >= 0.75f && good && (!ai || frand(0, 1) < 0.3f);
    if (smash) { float w = Doubles() ? DW : HW; tz = -Side(p.team) * frand(HL * 0.45f, HL * 0.72f); tx = Clamp(aim, -1, 1) * w * 0.55f; }
    float spd = (smash ? 34 : 20 + power * 13) * (good ? 1 : 0.82f);
    gK = smash ? 1.0f : power > 0.7f ? 1.3f : power < 0.45f ? 0.85f : 1.15f; spinR = smash ? 14 : power > 0.7f ? 48 : power < 0.45f ? -26 : 32;
    float tmul = 1;
    if (!smash && shot == 1) { tz = -Side(p.team) * (HL - 0.7f - frand(0, 1.3f)); spd = 13; tmul = 1.12f; gK = 0.95f; spinR = -18; }       // LOB: high and deep, over a player who has come in
    else if (!smash && shot == 2) { tz = -Side(p.team) * frand(2.4f, 4.2f); spd = 10; gK = 0.8f; spinR = -34; }                               // DROP: short and soft, drags the other player forward
    Launch(bp, { tx, 0, tz }, spd, tmul); smashLive = smash;
    int kmh = (int)(spd * 3.6f * 1.1f); radar = shot == 1 && !smash ? "LOB" : shot == 2 && !smash ? "DROP SHOT" : I(kmh) + " km/h"; radarT = 1.6f;
    Sfx(smash ? "smash" : "pok"); Shake(smash ? 10 : 2); if (!smash) fx.Burst(bp, 12, WHITE, 3, 0.5f, 0.05f, 2);
    if (smash) SmashShot(p); else if (good && power > 0.8f) { call = "💥 POWER SHOT!"; callT = 1; }
    p.sw = 1; p.swDir = bp.x > p.pos.x ? 1.f : -1.f;
    if (rally == 8) Sfx("cheer");
    Predict(); recv = Receiver();
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
      if (d < 4.6f && bp.z * Side(p.team) > -0.5f) { Hit((int)i, power, aimW, d < 2.4f, shot); if (d < 1.3f) { call = "PERFECT!"; callT = 0.8f; } return; }
      if (!smashLive) { qPid = pid; qT = 2.0f; qPow = power; qAim = aimW; qShot = shot; if (callT > 0 && call == "WAIT FOR IT…") callT = 0; return; }   // flick as soon as you see the ball coming: the swing is held and goes the moment it's in reach
      call = "WAIT FOR IT…"; callT = 0.6f;
    }
  }
  float servePow = 0.7f, serveAim = 0;
  void Act(int p, const std::string& k, int, float pw) override {
    if (k.rfind("swing", 0) != 0) return;
    float aim = NAN; int shot = 0; size_t c = k.find(':');
    if (c != std::string::npos) { float a = 0; char sh = 'd'; if (sscanf(k.c_str() + c + 1, "%f|%c", &a, &sh) >= 1) { aim = Clamp(a, -1, 1); shot = sh == 'l' ? 1 : sh == 's' ? 2 : 0; } }
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
      char b[520]; snprintf(b, sizeof b, "{\"t\":\"tn\",\"ph\":\"%s\",\"live\":%d,\"b\":[%.2f,%.2f,%.2f],\"k\":%.2f,\"z\":%d,\"r\":%d,\"sv\":%d,\"pl\":[%s],\"msg\":%s}", phase.c_str(), showBall ? 1 : 0, SX(bp.x), SD(bp.z), bp.y, Clamp((d - 1) / 9, 0, 1), d < 2.4f ? 2 : d < 4.6f ? 1 : 0, rm ? 1 : 0, phase == "serve" && (int)i == server ? 1 : 0, pl.c_str(), kit::Q(call).c_str());
      Send(me.pid, b);
    }
  }
  void Point(int team, const std::string& why) {
    if (phase == "point") return;
    sinceVar++; std::string pre = Prefix(why);
    pts[team]++; ptTeam = team; phase = "point"; t = 2.6f; live = false; call = why; callT = 2.2f; Sfx(why.find("ACE") != std::string::npos ? "cheer" : "applause");
    int a = pts[team], b = pts[1 - team];
    if (a >= 4 && a - b >= 2) {
      games[team]++; pts[0] = pts[1] = 0; call = (team ? TeamName(1) : TeamName(0)) + " WIN THE GAME!"; callT = 2.4f; Sfx("cheer");
      serveTeam = 1 - serveTeam;
      if (games[team] >= GAMES) { phase = "end"; t = 4; conf.Spawn({ 0, 4, Side(team) * 6 }, 160, 5, 6); Sfx("win"); Banner(TeamName(team) + " WIN THE MATCH!", 2600, team ? "#4dabff" : "#ff6b6b");
        WinShot(team); Say(0, "Game, set and match", pre + "Game, set and match.");
        return; }
      Say(0, "Game, " + TeamName(team), pre + "Game.");
    }
    if (pts[0] + pts[1] > 0) { std::string sc = ScoreCall(); Say(0, sc.substr(0, sc.size() - 1), pre + sc); }
    // rotate players in for big groups
    for (int tm = 0; tm < 2; tm++) if ((int)teamQ[tm].size() > (Doubles() ? 2 : 1)) { int f = teamQ[tm][0]; teamQ[tm].erase(teamQ[tm].begin()); teamQ[tm].push_back(f); }
  }
  std::string TeamName(int tm) { if (teamQ[tm].empty()) return "MACHINE"; if (teamQ[tm].size() == 1) return P(teamQ[tm][0]).name; return tm ? "BLUE TEAM" : "RED TEAM"; }
  std::string Score(int tm) { int a = pts[tm], b = pts[1 - tm]; if (a >= 3 && b >= 3) return a == b ? "40" : a > b ? "AD" : "40"; const char* S[] = { "0", "15", "30", "40" }; return S[std::min(3, a)]; }
  void Update(float dt) override {
    time += dt; fx.Update(dt); conf.Update(dt); callT -= dt; radarT -= dt; t -= dt;
    tnT -= dt; if (tnT <= 0) { tnT = 0.066f; SendTn(); }
    talkT -= dt; judgeT -= dt; squeakT -= dt; spinA += spinR * dt; squash = fmaxf(0, squash - dt * 9);
    for (auto& m : marks) m.life -= dt;
    for (int i = 0; i < N(); i++) if (!P(i).bot && P(i).a && !P(i).pa) Swing(i, 0.65f);
    for (auto& p : on) { p.sw = fmaxf(0, p.sw - dt * 3); p.cool -= dt; }
    if (phase == "intro") { if (t <= 0) phase = "serve"; return; }
    if (phase == "end") { if (t <= 0 && !fin) { fin = true; Sfx("crowdOff"); std::vector<int> o; int w = games[0] > games[1] ? 0 : 1; for (int i : teamQ[w]) o.push_back(i); for (int i : teamQ[1 - w]) o.push_back(i);
        std::stable_sort(o.begin(), o.begin() + teamQ[w].size(), [&](int a, int b) { return hits[a] > hits[b]; });
        FinishOrder(o, [&](int i) { int tm = i % 2; return std::string(tm == w ? "won " : "lost ") + I(games[tm]) + "-" + I(games[1 - tm]) + " · " + I(hits[i]) + " hits"; }); } return; }
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
      Pl& p = on[i]; bool netMan = Doubles() && fabsf(p.pos.z) < 6 && (int)i != recv;
      if (phase == "rally" && (int)i == recv) p.goal = { Clamp(hitPt.x - HandSide(p.team), -DW - 1.2f, DW + 1.2f), 0, Clamp(hitPt.z, -HL - 1.5f, HL + 1.5f) };
      else if (phase == "rally") { float bx2 = live ? bp.x : 0; p.goal = { Clamp(bx2 * 0.35f + (netMan ? (p.goal.x > 0 ? 1.2f : -1.2f) : 0), -3, 3), 0, Side(p.team) * (netMan ? 3.5f : HL + 0.4f) }; }
      Vector3 d = Vector3Subtract(p.goal, p.pos); d.y = 0; float l = Vector3Length(d);
      Vector3 want = l > 0.02f ? Vector3Scale(d, fminf(7.0f, l * 4.0f) / l) : Vector3{ 0, 0, 0 };
      Vector3 dv = Vector3Subtract(want, p.vel); float dl = Vector3Length(dv), acc = 28 * dt;
      if (dl > acc) dv = Vector3Scale(dv, acc / dl);
      if (dl > 5.5f && Vector3Length(p.vel) > 1.8f && squeakT <= 0) { Sfx("squeak"); squeakT = 0.4f; }   // shoes on the hard court
      p.vel = Vector3Add(p.vel, dv); p.pos = Vector3Add(p.pos, Vector3Scale(p.vel, dt));
    }
    if (phase == "serve") {
      Pl& s = on[server];
      if (tossT < 0) { if ((s.pid < 0 || P(s.pid).bot || g_dbg == 21 || g_dbg == 22) && t < 5.6f) { tossT = 0; servePow = frand(0.6f, 1); serveAim = frand(-0.7f, 0.7f); s.sw = 0; } if (t <= 0) { tossT = 0; servePow = 0.6f; serveAim = 0; } return; }
      tossT += dt; bp = { s.pos.x + 0.3f, 1.2f + 6.0f * tossT - 0.5f * G * tossT * tossT * 1.0f, s.pos.z }; live = false;
      if (tossT >= 0.55f) {  // hit at the top of the toss
        float sx = s.pos.x > 0 ? -1.f : 1.f;
        Vector3 target = { sx * frand(0.6f, HW - 0.5f) * 0.9f + serveAim * 0.6f, 0, -Side(s.team) * frand(SVC - 2.2f, SVC - 0.5f) };
        target.x = Clamp(target.x, -HW + 0.3f, HW - 0.3f); if (target.x * sx < 0.2f) target.x = sx * 0.6f;
        bp.y = 2.7f; lastHitTeam = s.team; float spd = 20 + servePow * 11; gK = 1.1f; spinR = 24; Launch(bp, target, spd);
        radar = I((int)(spd * 3.6f * 1.15f)) + " km/h"; radarT = 1.6f; Sfx("smash"); s.sw = 1; phase = "rally"; Predict(); recv = Receiver();
      }
      return;
    }
    if (phase == "rally" && live) {
      int steps = 4; float h = dt / steps;
      for (int k = 0; k < steps && live; k++) {
        float zb = bp.z; bv.y -= G * gK * h; bp = Vector3Add(bp, Vector3Scale(bv, h));
        path.push_back(bp); if (path.size() > 900) path.erase(path.begin(), path.begin() + 300);
        if ((zb < 0) != (bp.z < 0) && bp.y < NETH) { live = false; Sfx("netHit"); Point(1 - lastHitTeam, "NET!"); return; }
        if (bp.y < 0.033f && bv.y < 0) {
          float my = atan2f(bv.x, bv.z);
          bp.y = 0.033f; bv.y = -bv.y * 0.72f; bv.x *= 0.88f; bv.z *= 0.88f; bounces++; Sfx("bounce");
          squash = 1; marks.push_back({ bp, 3.0f, my }); if (marks.size() > 12) marks.erase(marks.begin());
          if (smashLive && bounces == 1) { cine::Impact(bp); Sfx("boom"); Shake(12); }
          float w = Doubles() ? DW : HW; bool onRecvSide = bp.z * Side(lastHitTeam) < 0;
          if (bounces == 1 && !smashLive && onRecvSide) {   // close to a line: the line judge calls it, Hawk-Eye decides
            float ox = fabsf(bp.x) - (w + 0.05f), oz = fabsf(bp.z) - (HL + 0.05f), m = fmaxf(ox, oz); bool alongX = oz > ox;
            if (m > 0 && m < 0.16f && sinceVar >= 2) { StartVar(false, m, alongX); return; }
            if (m <= 0 && m > -0.07f && sinceVar >= 2 && frand(0, 1) < 0.45f) { StartVar(true, -m, alongX); return; }
          }
          if (bounces == 1 && (!onRecvSide || fabsf(bp.x) > w + 0.05f || fabsf(bp.z) > HL + 0.05f)) { live = false; Point(1 - lastHitTeam, "OUT!"); return; }
          if (bounces >= 2) { live = false; Point(lastHitTeam, smashLive ? "SMASH WINNER!" : rally <= 1 ? "ACE! 🎾" : "WINNER!"); return; }
        }
      }
      trail.push_back(bp); if (trail.size() > 16) trail.erase(trail.begin());
      if (qT > 0) {
        qT -= dt;
        if (recv >= 0 && on[recv].pid == qPid) { Vector3 rk = RacketPt(on[recv]); float d = Vector3Distance(rk, bp); if (d < 3.0f && bp.z * Side(on[recv].team) > -0.5f) { qT = 0; Hit(recv, qPow, qAim, true, qShot); } }
        else qT = 0;
      }
      // the AI (bots + the practice machine) hits on time, most of the time
      if (recv >= 0) { Pl& r = on[recv]; bool ai = r.pid < 0 || P(r.pid).bot || g_dbg == 21 || g_dbg == 22; /* 21 = debug autoplay */ Vector3 rk = RacketPt(r);
        if (ai && !smashLive && Vector3Distance(rk, bp) < 1.9f && bounces == 1) { if (frand(0, 1) < (r.pid < 0 ? 0.9f : 0.8f)) Hit(recv, frand(0.45f, 0.9f), frand(-0.8f, 0.8f), true, AiShot()); else { recv = -1; } } }
      if (fabsf(bp.z) > HL + 9 || fabsf(bp.x) > 14) { live = false; Point(bounces >= 1 ? lastHitTeam : 1 - lastHitTeam, bounces >= 1 ? (smashLive ? "SMASH WINNER!" : "WINNER!") : "OUT!"); }
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
    Vector3 wp = { bx + 0.6f * sinf(time * 0.12f), 7.4f + 0.3f * sinf(time * 0.17f), -25.5f }, wt = { bx * 0.6f, 0.9f, 1.8f }; float wf = 42, k = 2.6f;
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
    // players
    // real footwork (run, side-step, backpedal), a swing that blends over whatever the legs are doing,
    // eyes on the ball, and the racket in the right hand
    for (size_t pi = 0; pi < on.size(); pi++) {
      Pl& p = on[pi];
      Color c = p.pid >= 0 ? P(p.pid).color : Hex(0xadb5bd);
      float yaw = p.team == 0 ? 0 : PI;
      Vector3 fw = { sinf(yaw), 0, cosf(yaw) }, rt = { -cosf(yaw), 0, sinf(yaw) };
      float spd = Vector3Length(p.vel), vf = Vector3DotProduct(p.vel, fw), vr = Vector3DotProduct(p.vel, rt);
      Vector3 look = live ? bp : Vector3{ 0, 1, 0 };
      float la = angDiff(yaw, atan2f(look.x - p.pos.x, look.z - p.pos.z)), body = Clamp(la, -0.7f, 0.7f) * 0.35f;
      PersonPose q; q.pos = p.pos; q.c = c; q.hsh = PersonHash(c); q.height = 1.8f; q.yaw = yaw + body; q.headYaw = Clamp(la - body, -1.1f, 1.1f);
      q.headPitch = live ? Clamp(-atan2f(bp.y - 1.6f, fmaxf(1.f, hypotf(bp.x - p.pos.x, bp.z - p.pos.z))), -0.5f, 0.4f) : 0;
      if (spd > 0.6f) { q.clip = fabsf(vr) > fabsf(vf) * 0.8f ? (vr > 0 ? "Run_Right" : "Run_Left") : vf < -0.3f ? "Run_Back" : "Run"; q.t = time * (0.55f + spd * 0.12f) + p.pid; }
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
    fx.Draw(); conf.Draw();
    EndMode3D();
    // scoreboard
    kit::Glass(520, 14, 560, 70, 22);
    Text(560, 37, 22, Hex(0xff8787), TeamName(0), -1, TS_HEAVY | TS_PLAIN); Text(560, 64, 22, Hex(0x74c0fc), TeamName(1), -1, TS_HEAVY | TS_PLAIN);
    Text(930, 37, 22, Hex(0xffe066), I(games[0]), 0, TS_DISPLAY); Text(930, 64, 22, Hex(0xffe066), I(games[1]), 0, TS_DISPLAY);
    Text(1010, 37, 22, WHITE, Score(0), 0, TS_DISPLAY); Text(1010, 64, 22, WHITE, Score(1), 0, TS_DISPLAY);
    Text(930, 20, 11, Hex(0xc9d3f2), "GAMES", 0, TS_PLAIN | TS_SPACED); Text(1010, 20, 11, Hex(0xc9d3f2), "PTS", 0, TS_PLAIN | TS_SPACED);
    for (auto& p : on) if (p.pid >= 0) { Vector2 s = GetWorldToScreen({ p.pos.x, 2.2f, p.pos.z }, cam); Text(s.x, s.y, 18, P(p.pid).color, P(p.pid).name, 0, TS_HEAVY); }
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
