// CAGE FIGHT: a mixed-martial-arts fight in a packed arena. Two fighters at a time, each on a phone: move round the cage, throw a jab, a cross, a body shot,
// a low kick or a head kick, block, dodge. Hits do real damage (health, and separate damage to the head, body and legs), cut and bleed, rock a fighter,
// knock him down (he has to tap to get up before the count of ten) or finish him. Two rounds, then the judges. Everybody else queues for the next fight.
#include "classic.h"
#include "kit.h"
#include "mesh.h"
#include "rlgl.h"

namespace {
using kit::I;
constexpr float APO = 4.0f, RC = APO / 0.92388f;   // the cage: distance to a side / to a corner (an octagon)

// the strikes: windup / contact / recovery (seconds), damage, stamina cost, reach (m), stun on the victim, push, where it lands (0 head, 1 body, 2 legs)
struct Atk { const char* name; const char* clip; float wind, act, rec, dmg, sta, reach, stun, push; int zone; bool heavy; };
const Atk ATK[5] = {
  { "JAB",       "Punch_Left",  0.10f, 0.07f, 0.17f,  5.f,  4.f, 1.60f, 0.22f, 0.12f, 0, false },
  { "CROSS",     "Punch_Right", 0.19f, 0.08f, 0.30f, 11.f,  9.f, 1.70f, 0.45f, 0.42f, 0, true  },
  { "BODY SHOT", "Punch_Right", 0.18f, 0.08f, 0.28f,  9.f,  8.f, 1.60f, 0.38f, 0.30f, 1, false },
  { "LOW KICK",  "Kick_Left",   0.20f, 0.08f, 0.30f,  7.f,  7.f, 1.90f, 0.32f, 0.20f, 2, false },
  { "HEAD KICK", "Kick_Right",  0.32f, 0.10f, 0.48f, 17.f, 15.f, 2.00f, 0.70f, 0.80f, 0, true  },
};

struct Fi {
  int pid = -1; bool bot = false; Color col = WHITE; std::string name;
  Vector3 pos{}, vel{}; float yaw = 0;
  float hp = 100, sta = 100, dH = 0, dB = 0, dL = 0;   // health, stamina, and the damage the head / body / legs have taken (0..100)
  Vector2 mv{}; bool blk = false, dodgeReq = false; int want = -1; float wantT = 0;   // what the phone is asking for
  enum St { IDLE, ATTACK, HIT, DODGE, ROCKED, DOWN, GETUP, KO } st = IDLE;
  int atk = -1, dodgeDir = 0; float t = 0, stun = 0, stunMax = 1; bool landed = false, hitHeavy = false;
  float inv = 0, dodgeCd = 0, comboT = 0, counterT = 0; int combo = 0;
  float downT = 0, tap = 0, tapNeed = 8; int knockdowns = 0;
  float bleed = 0, dripT = 0, flash = 0, blkFlash = 0, stride = 0, gassedT = 0;
  float dealt = 0, dealtRound = 0; int kds = 0;
  std::string call; float callT = 0; Color callC = WHITE;
  // the bot's head
  float think = 0, aggro = 0.6f, react = 0.45f, blkHold = 0, circ = 1, circT = 0;
  Matrix headM = MatrixIdentity();
};

struct Fight : Classic {
  Fi f[2]; std::vector<int> q; int bout = 0, BOUTS = 2, rd = 1, ROUNDS = 2; std::vector<int> pts, wins, kos;
  std::string phase = "intro", method, verdict; float t = 2.8f, clock = 0, bellT = 0, restT = 0, hitstop = 0, sendT = 0, flashA = 0; int winner = -1;
  const float RT = 55.f; Vector3 camDir{ 0, 0, 1 }; float roar = 0;   // roar: the crowd jumps when something big happens
  struct Pop { Vector3 p; std::string s; Color c; float t; float big; }; std::vector<Pop> pops;
  struct Blood { Vector3 p; float r, a; }; std::vector<Blood> pools;
  Mesh octa{}; Mesh crowdMesh{}; std::vector<Vector4> crowd; int judgeA = 0, judgeB = 0;
  ~Fight() { if (octa.vertexCount) UnloadMesh(octa); if (crowdMesh.vertexCount) UnloadMesh(crowdMesh); }

  void Start() override {
    InitCam(42);
    for (int s = 0; s < 6; s++) for (int k = 0; k < 64; k++) { float a = (k + (s & 1) * 0.5f) * 2 * PI / 64, r = 9.2f + s * 1.35f; crowd.push_back({ cosf(a) * r, 1.0f + s * 1.0f, sinf(a) * r, atan2f(-cosf(a), -sinf(a)) }); }
    pts.assign(N(), 0); wins.assign(N(), 0); kos.assign(N(), 0);
    std::vector<int> hum, bots; for (int i = 0; i < N(); i++) (P(i).bot ? bots : hum).push_back(i);
    kit::Shuffle(hum); kit::Shuffle(bots); for (int i : hum) q.push_back(i); for (int i : bots) q.push_back(i);
    BOUTS = N() <= 2 ? 2 : N() <= 4 ? 3 : 4;
    Layout(-1, kit::Msg("🥊", "Cage Fight", "Get ready…"));
    Sfx("crowdOn");
  }
  // ----------------------------------------------------------------------------------------------- a fight
  void Reset(Fi& x, int pid, float side) {
    Fi n; n.pid = pid; n.bot = P(pid).bot; n.col = P(pid).color; n.name = P(pid).name; n.pos = { side * 1.5f, 0.12f, 0 }; n.yaw = side < 0 ? PI / 2 : -PI / 2;
    n.aggro = frand(0.45f, 0.8f); n.react = frand(0.35f, 0.55f); x = n;
  }
  void StartBout() {
    bout++; if (bout > BOUTS) { Finish2(); return; }
    int a = q[0], b = q[1]; Reset(f[0], a, -1); Reset(f[1], b, 1); rd = 1; method = ""; verdict = ""; winner = -1; pools.clear(); pops.clear();
    BeginRound(true);
    for (int i = 0; i < N(); i++) if (!P(i).bot) {
      if (i == a || i == b) Layout(i, "{\"name\":\"fight\"}");
      else Layout(i, kit::Msg("🥊", P(a).name + " vs " + P(b).name, "Cheer them on — you are next in the queue!"));
    }
    Say("In the red corner, " + P(a).name + "! In the blue corner, " + P(b).name + "!");
  }
  void BZ(int pid, int ms) { if (pid >= 0 && pid < N() && !P(pid).bot) Buzz(pid, ms); }
  void Say(const std::string& s) { Sfx(("say:" + s).c_str()); }
  void BeginRound(bool first) {
    phase = "bell"; bellT = 3.0f; clock = RT;
    for (auto& x : f) { x.st = Fi::IDLE; x.stun = 0; x.inv = 0; x.dealtRound = 0; x.mv = {}; x.blk = false; x.want = -1; x.atk = -1; x.vel = {};
      x.pos = { (&x == &f[0] ? -1.5f : 1.5f), 0.12f, 0 }; x.callT = 0; }
    Banner(std::string("ROUND ") + I(rd), 1600, "#ffd43b"); Sfx(first ? "crowdOn" : "ding");
  }
  float Dist() { return hypotf(f[1].pos.x - f[0].pos.x, f[1].pos.z - f[0].pos.z); }
  Vector3 Fwd(const Fi& a, const Fi& b) { Vector3 d = { b.pos.x - a.pos.x, 0, b.pos.z - a.pos.z }; float l = Vector3Length(d); return l > 0.01f ? Vector3Scale(d, 1 / l) : Vector3{ sinf(a.yaw), 0, cosf(a.yaw) }; }
  void Call(Fi& x, const std::string& s, Color c, float t = 1.1f) { x.call = s; x.callC = c; x.callT = t; }
  void PopText(Vector3 at, const std::string& s, Color c, float big = 1) { pops.push_back({ at, s, c, 1.0f, big }); }
  Vector3 HeadPos(const Fi& x) { return { x.pos.x, 1.55f, x.pos.z }; }

