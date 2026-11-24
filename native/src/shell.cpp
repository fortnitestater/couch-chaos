// CC OS — the console shell (boot, home, intro, results) drawn natively.
// The website sends a JSON snapshot of what to show (cc_shell); we animate + draw it (cc_shell_frame).
#include "cc.h"
#include <emscripten/emscripten.h>
#include <map>
#include "rlgl.h"

namespace {
J D;                          // latest snapshot
std::string scr = "boot", shown = "boot";
float fade = 1, scrT = 0, T = 0; // fade: 1 = fully visible
WinterBG bg; Confetti conf; Camera3D cam{};
bool bgInit = false;

// --- animation state ---
std::map<std::string, Spring> tileS;  // focus scale per tile
Spring rowScroll, heroIn, playPulse; std::vector<Spring> colScroll;
std::string heroId; float heroT = 0;
struct Hit { Rectangle r; int code; };
std::vector<Hit> hits;
int lastFocusR = -1;

Color C(const J& j, Color d = Hex(0x888888)) { return j.t == J::STR ? ParseColor(j.s) : d; }
Color A(Color c, float a) { c.a = (unsigned char)Clamp(c.a * a, 0, 255); return c; }
const Color INK = Hex(0xf4f6ff), MUTED = Hex(0xa7b0d6), GLASS = Hex(0x0e1433, 175), EDGE = Hex(0xffffff, 34);

void Glass(Rectangle r, float rad, float a = 1) {
  UI::Shadow(r, rad, 34, A(Hex(0x02040f, 150), a), 14);
  UI::Box(r, rad, A(Hex(0x161d45, 200), a), A(Hex(0x0b1030, 215), a), 90, A(EDGE, a), 1.2f);
}
void Chip(float x, float y, const std::string& s, Color bg, Color fg, float size = 18, float a = 1, int align = -1) {
  float w = Measure(s, size, TS_PLAIN) + size * 1.3f, h = size * 1.9f;
  if (align == 0) x -= w / 2; else if (align == 1) x -= w;
  UI::Box({ x, y - h / 2, w, h }, h / 2, A(bg, a));
  Text(x + w / 2, y + 1, size, A(fg, a), s, 0, TS_PLAIN);
}
float ChipW(const std::string& s, float size = 18) { return Measure(s, size, TS_PLAIN) + size * 1.3f; }
void Rays(Vector2 c, float r, float t, Color col, int n = 14) {
  BeginBlendMode(BLEND_ADDITIVE);
  for (int i = 0; i < n; i++) { float a = t + i * 2 * PI / n, w = PI / n * 0.55f; DrawTriangle(c, { c.x + cosf(a + w) * r, c.y + sinf(a + w) * r }, { c.x + cosf(a - w) * r, c.y + sinf(a - w) * r }, col); }
  EndBlendMode();
}

// ---------------- BOOT ----------------
void DrawBoot(float a) {
  float bob = sinf(T * 1.6f) * 6;
  UI::Shadow({ 420, 250 + bob, 760, 260 }, 60, 80, A(Hex(0x000000, 110), a), 20);
  Text(800, 330 + bob, 150, A(INK, a), "COUCH", 0, TS_DISPLAY | TS_SHADOW);
  Text(800, 460 + bob, 150, A(Hex(0xff4d8d), a), "CHAOS", 0, TS_DISPLAY | TS_SHADOW);
  Chip(800, 572, "❄️  WINTER EDITION", Hex(0xffffff, 40), INK, 22, a, 0);
  float p = 0.55f + 0.45f * sinf(T * 3);
  UI::Box({ 620, 650, 360, 72 }, 36, A(Hex(0xffffff, (unsigned char)(235 * p + 20)), a), A(Hex(0xdfe7ff), a));
  Text(800, 687, 28, A(Hex(0x101633), a), "▶  Press any key", 0, TS_PLAIN | TS_HEAVY);
  Text(800, 770, 20, A(MUTED, a), "or click · F = fullscreen · phones join on the next screen", 0, TS_PLAIN | TS_LIGHT);
}

// ---------------- HOME ----------------
void DrawTopBar(float a) {
  Text(48, 50, 38, A(INK, a), "COUCH", -1, TS_DISPLAY);
  float w = Measure("COUCH", 38, TS_DISPLAY);
  Text(48 + w + 8, 50, 38, A(Hex(0xff4d8d), a), "CHAOS", -1, TS_DISPLAY);
  float w2 = Measure("CHAOS", 38, TS_DISPLAY);
  Chip(48 + w + w2 + 30, 50, "❄️ Winter Edition", Hex(0xffffff, 26), MUTED, 15, a);
  // right: avatars · room · clock
  float x = 1552;
  std::string clock = D["clock"].str();
  Text(x, 50, 26, A(INK, a), clock, 1, TS_PLAIN | TS_HEAVY); x -= Measure(clock, 26, TS_PLAIN) + 22;
  std::string room = "Room  " + D["code"].str(); float rw = ChipW(room, 17);
  Chip(x - rw, 50, room, Hex(0xffffff, 30), INK, 17, a); x -= rw + 18;
  const J& ps = D["players"];
  for (int i = (int)ps.size() - 1; i >= 0; i--) {
    const J& p = ps[i]; Color c = C(p["c"]); bool away = p["away"].yes();
    float cx = x - 22, pop = 1 + 0.06f * sinf(T * 3 + i);
    UI::Shadow({ cx - 21, 29, 42, 42 }, 21, 10, A(Hex(0x000000, 90), a), 3);
    UI::Box({ cx - 21 * pop, 50 - 21 * pop, 42 * pop, 42 * pop }, 21 * pop, A(Shade(c, 0.15f), away ? a * 0.35f : a), A(Shade(c, -0.2f), away ? a * 0.35f : a), 90, A(Hex(0xffffff, 120), a), 2);
    Text(cx, 51, 22, A(WHITE, away ? a * 0.4f : a), p["cam"].yes() ? "📷" : p["av"].str(), 0, TS_PLAIN);
    if (p["vip"].yes()) Text(cx, 24, 16, A(WHITE, a), "👑", 0, TS_PLAIN);
    x -= 50;
  }
}
void DrawHero(float a, float dt) {
  const J& h = D["hero"]; std::string id = h["id"].str();
  if (id != heroId) { heroId = id; heroIn.x = 0; heroIn.v = 0; }
  heroIn.To(1, dt, 120, 17); float k = Clamp(heroIn.x, 0, 1.2f);
  Rectangle card = { 48, 100, 1080, 330 };
  Glass(card, 30, a);
  Color a1 = C(h["a1"], Hex(0x4dabff)), a2 = C(h["a2"], Hex(0x1864ab));
  Rectangle art = { 48, 100, 380, 330 };
  UI::Box(art, 30, A(a1, a), A(a2, a), 120);
  BeginScissorMode((int)art.x, (int)art.y, (int)art.width, (int)art.height);
  Rays({ art.x + 190, art.y + 165 }, 420, T * 0.25f, A(Hex(0xffffff, 26), a));
  EndScissorMode();
  UI::Box({ art.x, art.y + 220, art.width, 110 }, 30, A(Hex(0x000000, 0), a), A(Hex(0x000000, 60), a));
  float bob = sinf(T * 2.2f) * 8, sc = 0.6f + 0.4f * EaseBack(k);
  Text(art.x + 190, art.y + 165 + bob, 150 * sc, A(WHITE, a), h["e"].str(), 0, TS_PLAIN | TS_SHADOW);
  float ox = (1 - k) * 30, ia = a * Clamp(k, 0, 1);
  float tx = 470 + ox;
  Text(tx, 140, 17, A(Shade(a1, 0.45f), ia), h["cat"].str(), -1, TS_PLAIN | TS_HEAVY);
  Text(tx, 190, 58, A(INK, ia), h["n"].str(), -1, TS_DISPLAY);
  Text(tx, 240, 22, A(MUTED, ia), h["d"].str(), -1, TS_PLAIN | TS_LIGHT, 610);
  float bx = tx; const char* keys[] = { "ctrl", "pl", "mins" };
  for (auto key : keys) { std::string s = h[key].str(); if (s.empty()) continue; Chip(bx, 330, s, Hex(0xffffff, 26), INK, 16, ia); bx += ChipW(s, 16) + 10; }
  // buttons
  playPulse.To(1, dt); float pp = 1 + 0.025f * sinf(T * 4);
  Rectangle play = { tx, 362, 170 * pp, 52 * pp };
  UI::Shadow(play, 26, 18, A(a1, ia * 0.8f), 6);
  UI::Box(play, 26, A(WHITE, ia), A(Hex(0xe3e9ff), ia));
  Text(play.x + play.width / 2, play.y + play.height / 2 + 1, 22, A(Hex(0x111632), ia), "▶  Play", 0, TS_PLAIN | TS_HEAVY);
  hits.push_back({ play, 2 });
  Rectangle party = { tx + 190, 362, 210, 52 };
  UI::Box(party, 26, A(Hex(0xffffff, 22), ia), A(Hex(0xffffff, 22), ia), 90, A(Hex(0xffffff, 70), ia), 1.5f);
  Text(party.x + party.width / 2, party.y + 27, 20, A(INK, ia), "🎉  Party Mode", 0, TS_PLAIN | TS_HEAVY);
  hits.push_back({ party, 3 });
}
void DrawJoin(float a) {
  Rectangle r = { 1152, 100, 400, 330 };
  Glass(r, 30, a);
  Text(r.x + 200, 128, 19, A(INK, a), "📱  Join with your phone", 0, TS_PLAIN | TS_HEAVY);
  // QR code drawn natively from the module bitmap
  const J& q = D["qr"]; int n = q["n"].i(); const std::string& m = q["m"].str();
  Rectangle qr = { r.x + 200 - 84, 148, 168, 168 };
  UI::Box(qr, 18, A(WHITE, a));
  if (n > 0 && (int)m.size() >= n * n) {
    float cell = 148.f / n, ox = qr.x + 10, oy = qr.y + 10;
    for (int y = 0; y < n; y++) for (int x = 0; x < n; x++) if (m[y * n + x] == '1') DrawRectangleRec({ ox + x * cell, oy + y * cell, cell + 0.6f, cell + 0.6f }, A(Hex(0x0a1024), a));
  } else Text(qr.x + 84, qr.y + 84, 18, A(Hex(0x445), a), "…", 0, TS_PLAIN);
  std::string code = D["code"].str(); std::string spaced; for (char ch : code) { spaced += ch; spaced += ' '; }
  Text(r.x + 200, 352, 42, A(INK, a), spaced, 0, TS_DISPLAY);
  Text(r.x + 200, 384, 14, A(MUTED, a), D["url"].str(), 0, TS_PLAIN | TS_LIGHT);
  std::string net = D["net"].str();
  Text(r.x + 200, 410, 14, A(Hex(0x8ce99a), a), net, 0, TS_PLAIN | TS_HEAVY);
}
void DrawRows(float a, float dt) {
  const J& rows = D["rows"]; int fr = D["f"][0].i(), fc = D["f"][1].i();
  const float top = 470, rowH = 196, tw = 226, th = 136, gap = 20, x0 = 48;
  if (colScroll.size() != rows.size()) colScroll.assign(rows.size(), Spring{});
  rowScroll.To(fmaxf(0, fr - 0.0f) * rowH, dt, 150, 22);
  BeginScissorMode(0, (int)top - 8, W, (int)(H - top - 30));
  TextClip(0, top - 8, W, H - top - 30);
  for (size_t r = 0; r < rows.size(); r++) {
    float y = top + r * rowH - rowScroll.x;
    if (y < top - rowH || y > H) continue;
    bool frow = (int)r == fr;
    const J& items = rows[r]["items"];
    int c0 = frow ? fc : 0; float want = fmaxf(0, (c0 - 4) * (tw + gap) + (frow && c0 >= 5 ? (tw + gap) * 0.5f : 0));
    colScroll[r].To(frow ? want : colScroll[r].x, dt, 150, 22);
    float ra = a * (frow ? 1 : 0.62f);
    Text(x0, y + 12, 22, A(INK, ra), rows[r]["name"].str(), -1, TS_PLAIN | TS_HEAVY);
    for (size_t c = 0; c < items.size(); c++) {
      const J& it = items[c]; std::string key = std::to_string(r) + ":" + it["id"].str();
      bool foc = frow && (int)c == fc;
      Spring& s = tileS[key]; if (s.x == 0) s.x = 1; s.To(foc ? 1.09f : 1, dt, 260, 20);
      float x = x0 + c * (tw + gap) - colScroll[r].x, cy = y + 40 + th / 2 - (foc ? 6 : 0) * (s.x - 1) / 0.09f;
      if (x > W + 20 || x + tw < -20) continue;
      float w = tw * s.x, hh = th * s.x; Rectangle tr = { x + tw / 2 - w / 2, cy - hh / 2, w, hh };
      Color a1 = C(it["a1"]), a2 = C(it["a2"]);
      if (foc) UI::Shadow(tr, 24, 34, A(a1, a * 0.9f), 10); else UI::Shadow(tr, 24, 16, A(Hex(0x000000, 120), ra), 6);
      UI::Box(tr, 24, A(Shade(a1, 0.05f), ra), A(a2, ra), 125, foc ? A(WHITE, a) : A(Hex(0xffffff, 40), ra), foc ? 4.f : 1.f);
      // gloss + special shimmer
      UI::Box({ tr.x, tr.y, tr.width, tr.height * 0.5f }, 24, A(Hex(0xffffff, 34), ra), A(Hex(0xffffff, 0), ra));
      if (it["sp"].yes()) { float sx = fmodf(T * 260 + c * 120, tr.width + 200) - 100; BeginScissorMode((int)tr.x, (int)tr.y, (int)tr.width, (int)tr.height); BeginBlendMode(BLEND_ADDITIVE); DrawRectangleGradientH((int)(tr.x + sx), (int)tr.y, 60, (int)tr.height, Hex(0xffffff, 0), Hex(0xffffff, 70)); EndBlendMode(); EndScissorMode(); BeginScissorMode(0, (int)top - 8, W, (int)(H - top - 30)); }
      float eb = foc ? sinf(T * 5) * 4 : 0;
      Text(tr.x + tr.width - 52 * s.x, tr.y + tr.height * 0.42f + eb, 58 * s.x, A(WHITE, ra), it["e"].str(), 0, TS_PLAIN | TS_SHADOW);
      Text(tr.x + 16, tr.y + tr.height - 24 * s.x, 20 * s.x, A(WHITE, ra), it["n"].str(), -1, TS_PLAIN | TS_HEAVY | TS_SHADOW, tr.width - 70);
      std::string cl = it["ctrl"].str();
      if (!cl.empty()) Chip(tr.x + 12, tr.y + 22 * s.x, cl, Hex(0x000000, 70), Hex(0xffffff, 230), 12 * s.x, ra);
      if (it["rec"].yes()) Chip(tr.x + tr.width - 12, tr.y + 22 * s.x, "For you", Hex(0xffffff, 215), Hex(0x111632), 11 * s.x, ra, 1);
      hits.push_back({ tr, 1000 + (int)r * 100 + (int)c });
    }
  }
  EndScissorMode(); TextUnclip();
  // row indicator dots
  for (size_t r = 0; r < rows.size(); r++) DrawCircle(1578, (int)(480 + r * 18), (int)r == fr ? 5 : 3, A(Hex(0xffffff, (int)r == fr ? 220 : 80), a));
  UI::Box({ 0, H - 44, W, 44 }, 0, A(Hex(0x060918, 0), a), A(Hex(0x060918, 200), a));
  Text(800, H - 20, 16, A(MUTED, a), "🎮  The 👑 VIP's phone is the remote  ·  ← → ↑ ↓ + OK  ·  keyboard and mouse work too  ·  P = party  ·  R = surprise", 0, TS_PLAIN);
}

// ---------------- INTRO ----------------
void DrawIntro(float a, float dt) {
  const J& in = D["intro"]; float t = scrT;
  Color a1 = C(in["a1"], Hex(0x4dabff)), a2 = C(in["a2"], Hex(0x1864ab));
  // left: art stage
  float slide = (1 - EaseOut(t * 2.2f)) * -200;
  Rectangle art = { 40 + slide, 40, 700, 820 };
  UI::Shadow(art, 40, 50, A(a1, a * 0.6f), 16);
  UI::Box(art, 40, A(a1, a), A(a2, a), 115);
  BeginScissorMode((int)art.x, (int)art.y, (int)art.width, (int)art.height);
  Rays({ art.x + 350, art.y + 380 }, 900, T * 0.3f, A(Hex(0xffffff, 30), a), 18);
  for (int i = 0; i < 26; i++) { float px = art.x + fmodf(i * 137.f + T * (20 + i % 5 * 8), art.width), py = art.y + art.height - fmodf(i * 53.f + T * (40 + i % 7 * 9), art.height); DrawCircle((int)px, (int)py, 2 + i % 4, A(Hex(0xffffff, 90), a)); }
  EndScissorMode();
  float pop = EaseBack(Clamp((t - 0.15f) * 2.2f, 0, 1));
  Text(art.x + 350, art.y + 370 + sinf(T * 2) * 14, 260 * pop + 1, A(WHITE, a), in["e"].str(), 0, TS_PLAIN | TS_SHADOW);
  UI::Box({ art.x, art.y + 640, art.width, 180 }, 40, A(Hex(0x000000, 0), a), A(Hex(0x000000, 90), a));
  Text(art.x + 350, art.y + 760, 22, A(Hex(0xffffff, 220), a), in["cat"].str(), 0, TS_PLAIN | TS_HEAVY);
  // right: info card
  float rx = 790, k = EaseOut(Clamp((t - 0.1f) * 2.4f, 0, 1)), ca = a * k;
  Rectangle card = { rx + (1 - k) * 80, 40, 770, 820 };
  Glass(card, 40, ca);
  float x = card.x + 56, y = 120;
  Text(x, y, 72, A(INK, ca), in["n"].str(), -1, TS_DISPLAY); y += 70;
  Chip(x, y, in["ctrl"].str(), A(a1, 0.85f), WHITE, 18, ca); y += 62;
  const J& how = in["how"];
  for (size_t i = 0; i < how.size(); i++) {
    float kk = EaseOut(Clamp((t - 0.35f - i * 0.12f) * 3, 0, 1)), ia = ca * kk;
    float yy = y + i * 74;
    UI::Box({ x + (1 - kk) * 40, yy - 24, 48, 48 }, 24, A(a1, ia), A(a2, ia));
    Text(x + 24 + (1 - kk) * 40, yy + 1, 24, A(WHITE, ia), std::to_string(i + 1), 0, TS_DISPLAY);
    Text(x + 70 + (1 - kk) * 40, yy, 26, A(INK, ia), how[i].str(), -1, TS_PLAIN, 580);
  }
  // ready chips
  float ry = 640; Text(x, ry, 18, A(MUTED, ca), "PLAYERS", -1, TS_PLAIN | TS_HEAVY); ry += 42;
  const J& rd = in["ready"]; float cx = x;
  for (size_t i = 0; i < rd.size(); i++) {
    const J& p = rd[i]; bool ok = p["ok"].yes(); std::string s = (ok ? "✓ " : "") + p["av"].str() + " " + p["n"].str();
    float w = ChipW(s, 19); if (cx + w > card.x + card.width - 40) { cx = x; ry += 52; }
    Color pc = C(p["c"]);
    UI::Box({ cx, ry - 20, w, 40 }, 20, ok ? A(pc, ca) : A(Hex(0xffffff, 18), ca), ok ? A(Shade(pc, -0.2f), ca) : A(Hex(0xffffff, 18), ca), 90, A(pc, ca), ok ? 0 : 2);
    Text(cx + w / 2, ry + 1, 19, A(WHITE, ca), s, 0, TS_PLAIN | TS_HEAVY);
    cx += w + 12;
  }
  // countdown ring
  float left = in["left"].num(15), tot = 15;
  Vector2 rc = { card.x + card.width - 110, 130 };
  DrawRing(rc, 46, 56, 0, 360, 64, A(Hex(0xffffff, 26), ca));
  DrawRing(rc, 46, 56, -90, -90 + 360 * Clamp(left / tot, 0, 1), 64, A(a1, ca));
  Text(rc.x, rc.y + 2, 40, A(INK, ca), std::to_string((int)ceilf(fmaxf(0, left))), 0, TS_DISPLAY);
  Text(card.x + card.width / 2, 822, 19, A(MUTED, ca), "Tap  READY  on your phone  ·  OK to start now", 0, TS_PLAIN);
}

// ---------------- RESULTS ----------------
float podRise[3] = { 0, 0, 0 }; Spring podS[3];
void DrawResults(float a, float dt) {
  const J& R = D["res"]; float t = scrT;
  cam.position = { sinf(T * 0.25f) * 3.5f, 5.0f, 12.5f }; cam.target = { 0, 3.1f, 0 }; cam.fovy = 40;
  UI::Sky(Hex(0x120a35), Hex(0x3a1f6e));
  BeginBlendMode(BLEND_ADDITIVE);
  for (int i = 0; i < 6; i++) { float x = 200 + i * 240 + sinf(T * 0.6f + i) * 120; DrawTriangle({ x, -20 }, { x - 220, (float)H }, { x + 220, (float)H }, Hex(i % 2 ? 0xffd43b : 0x8b5cf6, 16)); }
  EndBlendMode();
  lit.Begin(cam, { 0.15f, 0.09f, 0.3f }, 0.02f);
  BeginMode3D(cam);
  lit.Draw(prim.cyl, TRS({ 0, -0.3f, 0 }, { 0, 0, 0 }, { 16, 0.3f, 9 }), Hex(0x2b1d5c));
  lit.Draw(prim.cyl, TRS({ 0, -0.02f, 0 }, { 0, 0, 0 }, { 12, 0.04f, 7 }), Hex(0x3d2a80));
  const J& pod = R["pod"];
  const float px[3] = { 0, -2.6f, 2.6f }, ph[3] = { 2.4f, 1.7f, 1.15f };
  const unsigned pc[3] = { 0xffd43b, 0xced4da, 0xe8a87c };
  Vector3 heads[3]; bool have[3] = { false, false, false };
  for (int i = 0; i < 3 && i < (int)pod.size(); i++) {
    float delay = (2 - i) * 0.35f; podS[i].To(t > delay ? 1 : 0, dt, 90, 12);
    float h = ph[i] * Clamp(podS[i].x, 0, 1.3f);
    lit.Draw(prim.cube, TRS({ px[i], h / 2, 0 }, { 0, 0, 0 }, { 2.3f, h, 2.3f }), Hex(0x4c3a9e));
    lit.Draw(prim.cube, TRS({ px[i], h - 0.08f, 0 }, { 0, 0, 0 }, { 2.36f, 0.18f, 2.36f }), Hex(pc[i]));
    lit.Draw(prim.cube, TRS({ px[i], h * 0.5f, 1.16f }, { 0, 0, 0 }, { 1.0f, 0.9f * fminf(1, h), 0.04f }), Hex(pc[i]));
    Color c = C(pod[i]["c"]);
    float hop = i == 0 ? fabsf(sinf(T * 4.5f)) * 0.6f : fabsf(sinf(T * 3.2f + i)) * 0.25f;
    if (podS[i].x > 0.5f) DrawToon({ px[i], h, 0 }, sinf(T + i) * 0.3f, c, T, hop, i == 0 ? 1 : 0.4f, 1.15f);
    heads[i] = { px[i], h + 2.3f, 0 }; have[i] = podS[i].x > 0.5f;
    if (i == 0 && t > 1.0f && fmodf(T, 1.4f) < dt) conf.Spawn({ 0, 6, 0 }, 60, 3, 4);
  }
  conf.Update(dt); conf.Draw();
  EndMode3D();
  if (t < dt * 2) conf.Spawn({ 0, 7, 0 }, 170, 5, 5);
  // labels
  for (int i = 0; i < 3; i++) if (have[i]) {
    Vector2 s = GetWorldToScreen(heads[i], cam);
    Text(s.x, s.y - 10, 30, A(INK, a), pod[i]["n"].str(), 0, TS_DISPLAY);
    Text(s.x, s.y + 22, 17, A(Hex(0xffe8a3), a), pod[i]["sub"].str(), 0, TS_PLAIN | TS_HEAVY);
    Vector2 b = GetWorldToScreen({ px[i], ph[i] * 0.5f, 1.2f }, cam);
    Text(b.x, b.y, 44, A(Hex(0x2b1d5c), a), std::to_string(i + 1), 0, TS_DISPLAY | TS_PLAIN);
  }
  float k = EaseOut(t * 2);
  Text(800, 48 - (1 - k) * 40, 20, A(Hex(0xffd43b), a * k), R["k"].str(), 0, TS_PLAIN | TS_HEAVY);
  Text(800, 94 - (1 - k) * 40, 50, A(INK, a * k), R["title"].str(), 0, TS_DISPLAY);
  const J& list = R["list"];
  float lx = 800 - (fminf(5, (float)list.size()) * 230) / 2, ly = 770;
  for (size_t i = 0; i < list.size() && i < 5; i++) {
    const J& p = list[i]; float kk = EaseOut(Clamp((t - 1.2f - i * 0.1f) * 3, 0, 1));
    Rectangle r = { lx + i * 230, ly + (1 - kk) * 30, 215, 64 };
    Glass(r, 20, a * kk);
    UI::Box({ r.x + 8, r.y + 8, 6, 48 }, 3, A(C(p["c"]), a * kk));
    Text(r.x + 26, r.y + 22, 19, A(INK, a * kk), "#" + std::to_string(i + 4) + "  " + p["n"].str(), -1, TS_PLAIN | TS_HEAVY);
    Text(r.x + 26, r.y + 46, 14, A(MUTED, a * kk), p["st"].str(), -1, TS_PLAIN);
  }
  Text(800, 868, 18, A(MUTED, a), R["foot"].str(), 0, TS_PLAIN);
}
}  // namespace

