// CARD & BOARD GAMES — everyone around one 3D card table: Crazy Eights, Four in a Row, Snakes & Ladders,
// Blackjack, Bingo, Dice Poker, Memory, Higher or Lower and Hangman (Word Rescue).
// Phones use the website's ready-made screens (card hand, choice grid, bingo card, dice, panel).

#include "table.h"
#include "mesh.h"

namespace {

// =====================================================================================
// CRAZY EIGHTS — match the suit or the number. 8 = wild, 2 = +2, J skip, Q reverse, A = everyone passes their hand.
// =====================================================================================
struct Eights : Table {
  std::vector<Card> deck, pile; std::vector<std::vector<Card>> hands;
  int suit = 0, dir = 1, pending = 0; bool drew = false, needSuit = false; float timer = 30, botWait = 1.5f, chaosT = 0;
  struct Fly { Card c; Vector3 from; float t = 1; } fly;
  int winner = -1;
  Card Top() { return pile.back(); }
  int Idx(int k) { int n = N(); return ((turn + dir * k) % n + n) % n; }
  bool Special(Card c) { return c.r == 7 || c.r == 1 || c.r == 10 || c.r == 11 || c.r == 0; }
  bool Playable(Card c) { if (pending > 0) return c.r == 1; return c.r == 7 || c.s == suit || c.r == Top().r; }
  void Start() override {
    TableCam();
    deck = Deck(N() > 4 ? 2 : 1); int n = N() > 5 ? 5 : 7;
    hands.assign(N(), {});
    for (int k = 0; k < n; k++) for (int i = 0; i < N(); i++) { hands[i].push_back(deck.back()); deck.pop_back(); }
    while (Special(deck.back())) { deck.insert(deck.begin(), deck.back()); deck.pop_back(); }
    pile.push_back(deck.back()); deck.pop_back(); suit = Top().s;
    turn = rand() % N();
    Layout(-1, "{\"name\":\"cards\"}"); Sync(); Banner("🃏 CRAZY EIGHTS!", 1600, "#51cf66");
  }
  std::string HandMsg(int p) {
    auto& h = hands[p]; bool my = p == turn && !over; std::string cs, pl;
    for (size_t i = 0; i < h.size(); i++) { cs += std::string(i ? "," : "") + Q(CardStr(h[i])); pl += std::string(i ? "," : "") + B(my && !needSuit && Playable(h[i])); }
    return "{\"t\":\"hand\",\"cards\":[" + cs + "],\"playable\":[" + pl + "],\"myTurn\":" + B(my) + ",\"needSuit\":" + B(my && needSuit) +
      ",\"canDraw\":" + B(my && !needSuit && (!drew || pending > 0)) + ",\"canPass\":" + B(my && drew && !needSuit) + ",\"top\":" + Q(CardStr(Top())) +
      ",\"suit\":" + Q(SUIT[suit]) + ",\"pending\":" + std::to_string(pending) + ",\"turnName\":" + Q(P(turn).name) + ",\"turnColor\":" + Q(Hx(P(turn).color)) + "}";
  }
  void Sync() { for (int i = 0; i < N(); i++) if (!P(i).bot) Send(i, HandMsg(i)); }
  void Give(int p, int n) {
    for (int k = 0; k < n; k++) {
      if (deck.empty()) { Card t = Top(); pile.pop_back(); deck = pile; Shuffle(deck); pile = { t }; }
      if (deck.empty()) break;
      hands[p].push_back(deck.back()); deck.pop_back();
    }
    Sfx("card");
  }
  void Advance(int k) { turn = Idx(k); drew = false; needSuit = false; timer = 30; botWait = frand(1.1f, 1.9f); Sync(); }
  void Play(int p, int i) {
    auto& h = hands[p]; if (i < 0 || i >= (int)h.size() || !Playable(h[i])) return;
    Card c = h[i]; h.erase(h.begin() + i); pile.push_back(c); suit = c.s;
    fly = { c, Front(p, 4.2f, 0.1f), 0 }; Sfx("card");
    if (h.empty()) { over = true; endT = 3.5f; winner = p; Celebrate(p); Sync(); return; }
    if (h.size() == 1) Banner(P(p).name + ": LAST CARD!", 1500, "#ff4d6d");
    if (c.r == 7) { needSuit = true; timer = fmaxf(timer, 12); Sfx("ding"); Sync(); return; }
    if (c.r == 1) { pending += 2; Banner("+" + std::to_string(pending) + "!", 1100, "#ff922b"); Sfx("alarm"); Advance(1); }
    else if (c.r == 10) { Banner(P(Idx(1)).name + " SKIPPED!", 1300); Sfx("buzz"); Advance(2); }
    else if (c.r == 11) { dir = -dir; Banner("🔄 REVERSE!", 1100); Sfx("whoosh"); Advance(N() == 2 ? 2 : 1); }
    else if (c.r == 0) { auto hs = hands; for (int k = 0; k < N(); k++) hands[k] = hs[((k - dir) % N() + N()) % N()]; Banner("🌀 CHAOS! HANDS PASS!", 2000, "#cc5de8"); Sfx("whoosh"); chaosT = 1.2f; for (int k = 0; k < N(); k++) Buzz(k, 200); Advance(1); }
    else Advance(1);
  }
  void Draw1(int p) {
    if (pending > 0) { int n = pending; Give(p, n); pending = 0; Banner(P(p).name + " takes +" + std::to_string(n) + "!", 1300, "#ff922b"); Buzz(p, 300); Advance(1); return; }
    if (drew) return;
    Give(p, 1); drew = true;
    bool any = false; for (auto& c : hands[p]) any |= Playable(c);
    if (!any) Advance(1); else Sync();
  }
  int BestSuit(int p) { int cnt[4] = {}; for (auto& c : hands[p]) if (c.r != 7) cnt[c.s]++; int b = rand() % 4; for (int s = 0; s < 4; s++) if (cnt[s] > cnt[b]) b = s; return b; }
  void PickSuit(int p, int s) { suit = s; needSuit = false; Banner(P(p).name + " picks " + SUIT[s], 1300, s == 1 || s == 2 ? "#ff6b6b" : "#ffffff"); Advance(1); }
  void Act(int p, const std::string& k, int i, float) override {
    if (over || p != turn) return;
    if (k == "suit") { if (needSuit && i >= 0 && i < 4) PickSuit(p, i); return; }
    if (needSuit) return;
    if (k == "play") Play(p, i); else if (k == "draw") Draw1(p); else if (k == "pass" && drew) Advance(1);
  }
  void BotMove() {
    int p = turn; auto& h = hands[p];
    if (needSuit) { PickSuit(p, BestSuit(p)); return; }
    int pick = -1; for (size_t i = 0; i < h.size(); i++) if (Playable(h[i]) && h[i].r != 7) { pick = (int)i; break; }
    if (pick < 0) for (size_t i = 0; i < h.size(); i++) if (Playable(h[i])) { pick = (int)i; break; }
    if (pick >= 0) Play(p, pick); else if (!drew || pending) Draw1(p); else Advance(1);
  }
  void Update(float dt) override {
    Tick(dt); if (fly.t < 1) fly.t += dt * 3.2f; chaosT = fmaxf(0, chaosT - dt);
    if (over) { endT -= dt; if (endT <= 0 && !done) { done = true; auto o = Sorted([&](int i) { return i == winner ? 1000.f : -(float)hands[i].size(); }); FinishOrder(o, [&](int i) { return i == winner ? std::string("empty hand!") : std::to_string(hands[i].size()) + " cards left"; }); } return; }
    if (P(turn).bot) { botWait -= dt; if (botWait <= 0) { botWait = 0.9f; BotMove(); } return; }
    timer -= dt;
    if (timer <= 0) { int p = turn; if (needSuit) PickSuit(p, BestSuit(p)); else if (pending > 0 || !drew) { Draw1(p); if (turn == p && !over) Advance(1); } else Advance(1); }
  }
  bool done = false;
  void Draw() override {
    Room(); Seats();
    // draw pile + discard pile
    for (int k = 0; k < std::min<int>(14, (int)deck.size() / 3 + 1); k++) CardAt({ -1.6f, 0.05f + k * 0.035f, 0.3f }, 0.1f, false, {}, 1.35f);
    for (int k = std::max(0, (int)pile.size() - 4); k < (int)pile.size() - (fly.t < 1 ? 1 : 0); k++) { float j = (k * 37 % 11 - 5) * 0.05f; CardAt({ 1.4f + j * 0.4f, 0.05f + (k - (int)pile.size() + 4) * 0.035f, 0.3f + j * 0.3f }, j, true, pile[k], 1.35f); }
    if (fly.t < 1) { float e = EaseOut(fly.t); Vector3 to = { 1.4f, 0.25f, 0.3f }; CardAt({ Lerp(fly.from.x, to.x, e), 0.2f + sinf(e * PI) * 1.5f, Lerp(fly.from.z, to.z, e) }, e * 6.28f, true, fly.c, Lerp(0.8f, 1.35f, e)); }
    // everyone's hand as a fan of card backs
    for (int i = 0; i < N(); i++) {
      int n = (int)hands[i].size(); Vector3 f = Front(i, 4.5f); float a = SeatA(i);
      for (int k = 0; k < std::min(n, 12); k++) { float o = (k - (std::min(n, 12) - 1) / 2.f) * 0.32f; CardAt({ f.x - sinf(a) * o, 0.05f + k * 0.01f, f.z + cosf(a) * o * 0.78f }, -a - PI / 2 + o * 0.15f, false, {}, 0.7f, 1, i == turn ? Hex(0xe8590c) : Hex(0xc92a2a)); }
    }
    // direction ring
    for (int k = 0; k < 10; k++) { float a = time * 0.8f * dir + k * PI / 5; lit.Draw(prim.sphere, TRS({ cosf(a) * 3.1f, 0.05f, sinf(a) * 2.4f + 0.3f }, { 0, 0, 0 }, { 0.14f, 0.05f, 0.14f }), Fade(WHITE, 0.6f)); }
    End3D();
    SeatNames([&](int i) { return std::to_string(hands[i].size()) + " cards"; });
    Vector2 sp = GetWorldToScreen({ 1.4f, 0, 2.0f }, cam);
    UI::Box({ sp.x - 70, sp.y, 140, 46 }, 23, Hex(0x0b1020, 190)); Text(sp.x, sp.y + 23, 28, suit == 1 || suit == 2 ? Hex(0xff6b6b) : WHITE, std::string("suit ") + SUIT[suit], 0, TS_HEAVY | TS_PLAIN);
    if (pending) Text(800, 130, 40, Hex(0xff922b), "🔥 +" + std::to_string(pending) + " stack!", 0, TS_DISPLAY);
    Title(over ? P(winner).name + " WINS!" : P(turn).name + "'s turn", over ? P(winner).color : P(turn).color);
    if (!over && !P(turn).bot) { char b[8]; snprintf(b, sizeof b, "%d", (int)ceilf(timer)); Text(1540, 50, 34, timer < 8 ? Hex(0xff4d6d) : WHITE, b); }
    Hint("Match the SUIT or NUMBER · 8 wild · 2 = +2 · J skip · Q reverse · A = hands pass!");
  }
};

// =====================================================================================
// FOUR IN A ROW — red team vs yellow team, drop discs, line up four
// =====================================================================================
// the real Four in a Row frame: two moulded plates with 42 round holes, discs slide down the gap between them
static Mesh BuildFourGrid(float cell, float hr) {
  MeshBuilder b; const int seg = 24; float h = cell / 2; Color col = WHITE;
  for (float zc : { 0.2f, -0.2f }) {
    float zf = zc + 0.06f, zb = zc - 0.06f;
    for (int c = 0; c < 7; c++) for (int r = 0; r < 6; r++) {
      float x = (c - 3) * cell, y = 0.55f + r * cell;
      for (int k = 0; k < seg; k++) {
        float a0 = 2 * PI * k / seg, a1 = 2 * PI * (k + 1) / seg;
        float c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);
        float m0 = fmaxf(fabsf(c0), fabsf(s0)), m1 = fmaxf(fabsf(c1), fabsf(s1));
        Vector2 p0 = { x + c0 * hr, y + s0 * hr }, p1 = { x + c1 * hr, y + s1 * hr }, q0 = { x + c0 / m0 * h, y + s0 / m0 * h }, q1 = { x + c1 / m1 * h, y + s1 / m1 * h };
        for (int f = 0; f < 2; f++) {
          float z = f ? zb : zf; Vector3 n = { 0, 0, f ? -1.f : 1.f };
          b.Tri({ q0.x, q0.y, z }, { p0.x, p0.y, z }, { p1.x, p1.y, z }, n, n, n, col);
          b.Tri({ q0.x, q0.y, z }, { p1.x, p1.y, z }, { q1.x, q1.y, z }, n, n, n, col);
        }
        Vector3 n0 = { -c0, -s0, 0 }, n1 = { -c1, -s1, 0 };
        b.Tri({ p0.x, p0.y, zf }, { p1.x, p1.y, zf }, { p1.x, p1.y, zb }, n0, n1, n1, col);
        b.Tri({ p0.x, p0.y, zf }, { p1.x, p1.y, zb }, { p0.x, p0.y, zb }, n0, n1, n0, col);
      }
    }
    // a rim along the top of each plate
    b.Box({ 0, 0.55f + 5.5f * cell + 0.06f, zc }, { 7 * cell, 0.12f, 0.12f }, col);
  }
  // side and bottom edges close the gap between the plates
  for (int s = -1; s <= 1; s += 2) b.Box({ s * (3.5f * cell + 0.1f), 0.55f + 2.5f * cell, 0 }, { 0.2f, 6 * cell + 0.24f, 0.52f }, col);
  b.Box({ 0, 0.55f - 0.5f * cell - 0.08f, 0 }, { 7 * cell + 0.4f, 0.16f, 0.52f }, col);
  return b.Build();
}
// a disc with a raised rim and a ring groove on both faces (axis along z)
static Mesh BuildFourDisc() {
  MeshBuilder b;
  b.Lathe({ { 0, -0.085f }, { 0.22f, -0.085f }, { 0.25f, -0.072f }, { 0.28f, -0.085f }, { 0.34f, -0.09f }, { 0.39f, -0.075f }, { 0.41f, -0.04f }, { 0.41f, 0.04f },
            { 0.39f, 0.075f }, { 0.34f, 0.09f }, { 0.28f, 0.085f }, { 0.25f, 0.072f }, { 0.22f, 0.085f }, { 0, 0.085f } }, 28, [](float, int) { return WHITE; }, MatrixRotateX(PI / 2));
  return b.Build();
}
struct Four : Table {
  // everyone stands round the board: the TV looks at the grid from the active team's side, low, like a player leaning in
  int ViewSeat() override { return over || turn < 0 || turn >= N() ? -1 : turn; }
  // you step up to the board from your team's side and look at the grid at eye level
  bool PovHudOn() override { return false; }   // Four already shows whose turn it is
  void PovPose(int who, float& ang, float& rad, float& h, float& fov, Vector3& tgt) override { float x = side ? 4.6f : -4.6f, z = 9.4f; ang = atan2f(z, x); rad = hypotf(x, z); h = 1.1f; fov = 50; tgt = { 0, 0.55f, 0 }; }
  bool Focus(Vector3& p, float& w) override { if (drop.c >= 0) { p = { (drop.c - 3) * 0.95f, 0.4f, 0 }; w = 0.7f; return true; } return false; }
  Mesh gridM{}, discM{}; const float Y0 = -2.55f, F = -3.2f;
  ~Four() { if (gridM.vertexCount) UnloadMesh(gridM); if (discM.vertexCount) UnloadMesh(discM); }
  int g[7][6] = {}; int team[2] = { 0, 0 }; int side = 0; std::vector<int> T[2];
  struct Drop { int c = -1, r = 0, who = 0; float y = 0, v = 0; } drop;
  int win = -1; std::vector<std::pair<int, int>> line; float botT = 1.5f, timer = 25;
  const Color TC[2] = { Hex(0xff4d6d), Hex(0xffd43b) };
  int Cur() { return T[side][team[side] % T[side].size()]; }
  void Start() override {
    InitCam({ 2.6f, 1.8f, 14.6f }, { 0, 0.45f, 0 }, 40);   // low three-quarter view of the board
    for (int i = 0; i < N(); i++) T[i % 2].push_back(i);
    side = rand() % 2; turn = Cur(); Push();
    Banner("🔴 RED vs 🟡 YELLOW", 1800, "#ffd43b");
  }
  int RowFor(int c) { for (int r = 0; r < 6; r++) if (!g[c][r]) return r; return -1; }
  void Push() {
    // every phone shows the real board; on your turn you drag a disc over a column and let go
    std::string b; for (int r = 5; r >= 0; r--) { b += std::string(r < 5 ? "," : "") + "["; for (int c = 0; c < 7; c++) b += std::string(c ? "," : "") + std::to_string(g[c][r]); b += "]"; }
    std::string w; for (auto& q : line) w += std::string(w.empty() ? "" : ",") + "[" + std::to_string(q.first) + "," + std::to_string(5 - q.second) + "]";
    for (int i = 0; i < N(); i++) if (!P(i).bot) {
      bool my = i == turn && drop.c < 0 && win < 0; int mine = i % 2;
      std::string text = win >= 0 ? (win == mine ? "🏆 Your team wins!" : "Your team lost this one") : my ? "Your turn! Drag your disc over a column and let go" : drop.c >= 0 ? "Dropping…" : P(turn).name + "'s turn";
      Layout(i, "{\"name\":\"four\",\"my\":" + B(my) + ",\"side\":" + std::to_string(mine) + ",\"text\":" + Q(text) + ",\"board\":[" + b + "],\"win\":[" + w + "]}");
    }
  }
  // the winning four light up while the camera orbits the board
  void LineShot(int who) {
    Vector3 cen = { 0, Y0 + 0.55f + 2.5f * 0.95f, 0.3f };
    if (!line.empty()) { cen = {}; for (auto& q : line) cen = Vector3Add(cen, { (q.first - 3) * 0.95f, Y0 + 0.55f + q.second * 0.95f, 0.3f }); cen = Vector3Scale(cen, 1.f / line.size()); }
    cine::Shot s; s.kind = cine::WIN; s.at = cen; s.size = 2.4f; s.c = TC[who - 1]; s.floorY = F; s.elev = 0.1f; cine::Play(s);
  }
  void DebugShot(int k) override { if (k >= 7) { side = (k - 7) % 2; turn = Cur(); } else if (k == cine::WIN) LineShot(1); }
  bool Wins(int who, std::vector<std::pair<int, int>>* out = nullptr) {
    int d[4][2] = { { 1, 0 }, { 0, 1 }, { 1, 1 }, { 1, -1 } };
    for (int c = 0; c < 7; c++) for (int r = 0; r < 6; r++) for (auto& v : d) {
      int k = 0; for (; k < 4; k++) { int x = c + v[0] * k, y = r + v[1] * k; if (x < 0 || x > 6 || y < 0 || y > 5 || g[x][y] != who) break; }
      if (k == 4) { if (out) for (int q = 0; q < 4; q++) out->push_back({ c + v[0] * q, r + v[1] * q }); return true; }
    }
    return false;
  }
  void DropIn(int c) { int r = RowFor(c); if (r < 0 || drop.c >= 0 || win >= 0) return; drop = { c, r, side + 1, 7.4f, 0 }; Sfx("whoosh"); Push(); }
  void Act(int p, const std::string& k, int i, float) override { if (p == turn && k == "choice") DropIn(i); }
  int BotCol() {
    int me = side + 1, them = 2 - side;
    for (int who : { me, them }) for (int c = 0; c < 7; c++) { int r = RowFor(c); if (r < 0) continue; g[c][r] = who; bool w = Wins(who); g[c][r] = 0; if (w) return c; }
    int order[7] = { 3, 2, 4, 1, 5, 0, 6 };
    for (int c : order) { int r = RowFor(c); if (r < 0) continue; if (r + 1 < 6) { g[c][r] = me; g[c][r + 1] = them; bool bad = Wins(them); g[c][r] = g[c][r + 1] = 0; if (bad && frand(0, 1) < 0.85f) continue; } if (frand(0, 1) < 0.7f) return c; }
    for (int c : order) if (RowFor(c) >= 0) return c; return 0;
  }
  void Update(float dt) override {
    Tick(dt);
    if (over) { endT -= dt; if (endT <= 0 && !fin) { fin = true; auto o = Sorted([&](int i) { return win < 0 ? 0.f : (i % 2 == win ? 1.f : 0.f); }); FinishOrder(o, [&](int i) { return win < 0 ? std::string("draw") : i % 2 == win ? std::string("four in a row!") : std::string("so close"); }); } return; }
    if (drop.c >= 0) {
      drop.v += 30 * dt; drop.y -= drop.v * dt; float ty = 0.55f + drop.r * 0.95f;
      if (drop.y <= ty) {
        g[drop.c][drop.r] = drop.who; Sfx("pok"); fx.Burst({ (drop.c - 3) * 0.95f, ty + Y0, 0.4f }, 8, TC[drop.who - 1], 3, 0.4f, 0.1f);
        int who = drop.who; drop.c = -1;
        if (Wins(who, &line)) { win = who - 1; over = true; endT = 4; conf.Spawn({ 0, 6, 0 }, 160, 6, 6); Sfx("win"); Banner(who == 1 ? "🔴 RED TEAM WINS!" : "🟡 YELLOW TEAM WINS!", 2600, who == 1 ? "#ff4d6d" : "#ffd43b");
          LineShot(who);
          Push(); return; }
        bool full = true; for (int c = 0; c < 7; c++) full &= RowFor(c) < 0;
        if (full) { over = true; endT = 3; Banner("It's a draw!", 2000); Push(); return; }
        team[side]++; side = 1 - side; turn = Cur(); botT = frand(0.9f, 1.6f); timer = 25; Push();
      }
      return;
    }
    if (P(turn).bot) { botT -= dt; if (botT <= 0) DropIn(BotCol()); }
    else { timer -= dt; if (timer <= 0) DropIn(BotCol()); }
  }
  bool fin = false;
  void Draw() override {
    Room(false);
    if (!gridM.vertexCount) { gridM = BuildFourGrid(0.95f, 0.36f); discM = BuildFourDisc(); }
    Color blue = Hex(0x1f5fd1), blueD = Hex(0x174ba8);
    // legs: tall side posts on wide curved feet, and the release bar under the grid
    for (int s = -1; s <= 1; s += 2) {
      float x = s * (3.5f * 0.95f + 0.38f);
      lit.Draw(prim.cube, TRS({ x, (F + Y0 + 5.6f) / 2, 0 }, { 0, 0, 0 }, { 0.36f, Y0 + 5.6f - F, 0.42f }), blueD);
      lit.Draw(prim.cube, TRS({ x, F + 0.16f, 0 }, { 0, 0, 0 }, { 0.5f, 0.32f, 2.6f }), blueD);
      for (int q = -1; q <= 1; q += 2) lit.Draw(prim.sphere, TRS({ x, F + 0.16f, q * 1.3f }, { 0, 0, 0 }, { 0.5f, 0.32f, 0.5f }), blueD);
      lit.Draw(prim.sphere, TRS({ x, Y0 + 5.6f, 0 }, { 0, 0, 0 }, { 0.5f, 0.5f, 0.5f }), Hex(0xffd43b));
    }
    lit.Draw(prim.cube, TRS({ 0, Y0 - 0.12f, 0 }, { 0, 0, 0 }, { 7 * 0.95f + 0.5f, 0.16f, 0.7f }), Hex(0xffd43b));
    DrawProp(gridM, MatrixTranslate(0, Y0, 0), blue);
    for (int c = 0; c < 7; c++) for (int r = 0; r < 6; r++) {
      int v = g[c][r]; if (!v) continue;
      bool hl = false; for (auto& q : line) hl |= q.first == c && q.second == r;
      Color col = TC[v - 1]; if (hl) col = Shade(col, 0.25f + 0.25f * sinf(time * 10));
      DrawProp(discM, MatrixTranslate((c - 3) * 0.95f, Y0 + 0.55f + r * 0.95f, 0), col);
    }
    if (drop.c >= 0) DrawProp(discM, MatrixTranslate((drop.c - 3) * 0.95f, Y0 + drop.y, 0), TC[drop.who - 1]);
    // spare discs stacked beside the frame
    for (int s = 0; s < 2; s++) for (int k = 0; k < 5; k++) DrawProp(discM, MatrixMultiply(MatrixRotateX(PI / 2), MatrixTranslate((s ? 1 : -1) * 4.25f, F + 0.09f + k * 0.18f, 1.6f)), TC[s]);
    // the teams cheering on both sides
    for (int s = 0; s < 2; s++) for (size_t k = 0; k < T[s].size(); k++) {
      int i = T[s][k]; float x = (s ? 1 : -1) * (5.4f + (k % 2) * 1.6f), z = 0.8f + (k / 2) * 1.6f - 0.5f;
      bool hot = i == turn && !over; float ch = over && win == s ? fabsf(sinf(time * 6)) : 0;
      if (hot) lit.Draw(prim.cyl, TRS({ x, F + 0.03f, z }, { 0, 0, 0 }, { 1.5f, 0.03f, 1.5f }), Fade(TC[s], 0.8f));
      DrawToon({ x, F, z }, s ? -0.5f : 0.5f, P(i).color, time + i, hot ? fabsf(sinf(time * 5)) * 0.2f : over && win == s ? fabsf(sinf(time * 6 + i)) * 0.5f : 0, ch, 1.3f);
    }
    End3D();
    for (int s = 0; s < 2; s++) for (size_t k = 0; k < T[s].size(); k++) { int i = T[s][k]; float x = (s ? 1 : -1) * (5.4f + (k % 2) * 1.6f), z = 0.8f + (k / 2) * 1.6f - 0.5f; Vector2 sp = GetWorldToScreen({ x, F + 2.9f, z }, cam); Text(sp.x, sp.y, 20, i == turn ? Hex(0xffd43b) : WHITE, P(i).name, 0, TS_HEAVY); }
    if (!over) Title(std::string(side ? "🟡 " : "🔴 ") + P(turn).name + "'s turn", TC[side]);
    if (!over && !P(turn).bot && drop.c < 0) { char b[8]; snprintf(b, sizeof b, "%d", (int)ceilf(timer)); Text(1540, 50, 34, timer < 8 ? Hex(0xff4d6d) : WHITE, b); }
    Hint("Pick a column on your phone · line up FOUR in a row (any direction)");
  }
};

