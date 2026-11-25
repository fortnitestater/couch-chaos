// Shared art: SDF UI cards, toon characters, winter backdrop, confetti.
#include "cc.h"
#include "rlgl.h"

// ---------- UI: anti-aliased rounded cards with gradient, border and soft shadow (one SDF shader) ----------
static const char* UI_VS = R"(#version 100
attribute vec3 vertexPosition; attribute vec2 vertexTexCoord; attribute vec4 vertexColor;
uniform mat4 mvp; varying vec2 fragTexCoord; varying vec4 fragColor;
void main() { fragTexCoord = vertexTexCoord; fragColor = vertexColor; gl_Position = mvp * vec4(vertexPosition, 1.0); })";
static const char* UI_FS = R"(#version 100
precision mediump float;
varying vec2 fragTexCoord; varying vec4 fragColor;
uniform vec4 rect; uniform float radius; uniform vec4 c1; uniform vec4 c2; uniform float angle; uniform float shadow; uniform vec4 border; uniform float bw;
float sd(vec2 p, vec2 b, float r) { vec2 q = abs(p) - b + r; return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r; }
void main() {
  vec2 c = rect.xy + rect.zw * 0.5; vec2 p = gl_FragCoord.xy - c;
  float d = sd(p, rect.zw * 0.5, radius);
  if (shadow > 0.0) { float a = 1.0 - smoothstep(-shadow * 0.5, shadow, d); gl_FragColor = vec4(c1.rgb, a * a * c1.a); return; }
  vec2 dir = vec2(cos(angle), sin(angle));
  float t = clamp(dot(p / max(rect.zw, vec2(1.0)), dir) + 0.5, 0.0, 1.0);
  vec4 col = mix(c1, c2, t);
  if (bw > 0.0) { float e = smoothstep(-bw - 0.8, -bw + 0.8, d); col = mix(col, vec4(border.rgb, max(col.a, border.a)), e * border.a); }
  float aa = 1.0 - smoothstep(-0.8, 0.8, d);
  gl_FragColor = vec4(col.rgb, col.a * aa);
})";
static Shader uiS; static int uRect, uRad, uC1, uC2, uAng, uSh, uBor, uBw;
namespace UI {
void Init() {
  uiS = LoadShaderFromMemory(UI_VS, UI_FS);
  uRect = GetShaderLocation(uiS, "rect"); uRad = GetShaderLocation(uiS, "radius"); uC1 = GetShaderLocation(uiS, "c1"); uC2 = GetShaderLocation(uiS, "c2");
  uAng = GetShaderLocation(uiS, "angle"); uSh = GetShaderLocation(uiS, "shadow"); uBor = GetShaderLocation(uiS, "border"); uBw = GetShaderLocation(uiS, "bw");
}
static Vector4 V4(Color c) { return { c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f }; }
static void Go(Rectangle r, Rectangle quad, float rad, Color a, Color b, float ang, float sh, Color bor, float bw) {
  // framebuffer y grows upward; the canvas is exactly W x H
  // gl_FragCoord is in real framebuffer pixels (W*g_rs wide), so scale the box into that space
  const float k = g_rs;
  Vector4 R = { r.x * k, ((float)H - r.y - r.height) * k, r.width * k, r.height * k };
  Vector4 A = V4(a), B = V4(b), Bo = V4(bor); float rr = fminf(rad, fminf(r.width, r.height) * 0.5f) * k; sh *= k; bw *= k;
  BeginShaderMode(uiS);
  SetShaderValue(uiS, uRect, &R, SHADER_UNIFORM_VEC4); SetShaderValue(uiS, uRad, &rr, SHADER_UNIFORM_FLOAT);
  SetShaderValue(uiS, uC1, &A, SHADER_UNIFORM_VEC4); SetShaderValue(uiS, uC2, &B, SHADER_UNIFORM_VEC4);
  SetShaderValue(uiS, uAng, &ang, SHADER_UNIFORM_FLOAT); SetShaderValue(uiS, uSh, &sh, SHADER_UNIFORM_FLOAT);
  SetShaderValue(uiS, uBor, &Bo, SHADER_UNIFORM_VEC4); SetShaderValue(uiS, uBw, &bw, SHADER_UNIFORM_FLOAT);
  DrawRectangleRec(quad, WHITE);
  EndShaderMode();
}
void Box(Rectangle r, float rad, Color c1, Color c2, float deg, Color border, float bw) {
  if (r.width <= 0 || r.height <= 0) return;
  // angle measured on screen (y down) -> flip for framebuffer space
  Go(r, { r.x - 1, r.y - 1, r.width + 2, r.height + 2 }, rad, c1, c2, -deg * DEG2RAD, 0, border, bw);
}
void Shadow(Rectangle r, float rad, float blur, Color c, float dy) {
  Rectangle s = { r.x, r.y + dy, r.width, r.height };
  Go(s, { s.x - blur * 1.2f, s.y - blur * 1.2f, s.width + blur * 2.4f, s.height + blur * 2.4f }, rad, c, c, 0, fmaxf(1, blur), BLANK, 0);
}
void Sky(Color top, Color bottom) { DrawRectangleGradientV(0, 0, W, H, top, bottom); }
}

