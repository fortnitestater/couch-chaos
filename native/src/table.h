// Shared card-table pieces: phone layout helpers, playing cards, and the cosy 3D room + round table (Table base).
#pragma once
#include "cc.h"
#include "rlgl.h"
#include <functional>
#include <map>

namespace {
const char* OPT[] = { "#ff6b6b", "#4dabf7", "#51cf66", "#ffd43b", "#cc5de8", "#ff922b", "#22b8cf", "#f06595", "#94d82d", "#845ef7" };
std::string Q(const std::string& s) { std::string r = "\""; for (char c : s) { if (c == '"' || c == '\\') { r += '\\'; r += c; } else if (c == '\n') r += "\\n"; else r += c; } return r + "\""; }
std::string B(bool b) { return b ? "true" : "false"; }
std::string Hx(Color c) { char b[8]; snprintf(b, sizeof b, "#%02x%02x%02x", c.r, c.g, c.b); return b; }
template <class T> void Shuffle(std::vector<T>& v) { for (int i = (int)v.size() - 1; i > 0; i--) std::swap(v[i], v[rand() % (i + 1)]); }
int irand(int a, int b) { return a + rand() % (b - a + 1); }

// ---------- playing cards ----------
const char* SUIT[] = { "♠", "♥", "♦", "♣" };
const char* RANK[] = { "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K" };
struct Card { int r = 0, s = 0; };  // r 0..12 (A..K), s 0..3
std::string CardStr(Card c) { return std::string(RANK[c.r]) + SUIT[c.s]; }
bool Red(Card c) { return c.s == 1 || c.s == 2; }
std::vector<Card> Deck(int decks = 1) { std::vector<Card> d; for (int k = 0; k < decks; k++) for (int s = 0; s < 4; s++) for (int r = 0; r < 13; r++) d.push_back({ r, s }); Shuffle(d); return d; }

// a phone "choice" grid
std::string Choice(const std::string& prompt, const std::vector<std::string>& labels, const std::vector<bool>& off, int cols, bool big = false, const std::vector<std::string>& colors = {}) {
  std::string o;
  for (size_t i = 0; i < labels.size(); i++) o += std::string(i ? "," : "") + "{\"label\":" + Q(labels[i]) + ",\"color\":" + Q(i < colors.size() ? colors[i] : OPT[i % 10]) + ",\"disabled\":" + B(i < off.size() && off[i]) + "}";
  return "{\"name\":\"choice\",\"prompt\":" + Q(prompt) + ",\"cols\":" + std::to_string(cols) + ",\"big\":" + B(big) + ",\"options\":[" + o + "]}";
}
std::string Msg(const std::string& emoji, const std::string& text, const std::string& sub = "") { return "{\"name\":\"msg\",\"emoji\":" + Q(emoji) + ",\"text\":" + Q(text) + ",\"sub\":" + Q(sub) + "}"; }

// =====================================================================================
// the shared card table: a cosy room, a round wooden table with green felt, everyone sitting around it
// =====================================================================================
struct Table : Game {
  Camera3D cam{}; Particles3D fx; Confetti conf; float time = 0; int turn = -1; bool over = false; float endT = 0;
  CamRig rig; Camera3D master{};
  Color felt = Hex(0x2b8a3e), wood = Hex(0x8a5a35);
  Player& P(int i) { return (*ps)[i]; }
  int N() { return (int)ps->size(); }
  // Someone who lets a whole turn run out is "away" (in the kitchen, on the phone): from then on their turns wait
  // only a few seconds before the game plays for them, so one missing player never stalls the table. Touching the phone brings them back.
  std::vector<int> away; bool autoMove = false;   // autoMove: the game itself is making the move, that must not count as the player coming back
  bool IsAway(int p) { return p >= 0 && p < (int)away.size() && away[p] > 0; }
  void SetAway(int p, bool v) {
    if ((int)away.size() < N()) away.resize(N(), 0);
    if (p < 0 || p >= N() || P(p).bot || (!v && autoMove)) return;
    if (v) { if (!away[p]) Banner("💤 " + P(p).name + " is away — the game plays for them", 2200, "#adb5bd"); away[p]++; Buzz(p, 350); }
    else if (away[p]) { away[p] = 0; Banner("👋 " + P(p).name + " is back!", 1400, "#8ce99a"); }
  }
  float TurnClock(int p, float timer) { return IsAway(p) ? fminf(timer, 5.f) : timer; }
  void InitCam(Vector3 pos, Vector3 target, float fov = 45) { cam.position = pos; cam.target = target; cam.up = { 0, 1, 0 }; cam.fovy = fov; cam.projection = CAMERA_PERSPECTIVE; master = cam; }
  // what the camera should favour right now (default: the player whose turn it is); w = how strongly
  virtual bool Focus(Vector3& p, float& w) { if (over || turn < 0 || turn >= N()) return false; p = Seat(turn, 5.6f); p.y = 0.4f; w = 1; return true; }
  // ---- first person: on your turn the TV IS your eyes, from your seat, at eye level. Every seat gets the identical shot. ----
  int povWho = -1, povHide = -1; float povHold = 0;      // a game can lend the camera to someone for a moment
  virtual int ViewSeat() { return povHold > 0 ? povWho : -1; }   // whose eyes (-1: the film angle on the whole table)
  // where the eyes are: angle round the table, distance from its middle, height, field of view, and what they look at
  virtual void PovPose(int who, float& ang, float& rad, float& h, float& fov, Vector3& tgt) {
    float a = SeatA(who); ang = atan2f(sinf(a) * 0.78f, cosf(a)); rad = 8.7f; h = 2.7f; fov = 56; tgt = { 0, 0.15f, 0 };
  }
  void LendCamera(int who, float secs = 2.4f) { povWho = who; povHold = secs; }
  float cA = 0, cAv = 0, cR = 14, cRv = 0, cH = 7, cHv = 0, cF = 40, cFv = 0; Vector3 cT{}, cTv{}; bool cOn = false;
  static void Spr(float& x, float& v, float want, float k, float dt) { float a = (want - x) * k * k - v * 2 * k; v += a * dt; x += v * dt; }
  void UpdateCam() {
    g_cineK = 0;   // no opening glide and no drift: a first-person camera must not wander
    float dt = fminf(g_frameDt, 0.05f); povHold -= dt;
    Vector3 m = master.position; float mA = atan2f(m.z, m.x), mR = hypotf(m.x, m.z), mH = m.y;
    int who = ViewSeat(); povHide = who;
    float wA = mA, wR = mR, wH = mH, wF = master.fovy; Vector3 wT = master.target;
    if (who >= 0) PovPose(who, wA, wR, wH, wF, wT);
    else { Vector3 fp{}; float fw = 0; if (Focus(fp, fw)) wT = Vector3Lerp(wT, fp, 0.2f * fw); }
    if (!cOn) { cOn = true; cA = wA; cR = wR; cH = wH; cF = wF; cT = wT; cAv = cRv = cHv = cFv = 0; cTv = {}; }
    float dA = angDiff(cA, wA); Spr(cA, cAv, cA + dA, 1.7f, dt); Spr(cR, cRv, wR, 1.9f, dt); Spr(cH, cHv, wH, 1.9f, dt); Spr(cF, cFv, wF, 2.0f, dt);
    for (int i = 0; i < 3; i++) { float* x = (&cT.x) + i; float* v = (&cTv.x) + i; Spr(*x, *v, (&wT.x)[i], 2.0f, dt); }
    cam.position = { cosf(cA) * cR, cH, sinf(cA) * cR }; cam.target = cT; cam.fovy = cF; cam.up = { 0, 1, 0 };
  }
  // "MAYA'S TURN": whose eyes these are
  virtual bool PovHudOn() { return true; }
  void PovHud() {
    if (!PovHudOn() || povHide < 0 || povHide >= N() || over) return;
    const Player& p = P(povHide); std::string s = p.name + (p.bot ? "" : "'s turn"); float w = Measure(s, 30, TS_DISPLAY) + 96;
    RRect(800 - w / 2, 70, w, 50, 25, Color{ 10, 14, 36, 215 }, Color{ 10, 14, 36, 215 }, Fade(p.color, 0.95f), 3);
    RRect(800 - w / 2 + 12, 82, 26, 26, 13, p.color, p.color, BLANK, 0);
    Text(800 + 18, 96, 30, WHITE, s, 0, TS_DISPLAY | TS_PLAIN);
  }
  void TableCam() { InitCam({ 2.4f, 7.4f, 13.4f }, { 0.3f, 0.1f, -2.6f }, 40); }   // low, a little off-centre: a film angle on the table
  // seats run around the far half of the table so nobody blocks the view
  float SeatA(int i) { int n = N(); float span = n <= 2 ? 1.2f : n == 3 ? 1.9f : n == 4 ? 2.3f : 3.5f; return -PI / 2 + (n == 1 ? 0 : -span / 2 + span * i / (n - 1)); }
  Vector3 Seat(int i, float r = 7.4f) { float a = SeatA(i); return { cosf(a) * r, 0, sinf(a) * r * 0.78f }; }
  Vector3 Front(int i, float r = 4.6f, float h = 0.05f) { Vector3 s = Seat(i, r); s.y = h; return s; }
  // looking from a seat: the camera sits behind the player, a little off to one side, and looks at the middle of the table
  Vector3 SeatAway(int who) { float a = SeatA(who); return { -cosf(a), 0, -sinf(a) }; }
  void Room(bool table = true) {
    UpdateCam();
    UI::Sky(Hex(0x2a1d16), Hex(0x4a3324));
    lit.Begin(cam, { 0.2f, 0.15f, 0.12f }, 0.01f);
    // the mood: evening in a living room. A warm pendant over the table is the light that matters; a dim, cool moonlight
    // from the windows keeps the shadows; the corners fall into a soft warm dark; the lamp and the fire glow where they stand
    lit.Mood(0.2f, { 0.42f, 0.48f, 0.66f }, { 0.3f, -1.0f, 0.2f }, 0.04f); lit.Sky({ 0.5f, 0.4f, 0.34f });
    lit.Light({ 0, 6.2f, 0.5f }, { 2.3f, 1.55f, 0.9f }, 6.4f);
    BeginMode3D(cam);
    const float F = -3.2f, wz = -11.5f;
    struct Mood { unsigned floor, rugEdge, rug, wall, stripe, sofa, sofaD, curtA, curtB; };
    static const Mood MOODS[] = {
      { 0x8a5a33, 0xe8dcc4, 0x2f4f7f, 0x6f8b78, 0x7a9683, 0x3d5a80, 0x36506f, 0x9e3b3b, 0xae4545 },  // sage den
      { 0x6b3f26, 0xf0d9a8, 0x7a1f2b, 0x24344f, 0x2c3f5e, 0x8c2f39, 0x7a2731, 0xc9a227, 0xd9b23a },  // navy casino lounge
      { 0x9a6a3e, 0xfbe9c6, 0x1f6f6a, 0xd98f5c, 0xe09b68, 0x2a7f78, 0x226a64, 0xf3e4c2, 0xe6d3aa },  // terracotta sunroom
      { 0x5a3a28, 0xe9d5ff, 0x4a2a7a, 0x3b2a5c, 0x46346b, 0xd9a441, 0xc08f35, 0x7b3fa0, 0x8d4ab5 },  // plum game room
      { 0x7d5a3c, 0xdff3ff, 0x1c5d8a, 0x9fc3d6, 0xabd0e2, 0xe8eef3, 0xd3dce4, 0x2f6f9f, 0x3a7fb2 },  // winter cabin
    };
    const int mi = (int)(((unsigned)felt.r * 3 + (unsigned)felt.g * 7 + (unsigned)felt.b * 11) % 5);
    const Mood& M = MOODS[mi];
    // the rooms differ in more than colour: fireplace or window, bookcase or painting, wood / tile / carpet, round or square rug
    const bool fire = mi == 1 || mi == 3, shelf = mi != 1 && mi != 2, rectRug = mi == 1 || mi == 4;
    lit.Pattern(mi == 2 ? PAT_TILE : mi == 3 ? PAT_FELT : PAT_WOOD);
    lit.Draw(prim.plane, TRS({ 0, F, 0 }, { 0, 0, 0 }, { 120, 1, 120 }), Hex(mi == 2 ? 0xd8c7a8 : mi == 3 ? 0x3a2a55 : M.floor));
    lit.Pattern(PAT_RUG);
    const Mesh& rugM = rectRug ? prim.cube : prim.cyl;
    lit.Draw(rugM, TRS({ 0, F, 0.5f }, { 0, 0, 0 }, { 23, 0.04f, 15.5f }), Hex(M.rugEdge));   // rug border
    lit.Draw(rugM, TRS({ 0, F + 0.012f, 0.5f }, { 0, 0, 0 }, { 21.6f, 0.04f, 14.2f }), Hex(M.rug));
    lit.Pattern(PAT_NONE);
    // back wall: sage wallpaper with fine stripes, white painted wainscoting with raised panels, dado rail, skirting, crown moulding
    lit.Draw(prim.cube, TRS({ 0, 6, wz }, { 0, 0, 0 }, { 80, 19, 0.4f }), Hex(M.wall));
    for (int i = -24; i <= 24; i++) lit.Draw(prim.cube, TRS({ i * 1.6f, 7.2f, wz + 0.21f }, { 0, 0, 0 }, { 0.5f, 16.6f, 0.02f }), Hex(M.stripe));
    lit.Draw(prim.cube, TRS({ 0, -1.2f, wz + 0.3f }, { 0, 0, 0 }, { 80, 4.0f, 0.2f }), Hex(0xe9e4d8));
    for (int i = -12; i <= 12; i++) {
      lit.Draw(prim.cube, TRS({ i * 3.2f, -1.25f, wz + 0.42f }, { 0, 0, 0 }, { 2.6f, 2.9f, 0.06f }), Hex(0xf1ede3));
      lit.Draw(prim.cube, TRS({ i * 3.2f, -1.25f, wz + 0.46f }, { 0, 0, 0 }, { 2.3f, 2.6f, 0.04f }), Hex(0xe4dfd2));
    }
    lit.Draw(prim.cube, TRS({ 0, 0.85f, wz + 0.45f }, { 0, 0, 0 }, { 80, 0.18f, 0.25f }), Hex(0xf4f1ea));     // dado rail
    lit.Draw(prim.cube, TRS({ 0, F + 0.25f, wz + 0.45f }, { 0, 0, 0 }, { 80, 0.5f, 0.25f }), Hex(0xf4f1ea));  // skirting
    lit.Draw(prim.cube, TRS({ 0, 15.4f, wz + 0.5f }, { 0, 0, 0 }, { 80, 0.5f, 0.5f }), Hex(0xf4f1ea));        // crown
    {   // the other three walls (the TV's default angle never sees them; the player's-seat camera does)
      const float FZ = 21.f, SX = 27.f;
      for (int s = 0; s < 3; s++) {
        float yaw = s == 0 ? 0 : PI / 2; Vector3 c = s == 0 ? Vector3{ 0, 6, FZ } : Vector3{ s == 1 ? -SX : SX, 6, 4 };
        Vector3 al = { cosf(yaw), 0, -sinf(yaw) }, in = s == 0 ? Vector3{ 0, 0, -1 } : Vector3{ s == 1 ? 1.f : -1.f, 0, 0 };
        float len = s == 0 ? 80 : 70;
        auto at = [&](float la, float y, float inn) { return Vector3{ c.x + al.x * la + in.x * inn, y, c.z + al.z * la + in.z * inn }; };
        lit.Draw(prim.cube, TRS(c, { 0, yaw, 0 }, { len, 19, 0.4f }), Hex(M.wall));
        for (int i = -22; i <= 22; i++) lit.Draw(prim.cube, TRS(at(i * 1.6f, 7.2f, 0.21f), { 0, yaw, 0 }, { 0.5f, 16.6f, 0.02f }), Hex(M.stripe));
        lit.Draw(prim.cube, TRS(at(0, -1.2f, 0.3f), { 0, yaw, 0 }, { len, 4.0f, 0.2f }), Hex(0xe9e4d8));
        lit.Draw(prim.cube, TRS(at(0, 0.85f, 0.45f), { 0, yaw, 0 }, { len, 0.18f, 0.25f }), Hex(0xf4f1ea));
        lit.Draw(prim.cube, TRS(at(0, F + 0.25f, 0.45f), { 0, yaw, 0 }, { len, 0.5f, 0.25f }), Hex(0xf4f1ea));
        lit.Draw(prim.cube, TRS(at(0, 15.4f, 0.5f), { 0, yaw, 0 }, { len, 0.5f, 0.5f }), Hex(0xf4f1ea));
        for (int w = -1; w <= 1; w += 2) {   // two tall windows onto a night sky, with a moon and a warm lamp glow on the sill
          float lx = w * 9.5f;
          lit.Draw(prim.cube, TRS(at(lx, 6.2f, 0.3f), { 0, yaw, 0 }, { 5.6f, 5.6f, 0.3f }), Hex(0xf4f1ea));
          lit.Draw(prim.cube, TRS(at(lx, 6.2f, 0.45f), { 0, yaw, 0 }, { 5.0f, 5.0f, 0.1f }), Hex(0x1c2b5a));
          if (w > 0) lit.Draw(prim.sphere, TRS(at(lx + 1.1f, 7.4f, 0.55f), { 0, yaw, 0 }, { 1.0f, 1.0f, 0.1f }), Hex(0xfff3bf));
          for (int k = 0; k < 9; k++) lit.Draw(prim.sphere, TRS(at(lx - 2.1f + fmodf(k * 1.37f + w, 4.2f), 4.3f + fmodf(k * 1.9f, 3.6f), 0.55f), { 0, yaw, 0 }, { 0.08f, 0.08f, 0.05f }), WHITE);
          lit.Draw(prim.cube, TRS(at(lx, 6.2f, 0.58f), { 0, yaw, 0 }, { 0.16f, 4.8f, 0.1f }), Hex(0xf4f1ea));
          lit.Draw(prim.cube, TRS(at(lx, 6.2f, 0.58f), { 0, yaw, 0 }, { 5.0f, 0.16f, 0.1f }), Hex(0xf4f1ea));
          lit.Draw(prim.cube, TRS(at(lx, 3.45f, 0.7f), { 0, yaw, 0 }, { 6.0f, 0.2f, 0.7f }), Hex(0xf4f1ea));
          for (int cs = -1; cs <= 1; cs += 2) lit.Draw(prim.cube, TRS(at(lx + cs * 3.3f, 6.4f, 0.75f), { 0, yaw, 0 }, { 1.4f, 6.4f, 0.35f }), cs < 0 ? Hex(M.curtA) : Hex(M.curtB));
        }
        // framed pictures between the windows, and a tall plant in the corner
        for (int f = 0; f < 3; f++) { float lx = -2.5f + f * 2.5f; lit.Draw(prim.cube, TRS(at(lx, 7.0f, 0.3f), { 0, yaw, 0 }, { 1.9f, 2.5f, 0.15f }), Hex(0xf1ede3)); lit.Draw(prim.cube, TRS(at(lx, 7.0f, 0.4f), { 0, yaw, 0 }, { 1.6f, 2.2f, 0.1f }), Hex(f % 3 == 0 ? M.sofa : f % 3 == 1 ? M.curtB : M.rug)); }
      }
    }
    float wx = -9;
    if (!fire) {
    // window: frame, glass with the night sky + moon, sill, mullions, curtains on a rail
    lit.Draw(prim.cube, TRS({ wx, 6.2f, wz + 0.3f }, { 0, 0, 0 }, { 6.0f, 5.6f, 0.3f }), Hex(0xf4f1ea));
    lit.Draw(prim.cube, TRS({ wx, 6.2f, wz + 0.42f }, { 0, 0, 0 }, { 5.4f, 5.0f, 0.1f }), Hex(0x1c2b5a));
    lit.Draw(prim.sphere, TRS({ wx + 1.3f, 7.5f, wz + 0.5f }, { 0, 0, 0 }, { 1.1f, 1.1f, 0.1f }), Hex(0xfff3bf));
    for (int k = 0; k < 10; k++) lit.Draw(prim.sphere, TRS({ wx - 2.4f + fmodf(k * 1.37f, 4.8f), 4.2f + fmodf(k * 1.9f, 4.0f), wz + 0.5f }, { 0, 0, 0 }, { 0.08f, 0.08f, 0.05f }), WHITE);
    lit.Draw(prim.cube, TRS({ wx, 6.2f, wz + 0.52f }, { 0, 0, 0 }, { 0.16f, 5.0f, 0.1f }), Hex(0xf4f1ea));
    lit.Draw(prim.cube, TRS({ wx, 6.2f, wz + 0.52f }, { 0, 0, 0 }, { 5.4f, 0.16f, 0.1f }), Hex(0xf4f1ea));
    lit.Draw(prim.cube, TRS({ wx, 3.35f, wz + 0.6f }, { 0, 0, 0 }, { 6.6f, 0.2f, 0.7f }), Hex(0xf4f1ea));
    lit.Draw(prim.cyl, TRS({ wx - 4.4f, 9.6f, wz + 0.9f }, { 0, 0, PI / 2 }, { 0.12f, 8.8f, 0.12f }), Hex(0xb08d57));
    for (int s = -1; s <= 1; s += 2) for (int f = 0; f < 4; f++) {   // pleated curtains
      float x = wx + s * (3.4f + f * 0.32f);
      lit.Draw(prim.cyl, TRS({ x, 1.0f, wz + 0.95f + (f % 2) * 0.12f }, { 0, 0, 0 }, { 0.42f, 8.6f, 0.32f }), Hex(f % 2 ? M.curtA : M.curtB));
    }
    // sofa under the window: a real model in the room's colour (block sofa until it has loaded)
    if (!DrawModel("sofa.glb", { wx, F, wz + 2.2f }, 0, 2.9f, WHITE, Hex(M.sofa))) {
    // sofa under the window, with cushions
    lit.Draw(prim.cube, TRS({ wx, F + 0.75f, wz + 1.9f }, { 0, 0, 0 }, { 6.4f, 1.2f, 2.2f }), Hex(M.sofa));
    lit.Draw(prim.cube, TRS({ wx, F + 2.0f, wz + 1.05f }, { 0, 0, 0 }, { 6.4f, 2.2f, 0.6f }), Hex(M.sofaD));
    for (int s = -1; s <= 1; s += 2) lit.Draw(prim.cube, TRS({ wx + s * 3.0f, F + 1.5f, wz + 1.9f }, { 0, 0, 0 }, { 0.6f, 1.6f, 2.3f }), Hex(M.sofaD));
    for (int k = 0; k < 2; k++) lit.Draw(prim.cube, TRS({ wx - 1.35f + k * 2.7f, F + 1.55f, wz + 2.1f }, { 0, 0, 0 }, { 2.55f, 0.45f, 1.7f }), Shade(Hex(M.sofa), 0.14f));
    lit.Draw(prim.cube, TRS({ wx - 1.8f, F + 2.25f, wz + 1.5f }, { -0.25f, 0, 0.15f }, { 1.0f, 1.0f, 0.3f }), Hex(0xf2c14e));
    lit.Draw(prim.cube, TRS({ wx + 1.9f, F + 2.25f, wz + 1.5f }, { -0.25f, 0, -0.12f }, { 1.0f, 1.0f, 0.3f }), Hex(0xe76f51));
    for (int s = -1; s <= 1; s += 2) for (int q = -1; q <= 1; q += 2) lit.Draw(prim.cyl, TRS({ wx + s * 3.0f, F, wz + 1.9f + q * 0.9f }, { 0, 0, 0 }, { 0.14f, 0.18f, 0.14f }), Hex(0x3a2a1a));
    }
    DrawModel("floorlamp.glb", { wx + 5.0f, F, wz + 2.0f }, 0, 5.2f);
    lit.Light({ wx + 5.0f, F + 4.6f, wz + 2.4f }, { 1.6f, 1.0f, 0.55f }, 2.8f);   // the floor lamp warms its corner
    } else {
      // brick fireplace with a timber mantel, logs, a gilt mirror, candles and two wing chairs
      lit.Pattern(PAT_STONE);
      lit.Draw(prim.cube, TRS({ wx, F + 2.7f, wz + 0.9f }, { 0, 0, 0 }, { 6.6f, 5.4f, 1.4f }), Hex(0x9a5a44));
      lit.Draw(prim.cube, TRS({ wx, 8.6f, wz + 0.6f }, { 0, 0, 0 }, { 4.6f, 13.6f, 0.8f }), Hex(0x8a4f3c));
      lit.Pattern(PAT_NONE);
      lit.Draw(prim.cube, TRS({ wx, F + 1.7f, wz + 1.58f }, { 0, 0, 0 }, { 3.6f, 3.2f, 0.1f }), Hex(0x140d0b));
      { float fl = 0.85f + 0.15f * sinf(time * 9.f) * sinf(time * 5.3f); lit.Light({ wx, F + 1.4f, wz + 2.6f }, { 2.0f * fl, 0.9f * fl, 0.35f * fl }, 3.4f);   // firelight, flickering
        lit.Draw(prim.sphere, TRS({ wx + 0.3f, F + 0.9f, wz + 1.75f }, { 0, 0, 0 }, { 1.6f, 0.9f * fl, 0.5f }), Hex(0xffb347)); }
      lit.Pattern(PAT_WOOD); lit.Draw(prim.cube, TRS({ wx, F + 5.55f, wz + 1.0f }, { 0, 0, 0 }, { 7.4f, 0.32f, 1.9f }), Hex(0x5a3a22)); lit.Pattern(PAT_NONE);
      for (int q = -1; q <= 1; q++) lit.Draw(prim.cyl, TRS({ wx + q * 0.75f + 0.9f, F + 0.35f + (q == 0 ? 0.3f : 0), wz + 1.75f }, { 0, 0, PI / 2 }, { 0.32f, 1.8f, 0.32f }), Hex(0x4a2f1c));
      lit.Draw(prim.cube, TRS({ wx, F + 8.2f, wz + 1.05f }, { 0, 0, 0 }, { 3.4f, 3.0f, 0.14f }), Hex(0xc9a227));
      lit.Draw(prim.cube, TRS({ wx, F + 8.2f, wz + 1.14f }, { 0, 0, 0 }, { 2.9f, 2.5f, 0.04f }), Hex(0xbfd4e6));
      for (int q = 0; q < 3; q++) lit.Draw(prim.cyl, TRS({ wx - 2.6f + q * 2.6f, F + 5.7f, wz + 1.2f }, { 0, 0, 0 }, { 0.22f, 0.7f + (q % 2) * 0.4f, 0.22f }), Hex(0xf4f1ea));
      for (int sd = -1; sd <= 1; sd += 2) if (!DrawModel("armchair.glb", { wx + sd * 5.6f, F, wz + 2.6f }, -sd * 0.45f, 3.0f, WHITE, Hex(M.sofa))) {
        lit.Draw(prim.cube, TRS({ wx + sd * 5.6f, F + 0.9f, wz + 2.4f }, { 0, -sd * 0.35f, 0 }, { 2.4f, 1.3f, 2.2f }), Hex(M.sofa));
        lit.Draw(prim.cube, TRS({ wx + sd * 5.9f, F + 2.6f, wz + 1.6f }, { 0, -sd * 0.35f, 0 }, { 2.4f, 3.4f, 0.6f }), Hex(M.sofaD));
      }
    }
    if (shelf) {
    // bookshelf with books, a plant and ornaments
    lit.Pattern(PAT_WOOD);
    lit.Draw(prim.cube, TRS({ 9.5f, 3.4f, wz + 1.1f }, { 0, 0, 0 }, { 6.4f, 13.2f, 1.6f }), Hex(0x6b4426));
    lit.Pattern(PAT_NONE);
    lit.Draw(prim.cube, TRS({ 9.5f, 3.4f, wz + 1.5f }, { 0, 0, 0 }, { 5.8f, 12.4f, 1.0f }), Hex(0x3e2716));
    const unsigned bc[] = { 0xb03a2e, 0x1f618d, 0xd68910, 0x1e8449, 0x7d3c98, 0xd4ac0d, 0x117a65, 0xc0392b, 0xecf0f1, 0x34495e };
    for (int shelf = 0; shelf < 4; shelf++) {
      float y = -1.6f + shelf * 3.0f;
      lit.Draw(prim.cube, TRS({ 9.5f, y + 0.2f, wz + 1.6f }, { 0, 0, 0 }, { 5.9f, 0.2f, 1.2f }), Hex(0x7a4e2c));
      float x = 6.9f;
      for (int b = 0; b < 12 && x < 11.6f; b++) {
        if (b == 5 && shelf % 2) { lit.Draw(prim.sphere, TRS({ x + 0.4f, y + 0.75f, wz + 1.7f }, { 0, 0, 0 }, { 0.8f, 0.9f, 0.8f }), Hex(shelf == 1 ? 0xe9e4d8 : 0x2a9d8f)); x += 0.95f; continue; }
        float bw = 0.28f + 0.16f * ((b * 7 + shelf * 3) % 3) / 2.f, bh = 1.5f + 0.6f * ((b * 5 + shelf) % 4) / 3.f;
        float tilt = (b == 8 && shelf % 2 == 0) ? 0.28f : 0;
        lit.Draw(prim.cube, TRS({ x + bw / 2, y + 0.3f + bh / 2, wz + 1.75f }, { 0, 0, tilt }, { bw, bh, 0.95f }), Hex(bc[(b * 3 + shelf * 7) % 10]));
        x += bw + 0.04f + (tilt > 0 ? 0.35f : 0);
      }
    }
    } else {
      // a big gilt-framed landscape over a console table with a vase and a globe
      lit.Draw(prim.cube, TRS({ 9.5f, 7.2f, wz + 0.32f }, { 0, 0, 0 }, { 6.0f, 4.2f, 0.24f }), Hex(0xc9a227));
      lit.Draw(prim.cube, TRS({ 9.5f, 7.2f, wz + 0.46f }, { 0, 0, 0 }, { 5.4f, 3.6f, 0.04f }), Hex(0x8ecae6));
      lit.Draw(prim.sphere, TRS({ 10.8f, 8.0f, wz + 0.49f }, { 0, 0, 0 }, { 0.9f, 0.9f, 0.03f }), Hex(0xffe08a));
      lit.Draw(prim.sphere, TRS({ 8.4f, 5.95f, wz + 0.5f }, { 0, 0, 0 }, { 3.0f, 1.1f, 0.03f }), Hex(0x4f9d69));
      lit.Draw(prim.sphere, TRS({ 10.6f, 5.9f, wz + 0.51f }, { 0, 0, 0 }, { 3.2f, 0.9f, 0.03f }), Hex(0x3b7f57));
      lit.Pattern(PAT_WOOD);
      lit.Draw(prim.cube, TRS({ 9.5f, F + 2.3f, wz + 1.1f }, { 0, 0, 0 }, { 6.0f, 0.3f, 1.5f }), Hex(0x6b4426));
      for (int sd = -1; sd <= 1; sd += 2) lit.Draw(prim.cube, TRS({ 9.5f + sd * 2.7f, F + 1.1f, wz + 1.1f }, { 0, 0, 0 }, { 0.3f, 2.2f, 1.3f }), Hex(0x5a3a22));
      lit.Pattern(PAT_NONE);
      lit.Draw(prim.sphere, TRS({ 8.0f, F + 3.2f, wz + 1.1f }, { 0, 0, 0 }, { 1.1f, 1.5f, 1.1f }), Hex(0x2a9d8f));
      lit.Draw(prim.cyl, TRS({ 8.0f, F + 3.8f, wz + 1.1f }, { 0, 0, 0 }, { 0.4f, 0.5f, 0.4f }), Hex(0x2a9d8f));
      lit.Draw(prim.cyl, TRS({ 10.9f, F + 2.45f, wz + 1.1f }, { 0, 0, 0 }, { 0.9f, 0.25f, 0.9f }), Hex(0xd9a441));
      lit.Draw(prim.sphere, TRS({ 10.9f, F + 3.1f, wz + 1.1f }, { 0, 0, 0 }, { 1.0f, 1.0f, 1.0f }), Hex(0xe9e4d8));
    }
    // gallery wall: framed pictures with mats and simple art
    const unsigned pc[] = { 0xf4a261, 0x8ecae6, 0xe76f51, 0x90be6d };
    for (int k = 0; k < 4; k++) {
      float x = -2.6f + k * 2.6f, y = 7.0f + (k % 2 ? 0.9f : -0.2f), w = k % 2 ? 1.9f : 2.2f, h = k % 2 ? 2.4f : 1.8f;
      lit.Draw(prim.cube, TRS({ x, y, wz + 0.3f }, { 0, 0, 0 }, { w, h, 0.14f }), Hex(k == 2 ? 0xc9a227 : 0x2b2b2b));
      lit.Draw(prim.cube, TRS({ x, y, wz + 0.38f }, { 0, 0, 0 }, { w - 0.24f, h - 0.24f, 0.04f }), Hex(0xf8f5ee));
      lit.Draw(prim.cube, TRS({ x, y, wz + 0.41f }, { 0, 0, 0 }, { w - 0.6f, h - 0.6f, 0.03f }), Hex(pc[k]));
      lit.Draw(prim.sphere, TRS({ x - 0.15f, y - 0.1f, wz + 0.43f }, { 0, 0, 0 }, { (w - 0.6f) * 0.5f, (h - 0.6f) * 0.5f, 0.03f }), Shade(Hex(pc[k]), -0.3f));
    }
    // sideboard with a table lamp, and a tall plant in a ceramic pot
    lit.Pattern(PAT_WOOD);
    lit.Draw(prim.cube, TRS({ 3.6f, F + 1.3f, wz + 1.1f }, { 0, 0, 0 }, { 4.2f, 2.6f, 1.4f }), Hex(0x7a4e2c));
    lit.Pattern(PAT_NONE);
    for (int k = 0; k < 2; k++) lit.Draw(prim.cube, TRS({ 2.6f + k * 2.0f, F + 1.3f, wz + 1.82f }, { 0, 0, 0 }, { 1.85f, 2.2f, 0.05f }), Hex(0x8a5a35));
    lit.Draw(prim.cyl, TRS({ 4.6f, F + 2.6f, wz + 1.1f }, { 0, 0, 0 }, { 0.5f, 1.2f, 0.5f }), Hex(0x2a9d8f));
    lit.Draw(prim.cone, TRS({ 4.6f, F + 3.7f, wz + 1.1f }, { 0, 0, 0 }, { 1.4f, 1.0f, 1.4f }), Hex(0xf6e7c8));
    if (!DrawModel("monstera.glb", { -4.6f, F, wz + 2.0f }, 0.4f, 4.6f)) {
    lit.Draw(prim.cyl, TRS({ -4.6f, F, wz + 1.6f }, { 0, 0, 0 }, { 1.3f, 1.5f, 1.3f }), Hex(0xe9e4d8));
    for (int k = 0; k < 9; k++) { float a = k * 0.7f; lit.Draw(prim.sphere, TRS({ -4.6f + cosf(a) * 0.55f, F + 2.6f + (k % 3) * 0.8f, wz + 1.6f + sinf(a) * 0.35f }, { 0, 0, cosf(a) * 0.7f }, { 0.55f, 1.9f, 0.35f }), Hex(k % 2 ? 0x2d6a4f : 0x40916c)); }
    }
    DrawModel("snakeplant.glb", { 13.6f, F, wz + 2.6f }, 0, 3.6f);
    DrawModel("sidetable.glb", { 1.1f, F, wz + 2.0f }, 0, 2.4f, WHITE, Hex(0x5a3a22));
    // warm light: lamp glows + soft pools on the wall (unlit, so they bloom)
    rlDrawRenderBatchActive(); rlDisableDepthMask(); BeginBlendMode(BLEND_ADDITIVE);
    DrawSphere({ 4.6f, F + 3.4f, wz + 1.1f }, 0.5f, Fade(Hex(0xffd59a), 0.55f));
    for (int k = 0; k < 12; k++) { float r = 0.6f + k * 0.35f; DrawCylinderEx({ 4.6f, F + 3.9f, wz + 0.45f }, { 4.6f, F + 3.9f, wz + 0.46f }, r, r, 40, Fade(Hex(0xffb86b), 0.022f)); }
    if (fire) for (int k = 0; k < 9; k++) { float fl = 0.7f + 0.3f * sinf(time * 9 + k * 1.7f); DrawSphere({ wx + sinf(k * 2.1f) * 0.9f, F + 0.9f + (k % 4) * 0.4f, wz + 1.55f }, 0.6f - (k % 4) * 0.1f, Fade(Hex(k % 2 ? 0xff7a1a : 0xffc04d), 0.32f * fl)); }
    if (!fire) for (int k = 0; k < 10; k++) { float r = 0.8f + k * 0.4f; DrawCylinderEx({ wx + 1.3f, 7.5f, wz + 0.55f }, { wx + 1.3f, 7.5f, wz + 0.56f }, r, r, 40, Fade(Hex(0x8fb3ff), 0.014f)); }
    BeginBlendMode(BLEND_ALPHA); rlDrawRenderBatchActive(); rlEnableDepthMask();
    if (!table) return;
    lit.Draw(prim.cyl, TRS({ 0, F, 0 }, { 0, 0, 0 }, { 1.6f, 2.9f, 1.6f }), Shade(wood, -0.3f));
    lit.Pattern(PAT_WOOD);
    lit.Draw(prim.cyl, TRS({ 0, -0.45f, 0 }, { 0, 0, 0 }, { 13.6f, 0.42f, 11.2f }), wood);
    lit.Draw(prim.cyl, TRS({ 0, -0.1f, 0 }, { 0, 0, 0 }, { 12.75f, 0.1f, 10.45f }), Hex(0xd9b46a));   // brass inlay ring
    lit.Pattern(PAT_FELT);
    lit.Draw(prim.cyl, TRS({ 0, -0.06f, 0 }, { 0, 0, 0 }, { 12.4f, 0.08f, 10.1f }), felt);
    lit.Pattern(PAT_NONE);
    rlDrawRenderBatchActive(); rlDisableDepthMask(); BeginBlendMode(BLEND_ADDITIVE);
    for (int k = 0; k < 9; k++) { float r = 1.2f + k * 0.62f; DrawCylinder({ 0, 0.0f + k * 0.001f, -0.2f }, r, r, 0.001f, 48, Fade(Hex(0xffe2a8), 0.02f)); }
    BeginBlendMode(BLEND_ALPHA); rlDrawRenderBatchActive(); rlEnableDepthMask();
  }
  void Seats(std::function<bool(int)> hot = nullptr, std::function<float(int)> cheer = nullptr) {
    for (int i = 0; i < N(); i++) {
      Vector3 s = Seat(i); bool h = hot ? hot(i) : i == turn;
      if (h) lit.Draw(prim.cyl, TRS({ s.x, -3.17f, s.z }, { 0, 0, 0 }, { 3.2f, 0.04f, 3.2f }), Fade(P(i).color, 0.9f));
      float yaw = atan2f(-s.x, -s.z);
      if (HaveFile("p0.glb")) {
        Vector3 c = Seat(i, 9.4f);
        DrawModel("chair.glb", { c.x, -3.2f, c.z }, yaw, 3.4f);   // their chair, pushed back from the table
        if (i != povHide || cine::Busy()) DrawToon({ s.x, -3.2f, s.z }, yaw, P(i).color, time + i, h ? 0.01f : 0, cheer ? cheer(i) : 0, 2.05f);   // a replay shows everyone, even the player whose eyes the camera was
      } else {
        lit.Draw(prim.cube, TRS({ s.x * 1.08f, -1.3f, s.z * 1.08f }, { 0, yaw, 0 }, { 1.7f, 2.6f, 0.35f }), Hex(0x5c2f1a));  // chair back
        if (i != povHide || cine::Busy()) DrawToon({ s.x, -0.55f, s.z }, yaw, P(i).color, time + i, h ? fabsf(sinf(time * 5)) * 0.18f : 0, cheer ? cheer(i) : 0, 1.3f);
      }
    }
  }
  void SeatNames(std::function<std::string(int)> extra = nullptr) {
    for (int i = 0; i < N(); i++) {
      if (i == povHide) continue;   // that's whose eyes we are looking through
      Vector3 s = Seat(i); Vector2 p = GetWorldToScreen({ s.x, HaveFile("p0.glb") ? 3.3f : 2.2f, s.z }, cam);
      hud::Tag(p.x, p.y - 26, P(i).color, P(i).name, 28);
      if (i == turn) Text(p.x, p.y - 62, 30, Hex(0xffd43b), "▼", 0, TS_HEAVY | TS_SHADOW);
      if (extra) { std::string e = extra(i); if (!e.empty()) Text(p.x, p.y + 10, 28, WHITE, e, 0, TS_HEAVY | TS_SHADOW); }
    }
  }
  // px size of a world length at a spot (for text that sits on 3D things)
  float PxAt(Vector3 at, float len) { Vector2 a = GetWorldToScreen(at, cam), b = GetWorldToScreen({ at.x + len, at.y, at.z }, cam); return fabsf(b.x - a.x); }
  // a card lying on the table. face: 1 up, 0 down; squash fakes the flip
  struct TextJob { Vector3 at; float len; std::string s; Color c; float k; };
  std::vector<TextJob> jobs;
  // the printed cards (models/cards.png, made by native/tools/make_cards.py): row = suit, column = rank; row 4 holds the back
  static Texture2D& CardsTex() { static Texture2D t{}; if (!t.id && HaveFile("cards.png")) { t = LoadTexture("cards.png"); if (t.id) { GenTextureMipmaps(&t); SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR); } } return t; }
  static Vector4 CardCell(int col, int row) { return { (col * 157 + 7.5f) / 2048.f, (row * 204 + 0.5f) / 1024.f, 142.f / 2048.f, 203.f / 1024.f }; }
  void CardAt(Vector3 c, float yaw, bool faceUp, Card cd, float s = 1, float flip = 1, Color back = Hex(0xc92a2a)) {
    const float KT = 1.3f; c.x *= KT; c.z *= KT; s *= KT;   // everything on the felt is 30% bigger so it reads from the couch
    float sq = fabsf(flip * 2 - 1), lift = (1 - sq) * 0.6f * s;
    Vector3 p = { c.x, c.y + lift, c.z };
    lit.Draw(prim.cube, TRS({ p.x + 0.05f, 0.01f, p.z + 0.07f }, { 0, yaw, 0 }, { 0.92f * s * sq, 0.01f, 1.3f * s }), Hex(0x000000, 50));
    lit.Draw(prim.cube, TRS(p, { 0, yaw, 0 }, { 0.9f * s * sq, 0.03f, 1.28f * s }), faceUp ? Hex(0xfdfcf7) : Hex(0xffffff));
    if (Texture2D& tx = CardsTex(); tx.id) {   // a real printed card: pips, corner indices, court cards, a patterned back
      if (faceUp) { lit.Tex(tx, CardCell(cd.r, cd.s)); lit.Draw(prim.plane, TRS({ p.x, p.y + 0.017f, p.z }, { 0, yaw, 0 }, { 0.9f * s * sq, 1, 1.28f * s }), WHITE); }
      else { lit.Tex(tx, CardCell(0, 4)); lit.Draw(prim.plane, TRS({ p.x, p.y + 0.017f, p.z }, { 0, yaw, 0 }, { 0.8f * s * sq, 1, 1.18f * s }), back); }
      lit.NoTex(); return;
    }
    if (!faceUp) {
      lit.Draw(prim.cube, TRS({ p.x, p.y + 0.017f, p.z }, { 0, yaw, 0 }, { 0.78f * s * sq, 0.01f, 1.16f * s }), back);
      lit.Draw(prim.cube, TRS({ p.x, p.y + 0.02f, p.z }, { 0, yaw + PI / 4, 0 }, { 0.3f * s * sq, 0.01f, 0.3f * s }), Shade(back, 0.35f));
    } else if (sq > 0.3f) jobs.push_back({ { p.x, p.y + 0.02f, p.z }, 0.9f * s * sq, std::string(RANK[cd.r]) + "\n" + SUIT[cd.s], Red(cd) ? Hex(0xd62839) : Hex(0x1a1d29), 1 });
  }
  void Label(Vector3 at, float len, const std::string& s, Color c, float k = 1) { jobs.push_back({ at, len, s, c, k }); }
  void FlushText() {  // after EndMode3D
    for (auto& j : jobs) {
      Vector2 p = GetWorldToScreen(j.at, cam); float px = PxAt(j.at, j.len);
      size_t nl = j.s.find('\n');
      if (nl == std::string::npos) Text(p.x, p.y, fmaxf(9, px * 0.5f * j.k), j.c, j.s, 0, TS_HEAVY | TS_PLAIN);
      else { float sz = fmaxf(9, px * 0.42f * j.k); Text(p.x, p.y - sz * 0.42f, sz, j.c, j.s.substr(0, nl), 0, TS_HEAVY | TS_PLAIN); Text(p.x, p.y + sz * 0.5f, sz * 0.95f, j.c, j.s.substr(nl + 1), 0, TS_PLAIN); }
    }
    jobs.clear();
  }
  // a die: value on top, tumbling while roll > 0
  void Die(Vector3 c, int v, float roll, float s = 0.62f, Color body = WHITE, Color ring = BLANK) {
    const float KT = 1.3f; c.x *= KT; c.z *= KT; s *= KT;
    Vector3 r = { 0, 0, 0 }; float hop = 0;
    if (roll > 0) { r = { time * 13 + c.x, time * 9, time * 11 + c.z }; hop = fabsf(sinf(roll * 9)) * 0.7f * roll; }
    Vector3 p = { c.x, c.y + s / 2 + hop, c.z };
    lit.Draw(prim.cyl, TRS({ c.x + 0.06f, 0.01f, c.z + 0.06f }, { 0, 0, 0 }, { s * 1.2f, 0.01f, s * 1.2f }), Hex(0x000000, 50));
    if (ring.a) lit.Draw(prim.cyl, TRS({ c.x, 0.02f, c.z }, { 0, 0, 0 }, { s * 1.6f, 0.02f, s * 1.6f }), ring);
    lit.Draw(prim.cube, TRS(p, r, { s, s, s }), body);
    if (roll > 0) return;
    static const int P[7][6][2] = { {}, { { 0, 0 } }, { { -1, -1 }, { 1, 1 } }, { { -1, -1 }, { 0, 0 }, { 1, 1 } }, { { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 } }, { { -1, -1 }, { 1, -1 }, { 0, 0 }, { -1, 1 }, { 1, 1 } }, { { -1, -1 }, { 1, -1 }, { -1, 0 }, { 1, 0 }, { -1, 1 }, { 1, 1 } } };
    for (int k = 0; k < v && v <= 6; k++) lit.Draw(prim.sphere, TRS({ p.x + P[v][k][0] * s * 0.27f, p.y + s / 2, p.z + P[v][k][1] * s * 0.27f }, { 0, 0, 0 }, { s * 0.17f, s * 0.06f, s * 0.17f }), v == 1 ? Hex(0xe03131) : Hex(0x1a1438));
  }
  void Board(const std::string& title, const std::vector<int>& order, std::function<std::string(int)> val, float x = 18) {
    std::vector<hud::Row> rows; for (int p : order) rows.push_back({ P(p).color, P(p).name, val(p), p == turn });
    hud::Board(title, rows, x < 100 ? 24 : x, 340, true);   // bottom-left, clear of the players' heads
  }
  void Hint(const std::string& s, Color c = WHITE, float y = 857) { hud::Hint(s, c, y - 16); }
  void Title(const std::string& s, Color c = WHITE) { float w = Measure(s, 40, TS_DISPLAY) + 70; hud::Panel(20, 14, w, 68, 34); Text(20 + w / 2, 50, 40, c, s, 0, TS_DISPLAY | TS_SHADOW); }
  std::vector<int> Sorted(std::function<float(int)> key) { std::vector<int> o; for (int i = 0; i < N(); i++) o.push_back(i); std::stable_sort(o.begin(), o.end(), [&](int a, int b) { return key(a) > key(b); }); return o; }
  void FinishOrder(const std::vector<int>& o, std::function<std::string(int)> stat) { std::string r; for (int i : o) r += std::to_string(i) + ":" + stat(i) + ";"; Finish(r); }
  void Celebrate(int pid) { conf.Spawn({ 0, 4, 0 }, 140, 6, 6); Sfx("win"); if (pid >= 0) Banner(P(pid).name + " WINS!", 2600, Hx(P(pid).color).c_str()); WinShot(pid); }
  void WinShot(int pid) {
    if (pid < 0) return; Vector3 s = Seat(pid); bool real = HaveFile("p0.glb");
    cine::Shot sh; sh.kind = cine::WIN; sh.at = { s.x, real ? 0.0f : 0.2f, s.z }; sh.size = real ? 3.0f : 1.6f; sh.c = P(pid).color;   // the winner's chest, not the air over their head
    sh.face = { -s.x, 0, -s.z }; sh.elev = 0.3f; sh.floorY = real ? -3.2f : -1.6f; sh.hero = true; cine::Play(sh);
  }
  void DebugShot(int k) override { if (k >= 7) { turn = (k - 7) % N(); LendCamera(turn, 5); } else if (k == cine::WIN) WinShot(0); }   // 7, 8, 9: hand the turn (and the camera) to player 0, 1, 2
  void Tick(float dt) { time += dt; fx.Update(dt); conf.Update(dt); }

  void End3D() { fx.Draw(); conf.Draw(); EndMode3D(); FlushText(); PovHud(); }
};
}  // namespace