// =====================================================================================
// SNAKES & LADDERS — everyone rolls at the same time (fast!). Ladders go up, snakes slide you down.
// =====================================================================================
struct Snakes : Table {
  std::map<int, int> jump = { { 1, 38 }, { 4, 14 }, { 9, 31 }, { 21, 42 }, { 28, 84 }, { 36, 44 }, { 51, 67 }, { 71, 91 }, { 80, 100 },
                              { 16, 6 }, { 47, 26 }, { 49, 11 }, { 56, 53 }, { 62, 19 }, { 64, 60 }, { 87, 24 }, { 93, 73 }, { 95, 75 }, { 98, 78 } };
  struct Pawn { int sq = 0, roll = 0, steps = 0, dir = 1; float stepT = 0, slideT = -1, rollT = 0; int slideFrom = 0, slideTo = 0; bool rolled = false; int finish = 0; };
  std::vector<Pawn> pw; std::string phase = "roll"; float t = 10, botT = 1; int place = 0, round = 0; std::vector<int> finished;
  const float CS = 0.86f;
  Vector3 Sq(int n) { if (n <= 0) return { -4.9f * CS - 0.9f, 0.05f, 4.5f * CS }; int k = n - 1, row = k / 10, col = k % 10; if (row % 2) col = 9 - col; return { (col - 4.5f) * CS, 0.05f, (4.5f - row) * CS }; }
  void Start() override {
    InitCam({ 0, 13.6f, 8.4f }, { 0, -0.4f, 0.75f }, 46); felt = Hex(0xf1e3c6);
    pw.assign(N(), {}); NewRound();
    Banner("🐍 SNAKES & LADDERS 🪜", 1800, "#51cf66");
  }
  void NewRound() {
    round++; phase = "roll"; t = 9; botT = frand(0.6f, 1.4f);
    for (int i = 0; i < N(); i++) { pw[i].rolled = pw[i].finish > 0; pw[i].roll = 0; }
    for (int i = 0; i < N(); i++) if (!P(i).bot) Layout(i, pw[i].finish ? Msg("🏆", "You made it!", "Cheer on the others") : "{\"name\":\"button\",\"label\":\"🎲 ROLL!\",\"hint\":\"Everyone rolls at the same time\"}");
  }
  void Roll(int i) {
    Pawn& p = pw[i]; if (p.rolled || phase != "roll") return;
    p.rolled = true; p.roll = irand(1, 6); p.rollT = 0.9f; p.steps = p.roll; p.dir = 1; p.stepT = 0.9f; Sfx("dice");
    if (!P(i).bot) Layout(i, Msg("🎲", "You rolled a " + std::to_string(p.roll) + "!", "Watch your piece move"));
  }
  void Update(float dt) override {
    Tick(dt);
    for (auto& p : pw) p.rollT = fmaxf(0, p.rollT - dt);
    if (over) { endT -= dt; if (endT <= 0 && !fin) { fin = true; auto o = Sorted([&](int i) { return pw[i].finish ? 1000.f - pw[i].finish : (float)pw[i].sq; }); FinishOrder(o, [&](int i) { return pw[i].finish ? std::string("reached 100!") : "square " + std::to_string(pw[i].sq); }); } return; }
    for (int i = 0; i < N(); i++) if (!P(i).bot && P(i).a && !P(i).pa) Roll(i);
    if (phase == "roll") {
      t -= dt; botT -= dt;
      if (botT <= 0) { for (int i = 0; i < N(); i++) if (P(i).bot && !pw[i].rolled) { Roll(i); break; } botT = frand(0.2f, 0.6f); }
      bool all = true; for (auto& p : pw) all &= p.rolled;
      if (t <= 0) for (int i = 0; i < N(); i++) Roll(i);
      if (all || t <= 0) phase = "move";
    }
    bool busy = false;
    for (int i = 0; i < N(); i++) {
      Pawn& p = pw[i];
      if (p.rollT > 0) { busy = true; continue; }
      if (p.steps > 0) {
        busy = true; p.stepT -= dt;
        if (p.stepT <= 0) { p.stepT = 0.2f; if (p.sq + p.dir > 100) p.dir = -1; p.sq += p.dir; p.steps--; Sfx("tick"); }
        if (p.steps == 0) { auto j = jump.find(p.sq); if (j != jump.end()) { p.slideFrom = p.sq; p.slideTo = j->second; p.slideT = 0; Sfx(j->second > p.sq ? "zap" : "boom"); if (j->second > p.sq) Banner(P(i).name + " climbs a LADDER! 🪜", 1200, "#51cf66"); else Banner(P(i).name + " got bitten by a SNAKE! 🐍", 1200, "#ff6b6b"); } }
      } else if (p.slideT >= 0) {
        busy = true; p.slideT += dt / 1.1f;
        if (p.slideT >= 1) { p.sq = p.slideTo; p.slideT = -1; }
      } else if (p.sq == 100 && !p.finish) { p.finish = ++place; finished.push_back(i); fx.Burst(Sq(100), 30, P(i).color, 6, 0.8f, 0.15f); Sfx("win"); Banner(P(i).name + " WINS! 🏆", 2200, Hx(P(i).color).c_str()); }
    }
    if (phase == "move" && !busy) {
      if (place > 0) { over = true; endT = 3.5f; conf.Spawn({ 0, 5, 0 }, 160, 6, 6); return; }
      NewRound();
    }
  }
  bool fin = false;
  Vector3 PawnPos(int i) {
    Pawn& p = pw[i]; Vector3 v = Sq(p.sq);
    if (p.slideT >= 0) { Vector3 a = Sq(p.slideFrom), b = Sq(p.slideTo); float e = EaseOut(p.slideT); v = { Lerp(a.x, b.x, e), 0.05f + sinf(e * PI) * (p.slideTo > p.slideFrom ? 0.6f : 0.2f), Lerp(a.z, b.z, e) }; if (p.slideTo < p.slideFrom) v.x += sinf(e * PI * 3) * 0.35f; }
    int same = 0, idx = 0; for (int k = 0; k < N(); k++) if (pw[k].sq == p.sq && pw[k].slideT < 0) { if (k == i) idx = same; same++; }
    if (same > 1 && p.slideT < 0) { float a = idx * 2 * PI / same; v.x += cosf(a) * 0.22f; v.z += sinf(a) * 0.22f; }
    if (p.steps > 0) v.y += fabsf(sinf(p.stepT / 0.2f * PI)) * 0.35f;
    return v;
  }
  void Draw() override {
    Room();
    lit.Draw(prim.cube, TRS({ 0, 0.0f, 0 }, { 0, 0, 0 }, { 10 * CS + 0.4f, 0.1f, 10 * CS + 0.4f }), Hex(0x5c3d2e));
    const unsigned pal[] = { 0xffe066, 0x8ce99a, 0x74c0fc, 0xffa8a8, 0xd0bfff };
    for (int n = 1; n <= 100; n++) { Vector3 s = Sq(n); int k = n - 1; lit.Draw(prim.cube, TRS({ s.x, 0.06f, s.z }, { 0, 0, 0 }, { CS * 0.98f, 0.04f, CS * 0.98f }), Hex(pal[(k / 10 + k) % 5])); Label({ s.x - CS * 0.22f, 0.09f, s.z - CS * 0.24f }, CS * 0.5f, std::to_string(n), Hex(0x3b2d1d, 200), 0.7f); }
    for (auto& j : jump) {
      Vector3 a = Sq(j.first), b = Sq(j.second); a.y = b.y = 0.12f;
      float dx = b.x - a.x, dz = b.z - a.z, L = hypotf(dx, dz), ang = atan2f(dx, dz);
      if (j.second > j.first) {  // ladder: two rails + rungs
        float px = cosf(ang) * 0.18f, pz = -sinf(ang) * 0.18f;
        for (int s = -1; s <= 1; s += 2) lit.Draw(prim.cube, TRS({ (a.x + b.x) / 2 + px * s, 0.2f, (a.z + b.z) / 2 + pz * s }, { 0, ang, 0 }, { 0.07f, 0.07f, L }), Hex(0x8d5524));
        for (float u = 0.12f; u < L; u += 0.34f) lit.Draw(prim.cube, TRS({ a.x + dx * u / L, 0.22f, a.z + dz * u / L }, { 0, ang, 0 }, { 0.42f, 0.05f, 0.06f }), Hex(0xc68642));
      } else {  // snake: a wiggly chain of balls, fat head at the top
        int segs = (int)(L / 0.16f) + 4; unsigned sc = (j.first * 2654435761u) % 4; static const unsigned SC4[] = { 0x2f9e44, 0xe8590c, 0x7048e8, 0xd6336c }; Color body = Hex(SC4[sc]);
        for (int k = 0; k <= segs; k++) {
          float u = (float)k / segs, w = sinf(u * PI * 3 + time * 2) * 0.35f * sinf(u * PI);
          Vector3 p = { b.x + (a.x - b.x) * u + cosf(ang) * w, 0.16f, b.z + (a.z - b.z) * u - sinf(ang) * w };
          float r = 0.12f + 0.12f * u; lit.Draw(prim.sphere, TRS(p, { 0, 0, 0 }, { r * 2, r * 1.4f, r * 2 }), k % 3 ? body : Shade(body, 0.35f));
        }
        lit.Draw(prim.sphere, TRS({ a.x, 0.22f, a.z }, { 0, 0, 0 }, { 0.5f, 0.32f, 0.5f }), body);
        for (int s = -1; s <= 1; s += 2) lit.Draw(prim.sphere, TRS({ a.x + s * 0.12f, 0.36f, a.z - 0.1f }, { 0, 0, 0 }, { 0.12f, 0.12f, 0.12f }), WHITE);
      }
    }
    for (int i = 0; i < N(); i++) { Vector3 v = PawnPos(i); DrawToon(v, 0, P(i).color, time + i, 0, pw[i].finish ? 1 : 0, 0.58f); }
    // big dice row along the front edge
    for (int i = 0; i < N(); i++) if (pw[i].roll) Die({ (i - (N() - 1) / 2.f) * 1.15f, 0.05f, 5.45f }, pw[i].roll, pw[i].rollT, 0.6f, WHITE, P(i).color);
    End3D();
    for (int i = 0; i < N(); i++) { Vector3 v = PawnPos(i); Vector2 sp = GetWorldToScreen({ v.x, v.y + 1.25f, v.z }, cam); hud::Tag(sp.x, sp.y - 8, P(i).color, P(i).name, 22); }
    Board("🐍 RACE TO 100", Sorted([&](int i) { return pw[i].finish ? 1000.f - pw[i].finish : (float)pw[i].sq; }), [&](int i) { return pw[i].finish ? std::string("🏆") : std::to_string(pw[i].sq); });
    if (phase == "roll") { int left = 0; for (auto& p : pw) left += !p.rolled; Title("🎲 EVERYONE ROLL!"); Hint(std::to_string(left) + " still to roll · flick the dice on your phone", Hex(0xffd43b)); }
    else Hint("🪜 Ladders take you up · 🐍 snakes slide you down · land exactly on 100");
  }
};