ToonHold g_hold; float g_toonK = 1.4f;
Vector3 ToonPoint(Vector3 pos, float yaw, float sc, float hop, Vector3 local) {
  sc *= g_toonK;
  Matrix base = MatrixMultiply(MatrixMultiply(MatrixScale(sc, sc, sc), MatrixRotateY(yaw)), MatrixTranslate(pos.x, pos.y + hop, pos.z));
  return Vector3Transform(local, base);
}
// ---------- toon character: a kid in a hooded onesie, big face, walking legs, a hat per player ----------
// ---------- a realistic person: real proportions (1.78 m at scale 1), jointed limbs, walk/run cycle, face, hair, clothes ----------
// the shirt is the player's colour; hair, skin and shorts vary per player so everyone is recognisable
static Vector3 Ik(Vector3 sh, Vector3 target, float l1, float l2, Vector3 pole) {  // elbow position for a two-bone arm
  Vector3 d = Vector3Subtract(target, sh); float L = fminf(Vector3Length(d), l1 + l2 - 0.001f); if (L < 1e-4f) return Vector3Add(sh, { 0, -l1, 0 });
  Vector3 u = Vector3Normalize(d); float a = (l1 * l1 + L * L - l2 * l2) / (2 * l1 * L); a = Clamp(a, -1, 1);
  Vector3 pp = Vector3Subtract(pole, Vector3Scale(u, Vector3DotProduct(pole, u))); if (Vector3Length(pp) < 1e-4f) pp = { 0, -1, 0 };
  pp = Vector3Normalize(pp); return Vector3Add(sh, Vector3Add(Vector3Scale(u, l1 * a), Vector3Scale(pp, l1 * sqrtf(1 - a * a))));
}
void DrawToon(Vector3 pos, float yaw, Color c, float t, float hop, float cheer, float sc) {
  sc *= g_toonK;
  bool moving = hop > 0.02f, flash = c.r == 255 && c.g == 255 && c.b == 255;
  unsigned hsh = (unsigned)c.r * 7 + (unsigned)c.g * 13 + (unsigned)c.b * 29;
  const Color SKINS[] = { Hex(0xf2c9a8), Hex(0xe0ac82), Hex(0xc68a5e), Hex(0x8d5a3b), Hex(0xf6d5b8) };
  const Color HAIRS[] = { Hex(0x2b1d14), Hex(0x5a3a1e), Hex(0xc9a063), Hex(0x1a1a1a), Hex(0x8a3b1c), Hex(0x6b6b6b) };
  Color skin = flash ? WHITE : SKINS[hsh % 5], hair = flash ? WHITE : HAIRS[(hsh / 5) % 6];
  Color shirt = c, shirtD = Shade(c, -0.25f), shorts = flash ? WHITE : ((hsh / 3) % 2 ? Hex(0x1f2a44) : Hex(0x2d2d33)), shoe = flash ? WHITE : Hex(0xf4f4f2), sole = Hex(0x3a3a40);
  int style = (int)((hsh / 11) % 4);  // hair style
  float ph = t * 9.5f, s1 = moving ? sinf(ph) : 0.f, run = moving ? fminf(1, hop * 4) : 0;
  float bob = moving ? fabsf(cosf(ph)) * 0.04f : sinf(t * 2.2f) * 0.006f;
  lit.Caster(pos, 0.38f * sc);
  float lean = moving ? 0.1f * run : 0.f;
  Matrix base = MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixScale(sc, sc, sc), MatrixRotateX(lean)), MatrixRotateY(yaw)), MatrixTranslate(pos.x, pos.y + hop * 0.35f + bob * sc, pos.z));
  auto X = [&](Vector3 p) { return Vector3Transform(p, base); };
  auto E = [&](Vector3 p, Vector3 r, Vector3 s, Color col) { lit.Draw(prim.sphere, MatrixMultiply(TRS(p, r, s), base), col); };
  auto Lb = [&](Vector3 a, Vector3 b, float r, Color col) { Limb(X(a), X(b), r * sc, col); };
  // ---- legs: hip -> knee -> ankle -> foot, with a real stride ----
  for (int s = -1; s <= 1; s += 2) {
    float sw = s1 * s, th = sw * (0.35f + 0.25f * run), bend = moving ? fmaxf(0, -sw) * (0.6f + 0.6f * run) + 0.1f : 0.05f;
    Vector3 hip = { s * 0.095f, 0.93f, 0 };
    Vector3 knee = Vector3Add(hip, { 0, -0.44f * cosf(th), 0.44f * sinf(th) });
    Vector3 ank = Vector3Add(knee, { 0, -0.42f * cosf(th - bend), 0.42f * sinf(th - bend) });
    Lb(hip, Vector3Lerp(hip, knee, 0.55f), 0.088f, shorts);   // thigh in shorts
    Lb(Vector3Lerp(hip, knee, 0.45f), knee, 0.07f, skin);
    Lb(knee, ank, 0.058f, skin);                                // calf
    Lb(Vector3Lerp(knee, ank, 0.7f), ank, 0.047f, flash ? WHITE : Hex(0xf1f1f1));  // sock
    Vector3 toe = Vector3Add(ank, { 0, -0.045f, 0.12f });
    E({ (ank.x + toe.x) / 2, ank.y - 0.04f, (ank.z + toe.z) / 2 + 0.02f }, { 0, 0, 0 }, { 0.11f, 0.08f, 0.28f }, shoe);
    E({ (ank.x + toe.x) / 2, ank.y - 0.075f, (ank.z + toe.z) / 2 + 0.02f }, { 0, 0, 0 }, { 0.115f, 0.03f, 0.29f }, sole);
  }
  // ---- torso: pelvis, waist, chest (shirt), shoulders, collar ----
  E({ 0, 0.97f, 0 }, { 0, 0, 0 }, { 0.34f, 0.2f, 0.22f }, shorts);
  // one smooth tapered trunk (waist narrower than chest) instead of stacked balls
  lit.Draw(prim.cone, MatrixMultiply(TRS({ 0, 1.42f, 0.005f }, { PI, 0, 0 }, { 0.44f, 0.46f, 0.25f }), base), shirt);
  lit.Draw(prim.cyl, MatrixMultiply(TRS({ 0, 0.98f, 0.005f }, { 0, 0, 0 }, { 0.32f, 0.2f, 0.21f }), base), shirt);
  E({ 0, 1.4f, 0.005f }, { 0, 0, 0 }, { 0.44f, 0.12f, 0.25f }, shirt);   // shoulders
  E({ 0, 1.28f, 0.03f }, { 0, 0, 0 }, { 0.38f, 0.24f, 0.21f }, shirt);   // chest
  E({ 0, 1.465f, 0.015f }, { 0, 0, 0 }, { 0.13f, 0.04f, 0.11f }, shirtD);
  E({ 0, 1.3f, -0.115f }, { 0, 0, 0 }, { 0.14f, 0.16f, 0.02f }, flash ? WHITE : Hex(0xf8f9fa));  // number patch on the back
  // ---- arms: shoulder -> elbow -> wrist -> hand; swing while running, raise when cheering, grip when holding ----
  for (int s = -1; s <= 1; s += 2) {
    Vector3 sh = { s * 0.205f, 1.41f, 0 }, wrist;
    float up = cheer * (0.9f + 0.1f * sinf(t * 10 + s));
    if (g_hold.on && s == -1) wrist = g_hold.hand;
    else if (up > 0.05f) wrist = Vector3Lerp({ s * 0.3f, 0.95f, 0.05f }, { s * 0.32f, 1.98f + 0.04f * sinf(t * 12 + s), 0.08f }, fminf(1, up));
    else { float a = -s1 * s * (0.45f + 0.35f * run); wrist = { s * 0.25f, 1.41f - 0.5f * cosf(a) + (moving ? 0.1f * run : 0), 0.5f * sinf(a) + (moving ? 0.12f * run : 0.04f) }; }
    Vector3 el = Ik(sh, wrist, 0.29f, 0.26f, { s * 0.4f, -0.4f, -0.6f });
    Vector3 elDir = Vector3Normalize(Vector3Subtract(el, sh));
    Lb(sh, Vector3Add(sh, Vector3Scale(elDir, 0.14f)), 0.062f, shirt);    // sleeve
    Lb(Vector3Add(sh, Vector3Scale(elDir, 0.12f)), el, 0.05f, skin);      // upper arm
    Vector3 wd = Vector3Normalize(Vector3Subtract(wrist, el)); Vector3 wr = Vector3Add(el, Vector3Scale(wd, 0.26f));
    Lb(el, wr, 0.043f, skin);                                               // forearm
    Vector3 hc = Vector3Add(wr, Vector3Scale(wd, 0.06f));
    E(hc, { 0, 0, 0 }, { 0.075f, 0.1f, 0.05f }, skin);                       // hand
    E(Vector3Add(hc, { s * -0.03f, 0.02f, 0.03f }), { 0, 0, 0 }, { 0.03f, 0.05f, 0.03f }, skin);  // thumb
  }
  // ---- neck + head ----
  Lb({ 0, 1.44f, 0 }, { 0, 1.56f, 0.01f }, 0.05f, skin);
  float hy = 1.67f;
  E({ 0, hy, 0.005f }, { 0, 0, 0 }, { 0.19f, 0.235f, 0.215f }, skin);
  E({ 0, hy - 0.075f, 0.035f }, { 0, 0, 0 }, { 0.15f, 0.12f, 0.15f }, skin);   // jaw
  for (int s = -1; s <= 1; s += 2) {
    E({ s * 0.097f, hy - 0.005f, -0.005f }, { 0, 0, 0 }, { 0.03f, 0.055f, 0.04f }, Shade(skin, -0.06f));  // ears
    bool blink = fmodf(t + (float)(hsh % 7), 4.2f) < 0.12f;
    E({ s * 0.04f, hy + 0.012f, 0.098f }, { 0, 0, 0 }, { 0.032f, blink ? 0.004f : 0.02f, 0.012f }, flash ? WHITE : Hex(0xfdfdfd));
    if (!blink) E({ s * 0.04f, hy + 0.012f, 0.104f }, { 0, 0, 0 }, { 0.016f, 0.016f, 0.008f }, flash ? WHITE : Hex(0x3b2a1e));
    E({ s * 0.042f, hy + 0.045f, 0.098f }, { 0, 0, s * 0.12f }, { 0.04f, 0.009f, 0.012f }, hair);  // brows
  }
  E({ 0, hy - 0.022f, 0.108f }, { 0.3f, 0, 0 }, { 0.026f, 0.045f, 0.03f }, Shade(skin, -0.04f));    // nose
  E({ 0, hy - 0.07f, 0.095f }, { 0, 0, 0 }, { 0.05f, cheer > 0.3f ? 0.022f : 0.01f, 0.012f }, flash ? WHITE : Hex(cheer > 0.3f ? 0x7a2a2a : 0xb5655a));  // mouth
  // hair: short crop, ponytail, curly top, or a cap in the shirt colour
  if (style == 3) {
    E({ 0, hy + 0.085f, -0.005f }, { 0, 0, 0 }, { 0.205f, 0.11f, 0.225f }, shirtD);
    E({ 0, hy + 0.06f, 0.1f }, { 0.25f, 0, 0 }, { 0.17f, 0.02f, 0.12f }, shirtD);
  } else {
    E({ 0, hy + 0.05f, -0.02f }, { 0, 0, 0 }, { 0.205f, 0.17f, 0.215f }, hair);
    E({ 0, hy - 0.01f, -0.06f }, { 0, 0, 0 }, { 0.2f, 0.18f, 0.15f }, hair);
    if (style == 1) { E({ 0, hy - 0.02f, -0.13f }, { 0, 0, 0 }, { 0.07f, 0.07f, 0.07f }, hair); Lb({ 0, hy - 0.04f, -0.14f }, { 0, hy - 0.2f, -0.15f }, 0.03f, hair); }
    if (style == 2) for (int k = 0; k < 5; k++) E({ -0.07f + k * 0.035f, hy + 0.11f, 0.02f - (k % 2) * 0.03f }, { 0, 0, 0 }, { 0.07f, 0.06f, 0.07f }, hair);
  }
}

