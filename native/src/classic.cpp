// CLASSICS — the games everybody already knows: Musical Chairs, Red Light Green Light, Simon Says,
// Tug of War, Air Hockey and Tag. A joystick and one button at most: nothing to learn.

#include "classic.h"

namespace {

// =====================================================================================
// MUSICAL CHAIRS — walk around while the music plays, grab a chair when it stops.
// =====================================================================================
struct Chairs : Classic {
  struct K : Mover { bool out = false, seat = false; int chair = -1, outRound = 0; float dashT = 0, dcd = 0, react = 0; };
  std::vector<K> k; std::vector<float> chA; std::vector<int> chWho, outOrder;
  std::string phase = "count"; float t = 0, ringA = 0, RR = 165; int round = 0;
  Vector2 ChairPos(int c) { float a = chA[c] + ringA; return { 800 + cosf(a) * RR, 480 + sinf(a) * RR }; }
  int Alive() { int n = 0; for (auto& m : k) if (!m.out) n++; return n; }

  void Start() override {
    InitCam(42); sc.Init(25, 16, true);
    for (int i = 0; i < N(); i++) { float a = (float)i / N() * 2 * PI; K m; m.x = 800 + cosf(a) * 280; m.y = 480 + sinf(a) * 280; m.face = a + PI / 2; k.push_back(m); }
    Layout(-1, R"({"name":"pad","stick":"xy","a":"DASH","hint":"Walk around the chairs. When the music STOPS, run to a chair!"})");
    NewRound();
  }
  void NewRound() {
    round++; int m = Alive(); chA.clear(); chWho.clear();
    for (int c = 0; c < m - 1; c++) { chA.push_back((float)c / (m - 1) * 2 * PI); chWho.push_back(-1); }
    for (auto& q : k) { q.seat = false; q.chair = -1; q.react = frand(0.5f, 1.2f); }
    t = frand(5, 10);
    if (round == 1) phase = "count";
    else { phase = "music"; Sfx("musicOn"); Banner("ROUND " + std::to_string(round) + " · " + std::to_string(m - 1) + (m - 1 == 1 ? " CHAIR LEFT" : " CHAIRS"), 1600, "#ffd43b"); }
  }
  void Update(float dt) override {
    time += dt; fx.Update(dt); conf.Update(dt);
    if (phase == "count") { if (!Counting(dt, "🎵 MUSIC!")) { phase = "music"; Sfx("musicOn"); } return; }
    if (phase == "done") return;
    t -= dt;
    if (phase == "music") ringA += dt * 0.3f;  // the chairs slowly turn, so nobody can camp next to one
    for (int i = 0; i < N(); i++) {
      K& m = k[i]; if (m.out || m.seat) continue; Player& p = P(i);
      Vector2 in = p.stick; bool dash = p.a && !p.pa;
      if (p.bot) {
        dash = false;
        if (phase == "music") { float a = atan2f(m.y - 480, m.x - 800) + 0.6f, rr = RR + 100; in = Toward(m.x, m.y, 800 + cosf(a) * rr, 480 + sinf(a) * rr); in = Vector2Scale(in, 0.6f); }
        else if (phase == "scramble") {
          m.react -= dt;
          if (m.react > 0) in = { 0, 0 };
          else {
            int best = -1; float bd = 1e9f;
            for (size_t c = 0; c < chA.size(); c++) if (chWho[c] < 0) { Vector2 cp = ChairPos((int)c); float d = hypotf(cp.x - m.x, cp.y - m.y); if (d < bd) { bd = d; best = (int)c; } }
            if (best >= 0) { Vector2 cp = ChairPos(best); in = Toward(m.x, m.y, cp.x, cp.y); dash = bd > 160 && frand(0, 1) < 0.04f; }
          }
        } else in = { 0, 0 };
      }
      if (phase == "result") in = { 0, 0 };
      if (dash && m.dcd <= 0) { m.dashT = 0.3f; m.dcd = 1.4f; Sfx("dash"); fx.Burst(W3(m.x, m.y, 0.5f), 10, p.color, 5, 0.4f, 0.15f); }
      m.dashT -= dt; m.dcd -= dt;
      Steer(m, in, (phase == "music" ? 230.f : 310.f) * (m.dashT > 0 ? 1.9f : 1), dt);
      float dx = m.x - 800, dy = m.y - 480, d = fmaxf(1, hypotf(dx, dy)), inner = phase == "music" ? RR + 60 : RR - 50;
      if (d < inner) { m.x = 800 + dx / d * inner; m.y = 480 + dy / d * inner; }
      if (d > 420) { m.x = 800 + dx / d * 420; m.y = 480 + dy / d * 420; }
      if (phase == "scramble")
        for (size_t c = 0; c < chA.size(); c++) if (chWho[c] < 0) {
          Vector2 cp = ChairPos((int)c);
          if (hypotf(cp.x - m.x, cp.y - m.y) < 50) { chWho[c] = i; m.seat = true; m.chair = (int)c; m.x = cp.x; m.y = cp.y; Sfx("ding"); Buzz(i, 60); fx.Burst(W3(cp.x, cp.y, 1), 14, p.color, 5, 0.5f, 0.15f); break; }
        }
    }
    for (int i = 0; i < N(); i++) for (int j = i + 1; j < N(); j++) if (!k[i].out && !k[j].out && !k[i].seat && !k[j].seat) Separate(k[i], k[j], 58);
    if (phase == "music" && t <= 0) { phase = "scramble"; t = 6; Sfx("musicStop"); Sfx("alarm"); Banner("SIT DOWN!", 1200, "#ff4d6d"); Shake(6); }
    else if (phase == "scramble") {
      int fr = 0; for (int w : chWho) if (w < 0) fr++;
      if (fr == 0 || t <= 0) {
        int seated = (int)chA.size() - fr;
        if (seated == 0) Banner("NOBODY SAT DOWN! AGAIN!", 1600, "#ffd43b");
        else for (int i = 0; i < N(); i++) if (!k[i].out && !k[i].seat) {
          k[i].out = true; k[i].outRound = round; outOrder.push_back(i);
          Banner(P(i).name + " is OUT!", 1800, "#ff8787"); Sfx("buzz"); Buzz(i, 400); fx.Burst(W3(k[i].x, k[i].y, 1), 24, Hex(0xff8787), 7, 0.7f, 0.2f);
        }
        if (Alive() == 1) for (int i = 0; i < N(); i++) if (!k[i].out) { conf.Spawn(W3(k[i].x, k[i].y, 3), 160, 2, 4); Sfx("win"); }
        phase = "result"; t = 2.8f;
      }
    } else if (phase == "result" && t <= 0) {
      if (Alive() <= 1) End(); else NewRound();
    }
  }
  void End() {
    phase = "done"; Sfx("musicOff");
    std::vector<int> o; for (int i = 0; i < N(); i++) if (!k[i].out) o.push_back(i);
    for (int j = (int)outOrder.size() - 1; j >= 0; j--) o.push_back(outOrder[j]);
    FinishOrder(o, [&](int i) { return k[i].out ? "out in round " + std::to_string(k[i].outRound) : std::string("WINNER"); });
  }
  void DrawChair(Vector3 p, float a, Color seatC) {
    Vector3 u = { cosf(a), 0, sinf(a) }, v = { -sinf(a), 0, cosf(a) };
    for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2)
      lit.Draw(prim.cyl, TRS({ p.x + u.x * 0.32f * sx + v.x * 0.32f * sy, 0, p.z + u.z * 0.32f * sx + v.z * 0.32f * sy }, { 0, 0, 0 }, { 0.1f, 0.55f, 0.1f }), Hex(0x8d5524));
    lit.Draw(prim.cube, TRS({ p.x, 0.6f, p.z }, { 0, -a, 0 }, { 0.95f, 0.12f, 0.95f }), seatC);
    lit.Draw(prim.cube, TRS({ p.x - u.x * 0.43f, 1.1f, p.z - u.z * 0.43f }, { 0, -a, 0 }, { 0.12f, 0.9f, 0.95f }), Shade(seatC, -0.15f));
  }
  void Draw() override {
    cam.position = { sinf(time * 0.12f) * 1.5f, 19, 17 }; cam.target = { 0, 0, 0.6f };
    sc.Begin(cam);
    lit.Draw(prim.cyl, TRS({ 0, 0.02f, 0 }, { 0, 0, 0 }, { 23, 0.03f, 23 }), Hex(0xc2255c));
    lit.Pattern(PAT_TILE); lit.Draw(prim.cyl, TRS({ 0, 0.035f, 0 }, { 0, 0, 0 }, { 20, 0.03f, 20 }), Hex(0xf783ac)); lit.Pattern(PAT_NONE);
    lit.Draw(prim.cyl, TRS({ 0, 0.05f, 0 }, { 0, 0, 0 }, { 6, 0.03f, 6 }), Hex(0xffd43b));
    sc.Trees(time);
    const unsigned pal[] = { 0x4dabf7, 0xff922b, 0x51cf66, 0xcc5de8, 0xffd43b, 0xff6b6b, 0x20c997 };
    for (size_t c = 0; c < chA.size(); c++) { Vector2 cp = ChairPos((int)c); DrawChair(W3(cp.x, cp.y), chA[c] + ringA, Hex(pal[c % 7])); }
    int outI = 0;
    for (int i = 0; i < N(); i++) {
      K& m = k[i]; Vector3 pos = W3(m.x, m.y); float yaw = PI / 2 - m.face, hop = 0, cheer = 0;
      if (m.out) { pos = W3(130, 200 + outI++ * 80.f); yaw = PI / 2; cheer = 0.5f + 0.5f * sinf(time * 4 + i); }
      else if (m.seat) { float a = chA[m.chair] + ringA; pos.y = 0.45f; yaw = PI / 2 - a; cheer = phase == "result" ? 1 : 0.3f; hop = phase == "result" && Alive() == 1 ? fabsf(sinf(time * 8)) * 0.5f : 0; }
      else { float sp = hypotf(m.vx, m.vy); hop = fabsf(sinf(time * 12 + i)) * 0.25f * fminf(1, sp / 200); }
      if (!m.seat) Shadow(pos, 1.3f);
      DrawToon(pos, yaw, m.out ? Shade(P(i).color, -0.35f) : P(i).color, time + i, hop, cheer, 1.35f);
    }
    fx.Draw(); conf.Draw();
    EndMode3D();
    for (int i = 0; i < N(); i++) { Vector3 pos = k[i].out ? W3(130, 200 + 0.f) : W3(k[i].x, k[i].y); if (!k[i].out) NameTag({ pos.x, pos.y + 3.4f, pos.z }, i); }
    // floating music notes while the music plays
    if (phase == "music") for (int n = 0; n < 9; n++) { float x = fmodf(time * 90 + n * 190, 1750) - 70, y = 150 + 40 * sinf(time * 2 + n * 1.7f) + (n % 3) * 70; Text(x, y, 44, Fade(Hex(n % 2 ? 0xffd43b : 0xff8cc6), 0.85f), n % 2 ? "♪" : "♫", 0, TS_SHADOW); }
    char b[64]; snprintf(b, sizeof b, "ROUND %d · %d %s", round, (int)chA.size(), chA.size() == 1 ? "CHAIR" : "CHAIRS");
    UI::Box({ 650, 14, 300, 52 }, 26, Hex(0x0b1020, 160)); Text(800, 41, 26, WHITE, b, 0, TS_HEAVY | TS_PLAIN);
    if (outI) { UI::Box({ 40, 140, 180, 40 }, 20, Hex(0x0b1020, 140)); Text(130, 160, 18, Hex(0xff8787), "OUT", 0, TS_HEAVY | TS_PLAIN); }
    if (phase == "music") Hint("🎵 Keep walking around the chairs…");
    else if (phase == "scramble") Hint("🪑 SIT!  Run to a free chair!", Hex(0xff8787));
    DrawCount();
  }
};

