// GAME NIGHT at the 3D card table: Color Storm (shed your cards), Avalanche! (don't draw the avalanche)
// and Snow Tycoon (a winter resort property board game). Phones show a private panel: your cards + buttons.
// Easier than before: longer turn timers, gentler "last card" catching, shorter Tycoon (8 rounds, more cash).
#include "table.h"

namespace {

struct PCard { std::string label, sub, icon, color; bool ok = false; };
struct PBtn { std::string id, label, color; bool off = false, big = false; };
std::string PanelJ(const std::string& title, const std::string& info, const std::string& sub, const std::vector<PCard>& cards, const std::vector<PBtn>& btns, bool my) {
  std::string c, b;
  for (size_t i = 0; i < cards.size(); i++) c += std::string(i ? "," : "") + "{\"label\":" + Q(cards[i].label) + ",\"sub\":" + Q(cards[i].sub) + (cards[i].icon.empty() ? "" : ",\"icon\":" + Q(cards[i].icon)) + ",\"color\":" + Q(cards[i].color) + ",\"ok\":" + B(cards[i].ok) + "}";
  for (size_t i = 0; i < btns.size(); i++) b += std::string(i ? "," : "") + "{\"id\":" + Q(btns[i].id) + ",\"label\":" + Q(btns[i].label) + ",\"color\":" + Q(btns[i].color) + ",\"off\":" + B(btns[i].off) + ",\"big\":" + B(btns[i].big) + "}";
  return "{\"t\":\"panel\",\"title\":" + Q(title) + ",\"info\":" + Q(info) + ",\"sub\":" + Q(sub) + ",\"myTurn\":" + B(my) + ",\"cards\":[" + c + "],\"buttons\":[" + b + "]}";
}

// a big game card lying on the table: white border, coloured face, symbol on top
struct GameNight : Table {
  int ViewSeat() override { return over || turn < 0 || turn >= N() ? -1 : turn; }   // the TV looks from the seat of whoever's turn it is
  std::string log;
  void BigCard(Vector3 p, float yaw, Color face, const std::string& sym, float s = 1, bool back = false, Color backC = Hex(0x3b1f7a), float lift = 0) {
    p.y += lift;
    lit.Draw(prim.cube, TRS({ p.x + 0.06f * s, 0.01f, p.z + 0.08f * s }, { 0, yaw, 0 }, { 0.92f * s, 0.01f, 1.32f * s }), Hex(0x000000, 60));
    lit.Draw(prim.cube, TRS(p, { 0, yaw, 0 }, { 0.9f * s, 0.035f, 1.3f * s }), Hex(0xfdfcf7));
    lit.Draw(prim.cube, TRS({ p.x, p.y + 0.02f, p.z }, { 0, yaw, 0 }, { 0.78f * s, 0.01f, 1.18f * s }), back ? backC : face);
    if (back) { lit.Draw(prim.cube, TRS({ p.x, p.y + 0.026f, p.z }, { 0, yaw + PI / 4, 0 }, { 0.32f * s, 0.01f, 0.32f * s }), Shade(backC, 0.35f)); return; }
    lit.Draw(prim.sphere, TRS({ p.x, p.y + 0.025f, p.z }, { 0, yaw + 0.5f, 0 }, { 0.6f * s, 0.01f, 0.86f * s }), Hex(0xffffff, 235));
    if (!sym.empty()) Label({ p.x, p.y + 0.03f, p.z }, 0.9f * s, sym, Shade(face, -0.3f), sym.size() > 3 ? 0.75f : 1.15f);
  }
  void Backs(int i, int n, Color backC) {  // a player's hand as a fan of card backs in front of them
    Vector3 f = Front(i, 4.9f); float a = SeatA(i); int m = std::min(n, 12);
    for (int k = 0; k < m; k++) { float o = (k - (m - 1) / 2.f) * 0.32f; BigCard({ f.x - sinf(a) * o, 0.05f + k * 0.012f, f.z + cosf(a) * o * 0.8f }, -a - PI / 2 + (k - (m - 1) / 2.f) * 0.05f, BLANK, "", 0.62f, true, backC); }
  }
  void LogBar() { if (!log.empty()) { float w = Measure(log, 24, TS_HEAVY) + 70; RRect(800 - w / 2, 130, w, 48, 24, Hex(0x0a1230, 210), Hex(0x0a1230, 210), Hex(0xffffff, 150), 1.5f, false, true); Text(800, 154, 24, WHITE, log, 0, TS_HEAVY | TS_PLAIN); } }
};

// ===================================================================================== COLOR STORM
const char* SC_N[] = { "Red", "Blue", "Green", "Yellow" };
const unsigned SC_C[] = { 0xff4d6d, 0x4dabff, 0x51cf66, 0xffd43b };
struct SCard { int c; std::string v; float rot = 0; };  // c -1 = wild
std::string SSym(const SCard& c) { return c.v == "REV" ? "↺" : c.v == "STOP" ? "⛔" : c.v == "TWIN" ? "✌" : c.v == "COLOR" ? "🌈" : c.v == "BLIZZARD" ? "+4" : c.v == "SWAP" ? "⇄" : c.v; }
bool IsNum(const std::string& v) { return v.size() == 1 && v[0] >= '1' && v[0] <= '9'; }
struct Storm : GameNight {
  std::vector<SCard> deck, pile; std::vector<std::vector<SCard>> hand; int color = 0, dir = 1, pending = 0, lcP = -1, winner = -1; bool drew = false, needColor = false, twin = false, lcSafe = false, lcTried = false;
  float timer = 30, botT = 1.5f, lcT = 0; struct Fly { SCard c; Vector3 from; float t = 1; } fly;
  void Start() override {
    TableCam(); felt = Hex(0x0f6a7a);
    for (int c = 0; c < 4; c++) { for (int v = 1; v <= 9; v++) for (int k = 0; k < 2; k++) deck.push_back({ c, std::to_string(v) }); for (const char* v : { "+2", "STOP", "REV", "TWIN" }) for (int k = 0; k < 2; k++) deck.push_back({ c, v }); }
    for (int i = 0; i < 4; i++) { deck.push_back({ -1, "COLOR" }); deck.push_back({ -1, "BLIZZARD" }); }
    for (int i = 0; i < 2; i++) deck.push_back({ -1, "SWAP" });
    Shuffle(deck);
    int n = N() > 5 ? 5 : 7; hand.assign(N(), {});
    for (int i = 0; i < N(); i++) for (int k = 0; k < n; k++) { hand[i].push_back(deck.back()); deck.pop_back(); }
    while (true) { SCard t = deck.back(); deck.pop_back(); if (t.c >= 0 && IsNum(t.v)) { t.rot = frand(-0.2f, 0.2f); pile.push_back(t); color = t.c; break; } deck.insert(deck.begin(), t); }
    turn = rand() % N(); Layout(-1, "{\"name\":\"panel\"}"); Banner("🌪️ COLOR STORM!", 1600, "#cc5de8"); log = "Match the colour or the symbol!"; Sync();
  }
  SCard& Top() { return pile.back(); }
  int Idx(int k) { return ((turn + dir * k) % N() + N()) % N(); }
  bool Playable(const SCard& c) { if (twin) return c.c == color; if (pending > 0) return c.v == "+2" || c.v == "BLIZZARD"; return c.c < 0 || c.c == color || c.v == Top().v; }
  std::string PanelFor(int p) {
    bool my = p == turn && !over; std::vector<PBtn> bt; std::vector<PCard> cs;
    if (my && needColor) for (int k = 0; k < 4; k++) bt.push_back({ "col" + std::to_string(k), SC_N[k], Hx(Hex(SC_C[k])), false, true });
    else if (my) { if (!drew && !twin) bt.push_back({ "draw", pending ? "DRAW " + std::to_string(pending) : "DRAW", "#4dabff" }); if (drew || twin) bt.push_back({ "pass", twin ? "DONE" : "PASS", "#868e96" }); }
    if (lcP >= 0 && !lcSafe) { if (lcP == p) bt.push_back({ "last", "☝ LAST CARD!", "#ff922b", false, true }); else bt.push_back({ "catch", "🚨 CATCH " + P(lcP).name + "!", "#ff4d6d", false, true }); }
    for (auto& c : hand[p]) cs.push_back({ SSym(c), c.c < 0 ? c.v : "", "", c.c >= 0 ? Hx(Hex(SC_C[c.c])) : "#7048e8", my && !needColor && Playable(c) });
    std::string title = my ? (needColor ? "🌈 Pick a colour!" : twin ? std::string("✌ TWIN: play another ") + SC_N[color] + " card" : pending ? "⚠️ Stack a +2 / +4 or draw " + std::to_string(pending) : "🌪️ Your turn! Glowing cards can be played") : P(turn).name + "'s turn";
    return PanelJ(title, std::to_string(hand[p].size()) + " cards · colour: " + SC_N[color], log, cs, bt, my);
  }
  void Sync() { for (int i = 0; i < N(); i++) if (!P(i).bot) Send(i, PanelFor(i)); }
  SCard Draw1() { if (deck.empty()) { SCard t = pile.back(); pile.pop_back(); deck = pile; Shuffle(deck); pile = { t }; } if (deck.empty()) return { 0, "1" }; SCard c = deck.back(); deck.pop_back(); return c; }
  void Give(int p, int n) { for (int k = 0; k < n; k++) hand[p].push_back(Draw1()); Sfx("card"); }
  void Next(int k) { turn = Idx(k); drew = false; twin = false; timer = 30; botT = frand(1, 2); Sync(); }
  void Win(int p) { over = true; winner = p; endT = 3; Celebrate(p); Sync(); }
  void Act(int p, const std::string& k, int i, float) override {
    if (over || p < 0 || p >= N()) return;
    if (k == "btn:last" && lcP == p) { lcSafe = true; Banner("☝ " + P(p).name + ": LAST CARD!", 1300, Hx(P(p).color).c_str()); Sfx("ding"); Sync(); return; }
    if (k == "btn:catch" && lcP >= 0 && !lcSafe && lcP != p) { int v = lcP; lcP = -1; Give(v, 2); Banner("🚨 " + P(p).name + " caught " + P(v).name + "! +2", 1600, "#ff4d6d"); Sfx("buzz"); Sync(); return; }
    if (p != turn) return;
    if (k.rfind("btn:col", 0) == 0 && needColor) { color = atoi(k.c_str() + 7) % 4; needColor = false; log = P(p).name + " → " + SC_N[color]; Sfx("tick"); AfterPlay(); return; }
    if (needColor) return;
    if (k == "card") Play(p, i);
    else if (k == "btn:draw") DoDraw(p);
    else if (k == "btn:pass" && (drew || twin)) { twin = false; Next(1); }
  }
  void DoDraw(int p) {
    if (drew || twin) return;
    if (pending) { int n = pending; pending = 0; Give(p, n); log = P(p).name + " drew " + std::to_string(n) + "! 😱"; Shake(6); Next(1); return; }
    Give(p, 1); drew = true; log = P(p).name + " drew a card";
    if (!Playable(hand[p].back())) { Next(1); return; }
    Sync();
  }
  void Play(int p, int i) {
    if (i < 0 || i >= (int)hand[p].size() || !Playable(hand[p][i])) return;
    SCard c = hand[p][i]; hand[p].erase(hand[p].begin() + i); c.rot = frand(-0.4f, 0.4f); pile.push_back(c); Sfx("card");
    fly = { c, Front(p, 4.6f, 0.4f), 0 };
    twin = false; log = P(p).name + " played " + (c.c >= 0 ? std::string(SC_N[c.c]) + " " : "") + SSym(c);
    if (c.c >= 0) color = c.c;
    if (hand[p].size() == 1) { lcP = p; lcSafe = false; lcT = 4.5f; lcTried = false; }
    if (hand[p].empty()) { Win(p); return; }
    if (c.v == "+2") pending += 2;
    if (c.v == "BLIZZARD") { pending += 4; Shake(10); cine::Shot s; s.kind = cine::SMASH; s.at = { 1.0f, 0.3f, 0.4f }; s.size = 1.2f; s.c = Hex(0x74c0fc); s.caption = "BLIZZARD +4"; s.sub = P(p).name; s.elev = 0.6f; s.floorY = 0.02f; cine::Play(s); }
    if (c.v == "REV") { dir = -dir; if (N() == 2) turn = Idx(1); }
    if (c.v == "SWAP") { int ld = -1; for (int q = 0; q < N(); q++) if (q != p && (ld < 0 || hand[q].size() < hand[ld].size())) ld = q; if (ld >= 0) { std::swap(hand[p], hand[ld]); log = P(p).name + " SWAPPED hands with " + P(ld).name + "! ⇄"; Shake(8); if (hand[p].empty()) { Win(p); return; } } }
    if (c.c < 0) { needColor = true; Sync(); return; }
    AfterPlay();
  }
  void AfterPlay() {
    SCard& c = Top();
    if (c.v == "TWIN") { for (auto& h : hand[turn]) if (h.c == color) { twin = true; drew = false; Sync(); return; } }
    Next(c.v == "STOP" ? 2 : 1);
  }
  void BotTurn(int p) {
    if (needColor) { int cnt[4] = { 0, 0, 0, 0 }; for (auto& c : hand[p]) if (c.c >= 0) cnt[c.c]++; int b = 0; for (int k = 1; k < 4; k++) if (cnt[k] > cnt[b]) b = k; Act(p, "btn:col" + std::to_string(b), 0, 0); return; }
    int best = -1; for (int i = 0; i < (int)hand[p].size(); i++) if (Playable(hand[p][i]) && (best < 0 || (hand[p][best].c < 0 && hand[p][i].c >= 0))) best = i;
    if (best >= 0) { Play(p, best); return; }
    if (twin || drew) { Act(p, "btn:pass", 0, 0); return; }
    DoDraw(p);
  }
  bool fin = false;
  void Update(float dt) override {
    Tick(dt); fly.t = fminf(1, fly.t + dt * 2.6f);
    if (over) { endT -= dt; if (endT <= 0 && !fin) { fin = true; auto o = Sorted([&](int i) { return (i == winner ? 1000.f : 0) - hand[i].size(); }); FinishOrder(o, [&](int i) { return std::to_string(hand[i].size()) + " cards left"; }); } return; }
    if (lcP >= 0) {
      lcT -= dt;
      if (P(lcP).bot && !lcSafe && !lcTried && lcT < 3.8f) { lcTried = true; if (frand(0, 1) < 0.85f) { lcSafe = true; Banner("☝ " + P(lcP).name + ": LAST CARD!", 1200, Hx(P(lcP).color).c_str()); Sync(); } }
      if (!lcSafe && lcT < 1.5f) for (int q = 0; q < N(); q++) if (P(q).bot && q != lcP && frand(0, 1) < dt * 0.6f) { Act(q, "btn:catch", 0, 0); break; }
      if (lcP >= 0 && (lcT <= 0 || (lcSafe && lcT < 2.5f))) { lcP = -1; Sync(); }
    }
    if (P(turn).bot) { botT -= dt; if (botT <= 0) { botT = frand(0.9f, 1.6f); BotTurn(turn); } return; }
    timer -= dt; if (timer <= 0) { timer = 30; if (needColor) Act(turn, "btn:col0", 0, 0); else BotTurn(turn); }
  }
  void Draw() override {
    Room(); Seats([&](int i) { return i == turn && !over; }, [&](int i) { return over && i == winner ? 1.f : 0.f; });
    for (int i = 0; i < N(); i++) Backs(i, (int)hand[i].size(), Hex(0x3b1f7a));
    // draw pile + discard pile + current colour ring
    for (int k = 0; k < 6; k++) BigCard({ -2.4f, 0.05f + k * 0.04f, 0.4f }, -0.05f, BLANK, "", 1.3f, true, Hex(0x3b1f7a));
    Color cc = Hex(SC_C[color]);
    lit.Draw(prim.cyl, TRS({ 1.0f, 0.03f, 0.4f }, { 0, 0, 0 }, { 2.1f + 0.06f * sinf(time * 3), 0.01f, 2.1f + 0.06f * sinf(time * 3) }), Fade(cc, 0.55f));
    for (int k = 0; k < 6; k++) { float a = time * 0.6f * dir + k * PI / 3; lit.Draw(prim.sphere, TRS({ 1.0f + cosf(a) * 2.45f, 0.08f, 0.4f + sinf(a) * 2.45f }, { 0, 0, 0 }, { 0.14f, 0.08f, 0.14f }), Fade(WHITE, 0.6f)); }
    int start = std::max(0, (int)pile.size() - 5);
    for (int k = start; k < (int)pile.size(); k++) {
      bool top = k == (int)pile.size() - 1; SCard& c = pile[k];
      Vector3 at = { 1.0f, 0.06f + (k - start) * 0.03f, 0.4f };
      if (top && fly.t < 1) { float u = EaseOut(fly.t); at = Vector3Lerp(fly.from, at, u); at.y += sinf(u * PI) * 1.4f; }
      BigCard(at, c.rot, c.c >= 0 ? Hex(SC_C[c.c]) : Hex(0x7048e8), SSym(c), 1.45f);
    }
    if (pending) Label({ 2.9f, 0.3f, -0.6f }, 1.3f, "+" + std::to_string(pending), Hex(0xff4d6d), 1.2f);
    End3D();
    SeatNames([&](int i) { return std::to_string(hand[i].size()) + " cards" + (lcP == i ? (lcSafe ? " · ☝" : " · ⚠️ 1!") : ""); });
    Board("🌪️ CARDS LEFT", Sorted([&](int i) { return -(float)hand[i].size(); }), [&](int i) { return std::to_string(hand[i].size()); });
    Title("COLOR STORM", Hex(0xffe066));
    if (!over && !pile.empty()) {   // the card to match, big enough to read from the couch
      const SCard& t = Top(); float cx = 1470, cy = 400, w = 150, h = 214;
      Text(cx, cy - h / 2 - 20, 20, WHITE, "MATCH THIS", 0, TS_HEAVY | TS_SHADOW);
      RRect(cx - w / 2 + 6, cy - h / 2 + 8, w, h, 20, Hex(0, 90), Hex(0, 90), BLANK, 0);
      RRect(cx - w / 2, cy - h / 2, w, h, 20, Hex(0xfbfbf6), Hex(0xe6e6dc), BLANK, 0);
      RRect(cx - w / 2 + 9, cy - h / 2 + 9, w - 18, h - 18, 14, cc, Shade(cc, -0.25f), BLANK, 0);
      std::string sy = SSym(t); Text(cx, cy + 4, sy.size() > 3 ? 60 : 84, WHITE, sy, 0, TS_DISPLAY | TS_SHADOW);
      Text(cx, cy + h / 2 + 22, 22, cc, SC_N[color], 0, TS_HEAVY | TS_SHADOW);
    }
    LogBar();
    if (!over && !P(turn).bot) { char b[8]; snprintf(b, sizeof b, "%d", (int)ceilf(fmaxf(0, timer))); Text(1540, 50, 34, timer < 8 ? Hex(0xff4d6d) : WHITE, b, 0, TS_DISPLAY | TS_SHADOW); }
    Hint(std::string("Colour now: ") + SC_N[color] + "  ·  match colour or symbol  ·  +2 stacks  ·  ⛔ skip  ·  ↺ reverse", cc);
  }
};

// ===================================================================================== AVALANCHE!
enum { AV_AVAL, AV_SHOVEL, AV_PEEK, AV_SKIP, AV_DOUBLE, AV_SHUF, AV_STEAL, AV_BLOCK, AV_SCARF, AV_HAT, AV_CAP };
const char* AVI[] = { "💥", "🛡️", "👀", "⏭️", "⏩", "🔀", "🧤", "✋", "🧣", "🎩", "🧢" };
const char* AVN[] = { "AVALANCHE", "SHOVEL", "PEEK", "SKIP", "DOUBLE", "SHUFFLE", "STEAL", "BLOCK", "Scarf", "Top hat", "Cap" };
const unsigned AVC[] = { 0xff4d6d, 0x51cf66, 0x4dabff, 0xffd43b, 0xff922b, 0xcc5de8, 0x22d3ee, 0xf783ac, 0x868e96, 0x868e96, 0x868e96 };
bool IsPair(int k) { return k >= AV_SCARF; }
struct Aval : GameNight {
  void DebugShot(int k) override { if (k == cine::SNOW) cine::Moment(cine::SNOW, { 0, 0.8f, 0 }, 2.2f, Hex(0xd0ebff), "AVALANCHE!", P(0).name, 0.f); else GameNight::DebugShot(k); }
  std::vector<int> deck, discard, outOrder; std::vector<std::vector<int>> hand; std::vector<bool> alive; std::vector<int> knows;
  int owed = 1, peekFor = -1, winner = -1; std::string state = "play"; float timer = 30, botT = 1.5f, peekT = 0, buryT = 0, flash = 0;
  struct PA { int by = -1, card = 0; bool pair = false; float t = 0; int blockedBy = -1; } pa; std::vector<int> peekCards;
  void Start() override {
    TableCam(); felt = Hex(0x23406e);
    std::vector<int> base; auto add = [&](int k, int m) { for (int i = 0; i < m; i++) base.push_back(k); };
    int x = N() / 2; add(AV_PEEK, 4 + x); add(AV_SKIP, 5 + x); add(AV_DOUBLE, 3 + x); add(AV_SHUF, 4 + x); add(AV_STEAL, 4 + x); add(AV_BLOCK, 4 + x); add(AV_SCARF, 4 + x); add(AV_HAT, 4 + x); add(AV_CAP, 4 + x);
    Shuffle(base); hand.assign(N(), {}); alive.assign(N(), true); knows.assign(N(), -1);
    for (int i = 0; i < N(); i++) { hand[i].push_back(AV_SHOVEL); for (int k = 0; k < 5; k++) { hand[i].push_back(base.back()); base.pop_back(); } }
    deck = base; for (int i = 0; i < N() - 1; i++) deck.push_back(AV_AVAL); for (int i = 0; i < (N() <= 3 ? 2 : 2); i++) deck.push_back(AV_SHOVEL); Shuffle(deck);
    turn = rand() % N(); Layout(-1, "{\"name\":\"panel\"}"); Banner("💣 AVALANCHE!", 1600, "#ff4d6d"); log = "Don't draw the avalanche! Play cards, then DRAW."; Sync();
  }
  int NextAlive(int i) { for (int k = 1; k <= N(); k++) { int j = (i + k) % N(); if (alive[j]) return j; } return i; }
  float Danger() { int a = 0; for (int k : deck) a += k == AV_AVAL; return deck.empty() ? 0 : (float)a / deck.size(); }
  int Count(int p, int k) { int n = 0; for (int x : hand[p]) n += x == k; return n; }
  int Find(int p, int k) { for (size_t i = 0; i < hand[p].size(); i++) if (hand[p][i] == k) return (int)i; return -1; }
  std::string PanelFor(int p) {
    if (!alive[p]) return PanelJ("❄️ You got buried!", "Watch the others…", log, {}, {}, false);
    bool my = p == turn && !over; std::vector<PBtn> bt; std::vector<PCard> cs;
    if (state == "bury" && my) for (const char* w : { "top", "2nd", "middle", "bottom", "random" }) bt.push_back({ std::string("put_") + w, std::string(w) == "top" ? "⬆ On top (mean!)" : std::string(w) == "random" ? "🎲 Random" : w, "#ff4d6d" });
    else if (state == "target" && my) { for (int q = 0; q < N(); q++) if (q != p && alive[q] && !hand[q].empty()) bt.push_back({ "tgt_" + std::to_string(q), "🧤 " + P(q).name + " (" + std::to_string(hand[q].size()) + ")", Hx(P(q).color) }); }
    else if (my && state == "play" && pa.by < 0) bt.push_back({ "draw", owed > 1 ? "DRAW (" + std::to_string(owed) + " turns)" : "DRAW (ends your turn)", "#4dabff", false, true });
    if (pa.by >= 0 && pa.by != p && Find(p, AV_BLOCK) >= 0 && pa.blockedBy < 0) bt.push_back({ "block", std::string("✋ BLOCK ") + AVN[pa.card] + "!", "#f783ac", false, true });
    for (int k : hand[p]) { bool ok = my && state == "play" && pa.by < 0 && (k == AV_PEEK || k == AV_SKIP || k == AV_DOUBLE || k == AV_SHUF || k == AV_STEAL || (IsPair(k) && Count(p, k) >= 2)); cs.push_back({ AVN[k], IsPair(k) ? "pair = steal" : "", AVI[k], Hx(Hex(AVC[k])), ok }); }
    std::string sub = peekFor == p ? "👀 Top 3: " : log;
    if (peekFor == p) for (size_t k = 0; k < peekCards.size(); k++) sub += std::string(k ? " · " : "") + AVI[peekCards[k]] + " " + AVN[peekCards[k]];
    std::string title = my ? (state == "bury" ? "🛡️ Dug out! Hide the avalanche…" : state == "target" ? "🧤 Steal from who?" : "💣 Your turn — play cards, then DRAW") : P(turn).name + "'s turn";
    return PanelJ(title, std::to_string(deck.size()) + " in deck · danger " + std::to_string((int)(Danger() * 100)) + "%", sub, cs, bt, my);
  }
  void Sync() { for (int i = 0; i < N(); i++) if (!P(i).bot) Send(i, PanelFor(i)); }
  void Act(int p, const std::string& k, int i, float) override {
    if (over || p < 0 || p >= N() || !alive[p]) return;
    if (k == "btn:block" && pa.by >= 0 && pa.by != p && pa.blockedBy < 0) { int bi = Find(p, AV_BLOCK); if (bi < 0) return; hand[p].erase(hand[p].begin() + bi); pa.blockedBy = p; log = "✋ " + P(p).name + " BLOCKED " + AVN[pa.card] + "!"; Sfx("zap"); Shake(5); Sync(); return; }
    if (p != turn) return;
    if (state == "bury" && k.rfind("btn:put_", 0) == 0) { Bury(k.substr(8)); return; }
    if (state == "target" && k.rfind("btn:tgt_", 0) == 0) { int q = atoi(k.c_str() + 8); state = "play"; if (q >= 0 && q < N()) Steal(p, q); return; }
    if (state != "play" || pa.by >= 0) return;
    if (k == "btn:draw") { DrawCard(p); return; }
    if (k == "card" && i >= 0 && i < (int)hand[p].size()) {
      int c = hand[p][i];
      if (IsPair(c)) { if (Count(p, c) < 2) return; hand[p].erase(hand[p].begin() + Find(p, c)); hand[p].erase(hand[p].begin() + Find(p, c)); Announce(p, c, true); return; }
      if (c != AV_PEEK && c != AV_SKIP && c != AV_DOUBLE && c != AV_SHUF && c != AV_STEAL) return;
      hand[p].erase(hand[p].begin() + i); Announce(p, c, false);
    }
  }
  void Announce(int p, int c, bool pair) { discard.push_back(c); pa = { p, c, pair, 2.5f, -1 }; log = P(p).name + " plays " + AVI[c] + " " + (pair ? std::string("a PAIR") : std::string(AVN[c])) + "…"; Sfx("card"); Sync(); }
  void Resolve() {
    PA a = pa; pa.by = -1; int p = a.by;
    if (a.blockedBy >= 0) { Sync(); return; }
    if (a.pair || a.card == AV_STEAL) {
      int best = -1; for (int q = 0; q < N(); q++) if (q != p && alive[q] && !hand[q].empty() && (best < 0 || hand[q].size() > hand[best].size())) best = q;
      if (best < 0) { Sync(); return; }
      if (P(p).bot) { Steal(p, best); return; }
      state = "target"; Sync(); return;
    }
    if (a.card == AV_PEEK) { peekFor = p; peekT = 7; peekCards.clear(); for (int k = 0; k < 3 && k < (int)deck.size(); k++) peekCards.push_back(deck[deck.size() - 1 - k]); log = "👀 " + P(p).name + " peeked at the deck"; if (!peekCards.empty()) knows[p] = peekCards[0]; }
    if (a.card == AV_SHUF) { Shuffle(deck); log = "🔀 " + P(p).name + " shuffled the deck"; Sfx("whoosh"); knows.assign(N(), -1); }
    if (a.card == AV_SKIP) { log = "⏭️ " + P(p).name + " skips"; EndTurn(); return; }
    if (a.card == AV_DOUBLE) { log = "⏩ " + P(p).name + ": next player takes 2 turns!"; turn = NextAlive(turn); owed = 2; StartTurn(); return; }
    Sync();
  }
  void Steal(int p, int q) { if (hand[q].empty()) { Sync(); return; } int k = rand() % hand[q].size(); hand[p].push_back(hand[q][k]); hand[q].erase(hand[q].begin() + k); log = "🧤 " + P(p).name + " stole a card from " + P(q).name; Sfx("pickup"); Sync(); }
  void DrawCard(int p) {
    if (deck.empty()) { EndTurn(); return; }
    int k = deck.back(); deck.pop_back(); knows[p] = -1;
    if (k != AV_AVAL) { hand[p].push_back(k); log = P(p).name + " drew a card… phew 😮‍💨"; Sfx("card"); EndTurn(); return; }
    flash = 1; Shake(24); Sfx("boom"); Sfx("alarm");
    cine::Moment(cine::SNOW, { 0, 0.8f, 0 }, 2.2f, Hex(0xd0ebff), "AVALANCHE!", P(p).name, 0.f);
    int si = Find(p, AV_SHOVEL);
    if (si >= 0) { hand[p].erase(hand[p].begin() + si); log = "💥 AVALANCHE! " + P(p).name + " dug out with a SHOVEL 🛡️"; state = "bury"; buryT = 14; Banner("🛡️ " + P(p).name + " DUG OUT!", 1600, "#51cf66"); Sync(); return; }
    alive[p] = false; outOrder.push_back(p); log = "💥 AVALANCHE! " + P(p).name + " is buried ❄️"; Banner("❄️ " + P(p).name + " IS OUT!", 2000, "#ff4d6d"); Buzz(p, 600);
    int al = 0, last = -1; for (int q = 0; q < N(); q++) if (alive[q]) { al++; last = q; }
    if (al <= 1) { over = true; winner = last; endT = 3; Celebrate(last); Sync(); return; }
    owed = 1; turn = NextAlive(turn); StartTurn();
  }
  void Bury(const std::string& w) {
    int n = (int)deck.size(), pos = w == "top" ? n : w == "2nd" ? std::max(0, n - 1) : w == "middle" ? n / 2 : w == "bottom" ? 0 : irand(0, n);
    deck.insert(deck.begin() + pos, AV_AVAL); state = "play"; log = "🤫 " + P(turn).name + " hid the avalanche back in the deck…"; knows.assign(N(), -1); EndTurn();
  }
  void EndTurn() { owed--; if (owed > 0) { StartTurn(); return; } owed = 1; turn = NextAlive(turn); StartTurn(); }
  void StartTurn() { timer = 30; botT = frand(1.2f, 2.2f); peekFor = -1; state = "play"; Sync(); }
  void BotMove(int p) {
    float d = Danger();
    if (knows[p] == AV_AVAL || (d > 0.3f && frand(0, 1) < 0.6f)) for (int k : { AV_SKIP, AV_DOUBLE, AV_SHUF }) { int i = Find(p, k); if (i >= 0) { Act(p, "card", i, 0); return; } }
    if (d > 0.15f && Find(p, AV_PEEK) >= 0 && knows[p] < 0 && frand(0, 1) < 0.5f) { Act(p, "card", Find(p, AV_PEEK), 0); return; }
    if (frand(0, 1) < 0.25f) { for (int k : { AV_SCARF, AV_HAT, AV_CAP }) if (Count(p, k) >= 2) { Act(p, "card", Find(p, k), 0); return; } if (Find(p, AV_STEAL) >= 0) { Act(p, "card", Find(p, AV_STEAL), 0); return; } }
    DrawCard(p);
  }
  bool fin = false;
  void Update(float dt) override {
    Tick(dt); flash = fmaxf(0, flash - dt * 1.5f);
    if (over) { endT -= dt; if (endT <= 0 && !fin) { fin = true; std::vector<int> o; for (int i = 0; i < N(); i++) if (alive[i]) o.push_back(i); for (int j = (int)outOrder.size() - 1; j >= 0; j--) o.push_back(outOrder[j]); FinishOrder(o, [&](int i) { return alive[i] ? std::string("survived!") : std::string("buried"); }); } return; }
    if (peekFor >= 0) { peekT -= dt; if (peekT <= 0) { peekFor = -1; Sync(); } }
    if (pa.by >= 0) {
      pa.t -= dt;
      if (pa.blockedBy < 0 && (pa.card == AV_DOUBLE || pa.card == AV_STEAL)) for (int q = 0; q < N(); q++) if (P(q).bot && q != pa.by && alive[q] && Find(q, AV_BLOCK) >= 0 && frand(0, 1) < dt * 0.5f) { Act(q, "btn:block", 0, 0); break; }
      if (pa.t <= 0) Resolve();
      return;
    }
    int p = turn;
    if (state == "bury") { buryT -= dt; if (buryT <= 0 || (P(p).bot && buryT < 12.5f)) { const char* W[] = { "random", "top", "middle", "bottom", "2nd" }; Bury(P(p).bot ? W[rand() % 5] : "random"); } return; }
    if (P(p).bot) { botT -= dt; if (botT <= 0) { botT = frand(1, 1.8f); if (state == "target") { state = "play"; for (int q = 0; q < N(); q++) if (q != p && alive[q] && !hand[q].empty()) { Steal(p, q); break; } } else BotMove(p); } return; }
    timer -= dt; if (timer <= 0) { timer = 30; if (state == "target") { state = "play"; Sync(); } else DrawCard(p); }
  }
  void Draw() override {
    Room(); Seats([&](int i) { return i == turn && alive[i] && !over; }, [&](int i) { return over && i == winner ? 1.f : 0.f; });
    for (int i = 0; i < N(); i++) if (alive[i]) Backs(i, (int)hand[i].size(), Hex(0x1c3d7a));
    float d = Danger(), wob = sinf(time * (6 + d * 20)) * d * 0.12f;
    int dn = std::min(14, (int)deck.size());
    for (int k = 0; k < dn; k++) BigCard({ -0.6f + wob, 0.05f + k * 0.045f, 0.3f }, -0.03f + wob, BLANK, "", 1.5f, true, d > 0.25f ? Shade(Hex(0x1c3d7a), 0) : Hex(0x1c3d7a));
    if (d > 0.12f) lit.Draw(prim.cyl, TRS({ -0.6f, 0.03f, 0.3f }, { 0, 0, 0 }, { 1.7f + 0.1f * sinf(time * 8), 0.01f, 1.7f + 0.1f * sinf(time * 8) }), Fade(Hex(0xff4d5e), fminf(0.6f, d * 1.4f)));
    int s0 = std::max(0, (int)discard.size() - 4);
    for (int k = s0; k < (int)discard.size(); k++) BigCard({ 2.2f, 0.06f + (k - s0) * 0.03f, 0.3f }, (k % 3 - 1) * 0.25f, Hex(AVC[discard[k]]), AVI[discard[k]], 1.3f);
    End3D();
    if (flash > 0) DrawRectangle(0, 0, W, H, Fade(WHITE, flash));
    SeatNames([&](int i) { return alive[i] ? std::to_string(hand[i].size()) + " cards" + (i == turn && owed > 1 ? " · 2 turns" : "") : std::string("❄️ buried"); });
    Board("💣 STILL STANDING", Sorted([&](int i) { return alive[i] ? 1.f : 0.f; }), [&](int i) { return alive[i] ? std::string("✔") : std::string("❄️"); });
    Title("AVALANCHE!", Hex(0xff8787));
    // danger meter
    float mw = 420; RRect(800 - mw / 2, 190, mw, 40, 20, Hex(0x0a1230, 210), Hex(0x0a1230, 210), Hex(0xffffff, 120), 1.5f, false, true);
    RRect(800 - mw / 2 + 4, 194, fmaxf(32, (mw - 8) * fminf(1, d * 2.2f)), 32, 16, d > 0.3f ? Hex(0xff4d5e) : d > 0.15f ? Hex(0xff9f43) : Hex(0x34c77b), d > 0.3f ? Hex(0xc92a3a) : d > 0.15f ? Hex(0xe8590c) : Hex(0x2b8a3e), BLANK, 0, true);
    Text(800, 210, 18, WHITE, std::to_string(deck.size()) + " CARDS  ·  DANGER " + std::to_string((int)(d * 100)) + "%", 0, TS_HEAVY | TS_SHADOW);
    LogBar();
    if (pa.by >= 0) {
      RRect(560, 210, 480, 110, 24, Hex(0x14204a, 230), Hex(0x0a1230, 230), Hex(0xffffff, 200), 3, true, true);
      Text(800, 248, 40, pa.blockedBy >= 0 ? Hex(0x868e96) : Hex(0xffd43b), std::string(AVI[pa.card]) + " " + (pa.pair ? "PAIR → STEAL" : AVN[pa.card]), 0, TS_DISPLAY | TS_SHADOW);
      char b[48]; snprintf(b, sizeof b, "block it? %.1fs", fmaxf(0, pa.t));
      Text(800, 292, 22, pa.blockedBy >= 0 ? Hex(0xf783ac) : WHITE, pa.blockedBy >= 0 ? "✋ BLOCKED by " + P(pa.blockedBy).name + "!" : std::string(b), 0, TS_HEAVY | TS_PLAIN);
    }
    Hint("Play action cards, then DRAW · drew 💥? a 🛡️ SHOVEL saves you · last one standing wins", Hex(0xffc9c9));
  }
};

// ===================================================================================== SNOW TYCOON
struct Sq { const char* k; const char* n; int p; int g; const char* i; int a; };  // g colour group (-1 none)
const Sq TB[] = {
  { "start", "START", 0, -1, "🏁", 0 }, { "prop", "Igloo Lane", 60, 0, "", 0 }, { "chance", "Snow Chance", 0, -1, "❄️", 0 }, { "prop", "Penguin Pier", 70, 0, "", 0 }, { "lift", "North Lift", 150, -1, "🚡", 0 }, { "tax", "Plow Tax", 0, -1, "🚜", 80 },
  { "tub", "Hot Tub", 0, -1, "♨️", 0 }, { "prop", "Cocoa Café", 100, 1, "", 0 }, { "prop", "Frost Market", 110, 1, "", 0 }, { "chance", "Snow Chance", 0, -1, "❄️", 0 }, { "lift", "East Lift", 150, -1, "🚡", 0 }, { "prop", "Mitten Mall", 130, 2, "", 0 },
  { "pot", "Sleigh Ride", 0, -1, "🛷", 0 }, { "prop", "Sled Shop", 140, 2, "", 0 }, { "chance", "Snow Chance", 0, -1, "❄️", 0 }, { "prop", "Ice Rink", 170, 3, "", 0 }, { "prop", "Yeti Cave", 180, 3, "", 0 }, { "lift", "South Lift", 150, -1, "🚡", 0 },
  { "aval", "Avalanche!", 0, -1, "💥", 0 }, { "prop", "Aurora Hotel", 220, 4, "", 0 }, { "chance", "Snow Chance", 0, -1, "❄️", 0 }, { "prop", "Glacier Spa", 240, 4, "", 0 }, { "tax", "Luxury Tax", 0, -1, "💎", 120 }, { "prop", "Snow Palace", 300, 5, "", 0 },
};
const int NB = 24;
const unsigned TYC[] = { 0xa0522d, 0x22d3ee, 0xf783ac, 0xff922b, 0x9775fa, 0xffd43b };
const char* TYN[] = { "BROWN", "CYAN", "PINK", "ORANGE", "PURPLE", "GOLD" };
struct Chance { const char* t; int kind; int v; };  // kind: 0 cash 1 goto start 2 back 3 aval 4 each 5 cabin
const Chance TCH[] = { { "🏆 You won the ski race!", 0, 100 }, { "🛷 Your sled broke. Repairs:", 0, -60 }, { "🎿 Free lift pass — go to START!", 1, 0 }, { "❄️ Blizzard! Move back 3 spaces.", 2, 3 },
  { "💥 Caught in an avalanche — to the Hot Tub!", 3, 0 }, { "🎂 Snow party! Every player pays you $20.", 4, 20 }, { "☕ You buy everyone cocoa: pay $20 each.", 4, -20 }, { "🔨 Free cabin on one of your sets!", 5, 0 },
  { "🎁 Holiday bonus!", 0, 150 }, { "🧊 You slipped on ice. Doctor bill:", 0, -40 } };
struct Tycoon : GameNight {
  void PovPose(int who, float& ang, float& rad, float& h, float& fov, Vector3& tgt) override {   // leaning over the board from your seat: still your eyes, only a little higher so the squares read
    float a = SeatA(who); ang = atan2f(sinf(a) * 0.78f, cosf(a)); rad = 11.6f; h = 4.7f; fov = 58; tgt = { 0, -0.4f, 0 };
  }
  // the camera follows the token of whoever is moving around the board
  bool Focus(Vector3& p, float& w) override { if (over || turn < 0 || turn >= N()) return false; p = Cell3(st[turn].pos, 0); w = 1.3f; return true; }
  struct St { int pos = 0, cash = 1500, skip = 0; bool out = false; float shown = 1500; };
  std::vector<St> st; int own[NB], cab[NB], pot = 0, round = 1, ROUNDS = 8, dice[2] = { 1, 1 }, steps = 0; std::string state = "roll"; float timer = 25, botT = 1.3f, rollAnim = 0, stepT = 0, skipT = 0; std::vector<int> outOrder; bool fin = false;
  void Start() override {
    InitCam({ 1.8f, 10.6f, 14.2f }, { 0.1f, -0.8f, 0.2f }, 44); felt = Hex(0x1c3d7a);
    st.assign(N(), St{}); for (int i = 0; i < NB; i++) { own[i] = -1; cab[i] = 0; }
    turn = 0; Layout(-1, "{\"name\":\"panel\"}"); Banner("🏔️ SNOW TYCOON!", 1600, "#ffd43b"); log = "Roll the dice, buy spots, collect rent!"; Sync();
  }
  bool HasSet(int p, int g) { for (int i = 0; i < NB; i++) if (TB[i].g == g && own[i] != p) return false; return true; }
  int Rent(int i) {
    int o = own[i]; if (o < 0) return 0;
    if (std::string(TB[i].k) == "lift") { int n = 0; for (int j = 0; j < NB; j++) if (std::string(TB[j].k) == "lift" && own[j] == o) n++; return 25 << (n - 1); }
    int base = (int)roundf(TB[i].p / 8.f); const int M[] = { 1, 4, 9, 16 };
    if (cab[i]) return base * M[cab[i]]; return HasSet(o, TB[i].g) ? base * 2 : base;
  }
  int Worth(int p) { int w = st[p].cash; for (int i = 0; i < NB; i++) if (own[i] == p) w += TB[i].p + cab[i] * TB[i].p / 2; return w; }
  std::vector<int> Buildable(int p) { std::vector<int> o; for (int i = 0; i < NB; i++) if (std::string(TB[i].k) == "prop" && own[i] == p && HasSet(p, TB[i].g) && cab[i] < 3 && st[p].cash >= TB[i].p / 2) o.push_back(i); return o; }
  std::string PanelFor(int p) {
    St& s = st[p]; bool my = p == turn && !over && !s.out; std::vector<PBtn> bt; std::vector<PCard> cs;
    if (s.out) return PanelJ("💸 Bankrupt!", "Watch the tycoons…", log, {}, {}, false);
    if (my && state == "roll") { bt.push_back({ "roll", "🎲 ROLL", "#ffd43b", false, true }); auto b = Buildable(p); for (size_t k = 0; k < b.size() && k < 3; k++) bt.push_back({ "build_" + std::to_string(b[k]), std::string("🏠 Cabin on ") + TB[b[k]].n + " ($" + std::to_string(TB[b[k]].p / 2) + ")", Hx(Hex(TYC[TB[b[k]].g])) }); }
    if (my && state == "buy") { const Sq& b = TB[s.pos]; bt.push_back({ "buy", std::string("💰 BUY ") + b.n + " — $" + std::to_string(b.p), "#51cf66", s.cash < b.p, true }); bt.push_back({ "pass", "No thanks", "#868e96" }); }
    for (int i = 0; i < NB; i++) if (own[i] == p) { std::string ic = std::string(TB[i].k) == "lift" ? "🚡" : cab[i] ? std::string(cab[i] == 1 ? "🏠" : cab[i] == 2 ? "🏠🏠" : "🏠🏠🏠") : "📍"; cs.push_back({ TB[i].n, "rent $" + std::to_string(Rent(i)), ic, TB[i].g >= 0 ? Hx(Hex(TYC[TB[i].g])) : "#495057", false }); }
    std::string title = my ? (state == "buy" ? std::string("Buy ") + TB[s.pos].n + "?" : state == "roll" ? "🎲 Your turn — roll!" : "…") : P(turn).name + "'s turn";
    return PanelJ(title, "💵 $" + std::to_string(s.cash) + " · worth $" + std::to_string(Worth(p)), "Round " + std::to_string(round) + "/" + std::to_string(ROUNDS) + " · " + log, cs, bt, my);
  }
  void Sync() { for (int i = 0; i < N(); i++) if (!P(i).bot) Send(i, PanelFor(i)); }
  void Act(int p, const std::string& k, int, float) override {
    if (over || p != turn || k.rfind("btn:", 0) != 0) return;
    std::string id = k.substr(4); St& s = st[p];
    if (state == "roll" && id == "roll") { dice[0] = irand(1, 6); dice[1] = irand(1, 6); state = "moving"; rollAnim = 0.9f; steps = dice[0] + dice[1]; stepT = 0.25f; log = "🎲 " + P(p).name + " rolled " + std::to_string(steps) + "!"; Sfx("dice"); Sync(); return; }
    if (state == "roll" && id.rfind("build_", 0) == 0) { int i = atoi(id.c_str() + 6); auto b = Buildable(p); if (std::find(b.begin(), b.end(), i) == b.end()) return; s.cash -= TB[i].p / 2; cab[i]++; log = std::string("🏠 ") + P(p).name + " built a cabin on " + TB[i].n + "!"; Sfx("pickup"); Sync(); return; }
    if (state == "buy" && id == "buy") { const Sq& b = TB[s.pos]; if (s.cash < b.p) return; s.cash -= b.p; own[s.pos] = p; log = "💰 " + P(p).name + " bought " + b.n + "!"; Sfx("win"); fx.Burst(Cell3(s.pos, 0.6f), 20, P(p).color, 4, 0.7f, 0.1f); if (b.g >= 0 && HasSet(p, b.g)) Banner("⭐ " + P(p).name + " owns all " + TYN[b.g] + "!", 1600, Hx(Hex(TYC[b.g])).c_str()); EndTurn(); return; }
    if (state == "buy" && id == "pass") { log = P(p).name + " passed on " + TB[s.pos].n; EndTurn(); return; }
  }
  void Pay(int p, int to, int amt) {
    St& s = st[p];
    while (s.cash < amt) {
      int ci = -1; for (int i = 0; i < NB; i++) if (cab[i] > 0 && own[i] == p) { ci = i; break; }
      if (ci >= 0) { cab[ci]--; s.cash += TB[ci].p / 4; continue; }
      int pi = -1; for (int i = 0; i < NB; i++) if (own[i] == p) { pi = i; break; }
      if (pi >= 0) { own[pi] = -1; s.cash += TB[pi].p / 2; continue; }
      break;
    }
    int paid = std::min(amt, std::max(0, s.cash)); s.cash -= amt; if (to >= 0) st[to].cash += paid;
    if (s.cash < 0) { s.out = true; s.cash = 0; outOrder.push_back(p); for (int i = 0; i < NB; i++) if (own[i] == p) own[i] = -1; Banner("💸 " + P(p).name + " is BANKRUPT!", 2000, "#ff4d6d"); }
  }
  void Land(int p) {
    St& s = st[p]; const Sq& b = TB[s.pos]; std::string k = b.k;
    if (k == "prop" || k == "lift") {
      int o = own[s.pos];
      if (o < 0) { state = "buy"; timer = 20; log = P(p).name + " landed on " + b.n + " ($" + std::to_string(b.p) + ")"; Sync(); return; }
      if (o != p && !st[o].out) { int r = Rent(s.pos); Pay(p, o, r); log = "💸 " + P(p).name + " pays $" + std::to_string(r) + " rent to " + P(o).name; Sfx("buzz"); }
      else log = P(p).name + " relaxes at their own " + b.n;
    } else if (k == "tax") { Pay(p, -1, b.a); pot += b.a; log = std::string(b.i) + " " + P(p).name + " pays " + b.n + " $" + std::to_string(b.a); }
    else if (k == "chance") {
      const Chance& c = TCH[rand() % 10]; log = "❄️ " + P(p).name + ": " + c.t + (c.kind == 0 ? " $" + std::to_string(abs(c.v)) : ""); Banner(c.t, 2000, "#a5d8ff");
      if (c.kind == 0) { if (c.v > 0) s.cash += c.v; else { Pay(p, -1, -c.v); pot += -c.v; } }
      if (c.kind == 1) { s.pos = 0; s.cash += 200; }
      if (c.kind == 2) { s.pos = (s.pos + NB - c.v) % NB; Land(p); return; }
      if (c.kind == 3) { cine::Moment(cine::SNOW, Cell3(s.pos, 0.6f), 1.3f, Hex(0xd0ebff), "AVALANCHE!", P(p).name, 0.f); s.pos = 6; s.skip = 1; Shake(14); Sfx("boom"); }
      if (c.kind == 4) for (int q = 0; q < N(); q++) if (q != p && !st[q].out) { if (c.v > 0) Pay(q, p, c.v); else Pay(p, q, -c.v); }
      if (c.kind == 5) for (int i = 0; i < NB; i++) if (std::string(TB[i].k) == "prop" && own[i] == p && HasSet(p, TB[i].g) && cab[i] < 3) { cab[i]++; break; }
    } else if (k == "aval") { cine::Moment(cine::SNOW, Cell3(s.pos, 0.6f), 1.3f, Hex(0xd0ebff), "AVALANCHE!", P(p).name, 0.f); s.pos = 6; s.skip = 1; log = "💥 AVALANCHE! " + P(p).name + " is stuck in the Hot Tub (miss a turn)"; Shake(16); Sfx("boom"); fx.Burst(Cell3(18, 1), 60, WHITE, 6, 1.2f, 0.2f, 3); }
    else if (k == "pot") { if (pot) { s.cash += pot; log = "🛷 " + P(p).name + " grabs the Sleigh Ride pot: $" + std::to_string(pot) + "!"; Sfx("win"); conf.Spawn(Cell3(12, 1), 60, 2, 4); pot = 0; } else log = "🛷 " + P(p).name + " enjoys a sleigh ride"; }
    else log = P(p).name + " is at " + b.n;
    EndTurn();
  }
  void EndTurn() {
    state = "roll"; int alive = 0; for (auto& s : st) alive += !s.out;
    if (alive <= 1) { Finish2(); return; }
    int n = 0; do { turn = (turn + 1) % N(); if (turn == 0) round++; n++; } while (st[turn].out && n < 20);
    if (round > ROUNDS) { Finish2(); return; }
    if (st[turn].skip) { st[turn].skip = 0; log = "♨️ " + P(turn).name + " is relaxing in the Hot Tub…"; state = "skip"; skipT = 1.6f; Sync(); return; }
    timer = 25; botT = frand(1, 1.6f); Sync();
  }
  void Finish2() { if (over) return; over = true; endT = 3.5f; int b = Sorted([&](int i) { return (float)Worth(i); })[0]; conf.Spawn({ 0, 3, 0 }, 160, 6, 6); Sfx("win"); Banner("🏆 " + P(b).name + " is the SNOW TYCOON!", 2600, Hx(P(b).color).c_str()); Sync();
    cine::Shot sh; sh.kind = cine::WIN; sh.at = { 0, 0.6f, 0 }; sh.size = 2.4f; sh.c = P(b).color; sh.floorY = 0; sh.elev = 0.5f; cine::Play(sh); }
  void BotMove(int p) {
    St& s = st[p];
    if (state == "roll") { auto b = Buildable(p); if (!b.empty() && s.cash - TB[b[0]].p / 2 > 150) { Act(p, "btn:build_" + std::to_string(b[0]), 0, 0); return; } Act(p, "btn:roll", 0, 0); return; }
    if (state == "buy") Act(p, s.cash - TB[s.pos].p > 120 ? "btn:buy" : "btn:pass", 0, 0);
  }
  void Update(float dt) override {
    Tick(dt);
    for (auto& s : st) s.shown += (s.cash - s.shown) * fminf(1, dt * 6);
    if (over) { endT -= dt; if (endT <= 0 && !fin) { fin = true; auto o = Sorted([&](int i) { return (float)Worth(i) + (st[i].out ? -100000.f : 0); }); FinishOrder(o, [&](int i) { return "$" + std::to_string(Worth(i)) + " worth"; }); } return; }
    if (state == "skip") { skipT -= dt; if (skipT <= 0) EndTurn(); return; }
    if (state == "moving") {
      rollAnim = fmaxf(0, rollAnim - dt); if (rollAnim > 0) return;
      stepT -= dt;
      if (stepT <= 0 && steps > 0) { St& s = st[turn]; s.pos = (s.pos + 1) % NB; if (s.pos == 0) { s.cash += 200; Sfx("ding"); fx.Burst(Cell3(0, 0.8f), 20, Hex(0x51cf66), 4, 0.7f, 0.1f); } steps--; stepT = 0.24f; Sfx("tick"); if (steps == 0) Land(turn); }
      return;
    }
    if (P(turn).bot) { botT -= dt; if (botT <= 0) { botT = frand(1, 1.6f); BotMove(turn); } return; }
    timer -= dt; if (timer <= 0) { timer = 25; Act(turn, state == "buy" ? "btn:pass" : "btn:roll", 0, 0); }
  }
  // board: 24 squares around a square, 7 per side (corners shared)
  const float BS = 11.2f;
  Vector3 Cell3(int i, float y = 0) {
    int side = i / 6, k = i % 6; float c = BS / 7, h = BS / 2 - c / 2;
    if (side == 0) return { h - k * c, y, h };
    if (side == 1) return { -h, y, h - k * c };
    if (side == 2) return { -h + k * c, y, -h };
    return { h, y, -h + k * c };
  }
  void Draw() override {
    Room(false);
    // the board on a low wooden table
    lit.Draw(prim.cyl, TRS({ 0, -3.2f, 0 }, { 0, 0, 0 }, { 1.4f, 2.5f, 1.4f }), Shade(wood, -0.3f));
    lit.Pattern(PAT_WOOD); lit.Draw(prim.cube, TRS({ 0, -0.5f, 0 }, { 0, 0, 0 }, { BS + 1.2f, 0.5f, BS + 1.2f }), Hex(0x8a5a35)); lit.Pattern(PAT_NONE);
    lit.Draw(prim.cube, TRS({ 0, -0.22f, 0 }, { 0, 0, 0 }, { BS + 0.4f, 0.08f, BS + 0.4f }), Hex(0xe7f5ff));
    lit.Draw(prim.cube, TRS({ 0, -0.17f, 0 }, { 0, 0, 0 }, { BS * 5 / 7 - 0.2f, 0.02f, BS * 5 / 7 - 0.2f }), Hex(0x1c3d7a));
    float c = BS / 7;
    for (int i = 0; i < NB; i++) {
      Vector3 p = Cell3(i, -0.14f); const Sq& b = TB[i];
      lit.Draw(prim.cube, TRS(p, { 0, 0, 0 }, { c * 0.92f, 0.06f, c * 0.92f }), own[i] >= 0 ? Shade(P(own[i]).color, 0.6f) : Hex(0xf8f9fa));
      if (b.g >= 0) { int side = i / 6; Vector3 o = { 0, 0, 0 }; float bandW = c * 0.92f, bandD = c * 0.24f; Vector3 sc = { bandW, 0.02f, bandD };
        if (side == 0) o.z = -c * 0.34f; else if (side == 2) o.z = c * 0.34f; else { sc = { bandD, 0.02f, bandW }; o.x = side == 1 ? c * 0.34f : -c * 0.34f; }
        lit.Draw(prim.cube, TRS({ p.x + o.x, p.y + 0.04f, p.z + o.z }, { 0, 0, 0 }, sc), Hex(TYC[b.g])); }
      for (int k = 0; k < cab[i]; k++) { Vector3 q = { p.x - 0.35f + k * 0.35f, p.y + 0.15f, p.z }; lit.Draw(prim.cube, TRS(q, { 0, 0, 0 }, { 0.26f, 0.22f, 0.26f }), Hex(0xc0703a)); lit.Draw(prim.cone, TRS({ q.x, q.y + 0.11f, q.z }, { 0, PI / 4, 0 }, { 0.26f, 0.2f, 0.26f }), Hex(0xe03131)); }
      if (b.i[0]) { Label({ p.x, p.y + 0.06f, p.z + 0.3f }, c * 0.8f, b.n, Hex(0x0a1024), 0.36f); Label({ p.x, p.y + 0.07f, p.z - 0.3f }, c * 0.8f, b.i, WHITE, 0.6f); }
      else if (b.p) Label({ p.x, p.y + 0.06f, p.z + 0.1f }, c * 0.8f, std::string(b.n) + "\n" + (own[i] >= 0 ? "rent $" + std::to_string(Rent(i)) : "$" + std::to_string(b.p)), own[i] >= 0 ? Hex(0x7a1010) : Hex(0x0a1024), 0.44f);   // name over price, on one label: they can no longer collide
      else Label({ p.x, p.y + 0.06f, p.z }, c * 0.8f, b.n, Hex(0x0a1024), 0.36f);
    }
    // dice in the middle
    for (int k = 0; k < 2; k++) Die({ -0.5f + k * 1.0f, -0.15f, 1.0f }, rollAnim > 0 ? irand(1, 6) : dice[k], rollAnim, 0.5f);   // real dice size: they used to be nearly as big as a square
    // tokens: little characters hopping round the board
    for (int i = 0; i < N(); i++) {
      if (st[i].out) continue; Vector3 p = Cell3(st[i].pos, -0.1f); float ox = ((i % 3) - 1) * 0.42f, oz = (i / 3 - 0.5f) * 0.42f;
      float hop = i == turn && state == "moving" ? fabsf(sinf(stepT * 13)) * 0.45f : 0;
      DrawToon({ p.x + ox, p.y, p.z + oz }, 0, P(i).color, time + i, hop, over && Sorted([&](int q) { return (float)Worth(q); })[0] == i ? 1.f : 0, 0.42f);
    }
    End3D();
    Vector2 m = GetWorldToScreen({ 0, 0, -1.6f }, cam);
    Text(m.x, m.y, 40, Hex(0xffe066), "SNOW TYCOON", 0, TS_DISPLAY | TS_SHADOW | TS_GOLD);
    Text(m.x, m.y + 40, 20, Hex(0xa5d8ff), "Round " + std::to_string(std::min(round, ROUNDS)) + " / " + std::to_string(ROUNDS) + (pot ? " · 🛷 pot $" + std::to_string(pot) : ""), 0, TS_HEAVY | TS_PLAIN);
    for (int i = 0; i < N(); i++) if (!st[i].out) { Vector3 p = Cell3(st[i].pos, 1.0f); Vector2 s = GetWorldToScreen({ p.x + ((i % 3) - 1) * 0.42f, p.y, p.z + (i / 3 - 0.5f) * 0.42f }, cam); if (i == turn) Text(s.x, s.y - 8, 18, P(i).color, P(i).name, 0, TS_HEAVY); }
    Board("💵 CASH (worth)", Sorted([&](int i) { return (float)Worth(i) - (st[i].out ? 100000.f : 0); }), [&](int i) { return st[i].out ? std::string("bankrupt") : "$" + std::to_string((int)st[i].shown); });
    LogBar();
    Hint("🎲 Roll · BUY spots · own a whole colour to build 🏠 cabins · richest after " + std::to_string(ROUNDS) + " rounds wins", Hex(0xffe066));
  }
};
}  // namespace

static RegGame c1("storm", [] { return std::unique_ptr<Game>(new Storm); });
static RegGame c2("avalanche", [] { return std::unique_ptr<Game>(new Aval); });
static RegGame c3("tycoon", [] { return std::unique_ptr<Game>(new Tycoon); });