// =====================================================================================
// BLACKJACK — everyone plays the dealer at once. Get closer to 21 than the dealer without going over.
// =====================================================================================
struct Blackjack : Table {
  int cur = -1;   // one player at a time: whoever is deciding right now; the TV is their eyes
  int ViewSeat() override { return phase == "play" && cur >= 0 ? cur : -1; }
  bool Focus(Vector3& p, float& w) override { if (phase == "dealer" || phase == "settle") { p = { 0, 0.3f, 2.9f }; w = 0.8f; return true; } return false; }
  std::vector<Card> deck; std::vector<std::vector<Card>> hand; std::vector<Card> dealer;
  std::vector<int> pts, state, delta, dbl;  // state: 0 playing, 1 stand, 2 bust, 3 blackjack · dbl: 1 = doubled down (one more card, the stake counts twice)
  std::string phase = "deal"; float t = 0, botT = 0; int hno = 0, HANDS = 6; bool holeUp = false;
  int Val(const std::vector<Card>& h) { int v = 0, aces = 0; for (auto& c : h) { int r = c.r + 1; if (r == 1) { aces++; v += 11; } else v += r > 10 ? 10 : r; } while (v > 21 && aces) { v -= 10; aces--; } return v; }
  Card Pull() { if (deck.empty()) deck = Deck(2); Card c = deck.back(); deck.pop_back(); return c; }
  void Start() override { TableCam(); felt = Hex(0x0b6e4f); deck = Deck(2); pts.assign(N(), 0); delta.assign(N(), 0); Banner("♠️ BLACKJACK ♥️", 1600, "#ffd43b"); Layout(-1, "{\"name\":\"panel\"}"); Deal(); }
  void Deal() {
    hno++; phase = "deal"; t = 1.4f; holeUp = false;
    if ((int)deck.size() < 40) deck = Deck(2);
    hand.assign(N(), {}); state.assign(N(), 0); delta.assign(N(), 0); dbl.assign(N(), 0); dealer.clear();
    for (int k = 0; k < 2; k++) { for (int i = 0; i < N(); i++) hand[i].push_back(Pull()); dealer.push_back(Pull()); }
    for (int i = 0; i < N(); i++) if (Val(hand[i]) == 21) state[i] = 3;
    for (int i = 0; i < N(); i++) if (state[i] == 3) { Vector3 st = Seat(i); cine::Shot s; s.kind = cine::STRIKE; s.at = Front(i, 4.2f, 0.4f); s.size = 1.3f; s.c = Hex(0xffd43b); s.caption = "BLACKJACK!"; s.sub = P(i).name; s.elev = 0.75f; s.floorY = 0.02f; s.face = { -st.x, 0, -st.z }; s.yaw = -1.0f; cine::Play(s); break; }
    Sfx("card"); SyncAll();
  }
  std::string Panel(int i) {
    std::string cs; for (size_t k = 0; k < hand[i].size(); k++) { Card c = hand[i][k]; cs += std::string(k ? "," : "") + "{\"label\":" + Q(RANK[c.r]) + ",\"icon\":" + Q(SUIT[c.s]) + ",\"color\":" + Q(Red(c) ? "#e03131" : "#343a40") + ",\"ok\":false}"; }
    int v = Val(hand[i]); bool my = phase == "play" && state[i] == 0 && i == cur;
    std::string sub = state[i] == 2 ? "💥 BUST! Over 21" : state[i] == 3 ? "⭐ BLACKJACK!" : state[i] == 1 ? "✋ Standing — waiting for the dealer" : phase == "play" ? (my ? (hand[i].size() == 2 ? "Your turn! HIT · STAND · or DOUBLE: one more card, win or lose twice as much" : "Your turn! HIT = another card · STAND = keep it") : (cur >= 0 ? P(cur).name + " is playing…" : "")) : "";
    if (phase == "settle") sub = delta[i] > 0 ? "🎉 You beat the dealer! +" + std::to_string(delta[i]) : delta[i] < 0 ? "😬 Dealer wins this one" : "🤝 Push — tie";
    std::string btn = my ? "{\"id\":\"hit\",\"label\":\"👆 HIT\",\"color\":\"#2f9e44\",\"big\":true},{\"id\":\"stand\",\"label\":\"✋ STAND\",\"color\":\"#e03131\",\"big\":true}" + std::string(hand[i].size() == 2 ? ",{\"id\":\"double\",\"label\":\"✌️ DOUBLE ×2\",\"color\":\"#f08c00\",\"big\":true}" : "") : "";
    return "{\"t\":\"panel\",\"title\":" + Q("Hand " + std::to_string(hno) + " / " + std::to_string(HANDS) + "  ·  " + std::to_string(pts[i]) + " pts") + ",\"info\":" + Q("Total: " + std::to_string(v)) + ",\"sub\":" + Q(sub) + ",\"myTurn\":" + B(my) + ",\"cards\":[" + cs + "],\"buttons\":[" + btn + "]}";
  }
  void SyncAll() { for (int i = 0; i < N(); i++) if (!P(i).bot) Send(i, Panel(i)); }
  // the next player who still has a decision to make (a natural 21 or a bust is already decided); nobody left = the dealer plays
  void NextActor() {
    int from = cur < 0 ? 0 : cur + 1; cur = -1;
    for (int k = from; k < N(); k++) if (state[k] == 0) { cur = k; break; }
    if (cur < 0) { phase = "dealer"; t = 1.0f; holeUp = true; Sfx("card"); SyncAll(); return; }
    t = 15; botT = frand(1.0f, 1.6f); Sfx("whoosh"); if (!P(cur).bot) Buzz(cur, 200); SyncAll();
  }
  void Act(int p, const std::string& k, int, float) override {
    if (phase != "play" || p != cur || state[p] != 0) return;
    if (k == "btn:hit") { hand[p].push_back(Pull()); Sfx("card"); t = 15; int v = Val(hand[p]); if (v > 21) { state[p] = 2; Sfx("buzz"); Buzz(p, 300); } else if (v == 21) state[p] = 1; }
    else if (k == "btn:stand") state[p] = 1;
    else if (k == "btn:double" && hand[p].size() == 2) {   // double down: exactly one more card, and the hand then counts twice (win or lose)
      dbl[p] = 1; hand[p].push_back(Pull()); Sfx("card"); int v = Val(hand[p]); state[p] = v > 21 ? 2 : 1;
      if (v > 21) { Sfx("buzz"); Buzz(p, 300); } else Sfx("ding");
      Banner("✌️ " + P(p).name + " DOUBLES DOWN!", 1300, "#ffd43b");
    }
    else return;
    if (state[p] != 0) NextActor(); else SyncAll();
  }
  void Update(float dt) override {
    Tick(dt); t -= dt;
    if (over) { endT -= dt; if (endT <= 0 && !fin) { fin = true; FinishOrder(Sorted([&](int i) { return (float)pts[i]; }), [&](int i) { return std::to_string(pts[i]) + " pts"; }); } return; }
    if (phase == "deal" && t <= 0) { phase = "play"; NextActor(); }
    else if (phase == "play") {
      if (cur >= 0 && P(cur).bot) { botT -= dt; if (botT <= 0) { botT = frand(0.9f, 1.5f); int v = Val(hand[cur]), up = dealer[0].r + 1; bool weak = up >= 2 && up <= 6; if (hand[cur].size() == 2 && (v == 11 || (v == 10 && up < 10)) && frand(0, 1) < 0.7f) { Act(cur, "btn:double", 0, 0); return; } Act(cur, (v < 12 || (v < 17 && !weak) || (v < 13 && weak)) ? "btn:hit" : "btn:stand", 0, 0); } }
      else if (cur >= 0 && t <= 0) { state[cur] = 1; Send(cur, Panel(cur)); NextActor(); }   // out of time: stand
    } else if (phase == "dealer" && t <= 0) {
      bool anyAlive = false; for (int s : state) anyAlive |= s == 1;
      if (anyAlive && Val(dealer) < 17) { dealer.push_back(Pull()); Sfx("card"); t = 0.9f; }
      else {
        int d = Val(dealer); bool dbj = d == 21 && dealer.size() == 2;
        for (int i = 0; i < N(); i++) {
          int v = Val(hand[i]);
          if (state[i] == 2) delta[i] = -10; else if (state[i] == 3) delta[i] = dbj ? 0 : 15;
          else if (d > 21 || v > d) delta[i] = 10; else if (v < d) delta[i] = -10; else delta[i] = 0;
          if (dbl[i] && state[i] != 3) delta[i] *= 2;
          pts[i] += delta[i];
          if (delta[i] > 0) fx.Burst(Front(i, 4.2f, 0.5f), 18, Hex(0xffd43b), 4, 0.7f, 0.1f);
        }
        if (d > 21) Banner("Dealer BUSTS! 💥", 1600, "#51cf66"); else Banner("Dealer has " + std::to_string(d), 1400);
        phase = "settle"; t = 3.2f; Sfx("ding"); SyncAll();
      }
    } else if (phase == "settle" && t <= 0) { if (hno >= HANDS) { over = true; endT = 2.5f; int b = Sorted([&](int i) { return (float)pts[i]; })[0]; Celebrate(b); } else Deal(); }
  }
  bool fin = false;
  void Draw() override {
    Room(); Seats([&](int i) { return phase == "play" && i == cur; }, [&](int i) { return phase == "settle" && delta[i] > 0 ? fabsf(sinf(time * 6)) : 0.f; });
    // dealer at the near side, facing everyone
    DrawToon({ 0, -1.9f, 7.6f }, PI, Hex(0x212529), time, 0, 0, 1.3f);
    for (size_t k = 0; k < dealer.size(); k++) CardAt({ (k - (dealer.size() - 1) / 2.f) * 1.05f, 0.05f, 2.9f }, 0, k != 1 || holeUp, dealer[k], 1.25f);
    for (int i = 0; i < N(); i++) { Vector3 f = Front(i, 4.4f); float a = SeatA(i); for (size_t k = 0; k < hand[i].size(); k++) { float o = (k - (hand[i].size() - 1) / 2.f) * 0.55f; CardAt({ f.x - sinf(a) * o * 1.25f, 0.05f + k * 0.01f, f.z + cosf(a) * o * 0.98f }, -a - PI / 2, true, hand[i][k], 1.05f); } }
    End3D();
    SeatNames([&](int i) { int v = Val(hand[i]); return state[i] == 2 ? std::string("💥 BUST") + (dbl[i] ? " ×2" : "") : state[i] == 3 ? std::string("⭐ 21!") : phase == "settle" ? (delta[i] > 0 ? "+" + std::to_string(delta[i]) : delta[i] < 0 ? std::to_string(delta[i]) : std::string("push")) : std::to_string(v) + (dbl[i] ? " ×2" : ""); });
    Vector2 dp = GetWorldToScreen({ 0, 0.05f, 5.2f }, cam);
    Text(dp.x, dp.y + 6, 26, Hex(0xffd43b), holeUp ? "Dealer: " + std::to_string(Val(dealer)) : std::string("Dealer"), 0, TS_HEAVY);
    Board("🃏 POINTS", Sorted([&](int i) { return (float)pts[i]; }), [&](int i) { return std::to_string(pts[i]); });
    Title("Hand " + std::to_string(hno) + " of " + std::to_string(HANDS));
    if (phase == "play") { char b[8]; snprintf(b, sizeof b, "%d", (int)ceilf(fmaxf(0, t))); Text(1540, 50, 34, t < 6 ? Hex(0xff4d6d) : WHITE, b); Hint(cur >= 0 ? P(cur).name + ": HIT for another card or STAND · over 21 is a bust" : "", Hex(0xffd43b)); }
    else Hint("Beat the dealer: +10 · Blackjack: +15 · lose: −10 · DOUBLE DOWN: ×2");
  }
};