// =====================================================================================
// RED LIGHT, GREEN LIGHT — run on green, freeze on red. Caught moving = back you go.
// =====================================================================================
struct RedLight : Classic {
  struct K : Mover { int fin = -1, caught = 0; float stun = 0, react = 0; };
  std::vector<K> k; std::string light = "wait"; float lt = 0, grace = 0, T = 90, dollYaw = PI; int finN = 0; bool over = false;
  const float START = 800, GOAL = 160;
  void Start() override {
    InitCam(46); sc.Init(30, 22, true);
    for (int i = 0; i < N(); i++) { K m; m.x = 800 + (i - (N() - 1) / 2.f) * fminf(150, 1050.f / N()); m.y = START; m.face = -PI / 2; k.push_back(m); }
    Layout(-1, R"({"name":"pad","stick":"xy","a":"RUN","hint":"Hold RUN on GREEN. Let go on RED!"})");
  }
  void Update(float dt) override {
    time += dt; fx.Update(dt); conf.Update(dt);
    float tgt = light == "green" || light == "wait" ? PI : 0; dollYaw += angDiff(dollYaw, tgt) * fminf(1, dt * 9);
    if (Counting(dt, "GREEN LIGHT!")) return;
    if (light == "wait") { light = "green"; lt = frand(2, 4); }
    if (over) return;
    T -= dt; lt -= dt; grace -= dt;
    if (lt <= 0) {
      if (light == "green") { light = "turn"; lt = 1.0f; Sfx("whoosh"); for (auto& m : k) m.react = frand(0.0f, 1.15f); }
      else if (light == "turn") { light = "red"; lt = frand(1.6f, 3.0f); grace = 0.75f; Sfx("alarm"); Banner("🔴 RED LIGHT!", 900, "#ff4d6d"); }
      else { light = "green"; lt = frand(1.4f, 4.2f); Sfx("go"); Banner("🟢 GREEN LIGHT!", 900, "#51cf66"); }
    }
    for (int i = 0; i < N(); i++) {
      K& m = k[i]; if (m.fin >= 0) continue; Player& p = P(i); Vector2 in;
      if (p.bot) {
        bool go = light == "green" || (m.react > 0);
        if (light != "green") m.react -= dt;
        in = go ? Vector2{ 0.15f * sinf(time + i), -1 } : Vector2{ 0, 0 };
      } else { float run = fmaxf(p.a ? 1.f : 0.f, -p.stick.y); in = { p.stick.x * 0.7f, -run }; }
      if (m.stun > 0) { m.stun -= dt; in = { 0, 0 }; }
      Steer(m, in, 88, dt, 14);
      m.x = Clamp(m.x, 250, 1350); m.y = fminf(m.y, START + 10);
      if (light == "red" && grace <= 0 && m.stun <= 0 && hypotf(m.vx, m.vy) > 30) {
        fx.Burst(W3(m.x, m.y, 1), 20, Hex(0xff6b6b), 6, 0.6f, 0.2f);
        m.y = fminf(START, m.y + 140); m.vx = m.vy = 0; m.stun = 0.8f; m.caught++;
        Sfx("buzz"); Buzz(i, 300); Shake(5); Banner(p.name + " MOVED! Back you go", 1100, "#ff8787");
        fx.Burst(W3(m.x, m.y, 1), 14, WHITE, 5, 0.5f, 0.2f);
      }
      if (m.y <= GOAL) { m.fin = finN++; Sfx("win"); Buzz(i, 200); conf.Spawn(W3(m.x, m.y, 3), 70, 1.5f, 3); Banner(p.name + " MADE IT!", 1400, "#ffd43b"); }
    }
    for (int i = 0; i < N(); i++) for (int j = i + 1; j < N(); j++) if (k[i].fin < 0 && k[j].fin < 0) Separate(k[i], k[j], 50);
    if (finN == N() || T <= 0) End();
  }
  void End() {
    over = true;
    std::vector<int> o = Sorted([&](int i) { return k[i].fin >= 0 ? 100000.f - k[i].fin : -k[i].y; });
    FinishOrder(o, [&](int i) { if (k[i].fin >= 0) return std::string(k[i].fin == 0 ? "1st across!" : "made it"); char b[32]; snprintf(b, sizeof b, "%d m to go", (int)((k[i].y - GOAL) / 20)); return std::string(b); });
  }
  void Draw() override {
    cam.position = { 0, 13, 21 }; cam.target = { 0, 0, -3 };
    sc.Begin(cam);
    lit.Draw(prim.cube, TRS(W3(800, 480, 0.01f), { 0, 0, 0 }, { 1200 * SC, 0.02f, 780 * SC }), Hex(0xf8f9fa));
    lit.Draw(prim.cube, TRS(W3(800, START + 30, 0.03f), { 0, 0, 0 }, { 1200 * SC, 0.02f, 0.25f }), Hex(0x4dabf7));
    for (int i = 0; i < 30; i++) for (int r = 0; r < 2; r++) lit.Draw(prim.cube, TRS(W3(200 + i * 40.f + 20, GOAL - 10 + r * 20.f, 0.03f), { 0, 0, 0 }, { 1, 0.02f, 0.5f }), (i + r) % 2 ? Hex(0x212529) : WHITE);
    sc.Trees(time);
    // the doll: back turned on green, staring at you on red
    bool red = light == "red" || light == "turn";
    Vector3 dp = W3(800, 70); Shadow(dp, 4);
    DrawToon(dp, dollYaw, Hex(0xff922b), time, 0, red ? 0 : 0.4f, 4.0f);
    if (light == "red") for (int s = -1; s <= 1; s += 2) lit.Draw(prim.sphere, TRS({ dp.x + s * 0.55f + sinf(dollYaw) * 0.0f, dp.y + 5.0f, dp.z + 1.6f }, { 0, 0, 0 }, { 0.35f, 0.35f, 0.35f }), Hex(0xff2020));
    // traffic light
    Vector3 tl = W3(1300, 110);
    lit.Draw(prim.cyl, TRS(tl, { 0, 0, 0 }, { 0.3f, 6, 0.3f }), Hex(0x495057));
    lit.Draw(prim.cube, TRS({ tl.x, 7, tl.z }, { 0, 0, 0 }, { 1.4f, 2.8f, 1 }), Hex(0x212529));
    lit.Draw(prim.sphere, TRS({ tl.x, 7.7f, tl.z + 0.5f }, { 0, 0, 0 }, { 0.95f, 0.95f, 0.4f }), red ? Hex(0xff3b3b) : Hex(0x5c1a1a));
    lit.Draw(prim.sphere, TRS({ tl.x, 6.3f, tl.z + 0.5f }, { 0, 0, 0 }, { 0.95f, 0.95f, 0.4f }), !red ? Hex(0x40ff70) : Hex(0x15401f));
    for (int i = 0; i < N(); i++) {
      K& m = k[i]; Vector3 pos = W3(m.x, m.y); float sp = hypotf(m.vx, m.vy);
      Shadow(pos, 1.2f);
      float wob = m.stun > 0 ? sinf(time * 30) * 0.4f : 0;
      DrawToon(pos, PI / 2 - m.face + wob, m.stun > 0 && (int)(time * 10) % 2 ? WHITE : P(i).color, time + i, m.fin >= 0 ? fabsf(sinf(time * 7 + i)) * 0.5f : fabsf(sinf(time * 14 + i)) * 0.3f * fminf(1, sp / 80), m.fin >= 0 ? 1 : 0, 1.3f);
    }
    fx.Draw(); conf.Draw();
    EndMode3D();
    for (int i = 0; i < N(); i++) NameTag({ W3(k[i].x, k[i].y).x, 3.2f, W3(k[i].x, k[i].y).z }, i, 18);
    std::vector<int> o = Sorted([&](int i) { return k[i].fin >= 0 ? 100000.f - k[i].fin : -k[i].y; });
    Board("DISTANCE", o, [&](int i) { if (k[i].fin >= 0) return std::string("🏁"); return std::to_string((int)(100 * (START - k[i].y) / (START - GOAL))) + "%"; });
    Clock(T, 1500);
    if (light != "wait") {
      Color c = light == "green" ? Hex(0x2f9e44) : light == "turn" ? Hex(0xf08c00) : Hex(0xe03131);
      UI::Shadow({ 620, 14, 360, 70 }, 35, 20, Hex(0, 90), 6); UI::Box({ 620, 14, 360, 70 }, 35, Shade(c, 0.15f), c);
      Text(800, 50, 38, WHITE, light == "green" ? "GREEN LIGHT" : light == "turn" ? "LOOKING…" : "RED LIGHT", 0, TS_DISPLAY | TS_SHADOW);
    }
    Hint(light == "red" ? "🛑 FREEZE! Don't touch anything!" : "🏃 Hold RUN (or push up) to run to the finish", light == "red" ? Hex(0xff8787) : WHITE);
    DrawCount();
  }
};

