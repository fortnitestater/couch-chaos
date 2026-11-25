// QUICK-FIRE studio games: Reflex Rush (tap on GO!, not on the fakes) and Stop the Meter (stop the needle in gold).
// One big button on the phone. Easier: obvious fakes, a wider gold zone, a slower needle.
#include "show.h"

using namespace kit;
namespace {

// ===================================================================================== REFLEX RUSH
const char* FAKES[] = { "GOAT", "NO!", "GO?", "SO!", "WAIT", "GOLF", "STOP", "G0" };
struct Reflex : Show {
  int R = 6, round = 0; std::string phase = "intro", word; Color wc = WHITE; float t = 2.5f, waitT = 0, fakeT = 0, wordT = 0, goAt = 0;
  std::vector<int> locked, bad, tapOrder; std::vector<float> ms, botAt;
  void Start() override { InitStage(); tint = Hex(0x34c77b); Layout(-1, "{\"name\":\"button\",\"label\":\"TAP!\",\"hint\":\"Only tap on the big green GO! — red fake words don't count\"}"); }
  void Next() {
    round++; if (round > R) { phase = "end"; Win(); return; }
    phase = "ready"; t = 2; word = ""; locked.assign(N(), 0); bad.assign(N(), 0); tapOrder.clear(); ms.assign(N(), 0); botAt.assign(N(), 0);
    waitT = frand(2, 5); fakeT = frand(0.8f, 1.6f); Sfx("tick");
  }
  void Tap(int p) {
    if (p < 0 || p >= N() || locked[p] || (phase != "ready" && phase != "wait" && phase != "go")) return;
    if (phase != "go") { locked[p] = 1; bad[p] = 1; Gain(p, -50); Sfx("buzz"); Buzz(p, 400); Shake(4); return; }
    locked[p] = 1; ms[p] = (time - goAt) * 1000; tapOrder.push_back(p);
    int g = std::max(100, 500 - (int)tapOrder.size() * 100 + 100); Gain(p, g); Sfx("ding"); Buzz(p, 40);
    if (tapOrder.size() == 1) { fx.Burst({ PodX(p), 3, 2 }, 30, P(p).color, 6, 0.8f, 0.2f, 3); flash = 0.6f; }
  }
  void Update(float dt) override {
    Tick(dt); t -= dt;
    if (Ending(dt)) return;
    for (int i = 0; i < N(); i++) if (!P(i).bot && P(i).a && !P(i).pa) Tap(i);
    if (phase == "intro") { if (t <= 0) Next(); }
    else if (phase == "ready") { if (t <= 0) { phase = "wait"; t = waitT; } }
    else if (phase == "wait") {
      fakeT -= dt; wordT -= dt; if (wordT <= 0) word = "";
      if (fakeT <= 0 && t > 0.6f) { word = FAKES[rand() % 8]; wc = Hex(0xff4d6d); wordT = 0.6f; fakeT = frand(1.0f, 1.8f); Sfx("tick"); for (int i = 0; i < N(); i++) if (P(i).bot && frand(0, 1) < 0.08f) Tap(i); }
      if (t <= 0) { phase = "go"; goAt = time; t = 3; word = "GO!"; flash = 0.8f; Sfx("go"); for (int i = 0; i < N(); i++) botAt[i] = frand(0.3f, 0.75f); }
    } else if (phase == "go") {
      for (int i = 0; i < N(); i++) if (P(i).bot && !locked[i] && time - goAt > botAt[i]) Tap(i);
      bool all = true; for (int v : locked) all &= v != 0;
      if (t <= 0 || all) { phase = "show"; t = 2.6f; }
    } else if (phase == "show" && t <= 0) Next();
  }
  void Draw() override {
    if (phase == "go") tint = Hex(0x34c77b); else tint = Hex(0x3d8bff);
    Stage([&](int i) { return locked.size() == (size_t)N() && locked[i] && !bad[i]; });
    Text(110, 42, 17, Hex(0xc9d3f2), "ROUND " + I(std::min(std::max(1, round), R)) + " OF " + I(R), -1, TS_PLAIN | TS_HEAVY | TS_SPACED);
    Text(110, 70, 26, WHITE, "REFLEX RUSH", -1, TS_DISPLAY | TS_SHADOW | TS_GOLD);
    Glass(300, 110, 1000, 420, 34, phase == "go" ? 170 : 215);
    if (phase == "go") RRect(300, 110, 1000, 420, 34, Hex(0x1f9a52, 210), Hex(0x0f5c30, 210), BLANK, 0, true);
    if (phase == "intro") { Text(800, 270, 84, WHITE, "REFLEX RUSH", 0, TS_DISPLAY | TS_SHADOW | TS_GOLD); Text(800, 370, 28, Hex(0xdfe6ff), "Tap on the green GO! — not on the red fakes", 0, TS_HEAVY | TS_PLAIN); }
    else if (phase == "ready") Text(800, 320, 90, WHITE, "GET READY…", 0, TS_DISPLAY | TS_SHADOW);
    else if (phase == "wait") Text(800 + (word.empty() ? 0 : frand(-4, 4)), 320, word.empty() ? 140 : 170, word.empty() ? Hex(0x5a6488) : wc, word.empty() ? "…" : word, 0, TS_DISPLAY | TS_SHADOW);
    else if (phase == "go") Text(800, 320, 230 + sinf(time * 16) * 8, WHITE, "GO!", 0, TS_DISPLAY | TS_SHADOW);
    else if (phase == "show") { if (tapOrder.empty()) Text(800, 320, 80, Hex(0xff6b81), "NOBODY?!", 0, TS_DISPLAY | TS_SHADOW); else { int f = tapOrder[0]; Text(800, 290, 72, P(f).color, P(f).name, 0, TS_DISPLAY | TS_SHADOW); Text(800, 380, 44, WHITE, I((int)ms[f]) + " ms!", 0, TS_DISPLAY | TS_SHADOW); } }
    else if (phase == "end") Heading("FINAL SCORES", "REFLEX RUSH", 70, 52);
    PodiumLabels([&](int i) -> std::string {
      if (locked.size() != (size_t)N() || phase == "end") return "";
      if (bad[i]) return "TOO EARLY";
      for (size_t k = 0; k < tapOrder.size(); k++) if (tapOrder[k] == i) return "#" + I((int)k + 1) + " · " + I((int)ms[i]) + "ms";
      return "";
    });
  }
};

// ===================================================================================== STOP THE METER
struct Meter : Show {
  int R = 6, round = 0; std::string phase = "intro"; float t = 2.5f, target = 50, speed = 1, ph = 0, wait = 0;
  std::vector<float> mark; std::vector<int> gainR; std::vector<float> botAt;
  void Start() override { InitStage(); tint = Hex(0xffc93c); Layout(-1, "{\"name\":\"button\",\"label\":\"STOP!\",\"hint\":\"Stop the needle inside the gold zone!\"}"); }
  float Val() { return 50 + sinf(ph) * 50; }
  void Next() {
    round++; if (round > R) { phase = "end"; Win(); return; }
    target = frand(18, 82); speed = frand(0.45f, 0.7f) + round * 0.06f; ph = frand(0, 6.28f);
    mark.assign(N(), -1); gainR.assign(N(), 0); botAt.assign(N(), frand(1.5f, 5)); phase = "run"; t = 8; wait = 1.0f; Sfx("whoosh");
  }
  void Stop(int p) {
    if (phase != "run" || wait > 0 || p < 0 || p >= N() || mark[p] >= 0) return;
    float v = Val(), d = fabsf(v - target); int g = d < 2.5f ? 150 : std::max(0, (int)(100 - d * 3));
    mark[p] = v; gainR[p] = g; Gain(p, g);
    if (g >= 150) { Banner("PERFECT! " + P(p).name, 1300, Hx(P(p).color).c_str()); Sfx("win"); } else Sfx("ding");
    bool all = true; for (float m : mark) all &= m >= 0; if (all) t = fminf(t, 0.7f);
  }
  void Update(float dt) override {
    Tick(dt);
    if (Ending(dt)) return;
    for (int i = 0; i < N(); i++) if (!P(i).bot && P(i).a && !P(i).pa) Stop(i);
    if (phase == "intro") { t -= dt; if (t <= 0) Next(); return; }
    if (phase == "run") {
      if (wait > 0) { wait -= dt; return; }
      ph += dt * speed * 2 * PI * 0.5f; t -= dt;
      for (int i = 0; i < N(); i++) if (P(i).bot && mark[i] < 0) { botAt[i] -= dt; if (botAt[i] <= 0 && fabsf(Val() - target) < frand(2, 12)) Stop(i); }
      if (t <= 0) { phase = "show"; t = 2.6f; }
    } else if (phase == "show") { t -= dt; if (t <= 0) Next(); }
  }
  void Draw() override {
    Stage([&](int i) { return mark.size() == (size_t)N() && mark[i] >= 0; });
    Text(110, 42, 17, Hex(0xc9d3f2), "ROUND " + I(std::min(std::max(1, round), R)) + " OF " + I(R), -1, TS_PLAIN | TS_HEAVY | TS_SPACED);
    Text(110, 70, 26, WHITE, "STOP THE METER", -1, TS_DISPLAY | TS_SHADOW | TS_GOLD);
    if (phase == "intro") { Glass(400, 230, 800, 240, 34); Text(800, 330, 76, WHITE, "STOP THE METER", 0, TS_DISPLAY | TS_SHADOW | TS_GOLD); Text(800, 410, 26, Hex(0xdfe6ff), "Press STOP when the needle is in the gold", 0, TS_HEAVY | TS_PLAIN); PodiumLabels(); return; }
    if (phase == "end") { Heading("FINAL SCORES", "STOP THE METER", 70, 52); PodiumLabels(); return; }
    Glass(160, 130, 1280, 380, 34);
    float X = 230, Wd = 1140, Y = 330;
    // the meter bar (GL under the overlay would be hidden, so it's built from overlay rects)
    RRect(X, Y - 42, Wd, 84, 42, Hex(0xff5c7a), Hex(0xc92a4a), Hex(0xffffff, 60), 2, true);
    RRect(X + Wd * 0.25f, Y - 42, Wd * 0.5f, 84, 0, Hex(0x3d8bff), Hex(0x1f5fc9), BLANK, 0, true);
    float zx = X + target / 100 * Wd, zw = Wd * 0.05f;
    RRect(zx - zw / 2 - 6, Y - 56, zw + 12, 112, 14, Hex(0xfff3ce), Hex(0xf2c66d), Hex(0xffffff), 3, true, true);
    RRect(zx - 2, Y - 56, 4, 112, 2, WHITE, WHITE, BLANK, 0);
    Text(zx, Y - 80, 20, Hex(0xffe9a8), "GOLD", 0, TS_HEAVY | TS_SPACED | TS_PLAIN);
    for (int i = 0; i < N(); i++) if (mark[i] >= 0) { float mx = X + mark[i] / 100 * Wd; RRect(mx - 4, Y - 48, 8, 96, 4, P(i).color, P(i).color, WHITE, 2); Text(mx, Y + 78 + (i % 3) * 30, 20, P(i).color, P(i).name + " +" + I(gainR[i]), 0, TS_HEAVY | TS_SHADOW); }
    if (phase == "run" && wait <= 0) { float nx = X + Val() / 100 * Wd; RRect(nx - 6, Y - 70, 12, 140, 6, WHITE, Hex(0xdfe6ff), Hex(0x000000, 80), 2, true); Text(nx, Y - 92, 30, WHITE, "▼", 0, TS_PLAIN | TS_SHADOW); }
    if (phase == "run" && wait > 0) Text(800, 210, 40, WHITE, "GET READY…", 0, TS_DISPLAY | TS_SHADOW);
    if (phase == "run") Timer(t, 8, 1490, 66);
    PodiumLabels([&](int i) { return mark.size() == (size_t)N() && mark[i] >= 0 ? "+" + I(gainR[i]) : std::string(); });
  }
};
}  // namespace

static RegGame q1("reflex", [] { return std::unique_ptr<Game>(new Reflex); });
static RegGame q2("meter", [] { return std::unique_ptr<Game>(new Meter); });