// =====================================================================================
// BINGO — the cage spins, balls get called, mark your card on your phone, shout BINGO!
// =====================================================================================
const unsigned BALL[] = { 0x4dabf7, 0xff6b6b, 0xffffff, 0x51cf66, 0xcc5de8 }, LCOL[] = { 0x4dabf7, 0xff6b6b, 0xfab005, 0x51cf66, 0xcc5de8 };
struct Bingo : Table {
  std::vector<int> bag, called; std::vector<std::vector<int>> card; std::vector<std::vector<bool>> mark; std::vector<int> winners; std::vector<float> lock, botLag;
  float callT = 3, ballT = 0; int last = 0;
  void Start() override {
    InitCam({ 0.6f, 7.0f, 12.5f }, { 2.4f, 1.4f, 0 }, 44);
    for (int n = 1; n <= 75; n++) bag.push_back(n); Shuffle(bag);
    card.assign(N(), std::vector<int>(25)); mark.assign(N(), std::vector<bool>(25)); lock.assign(N(), 0); botLag.assign(N(), 0);
    for (int i = 0; i < N(); i++) { for (int c = 0; c < 5; c++) { std::vector<int> v; for (int k = 1; k <= 15; k++) v.push_back(c * 15 + k); Shuffle(v); for (int r = 0; r < 5; r++) card[i][r * 5 + c] = v[r]; } card[i][12] = 0; mark[i][12] = true; }
    PushAll(); Banner("🎱 BINGO!", 1600, "#cc5de8");
  }
  bool Called(int n) { return std::find(called.begin(), called.end(), n) != called.end(); }
  bool Won(int i) { return std::find(winners.begin(), winners.end(), i) != winners.end(); }
  void Push(int i) {
    std::string c, m, cl; for (int k = 0; k < 25; k++) { c += std::string(k ? "," : "") + std::to_string(card[i][k]); if (mark[i][k]) m += std::string(m.empty() ? "" : ",") + std::to_string(k); }
    for (size_t k = called.size() > 12 ? called.size() - 12 : 0; k < called.size(); k++) cl += std::string(cl.empty() ? "" : ",") + std::to_string(called[k]);
    Layout(i, "{\"name\":\"bingo\",\"card\":[" + c + "],\"marks\":[" + m + "],\"called\":[" + cl + "],\"last\":" + (last ? std::to_string(last) : "null") + ",\"won\":" + B(Won(i)) + ",\"locked\":" + B(lock[i] > time) + "}");
  }
  void PushAll() { for (int i = 0; i < N(); i++) if (!P(i).bot) Push(i); }
  bool HasBingo(int i) {
    auto ok = [&](int k) { return k == 12 || (mark[i][k] && Called(card[i][k])); };
    for (int r = 0; r < 5; r++) { bool a = true, b = true; for (int c = 0; c < 5; c++) { a &= ok(r * 5 + c); b &= ok(c * 5 + r); } if (a || b) return true; }
    bool d1 = true, d2 = true; for (int k = 0; k < 5; k++) { d1 &= ok(k * 6); d2 &= ok(4 + k * 4); } return d1 || d2;
  }
  int Good(int i) { int n = 0; for (int k = 0; k < 25; k++) n += k != 12 && mark[i][k] && Called(card[i][k]); return n; }
  void Act(int p, const std::string& k, int i, float) override {
    if (over || Won(p)) return;
    if (k == "mark") { if (i < 0 || i > 24 || i == 12) return; mark[p][i] = !mark[p][i]; if (!P(p).bot) Push(p); }
    else if (k == "bingo") {
      if (lock[p] > time) return;
      if (HasBingo(p)) { winners.push_back(p); Banner("🎉 " + P(p).name + ": BINGO!", 2400, Hx(P(p).color).c_str()); Sfx("win"); conf.Spawn({ -3, 5, 0 }, 120, 5, 5); if ((int)winners.size() >= std::min(3, std::max(1, N() - 1))) { over = true; endT = 3.5f; } }
      else { lock[p] = time + 5; Banner("❌ FALSE BINGO, " + P(p).name + "!", 1800, "#ff4d6d"); Sfx("buzz"); Buzz(p, 500); }
      if (!P(p).bot) Push(p);
    }
  }
  void Update(float dt) override {
    Tick(dt); ballT = fmaxf(0, ballT - dt);
    if (over) { endT -= dt; if (endT <= 0 && !fin) { fin = true; FinishOrder(Sorted([&](int i) { auto w = std::find(winners.begin(), winners.end(), i); return w != winners.end() ? 100.f - (w - winners.begin()) : (float)Good(i) / 30; }), [&](int i) { return Won(i) ? std::string("BINGO!") : std::to_string(Good(i)) + " marked"; }); } return; }
    callT -= dt;
    if (callT <= 0) {
      if (bag.empty()) { over = true; endT = 2; return; }
      last = bag.back(); bag.pop_back(); called.push_back(last); ballT = 1; callT = 4.2f; Sfx("pok");
      const char* L = "BINGO"; Banner(std::string(1, L[(last - 1) / 15]) + " " + std::to_string(last), 1800, "#ffffff");
      for (int i = 0; i < N(); i++) botLag[i] = frand(0.4f, 2.2f);
      PushAll();
    }
    for (int i = 0; i < N(); i++) if (P(i).bot && !Won(i) && botLag[i] > 0) {
      botLag[i] -= dt;
      if (botLag[i] <= 0) { for (int k = 0; k < 25; k++) if (card[i][k] == last && frand(0, 1) < 0.92f) mark[i][k] = true; if (HasBingo(i)) Act(i, "bingo", 0, 0); }
    }
  }
  bool fin = false;
  void Draw() override {
    Room();
    // the cage: three spinning rings with balls tumbling inside
    Vector3 c = { -1.2f, 2.4f, 0.5f }; float spin = time * 1.7f;
    for (int ring = 0; ring < 3; ring++) for (int k = 0; k < 28; k++) {
      float a = k * 2 * PI / 28; Vector3 p = { cosf(a) * 1.7f, sinf(a) * 1.7f, 0 };
      float ry = spin + ring * PI / 3; Vector3 q = { p.x * cosf(ry), p.y, p.x * sinf(ry) };
      lit.Draw(prim.sphere, TRS({ c.x + q.x, c.y + q.y, c.z + q.z }, { 0, 0, 0 }, { 0.13f, 0.13f, 0.13f }), Hex(0xffd43b));
    }
    for (int k = 0; k < 14; k++) { float a = time * (2 + k * 0.3f) + k, b = time * 1.3f + k * 2; lit.Draw(prim.sphere, TRS({ c.x + cosf(a) * 1.0f * sinf(b), c.y - 0.6f + fabsf(sinf(a * 1.3f)) * 1.2f, c.z + sinf(a) * 0.9f }, { 0, 0, 0 }, { 0.38f, 0.38f, 0.38f }), Hex(BALL[k % 5])); }
    lit.Draw(prim.cyl, TRS({ c.x, 0, c.z }, { 0, 0, 0 }, { 0.25f, 0.7f, 0.25f }), Hex(0x868e96));
    // the called ball rolls out to the front
    if (last) { float e = 1 - ballT; Vector3 b = { Lerp(c.x, -1.2f, e), Lerp(c.y, 0.5f, EaseOut(e)), Lerp(c.z, 3.3f, e) }; lit.Draw(prim.sphere, TRS(b, { 0, 0, 0 }, { 0.95f, 0.95f, 0.95f }), Hex(0xfdfcf7)); lit.Draw(prim.cyl, TRS({ b.x, b.y - 0.04f, b.z + 0.44f }, { PI / 2.3f, 0, 0 }, { 0.6f, 0.02f, 0.6f }), Hex(LCOL[(last - 1) / 15])); }
    End3D();
    if (last) { Vector2 sp = GetWorldToScreen({ Lerp(c.x, -1.2f, 1 - ballT), 0.9f, Lerp(c.z, 3.3f, 1 - ballT) }, cam); const char* L = "BINGO"; Text(sp.x, sp.y + 8, 30, Hex(0x1a0533), std::string(1, L[(last - 1) / 15]) + std::to_string(last), 0, TS_DISPLAY | TS_PLAIN); }
    // called-number board
    hud::Panel(760, 120, 800, 470, 28);
    const unsigned col[] = { 0x4dabf7, 0xff6b6b, 0xfab005, 0x51cf66, 0xcc5de8 };
    for (int L = 0; L < 5; L++) {
      Text(800, 172 + L * 88, 40, Hex(col[L]), std::string(1, "BINGO"[L]), 0, TS_DISPLAY);
      for (int k = 0; k < 15; k++) { int n = L * 15 + k + 1; bool on = Called(n); float x = 850 + k * 46.5f, y = 172 + L * 88; DrawCircle((int)x, (int)y, 20, on ? Hex(col[L]) : Hex(0xffffff, 20)); if (n == last) DrawCircleLines((int)x, (int)y, 23, WHITE); Text(x, y, 17, on ? Hex(0x1a0533) : Hex(0xffffff, 90), std::to_string(n), 0, TS_HEAVY | TS_PLAIN); }
    }
    Board("🎱 PLAYERS", Sorted([&](int i) { auto w = std::find(winners.begin(), winners.end(), i); return w != winners.end() ? 100.f - (w - winners.begin()) : (float)Good(i); }), [&](int i) { return Won(i) ? std::string("🏆") : std::to_string(Good(i)) + " ✔"; });
    Title(std::to_string(called.size()) + " balls called");
    Hint("Tap the called numbers on your phone · 5 in a row? Press BINGO!");
  }
};