// ---------- winter backdrop ----------
void WinterBG::Init() {
  snow.clear(); trees.clear(); hills.clear();
  for (int i = 0; i < 420; i++) snow.push_back({ { frand(-40, 40), frand(0, 26), frand(-30, 22) }, frand(0.6f, 1.6f), frand(0, 6) });
  for (int i = 0; i < 90; i++) { float a = frand(0, 2 * PI), d = frand(16, 60); Vector3 p = { cosf(a) * d, 0, sinf(a) * d - 12 }; if (p.z > 8) continue; trees.push_back({ p.x, p.z, frand(1.6f, 3.6f), frand(0, 6) }); }
  for (int i = 0; i < 26; i++) { float a = PI + (i / 25.f) * PI; float d = frand(70, 95); hills.push_back({ cosf(a) * d * 1.3f, sinf(a) * d * 0.6f - 30, frand(14, 30), frand(10, 22) }); }
}
void WinterBG::Update(float dt) {
  t += dt;
  for (auto& f : snow) { f.p.y -= f.s * dt; f.p.x += sinf(t * 0.7f + f.w) * dt * 0.5f; if (f.p.y < 0) { f.p.y = 26; f.p.x = frand(-40, 40); } }
}
void WinterBG::Draw(const Camera3D& cam, bool sky) {
  if (sky) {
    UI::Sky(Hex(0x070b24), Hex(0x24306b));
    // stars
    for (int i = 0; i < 140; i++) { float x = fmodf(i * 211.7f, W), y = fmodf(i * 97.3f, 420); float tw = 0.5f + 0.5f * sinf(t * 2 + i); DrawCircle((int)x, (int)y, 1 + (i % 3 == 0), Fade(WHITE, 0.25f + 0.6f * tw)); }
    // aurora ribbons
    BeginBlendMode(BLEND_ADDITIVE);
    for (int band = 0; band < 2; band++)
      for (int x = 0; x < W; x += 6) {
        float y = 170 + band * 60 + 50 * sinf(x * 0.0035f + t * 0.25f + band) + 22 * sinf(x * 0.011f - t * 0.4f);
        float a = 0.5f + 0.5f * sinf(x * 0.006f + t * 0.5f + band * 2);
        Color c = band ? Hex(0x8b5cf6, (unsigned char)(70 * a)) : Hex(0x2ef2b5, (unsigned char)(90 * a));
        DrawRectangleGradientV(x, (int)y - 120, 7, 120, Fade(c, 0), c);
        DrawRectangleGradientV(x, (int)y, 7, 40, c, Fade(c, 0));
      }
    EndBlendMode();
  }
  lit.Begin(cam, { 0.13f, 0.17f, 0.36f }, 0.011f);
  BeginMode3D(cam);
  lit.Pattern(PAT_SNOW);
  lit.Draw(prim.plane, TRS({ 0, 0, 0 }, { 0, 0, 0 }, { 400, 1, 400 }), Hex(0xc9d6f2));
  lit.Pattern(PAT_ICE);
  lit.Draw(prim.cyl, TRS({ 0, 0.01f, -6 }, { 0, 0, 0 }, { 26, 0.02f, 16 }), Hex(0x8fc7ff));        // frozen lake
  lit.Draw(prim.cyl, TRS({ 0, 0.015f, -6 }, { 0, 0, 0 }, { 20, 0.02f, 11 }), Hex(0xa5d8ff));
  lit.Pattern(PAT_NONE);
  for (auto& h : hills) { lit.Draw(prim.cone, TRS({ h.x, -1, h.y }, { 0, h.x, 0 }, { h.z * 2, h.w, h.z * 2 }), Hex(0x8ea6d8)); lit.Draw(prim.cone, TRS({ h.x, -1 + h.w * 0.55f, h.y }, { 0, h.x, 0 }, { h.z * 0.9f, h.w * 0.45f, h.z * 0.9f }), Hex(0xf1f5ff)); }
  for (auto& tr : trees) {
    float s = tr.z, sway = sinf(t + tr.w) * 0.03f;
    lit.Draw(prim.cyl, TRS({ tr.x, 0, tr.y }, { 0, 0, 0 }, { 0.3f * s * 0.4f, 0.5f * s, 0.3f * s * 0.4f }), Hex(0x5c3d2e));
    for (int k = 0; k < 3; k++) {
      float y = 0.4f * s + k * 0.45f * s, r = (1.1f - k * 0.28f) * s;
      lit.Draw(prim.cone, TRS({ tr.x, y, tr.y }, { sway, 0, sway }, { r, 0.75f * s, r }), Hex(0x1b5e45));
      lit.Draw(prim.cone, TRS({ tr.x, y + 0.42f * s, tr.y }, { sway, 0, sway }, { r * 0.5f, 0.34f * s, r * 0.5f }), Hex(0xf1f5ff));
    }
  }
  for (auto& f : snow) DrawCube(f.p, 0.07f, 0.07f, 0.07f, Fade(WHITE, 0.85f));
  EndMode3D();
}

