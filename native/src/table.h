// Shared card-table pieces: phone layout helpers, playing cards, and the cosy 3D room + round table (Table base).
#pragma once
#include "cc.h"
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
  Color felt = Hex(0x2b8a3e), wood = Hex(0x8a5a35);
  Player& P(int i) { return (*ps)[i]; }
  int N() { return (int)ps->size(); }
  void InitCam(Vector3 pos, Vector3 target, float fov = 45) { cam.position = pos; cam.target = target; cam.up = { 0, 1, 0 }; cam.fovy = fov; cam.projection = CAMERA_PERSPECTIVE; }
  void TableCam() { InitCam({ 0, 10.6f, 9.4f }, { 0, -0.9f, -0.2f }, 47); }
  // seats run around the far half of the table so nobody blocks the view
  float SeatA(int i) { int n = N(); float span = n <= 2 ? 1.4f : n <= 4 ? 2.6f : 3.5f; return -PI / 2 + (n == 1 ? 0 : -span / 2 + span * i / (n - 1)); }
  Vector3 Seat(int i, float r = 7.4f) { float a = SeatA(i); return { cosf(a) * r, 0, sinf(a) * r * 0.78f }; }
  Vector3 Front(int i, float r = 4.6f, float h = 0.05f) { Vector3 s = Seat(i, r); s.y = h; return s; }
  void Room(bool table = true) {
    UI::Sky(Hex(0x24133d), Hex(0x4a2c1d));
    lit.Begin(cam, { 0.16f, 0.1f, 0.12f }, 0.012f);
    BeginMode3D(cam);
    lit.Pattern(PAT_WOOD);
    lit.Draw(prim.plane, TRS({ 0, -3.2f, 0 }, { 0, 0, 0 }, { 120, 1, 120 }), Hex(0x7a4b2a));
    lit.Pattern(PAT_RUG);
    lit.Draw(prim.sphere, TRS({ 0, -3.18f, 0.5f }, { 0, 0, 0 }, { 22, 0.02f, 15 }), Hex(0x9c2f3a));  // rug
    lit.Pattern(PAT_NONE);
    // back wall: plum wallpaper, wood panelling, a night window, shelves, pictures, a plant
    float wz = -11.5f;
    lit.Draw(prim.cube, TRS({ 0, 6, wz }, { 0, 0, 0 }, { 80, 19, 0.4f }), Hex(0x4a2f4f));
    for (int i = -12; i <= 12; i++) lit.Draw(prim.cube, TRS({ i * 3.2f, 6, wz + 0.22f }, { 0, 0, 0 }, { 0.08f, 19, 0.05f }), Hex(0x3f2744));  // stripes
    lit.Pattern(PAT_WOOD);
    lit.Draw(prim.cube, TRS({ 0, -1.2f, wz + 0.35f }, { 0, 0, 0 }, { 80, 4.2f, 0.3f }), Hex(0x6b3f22));
    lit.Pattern(PAT_NONE);
    lit.Draw(prim.cube, TRS({ 0, 0.95f, wz + 0.55f }, { 0, 0, 0 }, { 80, 0.25f, 0.3f }), Hex(0x8a5a35));
    // window with the moon outside
    lit.Draw(prim.cube, TRS({ -9, 6.2f, wz + 0.3f }, { 0, 0, 0 }, { 6.4f, 5.4f, 0.3f }), Hex(0x8a5a35));
    lit.Draw(prim.cube, TRS({ -9, 6.2f, wz + 0.42f }, { 0, 0, 0 }, { 5.6f, 4.6f, 0.1f }), Hex(0x1b2a6b));
    lit.Draw(prim.sphere, TRS({ -7.6f, 7.4f, wz + 0.5f }, { 0, 0, 0 }, { 1.2f, 1.2f, 0.1f }), Hex(0xfff3bf));
    for (int k = 0; k < 9; k++) lit.Draw(prim.sphere, TRS({ -11.3f + (k * 1.37f - floorf(k * 1.37f / 4.6f) * 4.6f), 4.5f + fmodf(k * 1.9f, 3.6f), wz + 0.5f }, { 0, 0, 0 }, { 0.09f, 0.09f, 0.05f }), WHITE);
    lit.Draw(prim.cube, TRS({ -9, 6.2f, wz + 0.5f }, { 0, 0, 0 }, { 0.2f, 4.6f, 0.1f }), Hex(0x8a5a35));
    lit.Draw(prim.cube, TRS({ -9, 6.2f, wz + 0.5f }, { 0, 0, 0 }, { 5.6f, 0.2f, 0.1f }), Hex(0x8a5a35));
    // bookshelf
    lit.Pattern(PAT_WOOD);
    lit.Draw(prim.cube, TRS({ 9.5f, 3.4f, wz + 1.1f }, { 0, 0, 0 }, { 6.4f, 13.2f, 1.6f }), Hex(0x6b3f22));
    lit.Pattern(PAT_NONE);
    const unsigned bc[] = { 0xe03131, 0x1971c2, 0xf08c00, 0x2f9e44, 0x9c36b5, 0xfab005, 0x0c8599, 0xe64980 };
    for (int shelf = 0; shelf < 4; shelf++) {
      float y = -1.6f + shelf * 3.0f;
      lit.Draw(prim.cube, TRS({ 9.5f, y + 2.6f, wz + 1.9f }, { 0, 0, 0 }, { 6.0f, 0.18f, 0.2f }), Hex(0x8a5a35));
      float x = 7.0f;
      for (int b = 0; b < 9 && x < 12.0f; b++) {
        float bw = 0.35f + 0.25f * ((b * 7 + shelf * 3) % 3) / 2.f, bh = 1.7f + 0.5f * ((b * 5 + shelf) % 3) / 2.f;
        float tilt = (b == 6 && shelf % 2 == 0) ? 0.25f : 0;
        lit.Draw(prim.cube, TRS({ x + bw / 2, y + 0.3f + bh / 2, wz + 1.7f }, { 0, 0, tilt }, { bw, bh, 1.0f }), Hex(bc[(b + shelf * 3) % 8]));
        x += bw + 0.06f;
      }
    }
    // framed pictures
    const unsigned pc[] = { 0xffd43b, 0x74c0fc, 0xff8787 };
    for (int k = 0; k < 3; k++) {
      float x = -1.5f + k * 3.0f, y = 6.2f + (k == 1 ? 0.8f : 0);
      lit.Draw(prim.cube, TRS({ x, y, wz + 0.3f }, { 0, 0, 0 }, { 2.2f, 2.8f, 0.2f }), Hex(0xc9a227));
      lit.Draw(prim.cube, TRS({ x, y, wz + 0.42f }, { 0, 0, 0 }, { 1.8f, 2.4f, 0.05f }), Hex(pc[k]));
      lit.Draw(prim.sphere, TRS({ x, y - 0.3f, wz + 0.46f }, { 0, 0, 0 }, { 1.0f, 1.0f, 0.05f }), Shade(Hex(pc[k]), -0.35f));
    }
    // potted plant
    lit.Draw(prim.cyl, TRS({ 4.2f, -3.2f, wz + 1.5f }, { 0, 0, 0 }, { 1.4f, 1.4f, 1.4f }), Hex(0xd9480f));
    for (int k = 0; k < 7; k++) { float a = k * 0.9f; lit.Draw(prim.sphere, TRS({ 4.2f + cosf(a) * 0.5f, -1.0f + (k % 3) * 0.7f, wz + 1.5f + sinf(a) * 0.3f }, { 0, 0, cosf(a) * 0.6f }, { 0.5f, 1.6f, 0.4f }), Hex(k % 2 ? 0x2f9e44 : 0x40c057)); }
    if (!table) return;
    lit.Draw(prim.cyl, TRS({ 0, -3.2f, 0 }, { 0, 0, 0 }, { 1.6f, 2.9f, 1.6f }), Shade(wood, -0.3f));
    lit.Pattern(PAT_WOOD);
    lit.Draw(prim.cyl, TRS({ 0, -0.45f, 0 }, { 0, 0, 0 }, { 13.6f, 0.42f, 11.2f }), wood);
    lit.Pattern(PAT_FELT);
    lit.Draw(prim.cyl, TRS({ 0, -0.06f, 0 }, { 0, 0, 0 }, { 12.4f, 0.08f, 10.1f }), felt);
    lit.Pattern(PAT_NONE);
  }
  void Seats(std::function<bool(int)> hot = nullptr, std::function<float(int)> cheer = nullptr) {
    for (int i = 0; i < N(); i++) {
      Vector3 s = Seat(i); bool h = hot ? hot(i) : i == turn;
      if (h) lit.Draw(prim.cyl, TRS({ s.x, -0.6f, s.z }, { 0, 0, 0 }, { 2.0f, 0.05f, 2.0f }), Fade(P(i).color, 0.85f));
      float yaw = atan2f(-s.x, -s.z);
      lit.Draw(prim.cube, TRS({ s.x * 1.08f, -1.3f, s.z * 1.08f }, { 0, yaw, 0 }, { 1.7f, 2.6f, 0.35f }), Hex(0x5c2f1a));  // chair back
      DrawToon({ s.x, -0.55f, s.z }, yaw, P(i).color, time + i, h ? fabsf(sinf(time * 5)) * 0.18f : 0, cheer ? cheer(i) : 0, 1.3f);
    }
  }
  void SeatNames(std::function<std::string(int)> extra = nullptr) {
    for (int i = 0; i < N(); i++) {
      Vector3 s = Seat(i); Vector2 p = GetWorldToScreen({ s.x, 2.2f, s.z }, cam);
      Text(p.x, p.y, 21, i == turn ? Hex(0xffd43b) : P(i).color, P(i).name, 0, TS_HEAVY);
      if (extra) { std::string e = extra(i); if (!e.empty()) Text(p.x, p.y + 26, 18, WHITE, e, 0, TS_HEAVY); }
    }
  }
  // px size of a world length at a spot (for text that sits on 3D things)
  float PxAt(Vector3 at, float len) { Vector2 a = GetWorldToScreen(at, cam), b = GetWorldToScreen({ at.x + len, at.y, at.z }, cam); return fabsf(b.x - a.x); }
  // a card lying on the table. face: 1 up, 0 down; squash fakes the flip
  struct TextJob { Vector3 at; float len; std::string s; Color c; float k; };
  std::vector<TextJob> jobs;
  void CardAt(Vector3 c, float yaw, bool faceUp, Card cd, float s = 1, float flip = 1, Color back = Hex(0xc92a2a)) {
    float sq = fabsf(flip * 2 - 1), lift = (1 - sq) * 0.6f * s;
    Vector3 p = { c.x, c.y + lift, c.z };
    lit.Draw(prim.cube, TRS({ p.x + 0.05f, 0.01f, p.z + 0.07f }, { 0, yaw, 0 }, { 0.92f * s * sq, 0.01f, 1.3f * s }), Hex(0x000000, 50));
    lit.Draw(prim.cube, TRS(p, { 0, yaw, 0 }, { 0.9f * s * sq, 0.03f, 1.28f * s }), faceUp ? Hex(0xfdfcf7) : Hex(0xffffff));
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
    UI::Box({ x, 18, 290, 48 + order.size() * 34.f }, 18, Hex(0x0b1020, 175));
    Text(x + 18, 42, 18, Hex(0xadb5bd), title, -1);
    for (size_t i = 0; i < order.size(); i++) { float y = 76 + i * 34.f; int p = order[i]; DrawCircle((int)x + 26, (int)y, 8, P(p).color); Text(x + 42, y, 20, p == turn ? Hex(0xffd43b) : WHITE, P(p).name, -1); Text(x + 272, y, 22, Hex(0xffd43b), val(p), 1); }
  }
  void Hint(const std::string& s, Color c = WHITE, float y = 857) { float w = Measure(s, 26, TS_HEAVY) + 60; UI::Box({ 800 - w / 2, y - 25, w, 50 }, 25, Hex(0x0b1020, 185)); Text(800, y, 26, c, s, 0, TS_HEAVY | TS_PLAIN); }
  void Title(const std::string& s, Color c = WHITE) { Text(800, 52, 44, c, s, 0, TS_DISPLAY | TS_SHADOW); }
  std::vector<int> Sorted(std::function<float(int)> key) { std::vector<int> o; for (int i = 0; i < N(); i++) o.push_back(i); std::stable_sort(o.begin(), o.end(), [&](int a, int b) { return key(a) > key(b); }); return o; }
  void FinishOrder(const std::vector<int>& o, std::function<std::string(int)> stat) { std::string r; for (int i : o) r += std::to_string(i) + ":" + stat(i) + ";"; Finish(r); }
  void Celebrate(int pid) { conf.Spawn({ 0, 4, 0 }, 140, 6, 6); Sfx("win"); if (pid >= 0) Banner(P(pid).name + " WINS!", 2600, Hx(P(pid).color).c_str()); }
  void Tick(float dt) { time += dt; fx.Update(dt); conf.Update(dt); }
  void End3D() { fx.Draw(); conf.Draw(); EndMode3D(); FlushText(); }
};
}  // namespace