// =====================================================================================
// DICE POKER — roll 5 dice, hold the good ones, reroll twice. Best hand wins the round.
// =====================================================================================
const char* HANDS[] = { "Nothing", "Pair", "Two Pair", "Three of a Kind", "Small Straight", "Big Straight", "Full House", "Four of a Kind", "FIVE OF A KIND!" };
int DiceHand(const std::vector<int>& d, int* score = nullptr) {
  int cnt[7] = {}; int sum = 0; for (int v : d) { cnt[v]++; sum += v; }
  std::vector<int> c(cnt + 1, cnt + 7); std::sort(c.rbegin(), c.rend());
  auto has = [&](int a, int b) { for (int v = a; v <= b; v++) if (!cnt[v]) return false; return true; };
  int h = 0;
  if (c[0] == 5) h = 8; else if (c[0] == 4) h = 7; else if (c[0] == 3 && c[1] == 2) h = 6; else if (has(1, 5) || has(2, 6)) h = 5;
  else if (has(1, 4) || has(2, 5) || has(3, 6)) h = 4; else if (c[0] == 3) h = 3; else if (c[0] == 2 && c[1] == 2) h = 2; else if (c[0] == 2) h = 1;
  if (score) *score = h * 100 + sum; return h;
}
struct DicePoker : Table {
  struct S { std::vector<int> d = std::vector<int>(5), held = std::vector<int>(5); int left = 2; bool done = false; float shake = 1, botT = 2; };
  std::vector<S> st; std::vector<int> pts; std::string phase = "roll"; float t = 25; int round = 0, R = 5;
  void Start() override { TableCam(); felt = Hex(0x862e9c); pts.assign(N(), 0); Next(); Banner("🎲 DICE POKER", 1600, "#ff8787"); }
  void Next() {
    round++; if (round > R) { over = true; endT = 2.5f; Celebrate(Sorted([&](int i) { return (float)pts[i]; })[0]); return; }
    phase = "roll"; t = 25; st.assign(N(), S());
    for (auto& s : st) { for (auto& v : s.d) v = irand(1, 6); s.botT = frand(1.5f, 3); }
    Sfx("whoosh"); for (int i = 0; i < N(); i++) Push(i);
  }
  void Push(int i) {
    if (P(i).bot) return; S& s = st[i]; std::string d, h;
    for (int k = 0; k < 5; k++) { d += std::string(k ? "," : "") + std::to_string(s.d[k]); h += std::string(k ? "," : "") + std::to_string(s.held[k]); }
    Layout(i, "{\"name\":\"dice\",\"dice\":[" + d + "],\"held\":[" + h + "],\"left\":" + std::to_string(s.left) + ",\"done\":" + B(s.done) + ",\"hand\":" + Q(HANDS[DiceHand(s.d)]) + ",\"round\":" + std::to_string(round) + ",\"rounds\":" + std::to_string(R) + "}");
  }
  void Act(int p, const std::string& k, int i, float) override {
    if (phase != "roll" || p < 0 || p >= N()) return; S& s = st[p]; if (s.done) return;
    if (k == "hold" && i >= 0 && i < 5) s.held[i] = !s.held[i];
    else if (k == "roll" && s.left > 0) { for (int q = 0; q < 5; q++) if (!s.held[q]) s.d[q] = irand(1, 6); s.left--; s.shake = 1; Sfx("dice"); if (!s.left) s.done = true; }
    else if (k == "done") s.done = true;
    Push(p);
    bool all = true; for (auto& x : st) all &= x.done; if (all) t = fminf(t, 0.6f);
  }
  void BotPlay(int p) {
    S& s = st[p]; int cnt[7] = {}; for (int v : s.d) cnt[v]++;
    int best = 6; for (int v = 6; v >= 1; v--) if (cnt[v] > cnt[best]) best = v;
    if (DiceHand(s.d) >= 5) { Act(p, "done", 0, 0); return; }
    for (int q = 0; q < 5; q++) s.held[q] = s.d[q] == best && cnt[best] > 1;
    Act(p, "roll", 0, 0);
  }
  void Update(float dt) override {
    Tick(dt); t -= dt; for (auto& s : st) s.shake = fmaxf(0, s.shake - dt * 2.2f);
    if (over) { endT -= dt; if (endT <= 0 && !fin) { fin = true; FinishOrder(Sorted([&](int i) { return (float)pts[i]; }), [&](int i) { return std::to_string(pts[i]) + " pts"; }); } return; }
    if (phase == "roll") {
      for (int i = 0; i < N(); i++) if (P(i).bot && !st[i].done) { st[i].botT -= dt; if (st[i].botT <= 0) { st[i].botT = frand(1, 2.2f); BotPlay(i); } }
      if (t <= 0) {
        phase = "show"; t = 4.5f;
        auto o = Sorted([&](int i) { int sc; DiceHand(st[i].d, &sc); return (float)sc; });
        for (size_t k = 0; k < o.size(); k++) pts[o[k]] += N() - 1 - (int)k;
        Banner(P(o[0]).name + ": " + HANDS[DiceHand(st[o[0]].d)], 2200, Hx(P(o[0]).color).c_str()); Sfx("ding"); fx.Burst(Front(o[0], 4.3f, 0.6f), 30, Hex(0xffd43b), 5, 0.8f, 0.12f);
        for (int i = 0; i < N(); i++) if (!P(i).bot) Layout(i, Msg(i == o[0] ? "🏆" : "🎲", HANDS[DiceHand(st[i].d)], i == o[0] ? "You won the round!" : P(o[0]).name + " won this round"));
      }
    } else if (t <= 0) Next();
  }
  bool fin = false;
  void Draw() override {
    Room(); Seats([&](int i) { return phase == "roll" && !st[i].done; });
    float sp = N() > 4 ? 0.82f : 1.0f, ds = N() > 4 ? 0.66f : 0.8f;
    for (int i = 0; i < N(); i++) { Vector3 f = Front(i, 3.5f); float a = SeatA(i); for (int k = 0; k < 5; k++) { float o = (k - 2) * sp; Die({ f.x - sinf(a) * o, 0.02f, f.z + cosf(a) * o * 0.78f }, st[i].d[k], st[i].held[k] ? 0 : st[i].shake, ds, WHITE, st[i].held[k] ? Hex(0xffd43b) : BLANK); } }
    End3D();
    SeatNames([&](int i) { return std::string(HANDS[DiceHand(st[i].d)]); });
    Board("🎲 POINTS", Sorted([&](int i) { return (float)pts[i]; }), [&](int i) { return std::to_string(pts[i]); });
    Title("Round " + std::to_string(std::min(round, R)) + " of " + std::to_string(R));
    if (phase == "roll") { char b[8]; snprintf(b, sizeof b, "%d", (int)ceilf(fmaxf(0, t))); Text(1540, 50, 34, t < 6 ? Hex(0xff4d6d) : WHITE, b); }
    Hint("Tap dice to HOLD · ROLL up to 2 more times · five of a kind beats everything");
  }
};

