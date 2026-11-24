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
  Vector4 R = { r.x, (float)H - r.y - r.height, r.width, r.height };
  Vector4 A = V4(a), B = V4(b), Bo = V4(bor); float rr = fminf(rad, fminf(r.width, r.height) * 0.5f);
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

// ---------- toon character ----------
void DrawToon(Vector3 pos, float yaw, Color c, float t, float hop, float cheer, float sc) {
  float squash = 1 + 0.12f * sinf(t * 9) * (hop > 0.05f ? 1 : 0.25f);
  Matrix base = MatrixMultiply(MatrixMultiply(MatrixScale(sc, sc, sc), MatrixRotateY(yaw)), MatrixTranslate(pos.x, pos.y + hop, pos.z));
  auto P = [&](const Mesh& m, Vector3 p, Vector3 r, Vector3 s, Color col) { lit.Draw(m, MatrixMultiply(TRS(p, r, s), base), col); };
  Color dark = Shade(c, -0.3f), light = Shade(c, 0.25f);
  P(prim.sphere, { -0.17f, 0.1f, 0.05f }, { 0, 0, 0 }, { 0.26f, 0.2f, 0.34f }, Hex(0x343a40));        // feet
  P(prim.sphere, { 0.17f, 0.1f, 0.05f }, { 0, 0, 0 }, { 0.26f, 0.2f, 0.34f }, Hex(0x343a40));
  P(prim.sphere, { 0, 0.62f, 0 }, { 0, 0, 0 }, { 0.78f, 0.86f * squash, 0.7f }, c);                    // body
  P(prim.sphere, { 0, 0.55f, 0.24f }, { 0, 0, 0 }, { 0.5f, 0.55f * squash, 0.3f }, light);             // belly
  float hy = 1.22f + (squash - 1) * 0.5f;
  P(prim.sphere, { 0, hy, 0 }, { 0, 0, 0 }, { 0.66f, 0.62f, 0.62f }, Hex(0xffe0bd));                   // head
  P(prim.sphere, { 0, hy + 0.2f, -0.03f }, { 0, 0, 0 }, { 0.7f, 0.36f, 0.68f }, dark);                 // beanie
  P(prim.sphere, { 0, hy + 0.42f, -0.03f }, { 0, 0, 0 }, { 0.2f, 0.2f, 0.2f }, WHITE);                 // pom-pom
  for (int s = -1; s <= 1; s += 2) {
    P(prim.sphere, { s * 0.12f, hy + 0.02f, 0.27f }, { 0, 0, 0 }, { 0.14f, 0.17f, 0.08f }, WHITE);    // eyes
    P(prim.sphere, { s * 0.12f, hy + 0.01f, 0.31f }, { 0, 0, 0 }, { 0.07f, 0.09f, 0.04f }, Hex(0x1a1438));
    P(prim.sphere, { s * 0.2f, hy - 0.12f, 0.24f }, { 0, 0, 0 }, { 0.1f, 0.06f, 0.04f }, Hex(0xff8fab, 200)); // cheeks
    float up = cheer * (0.9f + 0.15f * sinf(t * 12 + s));
    Vector3 hand = { s * (0.48f + 0.05f * cheer), 0.62f + up * 0.75f, 0.06f };
    P(prim.sphere, { s * 0.38f, 0.66f + up * 0.35f, 0.02f }, { 0, 0, 0 }, { 0.2f, 0.2f, 0.2f }, c);    // arm
    P(prim.sphere, hand, { 0, 0, 0 }, { 0.2f, 0.2f, 0.2f }, Hex(0xffe0bd));                            // hand
  }
  P(prim.sphere, { 0, hy - 0.16f, 0.28f }, { 0, 0, 0 }, { 0.16f, 0.06f + 0.05f * cheer, 0.04f }, Hex(0x862e2e)); // smile
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
  lit.Draw(prim.plane, TRS({ 0, 0, 0 }, { 0, 0, 0 }, { 400, 1, 400 }), Hex(0xc9d6f2));
  lit.Draw(prim.cyl, TRS({ 0, 0.01f, -6 }, { 0, 0, 0 }, { 26, 0.02f, 16 }), Hex(0x8fc7ff));        // frozen lake
  lit.Draw(prim.cyl, TRS({ 0, 0.015f, -6 }, { 0, 0, 0 }, { 20, 0.02f, 11 }), Hex(0xa5d8ff));
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