// ---------- confetti ----------
void Confetti::Spawn(Vector3 at, int n, float spread, float up) {
  const unsigned cols[] = { 0xff4d6d, 0xffd43b, 0x4dabff, 0x51cf66, 0xcc5de8, 0xff922b, 0xffffff };
  for (int i = 0; i < n && l.size() < 900; i++)
    l.push_back({ { at.x + frand(-spread, spread), at.y + frand(0, 2), at.z + frand(-spread, spread) }, { frand(-3, 3), frand(up * 0.5f, up), frand(-3, 3) },
                  { frand(0, 6), frand(0, 6), frand(0, 6) }, { frand(-8, 8), frand(-8, 8), frand(-8, 8) }, Hex(cols[rand() % 7]), frand(4, 7) });
}
void Confetti::Update(float dt) {
  for (auto& c : l) { c.v.y -= 9 * dt; c.v = Vector3Scale(c.v, expf(-1.8f * dt)); if (c.v.y < -2.2f) c.v.y = -2.2f; c.p = Vector3Add(c.p, Vector3Scale(c.v, dt)); c.r = Vector3Add(c.r, Vector3Scale(c.w, dt)); c.life -= dt; if (c.p.y < 0.02f) { c.p.y = 0.02f; c.v = { 0, 0, 0 }; c.w = { 0, 0, 0 }; } }
  l.erase(std::remove_if(l.begin(), l.end(), [](const C& c) { return c.life <= 0; }), l.end());
}
void Confetti::Draw() { for (auto& c : l) lit.Draw(prim.cube, TRS(c.p, c.r, { 0.16f, 0.02f, 0.1f }), c.c); }