// =====================================================================================
// MEMORY — flip two cards, find the pairs. Match = point + go again.
// =====================================================================================
struct Memory : Table {
  struct C { std::string e; bool gone = false; float up = 0; int who = -1; };
  std::vector<C> cs; std::vector<int> sel, pts; std::vector<std::vector<int>> seen; float waitT = 0, botT = 1.5f, timer = 20; int pairsLeft = 10;
  void Start() override {
    InitCam({ 0, 13.8f, 9.6f }, { 0, -0.6f, 0.8f }, 44); felt = Hex(0x1864ab);
    const char* E[] = { "🐶", "🐱", "🦊", "🐸", "🐵", "🦄", "🐙", "🦁", "🐧", "🐢", "🍕", "🚀" };
    std::vector<int> pick; for (int i = 0; i < 12; i++) pick.push_back(i); Shuffle(pick);
    for (int k = 0; k < 10; k++) for (int c = 0; c < 2; c++) { C x; x.e = E[pick[k]]; cs.push_back(x); }
    Shuffle(cs); pts.assign(N(), 0); seen.assign(N(), {}); turn = rand() % N(); Push(); Banner("🧠 MEMORY MATCH", 1600, "#74c0fc");
  }
  Vector3 Pos(int k) { int c = k % 5, r = k / 5; return { (c - 2) * 1.55f, 0.06f, (r - 1.5f) * 1.85f + 0.5f }; }
  void Push() {
    std::vector<std::string> L; std::vector<bool> off; for (int k = 0; k < 20; k++) { L.push_back(std::to_string(k + 1)); off.push_back(cs[k].gone || std::find(sel.begin(), sel.end(), k) != sel.end()); }
    for (int i = 0; i < N(); i++) if (!P(i).bot) Layout(i, i == turn && waitT <= 0 && !over ? Choice(sel.empty() ? "Your turn! Pick a card" : "Now pick the 2nd card", L, off, 5, false, std::vector<std::string>(L.size(), "#2a3560")) : Msg("🧠", over ? "All pairs found!" : P(turn).name + "'s turn", "Watch the cards and remember!"));
  }
  void Flip(int k) {
    if (over || waitT > 0 || k < 0 || k >= 20 || cs[k].gone || std::find(sel.begin(), sel.end(), k) != sel.end() || sel.size() >= 2) return;
    sel.push_back(k); Sfx("card");
    for (int i = 0; i < N(); i++) if (P(i).bot && frand(0, 1) < 0.45f) seen[i].push_back(k);
    if (sel.size() == 2) { waitT = 1.6f; timer = 20; } Push();
  }
  void Act(int p, const std::string& k, int i, float) override { if (p == turn && k == "choice") Flip(i); }
  int BotPick() {
    auto& m = seen[turn]; auto known = [&](int k) { return std::find(m.begin(), m.end(), k) != m.end(); };
    if (sel.size() == 1) { for (int k = 0; k < 20; k++) if (k != sel[0] && !cs[k].gone && known(k) && cs[k].e == cs[sel[0]].e) return k; }
    else for (int a = 0; a < 20; a++) for (int b = a + 1; b < 20; b++) if (!cs[a].gone && !cs[b].gone && known(a) && known(b) && cs[a].e == cs[b].e) return a;
    std::vector<int> fresh, any; for (int k = 0; k < 20; k++) if (!cs[k].gone && (sel.empty() || k != sel[0])) { any.push_back(k); if (!known(k)) fresh.push_back(k); }
    auto& v = fresh.empty() ? any : fresh; return v[rand() % v.size()];
  }
  void Update(float dt) override {
    Tick(dt);
    for (int k = 0; k < 20; k++) { bool up = cs[k].gone || std::find(sel.begin(), sel.end(), k) != sel.end(); cs[k].up += ((up ? 1.f : 0.f) - cs[k].up) * fminf(1, dt * 9); }
    if (over) { endT -= dt; if (endT <= 0 && !fin) { fin = true; FinishOrder(Sorted([&](int i) { return (float)pts[i]; }), [&](int i) { return std::to_string(pts[i]) + " pairs"; }); } return; }
    if (waitT > 0) {
      waitT -= dt;
      if (waitT <= 0) {
        C &a = cs[sel[0]], &b = cs[sel[1]];
        if (a.e == b.e) { a.gone = b.gone = true; a.who = b.who = turn; pts[turn]++; pairsLeft--; Sfx("ding"); fx.Burst(Pos(sel[0]), 20, P(turn).color, 4, 0.6f, 0.1f); fx.Burst(Pos(sel[1]), 20, P(turn).color, 4, 0.6f, 0.1f); Banner(P(turn).name + ": MATCH! Go again", 1200, Hx(P(turn).color).c_str()); }
        else { Sfx("buzz"); turn = (turn + 1) % N(); }
        sel.clear(); botT = frand(0.9f, 1.5f);
        if (!pairsLeft) { over = true; endT = 3; Celebrate(Sorted([&](int i) { return (float)pts[i]; })[0]); }
        Push();
      }
      return;
    }
    if (P(turn).bot) { botT -= dt; if (botT <= 0) { botT = 0.8f; Flip(BotPick()); } }
    else { timer -= dt; if (timer <= 0) { timer = 20; Flip(BotPick()); } }
  }
  bool fin = false;
  void Draw() override {
    Room(); Seats();
    for (int k = 0; k < 20; k++) {
      Vector3 p = Pos(k); float u = cs[k].up, sq = fabsf(cosf(u * PI)), lift = sinf(u * PI) * 0.6f;
      bool faceUp = u > 0.5f; Color col = cs[k].gone && cs[k].who >= 0 ? P(cs[k].who).color : Hex(0xfdfcf7);
      lit.Draw(prim.cube, TRS({ p.x + 0.06f, 0.02f, p.z + 0.08f }, { 0, 0, 0 }, { 1.25f * sq, 0.01f, 1.6f }), Hex(0, 50));
      lit.Draw(prim.cube, TRS({ p.x, p.y + lift, p.z }, { 0, 0, 0 }, { 1.25f * sq, 0.05f, 1.6f }), faceUp ? (cs[k].gone ? Shade(col, 0.6f) : col) : Hex(0x1971c2));
      if (!faceUp) lit.Draw(prim.cube, TRS({ p.x, p.y + lift + 0.03f, p.z }, { 0, PI / 4, 0 }, { 0.78f * sq, 0.01f, 0.78f }), Hex(0x0d2b5e));
      if (sq > 0.35f) Label({ p.x, p.y + lift + 0.06f, p.z }, 1.25f * sq, faceUp ? cs[k].e : std::to_string(k + 1), faceUp ? WHITE : Hex(0xffffff), faceUp ? 1.15f : 1.05f);
    }
    End3D();
    SeatNames([&](int i) { return std::to_string(pts[i]) + " pairs"; });
    Title(over ? "All pairs found!" : P(turn).name + "'s turn", over ? WHITE : P(turn).color);
    if (!over && !P(turn).bot) { char b[8]; snprintf(b, sizeof b, "%d", (int)ceilf(timer)); Text(1540, 50, 34, timer < 6 ? Hex(0xff4d6d) : WHITE, b); }
    Hint("Pick 2 cards by number on your phone · a match = point + go again");
  }
};