// =====================================================================================
// SIMON SAYS — only do it when Simon says so!
// =====================================================================================
struct Simon : Classic {
  struct K { int resp = -1, pts = 0, streak = 0, poseAct = -1; float pose = 0, botT = 0; int botAct = -1; bool armed = false, ok = false; };
  std::vector<K> k; int cmd = 0, act = 0, total = 15; bool simon = true; std::string phase = "count"; float t = 0, win = 3, el = 0;
  static constexpr const char* NAME[6] = { "LEFT", "RIGHT", "HANDS UP", "SQUAT", "JUMP", "CLAP" };
  static constexpr const char* HOW[6] = { "⬅️ push the stick LEFT", "➡️ push the stick RIGHT", "🙌 push the stick UP", "⬇️ push the stick DOWN", "🦘 press JUMP", "👏 press CLAP" };
  void Start() override {
    InitCam(40); sc.Init(24, 14, true);
    k.resize(N());
    Layout(-1, R"({"name":"pad","stick":"xy","a":"JUMP","b":"CLAP","hint":"Only do it when SIMON SAYS!"})");
  }
  void NewCmd() {
    cmd++; int prev = act; do act = rand() % 6; while (act == prev);
    simon = cmd <= 3 ? true : frand(0, 1) > 0.28f;
    win = fmaxf(2.4f, 4.0f - cmd * 0.08f); t = win; el = 0; phase = "say";
    for (auto& m : k) { m.resp = -1; m.armed = false; m.ok = false; m.botT = frand(0.5f, win * 0.75f); m.botAct = simon ? (frand(0, 1) < 0.88f ? act : rand() % 6) : (frand(0, 1) < 0.7f ? -1 : act); }
    Sfx(simon ? "ding" : "blip");
  }
  int Read(int i) {
    Player& p = P(i); K& m = k[i]; float mag = hypotf(p.stick.x, p.stick.y);
    if (!m.armed) { if (mag < 0.35f && !p.a && !p.b) m.armed = true; return -1; }
    if (p.a && !p.pa) return 4;
    if (p.b && !p.pb) return 5;
    if (mag > 0.6f) { if (fabsf(p.stick.x) > fabsf(p.stick.y)) return p.stick.x < 0 ? 0 : 1; return p.stick.y < 0 ? 2 : 3; }
    return -1;
  }
  void Update(float dt) override {
    time += dt; fx.Update(dt); conf.Update(dt);
    for (auto& m : k) m.pose = fmaxf(0, m.pose - dt * 0.9f);
    if (phase == "count") { if (!Counting(dt, "LISTEN TO SIMON!")) NewCmd(); return; }
    if (phase == "done") return;
    t -= dt; el += dt;
    if (phase == "say") {
      int answered = 0;
      for (int i = 0; i < N(); i++) {
        K& m = k[i];
        if (m.resp < 0) {
          int a = P(i).bot ? (el >= m.botT ? m.botAct : -1) : Read(i);
          if (a >= 0) { m.resp = a; m.pose = 1; m.poseAct = a; if (a == 4) Sfx("bounce"); if (a == 5) Sfx("pok"); }
        }
        if (m.resp >= 0) answered++;
      }
      if (t <= 0 || (simon && answered == N())) {
        int right = 0;
        for (int i = 0; i < N(); i++) {
          K& m = k[i]; m.ok = simon ? m.resp == act : m.resp < 0;
          if (m.ok) { m.pts++; m.streak++; right++; } else { m.streak = 0; Buzz(i, 250); }
        }
        Sfx(right * 2 >= N() ? "win" : "buzz");
        phase = "judge"; t = 1.8f;
      }
    } else if (phase == "judge" && t <= 0) {
      if (cmd >= total) {
        phase = "done";
        std::vector<int> o = Sorted([&](int i) { return (float)k[i].pts; });
        FinishOrder(o, [&](int i) { return std::to_string(k[i].pts) + " / " + std::to_string(total); });
      } else NewCmd();
    }
  }
  void Draw() override {
    cam.position = { 0, 6.8f, 19.5f }; cam.target = { 0, 2.7f, -1 };
    sc.Begin(cam);
    lit.Draw(prim.cube, TRS({ 0, 0.3f, 1 }, { 0, 0, 0 }, { 34, 0.6f, 10 }), Hex(0xb07a4a));
    for (int i = 0; i < 12; i++) lit.Draw(prim.cube, TRS({ -16.5f + i * 3.0f, 0.61f, 1 }, { 0, 0, 0 }, { 0.06f, 0.02f, 10 }), Hex(0x8d5524));
    lit.Draw(prim.cyl, TRS({ 0, 0, -6 }, { 0, 0, 0 }, { 5, 1.4f, 5 }), Hex(0xcc5de8));
    sc.Trees(time);
    // Simon on his podium
    bool talking = phase == "say" && el < 1.0f;
    DrawToon({ 0, 1.4f, -6 }, 0, Hex(0xfab005), time, talking ? fabsf(sinf(time * 10)) * 0.15f : 0, phase == "say" ? 0.6f : 0.2f, 3.2f);
    lit.Draw(prim.cone, TRS({ 0, 1.4f + 3.2f * 1.95f, -6 }, { 0, 0, 0 }, { 1.4f, 1.2f, 1.4f }), Hex(0xffd43b)); // party hat
    float span = fminf(26, 3.6f * N());
    for (int i = 0; i < N(); i++) {
      K& m = k[i]; float x = N() == 1 ? 0 : -span / 2 + span * i / (N() - 1);
      Vector3 pos = { x, 0.6f, 3.5f }; float hop = 0, cheer = 0, s = 1.4f, e = sinf(fminf(1, m.pose) * PI / 2);
      if (m.poseAct == 0) pos.x -= 0.7f * e; else if (m.poseAct == 1) pos.x += 0.7f * e;
      else if (m.poseAct == 2) cheer = e; else if (m.poseAct == 3) pos.y -= 0.45f * e;
      else if (m.poseAct == 4) hop = sinf(fminf(1, m.pose) * PI) * 1.4f; else if (m.poseAct == 5) cheer = 0.5f * e;
      if (phase == "judge" && !m.ok) pos.x += sinf(time * 30) * 0.08f;
      DrawToon(pos, 0, P(i).color, time + i, hop, cheer, s);
      if (m.poseAct == 5 && m.pose > 0.85f) fx.Burst({ pos.x, pos.y + 3, pos.z + 0.5f }, 2, Hex(0xffd43b), 4, 0.3f, 0.1f);
    }
    fx.Draw(); conf.Draw();
    EndMode3D();
    for (int i = 0; i < N(); i++) {
      float x = N() == 1 ? 0 : -span / 2 + span * i / (N() - 1); Vector2 s = GetWorldToScreen({ x, 3.9f, 3.5f }, cam);
      Text(s.x, s.y + 6, 18, P(i).color, P(i).name);
      Text(s.x, s.y + 32, 20, Hex(0xffd43b), std::to_string(k[i].pts) + (k[i].streak >= 3 ? " 🔥" : ""), 0, TS_HEAVY);
      if (phase == "judge") Text(s.x, s.y - 34, 58, k[i].ok ? Hex(0x51cf66) : Hex(0xff6b6b), k[i].ok ? "✔" : "✖", 0, TS_DISPLAY | TS_SHADOW);
    }
    // speech bubble
    if (phase == "say" || phase == "judge") {
      std::string line = (simon ? "Simon says: " : "") + std::string(NAME[act]) + "!";
      float w = fmaxf(560, Measure(line, 54, TS_DISPLAY) + 90);
      UI::Shadow({ 800 - w / 2, 40, w, 150 }, 40, 26, Hex(0, 110), 8);
      UI::Box({ 800 - w / 2, 40, w, 150 }, 40, simon ? Hex(0xfff3bf) : WHITE, simon ? Hex(0xffe066) : Hex(0xe9ecef), 90, simon ? Hex(0xfab005) : Hex(0xadb5bd), 4);
      Text(800, 96, 54, Hex(0x212529), line, 0, TS_DISPLAY | TS_PLAIN);
      Text(800, 152, 24, Hex(0x495057), HOW[act], 0, TS_PLAIN);
      if (phase == "say") { UI::Box({ 800 - w / 2 + 40, 172, w - 80, 8 }, 4, Hex(0, 40)); UI::Box({ 800 - w / 2 + 40, 172, (w - 80) * fmaxf(0, t / win), 8 }, 4, Hex(0xfa5252)); }
      if (phase == "judge" && !simon) Text(800, 230, 36, Hex(0xff6b6b), "Simon didn't say! 😜", 0, TS_DISPLAY | TS_SHADOW);
    }
    char b[32]; snprintf(b, sizeof b, "%d / %d", cmd, total); UI::Box({ 1440, 18, 140, 44 }, 22, Hex(0x0b1020, 150)); Text(1510, 40, 22, WHITE, b, 0, TS_HEAVY | TS_PLAIN);
    DrawCount();
  }
};

