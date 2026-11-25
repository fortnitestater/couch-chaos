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

ToonHold g_hold;
Vector3 ToonPoint(Vector3 pos, float yaw, float sc, float hop, Vector3 local) {
  Matrix base = MatrixMultiply(MatrixMultiply(MatrixScale(sc, sc, sc), MatrixRotateY(yaw)), MatrixTranslate(pos.x, pos.y + hop, pos.z));
  return Vector3Transform(local, base);
}
// ---------- toon character: a kid in a hooded onesie, big face, walking legs, a hat per player ----------
void DrawToon(Vector3 pos, float yaw, Color c, float t, float hop, float cheer, float sc) {
  bool moving = hop > 0.02f;
  float ph = t * 11.f, swing = moving ? sinf(ph) : 0.f;
  float breath = sinf(t * 2.2f) * 0.015f;
  float squash = 1 + breath + (moving ? 0.08f * cosf(ph * 2) : 0) + cheer * 0.04f * sinf(t * 12);
  lit.Caster({ pos.x, pos.y, pos.z }, 0.62f * sc * (1 - fminf(hop, 1.2f) * 0.35f));
  float lean = moving ? 0.12f : 0.f;
  Matrix base = MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixScale(sc, sc, sc), MatrixRotateX(lean)), MatrixRotateY(yaw)), MatrixTranslate(pos.x, pos.y + hop, pos.z));
  auto P = [&](const Mesh& m, Vector3 p, Vector3 r, Vector3 s, Color col) { lit.Draw(m, MatrixMultiply(TRS(p, r, s), base), col); };
  auto X = [&](Vector3 p) { return Vector3Transform(p, base); };
  auto Lb = [&](Vector3 a, Vector3 b, float r, Color col) { Limb(X(a), X(b), r * sc, col); };
  bool flash = c.r == 255 && c.g == 255 && c.b == 255;
  Color dark = Shade(c, -0.35f), light = Shade(c, 0.35f), skin = Hex(0xffdcc0), ink = Hex(0x1c1633);
  Color shoe = flash ? WHITE : Hex(0x2b2d42), glove = Hex(0xfdfdfd);
  // legs + shoes
  for (int s = -1; s <= 1; s += 2) {
    float sw = swing * s, lift = moving ? fmaxf(0, -sw) * 0.1f : 0;
    Vector3 hip = { s * 0.16f, 0.46f, 0 }, foot = { s * 0.17f, 0.15f + lift, sw * 0.17f };
    Lb(hip, foot, 0.085f, dark);
    P(prim.sphere, { foot.x, foot.y - 0.03f, foot.z + 0.07f }, { 0, 0, 0 }, { 0.25f, 0.18f, 0.36f }, shoe);
    P(prim.sphere, { foot.x, foot.y - 0.09f, foot.z + 0.07f }, { 0, 0, 0 }, { 0.27f, 0.07f, 0.38f }, Hex(0xf1f3f5));
  }
  // body (onesie) with a zip and a lighter tummy
  float by = 0.8f;
  P(prim.sphere, { 0, by, 0 }, { 0, 0, 0 }, { 0.76f, 0.84f * squash, 0.66f }, c);
  P(prim.sphere, { 0, by - 0.06f, 0.2f }, { 0, 0, 0 }, { 0.48f, 0.56f * squash, 0.3f }, light);
  P(prim.cube, { 0, by + 0.05f, 0.335f }, { 0.25f, 0, 0 }, { 0.025f, 0.34f, 0.02f }, dark);
  // arms with white cartoon gloves
  for (int s = -1; s <= 1; s += 2) {
    float up = cheer * (0.9f + 0.15f * sinf(t * 12 + s));
    float armSw = moving ? -swing * s * 0.18f : sinf(t * 2.2f + s) * 0.02f;
    Vector3 sh = { s * 0.33f, by + 0.2f, 0 };
    Vector3 hand = { s * (0.5f + 0.08f * up), by - 0.18f + up * 1.05f + (1 - up) * 0.02f, 0.05f + armSw };
    if (g_hold.on && s == -1) hand = g_hold.hand;  // right hand gripping a racket / bat
    Lb(sh, hand, 0.08f, c);
    P(prim.sphere, hand, { 0, 0, 0 }, { 0.2f, 0.2f, 0.2f }, glove);
    P(prim.sphere, { hand.x + s * 0.02f, hand.y + 0.06f, hand.z + 0.07f }, { 0, 0, 0 }, { 0.08f, 0.1f, 0.08f }, glove);  // thumb
  }
  // head: hood in the player colour, face peeking out
  float hy = 1.4f + (squash - 1) * 0.6f + (moving ? 0.02f * fabsf(cosf(ph)) : 0);
  P(prim.sphere, { 0, hy, -0.02f }, { 0, 0, 0 }, { 0.9f, 0.84f, 0.84f }, c);
  P(prim.sphere, { 0, hy - 0.03f, 0.2f }, { 0, 0, 0 }, { 0.76f, 0.72f, 0.5f }, light);         // hood rim
  P(prim.sphere, { 0, hy - 0.03f, 0.26f }, { 0, 0, 0 }, { 0.68f, 0.63f, 0.5f }, skin);          // face
  // eyes: big, glossy, blink every few seconds, glance around
  float bl = fmodf(t * 0.73f + 0.3f, 3.3f) < 0.11f ? 0.12f : 1.f;
  float look = sinf(t * 0.6f) * 0.018f;
  for (int s = -1; s <= 1; s += 2) {
    P(prim.sphere, { s * 0.13f, hy + 0.02f, 0.465f }, { 0, 0, 0 }, { 0.17f, 0.23f * bl, 0.08f }, WHITE);
    if (bl > 0.5f) {
      P(prim.sphere, { s * 0.13f + look, hy + 0.0f, 0.5f }, { 0, 0, 0 }, { 0.115f, 0.155f, 0.04f }, ink);
      P(prim.sphere, { s * 0.13f + look + 0.03f, hy + 0.05f, 0.52f }, { 0, 0, 0 }, { 0.04f, 0.045f, 0.02f }, WHITE);
    }
    if (cheer > 0.4f) P(prim.cube, { s * 0.14f, hy + 0.19f + cheer * 0.03f, 0.47f }, { 0, 0, s * 0.25f }, { 0.12f, 0.03f, 0.03f }, ink);  // eyebrows
    P(prim.sphere, { s * 0.24f, hy - 0.1f, 0.42f }, { 0, 0, 0 }, { 0.12f, 0.07f, 0.04f }, Hex(0xff8fab, 200));                         // cheeks
  }
  // mouth: a D-shaped open smile that widens when cheering
  float mo = 0.08f + 0.07f * cheer;
  P(prim.hemi, { 0, hy - 0.13f, 0.47f }, { PI - 0.25f, 0, 0 }, { 0.17f + 0.05f * cheer, mo, 0.07f }, Hex(0x5c1626));
  P(prim.sphere, { 0, hy - 0.13f - mo * 0.6f, 0.49f }, { 0, 0, 0 }, { 0.09f + 0.03f * cheer, 0.04f + 0.02f * cheer, 0.03f }, Hex(0xff6b81));
  // accessory, picked from the colour so every player looks different
  int acc = ((int)c.r * 7 + (int)c.g * 13 + (int)c.b * 29) % 8;
  if (flash) acc = -1;
  float top = hy + 0.4f;
  switch (acc) {
    case 0:  // beanie + pom-pom
      P(prim.hemi, { 0, hy + 0.12f, -0.02f }, { -0.15f, 0, 0 }, { 0.93f, 0.6f, 0.88f }, dark);
      P(prim.cyl, { 0, hy + 0.08f, -0.02f }, { -0.15f, 0, 0 }, { 0.95f, 0.11f, 0.9f }, light);
      P(prim.sphere, { 0, top + 0.1f, -0.08f }, { 0, 0, 0 }, { 0.2f, 0.2f, 0.2f }, WHITE);
      break;
    case 1:  // baseball cap
      P(prim.hemi, { 0, hy + 0.15f, -0.02f }, { -0.1f, 0, 0 }, { 0.9f, 0.55f, 0.86f }, dark);
      P(prim.cyl, { 0, hy + 0.17f, 0.38f }, { 0.12f, 0, 0 }, { 0.6f, 0.03f, 0.42f }, dark);
      P(prim.sphere, { 0, top + 0.02f, -0.05f }, { 0, 0, 0 }, { 0.08f, 0.05f, 0.08f }, light);
      break;
    case 2: {  // golden crown
      Color gold = Hex(0xffc93c);
      P(prim.cyl, { 0, top - 0.1f, -0.02f }, { 0, 0, 0 }, { 0.56f, 0.18f, 0.56f }, gold);
      for (int k = 0; k < 5; k++) { float a = k * 2 * PI / 5; P(prim.cone, { sinf(a) * 0.24f, top + 0.07f, cosf(a) * 0.24f - 0.02f }, { 0, 0, 0 }, { 0.15f, 0.2f, 0.15f }, gold); }
      P(prim.sphere, { 0, top - 0.01f, 0.27f }, { 0, 0, 0 }, { 0.09f, 0.09f, 0.05f }, Hex(0xe03131));
    } break;
    case 3:  // bunny ears on the hood
      for (int s = -1; s <= 1; s += 2) {
        float wob = sinf(t * 3 + s) * 0.08f + (moving ? 0.15f * sinf(ph) * s : 0);
        P(prim.sphere, { s * 0.17f, top + 0.18f, -0.05f }, { -0.1f, 0, s * (0.18f + wob) }, { 0.17f, 0.5f, 0.12f }, c);
        P(prim.sphere, { s * 0.17f, top + 0.18f, 0.0f }, { -0.1f, 0, s * (0.18f + wob) }, { 0.08f, 0.36f, 0.04f }, Hex(0xffb3c6));
      }
      break;
    case 4:  // headphones
      for (int k = 0; k <= 8; k++) { float a = -PI / 2 + k * PI / 8; P(prim.sphere, { sinf(a) * 0.47f, hy + 0.02f + cosf(a) * 0.45f, -0.04f }, { 0, 0, 0 }, { 0.09f, 0.09f, 0.09f }, ink); }
      for (int s = -1; s <= 1; s += 2) P(prim.cyl, { s * 0.42f, hy - 0.02f, -0.02f }, { 0, 0, PI / 2 }, { 0.3f, 0.14f, 0.3f }, Hex(0xff4d6d));
      break;
    case 5:  // party hat
      P(prim.cone, { 0.04f, top - 0.06f, -0.02f }, { 0, 0, -0.12f }, { 0.36f, 0.52f, 0.36f }, Hex(0xffd43b));
      P(prim.cyl, { 0.03f, top + 0.06f, -0.02f }, { 0, 0, -0.12f }, { 0.29f, 0.05f, 0.29f }, Hex(0xff6b81));
      P(prim.sphere, { -0.03f, top + 0.47f, -0.02f }, { 0, 0, 0 }, { 0.13f, 0.13f, 0.13f }, Hex(0x4dabf7));
      break;
    case 6: {  // bobbing antenna
      float b = sinf(t * 4) * 0.05f;
      P(prim.cyl, { 0, top - 0.05f, -0.04f }, { b, 0, 0 }, { 0.04f, 0.3f, 0.04f }, ink);
      P(prim.sphere, { 0, top + 0.28f, -0.04f + b * 0.3f }, { 0, 0, 0 }, { 0.15f, 0.15f, 0.15f }, Hex(0xfff3bf));
    } break;
    case 7:  // sprout
      P(prim.cyl, { 0, top - 0.05f, -0.04f }, { 0, 0, 0 }, { 0.035f, 0.22f, 0.035f }, Hex(0x2f9e44));
      for (int s = -1; s <= 1; s += 2) P(prim.sphere, { s * 0.1f, top + 0.18f, -0.04f }, { 0, 0, s * 0.5f + sinf(t * 2) * 0.1f }, { 0.22f, 0.07f, 0.13f }, Hex(0x51cf66));
      break;
    default:  // bear ears on the hood
      for (int s = -1; s <= 1; s += 2) P(prim.sphere, { s * 0.27f, top - 0.04f, -0.06f }, { 0, 0, 0 }, { 0.2f, 0.2f, 0.12f }, c);
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
