// Small shared helpers for the newer game files: phone layouts (JSON), shuffling, number formatting,
// and the glossy 2D UI pieces (answer tiles, panels, timer) drawn on the website's overlay layer.
#pragma once
#include "cc.h"
#include <functional>
#include <string>
#include <vector>

namespace kit {
inline const char* OPT[] = { "#ff5c7a", "#3d8bff", "#ffc93c", "#34c77b", "#b86bff", "#ff8f3c", "#22c3d6", "#f06595", "#94d82d", "#845ef7" };
inline std::string Q(const std::string& s) { std::string r = "\""; for (char c : s) { if (c == '"' || c == '\\') { r += '\\'; r += c; } else if (c == '\n') r += "\\n"; else r += c; } return r + "\""; }
inline std::string B(bool b) { return b ? "true" : "false"; }
inline std::string Hx(Color c) { char b[8]; snprintf(b, sizeof b, "#%02x%02x%02x", c.r, c.g, c.b); return b; }
template <class T> void Shuffle(std::vector<T>& v) { for (int i = (int)v.size() - 1; i > 0; i--) std::swap(v[i], v[rand() % (i + 1)]); }
template <class T> const T& Pick(const std::vector<T>& v) { return v[rand() % v.size()]; }
inline int irand(int a, int b) { return a + rand() % (b - a + 1); }
inline std::string I(int v) { return std::to_string(v); }

inline std::string Choice(const std::string& prompt, const std::vector<std::string>& labels, int cols, bool big = false, const std::vector<std::string>& colors = {}, const std::vector<bool>& off = {}, int multi = 0, bool secret = false) {
  std::string o;
  for (size_t i = 0; i < labels.size(); i++) o += std::string(i ? "," : "") + "{\"label\":" + Q(labels[i]) + ",\"color\":" + Q(i < colors.size() ? colors[i] : OPT[i % 10]) + ",\"disabled\":" + B(i < off.size() && off[i]) + "}";
  return "{\"name\":\"choice\",\"prompt\":" + Q(prompt) + ",\"cols\":" + I(cols) + ",\"big\":" + B(big) + (multi ? ",\"multi\":" + I(multi) : "") + (secret ? ",\"secret\":true" : "") + ",\"options\":[" + o + "]}";
}
inline std::string Msg(const std::string& emoji, const std::string& text, const std::string& sub = "") { return "{\"name\":\"msg\",\"emoji\":" + Q(emoji) + ",\"text\":" + Q(text) + ",\"sub\":" + Q(sub) + "}"; }
inline std::string Pad(const std::string& stick, const std::string& a, const std::string& b, const std::string& hint) {
  return "{\"name\":\"pad\"" + (stick.empty() ? std::string(",\"stick\":null") : ",\"stick\":" + Q(stick)) + (a.empty() ? "" : ",\"a\":" + Q(a)) + (b.empty() ? "" : ",\"b\":" + Q(b)) + ",\"hint\":" + Q(hint) + "}";
}

// ---------- console-style overlay UI (matches the home screen: navy glass + gold) ----------
inline const Color NAVY = Hex(0x0c1638), GOLDC = Hex(0xf2c66d);
inline void Glass(float x, float y, float w, float h, float r = 26, unsigned char a = 215, bool gold = true) {
  RRect(x, y + 6, w, h, r, Hex(0x000000, 70), Hex(0x000000, 0), BLANK, 0, true);
  Color t = hud::TH().top, b = hud::TH().bot; t.a = b.a = a;
  RRect(x, y, w, h, r, t, b, gold ? hud::TH().rim : Hex(0xffffff, 40), gold ? 3 : 1.5f, true, gold && hud::TH().gold);
}
// a glossy candy tile like the phone buttons: lip underneath, gradient top, gloss band
inline void Tile(float x, float y, float w, float h, Color c, float alpha = 1, float press = 0, bool goldRim = false) {
  unsigned char A = (unsigned char)(255 * alpha);
  float d = 8 * (1 - press), R = fminf(30, h * 0.3f);
  // a chunky moulded 3D block: floor shadow, deep dark side wall, bevelled face with a lit top edge
  RRect(x + 4, y + 30, w - 8, h, R, Hex(0x000000, (unsigned char)(95 * alpha)), Hex(0x000000, 0), BLANK, 0, true);
  Color wall = Shade(c, -0.58f), wall2 = Shade(c, -0.42f); wall.a = wall2.a = A;
  RRect(x, y + 20, w, h, R, wall2, wall, BLANK, 0, true);
  Color top = Shade(c, 0.2f), bot = Shade(c, -0.16f); top.a = bot.a = A;
  float ty = y + 8 - d;
  RRect(x, ty, w, h, R, top, bot, BLANK, 0, true);
  // bevel: bright rim on top, soft inner face, dark seat at the bottom
  RRect(x + 3, ty + 3, w - 6, h - 6, R - 3, BLANK, BLANK, Hex(0xffffff, (unsigned char)(95 * alpha)), 3);
  RRect(x + 9, ty + 9, w - 18, h - 18, R - 8, Hex(0xffffff, (unsigned char)(34 * alpha)), Hex(0xffffff, 0), BLANK, 0, true);
  RRect(x + 14, ty + h - 16, w - 28, 8, 4, Hex(0x000000, (unsigned char)(38 * alpha)), Hex(0x000000, (unsigned char)(38 * alpha)), BLANK, 0);
  if (goldRim) RRect(x - 3, ty - 3, w + 6, h + 6, R + 3, BLANK, BLANK, Hex(0xf2b84b, A), 7, false, true);
}
inline float TileTop(float y, float press = 0) { return y + 8 - 8 * (1 - press); }
// a gold title with a small spaced kicker above it
inline void Heading(const std::string& kicker, const std::string& title, float y = 60, float size = 46) {
  if (!kicker.empty()) Text(800, y - size * 0.75f, 17, Hex(0xc9d3f2), kicker, 0, TS_PLAIN | TS_HEAVY | TS_SPACED);
  Text(800, y, size, WHITE, title, 0, TS_DISPLAY | TS_SHADOW | TS_GOLD);
}
// circular countdown in the top-right corner
inline void Timer(float t, float tMax, float x = 1500, float y = 70) {
  float f = Clamp(t / fmaxf(0.01f, tMax), 0, 1);
  x -= 46; y -= 9;   // sits in the header band, clear of the question panel
  DrawCircle((int)x, (int)y + 4, 50, Hex(0x000000, 70));
  DrawCircle((int)x, (int)y, 50, Hex(0x0a1233, 245));
  DrawRing({ x, y }, 38, 50, -90, 270, 64, Hex(0x24306b));
  DrawRing({ x, y }, 38, 50, -90, -90 + 360 * f, 64, t < 4 ? Hex(0xff4d6d) : GOLDC);
  Text(x, y + 3, 46, t < 4 ? Hex(0xff6b81) : WHITE, I((int)ceilf(fmaxf(0, t))), 0, TS_DISPLAY | TS_SHADOW);
}
inline void HintBar(const std::string& s, float y = 860, Color c = WHITE) {
  hud::Hint(s, c, y - 14);
}
}  // namespace kit