extern "C" {
EMSCRIPTEN_KEEPALIVE void cc_shell(const char* json) {
  D = J::Parse(json);
  std::string s = D["st"].str(); if (s.empty()) s = "home";
  if (s != scr) { scr = s; }
}
EMSCRIPTEN_KEEPALIVE int cc_shell_pick(float x, float y) {
  for (int i = (int)hits.size() - 1; i >= 0; i--) if (CheckCollisionPointRec({ x, y }, hits[i].r)) return hits[i].code;
  return 0;
}
EMSCRIPTEN_KEEPALIVE void cc_shell_frame(float dt) {
  dt = fminf(dt, 0.05f); T += dt; scrT += dt;
  if (!bgInit) { bg.Init(); bgInit = true; cam.up = { 0, 1, 0 }; cam.projection = CAMERA_PERSPECTIVE; }
  // screen change: fade out the old one, then fade in the new one
  if (shown != scr) { fade -= dt * 6; if (fade <= 0) { fade = 0; shown = scr; scrT = 0; tileS.clear(); if (shown == "results") { conf.l.clear(); for (auto& s : podS) s = Spring{}; } } }
  else fade = fminf(1, fade + dt * 5);
  float a = EaseOut(fade);
  hits.clear();
  bg.Update(dt);
  PostBegin();
  rlEnableDepthMask(); ClearBackground(Hex(0x070b24));
  if (shown == "results") DrawResults(a, dt);
  else {
    cam.fovy = 50;
    cam.position = { sinf(T * 0.05f) * 5, 6.5f + sinf(T * 0.1f) * 0.4f, 24 }; cam.target = { sinf(T * 0.05f) * 2, 4.5f, -10 };
    bg.Draw(cam);
    // legibility veil
    UI::Box({ 0, 0, W, H }, 0, Hex(0x060918, 40), Hex(0x060918, shown == "home" ? 170 : 120));
    if (shown == "boot") DrawBoot(a);
    else if (shown == "home") { DrawTopBar(a); DrawHero(a, dt); DrawJoin(a); DrawRows(a, dt); }
    else if (shown == "intro") DrawIntro(a, dt);
  }
  PostEnd();
  lit.EndFrame(dt);
}
}
