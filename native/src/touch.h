// Touch maps: the phone shows the play area live (sent ~20x a second) and your finger moves you.
#pragma once
#include "cc.h"
#include <string>
#include <vector>

// ---------- touch maps: the phone shows the play area live and your finger moves you ----------
// phone space: x 0..1 across, y 0..1 down; r is a fraction of the map width.
// k: 0 ball/puck, 1 me, 2 my team, 3 other player, 4 good pickup (coin), 5 danger circle, 6 danger tile (r = half width, r2 = half height), 7 broken tile
struct TouchObj { float x, y, r; Color c; int k; float r2 = 0; };
inline std::string TouchLayout(const std::string& theme, float aspect, const std::string& mode, const std::string& btn, const std::string& hint, int team = -1, const std::string& btn2 = "", const std::string& extra = "") {
  char a[32]; snprintf(a, sizeof a, "%.4f", aspect);
  std::string q = [](const std::string& s) { std::string r = "\""; for (char c : s) { if (c == '"' || c == '\\') { r += '\\'; r += c; } else r += c; } return r + "\""; }(hint);
  return std::string("{\"name\":\"touch\",\"theme\":\"") + theme + "\",\"aspect\":" + a + ",\"mode\":\"" + mode + "\",\"btn\":\"" + btn + "\",\"hint\":" + q + ",\"team\":" + std::to_string(team) + (btn2.empty() ? "" : ",\"btn2\":\"" + btn2 + "\"") + extra + "}";
}
inline void SendSnap(int pid, const std::vector<TouchObj>& o, const std::string& info = "") {
  std::string s = "{\"t\":\"snap\",\"o\":["; char b[96];
  for (size_t i = 0; i < o.size(); i++) {
    snprintf(b, sizeof b, "%s[%.3f,%.3f,%.3f,\"#%02x%02x%02x\",%d,%.3f]", i ? "," : "", o[i].x, o[i].y, o[i].r, o[i].c.r, o[i].c.g, o[i].c.b, o[i].k, o[i].r2);
    s += b;
  }
  s += "]";
  if (!info.empty()) { s += ",\"i\":\""; for (char c : info) if (c != '"' && c != '\\') s += c; s += "\""; }
  Send(pid, s + "}");
}
// where the finger is, in phone space (valid only while P.touch)
inline Vector2 TouchAt(const Player& p) { return { Clamp((p.stick.x + 1) / 2, 0, 1), Clamp((p.stick.y + 1) / 2, 0, 1) }; }
// a rectangle of the game world shown on the phone
struct TouchBox {
  float x0, y0, x1, y1;
  Vector2 P(float x, float y) const { return { (x - x0) / (x1 - x0), (y - y0) / (y1 - y0) }; }
  float R(float r) const { return r / (x1 - x0); }
  Vector2 W(Vector2 s) const { return { x0 + s.x * (x1 - x0), y0 + s.y * (y1 - y0) }; }
  float Aspect() const { return (x1 - x0) / (y1 - y0); }
};
// joystick-style input that runs toward a point and slows down on arrival
inline Vector2 GoToward(float x, float y, float tx, float ty, float slow = 50) {
  float dx = tx - x, dy = ty - y, d = hypotf(dx, dy); if (d < 8) return { 0, 0 };
  float k = fminf(1, d / slow); return { dx / d * k, dy / d * k };
}
struct SnapTimer { float t = 0; bool Tick(float dt) { t -= dt; if (t > 0) return false; t = 0.05f; return true; } };
