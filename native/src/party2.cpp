// More studio games: Imposter (talk & vote), Copy Cat (remember the colours) and Doodle Dash
// (draw on your phone, everyone guesses). Same stage + podiums as the quiz shows.
#include "show.h"
#include "data.h"
#include <sstream>

using namespace kit;
namespace {

// ===================================================================================== IMPOSTER
// Everyone gets the secret word, except ONE player (the imposter) who only gets a CLUE: a word that connects to it. Everybody says one word that fits,
// they talk it out, and vote. No timers: the round moves on when everybody is ready (the host can push it on).
struct Elf : Show {   // (the game id stays "elf")
  int R = 3, round = 0, elf = -1, host = -1; std::string cat, word, clue, phase = "intro", result; float t = 3, phaseT = 0, nxt = -1;
  std::vector<int> order, votes, cnt; std::vector<bool> ready; std::vector<std::string> guessOpts, usedWords; std::vector<float> botT;
  int Humans() { int h = 0; for (int i = 0; i < N(); i++) h += !P(i).bot; return h; }
  void Start() override {
    InitStage(); tint = Hex(0xd6336c); R = std::min(3, std::max(2, N() - 1));
    for (int i = 0; i < N(); i++) if (!P(i).bot) { host = i; break; }
    Layout(-1, Msg("🕵️", "Imposter", "Your secret word is coming…"));
  }
  void Next() {
    round++;
    if (round > R) { phase = "end"; Win(); return; }
    const data::Impo* e = &data::IMPO[0];
    for (int tries = 0; tries < 40; tries++) { e = &data::IMPO[rand() % data::IMPO.size()]; if (std::find(usedWords.begin(), usedWords.end(), std::string(e->word)) == usedWords.end()) break; }
    cat = e->cat; word = e->word; clue = e->clue; usedWords.push_back(word);
    std::vector<int> humans; for (int i = 0; i < N(); i++) if (!P(i).bot) humans.push_back(i);
    elf = humans.size() >= 3 ? Pick(humans) : rand() % N();
    order.clear(); for (int i = 0; i < N(); i++) order.push_back(i); Shuffle(order);
    phase = "talk"; phaseT = 0; nxt = -1; ready.assign(N(), false); votes.assign(N(), -1); cnt.assign(N(), 0);
    for (int i = 0; i < N(); i++) if (!P(i).bot)
      Layout(i, Choice(i == elf ? "You are the 🕵️ IMPOSTER\n" + clue + "\nYour CLUE: a word connected to the real secret word. Say ONE word that fits it — and blend in!"
                                : "Your secret word\n" + word + "\nSay ONE word that fits it. One of you only has a clue: find the imposter!",
                       { "✅ Ready to vote" }, 1, true, { i == elf ? "#e8590c" : "#34c77b" }, {}, 0, true));
    Sfx("whoosh");
  }
  int Ready() { int r = 0; for (int q = 0; q < N(); q++) if (!P(q).bot) r += ready[q]; return r; }
  int Voted() { int v = 0; for (int x : votes) v += x >= 0; return v; }
  void Act(int p, const std::string& k, int i, float) override {
    if (k != "choice" || p < 0 || p >= N()) return;
    if (phase == "talk") {
      if (ready[p]) { if (p == host && i == 0) nxt = 0.2f; return; }   // the host's button once they are ready: "vote now"
      ready[p] = true; Sfx("tick");
      if (!P(p).bot) Layout(p, p == host ? Choice("Waiting for the others… (" + std::string(p == elf ? "your clue: " + clue : "the word: " + word) + ")", { "⏩ Everyone is done: vote now" }, 1, false, { "#f08c00" })
                                          : Msg("⏳", "Waiting for the others…", p == elf ? "Shh… you are the imposter 🕵️ · your clue: " + clue : "The word: " + word));
      if (Humans() && Ready() >= Humans()) nxt = 0.8f;
    } else if (phase == "vote") {
      if (votes[p] >= 0) { if (p == host && !P(p).bot && i == 0) nxt = 0.2f; return; }   // the host's button once they have voted: "count the votes now"
      if (i < 0 || i >= N() || i == p) return;
      votes[p] = i; cnt[i]++; Sfx("tick");
      if (!P(p).bot) Layout(p, p == host ? Choice("You voted " + P(i).name + ". Waiting for the others…", { "⏩ Count the votes now" }, 1, false, { "#f08c00" }) : Msg("🗳️", "You voted " + P(i).name, "Waiting for the others…"));
      if (Voted() >= N()) nxt = 0.7f;
    } else if (phase == "guess") {
      if (p == elf && i >= 0 && i < (int)guessOpts.size()) Guess(guessOpts[i]);
      else if (p == host && p != elf && i == 0) Guess("");   // the host skips a guess that never comes
    }
  }
  void StartVote() {
    phase = "vote"; phaseT = 0; nxt = -1; Sfx("alarm"); Banner("🗳️ VOTE: WHO IS THE IMPOSTER?", 1800, "#ffd43b");
    std::vector<std::string> names, cols; for (int i = 0; i < N(); i++) { names.push_back(P(i).name); cols.push_back(Hx(P(i).color)); }
    botT.assign(N(), 0);
    for (int i = 0; i < N(); i++) { if (!P(i).bot) { std::vector<bool> off(N(), false); off[i] = true; Layout(i, Choice("Who is the imposter?", names, 2, false, cols, off)); } else botT[i] = frand(1.5f, 7); }
  }
  void Tally() {
    nxt = -1;
    int mx = 0, top = -1, ties = 0; for (int i = 0; i < N(); i++) { if (cnt[i] > mx) { mx = cnt[i]; top = i; ties = 1; } else if (cnt[i] == mx && mx > 0) ties++; }
    if (ties == 1 && top == elf) {   // caught: one last chance, name the secret word
      phase = "guess"; phaseT = 0;
      guessOpts.clear(); for (auto& e : data::IMPO) if (cat == e.cat) guessOpts.push_back(e.word);
      Shuffle(guessOpts); if (guessOpts.size() > 6) guessOpts.resize(6); if (std::find(guessOpts.begin(), guessOpts.end(), word) == guessOpts.end()) guessOpts[rand() % guessOpts.size()] = word;
      Banner("CAUGHT! " + P(elf).name + " WAS THE IMPOSTER!", 2200, Hx(P(elf).color).c_str()); Sfx("ding");
      for (int i = 0; i < N(); i++) if (!P(i).bot)
        Layout(i, i == elf ? Choice("Last chance! What was the secret word? (your clue was " + clue + ")", guessOpts, 2)
                           : i == host ? Choice(P(elf).name + " is guessing the word…", { "⏩ Skip the guess" }, 1, false, { "#f08c00" }) : Msg("🕵️", P(elf).name + " is guessing the word…"));
    } else {
      Gain(elf, 300); Banner("THE IMPOSTER ESCAPED! It was " + P(elf).name, 2600, Hx(P(elf).color).c_str()); Sfx("buzz");
      Result(P(elf).name + " fooled everyone!");
    }
  }
  void Guess(const std::string& w) {
    if (phase != "guess") return;
    if (w == word) { Gain(elf, 300); Result(P(elf).name + " named the word and steals the win!"); Sfx("win"); }
    else { for (int i = 0; i < N(); i++) if (i != elf) Gain(i, votes[i] == elf ? 200 : 100); Result("Wrong guess: the crew wins!"); conf.Spawn({ 0, 4, 0 }, 100, 6, 6); Sfx("win"); }
  }
  void Result(const std::string& s) { phase = "result"; t = 8; phaseT = 0; result = s; for (int i = 0; i < N(); i++) if (!P(i).bot) Layout(i, Msg("🕵️", s, "Secret word: " + word + " · the imposter's clue: " + clue)); }
  void Update(float dt) override {
    Tick(dt); t -= dt; phaseT += dt;
    if (nxt > 0) { nxt -= dt; if (nxt <= 0) { nxt = -1; if (phase == "talk") StartVote(); else if (phase == "vote") Tally(); } }
    if (Ending(dt)) return;
    if (phase == "intro") { if (t <= 0) Next(); }
    else if (phase == "talk") { for (int i = 0; i < N(); i++) if (P(i).bot && !ready[i] && phaseT > 4 + i) ready[i] = true; }
    else if (phase == "vote") { for (int i = 0; i < N(); i++) if (P(i).bot && votes[i] < 0 && phaseT > botT[i]) { int v; do v = (frand(0, 1) < 0.35f && i != elf) ? elf : rand() % N(); while (v == i); Act(i, "choice", v, 0); } }
    else if (phase == "guess") { if (P(elf).bot && phaseT > 2.5f) Guess(frand(0, 1) < 0.35f ? word : Pick(guessOpts)); }
    else if (phase == "result" && t <= 0) Next();
  }
  void Draw() override {
    Stage([&](int i) { return (phase == "talk" && ready[i]) || (phase == "vote" && votes[i] >= 0); });
    Glass(78, 14, 330, 76, 20);   // the heading sits on glass: the neon frame no longer cuts through it
    Text(110, 42, 20, Hex(0xdfe4f2), "ROUND " + I(std::min(round, R)) + " OF " + I(R), -1, TS_PLAIN | TS_HEAVY | TS_SPACED);
    Text(110, 70, 26, WHITE, "IMPOSTER", -1, TS_DISPLAY | TS_SHADOW | TS_GOLD);
    if (phase == "intro") { Glass(400, 230, 800, 260, 34); Text(800, 295, 20, Hex(0xc9d3f2), "TALK · BLUFF · FIND THE IMPOSTER", 0, TS_PLAIN | TS_HEAVY | TS_SPACED); Text(800, 365, 80, WHITE, "IMPOSTER", 0, TS_DISPLAY | TS_SHADOW | TS_GOLD);
      Text(800, 440, 24, Hex(0xdfe6ff), "Everybody knows the word — except one of you, who only has a clue", 0, TS_HEAVY | TS_PLAIN, 740); }
    else if (phase == "talk") {
      Glass(110, 108, 1380, 420, 30);
      Text(800, 148, 20, Hex(0xc9d3f2), "THE SECRET WORD IS ON YOUR PHONES", 0, TS_PLAIN | TS_HEAVY | TS_SPACED);
      Text(800, 210, 64, WHITE, "FIND THE IMPOSTER", 0, TS_DISPLAY | TS_SHADOW | TS_GOLD);
      Text(800, 285, 28, WHITE, "Everyone knows the word — except ONE imposter, who only got a clue that connects to it.", 0, TS_HEAVY | TS_PLAIN, 1250);
      Text(800, 335, 28, Hex(0xffe9a8), "Go round the circle: say ONE word that fits the secret word. Talk, argue, accuse — no timer!", 0, TS_HEAVY | TS_PLAIN, 1250);
      Text(800, 405, 30, Hex(0x8ce99a), "✅ " + I(Ready()) + " of " + I(Humans()) + " ready to vote", 0, TS_HEAVY | TS_SHADOW | TS_PLAIN);
      Text(800, 468, 22, Hex(0xc9d3f2), "Tap \"Ready to vote\" on your phone when you are sure who it is", 0, TS_HEAVY | TS_PLAIN);
    } else if (phase == "vote") {
      Glass(310, 150, 980, 220, 30);
      Text(800, 230, 66, WHITE, "WHO IS THE IMPOSTER?", 0, TS_DISPLAY | TS_SHADOW | TS_GOLD);
      Text(800, 305, 26, Hex(0xc9d3f2), "Vote on your phone · " + I(Voted()) + " of " + I(N()) + " voted", 0, TS_HEAVY | TS_PLAIN);
    } else if (phase == "guess" || phase == "result") {
      bool res = phase == "result"; Glass(210, 110, 1180, res ? 400 : 230, 30);
      Text(800, 175, 40, WHITE, phase == "guess" ? P(elf).name + " is guessing the word…" : result, 0, TS_HEAVY | TS_SHADOW | TS_PLAIN, 1100);
      if (res) {
        Text(800, 245, 46, Hex(0xffe066), "The word was: " + word, 0, TS_DISPLAY | TS_SHADOW);
        Text(800, 295, 24, Hex(0xdfe6ff), "The imposter (" + P(elf).name + ") only had the clue: " + clue, 0, TS_HEAVY | TS_PLAIN, 1100);
        for (int i = 0; i < N(); i++) Text(N() <= 4 ? 800 : 800 + (i % 2 ? 250 : -250), N() <= 4 ? 352 + i * 34 : 352 + (i / 2) * 34, 22, P(i).color, P(i).name + " → " + (votes[i] >= 0 ? P(votes[i]).name : std::string("—")), 0, TS_HEAVY | TS_PLAIN);   // who voted for whom
      }
    } else if (phase == "end") Heading("FINAL SCORES", "IMPOSTER", 70, 52);
    PodiumLabels([&](int i) -> std::string {
      if (phase == "talk") { int k = (int)(std::find(order.begin(), order.end(), i) - order.begin()); return I(k + 1) + (ready[i] ? " ✅" : ""); }
      if (phase == "vote") return votes[i] >= 0 ? "🗳️" : "";
      if (phase == "guess" || phase == "result") return (i == elf ? "🕵️ " : "") + I(cnt[i]) + (cnt[i] == 1 ? " vote" : " votes");
      return "";
    });
  }
};

// ===================================================================================== COPY CAT
const unsigned PADC[] = { 0xff4d6d, 0x3d8bff, 0xffc93c, 0x34c77b };
const char* PADN[] = { "RED", "BLUE", "YELLOW", "GREEN" };
struct CopyCat : Show {
  int R = 6, round = 0, len = 0, k = -1, litPad = -1; std::vector<int> seq; std::vector<std::vector<int>> entry; std::vector<int> res; std::vector<float> botNext;
  std::string phase = "intro"; float t = 3, tMax = 1; std::vector<Spring> padP = std::vector<Spring>(4);
  void Start() override { InitStage(); tint = Hex(0xffc93c); Layout(-1, Msg("🐱", "Copy Cat", "Watch the TV and remember the colours!")); }
  void Next() {
    round++; if (round > R) { phase = "end"; Win(); return; }
    len = round + 2; seq.clear(); for (int i = 0; i < len; i++) seq.push_back(rand() % 4);
    phase = "show"; t = 1.2f; k = -1; litPad = -1; entry.assign(N(), {}); res.assign(N(), -1); botNext.assign(N(), 0);
    Layout(-1, Msg("👀", "WATCH THE TV!", "Remember " + I(len) + " colours"));
  }
  void Act(int p, const std::string& kk, int i, float) override {
    if (phase != "input" || kk != "choice" || p < 0 || p >= N() || res[p] >= 0 || i < 0 || i > 3) return;
    auto& e = entry[p]; e.push_back(i); litPad = i; padP[i].v -= 6;
    size_t n = e.size();
    if (e[n - 1] != seq[n - 1]) {  // partial credit for every colour you got right
      res[p] = (int)(n - 1) * 40; Gain(p, res[p]); Sfx("buzz");
      if (!P(p).bot) Layout(p, Msg("❌", "Oops!", "You got " + I((int)n - 1) + " of " + I(len) + " right" + (res[p] ? " (+" + I(res[p]) + ")" : "")));
    } else if ((int)n == len) {
      res[p] = 100 * len + (int)(t * 10); Gain(p, res[p]); Sfx("ding");
      if (!P(p).bot) Layout(p, Msg("✅", "+" + I(res[p]), "Perfect memory!"));
    }
    bool all = true; for (int r : res) all &= r >= 0; if (all) t = fminf(t, 0.5f);
  }
  void Update(float dt) override {
    Tick(dt); t -= dt;
    for (int i = 0; i < 4; i++) padP[i].To(i == litPad ? 1.f : 0.f, dt, 260, 22);
    if (Ending(dt)) return;
    if (phase == "intro") { if (t <= 0) Next(); }
    else if (phase == "show") {
      if (t <= 0) {
        if (litPad >= 0) { litPad = -1; t = 0.28f; return; }
        k++;
        if (k >= len) {
          phase = "input"; t = tMax = 6 + len * 1.6f;
          std::vector<std::string> L, C; for (int i = 0; i < 4; i++) { L.push_back(PADN[i]); C.push_back(Hx(Hex(PADC[i]))); }
          for (int i = 0; i < N(); i++) if (!P(i).bot) Layout(i, Choice("Tap the " + I(len) + " colours in order!", L, 2, true, C, {}, len)); else botNext[i] = frand(1.2f, 2.5f);
          return;
        }
        litPad = seq[k]; t = fmaxf(0.5f, 0.85f - round * 0.04f); Sfx("tick");
      }
    } else if (phase == "input") {
      for (int i = 0; i < N(); i++) if (P(i).bot && res[i] < 0 && tMax - t > botNext[i]) {
        size_t n = entry[i].size(); bool slip = frand(0, 1) < 0.04f * len;
        Act(i, "choice", slip ? (seq[n] + 1) % 4 : seq[n], 0); botNext[i] += frand(0.45f, 0.8f);
      }
      if (litPad >= 0 && padP[litPad].x > 0.95f) litPad = -1;
      if (t <= 0) {
        for (int i = 0; i < N(); i++) if (res[i] < 0) { res[i] = (int)entry[i].size() * 40; Gain(i, res[i]); if (!P(i).bot) Layout(i, Msg("⏰", "Time up!", "")); }
        phase = "reveal"; t = 3;
      }
    } else if (phase == "reveal" && t <= 0) Next();
  }
  void Draw() override {
    Stage([&](int i) { return phase == "input" && res.size() == (size_t)N() && res[i] >= 0; });
    Glass(78, 14, 450, 76, 20);   // the heading sits on glass: the neon frame no longer cuts through it
    Text(110, 42, 20, Hex(0xdfe4f2), "ROUND " + I(std::min(std::max(round, 1), R)) + " OF " + I(R) + " · " + I(len) + " COLOURS", -1, TS_PLAIN | TS_HEAVY | TS_SPACED);
    Text(110, 70, 26, WHITE, "COPY CAT", -1, TS_DISPLAY | TS_SHADOW | TS_GOLD);
    if (phase == "intro") { Glass(400, 230, 800, 240, 34); Text(800, 300, 20, Hex(0xc9d3f2), "WATCH · REMEMBER · REPEAT", 0, TS_PLAIN | TS_HEAVY | TS_SPACED); Text(800, 370, 80, WHITE, "COPY CAT", 0, TS_DISPLAY | TS_SHADOW | TS_GOLD); PodiumLabels(); return; }
    if (phase == "end") { Heading("FINAL SCORES", "COPY CAT", 70, 52); PodiumLabels(); return; }
    // four giant pads
    float pw = 330, ph = 176, gx = 24, x0 = 800 - pw - gx / 2, y0 = 120;
    for (int i = 0; i < 4; i++) {
      float x = x0 + (i % 2) * (pw + gx), y = y0 + (i / 2) * (ph + 26);
      float on = padP[i].x;
      Color c = Hex(PADC[i]);
      Tile(x, y, pw, ph, on > 0.05f ? Shade(c, 0.25f * on) : Shade(c, -0.35f), 0.45f + 0.55f * fmaxf(on, 0.35f), Clamp(on, 0, 1), on > 0.5f);
      Text(x + pw / 2, TileTop(y, Clamp(on, 0, 1)) + ph / 2, 40, Fade(WHITE, 0.5f + 0.5f * on), PADN[i], 0, TS_DISPLAY | TS_SHADOW);
    }
    if (phase == "input") { Timer(t, tMax, 1490, 66); HintBar("Now tap the colours on your phone, in order!", 66); }
    if (phase == "show") HintBar("Watch closely… " + I(std::max(0, k + 1)) + " / " + I(len), 66, Hex(0xffe9a8));
    if (phase == "reveal") { std::string s; for (int v : seq) s += std::string(s.empty() ? "" : " · ") + PADN[v]; HintBar(s, 66, Hex(0xffe066)); }
    PodiumLabels([&](int i) -> std::string { if (phase != "input" || res[i] < 0) return ""; return res[i] >= 100 * len ? "✅" : "❌"; });
  }
};

// ===================================================================================== DOODLE DASH
struct Doodle : Show {
  struct Stroke { std::string id; Color c; float w; std::vector<Vector2> p; };
  std::vector<Stroke> strokes; std::vector<int> drawers; std::vector<std::string> words, opts; std::string word, phase = "intro";
  int R = 0, round = 0, drawer = -1; float t = 3, tMax = 1; std::vector<int> guess; std::vector<float> botAt;
  const float CX = 470, CY = 100, CW = 660, CH = 660 * 690.f / 1100;   // small enough that the players on their podiums stay in view under it
  void Start() override {
    InitStage(); tint = Hex(0xff8f3c); words = data::DOODLE; Shuffle(words);
    for (int i = 0; i < N(); i++) if (!P(i).bot) drawers.push_back(i);
    Shuffle(drawers); if (drawers.size() > 5) drawers.resize(5); if (drawers.size() == 1) drawers.push_back(drawers[0]);
    R = (int)drawers.size();
    Layout(-1, Msg("✏️", "Doodle Dash", "Get your drawing finger ready!"));
  }
  void Next() {
    round++; if (round > R) { phase = "end"; Win(); return; }
    drawer = drawers[round - 1]; word = words.back(); words.pop_back();
    opts = { word }; std::vector<std::string> rest(words.begin(), words.end()); Shuffle(rest); for (int i = 0; i < 3; i++) opts.push_back(rest[i]); Shuffle(opts);
    strokes.clear(); guess.assign(N(), -1); botAt.assign(N(), frand(12, 30)); phase = "draw"; t = tMax = 60;
    for (int i = 0; i < N(); i++) if (!P(i).bot) Layout(i, i == drawer ? "{\"name\":\"draw\",\"word\":" + Q(word) + "}" : Choice("What is " + P(drawer).name + " drawing?", opts, 2));
    Banner("✏️ " + P(drawer).name + " is drawing!", 1500, Hx(P(drawer).color).c_str());
  }
  void Act(int p, const std::string& k, int i, float) override {
    if (phase != "draw" || p < 0 || p >= N()) return;
    if (p == drawer) {
      if (k.rfind("stroke:", 0) == 0) {
        std::stringstream ss(k.substr(7)); std::string id, col, w, pts;
        std::getline(ss, id, '|'); std::getline(ss, col, '|'); std::getline(ss, w, '|'); std::getline(ss, pts);
        Stroke* s = nullptr; for (auto& x : strokes) if (x.id == id) s = &x;
        if (!s) { strokes.push_back({ id, ParseColor(col), Clamp((float)atof(w.c_str()), 2, 40), {} }); s = &strokes.back(); }
        std::stringstream ps(pts); std::string pt;
        while (std::getline(ps, pt, ';')) { float x = 0, y = 0; if (sscanf(pt.c_str(), "%f,%f", &x, &y) == 2) s->p.push_back({ Clamp(x, 0, 1), Clamp(y, 0, 1) }); }
      } else if (k == "clear") strokes.clear();
      return;
    }
    if (k != "choice" || guess[p] >= 0 || i < 0 || i >= (int)opts.size()) return;
    bool ok = opts[i] == word; guess[p] = ok;
    if (ok) { int g = 300 + (int)(700 * t / tMax) / 10 * 10; Gain(p, g); Gain(drawer, 150); Sfx("ding"); }
    else Sfx("buzz");
    if (!P(p).bot) Layout(p, Msg(ok ? "✅" : "❌", ok ? "You got it!" : "Wrong!", ok ? "Nice eye!" : "Better luck next round"));
    int left = 0; for (int q = 0; q < N(); q++) if (q != drawer && guess[q] < 0) left++;
    if (!left) t = fminf(t, 0.6f);
  }
  void Update(float dt) override {
    Tick(dt); t -= dt;
    if (Ending(dt)) return;
    if (phase == "intro") { if (t <= 0) { if (R == 0) { phase = "end"; Win(); } else Next(); } }
    else if (phase == "draw") {
      for (int i = 0; i < N(); i++) if (P(i).bot && i != drawer && guess[i] < 0 && tMax - t > botAt[i]) { int pick = frand(0, 1) < 0.6f ? (int)(std::find(opts.begin(), opts.end(), word) - opts.begin()) : rand() % 4; Act(i, "choice", pick, 0); }
      if (t <= 0) { phase = "reveal"; t = 3.5f; Sfx("win"); for (int i = 0; i < N(); i++) if (!P(i).bot && (i == drawer || guess[i] < 0)) Layout(i, Msg("✏️", "It was: " + word)); }
    } else if (phase == "reveal" && t <= 0) Next();
  }
  void Draw() override {
    Stage([&](int i) { return phase == "draw" && guess.size() == (size_t)N() && guess[i] >= 0; });
    // the canvas (GL, so the strokes sit on it)
    DrawRectangleRounded({ CX + 6, CY + 10, CW, CH }, 0.04f, 8, Fade(BLACK, 0.35f));
    DrawRectangleRounded({ CX - 8, CY - 8, CW + 16, CH + 16 }, 0.04f, 8, Hex(0xc9a24f));
    DrawRectangleRounded({ CX, CY, CW, CH }, 0.035f, 8, Hex(0xfffdf7));
    for (auto& s : strokes) {
      float w = s.w * CW / 600;
      for (size_t k = 0; k < s.p.size(); k++) {
        Vector2 a = { CX + s.p[k].x * CW, CY + s.p[k].y * CH };
        DrawCircleV(a, w / 2, s.c);
        if (k) { Vector2 b = { CX + s.p[k - 1].x * CW, CY + s.p[k - 1].y * CH }; DrawLineEx(b, a, w, s.c); }
      }
    }
    Glass(78, 14, 330, 76, 20);   // the heading sits on glass: the neon frame no longer cuts through it
    Text(110, 42, 20, Hex(0xdfe4f2), "ROUND " + I(std::min(std::max(round, 1), std::max(R, 1))) + " OF " + I(R), -1, TS_PLAIN | TS_HEAVY | TS_SPACED);
    Text(110, 70, 26, WHITE, "DOODLE DASH", -1, TS_DISPLAY | TS_SHADOW | TS_GOLD);
    if (phase == "draw") {
      Timer(t, tMax, 1490, 66);
      std::string blanks; for (char c : word) blanks += c == ' ' ? "   " : "_ ";
      HintBar("✏️ " + P(drawer).name + " is drawing:  " + blanks, 66);
    } else if (phase == "reveal") HintBar("It was: " + word, 66, Hex(0xffe066));
    else if (phase == "intro") { Text(800, CY + CH / 2, 70, Hex(0x1a1438), "DOODLE DASH", 0, TS_DISPLAY | TS_PLAIN); if (R == 0) Text(800, CY + CH / 2 + 70, 26, Hex(0x495057), "Needs at least one phone to draw on", 0, TS_HEAVY | TS_PLAIN); }
    else if (phase == "end") Heading("FINAL SCORES", "DOODLE DASH", 70, 52);
    PodiumLabels([&](int i) -> std::string { if (phase != "draw") return ""; if (i == drawer) return "✏️"; return guess[i] < 0 ? "" : guess[i] ? "✅" : "❌"; });
  }
};
}  // namespace

static RegGame r1("elf", [] { return std::unique_ptr<Game>(new Elf); });
static RegGame r2("copycat", [] { return std::unique_ptr<Game>(new CopyCat); });
static RegGame r3("doodle", [] { return std::unique_ptr<Game>(new Doodle); });