// =====================================================================================
// TUG OF WAR — two teams, one rope, a mud pit. Tap tap tap!
// =====================================================================================
struct Tug : Classic {
  std::vector<int> team, taps; std::vector<float> botAcc, botRate, flash; float rp = 0, rpS = 0, t = 0, rt = 0; int round = 0, tw[2] = { 0, 0 }, lastWin = -1; std::string phase = "count";
  int Size(int tm) { int n = 0; for (int x : team) if (x == tm) n++; return std::max(1, n); }
  void Start() override {
    InitCam(38); sc.Init(26, 14, true);
    for (int i = 0; i < N(); i++) { team.push_back(i % 2); taps.push_back(0); botAcc.push_back(0); botRate.push_back(frand(4.6f, 6.4f)); flash.push_back(0); }
    Layout(-1, R"({"name":"shake","hint":"TAP as fast as you can to pull the rope!"})");
    NewRound();
  }
  void NewRound() { round++; rp = 0; phase = "count"; Recount(); rt = 0; if (round > 1) Banner("ROUND " + std::to_string(round), 1400, "#ffd43b"); }
  void Pull(int i, int n) {
    if (phase != "pull") return;
    taps[i] += n; flash[i] = 1;
    rp += (team[i] == 0 ? -1.f : 1.f) * 0.03f * n / Size(team[i]);
  }
  void Act(int pid, const std::string& kind, int n, float) override { if (kind == "pump" && pid >= 0 && pid < N() && !P(pid).bot) Pull(pid, std::max(1, std::min(n, 4))); }
  void Update(float dt) override {
    time += dt; fx.Update(dt); conf.Update(dt);
    for (auto& f : flash) f = fmaxf(0, f - dt * 6);
    rpS += (rp - rpS) * fminf(1, dt * 10);
    if (phase == "count") { if (!Counting(dt, "PULL!")) phase = "pull"; return; }
    if (phase == "done") return;
    if (phase == "over") { t -= dt; if (t <= 0) { if (tw[lastWin] >= 2) End(); else NewRound(); } return; }
    rt += dt;
    for (int i = 0; i < N(); i++) if (P(i).bot) { botAcc[i] += dt * botRate[i] * (0.85f + 0.3f * sinf(time * 1.3f + i)); while (botAcc[i] >= 1) { botAcc[i] -= 1; Pull(i, 1); } }
    if (fabsf(rp) >= 1 || rt > 22) {
      lastWin = rp < 0 ? 0 : 1; tw[lastWin]++; phase = "over"; t = 3.2f;
      Banner(std::string(lastWin == 0 ? "RED" : "BLUE") + " TEAM WINS THE ROUND!", 2200, lastWin == 0 ? "#ff6b6b" : "#4dabf7");
      Sfx("splash"); Sfx("cheer"); Shake(10);
      fx.Burst({ 0, 0.5f, 0 }, 60, Hex(0x6b4423), 9, 1.0f, 0.25f, 9);
      conf.Spawn({ lastWin == 0 ? -8.f : 8.f, 4, 0 }, 120, 3, 4);
    }
  }
  void End() {
    phase = "done";
    int w = tw[0] >= 2 ? 0 : 1;
    std::vector<int> o = Sorted([&](int i) { return (team[i] == w ? 100000.f : 0) + taps[i]; });
    FinishOrder(o, [&](int i) { return std::string(team[i] == w ? "won · " : "") + std::to_string(taps[i]) + " pulls"; });
  }
  void Draw() override {
    cam.position = { 0, 5.8f, 15.5f }; cam.target = { 0, 1.9f, 0 };
    sc.Begin(cam);
    // mud pit + team mats
    lit.Draw(prim.cyl, TRS({ 0, 0.02f, 0 }, { 0, 0, 0 }, { 5, 0.05f, 5.5f }), Hex(0x5c3d1e));
    lit.Pattern(PAT_WOOD); lit.Draw(prim.cyl, TRS({ 0, 0.04f, 0 }, { 0, 0, 0 }, { 4, 0.05f, 4.5f }), Hex(0x7a5230)); lit.Pattern(PAT_NONE);
    lit.Draw(prim.cube, TRS({ -10, 0.02f, 0 }, { 0, 0, 0 }, { 14, 0.04f, 4 }), Hex(0xffc9c9));
    lit.Draw(prim.cube, TRS({ 10, 0.02f, 0 }, { 0, 0, 0 }, { 14, 0.04f, 4 }), Hex(0xd0ebff));
    for (int s = -1; s <= 1; s += 2) { lit.Draw(prim.cyl, TRS({ s * 6.f, 0, 2.3f }, { 0, 0, 0 }, { 0.12f, 2.2f, 0.12f }), Hex(0x868e96)); lit.Draw(prim.cube, TRS({ s * 6.f + 0.45f * -s, 1.9f, 2.3f }, { 0, 0, 0 }, { 0.9f, 0.55f, 0.05f }), s < 0 ? Hex(0xfa5252) : Hex(0x339af0)); }
    sc.Trees(time);
    float off = rpS * 6, fall = phase == "over" ? fminf(1, (3.2f - t) / 0.8f) : 0;
    lit.Draw(prim.cyl, TRS({ -16 + off, 1.15f, 0 }, { 0, 0, -PI / 2 }, { 0.16f, 32, 0.16f }), Hex(0xd9a066));
    lit.Draw(prim.cube, TRS({ off, 0.75f, 0 }, { 0, 0, 0 }, { 0.12f, 0.8f, 0.6f }), Hex(0xffd43b));
    for (int tm = 0; tm < 2; tm++) {
      int j = 0; float s = tm == 0 ? -1.f : 1.f;
      for (int i = 0; i < N(); i++) if (team[i] == tm) {
        float x = s * (3.2f + j * 1.7f) + off, z = (j % 2 ? 0.45f : -0.45f), y = 0, hop = flash[i] * 0.25f;
        if (phase == "over" && tm != lastWin) { x -= s * 1.8f * fall; if (fabsf(x) < 4.6f) y = -0.55f * fall; }
        Vector3 pos = { x, y, z }; Shadow(pos, 1.2f);
        DrawToon(pos, tm == 0 ? PI / 2 : -PI / 2, P(i).color, time + i, phase == "over" && tm == lastWin ? fabsf(sinf(time * 8 + i)) * 0.5f : hop, phase == "over" && tm == lastWin ? 1 : 0.55f, 1.35f);
        j++;
      }
    }
    fx.Draw(); conf.Draw();
    EndMode3D();
    for (int tm = 0; tm < 2; tm++) { int j = 0; float s = tm == 0 ? -1.f : 1.f; for (int i = 0; i < N(); i++) if (team[i] == tm) { NameTag({ s * (3.2f + j * 1.7f) + off, 3.3f, (j % 2 ? 0.45f : -0.45f) }, i, 17); j++; } }
    // team panels with round wins
    for (int tm = 0; tm < 2; tm++) {
      float x = tm == 0 ? 40 : 1220; Color c = tm == 0 ? Hex(0xfa5252) : Hex(0x339af0);
      UI::Box({ x, 20, 340, 96 }, 22, Shade(c, 0.1f), Shade(c, -0.2f));
      Text(x + 24, 50, 30, WHITE, tm == 0 ? "RED TEAM" : "BLUE TEAM", -1, TS_DISPLAY | TS_SHADOW);
      for (int r = 0; r < 2; r++) DrawCircle((int)(x + 280 + r * 34), 50, 12, r < tw[tm] ? Hex(0xffd43b) : Fade(WHITE, 0.3f));
      int sum = 0; for (int i = 0; i < N(); i++) if (team[i] == tm) sum += taps[i];
      Text(x + 24, 92, 20, Fade(WHITE, 0.9f), std::to_string(sum) + " pulls", -1, TS_PLAIN);
    }
    // tug meter
    UI::Box({ 500, 140, 600, 22 }, 11, Hex(0x0b1020, 140));
    UI::Box({ 800 - 3, 134, 6, 34 }, 3, WHITE);
    float mx = 800 + Clamp(rpS, -1, 1) * 300; UI::Box({ mx - 14, 136, 28, 30 }, 10, Hex(0xffd43b));
    if (phase == "pull") Hint("👆 TAP TAP TAP on your phone!");
    DrawCount();
  }
};