// =====================================================================================
// HIGHER OR LOWER — everyone guesses at once: is the next card higher or lower?
// =====================================================================================
struct HiLo : Table {
  std::vector<Card> deck; Card cur, nxt; std::vector<int> pick, pts; std::vector<float> at; std::string phase = "ask"; float t = 0, flip = 0; int round = 0, R = 10;
  void Start() override { InitCam({ 0, 11.5f, 11.0f }, { 0, -0.2f, 0.6f }, 42); felt = Hex(0x1e6b3a); deck = Deck(); pts.assign(N(), 0); cur = deck.back(); deck.pop_back(); Ask(); Banner("⬆️ HIGHER OR LOWER? ⬇️", 1600, "#69db7c"); }
  void Ask() {
    round++; if (round > R) { over = true; endT = 2.5f; Celebrate(Sorted([&](int i) { return (float)pts[i]; })[0]); return; }
    if (deck.empty()) deck = Deck();
    nxt = deck.back(); deck.pop_back(); phase = "ask"; t = 7; flip = 0; pick.assign(N(), -1); at.assign(N(), 0);
    for (int i = 0; i < N(); i++) if (!P(i).bot) Layout(i, Choice(std::string("Card is ") + RANK[cur.r] + SUIT[cur.s] + " — next one is…", { "⬆️ HIGHER", "⬇️ LOWER" }, {}, 1, true, { "#2f9e44", "#e03131" }));
  }
  void Act(int p, const std::string& k, int i, float) override { if (phase == "ask" && k == "choice" && pick[p] < 0 && (i == 0 || i == 1)) { pick[p] = i; at[p] = t; Sfx("tick"); } }
  void Update(float dt) override {
    Tick(dt); t -= dt;
    if (over) { endT -= dt; if (endT <= 0 && !fin) { fin = true; FinishOrder(Sorted([&](int i) { return (float)pts[i]; }), [&](int i) { return std::to_string(pts[i]) + " pts"; }); } return; }
    if (phase == "ask") {
      for (int i = 0; i < N(); i++) if (P(i).bot && pick[i] < 0 && t < 6 - i * 0.4f) { int mid = cur.r; Act(i, "choice", (mid < 6 ? 0 : mid > 6 ? 1 : rand() % 2) ^ (frand(0, 1) < 0.15f), 0); }
      bool all = true; for (int v : pick) all &= v >= 0;
      if (all || t <= 0) { phase = "flip"; t = 3.2f; Sfx("card");
        int ans = nxt.r > cur.r ? 0 : nxt.r < cur.r ? 1 : -1;
        for (int i = 0; i < N(); i++) { bool ok = ans < 0 ? pick[i] >= 0 : pick[i] == ans; if (ok) { int g = 100 + (int)(at[i] / 7 * 50); pts[i] += g; fx.Burst(Front(i, 4.2f, 0.8f), 14, Hex(0x51cf66), 4, 0.6f, 0.1f); } if (!P(i).bot) Layout(i, Msg(ok ? "✅" : "❌", ok ? "Correct!" : "Wrong!", std::string("It was ") + RANK[nxt.r] + SUIT[nxt.s])); }
        Sfx(ans < 0 ? "ding" : "go");
      }
    } else { flip = fminf(1, flip + dt * 2.5f); if (t <= 0) { cur = nxt; Ask(); } }
  }
  bool fin = false;
  void Draw() override {
    Room(); Seats([&](int i) { return phase == "ask" && pick[i] < 0; });
    CardAt({ -1.3f, 0.06f, 0.6f }, 0, true, cur, 2.1f);
    CardAt({ 1.3f, 0.06f, 0.6f }, 0, phase != "ask" && flip > 0.5f, nxt, 2.1f, phase == "ask" ? 0 : flip, Hex(0x5f3dc4));
    End3D();
    int ans = nxt.r > cur.r ? 0 : nxt.r < cur.r ? 1 : -1;
    SeatNames([&](int i) { if (pick[i] < 0) return std::string(phase == "ask" ? "🤔" : "—"); if (phase == "ask") return std::string("✔ locked in"); bool ok = ans < 0 || pick[i] == ans; return std::string(pick[i] ? "⬇️" : "⬆️") + (ok ? " ✅" : " ❌"); });
    Board("🃏 POINTS", Sorted([&](int i) { return (float)pts[i]; }), [&](int i) { return std::to_string(pts[i]); });
    Title(phase == "ask" ? "HIGHER or LOWER?" : ans < 0 ? "SAME! Everyone scores" : ans == 0 ? "⬆️ HIGHER!" : "⬇️ LOWER!", phase == "ask" ? WHITE : Hex(0xffd43b));
    if (phase == "ask") { char b[8]; snprintf(b, sizeof b, "%d", (int)ceilf(fmaxf(0, t))); Text(1540, 50, 34, t < 3 ? Hex(0xff4d6d) : WHITE, b); }
    Hint("Round " + std::to_string(std::min(round, R)) + "/" + std::to_string(R) + " · Ace is lowest, King is highest · faster = more points");
  }
};