  // ----------------------------------------------------------------------------------------------- damage
  void Resolve(Fi& a, Fi& d, const Atk& A) {
    Vector3 fw = Fwd(a, d); float dist = Dist();
    Vector3 hit = { (a.pos.x + d.pos.x) / 2, A.zone == 0 ? 1.5f : A.zone == 1 ? 1.1f : 0.5f, (a.pos.z + d.pos.z) / 2 };
    if (dist > A.reach + 0.15f) { Sfx("fSwing"); return; }                                 // a miss
    if (d.inv > 0) { Call(d, "DODGED!", Hex(0x74c0fc), 0.8f); Call(a, "MISSED!", Hex(0xced4da), 0.7f); d.counterT = 0.9f; a.t += 0.22f; PopText({ d.pos.x, 1.9f, d.pos.z }, "DODGE!", Hex(0x74c0fc)); Sfx("fSwing"); return; }
    if (d.st == Fi::DOWN || d.st == Fi::KO || d.st == Fi::GETUP) return;
    float dmg = 0.72f * A.dmg * (0.6f + 0.4f * a.sta / 100.f);                          // a tired fighter hits softer
    if (a.comboT > 0) { a.combo++; dmg *= 1 + 0.1f * fminf(4, (float)a.combo); } else a.combo = 1; a.comboT = 0.75f;
    bool counter = (d.st == Fi::ATTACK && d.t < ATK[d.atk].wind) || a.counterT > 0; if (counter) { dmg *= 1.5f; a.counterT = 0; }
    bool blocked = d.blk && d.st == Fi::IDLE && A.zone != 2;                               // a raised guard stops punches and high kicks, never a low kick
    if (blocked) {
      dmg *= 0.2f; d.sta -= A.dmg * 1.1f; d.blkFlash = 0.2f; d.hp -= dmg * 0.5f; Sfx("fBlock"); fx.Burst(hit, 8, Hex(0xffe066), 3, 0.3f, 0.04f); Call(d, "BLOCKED", Hex(0xffe066), 0.7f);
      d.vel = Vector3Add(d.vel, Vector3Scale(fw, A.push * 3)); if (d.sta <= 0) { d.sta = 0; d.st = Fi::HIT; d.t = 0; d.stun = d.stunMax = 0.9f; d.hitHeavy = true; Call(d, "GUARD BROKEN!", Hex(0xff922b), 1.2f); BZ(d.pid, 200); }
      return;
    }
    float hpK = A.zone == 0 ? 1.f : A.zone == 1 ? 0.85f : 0.75f; d.hp -= dmg * hpK; a.dealt += dmg; a.dealtRound += dmg;
    if (A.zone == 0) d.dH = fminf(100.f, d.dH + dmg * (A.heavy ? 1.7f : 1.15f));
    else if (A.zone == 1) { d.dB = fminf(100.f, d.dB + dmg * 1.4f); d.sta = fmaxf(0.f, d.sta - dmg * 1.1f); }
    else d.dL = fminf(100.f, d.dL + dmg * 1.5f);
    d.flash = 0.12f; d.vel = Vector3Add(d.vel, Vector3Scale(fw, A.push * 5)); d.pos = Vector3Add(d.pos, Vector3Scale(fw, A.push * 0.5f));
    hitstop = A.heavy || counter ? 0.10f : 0.05f; Shake(A.heavy ? 8.f : 3.f); roar = fmaxf(roar, A.heavy ? 0.7f : 0.3f);
    static const char* HS[5] = { "fJab", "fCross", "fBody", "fLow", "fHead" }; Sfx(HS[&A - ATK]); BZ(d.pid, A.heavy ? 90 : 40); BZ(a.pid, 25);
    fx.Burst(hit, 10 + (int)dmg, Hex(0xfff3bf), 4, 0.25f, 0.04f);                          // sparks of the impact
    PopText({ d.pos.x, 2.0f, d.pos.z }, I((int)ceilf(dmg)), counter ? Hex(0xff6b6b) : Hex(0xffffff), counter || A.heavy ? 1.4f : 1.f);
    if (counter) { Call(a, "COUNTER!", Hex(0xff6b6b), 1.1f); Sfx("ooh"); }
    else if (a.combo >= 2) { Call(a, std::string("COMBO x") + I(a.combo), Hex(0xffd43b), 1.0f); }
    // blood: a cut over the eye, a split lip: the harder and the more often the head is hit, the more there is
    if (A.zone == 0 && (A.heavy || d.dH > 28 || counter)) { fx.Burst(HeadPos(d), 8 + (int)(dmg * 1.6f), Hex(0x9e1019), 3.6f, 0.8f, 0.028f, 7); pools.push_back({ { d.pos.x + frand(-0.5f, 0.5f), 0.123f, d.pos.z + frand(-0.5f, 0.5f) }, frand(0.08f, 0.2f), 0.85f }); }
    if (A.zone != 0 && A.heavy) fx.Burst(hit, 6, Hex(0x9e1019), 3.f, 0.6f, 0.026f, 7);
    if (d.dH > 38) d.bleed = fmaxf(d.bleed, fminf(1.f, d.dH / 80.f));
    // what it does to him
    bool rocked = d.st == Fi::ROCKED, down = false;
    if (d.hp <= 0) { d.hp = 0; Ko(a, d, "KO"); return; }
    if (A.heavy && A.zone == 0 && (rocked || d.dH >= 85 || (A.wind > 0.3f && d.dH >= 65))) down = true;   // a clean head shot on a fighter who is already rocked, or on a battered head: down he goes
    if (down) { Knockdown(a, d); return; }
    if (A.heavy && A.zone == 0 && d.dH >= 50 && !rocked) { d.st = Fi::ROCKED; d.t = 0; d.stun = d.stunMax = 1.7f; d.atk = -1; Call(d, "ROCKED!", Hex(0xff922b), 1.5f); PopText({ d.pos.x, 2.3f, d.pos.z }, "ROCKED!", Hex(0xff922b), 1.6f); Sfx("ooh"); BZ(d.pid, 250); return; }
    if (!rocked) { d.st = Fi::HIT; d.t = 0; d.stun = d.stunMax = A.stun * (counter ? 1.4f : 1.f); d.hitHeavy = A.heavy; d.atk = -1; }
  }
  void Knockdown(Fi& a, Fi& d) {
    d.st = Fi::DOWN; d.downT = 0; d.tap = 0; d.tapNeed = 7 + 2.f * d.knockdowns + d.dH / 28.f; d.knockdowns++; a.kds++; d.atk = -1; d.blk = false; d.vel = {};
    hitstop = 0.14f; Shake(14); roar = 1.f; Sfx("fKO"); Sfx("ooh"); Sfx("cheer"); BZ(d.pid, 400);
    fx.Burst(HeadPos(d), 30, Hex(0x9e1019), 4.5f, 1.0f, 0.034f, 8);
    Banner("KNOCKDOWN!", 1500, kit::Hx(a.col).c_str());
    cine::Moment(cine::SMASH, { d.pos.x, 1.0f, d.pos.z }, 1.2f, a.col, "KNOCKDOWN!", d.name, 0.12f);
    Say("Down! " + d.name + " is down!");
  }
  void Ko(Fi& a, Fi& d, const char* how) {
    d.st = Fi::KO; d.downT = 0; d.vel = {}; d.blk = false; d.atk = -1; hitstop = 0.18f; Shake(18); roar = 1.5f; Sfx("fKO"); Sfx("cheer");
    fx.Burst(HeadPos(d), 40, Hex(0x9e1019), 4.5f, 1.1f, 0.034f, 8); pools.push_back({ { d.pos.x, 0.123f, d.pos.z }, 0.35f, 0.9f });
    method = how; winner = &a == &f[0] ? 0 : 1; kos[a.pid]++;
    cine::Moment(cine::SMASH, { d.pos.x, 1.0f, d.pos.z }, 1.4f, a.col, std::string(how) + "!", a.name + " wins", 0.12f);
    EndBout(winner, how);
  }
  void EndBout(int w, const std::string& how) {
    phase = "result"; t = 7.5f; winner = w; method = how; conf.Spawn({ 0, 4, 0 }, 120, 5, 6); Sfx("win");
    for (auto& x : f) { x.blk = false; x.mv = {}; x.want = -1; }
    Fi* W = w >= 0 ? &f[w] : nullptr; Fi* L = w >= 0 ? &f[1 - w] : nullptr;
    if (W) { bool fin = how == "KO" || how == "TKO"; pts[W->pid] += fin ? 100 : 70; wins[W->pid]++; pts[L->pid] += 20 + (int)(L->dealt / 4); verdict = W->name + " WINS BY " + how + "!"; }
    else { for (auto& x : f) pts[x.pid] += 40 + (int)(x.dealt / 4); verdict = "A DRAW!"; }
    // the next fight: the loser goes to the back of the queue, the winner stays on (a draw: the one who fought longest steps out)
    int a = f[0].pid, b = f[1].pid; q.erase(std::remove(q.begin(), q.end(), a), q.end()); q.erase(std::remove(q.begin(), q.end(), b), q.end());
    int stay = W ? W->pid : a, out = W ? L->pid : b; q.insert(q.begin(), stay); q.push_back(out);
    for (int i = 0; i < N(); i++) if (!P(i).bot) Layout(i, kit::Msg("🥊", verdict, "Next fight coming up…"));
  }
  void EndRound() {
    float a = f[0].dealtRound + f[0].kds * 6.f, b = f[1].dealtRound + f[1].kds * 6.f;   // the judges score the damage and the knockdowns
    int s0 = a > b + 0.5f ? 10 : 9, s1 = b > a + 0.5f ? 10 : 9; if (fabsf(a - b) <= 0.5f) s0 = s1 = 10; if (a > b + 25) s1 = 8; if (b > a + 25) s0 = 8;
    judgeA += s0; judgeB += s1;
    if (rd >= ROUNDS) {   // the decision
      Sfx("ding"); float da = f[0].dealt + f[0].kds * 6.f, db = f[1].dealt + f[1].kds * 6.f; int w = da > db + 1 ? 0 : db > da + 1 ? 1 : (f[0].hp > f[1].hp + 2 ? 0 : f[1].hp > f[0].hp + 2 ? 1 : -1);
      EndBout(w, "DECISION"); return;
    }
    phase = "rest"; restT = 5.0f; Sfx("ding"); Banner(std::string("END OF ROUND ") + I(rd), 1600, "#ffd43b");
    for (auto& x : f) { x.sta = fminf(100.f, x.sta + 45); x.hp = fminf(100.f, x.hp + (100 - x.hp) * 0.2f); x.dH *= 0.8f; x.bleed *= 0.5f; x.blk = false; x.mv = {}; x.st = Fi::IDLE; x.atk = -1; x.stun = 0; }   // the corner patches him up
  }
  // ----------------------------------------------------------------------------------------------- phones and bots
  Fi* Mine(int pid) { for (auto& x : f) if (x.pid == pid) return &x; return nullptr; }
  void Act(int pid, const std::string& k, int, float) override {
    Fi* x = Mine(pid); if (!x || x->bot) return;
    if (k.rfind("mv", 0) == 0) { float a = 0, b = 0; if (sscanf(k.c_str() + 3, "%f|%f", &a, &b) == 2) x->mv = { Clamp(a, -1.f, 1.f), Clamp(b, -1.f, 1.f) }; }
    else if (k.rfind("atk", 0) == 0 && k.size() > 4) { char c = k[4]; int i = c == 'j' ? 0 : c == 'c' ? 1 : c == 'b' ? 2 : c == 'l' ? 3 : c == 'h' ? 4 : -1; if (i >= 0) { x->want = i; x->wantT = 0.3f; } }
    else if (k.rfind("blk", 0) == 0) x->blk = k.size() > 4 && k[4] == '1';
    else if (k == "dg") x->dodgeReq = true;
    else if (k == "up") { if (x->st == Fi::DOWN) x->tap += 1; }
  }
  void Think(Fi& a, Fi& b, float dt) {
    a.think -= dt; if (a.think > 0) return; a.think = frand(0.07f, 0.18f);
    float D = Dist(); a.mv = { 0, 0 };
    if (a.blkHold > 0) { a.blkHold -= 0.12f; if (a.blkHold <= 0) a.blk = false; }
    bool threat = b.st == Fi::ATTACK && b.t < ATK[b.atk].wind + 0.06f && D < ATK[b.atk].reach + 0.45f;
    if (threat && a.st == Fi::IDLE && frand(0, 1) < a.react) {
      if (frand(0, 1) < 0.6f && ATK[b.atk].zone != 2) { a.blk = true; a.blkHold = 0.5f; } else if (a.sta > 15 && a.dodgeCd <= 0) a.dodgeReq = true;
      return;
    }
    if (a.st != Fi::IDLE) return;
    a.circT -= 0.12f; if (a.circT <= 0) { a.circ = frand(0, 1) < 0.5f ? 1.f : -1.f; a.circT = frand(0.8f, 2.2f); }
    bool tired = a.sta < 22 || a.hp < 22 && frand(0, 1) < 0.5f;
    if (D > 2.2f) { a.mv = { a.circ * 0.4f, 1 }; a.blk = false; }
    else if (D < 1.05f) a.mv = { a.circ * 0.5f, -0.7f };
    else a.mv = { a.circ * 0.6f, D > 1.7f ? 0.4f : 0.f };
    if (tired && D < 2.4f) { a.mv = { a.circ * 0.5f, -0.6f }; if (a.blkHold <= 0 && frand(0, 1) < 0.4f) { a.blk = true; a.blkHold = 0.6f; } return; }
    if (D < 2.05f && a.blkHold <= 0 && frand(0, 1) < 0.3f + 0.5f * a.aggro) {
      float r = frand(0, 1); int pick;
      if (b.st == Fi::ROCKED || b.st == Fi::HIT) pick = r < 0.45f ? 1 : r < 0.75f ? 4 : 0;           // finish a hurt fighter
      else pick = r < 0.34f ? 0 : r < 0.5f ? 1 : r < 0.62f ? 2 : r < 0.84f ? 3 : 4;
      if (D > ATK[pick].reach) pick = D > 1.7f && D <= ATK[3].reach ? 3 : 0;
      if (a.sta < ATK[pick].sta) pick = 0;
      a.want = pick; a.wantT = 0.3f; a.blk = false;
    }
  }
  // ----------------------------------------------------------------------------------------------- the update
  void UpdateFighter(int i, float dt, bool live) {
    Fi& a = f[i]; Fi& b = f[1 - i];
    a.callT -= dt; a.inv -= dt; a.dodgeCd -= dt; a.comboT -= dt; a.counterT -= dt; a.flash -= dt; a.blkFlash -= dt; a.wantT -= dt; if (a.wantT <= 0) a.want = -1;
    if (a.bot && live && a.st != Fi::DOWN && a.st != Fi::KO && b.st != Fi::KO) Think(a, b, dt); else if (a.bot && !live) { a.mv = {}; a.blk = false; }
    Vector3 fw = Fwd(a, b), rt = { -fw.z, 0, fw.x };
    float want = atan2f(fw.x, fw.z);
    bool canTurn = a.st != Fi::DOWN && a.st != Fi::KO && a.st != Fi::GETUP;
    if (canTurn) a.yaw += Clamp(angDiff(a.yaw, want), -11.f * dt, 11.f * dt);
    float legK = 1.f - a.dL / 160.f, staK = a.sta < 20 ? 0.75f : 1.f;
    // stamina: it comes back when you stand still, slower when your body has taken punishment
    float regen = (a.st == Fi::IDLE ? (Vector2Length(a.mv) > 0.3f ? 8.f : 14.f) : a.st == Fi::ROCKED ? 3.f : 2.f) * (1.f - a.dB / 170.f); if (a.blk && a.st == Fi::IDLE) regen = 4.f;
    a.sta = Clamp(a.sta + regen * dt, 0.f, 100.f);
    if (a.sta < 15) { a.gassedT += dt; } else a.gassedT = 0;
    if (a.bleed > 0.05f) { a.dripT -= dt; if (a.dripT <= 0) { a.dripT = 0.35f / a.bleed; Vector3 h = HeadPos(a); fx.Burst({ h.x, h.y - 0.1f, h.z }, 2, Hex(0x9e1019), 0.8f, 0.7f, 0.022f, 9); if (frand(0, 1) < 0.4f) pools.push_back({ { a.pos.x + frand(-0.3f, 0.3f), 0.123f, a.pos.z + frand(-0.3f, 0.3f) }, frand(0.04f, 0.09f), 0.8f }); } }
    Vector3 target = { 0, 0, 0 }; float accel = 18;
    switch (a.st) {
      case Fi::IDLE: {
        if (live) {
          bool blocking = a.blk; float sp = (blocking ? 0.45f : 1.f) * legK * staK;
          target = Vector3Add(Vector3Scale(fw, a.mv.y * (a.mv.y > 0 ? 2.3f : 1.7f) * sp), Vector3Scale(rt, a.mv.x * 2.0f * sp));
          if (a.want >= 0) { int k = a.want; a.want = -1; a.blk = false; a.st = Fi::ATTACK; a.atk = k; a.t = 0; a.landed = false; float cost = ATK[k].sta; if (a.sta < cost) { a.sta = 0; Call(a, "GASSED!", Hex(0xadb5bd), 0.9f); } else a.sta -= cost; Sfx("fSwing"); }
          else if (a.dodgeReq && a.dodgeCd <= 0 && a.sta >= 10) { a.st = Fi::DODGE; a.t = 0; a.inv = 0.3f; a.dodgeCd = 0.7f; a.sta -= 12; a.dodgeDir = a.mv.x > 0.2f ? 1 : a.mv.x < -0.2f ? -1 : 0; Sfx("whoosh"); }
        }
        a.dodgeReq = false; break; }
      case Fi::ATTACK: {
        const Atk& A = ATK[a.atk]; float ds = 1.f + (1.f - a.sta / 100.f) * 0.35f; a.t += dt; float u = a.t / ds;
        target = Vector3Scale(fw, (u < A.wind + A.act ? 1.4f : 0.f) * legK);   // a step into the strike
        if (!a.landed && u >= A.wind) { a.landed = true; Resolve(a, b, A); }
        if (u >= A.wind + A.act + A.rec) { a.st = Fi::IDLE; a.atk = -1; }
        a.dodgeReq = false; break; }
      case Fi::HIT: a.t += dt; accel = 6; if (a.t >= a.stun) a.st = Fi::IDLE; break;
      case Fi::DODGE: {
        a.t += dt; Vector3 dir = a.dodgeDir == 0 ? Vector3Scale(fw, -1) : Vector3Scale(rt, (float)a.dodgeDir);
        target = Vector3Scale(dir, a.t < 0.25f ? 5.5f : 0.f); accel = 40; if (a.t >= 0.42f) a.st = Fi::IDLE; break; }
      case Fi::ROCKED: {
        a.t += dt; target = Vector3Add(Vector3Scale(rt, sinf(a.t * 7) * 0.9f), Vector3Scale(fw, -0.3f)); accel = 8; if (a.t >= a.stun) a.st = Fi::IDLE; break; }
      case Fi::DOWN: {
        a.downT += dt; if (a.bot && a.downT > 1.2f) a.tap += dt * (2.5f + frand(0, 3.f)) * (a.hp > 30 ? 1.2f : 0.8f);   // the bots fight to get up too
        if (live && a.downT >= 1.3f && a.tap >= a.tapNeed) { a.st = Fi::GETUP; a.t = 0; a.hp = fminf(100.f, a.hp + 8); a.dH *= 0.75f; Sfx("cheer"); Call(a, "BACK ON HIS FEET!", Hex(0x8ce99a), 1.4f); }
        else if (a.downT >= 10.f && live) { Ko(b, a, "TKO"); }
        break; }
      case Fi::GETUP: a.t += dt; if (a.t >= 1.15f) { a.st = Fi::IDLE; a.inv = 0.9f; } break;
      case Fi::KO: break;
    }
    // movement
    Vector3 dv = Vector3Subtract(target, a.vel); float dl = Vector3Length(dv), ac = accel * dt; if (dl > ac) dv = Vector3Scale(dv, ac / dl);
    a.vel = Vector3Add(a.vel, dv); a.vel.y = 0; a.pos = Vector3Add(a.pos, Vector3Scale(a.vel, dt)); a.pos.y = 0.12f;
    float sp = Vector3Length(a.vel); a.stride += dt * (0.9f + sp * 0.5f);
  }
  void Update(float dt) override {
    time += dt; fx.Update(dt); conf.Update(dt); roar = fmaxf(0.f, roar - dt * 0.5f);
    for (auto& p : pops) p.t -= dt * 0.8f; pops.erase(std::remove_if(pops.begin(), pops.end(), [](const Pop& p) { return p.t <= 0; }), pops.end());
    for (auto& b : pools) b.a = fmaxf(0.f, b.a - dt * 0.004f);
    flashA = fmaxf(0.f, flashA - dt);
    sendT -= dt; if (sendT <= 0) { sendT = 0.066f; SendFi(); }
    if (phase == "intro") { t -= dt; if (t <= 0) StartBout(); return; }
    if (hitstop > 0) { hitstop -= dt; return; }
    if (phase == "bell") { bellT -= dt; for (int i = 0; i < 2; i++) UpdateFighter(i, dt, false); if (bellT <= 0) { phase = "fight"; Sfx("fBell"); Banner("FIGHT!", 1000, "#ff4d6d"); } return; }
    if (phase == "fight") {
      clock -= dt;
      for (int i = 0; i < 2; i++) UpdateFighter(i, dt, true);
      // the two fighters cannot stand in each other, nor leave the cage
      Fi &a = f[0], &b = f[1]; float dx = b.pos.x - a.pos.x, dz = b.pos.z - a.pos.z, d = hypotf(dx, dz);
      if (d < 0.85f && d > 0.001f) { float o = (0.85f - d) / 2; a.pos.x -= dx / d * o; a.pos.z -= dz / d * o; b.pos.x += dx / d * o; b.pos.z += dz / d * o; }
      for (auto& x : f) { float r = hypotf(x.pos.x, x.pos.z), lim = APO - 0.45f; if (r > lim) { x.pos.x *= lim / r; x.pos.z *= lim / r; } }
      if (phase == "fight" && clock <= 0) EndRound();
      if (clock > 0 && clock < 10 && ceilf(clock) != lastTickT) { lastTickT = ceilf(clock); if (clock < 4) Sfx("tick"); }
    } else if (phase == "rest") {
      restT -= dt; for (int i = 0; i < 2; i++) UpdateFighter(i, dt, false); if (restT <= 0) { rd++; BeginRound(false); }
    } else if (phase == "result") {
      t -= dt; for (int i = 0; i < 2; i++) UpdateFighter(i, dt, false); if (t <= 0) StartBout();
    }
  }
  float lastTickT = 0;
  void Finish2() {
    phase = "end"; auto o = Sorted([&](int i) { return (float)pts[i] + wins[i] * 0.1f; });
    FinishOrder(o, [&](int i) { return I(pts[i]) + " pts · " + I(wins[i]) + (wins[i] == 1 ? " win" : " wins") + (kos[i] ? " · " + I(kos[i]) + " KO" : std::string()); });
  }
  void SendFi() {
    for (int i = 0; i < 2; i++) { Fi& x = f[i]; if (x.pid < 0 || x.bot || (phase != "bell" && phase != "fight" && phase != "rest" && phase != "result")) continue;
      const char* st = x.st == Fi::DOWN ? "down" : x.st == Fi::KO ? "ko" : x.st == Fi::ROCKED ? "rocked" : x.st == Fi::GETUP ? "getup" : x.st == Fi::HIT ? "hit" : "ok";
      char b[420]; snprintf(b, sizeof b, "{\"t\":\"fi\",\"hp\":%.1f,\"sta\":%.1f,\"h\":%.1f,\"b\":%.1f,\"l\":%.1f,\"st\":\"%s\",\"tap\":%.2f,\"rd\":%d,\"rds\":%d,\"tm\":%d,\"ph\":\"%s\",\"bl\":%d,\"ms\":%s,\"op\":%s,\"oh\":%.1f}",
        x.hp, x.sta, x.dH, x.dB, x.dL, st, x.st == Fi::DOWN ? Clamp(x.tap / x.tapNeed, 0.f, 1.f) : 0.f, rd, ROUNDS, (int)ceilf(fmaxf(0.f, clock)), phase.c_str(), x.bleed > 0.1f ? 1 : 0,
        kit::Q(x.callT > 0 ? x.call : std::string()).c_str(), kit::Q(f[1 - i].name).c_str(), f[1 - i].hp);
      Send(x.pid, b);
    }
  }
  // ----------------------------------------------------------------------------------------------- drawing
  void Cage() {
    if (!octa.vertexCount) octa = GenMeshCylinder(0.5f, 1, 8);
    lit.Draw(prim.plane, TRS({ 0, -0.05f, 0 }, { 0, 0, 0 }, { 200, 1, 200 }), Hex(0x0b0d1a));
    // the mat: a canvas octagon with the corner colours, the centre ring and the cage's padded base
    lit.Draw(octa, TRS({ 0, 0.0f, 0 }, { 0, 0, 0 }, { RC * 2.35f, 0.2f, RC * 2.35f }), Hex(0x232845));
    lit.Draw(octa, TRS({ 0, 0.0f, 0 }, { 0, 0, 0 }, { RC * 2.1f, 0.22f, RC * 2.1f }), Hex(0x6f79b8));
    lit.Draw(octa, TRS({ 0, 0.0f, 0 }, { 0, 0, 0 }, { RC * 1.96f, 0.24f, RC * 1.96f }), Hex(0x8c97d6));   // three layers of canvas, each a hair higher: no flicker
    lit.Draw(prim.cyl, TRS({ 0, 0.121f, 0 }, { 0, 0, 0 }, { 2.2f, 0.004f, 2.2f }), Hex(0xe9ecff)); lit.Draw(prim.cyl, TRS({ 0, 0.123f, 0 }, { 0, 0, 0 }, { 1.9f, 0.004f, 1.9f }), Hex(0x8c97d6));
    for (int sd = -1; sd <= 1; sd += 2) lit.Draw(prim.cyl, TRS({ sd * 1.5f, 0.124f, 0 }, { 0, 0, 0 }, { 0.9f, 0.004f, 0.9f }), sd < 0 ? Hex(0xc92a2a) : Hex(0x1c7ed6));   // the two corners
    for (auto& bl : pools) lit.Draw(prim.cyl, TRS(bl.p, { 0, 0, 0 }, { bl.r * 2.f, 0.003f, bl.r * 1.7f }), Fade(Hex(0x7a0d14), bl.a));   // blood on the canvas
    // the fence: eight panels, corner posts, padded rails, chain-link
    for (int k = 0; k < 8; k++) {
      float a0 = k * PI / 4, a1 = (k + 1) * PI / 4; Vector3 p0 = { cosf(a0) * RC, 0, sinf(a0) * RC }, p1 = { cosf(a1) * RC, 0, sinf(a1) * RC };
      Vector3 mid = { (p0.x + p1.x) / 2, 0, (p0.z + p1.z) / 2 };
      if (Vector3DotProduct(Vector3Normalize(mid), camDir) > 0.55f) continue;   // the panel in front of the camera is open, as on the broadcast
      float len = Vector3Distance(p0, p1), yaw = atan2f(p1.x - p0.x, p1.z - p0.z);
      lit.Draw(prim.cube, TRS({ mid.x, 0.55f, mid.z }, { 0, yaw, 0 }, { 0.14f, 0.9f, len }), Hex(0x15161d));        // the padded base
      lit.Draw(prim.cube, TRS({ mid.x, 2.05f, mid.z }, { 0, yaw, 0 }, { 0.1f, 0.1f, len }), Hex(0x2d3040));        // top rail
      lit.Draw(prim.cube, TRS({ mid.x, 1.3f, mid.z }, { 0, yaw, 0 }, { 0.02f, 1.5f, len }), Fade(Hex(0xaeb7c9), 0.1f));   // the mesh, a faint haze
      for (int j = 1; j < 9; j++) { float u = j / 9.f; lit.Draw(prim.cube, TRS({ p0.x + (p1.x - p0.x) * u, 1.3f, p0.z + (p1.z - p0.z) * u }, { 0, 0, 0 }, { 0.025f, 1.5f, 0.025f }), Fade(Hex(0xaeb7c9), 0.55f)); }
      if (Vector3DotProduct(Vector3Normalize(p0), camDir) < 0.75f) lit.Draw(prim.cube, TRS({ p0.x, 1.1f, p0.z }, { 0, 0, 0 }, { 0.22f, 2.1f, 0.22f }), k % 2 ? Hex(0x1c7ed6) : Hex(0xc92a2a));   // the corner posts (not the ones right in front of the lens)
    }
    // lights over the cage
    for (int k = 0; k < 16; k++) { float a = k * PI / 8; Vector3 p = { cosf(a) * 6.2f, 7.5f, sinf(a) * 6.2f }; DrawCube(p, 0.5f, 0.2f, 0.5f, k % 2 ? Hex(0xfff3bf) : Hex(0xffe066)); lit.Draw(prim.cube, TRS({ p.x * 0.97f, 7.9f, p.z * 0.97f }, { 0, 0, 0 }, { 0.1f, 0.8f, 0.1f }), Hex(0x2d3040)); }
    DrawCube({ 0, 9.4f, 0 }, 3.2f, 1.7f, 3.2f, Hex(0x1c2038));   // the big screen over the cage
    DrawCube({ 0, 9.4f, 1.63f }, 2.9f, 1.4f, 0.05f, Hex(0x3b5bdb));
    if (!crowdMesh.vertexCount) crowdMesh = BuildCrowd(crowd, 11);
    float ex = (phase == "fight" ? 0.04f : 0.02f) + 0.16f * Clamp(roar, 0.f, 1.f); DrawProp(crowdMesh, MatrixTranslate(0, fabsf(sinf(time * (6 + 6 * roar))) * ex, 0));
  }
  const char* ClipFor(Fi& x, float& t, bool& scrub, const char*& c2, float& t2, bool& scrub2, float& mix) {
    scrub = false; scrub2 = false; c2 = nullptr; mix = 0; t2 = 0;
    switch (x.st) {
      case Fi::ATTACK: { const Atk& A = ATK[x.atk]; float ds = 1.f + (1.f - x.sta / 100.f) * 0.35f, u = x.t / ds, tot = A.wind + A.act + A.rec; scrub = true; t = u < A.wind ? 0.42f * (u / A.wind) : 0.42f + 0.5f * ((u - A.wind) / (tot - A.wind)); (void)tot; return A.clip; }
      case Fi::HIT: scrub = true; t = Clamp(x.t / x.stunMax, 0.f, 1.f) * 0.95f; return x.hitHeavy ? "HitRecieve_2" : "HitRecieve";
      case Fi::ROCKED: scrub = true; t = 0.45f + 0.2f * sinf(x.t * 8); return "HitRecieve_2";
      case Fi::DOWN: scrub = true; t = Clamp(x.downT / 0.9f, 0.f, 1.f); return "Death";
      case Fi::KO: x.downT += 0; scrub = true; t = 1.f; return "Death";
      case Fi::GETUP: scrub = true; t = 1.f - Clamp(x.t / 1.1f, 0.f, 1.f); return "Death";
      case Fi::DODGE: t = x.stride; return x.dodgeDir > 0 ? "Run_Right" : x.dodgeDir < 0 ? "Run_Left" : "Run_Back";
      default: break;
    }
    if (phase == "result" && winner >= 0 && &x == &f[winner] && x.st == Fi::IDLE) { t = time * 1.2f; return "Wave"; }   // the winner celebrates
    Vector3 fw = { sinf(x.yaw), 0, cosf(x.yaw) }, rt = { -fw.z, 0, fw.x }; float vf = Vector3DotProduct(x.vel, fw), vr = Vector3DotProduct(x.vel, rt), sp = Vector3Length(x.vel);
    if (x.blk) { c2 = "HitRecieve"; t2 = 0.12f; scrub2 = true; mix = 0.65f; }   // a raised guard: the arms come up and the body tucks in
    if (sp > 0.5f) { t = x.stride; return fabsf(vr) > fabsf(vf) * 0.9f ? (vr > 0 ? "Run_Right" : "Run_Left") : vf < 0 ? "Run_Back" : "Run"; }
    t = time + x.pid * 0.4f; return "Idle_Sword";
  }
  void DrawFighter(Fi& x) {
    PersonPose q; q.pos = x.pos; q.yaw = x.yaw; q.c = x.flash > 0 ? Hex(0xffffff) : x.col; q.hsh = PersonHash(x.col); q.height = 1.8f; q.headM = &x.headM;
    bool scrub, scrub2; const char* c2; float t, t2, mix; q.clip = ClipFor(x, t, scrub, c2, t2, scrub2, mix); q.t = t; q.scrub = scrub; q.clip2 = c2; q.t2 = t2; q.scrub2 = scrub2; q.mix = mix;
    if (x.st == Fi::DOWN || x.st == Fi::KO) q.pos.y = 0.12f;
    if (!DrawPersonPose(q)) DrawToon(x.pos, x.yaw, x.col, time, 0, 0, 1.2f);
    // cuts and swelling on the face: they grow with the damage the head has taken
    float cut = Clamp((x.dH - 25.f) / 55.f, 0.f, 1.f);
    if (cut > 0.02f) { Vector3 h = { x.headM.m12, x.headM.m13, x.headM.m14 }; Vector3 fwd = { sinf(x.yaw), 0, cosf(x.yaw) }; Vector3 rt = { -fwd.z, 0, fwd.x };
      Vector3 c = Vector3Add(h, Vector3Add(Vector3Scale(fwd, 0.1f), { 0, 0.03f, 0 }));
      lit.Draw(prim.sphere, TRS(Vector3Add(c, Vector3Scale(rt, 0.06f)), { 0, x.yaw, 0 }, { 0.09f * cut, 0.025f, 0.02f }), Hex(0xa3121c));           // a cut over the eye
      if (cut > 0.45f) lit.Draw(prim.sphere, TRS(Vector3Add(Vector3Add(c, Vector3Scale(rt, -0.07f)), { 0, -0.07f, 0 }), { 0, x.yaw, 0 }, { 0.06f * cut, 0.05f * cut, 0.03f }), Hex(0x6b2a5a));   // a swollen eye
      if (cut > 0.7f) lit.Draw(prim.sphere, TRS(Vector3Add(c, { 0, -0.13f, 0 }), { 0, x.yaw, 0 }, { 0.05f, 0.04f, 0.02f }), Hex(0xa3121c)); }   // a split lip
  }
  void Draw() override {
    // camera: side-on to the fight, pulling back as they separate, cutting to a close shot on a knockdown
    Vector3 mid = { (f[0].pos.x + f[1].pos.x) / 2, 1.05f, (f[0].pos.z + f[1].pos.z) / 2 }; float sep = Dist();
    Vector3 dir = Vector3Subtract(f[1].pos, f[0].pos); dir.y = 0; float dl = Vector3Length(dir); if (dl < 0.01f) dir = { 1, 0, 0 }; else dir = Vector3Scale(dir, 1 / dl);
    Vector3 perp = { -dir.z, 0, dir.x }; float swing = sinf(time * 0.25f) * 0.35f;   // always side-on, a slow drift
    Vector3 side = Vector3Normalize(Vector3Add(Vector3Scale(perp, cosf(swing)), Vector3Scale(dir, sinf(swing))));
    float dist = 4.9f + sep * 0.6f; bool kd = f[0].st == Fi::DOWN || f[1].st == Fi::DOWN || phase == "result";
    Vector3 wp = Vector3Add(mid, Vector3Add(Vector3Scale(side, kd ? dist * 0.8f : dist), { 0, kd ? 1.6f : 2.0f + sep * 0.15f, 0 })); Vector3 wt = { mid.x, kd ? 0.7f : 1.3f, mid.z };
    { Camera3D rc = rig.Go(wp, wt, kd ? 38 : 42, 2.4f); cam.position = rc.position; cam.target = rc.target; cam.fovy = rc.fovy; }
    UI::Sky(Hex(0x05060f), Hex(0x161b3a));
    lit.Begin(cam, { 0.45f, 0.48f, 0.68f }, 0.004f);
    camDir = Vector3Normalize({ cam.position.x, 0, cam.position.z });
    BeginMode3D(cam);
    Cage();
    for (auto& x : f) if (x.pid >= 0) DrawFighter(x);
    {   // the referee keeps clear of the camera, side-on to the fight, and steps in over a fallen fighter
      Fi* dn = f[0].st == Fi::DOWN ? &f[0] : f[1].st == Fi::DOWN ? &f[1] : nullptr; Vector3 rp = Vector3Add(mid, Vector3Scale(side, -2.1f)); if (dn) rp = Vector3Add({ dn->pos.x, 0, dn->pos.z }, Vector3Scale(side, -1.3f)); rp.y = 0.12f;
      float rl = hypotf(rp.x, rp.z), lim = APO - 0.5f; if (rl > lim) { rp.x *= lim / rl; rp.z *= lim / rl; }
      static Vector3 rs = { 0, 0.12f, -2 }; rs = Vector3Lerp(rs, rp, 0.06f); Vector3 lk = dn ? Vector3{ dn->pos.x, 0, dn->pos.z } : Vector3{ mid.x, 0, mid.z };
      PersonPose q; q.pos = rs; q.yaw = atan2f(lk.x - rs.x, lk.z - rs.z); q.c = Hex(0x15161d); q.hsh = 29; q.height = 1.8f; q.clip = Vector3Distance(rs, rp) > 0.15f ? "Walk" : "Idle"; q.t = time * 1.1f;
      if (!DrawPersonPose(q)) {} }
    fx.Draw(); conf.Draw();
    EndMode3D();
    DrawHud();
  }
  void Bar(float x, float y, float w, float h, float v, Color c1, Color c2, bool flip) {
    Color bg = Color{ 10, 12, 28, 210 }; RRect(x, y, w, h, h / 2, bg, bg, Color{ 255, 255, 255, 90 }, 1.5f);
    float fw = fmaxf(0.f, (w - 6) * Clamp(v, 0.f, 1.f)); if (fw > 2) RRect(flip ? x + 3 + (w - 6) - fw : x + 3, y + 3, fw, h - 6, (h - 6) / 2, c1, c2, BLANK, 0);
  }
  void DrawHud() {
    for (int i = 0; i < 2; i++) {
      Fi& x = f[i]; if (x.pid < 0) continue; bool right = i == 1; float bx = right ? 1590 - 600 : 10 + 0, by = 26, bw = 600;
      bx = right ? 940 : 60;
      Color hc = x.hp > 55 ? Hex(0x51cf66) : x.hp > 28 ? Hex(0xffd43b) : Hex(0xff4d4d), hc2 = x.hp > 55 ? Hex(0x2b8a3e) : x.hp > 28 ? Hex(0xf08c00) : Hex(0xc92a2a);
      RRect(bx - 10, 8, bw + 20, 124, 24, Color{ 8, 10, 26, 150 }, Color{ 8, 10, 26, 150 }, BLANK, 0);
      Text(right ? bx + bw : bx, by - 2, 30, x.col, x.name, right ? 1 : -1, TS_DISPLAY | TS_SHADOW);
      Bar(bx, 44, bw, 30, x.hp / 100.f, hc, hc2, right);
      Bar(bx, 82, bw * 0.62f + 0, 14, x.sta / 100.f, Hex(0x4dabf7), Hex(0x1971c2), right);   // stamina
      // the damage the body has taken: head, body, legs
      const char* LB[3] = { "HEAD", "BODY", "LEGS" }; float dv[3] = { x.dH, x.dB, x.dL };
      for (int k = 0; k < 3; k++) { float px = right ? bx + bw - 76.f - k * 80.f : bx + bw * 0.62f + 14 + k * 80.f; Color dc = dv[k] > 66 ? Hex(0xff4d4d) : dv[k] > 33 ? Hex(0xff922b) : Hex(0xffd43b);
        Text(px + 34, 89, 14, Hex(0xdfe4f2), LB[k], 0, TS_PLAIN | TS_HEAVY | TS_SPACED); RRect(px, 98, 68, 9, 4.5f, Color{ 10, 12, 28, 210 }, Color{ 10, 12, 28, 210 }, BLANK, 0); if (dv[k] > 1) RRect(px + 1, 99, fmaxf(5.f, 66 * dv[k] / 100.f), 7, 3.5f, dc, dc, BLANK, 0); }
      if (x.bleed > 0.1f) Text(right ? bx + bw - 120 : bx + 120, by + 4, 22, Hex(0xff4d4d), "🩸", 0, TS_PLAIN);
      for (int k = 0; k < x.knockdowns; k++) Text(right ? bx + 22 + k * 26 : bx + bw - 22 - k * 26, 114, 18, Hex(0xff8787), "↓", 0, TS_HEAVY | TS_PLAIN);
    }
    // the clock and the round
    RRect(700, 8, 200, 100, 26, Color{ 8, 10, 26, 190 }, Color{ 8, 10, 26, 190 }, Color{ 255, 255, 255, 80 }, 2);
    Text(800, 34, 20, Hex(0xffd43b), std::string("ROUND ") + I(rd) + " / " + I(ROUNDS), 0, TS_HEAVY | TS_PLAIN | TS_SPACED);
    int cs = phase == "fight" ? (int)ceilf(fmaxf(0.f, clock)) : phase == "bell" ? (int)RT : 0; char tb[16]; snprintf(tb, sizeof tb, "%d:%02d", cs / 60, cs % 60);
    Text(800, 78, 50, clock < 10 && phase == "fight" ? Hex(0xff6b6b) : WHITE, tb, 0, TS_DISPLAY | TS_SHADOW);
    // calls over the fighters' heads
    for (auto& p : pops) { Vector2 s = GetWorldToScreen({ p.p.x, p.p.y + (1 - p.t) * 0.6f, p.p.z }, cam); Text(s.x, s.y, 34 * p.big, Fade(p.c, Clamp(p.t * 2, 0.f, 1.f)), p.s, 0, TS_DISPLAY | TS_SHADOW); }
    for (auto& x : f) if (x.pid >= 0 && x.callT > 0) { Vector2 s = GetWorldToScreen({ x.pos.x, 2.25f, x.pos.z }, cam); Text(s.x, s.y, 26, Fade(x.callC, Clamp(x.callT * 3, 0.f, 1.f)), x.call, 0, TS_DISPLAY | TS_SHADOW); }
    // the referee's count
    for (auto& x : f) if (x.st == Fi::DOWN) { int n = (int)x.downT + 1; Text(800, 330, 190 + 40 * (1 - (x.downT - floorf(x.downT))), Fade(Hex(0xffd43b), 0.95f), I(n), 0, TS_DISPLAY | TS_SHADOW);
      Text(800, 470, 30, WHITE, x.name + " is down — tap to get up!", 0, TS_HEAVY | TS_SHADOW | TS_PLAIN); Bar(560, 500, 480, 22, x.tap / x.tapNeed, Hex(0xff922b), Hex(0xf08c00), false); }
    if (phase == "bell") { float b = bellT; Text(800, 330, 70, WHITE, b > 1.6f ? std::string("ROUND ") + I(rd) : "READY…", 0, TS_DISPLAY | TS_SHADOW | TS_GOLD); }
    if (phase == "intro") { RRect(380, 230, 840, 280, 36, Color{ 8, 10, 26, 200 }, Color{ 8, 10, 26, 200 }, Color{ 255, 255, 255, 80 }, 2); Text(800, 290, 20, Hex(0xc9d3f2), "FIGHT NIGHT", 0, TS_PLAIN | TS_HEAVY | TS_SPACED); Text(800, 370, 90, WHITE, "CAGE FIGHT", 0, TS_DISPLAY | TS_SHADOW | TS_GOLD); Text(800, 450, 26, Hex(0xdfe6ff), "Punches, kicks, blocks, blood — win by KO or by the judges", 0, TS_HEAVY | TS_PLAIN, 780); }
    if (phase == "result") { RRect(430, 180, 740, 190, 30, Color{ 8, 10, 26, 205 }, Color{ 8, 10, 26, 205 }, Color{ 255, 255, 255, 90 }, 2); Text(800, 250, 56, winner >= 0 ? f[winner].col : WHITE, verdict, 0, TS_DISPLAY | TS_SHADOW, 700);
      char sb[64]; snprintf(sb, sizeof sb, "Judges: %d – %d", judgeA, judgeB); if (method == "DECISION") Text(800, 320, 26, Hex(0xc9d3f2), sb, 0, TS_HEAVY | TS_PLAIN); }
    if (phase == "fight" && time < 12) Text(800, 876, 21, Fade(WHITE, 0.92f), "BLOCK stops punches and high kicks  ·  DODGE slips a strike, then counter  ·  low kicks cannot be blocked", 0, TS_HEAVY | TS_PLAIN);
    Text(800, 142, 20, Hex(0xdfe4f2), std::string("FIGHT ") + I(bout) + " OF " + I(BOUTS), 0, TS_HEAVY | TS_PLAIN | TS_SPACED);
  }
};
}  // namespace

static RegGame fight1("fight", [] { return std::unique_ptr<Game>(new Fight); });
