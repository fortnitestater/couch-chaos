// QUIZ SHOW games on the studio stage: Trivia Blitz, Color Clash, Count It!, Quick Math, Most Likely To,
// Lowest Unique, RPS Royale and Bomb Squad. Answers are big buttons on the phone (the question is shown
// there too, so nobody has to look back and forth). Generous timers; any right answer is worth 600+.
#include "show.h"
#include "data.h"

using namespace kit;
namespace {

struct Round {
  std::string text, phone, reveal; std::vector<std::string> opts; std::vector<Color> colors;
  int correct = -1; float time = 16; int cols = 0; bool big = false, hideOpts = false; std::vector<bool> good;
};

struct QuizShow : Show {
  std::string title, kicker; int R = 8, round = 0; std::string phase = "intro"; float t = 3.0f, tMax = 1, phaseT = 0;
  Round q; std::vector<int> ans, gain; std::vector<float> at, botAt; float botSkill = 0.62f;
  std::vector<Spring> tileIn;
  virtual Round Make(int n) = 0;
  virtual int BotAnswer(int p) { if (q.correct >= 0 && frand(0, 1) < botSkill) return q.correct; return rand() % (int)q.opts.size(); }
  virtual void Resolve() {
    for (int i = 0; i < N(); i++) if (ans[i] >= 0 && ans[i] == q.correct) gain[i] = 600 + (int)(400 * at[i] / tMax) / 10 * 10;
  }
  virtual void Board(float x, float y, float w, float h) {}
  virtual std::string PhoneLayout() { return ""; }  // custom phone screen (gesture) instead of answer buttons   // custom picture inside the big screen
  virtual bool ShowTiles() { return !q.hideOpts; }
  virtual float PanelBottom() { return ShowTiles() ? 268 : 590; }
  void Start() override {
    InitStage();
    Layout(-1, Msg("📺", "Get ready…", "The answers will pop up right here on your phone"));
    Sfx("whoosh");
  }
  void Next() {
    round++;
    if (round > R) { phase = "end"; Win(); for (int i = 0; i < N(); i++) if (!P(i).bot) Layout(i, Msg(Order()[0] == i ? "🏆" : "🎉", Order()[0] == i ? "You won!" : P(Order()[0]).name + " won!", "Final score: " + I(pts[i]))); return; }
    q = Make(round);
    if (q.good.empty()) q.good.assign(q.opts.size(), false);
    if (q.correct >= 0) q.good[q.correct] = true;
    ans.assign(N(), -1); gain.assign(N(), 0); at.assign(N(), 0); botAt.assign(N(), 0);
    tileIn.assign(q.opts.size(), Spring{});
    phase = "ask"; t = tMax = q.time; phaseT = 0;
    std::vector<std::string> cols; for (size_t i = 0; i < q.opts.size(); i++) cols.push_back(i < q.colors.size() ? Hx(q.colors[i]) : OPT[i % 10]);
    int c = q.cols ? q.cols : (q.opts.size() > 4 ? 3 : q.opts.size() == 3 ? 3 : 2);
    for (int i = 0; i < N(); i++) {
      if (!P(i).bot) { std::string pl = PhoneLayout(); std::string ch = Choice(q.phone.empty() ? q.text : q.phone, q.opts, c, q.big, cols); ch.insert(ch.size() - 1, ",\"letters\":true"); Layout(i, pl.empty() ? ch : pl); }
      else botAt[i] = frand(1.6f, fmaxf(2.0f, fminf(8.0f, tMax - 2)));
    }
    Sfx("whoosh");
  }
  void Act(int p, const std::string& k, int i, float) override {
    if (phase != "ask" || k != "choice" || p < 0 || p >= N() || ans[p] >= 0 || i < 0 || i >= (int)q.opts.size()) return;
    ans[p] = i; at[p] = t; Sfx("tick");
    if (!P(p).bot && PhoneLayout().empty()) Layout(p, Msg("🔒", "Locked in!", q.opts[i]));
    bool all = true; for (int v : ans) all &= v >= 0;
    if (all) t = fminf(t, 0.6f);
  }
  void Reveal() {
    phase = "reveal"; t = 4.2f; phaseT = 0;
    Resolve();
    bool any = false;
    for (int i = 0; i < N(); i++) {
      Gain(i, gain[i]); if (gain[i] > 0) any = true;
      if (!P(i).bot) Layout(i, Msg(ans[i] < 0 ? "⏰" : gain[i] > 0 ? "✅" : "❌", ans[i] < 0 ? "Too slow!" : gain[i] > 0 ? "+" + I(gain[i]) + " points!" : "Not this time", q.reveal));
    }
    Sfx(any ? "ding" : "buzz");
    if (any) fx.Burst({ 0, 5, 0 }, 30, Hex(0xffd43b), 6, 0.9f, 0.12f, 4);
  }
  void Update(float dt) override {
    Tick(dt); t -= dt; phaseT += dt;
    if (Ending(dt)) return;
    for (size_t i = 0; i < tileIn.size(); i++) tileIn[i].To(phaseT > 0.08f * i ? 1.f : 0.f, dt, 170, 24);
    if (phase == "intro") { if (t <= 0) Next(); return; }
    if (phase == "ask") {
      for (int i = 0; i < N(); i++) if (P(i).bot && ans[i] < 0 && tMax - t >= botAt[i]) Act(i, "choice", BotAnswer(i), 0);
      if (t <= 0) Reveal();
    } else if (phase == "reveal" && t <= 0) Next();
  }
  // ----- drawing -----
  void Tiles(float y0, float y1) {
    int n = (int)q.opts.size(); if (!n) return;
    int cols = q.cols ? q.cols : (n > 4 ? 3 : n == 3 ? 3 : 2), rows = (n + cols - 1) / cols;
    float X0 = 110, Wd = 1380, gap = 18, cw = (Wd - gap * (cols - 1)) / cols, ch = fminf(118, (y1 - y0 - gap * (rows - 1)) / rows);
    float total = rows * ch + (rows - 1) * gap, oy = y0 + (y1 - y0 - total) / 2;
    bool rev = phase == "reveal";
    for (int i = 0; i < n; i++) {
      float s = tileIn[i].x; if (s < 0.01f) continue;
      float x = X0 + (i % cols) * (cw + gap), y = oy + (i / cols) * (ch + gap) + (1 - s) * 40;
      bool good = rev && q.good[i];
      float alpha = fminf(1, s) * (rev && !good && (q.correct >= 0 || std::count(q.good.begin(), q.good.end(), true)) ? 0.35f : 1);
      Color c = i < (int)q.colors.size() ? q.colors[i] : ParseColor(OPT[i % 10]);
      float pulse = good ? 0.5f + 0.5f * sinf(phaseT * 7) : 0;
      Tile(x, y, cw, ch, c, alpha, good ? 0.15f * pulse : 0, good);
      float ty = TileTop(y, good ? 0.15f * pulse : 0) + ch / 2;
      RRect(x + 16, ty - 23, 46, 46, 23, Hex(0xffffff, (unsigned char)(235 * alpha)), Hex(0xffffff, (unsigned char)(235 * alpha)), BLANK, 0);
      Text(x + 39, ty + 1, 24, Fade(Shade(c, -0.5f), alpha), std::string(1, (char)('A' + i)), 0, TS_DISPLAY | TS_PLAIN);
      float fs = q.big ? fminf(66, ch * 0.55f) : 36;
      while (fs > 16 && Measure(q.opts[i], fs, TS_DISPLAY) > cw - 110) fs -= 2;
      bool lightC = (c.r * 299 + c.g * 587 + c.b * 114) / 1000 > 175;
      (void)lightC; Text(x + cw / 2 + 22, ty, fs, Fade(WHITE, alpha), q.opts[i], 0, TS_DISPLAY | TS_SHADOW);
      if (rev) {  // who picked it
        int k = 0;
        for (int p = 0; p < N(); p++) if (ans[p] == i) { float px = x + cw - 30 - k * 26, py = TileTop(y) + 22; RRect(px - 12, py - 12, 24, 24, 12, P(p).color, P(p).color, WHITE, 2.5f); k++; }
      }
    }
  }
  void Draw() override {
    Stage([&](int i) { return phase == "ask" && ans.size() == (size_t)N() && ans[i] >= 0; });
    // header
    std::string kick = phase == "intro" || round == 0 ? kicker : "ROUND " + I(std::min(round, R)) + " OF " + I(R);
    if (phase == "intro") {
      float k = EaseBack(fminf(1, phaseT * 1.6f));
      Glass(400, 230 - 20 * (1 - k), 800, 260, 34);
      Text(800, 300, 20, Hex(0xc9d3f2), kicker, 0, TS_PLAIN | TS_HEAVY | TS_SPACED);
      Text(800, 370, 84 * k, WHITE, title, 0, TS_DISPLAY | TS_SHADOW | TS_GOLD);
      Text(800, 440, 26, Hex(0xdfe6ff), I(R) + " rounds · answer on your phone", 0, TS_HEAVY | TS_PLAIN);
      PodiumLabels(); return;
    }
    if (phase == "end") { Heading("FINAL SCORES", title, 70, 52); PodiumLabels(); return; }
    Text(110, 42, 17, Hex(0xc9d3f2), kick, -1, TS_PLAIN | TS_HEAVY | TS_SPACED);
    Text(110, 70, 26, WHITE, title, -1, TS_DISPLAY | TS_SHADOW | TS_GOLD);
    if (phase == "ask") Timer(t, tMax, 1490, 66);
    bool tiles = ShowTiles();
    float qb = PanelBottom();
    Glass(110, 108, 1380, qb - 108, 30, 205);
    if (!q.text.empty()) {
      float fs = q.text.size() > 70 ? 40 : 50;
      if (!tiles) fs = 36;
      Text(800, (108 + qb) / 2 - (q.text.size() > (tiles ? 48 : 64) ? fs * 0.6f : 0), fs, WHITE, q.text, 0, TS_HEAVY | TS_SHADOW | TS_PLAIN, 1260);
    }
    Board(110, 108, 1380, qb - 108);
    if (tiles) Tiles(290, 578);
    if (phase == "reveal" && !q.reveal.empty()) {
      float k = EaseBack(fminf(1, phaseT * 3));
      float w = Measure(q.reveal, 32, TS_HEAVY) + 70;
      RRect(800 - w / 2, qb - 27, w, 54 * k, 27, Hex(0xfff3ce), Hex(0xf2c66d), BLANK, 0, true);
      if (k > 0.6f) Text(800, qb, 30, Hex(0x2b1a00), q.reveal, 0, TS_HEAVY | TS_PLAIN);
    } else if (phase == "ask") {
      int n = 0; for (int v : ans) n += v >= 0;
      Text(1420, 66, 20, Hex(0xc9d3f2), I(n) + " / " + I(N()) + " answered", 1, TS_HEAVY | TS_PLAIN);
    }
    PodiumLabels([&](int i) { return phase == "ask" && ans[i] >= 0 ? std::string("🔒") : std::string(); });
  }
};

// ---------------- TRIVIA BLITZ ----------------
struct Trivia : QuizShow {
  std::vector<int> bank;
  Trivia() { title = "TRIVIA BLITZ"; kicker = "FAMILY QUIZ"; R = 8; for (size_t i = 0; i < data::TRIVIA.size(); i++) bank.push_back((int)i); Shuffle(bank); tint = Hex(0x4d7cff); }
  Round Make(int) override {
    auto& row = data::TRIVIA[bank.back()]; bank.pop_back();
    Round r; r.text = row[0]; r.time = 18;
    std::vector<int> o = { 1, 2, 3, 4 }; Shuffle(o);
    for (int k : o) r.opts.push_back(row[k]);
    for (int k = 0; k < 4; k++) if (o[k] == 1) r.correct = k;
    r.reveal = "Answer: " + row[1];
    return r;
  }
};

// ---------------- COLOR CLASH ----------------
const char* CCN[] = { "RED", "BLUE", "GREEN", "YELLOW", "PURPLE", "ORANGE" };
const unsigned CCC[] = { 0xff4d6d, 0x4dabff, 0x51cf66, 0xffd43b, 0xcc5de8, 0xff922b };
struct Clash : QuizShow {
  int word = 0, ink = 0; bool inkMode = true;
  Clash() { title = "COLOR CLASH"; kicker = "BRAIN TWISTER"; R = 10; tint = Hex(0xff4d6d); }
  Round Make(int n) override {
    word = rand() % 6; do ink = rand() % 6; while (ink == word);
    inkMode = n % 3 != 0;
    int target = inkMode ? ink : word;
    std::vector<int> o = { word, ink }; std::vector<int> rest; for (int i = 0; i < 6; i++) if (i != word && i != ink) rest.push_back(i); Shuffle(rest); o.push_back(rest[0]); o.push_back(rest[1]); Shuffle(o);
    Round r; r.time = 9; r.big = true;
    r.phone = inkMode ? "Tap the INK colour (the colour the word is painted in)" : "Tap what the WORD says";
    for (int k = 0; k < 4; k++) { r.opts.push_back(CCN[o[k]]); r.colors.push_back(Hex(0x3a4670)); if (o[k] == target) r.correct = k; }
    r.reveal = std::string("It was ") + CCN[target];
    return r;
  }
  void Board(float x, float y, float w, float h) override {
    Text(800, y + 34, 22, Hex(0xffe9a8), inkMode ? "TAP THE INK COLOUR" : "TAP WHAT THE WORD SAYS", 0, TS_PLAIN | TS_HEAVY | TS_SPACED);
    Text(800, y + h / 2 + 14, 96, Hex(CCC[ink]), CCN[word], 0, TS_DISPLAY | TS_SHADOW);
  }
};

// ---------------- COUNT IT ----------------
const char* CNT[][2] = { { "🐧", "🦭" }, { "⛄", "🎄" }, { "❄️", "⭐" }, { "🍪", "🧁" }, { "🦌", "🐻" }, { "🎁", "🧦" }, { "🐟", "🐠" } };
struct CountIt : QuizShow {
  struct E { float x, y, r; bool a; }; std::vector<E> items; int set = 0, real = 0; const float SHOW = 4.5f;
  CountIt() { title = "COUNT IT!"; kicker = "SHARP EYES"; R = 7; tint = Hex(0x22c3d6); }
  Round Make(int n) override {
    set = rand() % 7; items.clear();
    int A = irand(4, 6 + n * 2), Bn = irand(2, 3 + n);
    auto place = [&](bool a) { for (int t = 0; t < 80; t++) { float x = frand(170, 1430), y = frand(150, 545); bool ok = true; for (auto& e : items) if (hypotf(e.x - x, e.y - y) < 74) { ok = false; break; } if (ok) { items.push_back({ x, y, frand(-0.3f, 0.3f), a }); return; } } };
    for (int i = 0; i < A; i++) place(true);
    for (int i = 0; i < Bn; i++) place(false);
    real = 0; for (auto& e : items) real += e.a;
    std::vector<int> s = { real }; while (s.size() < 4) { int v = std::max(1, real + irand(-3, 3)); if (std::find(s.begin(), s.end(), v) == s.end()) s.push_back(v); }
    Shuffle(s);
    Round r; r.time = 15; r.big = true; r.text = ""; r.phone = std::string("How many ") + CNT[set][0] + " did you see?";
    for (int k = 0; k < 4; k++) { r.opts.push_back(I(s[k])); if (s[k] == real) r.correct = k; }
    r.reveal = "There were " + I(real) + " " + CNT[set][0];
    return r;
  }
  bool Showing() { return phase == "reveal" || (phase == "ask" && tMax - t < SHOW); }
  bool ShowTiles() override { return !Showing() || phase == "reveal"; }
  void Board(float x, float y, float w, float h) override {
    if (phase == "ask" && Showing()) {
      Text(800, y + 30, 24, Hex(0xffe9a8), std::string("COUNT THE ") + CNT[set][0] + "  " + I((int)ceilf(SHOW - (tMax - t))), 0, TS_DISPLAY | TS_SHADOW);
      for (auto& e : items) Text(e.x, e.y + 20, 62, WHITE, CNT[set][e.a ? 0 : 1], 0, TS_PLAIN);
    } else if (phase == "ask") Text(800, (y + y + h) / 2, 52, WHITE, std::string("How many ") + CNT[set][0] + " were there?", 0, TS_DISPLAY | TS_SHADOW);
    else Text(800, (y + y + h) / 2, 70, Hex(0xffe066), I(real) + " " + CNT[set][0], 0, TS_DISPLAY | TS_SHADOW);
  }
};

// ---------------- QUICK MATH ----------------
struct Maths : QuizShow {
  std::string eq;
  Maths() { title = "QUICK MATH"; kicker = "NUMBER CRUNCH"; R = 8; tint = Hex(0x34c77b); }
  Round Make(int n) override {
    int a, b, ans; std::string op;
    if (n <= 4) { a = irand(2, 15); b = irand(1, 10); if (rand() % 2) { op = "+"; ans = a + b; } else { if (b > a) std::swap(a, b); op = "−"; ans = a - b; } }
    else if (n <= 7) { a = irand(2, 10); b = irand(2, 5); op = "×"; ans = a * b; }
    else { b = irand(2, 5); ans = irand(2, 10); a = b * ans; op = "÷"; }
    eq = I(a) + " " + op + " " + I(b);
    std::vector<int> s = { ans }; const int D[] = { -10, -2, -1, 1, 2, 10, 3, -3 };
    while (s.size() < 4) { int v = std::max(0, ans + D[rand() % 8]); if (std::find(s.begin(), s.end(), v) == s.end()) s.push_back(v); }
    Shuffle(s);
    Round r; r.time = 14; r.big = true; r.phone = eq + " = ?";
    for (int k = 0; k < 4; k++) { r.opts.push_back(I(s[k])); if (s[k] == ans) r.correct = k; }
    r.reveal = eq + " = " + I(ans);
    return r;
  }
  void Board(float x, float y, float w, float h) override { Text(800, y + h / 2 + 4, 100, WHITE, eq + " = ?", 0, TS_DISPLAY | TS_SHADOW); }
};

// ---------------- MOST LIKELY TO ----------------
struct Likely : QuizShow {
  std::vector<std::string> bank;
  Likely() { title = "MOST LIKELY TO"; kicker = "VOTE FOR YOUR FAMILY"; R = 7; bank = data::LIKELY; Shuffle(bank); tint = Hex(0xf06595); }
  Round Make(int) override {
    Round r; std::string pr = bank.back(); bank.pop_back();
    r.text = "Who is most likely to " + pr; r.time = 18;
    for (int i = 0; i < N(); i++) { r.opts.push_back(P(i).name); r.colors.push_back(P(i).color); }
    r.cols = N() > 4 ? 3 : 2;
    return r;
  }
  int BotAnswer(int) override { return rand() % N(); }
  void Resolve() override {
    std::vector<int> cnt(N(), 0); int mx = 0;
    for (int a : ans) if (a >= 0) mx = std::max(mx, ++cnt[a]);
    q.good.assign(N(), false); std::string names;
    for (int i = 0; i < N(); i++) if (mx > 0 && cnt[i] == mx) { q.good[i] = true; names += (names.empty() ? "" : " & ") + P(i).name; gain[i] += 100; }
    for (int i = 0; i < N(); i++) if (ans[i] >= 0 && q.good[ans[i]]) gain[i] += 400;
    q.reveal = mx ? names + "! (" + I(mx) + " vote" + (mx == 1 ? "" : "s") + ")" : "Nobody voted!";
  }
};

// ---------------- LOWEST UNIQUE ----------------
struct Lowest : QuizShow {
  Lowest() { title = "LOWEST UNIQUE"; kicker = "OUTSMART EVERYONE"; R = 6; tint = Hex(0x22c3d6); }
  Round Make(int) override {
    Round r; r.text = "Pick the LOWEST number that NOBODY else picks!"; r.time = 14; r.cols = 5; r.big = true;
    for (int i = 1; i <= 10; i++) r.opts.push_back(I(i));
    return r;
  }
  int BotAnswer(int) override { return std::min(9, (int)(powf(frand(0, 1), 1.6f) * 10)); }
  void Resolve() override {
    std::vector<int> cnt(10, 0); for (int a : ans) if (a >= 0) cnt[a]++;
    q.good.assign(10, false);
    for (int v = 0; v < 10; v++) if (cnt[v] == 1) {
      for (int i = 0; i < N(); i++) if (ans[i] == v) { gain[i] = 1000; q.good[v] = true; q.reveal = P(i).name + " wins with " + I(v + 1) + "!"; }
      return;
    }
    q.reveal = "Everyone clashed! Nobody wins.";
  }
};

// ---------------- RPS ROYALE ----------------
struct RPS : QuizShow {
  RPS() { title = "RPS ROYALE"; kicker = "EVERYONE VS EVERYONE"; R = 6; tint = Hex(0xb86bff); }
  Round Make(int) override {
    Round r; r.text = "Beat as many people as you can!"; r.time = 10; r.big = true; r.cols = 3;
    r.opts = { "✊ Rock", "✋ Paper", "✌️ Scissors" }; r.colors = { Hex(0xff8f3c), Hex(0x3d8bff), Hex(0x34c77b) };
    return r;
  }
  int BotAnswer(int) override { return rand() % 3; }
  void Resolve() override {
    int c[3] = { 0, 0, 0 };
    for (int i = 0; i < N(); i++) if (ans[i] >= 0) { c[ans[i]]++; int w = 0; for (int j = 0; j < N(); j++) if (j != i && ans[j] >= 0 && (ans[i] - ans[j] + 3) % 3 == 1) w++; gain[i] = w * 150; }
    q.reveal = "✊ " + I(c[0]) + "    ✋ " + I(c[1]) + "    ✌️ " + I(c[2]);
    int best = 0; for (int k = 1; k < 3; k++) if (c[k] > c[best]) best = k;
    q.good.assign(3, false); for (int i = 0; i < N(); i++) if (gain[i] > 0 && ans[i] >= 0) q.good[ans[i]] = true;
  }
};

// ---------------- BOMB SQUAD ----------------
const char* WN[] = { "RED", "BLUE", "YELLOW", "GREEN", "WHITE", "BLACK" };
const unsigned WC[] = { 0xe03131, 0x1c7ed6, 0xfcc419, 0x2f9e44, 0xf1f3f5, 0x343a40 };
const char* ORD[] = { "first", "second", "third", "fourth", "fifth", "sixth" };
struct Bomb : QuizShow {
  std::vector<int> wires; int ansW = 0; bool boom = false; float boomT = 0;
  Bomb() { title = "BOMB SQUAD"; kicker = "CUT THE RIGHT WIRE"; R = 8; tint = Hex(0xff6b3c); botSkill = 0.6f; }
  Round Make(int n) override {
    int count = std::min(6, 3 + n / 3); std::string rule; boom = false;
    for (int tries = 0; tries < 300; tries++) {
      wires.clear(); for (int i = 0; i < count; i++) wires.push_back(rand() % 6);
      auto cnt = [&](int c) { int k = 0; for (int w : wires) k += w == c; return k; };
      auto last = [&](int c) { int k = -1; for (int i = 0; i < count; i++) if (wires[i] == c) k = i; return k; };
      int kind = n <= 3 ? rand() % 2 : rand() % 6;
      if (kind == 0) { int k = rand() % count; rule = std::string("Cut the ") + ORD[k] + " wire."; ansW = k; }
      else if (kind == 1) { int c = wires[rand() % count]; rule = std::string("Cut the LAST ") + WN[c] + " wire."; ansW = last(c); }
      else if (kind == 2) { rule = "More than one RED wire? Cut the last RED one. Otherwise cut the second wire."; ansW = cnt(0) > 1 ? last(0) : 1; }
      else if (kind == 3) { int u = -1, nu = 0; for (int i = 0; i < count; i++) if (cnt(wires[i]) == 1) { nu++; u = i; } if (nu != 1) continue; rule = "Cut the only wire whose colour appears just ONCE."; ansW = u; }
      else if (kind == 4) { int i = -1; for (int k = 0; k < count; k++) if (wires[k] == 1) { i = k; break; } if (i < 0 || i == count - 1) continue; rule = "Cut the wire right AFTER the first BLUE wire."; ansW = i + 1; }
      else { rule = "Is there a YELLOW wire? Cut the first wire. Otherwise cut the last wire."; ansW = cnt(2) ? 0 : count - 1; }
      break;
    }
    Round r; r.text = rule; r.time = 18; r.hideOpts = true; r.correct = ansW; r.cols = count > 4 ? 3 : 2;
    r.phone = "📖 " + rule;
    for (int i = 0; i < count; i++) { r.opts.push_back(I(i + 1) + " " + WN[wires[i]]); r.colors.push_back(Hex(WC[wires[i]])); }
    r.reveal = "Correct wire: #" + I(ansW + 1) + " " + WN[wires[ansW]];
    return r;
  }
  void Resolve() override {
    QuizShow::Resolve();
    bool wrong = false; for (int a : ans) if (a >= 0 && a != ansW) wrong = true;
    if (wrong) { boom = true; boomT = 0; fx.Burst({ 0, 3.6f, -3 }, 70, Hex(0xff922b), 9, 1.2f, 0.25f, 3); fx.Burst({ 0, 3.6f, -3 }, 40, Hex(0xffd43b), 7, 1.0f, 0.2f, 2); Sfx("boom"); Shake(16); flash = 1; for (int i = 0; i < N(); i++) if (ans[i] >= 0 && ans[i] != ansW) Buzz(i, 500); }
    else fx.Burst({ 0, 3.6f, -3 }, 40, Hex(0x51cf66), 5, 0.8f, 0.15f, 1);
  }
  float WY(int i) { int n = (int)wires.size(); return 5.35f - (i + 0.5f) * (2.4f / n); }
  void Props3D() override {
    float z = -3, sh = boom && phase == "reveal" ? sinf(time * 60) * 0.05f * fmaxf(0, 1 - phaseT) : 0;
    lit.Draw(prim.cube, TRS({ sh, 4.15f, z - 0.4f }, { 0, 0, 0 }, { 9.2f, 3.0f, 0.8f }), Hex(0x5c6672));
    lit.Draw(prim.cube, TRS({ sh, 4.15f, z - 0.1f }, { 0, 0, 0 }, { 8.8f, 2.6f, 0.3f }), Hex(0x2b3036));
    for (int a = -1; a <= 1; a += 2) for (int b = -1; b <= 1; b += 2) lit.Draw(prim.sphere, TRS({ a * 4.3f + sh, 4.15f + b * 1.3f, z }, { 0, 0, 0 }, { 0.22f, 0.22f, 0.22f }), Hex(0xadb5bd));
    lit.Draw(prim.cube, TRS({ sh, 5.95f, z - 0.2f }, { 0, 0, 0 }, { 2.6f, 0.7f, 0.5f }), Hex(0x111111));
    for (int k = 0; k < 4; k++) lit.Draw(prim.cyl, TRS({ -3.2f + k * 0.45f + sh, 5.65f, z - 0.4f }, { 0, 0, 0 }, { 0.2f, 0.9f, 0.2f }), Hex(0xd6336c));
    for (int k = 0; k < 4; k++) lit.Draw(prim.cyl, TRS({ 1.85f + k * 0.45f + sh, 5.65f, z - 0.4f }, { 0, 0, 0 }, { 0.2f, 0.9f, 0.2f }), Hex(0xd6336c));
    int n = (int)wires.size();
    for (int i = 0; i < n; i++) {
      float y = WY(i); bool cut = phase == "reveal" && i == ansW;
      Color c = Hex(WC[wires[i]]);
      const int S = 14;
      for (int k = 0; k < S; k++) {
        if (cut && (k == S / 2 || k == S / 2 - 1)) continue;
        float u0 = (float)k / S, u1 = (float)(k + 1) / S;
        auto at = [&](float u) { float dy = sinf(u * PI * 2 + i) * 0.12f; if (cut) dy -= (u > 0.5f ? (1 - u) : u) * 0.4f * fminf(1, phaseT * 3); return Vector3{ -3.6f + u * 7.2f + sh, y + dy, z + 0.15f }; };
        Limb(at(u0), at(u1), 0.1f, c);
      }
      lit.Draw(prim.sphere, TRS({ -3.75f + sh, y, z + 0.1f }, { 0, 0, 0 }, { 0.16f, 0.16f, 0.16f }), Hex(0xadb5bd));
      lit.Draw(prim.sphere, TRS({ 3.75f + sh, y, z + 0.1f }, { 0, 0, 0 }, { 0.16f, 0.16f, 0.16f }), Hex(0xadb5bd));
    }
  }
  void Board(float x, float y, float w, float h) override {
    Vector2 lcd = GetWorldToScreen({ 0, 5.95f, -2.9f }, cam);
    float secs = phase == "ask" ? fmaxf(0, t) : 0;
    Text(lcd.x, lcd.y, 34, (secs < 4 && ((int)(secs * 4)) % 2) ? Hex(0xff8787) : Hex(0xff2a2a), "00:" + std::string(secs < 9.5f ? "0" : "") + I((int)ceilf(secs)), 0, TS_DISPLAY | TS_PLAIN);
    for (int i = 0; i < (int)wires.size(); i++) { Vector2 p = GetWorldToScreen({ -4.05f, WY(i), -2.85f }, cam); Text(p.x, p.y, 26, WHITE, I(i + 1), 0, TS_DISPLAY | TS_SHADOW); }
  }
  void Draw() override {
    QuizShow::Draw();
  }
  bool ShowTiles() override { return false; }
  float PanelBottom() override { return 214; }
  std::string PhoneLayout() override { std::string w; for (size_t i = 0; i < wires.size(); i++) w += std::string(i ? "," : "") + "{\"color\":\"" + Hx(Hex(WC[wires[i]])) + "\"}"; return "{\"name\":\"wires\",\"prompt\":" + Q(q.phone) + ",\"wires\":[" + w + "]}"; }
};
}  // namespace

std::unique_ptr<Game> MakeTrivia() { return std::make_unique<Trivia>(); }
std::unique_ptr<Game> MakeClash() { return std::make_unique<Clash>(); }
std::unique_ptr<Game> MakeCount() { return std::make_unique<CountIt>(); }
std::unique_ptr<Game> MakeMath() { return std::make_unique<Maths>(); }
std::unique_ptr<Game> MakeLikely() { return std::make_unique<Likely>(); }
std::unique_ptr<Game> MakeLowest() { return std::make_unique<Lowest>(); }
std::unique_ptr<Game> MakeRPS() { return std::make_unique<RPS>(); }
std::unique_ptr<Game> MakeBomb() { return std::make_unique<Bomb>(); }