// =====================================================================================
// AIR HOCKEY — classic table, mallets and a puck. Red side vs blue side.
// =====================================================================================
struct Hockey : Classic {
  struct M { float x, y, vx = 0, vy = 0; int team, goals = 0; float back = 0; };
  std::vector<M> m; float px = 800, py = 490, pvx = 0, pvy = 0, pause = 0, T = 150, sndT = 0, stuckT = 0; int score[2] = { 0, 0 }, last = -1; bool over = false;
  const float L = 250, R = 1350, TOP = 190, BOT = 790, CY = 490, GH = 125, MR = 46, PR = 26;
  void Start() override {
    InitCam(40); sc.Init(26, 17, true);
    int cnt[2] = { 0, 0 }; for (int i = 0; i < N(); i++) cnt[i % 2]++;
    int j[2] = { 0, 0 };
    for (int i = 0; i < N(); i++) { int tm = i % 2; M q; q.team = tm; q.x = tm == 0 ? L + 140 : R - 140; q.y = CY + (j[tm]++ - (cnt[tm] - 1) / 2.f) * 150; m.push_back(q); }
    Layout(-1, R"({"name":"pad","stick":"xy","hint":"Move your mallet. Hit the puck into their goal!"})");
    pause = 0.6f; px = 800 + (rand() % 2 ? 150.f : -150.f);
  }
  int Closest(int tm) { int b = -1; float bd = 1e9f; for (int i = 0; i < N(); i++) if (m[i].team == tm) { float d = hypotf(m[i].x - px, m[i].y - py); if (d < bd) { bd = d; b = i; } } return b; }
  void Goal(int tm) {
    score[tm]++; if (last >= 0 && m[last].team == tm) m[last].goals++;
    Sfx("boom"); Sfx("cheer"); Shake(10);
    float gx = tm == 0 ? R : L; conf.Spawn(W3(gx, CY, 2), 100, 2, 4); fx.Burst(W3(gx, CY, 0.5f), 30, tm == 0 ? Hex(0xff6b6b) : Hex(0x4dabf7), 8, 0.7f, 0.2f);
    Banner(last >= 0 && m[last].team == tm ? "GOAL! " + P(last).name : std::string(tm == 0 ? "RED SCORES!" : "BLUE SCORES!"), 1500, tm == 0 ? "#ff6b6b" : "#4dabf7");
    px = tm == 0 ? 800 + 160 : 800 - 160; py = CY; pvx = pvy = 0; pause = 1.3f; last = -1;
    if (score[tm] >= 7) End();
  }
  void End() {
    over = true; int w = score[0] == score[1] ? -1 : score[0] > score[1] ? 0 : 1;
    std::vector<int> o = Sorted([&](int i) { return (m[i].team == w ? 1000.f : 0) + m[i].goals; });
    FinishOrder(o, [&](int i) { return std::to_string(score[m[i].team]) + "-" + std::to_string(score[1 - m[i].team]) + (m[i].goals ? " · " + std::to_string(m[i].goals) + " goals" : std::string("")); });
  }
  void Update(float dt) override {
    time += dt; fx.Update(dt); conf.Update(dt);
    if (Counting(dt, "FACE OFF!")) return;
    if (over) return;
    T -= dt; pause -= dt; sndT -= dt;
    stuckT = hypotf(pvx, pvy) < 140 ? stuckT + dt : 0; if (stuckT > 2.2f) stuckT = 0;
    if (T <= 0) { End(); return; }
    for (int i = 0; i < N(); i++) {
      M& q = m[i]; Vector2 in;
      if (P(i).bot) {
        float dir = q.team == 0 ? 1.f : -1.f, gx = q.team == 0 ? L : R; bool mine = (px - 800) * dir <= 5;
        if (mine && pause <= 0 && stuckT < 1.2f && i == Closest(q.team)) {
          Vector2 dg = Toward(px, py, q.team == 0 ? R : L, CY + frand(-40, 40));         // puck -> their goal
          float side = (q.x - px) * dg.x + (q.y - py) * dg.y;                             // < 0: I'm behind the puck
          if (side < -25) in = Toward(q.x, q.y, px + dg.x * 80, py + dg.y * 80);          // STRIKE through it
          else {
            float tx = px - dg.x * 95, ty = py - dg.y * 95 + (q.y > py ? 40.f : -40.f);
            bool reachable = ty > TOP + MR && ty < BOT - MR && (q.team == 0 ? tx > L + MR && tx < 800 - MR : tx < R - MR && tx > 800 + MR);
            if (!reachable) {  // puck on a rail / in a corner: back up, then smack it so it bounces out
              Vector2 d = Toward(q.x, q.y, px, py); float dist = hypotf(px - q.x, py - q.y);
              if (dist < MR + PR + 25) q.back = 0.4f;
              in = q.back > 0 ? Toward(q.x, q.y, px - d.x * 200, py - d.y * 200) : Toward(q.x, q.y, px + d.x * 60, py + d.y * 60);
            }
            else in = Vector2Scale(Toward(q.x, q.y, tx, ty), fminf(1, hypotf(tx - q.x, ty - q.y) / 50));
          }
        } else { float tx = gx + dir * 120, ty = Clamp(py, CY - 120, CY + 120); if (Closest(q.team) == i && !mine) {} else if (Closest(q.team) != i) { tx = gx + dir * (i % 4 < 2 ? 90.f : 260.f); ty = CY + (i % 4 < 2 ? 0.f : (py > CY ? -90.f : 90.f)); } in = Vector2Scale(Toward(q.x, q.y, tx, ty), fminf(1, hypotf(tx - q.x, ty - q.y) / 60) * 0.85f); }
      } else in = Norm(P(i).stick);
      q.back -= dt;
      float k = fminf(1, dt * 16); q.vx += (in.x * 950 - q.vx) * k; q.vy += (in.y * 950 - q.vy) * k;
      q.x += q.vx * dt; q.y += q.vy * dt;
      float lo = q.team == 0 ? L + MR : 800 + MR, hi = q.team == 0 ? 800 - MR : R - MR;
      if (q.x < lo) { q.x = lo; q.vx = 0; } if (q.x > hi) { q.x = hi; q.vx = 0; }
      if (q.y < TOP + MR) { q.y = TOP + MR; q.vy = 0; } if (q.y > BOT - MR) { q.y = BOT - MR; q.vy = 0; }
    }
    for (int i = 0; i < N(); i++) for (int j = i + 1; j < N(); j++) {
      float dx = m[j].x - m[i].x, dy = m[j].y - m[i].y, d = hypotf(dx, dy);
      if (d < MR * 2 && d > 0.01f) { float o = (MR * 2 - d) / 2; m[i].x -= dx / d * o; m[i].y -= dy / d * o; m[j].x += dx / d * o; m[j].y += dy / d * o; }
    }
    if (pause > 0) return;
    const int SUB = 4; float h = dt / SUB;
    for (int s = 0; s < SUB && !over; s++) {
      px += pvx * h; py += pvy * h;
      float f = expf(-0.22f * h); pvx *= f; pvy *= f;
      // the table's air gently blows a slow puck off the rails, so it never gets stuck in a corner
      float nd = 320 * h; bool nearGoal = fabsf(py - CY) < GH;
      if (py < TOP + PR + 10) pvy += nd; if (py > BOT - PR - 10) pvy -= nd;
      if (!nearGoal && px < L + PR + 10) pvx += nd; if (!nearGoal && px > R - PR - 10) pvx -= nd;
      if (py < TOP + PR) { py = TOP + PR; pvy = fabsf(pvy) * 0.92f; if (sndT <= 0 && hypotf(pvx, pvy) > 150) { Sfx("tink"); sndT = 0.06f; } }
      if (py > BOT - PR) { py = BOT - PR; pvy = -fabsf(pvy) * 0.92f; if (sndT <= 0 && hypotf(pvx, pvy) > 150) { Sfx("tink"); sndT = 0.06f; } }
      bool inGoal = fabsf(py - CY) < GH - PR * 0.3f;
      if (px < L + PR) { if (inGoal) { if (px < L - PR) { Goal(1); break; } } else { px = L + PR; pvx = fabsf(pvx) * 0.92f; if (sndT <= 0 && hypotf(pvx, pvy) > 150) { Sfx("tink"); sndT = 0.06f; } } }
      if (px > R - PR) { if (inGoal) { if (px > R + PR) { Goal(0); break; } } else { px = R - PR; pvx = -fabsf(pvx) * 0.92f; if (sndT <= 0 && hypotf(pvx, pvy) > 150) { Sfx("tink"); sndT = 0.06f; } } }
      for (int i = 0; i < N(); i++) {
        M& q = m[i]; float dx = px - q.x, dy = py - q.y, d = hypotf(dx, dy);
        if (d < MR + PR && d > 0.01f) {
          float nx = dx / d, ny = dy / d; px = q.x + nx * (MR + PR); py = q.y + ny * (MR + PR);
          float rvx = pvx - q.vx, rvy = pvy - q.vy, vn = rvx * nx + rvy * ny;
          if (vn < 0) { pvx -= 1.9f * vn * nx; pvy -= 1.9f * vn * ny; last = i; if (sndT <= 0 && fabsf(vn) > 80) { Sfx("pok"); sndT = 0.09f; } if (fabsf(vn) > 500) { Shake(3); fx.Burst(W3(px, py, 0.3f), 8, WHITE, 4, 0.3f, 0.12f); } if (!P(i).bot) Buzz(i, 30); }
        }
      }
      float sp = hypotf(pvx, pvy); if (sp > 1700) { pvx *= 1700 / sp; pvy *= 1700 / sp; }
    }
  }
  void Draw() override {
    cam.position = { 0, 24, 16.5f }; cam.target = { 0, 0, 0.8f };
    sc.Begin(cam);
    float w = (R - L) * SC, d = (BOT - TOP) * SC; Vector3 c = W3(800, CY);
    for (int lg = 0; lg < 4; lg++) lit.Draw(prim.cube, TRS({ c.x + (lg % 2 ? 1 : -1) * (w / 2 - 0.6f), -1.6f, c.z + (lg / 2 ? 1 : -1) * (d / 2 - 0.6f) }, { 0, 0, 0 }, { 0.6f, 3.2f, 0.6f }), Hex(0x343a40));
    lit.Draw(prim.cube, TRS({ c.x, -0.35f, c.z }, { 0, 0, 0 }, { w + 1.2f, 0.6f, d + 1.2f }), Hex(0x1c2541));
    lit.Draw(prim.cube, TRS({ c.x, -0.05f, c.z }, { 0, 0, 0 }, { w, 0.1f, d }), Hex(0xf1f8ff));
    lit.Draw(prim.cube, TRS({ c.x, 0.01f, c.z }, { 0, 0, 0 }, { 0.12f, 0.02f, d }), Hex(0xfa5252));
    DrawCircle3D({ c.x, 0.02f, c.z }, 2.2f, { 1, 0, 0 }, 90, Hex(0x339af0));
    for (int s = -1; s <= 1; s += 2) DrawCircle3D({ c.x + s * w / 2, 0.02f, c.z }, 3.0f, { 1, 0, 0 }, 90, Hex(0x339af0));
    // rails (with gaps for the goals)
    float gh = GH * SC, rail = 0.45f;
    for (int s = -1; s <= 1; s += 2) lit.Draw(prim.cube, TRS({ c.x, 0.2f, c.z + s * (d / 2 + 0.3f) }, { 0, 0, 0 }, { w + 1.2f, rail, 0.6f }), Hex(0xdee2e6));
    for (int s = -1; s <= 1; s += 2) for (int u = -1; u <= 1; u += 2) { float seg = (d / 2 - gh); lit.Draw(prim.cube, TRS({ c.x + s * (w / 2 + 0.3f), 0.2f, c.z + u * (gh + seg / 2) }, { 0, 0, 0 }, { 0.6f, rail, seg }), s < 0 ? Hex(0xff8787) : Hex(0x74c0fc)); lit.Draw(prim.cube, TRS({ c.x + s * (w / 2 + 0.3f), 0.0f, c.z }, { 0, 0, 0 }, { 0.62f, 0.12f, gh * 2 }), Hex(0x0b1020)); }
    sc.Trees(time);
    // players stand at their end
    int j[2] = { 0, 0 };
    for (int i = 0; i < N(); i++) { int tm = m[i].team; Vector3 pos = W3(tm == 0 ? L - 95 : R + 95, CY + (j[tm]++ - 0.5f) * 170 * (N() > 2 ? 1 : 0) + (N() <= 2 ? 0 : 0)); Shadow(pos, 1.3f); DrawToon(pos, tm == 0 ? PI / 2 : -PI / 2, P(i).color, time + i, 0, 0.3f, 1.4f); }
    for (int i = 0; i < N(); i++) {
      Vector3 p = W3(m[i].x, m[i].y);
      lit.Draw(prim.cyl, TRS(p, { 0, 0, 0 }, { MR * SC * 2, 0.28f, MR * SC * 2 }), P(i).color);
      lit.Draw(prim.cyl, TRS({ p.x, 0.28f, p.z }, { 0, 0, 0 }, { MR * SC * 1.3f, 0.06f, MR * SC * 1.3f }), Shade(P(i).color, 0.3f));
      lit.Draw(prim.cyl, TRS({ p.x, 0.28f, p.z }, { 0, 0, 0 }, { 0.75f, 0.6f, 0.75f }), Shade(P(i).color, -0.2f));
      lit.Draw(prim.sphere, TRS({ p.x, 0.9f, p.z }, { 0, 0, 0 }, { 0.8f, 0.6f, 0.8f }), Shade(P(i).color, -0.1f));
    }
    Vector3 pp = W3(px, py);
    lit.Draw(prim.cyl, TRS(pp, { 0, 0, 0 }, { PR * SC * 2, 0.16f, PR * SC * 2 }), Hex(0x212529));
    lit.Draw(prim.cyl, TRS({ pp.x, 0.16f, pp.z }, { 0, 0, 0 }, { PR * SC * 1.3f, 0.01f, PR * SC * 1.3f }), Hex(0xffd43b));
    if (hypotf(pvx, pvy) > 700) fx.Puff({ pp.x, 0.1f, pp.z }, Hex(0xffd43b, 120), 0.15f, 0.25f);
    fx.Draw(); conf.Draw();
    EndMode3D();
    for (int i = 0; i < N(); i++) NameTag(Vector3Add(W3(m[i].x, m[i].y), { 0, 1.6f, 0 }), i, 17);
    // scoreboard
    UI::Shadow({ 560, 10, 480, 86 }, 30, 20, Hex(0, 90), 6);
    UI::Box({ 560, 10, 240, 86 }, 30, Hex(0xff6b6b), Hex(0xc92a2a)); UI::Box({ 800, 10, 240, 86 }, 30, Hex(0x4dabf7), Hex(0x1864ab));
    Text(690, 54, 60, WHITE, std::to_string(score[0]), 0, TS_DISPLAY | TS_SHADOW); Text(910, 54, 60, WHITE, std::to_string(score[1]), 0, TS_DISPLAY | TS_SHADOW);
    Text(600, 54, 18, Fade(WHITE, 0.85f), "RED", 0, TS_HEAVY | TS_PLAIN); Text(1000, 54, 18, Fade(WHITE, 0.85f), "BLUE", 0, TS_HEAVY | TS_PLAIN);
    Clock(T, 1500);
    Text(800, 118, 18, Hex(0x495057), "First to 7", 0, TS_HEAVY | TS_PLAIN);
    DrawCount();
  }
};

