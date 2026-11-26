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
const Color INK = Hex(0xf4f6ff), MUTED = Hex(0xa7b0d6), GLASS = Hex(0x0e1433, 175), EDGE = Hex(0xd9aa5a, 120);

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

// ---------------- HOME (gold & midnight console) ----------------
const Color GOLDN = Hex(0xf2c66d), GOLDN_D = Hex(0xb8873a), NAVY = Hex(0x0a1633), CREAM = Hex(0xfff3d6);

struct Spark { float x, y, s, ph, sp; };
std::vector<Spark> sparks;
Spring tabX, tabW, rowIn, carousel, descIn; std::vector<float> rowMemoScroll;
int lastRow = -1, lastCol = -1; std::string lastDescId;

void NightSky(float a) {
  DrawRectangleGradientV(0, 0, W, H, Hex(0x0b1b42), Hex(0x050c22));
  // soft blue haze in the middle + darker corners
  DrawCircleGradient(800, 380, 820, Hex(0x1a3570, 120), Hex(0x0b1b42, 0));
  DrawCircleGradient(260, 760, 420, Hex(0x14295a, 150), Hex(0x14295a, 0));
  DrawCircleGradient(1380, 800, 460, Hex(0x14295a, 150), Hex(0x14295a, 0));
  // drifting clouds at the bottom corners
  for (int i = 0; i < 9; i++) {
    float cx = (i < 5 ? 60 + i * 95.f : 1250 + (i - 5) * 110.f) + sinf(T * 0.08f + i) * 14, cy = 800 - (i % 3) * 28 + cosf(T * 0.1f + i) * 5;
    DrawCircleGradient((int)cx, (int)cy, 150 + (i % 3) * 30.f, Hex(0x1d3466, 95), Hex(0x1d3466, 0));
  }
  // luminous gold wave sweeping across the horizon
  BeginBlendMode(BLEND_ADDITIVE);
  for (int pass = 0; pass < 2; pass++) {
    float amp = pass ? 30 : 46, off = pass ? 26 : 0, sp = pass ? 0.11f : 0.07f;
    Vector2 prev{};
    for (int x = -20; x <= W + 20; x += 10) {
      float y = 700 + off + amp * sinf(x * 0.0024f + T * sp + pass * 1.7f) + 0.00011f * (x - 800) * (x - 800) * (pass ? -0.4f : -0.6f);
      Vector2 p = { (float)x, y };
      if (x > -20) {
        float edge = 1 - fabsf(x - 800) / 900.f; edge = Clamp(edge, 0, 1);
        unsigned char k = (unsigned char)(255 * edge * a);
        DrawLineEx(prev, p, 16, Fade(Hex(0xf2b84b), 0.05f * k / 255 * (pass ? 0.6f : 1)));
        DrawLineEx(prev, p, 5, Fade(Hex(0xf6c96a), 0.18f * k / 255 * (pass ? 0.6f : 1)));
        DrawLineEx(prev, p, 1.6f, Fade(Hex(0xfff0c8), 0.75f * k / 255 * (pass ? 0.5f : 1)));
      }
      prev = p;
    }
  }
  // gold bokeh sparkles drifting upwards, twinkling
  if (sparks.empty()) for (int i = 0; i < 90; i++) sparks.push_back({ frand(0, W), frand(0, H), frand(1.2f, 3.4f), frand(0, 6.28f), frand(4, 14) });
  for (auto& s : sparks) {
    float y = fmodf(s.y - T * s.sp + H * 10, H), tw = 0.45f + 0.55f * sinf(T * 1.3f + s.ph);
    Color c = Hex(0xf5cf7a, (unsigned char)(160 * tw * a));
    DrawCircleGradient((int)s.x, (int)y, s.s * 5, Fade(c, 0.22f * tw), Fade(c, 0));
    DrawCircleV({ s.x, y }, s.s * 0.7f, c);
  }
  EndBlendMode();
  // thin gold frame
  DrawRectangleRoundedLinesEx({ 44, 40, 1512, 820 }, 0.045f, 12, 1.4f, Hex(0xd9aa5a, (unsigned char)(110 * a)));
}
void Logo(float x, float y, float sc, float a, bool center = false) {
  float s = 64 * sc;
  float wc = Measure("couch", s, TS_DISPLAY), wh = Measure("chaos", s, TS_DISPLAY), wo = Measure("os", s, TS_DISPLAY);
  float x0 = center ? x - (wh + wo + 14 * sc) / 2 : x;
  Text(x0, y, s, A(WHITE, a), "couch", -1, TS_DISPLAY | TS_PLAIN | TS_GOLD | TS_SHADOW);
  Text(x0, y + s * 0.86f, s, A(WHITE, a), "chaos", -1, TS_DISPLAY | TS_PLAIN | TS_SILVER | TS_SHADOW);
  Text(x0 + wh + 14 * sc, y + s * 0.86f, s, A(WHITE, a), "os", -1, TS_DISPLAY | TS_PLAIN | TS_GOLD | TS_SHADOW);
  for (int i = 0; i < 3; i++) DrawCircleV({ x0 + wc + 18 * sc + i * 13 * sc, y - 8 * sc + (i == 1 ? -9 : 0) * sc }, (i == 1 ? 4.5f : 3.2f) * sc, A(GOLDN, a * (0.6f + 0.4f * sinf(T * 2 + i))));
  Text(x0 + (wh + wo + 14 * sc) / 2, y + s * 1.55f, 17 * sc, A(Hex(0xc9d2ea), a), "gaming console", 0, TS_PLAIN | TS_LIGHT);
}
std::string Upper(std::string s) { for (auto& ch : s) if (ch >= 'a' && ch <= 'z') ch -= 32; return s; }
void Icon(int kind, float cx, float cy, float s, Color c) {
  if (kind == 0) {  // gamepad
    DrawRectangleRoundedLinesEx({ cx - s, cy - s * 0.55f, s * 2, s * 1.1f }, 0.9f, 10, 2, c);
    DrawLineEx({ cx - s * 0.62f, cy }, { cx - s * 0.25f, cy }, 2, c); DrawLineEx({ cx - s * 0.435f, cy - s * 0.18f }, { cx - s * 0.435f, cy + s * 0.18f }, 2, c);
    DrawCircleV({ cx + s * 0.4f, cy - s * 0.1f }, 2.4f, c); DrawCircleV({ cx + s * 0.6f, cy + s * 0.1f }, 2.4f, c);
  } else if (kind == 1) {  // touch
    float k = s * 0.35f;
    for (int i = 0; i < 4; i++) { float sx = i % 2 ? 1 : -1, sy = i / 2 ? 1 : -1; Vector2 c0 = { cx + sx * s, cy + sy * s }; DrawLineEx(c0, { c0.x - sx * k, c0.y }, 2, c); DrawLineEx(c0, { c0.x, c0.y - sy * k }, 2, c); }
    DrawCircleV({ cx, cy - s * 0.15f }, s * 0.18f, c); DrawLineEx({ cx, cy - s * 0.15f }, { cx, cy + s * 0.5f }, 2.4f, c);
  } else if (kind == 2) {  // phone
    DrawRectangleRoundedLinesEx({ cx - s * 0.55f, cy - s, s * 1.1f, s * 2 }, 0.3f, 8, 2, c);
    DrawLineEx({ cx - s * 0.2f, cy + s * 0.75f }, { cx + s * 0.2f, cy + s * 0.75f }, 2, c);
  } else if (kind == 3) {  // wifi
    for (int i = 0; i < 3; i++) DrawRing({ cx, cy + s * 0.6f }, s * (0.35f + i * 0.33f), s * (0.35f + i * 0.33f) + 2.2f, 225, 315, 20, c);
    DrawCircleV({ cx, cy + s * 0.6f }, 2.6f, c);
  } else if (kind == 4) {  // speaker
    DrawRectangleV({ cx - s * 0.8f, cy - s * 0.25f }, { s * 0.35f, s * 0.5f }, c);
    DrawTriangle({ cx - s * 0.45f, cy - s * 0.25f }, { cx - s * 0.45f, cy + s * 0.25f }, { cx - s * 0.05f, cy + s * 0.6f }, c);
    DrawTriangle({ cx - s * 0.45f, cy - s * 0.25f }, { cx - s * 0.05f, cy + s * 0.6f }, { cx - s * 0.05f, cy - s * 0.6f }, c);
    for (int i = 0; i < 2; i++) DrawRing({ cx, cy }, s * (0.35f + i * 0.3f), s * (0.35f + i * 0.3f) + 2, -50, 50, 16, c);
  } else if (kind == 5) {  // people
    DrawCircleV({ cx - s * 0.3f, cy - s * 0.3f }, s * 0.26f, c); DrawCircleV({ cx + s * 0.3f, cy - s * 0.3f }, s * 0.26f, c);
    DrawCircleSector({ cx - s * 0.3f, cy + s * 0.45f }, s * 0.45f, 180, 360, 16, c); DrawCircleSector({ cx + s * 0.3f, cy + s * 0.45f }, s * 0.45f, 180, 360, 16, c);
  }
}
static bool Joined() { return D["players"].size() > 0; }
// once someone has joined, the big panel folds into a small "more players" card so the games get the room
void JoinCard(float a) {
  Rectangle r = { 1306, 262, 200, 338 };
  UI::Shadow(r, 26, 30, A(Hex(0x000000, 150), a), 14);
  UI::Box(r, 26, A(Hex(0x1a2647, 235), a), A(Hex(0x0d1530, 240), a), 90, A(Hex(0xd9aa5a, 140), a), 1.4f);
  float cx = r.x + r.width / 2;
  Text(cx, r.y + 30, 13, A(Hex(0xe6e9f5), a), "MORE PLAYERS?", 0, TS_PLAIN | TS_HEAVY | TS_SPACED);
  Rectangle qr = { cx - 74, r.y + 56, 148, 148 };
  UI::Box(qr, 18, A(WHITE, a));
  const J& q = D["qr"]; int n = q["n"].i(); const std::string& m = q["m"].str();
  if (n > 0 && (int)m.size() >= n * n) {
    float cell = 128.f / n, ox = qr.x + 10, oy = qr.y + 10;
    for (int y = 0; y < n; y++) for (int x = 0; x < n; x++) if (m[y * n + x] == '1') DrawRectangleRec({ ox + x * cell, oy + y * cell, cell + 0.6f, cell + 0.6f }, A(Hex(0x0a1024), a));
  }
  Text(cx, r.y + 232, 13, A(Hex(0xb9c2dc), a), "Scan or enter code", 0, TS_PLAIN);
  std::string code = D["code"].str(), spaced; for (char ch : code) { spaced += ch; spaced += ' '; }
  Text(cx, r.y + 282, 34, A(CREAM, a), spaced, 0, TS_DISPLAY | TS_PLAIN);
}
void ConnectPanel(float a) {
  if (Joined()) { JoinCard(a); return; }
  Rectangle r = { 1214, 96, 292, 596 };
  UI::Shadow(r, 30, 40, A(Hex(0x000000, 150), a), 16);
  UI::Box(r, 30, A(Hex(0x1a2647, 235), a), A(Hex(0x0d1530, 240), a), 90, A(Hex(0xd9aa5a, 150), a), 1.4f);
  float cx = r.x + r.width / 2;
  Text(cx, r.y + 36, 15, A(Hex(0xe6e9f5), a), "CONNECT CONTROLLER", 0, TS_PLAIN | TS_HEAVY | TS_SPACED);
  DrawLineEx({ r.x + 1, r.y + 64 }, { r.x + r.width - 1, r.y + 64 }, 1.2f, A(Hex(0xd9aa5a, 90), a));
  Icon(0, cx - 86, r.y + 104, 22, A(Hex(0xc9d2ea), a));
  Icon(1, cx, r.y + 104, 20, A(GOLDN, a));
  Icon(2, cx + 86, r.y + 104, 22, A(Hex(0xc9d2ea), a));
  // QR on a white card with a glowing gold rim
  Rectangle qr = { cx - 104, r.y + 146, 208, 208 };
  float glow = 0.55f + 0.45f * sinf(T * 1.6f);
  UI::Shadow(qr, 26, 26, A(Hex(0xf2c66d, (unsigned char)(110 * glow)), a), 0);
  UI::Box({ qr.x - 5, qr.y - 5, qr.width + 10, qr.height + 10 }, 28, A(Hex(0xf7d98d), a), A(Hex(0xb8873a), a), 90);
  UI::Box(qr, 24, A(WHITE, a));
  const J& q = D["qr"]; int n = q["n"].i(); const std::string& m = q["m"].str();
  if (n > 0 && (int)m.size() >= n * n) {
    float cell = 180.f / n, ox = qr.x + 14, oy = qr.y + 14;
    for (int y = 0; y < n; y++) for (int x = 0; x < n; x++) if (m[y * n + x] == '1') DrawRectangleRec({ ox + x * cell, oy + y * cell, cell + 0.6f, cell + 0.6f }, A(Hex(0x0a1024), a));
  } else Text(qr.x + 104, qr.y + 104, 18, A(Hex(0x445566), a), "…", 0, TS_PLAIN);
  Text(cx, r.y + 392, 21, A(WHITE, a), "/  SCAN TO JOIN!  /", 0, TS_PLAIN | TS_HEAVY | TS_GOLD);
  Text(cx, r.y + 424, 15, A(Hex(0xdfe4f2), a), "Use your phone as a controller", 0, TS_PLAIN);
  Text(cx, r.y + 446, 15, A(Hex(0xdfe4f2), a), "and motion sensor.", 0, TS_PLAIN);
  // room code in a little phone frame
  Rectangle ph = { r.x + 26, r.y + 476, r.width - 52, 98 };
  UI::Box(ph, 22, A(Hex(0x070d22, 230), a), A(Hex(0x070d22, 230), a), 90, A(Hex(0x3a4a78), a), 2);
  std::string code = D["code"].str(), spaced; for (char ch : code) { spaced += ch; spaced += ' '; }
  Text(cx, ph.y + 36, 36, A(CREAM, a), spaced, 0, TS_DISPLAY | TS_PLAIN);
  { std::string url = D["url"].str(); float us = 12; while (us > 8 && Measure(url, us, TS_PLAIN | TS_LIGHT) > ph.width - 24) us -= 0.5f;
    Text(cx, ph.y + 70, us, A(Hex(0x9aa6c8), a), url, 0, TS_PLAIN | TS_LIGHT); }
  DrawRectangleRounded({ cx - 30, ph.y + ph.height - 10, 60, 4 }, 1, 6, A(Hex(0x9aa6c8, 160), a));
}
void PlayerBar(float a) {
  Rectangle bar = { 100, 712, 1400, 100 };
  UI::Box(bar, 50, A(Hex(0x0c1532, 170), a), A(Hex(0x0a1129, 190), a), 90, A(Hex(0xffffff, 18), a), 1);
  UI::Box({ 132, 734, 56, 56 }, 28, A(Hex(0x1d2a52), a), A(Hex(0x141e3e), a), 90, A(Hex(0xd9aa5a, 120), a), 1.4f);
  Icon(5, 160, 763, 17, A(GOLDN, a));
  const J& ps = D["players"]; float x = 250;
  for (size_t i = 0; i < ps.size() && i < 9; i++) {
    const J& p = ps[i]; Color c = C(p["c"]); bool away = p["away"].yes(), vip = p["vip"].yes();
    float pa = away ? a * 0.4f : a, bob = sinf(T * 1.8f + i) * 1.5f, cy = 752 + bob;
    if (vip) { float g = 0.6f + 0.4f * sinf(T * 2.4f); UI::Shadow({ x - 34, cy - 34, 68, 68 }, 34, 22, A(Hex(0xf2c66d, (unsigned char)(140 * g)), pa), 0); }
    UI::Box({ x - 33, cy - 33, 66, 66 }, 33, A(vip ? Hex(0xf7d98d) : Shade(c, 0.1f), pa), A(vip ? Hex(0xb8873a) : Shade(c, -0.3f), pa), 90);
    UI::Box({ x - 29, cy - 29, 58, 58 }, 29, A(Shade(c, 0.25f), pa), A(Shade(c, -0.25f), pa), 120);
    Text(x, cy + 1, 30, A(WHITE, pa), p["cam"].yes() ? "📷" : p["av"].str(), 0, TS_PLAIN);
    if (!away) { DrawCircleV({ x + 24, cy - 24 }, 7, A(Hex(0x0a1129), a)); DrawCircleV({ x + 24, cy - 24 }, 5, A(Hex(0x8ce99a), a)); }
    if (vip) Text(x, cy - 44, 16, A(WHITE, a), "👑", 0, TS_PLAIN);
    Text(x, 800, 15, A(vip ? CREAM : Hex(0xb9c2dc), pa), p["n"].str(), 0, TS_PLAIN | (vip ? TS_HEAVY : 0));
    x += 112;
  }
  // add player
  float cy = 752;
  for (int k = 0; k < 24; k++) { float a0 = k * PI / 12 + T * 0.15f; DrawRing({ x, cy }, 31, 33, a0 * RAD2DEG, a0 * RAD2DEG + 8, 4, A(Hex(0x8090b8), a)); }
  DrawLineEx({ x - 9, cy }, { x + 9, cy }, 2.6f, A(Hex(0xc9d2ea), a)); DrawLineEx({ x, cy - 9 }, { x, cy + 9 }, 2.6f, A(Hex(0xc9d2ea), a));
  Text(x, 800, 15, A(Hex(0x8f9bbd), a), ps.size() ? "Add Player" : "Scan to join", 0, TS_PLAIN);
  // status icons + clock
  bool music = D["music"].yes();
  Icon(4, 1318, 762, 15, A(music ? Hex(0xdfe4f2) : Hex(0x6b7699), a));
  Icon(3, 1370, 760, 15, A(Hex(0xdfe4f2), a));
  Text(1462, 762, 22, A(Hex(0xeef1fa), a), D["clock"].str(), 0, TS_PLAIN | TS_HEAVY);
}
void Cards(float a, float dt) {
  const J& rows = D["rows"]; int fr = D["f"][0].i(), fc = D["f"][1].i();
  if (fr < 0 || fr >= (int)rows.size()) return;
  // ---- category tabs ----
  float tx = 120, ty = 262; float fx = 0, fw = 0;
  for (size_t r = 0; r < rows.size(); r++) {
    std::string s = rows[r]["short"].str(); float w = Measure(s, 17, TS_PLAIN | TS_HEAVY);
    bool on = (int)r == fr;
    Text(tx, ty, 17, on ? A(CREAM, a) : A(Hex(0x8592b5), a), s, -1, TS_PLAIN | TS_HEAVY | (on ? TS_GOLD : 0));
    hits.push_back({ { tx - 10, ty - 20, w + 20, 40 }, 500 + (int)r });
    if (on) { fx = tx; fw = w; }
    tx += w + 34;
  }
  if (tabX.x == 0) { tabX.x = fx; tabW.x = fw; }
  tabX.To(fx, dt, 170, 26); tabW.To(fw, dt, 170, 26);
  DrawRectangleRounded({ tabX.x, ty + 18, tabW.x, 3 }, 1, 6, A(GOLDN, a));
  BeginBlendMode(BLEND_ADDITIVE); DrawRectangleRounded({ tabX.x - 4, ty + 15, tabW.x + 8, 9 }, 1, 6, A(Hex(0xf2c66d, 50), a)); EndBlendMode();
  // ---- cards carousel ----
  if (fr != lastRow) { rowIn.x = 0; rowIn.v = 0; lastRow = fr; carousel.x = fmaxf(0, (fc - 1)) * 292; carousel.v = 0; }
  rowIn.To(1, dt, 90, 19);
  const J& items = rows[fr]["items"];
  const float cw = 262, ch = 360, gap = 30, x0 = 120, top = 304;
  float want = fmaxf(0, fminf((float)fc - 1, (float)items.size() - 3.6f)) * (cw + gap);
  carousel.To(want, dt, 150, 24.5f);
  float clipW = Joined() ? 1230 : 1140; BeginScissorL(60, 230, clipW, 470); TextClip(60, 230, clipW, 470);
  for (size_t c = 0; c < items.size(); c++) {
    const J& it = items[c]; bool foc = (int)c == fc;
    std::string key = std::to_string(fr) + ":" + it["id"].str();
    Spring& s = tileS[key]; if (s.x == 0) s.x = 1;
    s.To(foc ? 1.065f : 1, dt, 230, 26);
    float stag = Clamp(rowIn.x * 1.25f - c * 0.07f, 0, 1), ia = a * stag;
    float x = x0 + c * (cw + gap) - carousel.x + (1 - stag) * 60;
    if (x > (Joined() ? 1300 : 1220) || x + cw < 40 || ia < 0.01f) continue;
    float w = cw * s.x, h = ch * s.x, lift = (s.x - 1) / 0.065f * 8;
    Rectangle r = { x + cw / 2 - w / 2, top + ch / 2 - h / 2 - lift, w, h };
    float fk = Clamp((s.x - 1) / 0.065f, 0, 1);
    // glow + shadow (GL layer)
    UI::Shadow(r, 26, 30, A(Hex(0x000000, 160), ia), 14);
    if (fk > 0.02f) UI::Shadow(r, 26, 46, A(Hex(0xf2c66d, (unsigned char)(150 * fk)), ia), 0);
    Color a1 = C(it["a1"]), a2 = C(it["a2"]);
    UI::Box(r, 24, A(a1, ia), A(a2, ia), 120);
    Text(r.x + r.width / 2, r.y + r.height * 0.4f, 90 * s.x, A(WHITE, ia), it["e"].str(), 0, TS_PLAIN | TS_SHADOW);
    // overlay: cover art, legibility gradient, gold rim, title
    float pan = foc ? sinf(T * 0.22f) * 0.55f : 0;
    Img(it["id"].str(), r.x, r.y, r.width, r.height, 24, ia, pan, foc ? 1.04f + 0.02f * sinf(T * 0.3f) : 1.0f);
    RRect(r.x, r.y + r.height * 0.42f, r.width, r.height * 0.58f, 24, Hex(0x081230, 0), Hex(0x081230, (unsigned char)(240 * ia)), BLANK, 0, true);
    RRect(r.x, r.y, r.width, r.height, 24, BLANK, BLANK, A(Hex(0xf2c66d, foc ? 255 : 120), ia), foc ? 3.2f : 1.4f, false, true);
    std::string name = Upper(it["n"].str());
    float fs = 30 * s.x; while (fs > 17 && Measure(name, fs, TS_DISPLAY) > r.width - 30) fs -= 1;
    bool two = Measure(name, fs, TS_DISPLAY) > r.width - 30;
    Text(r.x + r.width / 2, r.y + r.height - (two ? 84 : 62) * s.x, fs, A(WHITE, ia), name, 0, TS_DISPLAY | TS_PLAIN | TS_GOLD | TS_SHADOW, two ? r.width - 26 : 0);
    std::string sub = it["k"].str() + "  •  " + it["pl"].str() + " PLAYERS";
    Text(r.x + r.width / 2, r.y + r.height - 28 * s.x, 12.5f * s.x, A(Hex(0xd5dbeb), ia), sub, 0, TS_PLAIN | TS_HEAVY | TS_SPACED);
    if (it["rec"].yes()) { float bw = Measure("FOR YOU", 11, TS_PLAIN | TS_HEAVY) + 26; RRect(r.x + r.width - bw - 14, r.y + 14, bw, 26, 13, Hex(0xf7d98d), Hex(0xc9963f), BLANK, 0, true); Text(r.x + r.width - 14 - bw / 2, r.y + 27, 11, A(Hex(0x3a2508), ia), "FOR YOU", 0, TS_PLAIN | TS_HEAVY | TS_SPACED); }
    hits.push_back({ r, 1000 + fr * 100 + (int)c });
  }
  EndScissorMode(); TextUnclip();
  // edge fades hint that the row scrolls
  if (carousel.x > 4) DrawRectangleGradientH(60, 280, 70, 420, A(Hex(0x0a1838, 230), a), Hex(0x0a1838, 0));
  if (items.size() > 4 && carousel.x < (items.size() - 3.6f) * (cw + gap) - 4) DrawRectangleGradientH(60 + clipW - 70, 280, 70, 420, Hex(0x0a1838, 0), A(Hex(0x0a1838, 230), a));
  // ---- focused game: one line of description ----
  if (fc >= 0 && fc < (int)items.size()) {
    const J& it = items[fc]; std::string id = it["id"].str();
    if (id != lastDescId) { lastDescId = id; descIn.x = 0; descIn.v = 0; }
    descIn.To(1, dt, 120, 22); float k = Clamp(descIn.x, 0, 1);
    std::string line = it["d"].str();
    Text(120 + (1 - k) * 16, 682, 18, A(Hex(0xc3cbe3), a * k), line, -1, TS_PLAIN, 1060);
  }
}
void DrawHome(float a, float dt) {
  NightSky(a);
  Logo(120, 112, 1, a);
  Text(1180, 112, 14, A(Hex(0x8f9bbd), a), "← → choose   ↑ ↓ category   OK play", 1, TS_PLAIN | TS_SPACED);
  Cards(a, dt);
  ConnectPanel(a);
  PlayerBar(a);
}
void DrawBootGold(float a) {
  NightSky(a);
  float bob = sinf(T * 1.4f) * 5;
  Logo(800, 300 + bob, 1.9f, a, true);
  float p = 0.6f + 0.4f * sinf(T * 2.6f);
  Rectangle b = { 640, 620, 320, 64 };
  UI::Shadow(b, 32, 30, A(Hex(0xf2c66d, (unsigned char)(120 * p)), a), 0);
  UI::Box(b, 32, A(Hex(0xf7d98d), a), A(Hex(0xc9963f), a));
  Text(800, 653, 22, A(Hex(0x2a1a05), a), "▶  Press any key", 0, TS_PLAIN | TS_HEAVY);
  Text(800, 730, 16, A(Hex(0x9aa6c8), a), "or click  ·  F = fullscreen  ·  phones join on the next screen", 0, TS_PLAIN | TS_LIGHT);
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
  BeginScissorL(art.x, art.y, art.width, art.height);
  Rays({ art.x + 350, art.y + 380 }, 900, T * 0.3f, A(Hex(0xffffff, 30), a), 18);
  for (int i = 0; i < 26; i++) { float px = art.x + fmodf(i * 137.f + T * (20 + i % 5 * 8), art.width), py = art.y + art.height - fmodf(i * 53.f + T * (40 + i % 7 * 9), art.height); DrawCircle((int)px, (int)py, 2 + i % 4, A(Hex(0xffffff, 90), a)); }
  EndScissorMode();
  float pop = EaseBack(Clamp((t - 0.15f) * 2.2f, 0, 1));
  Text(art.x + 350, art.y + 370 + sinf(T * 2) * 14, 260 * pop + 1, A(WHITE, a), in["e"].str(), 0, TS_PLAIN | TS_SHADOW);
  Img(in["id"].str(), art.x, art.y, art.width, art.height, 40, a * EaseOut(t * 2.5f), sinf(T * 0.18f) * 0.8f, 1.06f + 0.04f * sinf(T * 0.25f));
  RRect(art.x, art.y + art.height * 0.55f, art.width, art.height * 0.45f, 40, Hex(0x081230, 0), Hex(0x081230, (unsigned char)(235 * a)), BLANK, 0, true);
  RRect(art.x, art.y, art.width, art.height, 40, BLANK, BLANK, A(Hex(0xf2c66d), a), 3, false, true);
  Text(art.x + 350, art.y + 690, 54, A(WHITE, a), Upper(in["n"].str()), 0, TS_DISPLAY | TS_PLAIN | TS_GOLD | TS_SHADOW, 640);
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
  lit.ShadowPass();
  PostBegin();
  rlEnableDepthMask(); ClearBackground(Hex(0x070b24));
  if (shown == "results") DrawResults(a, dt);
  else {
    if (shown == "boot") DrawBootGold(a);
    else if (shown == "home") DrawHome(a, dt);
    else if (shown == "intro") { NightSky(1); DrawIntro(a, dt); }
  }
  PostEnd();
  lit.EndFrame(dt);
}
}