// =====================================================================================
// WORD RESCUE (hangman) — pick letters to spell the word before the snowman melts
// =====================================================================================
struct Word : Table {
  std::vector<std::string> words = { "SNOWMAN", "PENGUIN", "PIZZA", "RAINBOW", "DINOSAUR", "BIRTHDAY", "PANCAKE", "CHOCOLATE", "VOLCANO", "ROCKET", "PUMPKIN", "GIRAFFE", "SANDWICH", "JELLYFISH", "PRINCESS", "TREASURE", "DRAGON", "CUPCAKE", "TRAMPOLINE", "ASTRONAUT", "SUNFLOWER", "BUTTERFLY", "KANGAROO", "POPCORN", "SKATEBOARD", "MARSHMALLOW", "AVOCADO", "OCTOPUS", "HAMBURGER", "SPAGHETTI" };
  std::string word; std::vector<bool> used = std::vector<bool>(26); int miss = 0, wno = 0, W = 3; std::vector<int> pts; float botT = 1.5f, timer = 20, waitT = 0; std::string phase = "play";
  void Start() override { InitCam({ 0, 9.0f, 12.5f }, { 0, 1.2f, 0 }, 44); felt = Hex(0x4a6fa5); Shuffle(words); pts.assign(N(), 0); turn = rand() % N(); NewWord(); Banner("⛄ WORD RESCUE", 1600, "#74c0fc"); }
  void NewWord() { wno++; word = words[wno % words.size()]; used.assign(26, false); miss = 0; phase = "play"; Push(); }
  bool Solved() { for (char c : word) if (!used[c - 'A']) return false; return true; }
  void Push() {
    std::vector<std::string> L; std::vector<bool> off; for (int k = 0; k < 26; k++) { L.push_back(std::string(1, (char)('A' + k))); off.push_back(used[k]); }
    for (int i = 0; i < N(); i++) if (!P(i).bot) Layout(i, i == turn && phase == "play" && !over ? Choice("Your turn — pick a letter!", L, off, 6) : Msg("⛄", phase == "play" ? P(turn).name + "'s turn" : "Next word…", "Save the snowman!"));
  }
  void Guess(int k) {
    if (phase != "play" || k < 0 || k > 25 || used[k]) return;
    used[k] = true; int n = 0; for (char c : word) n += c == 'A' + k;
    if (n) { pts[turn] += 10 * n; Sfx("ding"); fx.Burst({ 0, 1.0f, 1.6f }, 12, P(turn).color, 4, 0.5f, 0.1f); Banner(std::to_string(n) + "× " + std::string(1, (char)('A' + k)) + "!", 900, "#51cf66"); }
    else { miss++; Sfx("buzz"); Shake(6); turn = (turn + 1) % N(); }
    if (Solved()) { pts[turn] += 30; phase = "done"; waitT = 3; Banner(P(turn).name + " saved the snowman! +30", 2000, Hx(P(turn).color).c_str()); conf.Spawn({ 0, 4, 0 }, 90, 5, 5); Sfx("win"); turn = (turn + 1) % N(); }
    else if (miss >= 6) { phase = "done"; waitT = 3; Banner("The snowman melted! It was " + word, 2400, "#ff6b6b"); Sfx("boom"); }
    timer = 20; botT = frand(0.8f, 1.5f); Push();
  }
  void Act(int p, const std::string& k, int i, float) override { if (p == turn && k == "choice") Guess(i); }
  int BotLetter() { const char* freq = "EAIORNTSLCUDPMHGBYFKVWZXQJ"; std::vector<int> c; for (int k = 0; k < 26; k++) if (!used[freq[k] - 'A']) c.push_back(freq[k] - 'A'); return c[std::min((int)c.size() - 1, rand() % 4)]; }
  void Update(float dt) override {
    Tick(dt);
    if (over) { endT -= dt; if (endT <= 0 && !fin) { fin = true; FinishOrder(Sorted([&](int i) { return (float)pts[i]; }), [&](int i) { return std::to_string(pts[i]) + " pts"; }); } return; }
    if (phase == "done") { waitT -= dt; if (waitT <= 0) { if (wno >= W) { over = true; endT = 2; Celebrate(Sorted([&](int i) { return (float)pts[i]; })[0]); } else NewWord(); } return; }
    if (P(turn).bot) { botT -= dt; if (botT <= 0) Guess(BotLetter()); }
    else { timer -= dt; if (timer <= 0) Guess(BotLetter()); }
  }
  bool fin = false;
  void Draw() override {
    Room(); Seats();
    // the snowman melts a bit with every miss
    float m = miss / 6.f, h = 1 - m * 0.85f;
    lit.Draw(prim.sphere, TRS({ 0, 0, -0.6f }, { 0, 0, 0 }, { 2.4f + m * 2.2f, 0.25f + m * 0.1f, 2.0f + m * 1.6f }), Hex(0xd0ebff, 230));
    if (miss < 6) {
      lit.Draw(prim.sphere, TRS({ 0, 0.85f * h, -0.6f }, { 0, 0, 0 }, { 1.8f, 1.7f * h, 1.8f }), WHITE);
      lit.Draw(prim.sphere, TRS({ 0, 2.2f * h, -0.6f }, { 0, 0, 0 }, { 1.3f, 1.25f * h, 1.3f }), WHITE);
      lit.Draw(prim.sphere, TRS({ 0, 3.25f * h, -0.6f }, { 0, 0, 0 }, { 0.95f, 0.9f * h, 0.95f }), WHITE);
      lit.Draw(prim.cone, TRS({ 0, 3.25f * h, -0.15f }, { PI / 2, 0, 0 }, { 0.16f, 0.6f, 0.16f }), Hex(0xff922b));
      for (int s = -1; s <= 1; s += 2) lit.Draw(prim.sphere, TRS({ s * 0.18f, 3.42f * h, -0.2f }, { 0, 0, 0 }, { 0.12f, 0.12f, 0.12f }), Hex(0x1a1438));
      if (miss < 4) { lit.Draw(prim.cyl, TRS({ 0, 3.62f * h, -0.6f }, { 0, 0, 0 }, { 1.1f, 0.08f, 1.1f }), Hex(0x212529)); lit.Draw(prim.cyl, TRS({ 0, 3.65f * h, -0.6f }, { 0, 0, 0 }, { 0.7f, 0.6f, 0.7f }), Hex(0x212529)); }
    }
    // letter tiles on a little stand
    int n = (int)word.size(); float w = fminf(0.9f, 9.0f / n);
    for (int k = 0; k < n; k++) {
      Vector3 p = { (k - (n - 1) / 2.f) * w, 0.3f, 2.2f }; bool show = used[word[k] - 'A'] || phase == "done";
      lit.Draw(prim.cube, TRS(p, { -0.5f, 0, 0 }, { w * 0.86f, 0.12f, w * 1.05f }), show ? Hex(0xfff3bf) : Hex(0xc98a4b));
      if (show) Label({ p.x, p.y + 0.12f, p.z }, w * 0.86f, std::string(1, word[k]), used[word[k] - 'A'] ? Hex(0x1a1438) : Hex(0xe03131), 1.3f);
    }
    End3D();
    SeatNames([&](int i) { return std::to_string(pts[i]) + " pts"; });
    std::string wrong; for (int k = 0; k < 26; k++) if (used[k] && word.find((char)('A' + k)) == std::string::npos) wrong += std::string(wrong.empty() ? "" : " ") + (char)('A' + k);
    if (!wrong.empty()) Text(800, 130, 30, Hex(0xff8787), "✗ " + wrong, 0, TS_HEAVY);
    Title(phase == "play" ? P(turn).name + "'s turn" : "Word " + std::to_string(wno) + " done!", phase == "play" ? P(turn).color : WHITE);
    Text(800, 172, 22, Hex(0xd0ebff), "⛄ " + std::to_string(6 - miss) + " misses left", 0, TS_HEAVY);
    Hint("Word " + std::to_string(wno) + " of " + std::to_string(W) + " · right letter = 10 pts each · miss = next player");
  }
};
}  // namespace

std::unique_ptr<Game> MakeEights() { return std::make_unique<Eights>(); }
std::unique_ptr<Game> MakeFour() { return std::make_unique<Four>(); }
std::unique_ptr<Game> MakeSnakes() { return std::make_unique<Snakes>(); }
std::unique_ptr<Game> MakeBlackjack() { return std::make_unique<Blackjack>(); }
std::unique_ptr<Game> MakeBingo() { return std::make_unique<Bingo>(); }
std::unique_ptr<Game> MakeDice() { return std::make_unique<DicePoker>(); }
std::unique_ptr<Game> MakeMemory() { return std::make_unique<Memory>(); }
std::unique_ptr<Game> MakeHiLo() { return std::make_unique<HiLo>(); }
std::unique_ptr<Game> MakeWord() { return std::make_unique<Word>(); }