// =====================================================================================
// TAG — you're IT! Tag someone to pass it on. Least time as IT wins.
// =====================================================================================
struct TagGame : Classic {
  struct K : Mover { float itTime = 0, freeze = 0, immune = 0, dashT = 0, dcd = 0; Vector2 wander{}; float wt = 0; };
  std::vector<K> k; std::vector<Vector3> bush; int it = 0; float T = 75; bool over = false;
  const float X0 = 150, X1 = 1450, Y0 = 160, Y1 = 830;
  void Start() override {
    InitCam(44); sc.Init(30, 20, false);
    for (int i = 0; i < N(); i++) { float a = (float)i / N() * 2 * PI; K m; m.x = 800 + cosf(a) * 300; m.y = 495 + sinf(a) * 220; k.push_back(m); }
    for (int tries = 0; tries < 300 && bush.size() < 6; tries++) {
      Vector3 b = { frand(X0 + 150, X1 - 150), frand(Y0 + 110, Y1 - 110), frand(34, 50) }; bool ok = true;
      for (auto& o : bush) if (hypotf(o.x - b.x, o.y - b.y) < o.z + b.z + 120) ok = false;
      for (auto& m : k) if (hypotf(m.x - b.x, m.y - b.y) < b.z + 70) ok = false;
      if (ok) bush.push_back(b);
    }
    it = rand() % N(); k[it].freeze = 0;
    Layout(-1, R"({"name":"pad","stick":"xy","a":"DASH","hint":"Don't be IT! If you are IT, tag someone!"})");
  }
  void Update(float dt) override {
    time += dt; fx.Update(dt); conf.Update(dt);
    if (cd > 0 && cd < 3.4f && lastTick == 4) Banner(P(it).name + " is IT!", 1600, "#ff6b6b");
    if (Counting(dt, "RUN!")) return;
    if (over) return;
    T -= dt; k[it].itTime += dt;
    if (T <= 0) {
      over = true;
      std::vector<int> o = Sorted([&](int i) { return -k[i].itTime; });
      FinishOrder(o, [&](int i) { char b[32]; snprintf(b, sizeof b, "%.1fs as IT", k[i].itTime); return std::string(b); });
      return;
    }
    for (int i = 0; i < N(); i++) {
      K& m = k[i]; Player& p = P(i); Vector2 in = p.stick; bool dash = p.a && !p.pa;
      if (p.bot) {
        dash = false;
        if (i == it) {
          int best = -1; float bd = 1e9f;
          for (int j = 0; j < N(); j++) if (j != i && k[j].immune <= 0) { float d = hypotf(k[j].x - m.x, k[j].y - m.y); if (d < bd) { bd = d; best = j; } }
          if (best >= 0) { in = Toward(m.x, m.y, k[best].x + k[best].vx * 0.25f, k[best].y + k[best].vy * 0.25f); dash = bd < 200 && frand(0, 1) < 0.05f; }
        } else {
          K& c = k[it]; float d = fmaxf(1, hypotf(m.x - c.x, m.y - c.y));
          m.wt -= dt; if (m.wt <= 0) { m.wt = frand(0.6f, 1.5f); m.wander = { frand(-1, 1), frand(-1, 1) }; }
          float w = d < 380 ? 1.0f : 0.25f;
          in = { (m.x - c.x) / d * w + m.wander.x * 0.4f, (m.y - c.y) / d * w + m.wander.y * 0.4f };
          if (m.x < X0 + 120) in.x += 1.2f; if (m.x > X1 - 120) in.x -= 1.2f; if (m.y < Y0 + 100) in.y += 1.2f; if (m.y > Y1 - 100) in.y -= 1.2f;
          dash = d < 160 && frand(0, 1) < 0.06f;
        }
      }
      if (m.freeze > 0) { m.freeze -= dt; in = { 0, 0 }; }
      m.immune -= dt;
      if (dash && m.dcd <= 0) { m.dashT = 0.28f; m.dcd = 2; Sfx("dash"); fx.Burst(W3(m.x, m.y, 0.5f), 10, p.color, 5, 0.4f, 0.15f); }
      m.dashT -= dt; m.dcd -= dt;
      Steer(m, in, (i == it ? 345.f : 320.f) * (m.dashT > 0 ? 1.8f : 1), dt, 9);
      m.x = Clamp(m.x, X0, X1); m.y = Clamp(m.y, Y0, Y1);
      for (auto& b : bush) { float dx = m.x - b.x, dy = m.y - b.y, d = hypotf(dx, dy), r = b.z + 26; if (d < r && d > 0.01f) { m.x = b.x + dx / d * r; m.y = b.y + dy / d * r; } }
    }
    for (int i = 0; i < N(); i++) for (int j = i + 1; j < N(); j++) if (i != it && j != it) Separate(k[i], k[j], 52);
    if (k[it].freeze <= 0)
      for (int j = 0; j < N(); j++) if (j != it && k[j].immune <= 0 && hypotf(k[j].x - k[it].x, k[j].y - k[it].y) < 62) {
        k[it].immune = 1.6f; int old = it; it = j; k[j].freeze = 1.0f;
        Sfx("bonk"); Shake(5); Buzz(j, 300); Buzz(old, 60);
        fx.Burst(W3(k[j].x, k[j].y, 1.2f), 24, Hex(0xff6b6b), 7, 0.6f, 0.18f);
        Banner(P(j).name + " is IT!", 1000, "#ff6b6b");
        break;
      }
  }
  void Draw() override {
    cam.position = { sinf(time * 0.1f) * 1.2f, 24, 17.5f }; cam.target = { 0, 0, 0.9f };
    sc.Begin(cam);
    lit.Draw(prim.cube, TRS(W3(800, (Y0 + Y1) / 2, 0.01f), { 0, 0, 0 }, { (X1 - X0 + 80) * SC, 0.02f, (Y1 - Y0 + 80) * SC }), Hex(0x8ce99a));
    for (int i = 0; i <= 26; i++) for (float y : { Y0 - 40, Y1 + 40 }) lit.Draw(prim.cube, TRS(W3(X0 - 40 + (X1 - X0 + 80) * i / 26, y, 0.35f), { 0, 0, 0 }, { 0.15f, 0.7f, 0.15f }), WHITE);
    for (int i = 0; i <= 14; i++) for (float x : { X0 - 40, X1 + 40 }) lit.Draw(prim.cube, TRS(W3(x, Y0 - 40 + (Y1 - Y0 + 80) * i / 14, 0.35f), { 0, 0, 0 }, { 0.15f, 0.7f, 0.15f }), WHITE);
    sc.Trees(time);
    for (auto& b : bush) { Vector3 c = W3(b.x, b.y); float r = b.z * SC; for (int i = 0; i < 5; i++) { float a = i * 1.26f; lit.Draw(prim.sphere, TRS({ c.x + cosf(a) * r * 0.5f, r * 0.7f, c.z + sinf(a) * r * 0.5f }, { 0, 0, 0 }, { r * 1.3f, r * 1.3f, r * 1.3f }), i % 2 ? Hex(0x2b8a3e) : Hex(0x37b24d)); } lit.Draw(prim.sphere, TRS({ c.x, r * 1.1f, c.z }, { 0, 0, 0 }, { r * 1.4f, r * 1.2f, r * 1.4f }), Hex(0x40c057)); }
    for (int i = 0; i < N(); i++) {
      K& m = k[i]; Vector3 pos = W3(m.x, m.y); float sp = hypotf(m.vx, m.vy);
      if (i == it) { float pr = 1.3f + 0.15f * sinf(time * 8); lit.Draw(prim.cyl, TRS({ pos.x, 0.05f, pos.z }, { 0, 0, 0 }, { pr * 1.6f, 0.02f, pr * 1.6f }), Hex(0xff6b6b, 170)); }
      else Shadow(pos, 1.2f);
      DrawToon(pos, PI / 2 - m.face, m.immune > 0 && (int)(time * 10) % 2 ? WHITE : P(i).color, time + i, fabsf(sinf(time * 13 + i)) * 0.28f * fminf(1, sp / 200), i == it ? 0.8f : 0, 1.35f);
    }
    fx.Draw(); conf.Draw();
    EndMode3D();
    for (int i = 0; i < N(); i++) {
      Vector3 pos = W3(k[i].x, k[i].y);
      if (i == it) { Vector2 s = GetWorldToScreen({ pos.x, 4.6f + 0.2f * sinf(time * 6), pos.z }, cam); Text(s.x, s.y - 26, 40, Hex(0xff4d4d), k[i].freeze > 0 ? "IT! " + std::to_string((int)ceilf(k[i].freeze)) : "IT!", 0, TS_DISPLAY | TS_SHADOW); }
      NameTag({ pos.x, 3.4f, pos.z }, i, 18);
    }
    std::vector<int> o = Sorted([&](int i) { return -k[i].itTime; });
    Board("TIME AS IT (low wins)", o, [&](int i) { char b[16]; snprintf(b, sizeof b, "%.0fs", k[i].itTime); return std::string(b); });
    Clock(T);
    Hint(std::string("🏃 ") + P(it).name + " is IT — run!", Hex(0xffc9c9));
    DrawCount();
  }
};
}  // namespace

std::unique_ptr<Game> MakeChairs() { return std::make_unique<Chairs>(); }
std::unique_ptr<Game> MakeRedLight() { return std::make_unique<RedLight>(); }
std::unique_ptr<Game> MakeSimon() { return std::make_unique<Simon>(); }
std::unique_ptr<Game> MakeTug() { return std::make_unique<Tug>(); }
std::unique_ptr<Game> MakeHockey() { return std::make_unique<Hockey>(); }
std::unique_ptr<Game> MakeTag() { return std::make_unique<TagGame>(); }
