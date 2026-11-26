// Shared art: SDF UI cards, toon characters, winter backdrop, confetti.
#include "cc.h"
#include "mesh.h"
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
static inline bool Chunky() { return g_toonK > 1.01f; }
static inline Vector3 ChunkS() { return Chunky() ? Vector3{ 1.34f, 0.9f, 1.3f } : Vector3{ 1, 1, 1 }; }
Vector3 ToonPoint(Vector3 pos, float yaw, float sc, float hop, Vector3 local) {
  sc *= g_toonK; Vector3 cs = ChunkS();
  Matrix base = MatrixMultiply(MatrixMultiply(MatrixScale(sc * cs.x, sc * cs.y, sc * cs.z), MatrixRotateY(yaw)), MatrixTranslate(pos.x, pos.y + hop, pos.z));
  return Vector3Transform(local, base);
}
// ---------- a person: natural body, friendly face, jointed limbs, walk/run cycle, hair, clothes ----------
// the shirt is the player's colour; hair, skin and shorts vary per player so everyone is recognisable
static Vector3 Ik(Vector3 sh, Vector3 target, float l1, float l2, Vector3 pole) {  // elbow position for a two-bone arm
  Vector3 d = Vector3Subtract(target, sh); float L = fminf(Vector3Length(d), l1 + l2 - 0.001f); if (L < 1e-4f) return Vector3Add(sh, { 0, -l1, 0 });
  Vector3 u = Vector3Normalize(d); float a = (l1 * l1 + L * L - l2 * l2) / (2 * l1 * L); a = Clamp(a, -1, 1);
  Vector3 pp = Vector3Subtract(pole, Vector3Scale(u, Vector3DotProduct(pole, u))); if (Vector3Length(pp) < 1e-4f) pp = { 0, -1, 0 };
  pp = Vector3Normalize(pp); return Vector3Add(sh, Vector3Add(Vector3Scale(u, l1 * a), Vector3Scale(pp, l1 * sqrtf(1 - a * a))));
}
void DrawToon(Vector3 pos, float yaw, Color c, float t, float hop, float cheer, float sc) {
  // a friendly, polished person: natural body, slightly larger head, big bright eyes, smile, smooth tapered limbs
  unsigned hsh = (unsigned)c.r * 7 + (unsigned)c.g * 13 + (unsigned)c.b * 29;
  if (!g_hold.on && DrawPerson(pos, yaw, c, t, hop, cheer, sc, g_toonK, hsh)) return;   // real rigged people when loaded
  sc *= g_toonK;
  bool moving = hop > 0.02f, flash = c.r == 255 && c.g == 255 && c.b == 255;
  const Color SKINS[] = { Hex(0xf5d0b0), Hex(0xe8b48c), Hex(0xc98f63), Hex(0x94603f), Hex(0xf8dcc4) };
  const Color HAIRS[] = { Hex(0x3a2516), Hex(0x6b4423), Hex(0xd9a441), Hex(0x221a16), Hex(0x9c4a22), Hex(0x2e2e34) };
  Color skin = flash ? WHITE : SKINS[hsh % 5], hair = flash ? WHITE : HAIRS[(hsh / 5) % 6];
  Color shirt = c, shirtD = Shade(c, -0.22f), shirtL = Shade(c, 0.25f);
  Color shorts = flash ? WHITE : ((hsh / 3) % 2 ? Hex(0x2a3a5e) : Hex(0x34343c)), shoe = flash ? WHITE : Hex(0xfafafa), sole = flash ? WHITE : Hex(0x45454d);
  Color blush = flash ? WHITE : ColorLerp(skin, Hex(0xff7d7d), 0.32f), eyeW = flash ? WHITE : Hex(0xffffff), iris = flash ? WHITE : Hex((hsh / 7) % 3 == 0 ? 0x4a7fb5 : 0x3b2618), lip = flash ? WHITE : Hex(0x6e2a26);
  int style = (int)((hsh / 11) % 4);  // hair style
  float ph = t * 9.5f, s1 = moving ? sinf(ph) : 0.f, run = moving ? fminf(1, hop * 4) : 0;
  float bob = moving ? fabsf(cosf(ph)) * 0.04f : sinf(t * 2.2f) * 0.006f;
  Vector3 cs = ChunkS(); float thick = Chunky() ? 1.42f : 1.0f;
  lit.Caster(pos, (Chunky() ? 0.56f : 0.38f) * sc);
  float lean = moving ? 0.1f * run : 0.f;
  float swy = moving ? 0.f : sinf(t * 1.3f + (float)(hsh % 7)) * 0.035f;   // idle weight shift
  Matrix place = MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixRotateX(lean), MatrixRotateZ(swy)), MatrixRotateY(yaw)), MatrixTranslate(pos.x, pos.y + hop * 0.35f + bob * sc, pos.z));
  Matrix base = MatrixMultiply(MatrixScale(sc * cs.x, sc * cs.y, sc * cs.z), place);
  auto X = [&](Vector3 p) { return Vector3Transform(p, base); };
  auto E = [&](Vector3 p, Vector3 r, Vector3 s, Color col) { lit.Draw(prim.sphere, MatrixMultiply(TRS(p, r, s), base), col); };
  // smooth tapered limb: radius ra at a, rb at b, rounded ends
  auto Tl = [&](Vector3 a, Vector3 b, float ra, float rb, Color col) {
    Vector3 A = X(a), B = X(b), d = Vector3Subtract(B, A); float len = Vector3Length(d); if (len < 1e-4f) return;
    Vector3 u = Vector3Scale(d, 1 / len), ax = Vector3CrossProduct({ 0, 1, 0 }, u); float s = Vector3Length(ax), ang = acosf(Clamp(u.y, -1, 1));
    Matrix R = s < 1e-4f ? (u.y > 0 ? MatrixIdentity() : MatrixRotateX(PI)) : MatrixRotate(Vector3Scale(ax, 1 / s), ang);
    // the limb mesh tapers 1 -> 0.75; pick the scale so the far end matches rb
    float k = ra * sc * thick; lit.Draw(props.limb, MatrixMultiply(MatrixMultiply(MatrixScale(k, len, k), R), MatrixTranslate(A.x, A.y, A.z)), col);
    float r2 = ra * 2 * sc * thick; lit.Draw(prim.sphere, MatrixMultiply(MatrixScale(r2, r2, r2), MatrixTranslate(A.x, A.y, A.z)), col);
    float re = ra * 0.75f * 2 * sc * thick; (void)rb;
    lit.Draw(prim.sphere, MatrixMultiply(MatrixScale(re, re, re), MatrixTranslate(B.x, B.y, B.z)), col);
  };
  // ---- legs: hip -> knee -> ankle -> sneaker ----
  for (int s = -1; s <= 1; s += 2) {
    float sw = s1 * s, th = sw * (0.35f + 0.25f * run), bend = moving ? fmaxf(0, -sw) * (0.6f + 0.6f * run) + 0.1f : 0.05f;
    Vector3 hip = { s * 0.092f, 0.93f, 0 };
    Vector3 knee = Vector3Add(hip, { 0, -0.43f * cosf(th), 0.43f * sinf(th) });
    Vector3 ank = Vector3Add(knee, { 0, -0.41f * cosf(th - bend), 0.41f * sinf(th - bend) });
    Tl(Vector3Lerp(hip, knee, 0.3f), knee, 0.088f, 0, skin);                        // thigh
    Tl(hip, Vector3Lerp(hip, knee, 0.5f), 0.104f, 0, shorts);                       // shorts leg
    Tl(knee, ank, 0.066f, 0, skin);                                                 // calf
    Tl(Vector3Lerp(knee, ank, 0.72f), ank, 0.054f, 0, flash ? WHITE : Hex(0xf4f4f4)); // sock
    Vector3 f = { ank.x, ank.y - 0.05f, ank.z + 0.06f };
    E(f, { 0, 0, 0 }, { 0.115f, 0.095f, 0.27f }, shoe);                             // sneaker
    E({ f.x, f.y - 0.032f, f.z }, { 0, 0, 0 }, { 0.122f, 0.04f, 0.285f }, sole);
    E({ f.x + s * 0.052f, f.y + 0.005f, f.z - 0.01f }, { 0, 0, 0 }, { 0.02f, 0.035f, 0.13f }, shirt);  // colour stripe
  }
  // ---- torso ----
  DrawProp(props.torsoShorts, base, shorts);
  DrawProp(props.torsoShirt, base, shirt);
  E({ 0, 1.475f, 0.02f }, { 0.25f, 0, 0 }, { 0.16f, 0.04f, 0.13f }, shirtD);    // collar
  // ---- arms ----
  for (int s = -1; s <= 1; s += 2) {
    Vector3 sh = { s * 0.2f, 1.4f, 0 }, wrist;
    float up = cheer * (0.9f + 0.1f * sinf(t * 10 + s));
    if (g_hold.on && s == -1) wrist = g_hold.hand;
    else if (up > 0.05f) wrist = Vector3Lerp({ s * 0.28f, 0.98f, 0.08f }, { s * 0.34f, 1.98f + 0.04f * sinf(t * 12 + s), 0.1f }, fminf(1, up));
    else {
      float a = -s1 * s * (0.45f + 0.35f * run), br = moving ? 0 : sinf(t * 2.2f + s) * 0.01f;
      wrist = { s * 0.255f, 1.4f - 0.49f * cosf(a) + (moving ? 0.1f * run : 0.03f) + br, 0.49f * sinf(a) + (moving ? 0.12f * run : 0.07f) };
      if (!moving && Chunky()) {   // idle personality: hands on hips, a wave now and then, or a thoughtful chin rub
        int pose = (int)((hsh / 13) % 3); float cyc = fmodf(t * 0.9f + (float)(hsh % 9), 7.f);
        if (pose == 1) wrist = { s * 0.215f, 1.0f + br, 0.05f };
        if (pose == 2 && s == 1 && cyc < 1.7f) wrist = { 0.36f + 0.08f * sinf(t * 11), 1.84f, 0.12f };
        if (pose == 0 && s == -1 && cyc > 5.0f) wrist = { -0.09f, 1.52f, 0.2f };
      }
    }
    Vector3 el = Ik(sh, wrist, 0.28f, 0.25f, { s * 0.5f, -0.3f, -0.6f });
    Vector3 elDir = Vector3Normalize(Vector3Subtract(el, sh));
    Tl(Vector3Add(sh, Vector3Scale(elDir, 0.05f)), el, 0.058f, 0, skin);             // upper arm
    Tl(sh, Vector3Add(sh, Vector3Scale(elDir, 0.15f)), 0.07f, 0, shirt);            // sleeve
    Vector3 wd = Vector3Normalize(Vector3Subtract(wrist, el)); Vector3 wr = Vector3Add(el, Vector3Scale(wd, 0.24f));
    Tl(el, wr, 0.044f, 0, skin);                                                    // forearm
    Vector3 hc = Vector3Add(wr, Vector3Scale(wd, 0.055f));
    E(hc, { 0, 0, 0 }, { 0.08f, 0.1f, 0.055f }, skin);                               // hand
    E(Vector3Add(hc, { s * -0.034f, 0.025f, 0.028f }), { 0, 0, 0 }, { 0.032f, 0.055f, 0.032f }, skin);  // thumb
  }
  // ---- neck + head (head ~1.3x natural: friendlier and readable from the couch) ----
  if (Chunky()) {  // big readable head: scaled up around the neck, sitting on the wide body
    const float HK = 1.62f, piv = 1.5f;
    float look = sinf(t * 0.6f + (float)(hsh % 11)) * 0.3f, tilt = sinf(t * 0.9f + (float)(hsh % 5)) * 0.07f + (cheer > 0.3f ? -0.12f : 0);   // heads look around
    Matrix hr = MatrixMultiply(MatrixRotateZ(tilt), MatrixRotateY(look));
    base = MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixTranslate(0, -piv, 0), MatrixScale(sc * HK, sc * HK, sc * HK)), hr), MatrixTranslate(0, (piv - 0.05f) * cs.y * sc, 0)), place);
    thick = HK;
  }
  Tl({ 0, 1.43f, 0 }, { 0, 1.56f, 0.01f }, 0.058f, 0, skin);
  const float K = 1.32f, hy = 1.69f;
  DrawProp(props.head, MatrixMultiply(MatrixMultiply(MatrixScale(K, K, K), MatrixTranslate(0, hy, 0.01f)), base), skin);
  bool blink = fmodf(t + (float)(hsh % 7), 4.2f) < 0.12f;
  bool happy = cheer > 0.3f;
  for (int s = -1; s <= 1; s += 2) {
    E({ s * 0.13f, hy - 0.005f, -0.004f }, { 0, s * 0.3f, 0 }, { 0.035f, 0.065f, 0.05f }, Shade(skin, -0.05f));  // ears
    // eyes follow the curve of the face
    float ry = s * 0.36f; Vector3 n = { s * 0.35f, 0, 0.94f };
    Vector3 ec = { s * 0.048f, hy + 0.012f, 0.131f };
    if (blink || happy) {
      E(Vector3Add(ec, Vector3Scale(n, 0.004f)), { 0, ry, 0 }, { 0.046f, 0.008f, 0.014f }, flash ? WHITE : Hex(0x2a1a12));  // closed / smiling eye
    } else {
      E(ec, { 0, ry, 0 }, { 0.05f, 0.058f, 0.02f }, eyeW);
      Vector3 ic = Vector3Add(ec, Vector3Scale(n, 0.008f)); ic.y -= 0.004f;
      E(ic, { 0, ry, 0 }, { 0.034f, 0.038f, 0.012f }, iris);
      E(Vector3Add(ic, Vector3Scale(n, 0.004f)), { 0, ry, 0 }, { 0.018f, 0.02f, 0.008f }, flash ? WHITE : Hex(0x120c08));  // pupil
      E(Vector3Add(Vector3Add(ic, Vector3Scale(n, 0.007f)), { s * 0.006f - 0.004f, 0.009f, 0 }), { 0, 0, 0 }, { 0.011f, 0.011f, 0.006f }, WHITE);  // sparkle
    }
    E({ s * 0.05f, hy + 0.06f, 0.124f }, { 0, s * 0.38f, s * -0.12f }, { 0.052f, 0.013f, 0.016f }, hair);  // brows
    E({ s * 0.078f, hy - 0.03f, 0.109f }, { 0, s * 0.62f, 0 }, { 0.044f, 0.026f, 0.006f }, blush);       // cheeks
  }
  E({ 0, hy - 0.022f, 0.141f }, { 0, 0, 0 }, { 0.024f, 0.026f, 0.022f }, Shade(skin, -0.02f));  // nose
  if (happy) {
    E({ 0, hy - 0.066f, 0.126f }, { 0.2f, 0, 0 }, { 0.058f, 0.036f, 0.02f }, flash ? WHITE : Hex(0x5a1e1e));  // open laugh
    E({ 0, hy - 0.056f, 0.132f }, { 0.2f, 0, 0 }, { 0.046f, 0.01f, 0.012f }, WHITE);                        // teeth
  } else {
    for (int k = -4; k <= 4; k++) {  // a gentle smile line
      float x = k * 0.0075f, y = hy - 0.062f + 0.013f * (k * k) / 16.f;
      E({ x, y, 0.135f - 0.0007f * k * k }, { 0, 0, 0 }, { 0.014f, 0.011f, 0.012f }, lip);
    }
  }
  // ---- hair: neat crop, long with ponytail, curly, or a cap in the shirt colour ----
  if (style == 3) {
    DrawProp(props.dome, MatrixMultiply(TRS({ 0, hy + 0.03f, -0.008f }, { -0.3f, 0, 0 }, { 0.157f, 0.17f, 0.177f }), base), shirtD);
    E({ 0, hy + 0.075f, 0.15f }, { 0.22f, 0, 0 }, { 0.22f, 0.024f, 0.16f }, shirtD);   // brim
    E({ 0, hy + 0.175f, 0 }, { 0, 0, 0 }, { 0.03f, 0.02f, 0.03f }, shirt);             // button
    E({ 0, hy - 0.03f, -0.04f }, { 0, 0, 0 }, { 0.26f, 0.2f, 0.24f }, hair);          // hair under the cap
  } else {
    DrawProp(props.dome, MatrixMultiply(TRS({ 0, hy + 0.025f, -0.012f }, { -0.32f, 0, 0 }, { 0.155f, 0.175f, 0.175f }), base), hair);
    E({ 0, hy + 0.02f, -0.045f }, { 0, 0, 0 }, { 0.276f, 0.29f, 0.25f }, hair);      // back of the head
    E({ -0.035f, hy + 0.1f, 0.098f }, { -0.5f, 0, 0.25f }, { 0.17f, 0.045f, 0.09f }, hair); // side-swept fringe
    if (style == 1) {
      for (int s = -1; s <= 1; s += 2) Tl({ s * 0.12f, hy + 0.02f, -0.02f }, { s * 0.135f, hy - 0.16f, -0.04f }, 0.04f, 0, hair);  // long sides
      E({ 0, hy + 0.0f, -0.15f }, { 0, 0, 0 }, { 0.08f, 0.08f, 0.08f }, Shade(c, -0.1f));  // scrunchie
      Tl({ 0, hy - 0.01f, -0.17f }, { 0, hy - 0.22f, -0.19f }, 0.042f, 0, hair);          // ponytail
    }
    if (style == 2) for (int k = 0; k < 7; k++) { float a = k / 6.f * PI; E({ cosf(a) * 0.1f, hy + 0.1f + sinf(a) * 0.045f, 0.01f - (k % 2) * 0.05f }, { 0, 0, 0 }, { 0.085f, 0.075f, 0.085f }, hair); }
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
