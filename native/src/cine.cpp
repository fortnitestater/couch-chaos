// CINEMATIC MOMENTS — big plays (a bomb going off, a strike, a smash, a goal, a winner) cut to a movie shot:
// a hit-stop and a speed-ramped slow motion, a low orbiting camera with a dutch tilt, a dolly zoom and a shake,
// a fireball / sparks / debris / smoke / shockwave in the world, a flash that lights the whole scene,
// and lens effects (impact frame, zoom blur, speed lines, colour split, letterbox) in the final pass.
#include "cc.h"
#include "rlgl.h"

extern Camera3D g_finalCam;   // the camera the frame is actually drawn with (main.cpp)
float g_hudA = 1;             // overlay (HUD) opacity: the shot clears the screen

namespace cine {
namespace {
struct Preset {
  float dur, hitStop, slowDur, slowMin;   // timing (real seconds) and how slow the slow motion gets
  float frame, fov0, fov1, elev, yawOff, orbit, roll, shake, tIn, tOut; bool cut;
  float zoom, aber, speed, impact, flash, bars, hud, light;
};
//                     dur   hit    slow  min    frame fov0 fov1 elev   yaw    orbit   roll   shake tIn    tOut  cut    zoom  aber  speed imp   flash bars  hud   light
const Preset PRE[] = {
  /* BOOM   */ { 3.0f, 0.16f, 1.5f, 0.16f, 1.5f, 30, 46, 0.10f, 0.42f, -0.55f, 0.11f, 1.0f, 0.0f, 0.4f, true, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f },
  /* STRIKE */ { 2.9f, 0.10f, 1.5f, 0.2f, 1.6f, 30, 38, 0.12f, 1.25f, -0.85f, -0.09f, 0.55f, 0.0f, 0.4f, true, 0.7f, 0.8f, 1.0f, 1.0f, 0.6f, 1.0f, 0.0f, 0.8f },
  /* SMASH  */ { 5.0f, 0.12f, 0.8f, 0.2f, 1.5f, 34, 34, 0.06f, 0.5f, 0.0f, 0.14f, 0.6f, 0.0f, 0.5f, true, 0.5f, 0.8f, 1.0f, 1.0f, 0.4f, 1.0f, 0.0f, 0.6f },
  /* GOAL   */ { 2.5f, 0.10f, 1.1f, 0.28f, 1.7f, 32, 42, 0.30f, 0.55f, 0.5f, 0.08f, 0.7f, 0.0f, 0.4f, true, 0.8f, 0.8f, 1.0f, 1.0f, 0.7f, 1.0f, 0.0f, 0.9f },
  /* WIN    */ { 3.6f, 0.0f, 0.0f, 1.0f, 1.5f, 38, 30, 0.05f, -0.5f, 0.85f, 0.04f, 0.0f, 0.75f, 0.9f, false, 0.0f, 0.25f, 0.0f, 0.0f, 0.25f, 1.0f, 0.0f, 0.6f },
  /* REVEAL */ { 2.1f, 0.0f, 0.0f, 1.0f, 1.8f, 36, 32, 0.08f, 0.25f, 0.18f, 0.0f, 0.0f, 0.45f, 0.7f, false, 0.0f, 0.3f, 0.0f, 0.0f, 0.15f, 0.5f, 1.0f, 0.5f },
  /* SNOW   */ { 3.0f, 0.14f, 1.5f, 0.18f, 1.6f, 30, 44, 0.16f, 0.35f, -0.5f, 0.09f, 1.0f, 0.0f, 0.4f, true, 0.9f, 0.7f, 0.0f, 1.0f, 0.9f, 1.0f, 0.0f, 0.8f },
  /* KO     */ { 2.6f, 0.12f, 1.3f, 0.22f, 1.05f, 34, 40, 0.24f, 0.35f, -0.45f, 0.07f, 0.8f, 0.0f, 0.45f, true, 0.8f, 0.7f, 0.5f, 1.0f, 0.6f, 1.0f, 0.0f, 0.8f },
};
const int PRIO[] = { 4, 3, 2, 3, 5, 1, 4, 3 };

struct State { bool on = false; float t = 0, side = 1; Shot s; Preset p{}; bool follow = false; float impT = -1; Vector3 impAt{}, lastF{}; } S;

// ---- world effects ----
enum { F_GLOW, F_SPARK, F_DEBRIS, F_SMOKE, F_RING, F_RINGV, F_DECAL };
struct Fx { int type; Vector3 p, v; float life, max, s0, s1; Color c; float drag, grav; Vector3 rot, spin; };
std::vector<Fx> fx;
Mesh disc{}, ring{}, streak{}; Material um{}; bool built = false;
float emitAcc = 0;

inline float Sm(float x) { x = Clamp(x, 0, 1); return x * x * (3 - 2 * x); }
inline float EaseOut(float x) { x = Clamp(x, 0, 1); return 1 - (1 - x) * (1 - x) * (1 - x); }
Vector3 RandDir() { Vector3 d; do d = { frand(-1, 1), frand(-1, 1), frand(-1, 1) }; while (Vector3LengthSqr(d) > 1 || Vector3LengthSqr(d) < 0.01f); return Vector3Normalize(d); }
Color Mix(Color a, Color b, float k) { return ColorLerp(a, b, Clamp(k, 0, 1)); }

// soft round glow (alpha falls off to the rim), a soft-edged ring band, and a spark streak that fades to its tail
void Build() {
  if (built) return; built = true;
  const int SEG = 40;
  disc = Mesh{}; disc.vertexCount = SEG * 3; disc.triangleCount = SEG;
  disc.vertices = (float*)MemAlloc(disc.vertexCount * 3 * sizeof(float)); disc.colors = (unsigned char*)MemAlloc(disc.vertexCount * 4);
  disc.texcoords = (float*)MemAlloc(disc.vertexCount * 2 * sizeof(float));
  for (int i = 0; i < SEG; i++) {
    float a0 = 2 * PI * i / SEG, a1 = 2 * PI * (i + 1) / SEG; float P[9] = { 0, 0, 0, cosf(a0) * 0.5f, sinf(a0) * 0.5f, 0, cosf(a1) * 0.5f, sinf(a1) * 0.5f, 0 };
    for (int k = 0; k < 9; k++) disc.vertices[i * 9 + k] = P[k];
    for (int v = 0; v < 3; v++) { unsigned char* c = disc.colors + (i * 3 + v) * 4; c[0] = c[1] = c[2] = 255; c[3] = v == 0 ? 255 : 0; }
  }
  UploadMesh(&disc, false);
  // ring: inner edge (clear) → band (solid) → outer edge (clear)
  const int RS = 96; const float R0 = 0.78f, R1 = 0.93f, R2 = 1.0f;
  ring = Mesh{}; ring.vertexCount = RS * 12; ring.triangleCount = RS * 4;
  ring.vertices = (float*)MemAlloc(ring.vertexCount * 3 * sizeof(float)); ring.colors = (unsigned char*)MemAlloc(ring.vertexCount * 4);
  ring.texcoords = (float*)MemAlloc(ring.vertexCount * 2 * sizeof(float));
  int vi = 0;
  auto V = [&](float r, float a, unsigned char al) { ring.vertices[vi * 3] = cosf(a) * r; ring.vertices[vi * 3 + 1] = sinf(a) * r; ring.vertices[vi * 3 + 2] = 0; unsigned char* c = ring.colors + vi * 4; c[0] = c[1] = c[2] = 255; c[3] = al; vi++; };
  for (int i = 0; i < RS; i++) {
    float a0 = 2 * PI * i / RS, a1 = 2 * PI * (i + 1) / RS;
    const float rr[3] = { R0, R1, R2 }; const unsigned char aa[3] = { 0, 255, 0 };
    for (int b = 0; b < 2; b++) {
      V(rr[b], a0, aa[b]); V(rr[b + 1], a0, aa[b + 1]); V(rr[b + 1], a1, aa[b + 1]);
      V(rr[b], a0, aa[b]); V(rr[b + 1], a1, aa[b + 1]); V(rr[b], a1, aa[b]);
    }
  }
  UploadMesh(&ring, false);
  // streak: a thin quad from the head (y=0, bright) to the tail (y=1, clear)
  streak = Mesh{}; streak.vertexCount = 6; streak.triangleCount = 2;
  streak.vertices = (float*)MemAlloc(6 * 3 * sizeof(float)); streak.colors = (unsigned char*)MemAlloc(6 * 4); streak.texcoords = (float*)MemAlloc(6 * 2 * sizeof(float));
  const float SV[18] = { -0.5f, 0, 0, 0.5f, 0, 0, 0.5f, 1, 0, -0.5f, 0, 0, 0.5f, 1, 0, -0.5f, 1, 0 };
  for (int k = 0; k < 18; k++) streak.vertices[k] = SV[k];
  for (int v = 0; v < 6; v++) { unsigned char* c = streak.colors + v * 4; c[0] = c[1] = c[2] = 255; c[3] = SV[v * 3 + 1] > 0.5f ? 0 : 255; }
  UploadMesh(&streak, false);
  um = LoadMaterialDefault();
}

void Glow(Vector3 p, Vector3 v, float s0, float s1, float life, Color c, float drag = 3, float grav = 0) { if (fx.size() < 900) fx.push_back({ F_GLOW, p, v, life, life, s0, s1, c, drag, grav, {}, {} }); }
void Spark(Vector3 p, Vector3 v, float len, float life, Color c, float grav, float drag = 1.2f) { if (fx.size() < 900) fx.push_back({ F_SPARK, p, v, life, life, len, len, c, drag, grav, {}, {} }); }
void Debris(Vector3 p, Vector3 v, float s, float life, Color c, float grav) { if (fx.size() < 900) fx.push_back({ F_DEBRIS, p, v, life, life, s, s, c, 0.4f, grav, { frand(0, 6), frand(0, 6), frand(0, 6) }, { frand(-9, 9), frand(-9, 9), frand(-9, 9) } }); }
void Smoke(Vector3 p, Vector3 v, float s0, float s1, float life, Color c) { if (fx.size() < 900) fx.push_back({ F_SMOKE, p, v, life, life, s0, s1, c, 1.4f, -0.12f * s1, {}, {} }); }
void Ring(Vector3 p, float r1, float life, Color c, bool vertical) { if (fx.size() < 900) fx.push_back({ vertical ? F_RINGV : F_RING, p, {}, life, life, 0.05f * r1, r1, c, 0, 0, {}, {} }); }

// what each kind of moment throws into the world
void Spawn(const Shot& s, int kind) {
  float z = s.size; Vector3 at = s.at; bool fl = s.floorY > -1e8f; Vector3 gp = { at.x, fl ? s.floorY + 0.02f * z : at.y, at.z };
  Color hot = Hex(0xfff3c4), c = s.c;
  if (kind == BOOM || kind == SNOW) {
    bool snow = kind == SNOW;
    Color core = snow ? Hex(0xf4fbff) : hot, fire1 = snow ? Hex(0xd0ebff) : Hex(0xffb347), fire2 = snow ? Hex(0x74c0fc) : Hex(0xff5a1f);
    Glow(at, {}, 0.3f * z, (snow ? 1.2f : 1.9f) * z, 0.2f, core, 0);
    for (int i = 0; i < (snow ? 8 : 34); i++) { Vector3 d = RandDir(); Glow(Vector3Add(at, Vector3Scale(d, 0.2f * z)), Vector3Scale(d, z * frand(1.5f, 4.6f)), z * frand(0.3f, 0.55f), z * frand(0.8f, 1.4f), frand(0.55f, 1.15f), i % 3 ? fire1 : fire2, 3.2f, -0.5f * z); }
    for (int i = 0; i < 90; i++) { Vector3 d = RandDir(); d.y = fabsf(d.y) * 0.8f + 0.15f; Spark(at, Vector3Scale(Vector3Normalize(d), z * frand(4, 11)), z * frand(0.25f, 0.6f), frand(0.5f, 1.4f), i % 4 ? (snow ? WHITE : Hex(0xffd27a)) : (snow ? Hex(0xa5d8ff) : Hex(0xff7b39)), z * 4.5f); }
    for (int i = 0; i < 26; i++) { Vector3 d = RandDir(); d.y = fabsf(d.y) + 0.3f; Debris(at, Vector3Scale(Vector3Normalize(d), z * frand(2, 6)), z * frand(0.07f, 0.2f), frand(2.2f, 3.4f), snow ? Mix(WHITE, Hex(0xd0ebff), frand(0, 1)) : Mix(Hex(0x2b2b2b), Hex(0x5c4a3a), frand(0, 1)), z * 5); }
    for (int i = 0; i < (snow ? 30 : 18); i++) { Vector3 d = RandDir(); if (snow) d.y = fabsf(d.y) * 0.6f; Smoke(Vector3Add(at, Vector3Scale(d, 0.4f * z)), Vector3Add(Vector3Scale(d, z * frand(0.8f, 2.0f)), { 0, z * frand(0.4f, 1.1f), 0 }), z * (snow ? 0.4f : 0.6f), z * (snow ? frand(0.8f, 1.3f) : frand(1.5f, 2.4f)), frand(2.4f, 3.6f), snow ? Color{ 236, 245, 255, 150 } : Color{ 30, 26, 30, 215 }); }
    Ring(at, 3.2f * z, 0.45f, snow ? Fade(core, 0.6f) : fire1, true); Ring(at, 4.4f * z, 0.7f, Fade(fire2, 0.8f), true);
    if (fl) { Ring(gp, 5.5f * z, 0.75f, fire1, false); Ring(gp, 3.0f * z, 0.5f, core, false); }
  } else if (kind == STRIKE) {
    Glow(at, {}, 0.3f * z, 1.7f * z, 0.2f, hot, 0);
    for (int i = 0; i < 80; i++) { Vector3 d = RandDir(); d.y = fabsf(d.y) * 1.2f + 0.2f; Spark(at, Vector3Scale(Vector3Normalize(d), z * frand(4, 12)), z * frand(0.3f, 0.7f), frand(0.5f, 1.3f), i % 3 ? Hex(0xffe08a) : WHITE, z * 6); }
    for (int i = 0; i < 18; i++) { Vector3 d = RandDir(); d.y = fabsf(d.y) + 0.4f; Debris(at, Vector3Scale(Vector3Normalize(d), z * frand(3, 7)), z * frand(0.05f, 0.12f), frand(1.6f, 2.6f), Mix(Hex(0xf1e3c8), Hex(0xd9b382), frand(0, 1)), z * 7); }
    for (int i = 0; i < 10; i++) Glow(Vector3Add(at, Vector3Scale(RandDir(), 0.4f * z)), Vector3Scale(RandDir(), z), z * 0.3f, z * 0.9f, frand(0.4f, 0.8f), c, 3);
    Ring(gp, 5.0f * z, 0.6f, c, false); Ring(gp, 2.6f * z, 0.4f, hot, false); Ring(at, 2.4f * z, 0.4f, hot, true);
    for (int i = 0; i < 8; i++) Smoke(Vector3Add(gp, { frand(-1, 1) * z, 0.1f * z, frand(-1, 1) * z }), { frand(-1, 1) * z, z * 0.4f, frand(-1, 1) * z }, z * 0.4f, z * 1.3f, frand(1.4f, 2.2f), Color{ 255, 240, 220, 70 });
  } else if (kind == SMASH) {
    Glow(at, {}, 0.1f, 0.9f, 0.12f, hot, 0);
    for (int i = 0; i < 40; i++) Spark(at, Vector3Scale(RandDir(), frand(2, 6)), frand(0.1f, 0.25f), frand(0.12f, 0.3f), i % 3 ? Hex(0xffc46b) : WHITE, 3);
    Ring(at, 1.0f, 0.12f, hot, true);
  } else if (kind == GOAL) {
    Glow(at, {}, 0.3f * z, 2.0f * z, 0.22f, Mix(c, WHITE, 0.5f), 0);
    for (int i = 0; i < 100; i++) { Vector3 d = RandDir(); d.y = fabsf(d.y) * 1.5f + 0.3f; Spark(at, Vector3Scale(Vector3Normalize(d), z * frand(3, 10)), z * frand(0.3f, 0.6f), frand(0.6f, 1.5f), i % 3 ? Mix(c, WHITE, 0.35f) : WHITE, z * 4); }
    for (int i = 0; i < 14; i++) Glow(Vector3Add(gp, { frand(-0.4f, 0.4f) * z, frand(0, 3) * z, frand(-0.4f, 0.4f) * z }), { 0, z * frand(1, 3), 0 }, z * 0.4f, z * 0.9f, frand(0.6f, 1.1f), c, 1.5f);
    Ring(gp, 5.0f * z, 0.7f, c, false); Ring(gp, 2.8f * z, 0.45f, WHITE, false); Ring(at, 3.4f * z, 0.5f, Mix(c, WHITE, 0.5f), true);
  } else if (kind == WIN) {
    Ring(gp, 2.6f * z, 1.0f, Hex(0xffd43b), false); Ring(gp, 1.6f * z, 0.7f, hot, false);
    for (int i = 0; i < 40; i++) { Vector3 d = RandDir(); d.y = fabsf(d.y) * 2 + 0.6f; Spark(at, Vector3Scale(Vector3Normalize(d), z * frand(2, 6)), z * frand(0.2f, 0.45f), frand(0.8f, 1.6f), i % 2 ? Hex(0xffd43b) : WHITE, z * 2.5f); }
  } else if (kind == KO) {   // the blow that drops him: a white-hot flash, sparks, a ring on the canvas
    Glow(at, {}, 0.3f * z, 1.5f * z, 0.18f, hot, 0);
    for (int i = 0; i < 60; i++) { Vector3 d = RandDir(); d.y = fabsf(d.y) * 0.9f + 0.2f; Spark(at, Vector3Scale(Vector3Normalize(d), z * frand(3, 9)), z * frand(0.25f, 0.55f), frand(0.4f, 1.0f), i % 3 ? Hex(0xffe08a) : WHITE, z * 5); }
    Ring(gp, 4.0f * z, 0.55f, c, false); Ring(at, 2.0f * z, 0.35f, hot, true);
  } else if (kind == REVEAL) {
    for (int i = 0; i < 30; i++) Spark(at, Vector3Scale(RandDir(), z * frand(2, 5)), z * frand(0.15f, 0.35f), frand(0.4f, 0.9f), i % 2 ? c : WHITE, z * 2);
    Ring(at, 2.2f * z, 0.5f, c, true);
  }
}

// a billboard facing the camera (disc / ring lie in the local XY plane)
Matrix Billboard(Vector3 p, float sx, float sy, float rotZ = 0) {
  Vector3 f = Vector3Normalize(Vector3Subtract(g_finalCam.position, p)), up0 = { 0, 1, 0 };
  Vector3 r = Vector3CrossProduct(up0, f); if (Vector3LengthSqr(r) < 1e-6f) r = { 1, 0, 0 }; r = Vector3Normalize(r);
  Vector3 u = Vector3CrossProduct(f, r);
  float c = cosf(rotZ), s = sinf(rotZ); Vector3 R = Vector3Add(Vector3Scale(r, c), Vector3Scale(u, s)), U = Vector3Subtract(Vector3Scale(u, c), Vector3Scale(r, s));
  Matrix m = MatrixIdentity();
  m.m0 = R.x * sx; m.m1 = R.y * sx; m.m2 = R.z * sx; m.m4 = U.x * sy; m.m5 = U.y * sy; m.m6 = U.z * sy; m.m8 = f.x; m.m9 = f.y; m.m10 = f.z; m.m12 = p.x; m.m13 = p.y; m.m14 = p.z;
  return m;
}
void DrawU(const Mesh& m, Matrix t, Color c) { um.maps[MATERIAL_MAP_DIFFUSE].color = c; DrawMesh(m, um, t); }
}  // namespace

bool Busy() { return S.on; }
bool Hero(Color c) { return S.on && S.s.hero && S.s.c.r == c.r && S.s.c.g == c.g && S.s.c.b == c.b; }
void Reset() { S = State{}; fx.clear(); g_hudA = 1; }
void End() { if (S.on && S.t < S.p.dur - S.p.tOut) S.p.dur = S.t + S.p.tOut; }

void Play(const Shot& s) {
  int k = Clamp(s.kind, 0, (int)(sizeof(PRE) / sizeof(PRE[0])) - 1);
  if (S.on) {
    int cur = PRIO[S.s.kind];
    if (cur == 5 && k != WIN) return;                     // never interrupt the winner's shot
    if (PRIO[k] < cur || (PRIO[k] == cur && S.t < 0.6f)) return;
  }
  S.on = true; S.t = 0; S.s = s; S.s.kind = k; S.p = PRE[k]; S.side = s.side != 0 ? s.side : (frand(0, 1) < 0.5f ? -1.f : 1.f);
  if (s.dur > 0) S.p.dur = s.dur;
  emitAcc = 0;
  Spawn(S.s, k);
  if (S.p.hitStop > 0) { Sfx("cinehit"); Sfx("slowmo"); } else if (k == WIN) Sfx("whoosh");
}

// a tracking shot: the camera stays on this moving subject, which leaves a trail of fire
void Follow(Vector3 p) {
  if (!S.on || S.s.kind != SMASH || S.impT >= 0) return;
  if (!S.follow) { S.follow = true; S.lastF = p; }
  float d = Vector3Distance(p, S.lastF); int n = (int)(d / 0.07f);
  for (int i = 1; i <= n && fx.size() < 860; i++) {
    Vector3 q = Vector3Lerp(S.lastF, p, (float)i / n);
    Glow(q, Vector3Scale(RandDir(), 0.35f), 0.09f, frand(0.22f, 0.36f), frand(0.16f, 0.26f), i % 3 ? Hex(0xff7a1a) : Hex(0xffc94d), 2.5f, -0.8f);
    if (i % 4 == 0) Spark(q, Vector3Scale(RandDir(), frand(1, 3.5f)), frand(0.06f, 0.14f), frand(0.15f, 0.35f), Hex(0xffd27a), 3);
  }
  if (n > 0) S.lastF = p;
  S.s.at = p;
}
// the ball slams into the ground: freeze, flash, crater, dust and chips, then the shot wraps up
void Impact(Vector3 at) {
  if (!S.on || S.s.kind != SMASH || S.impT >= 0) return;
  S.impT = S.t; S.impAt = at; S.s.at = at; S.p.dur = S.t + 2.3f; Sfx("cinehit");
  float fy = S.s.floorY > -1e8f ? S.s.floorY : at.y; Vector3 g = { at.x, fy + 0.02f, at.z };
  Color hot = Hex(0xfff3c4), org = Hex(0xff8a2a);
  Glow({ g.x, g.y + 0.2f, g.z }, {}, 0.3f, 2.6f, 0.22f, hot, 0);
  for (int i = 0; i < 12; i++) Glow({ g.x, g.y + 0.15f, g.z }, { frand(-3, 3), frand(0.5f, 2.5f), frand(-3, 3) }, 0.25f, frand(0.6f, 1.0f), frand(0.25f, 0.5f), i % 2 ? org : Hex(0xffc94d), 3.5f);
  for (int i = 0; i < 80; i++) { Vector3 d = RandDir(); d.y = fabsf(d.y) * 1.6f + 0.35f; Spark(g, Vector3Scale(Vector3Normalize(d), frand(4, 12)), frand(0.25f, 0.6f), frand(0.4f, 1.1f), i % 3 ? Hex(0xffd27a) : WHITE, 6); }
  for (int i = 0; i < 28; i++) { float a = frand(0, 2 * PI), r = frand(0, 0.35f); Smoke({ g.x + cosf(a) * r, g.y + 0.15f, g.z + sinf(a) * r }, { cosf(a) * frand(1.5f, 4.5f), frand(0.4f, 1.6f), sinf(a) * frand(1.5f, 4.5f) }, 0.3f, frand(1.0f, 1.8f), frand(1.6f, 2.6f), Color{ 228, 220, 204, 150 }); }
  for (int i = 0; i < 20; i++) { Vector3 d = RandDir(); d.y = fabsf(d.y) * 2 + 0.5f; Debris(g, Vector3Scale(Vector3Normalize(d), frand(2.5f, 7)), frand(0.04f, 0.11f), frand(1.8f, 2.8f), i % 3 ? Hex(0x2a6fb0) : Hex(0xe9f1f7), 9.8f); }
  Ring(g, 5.0f, 0.7f, hot, false); Ring(g, 3.0f, 0.45f, org, false); Ring({ g.x, g.y + 0.35f, g.z }, 2.6f, 0.4f, WHITE, true);
  if (fx.size() < 900) fx.push_back({ F_DECAL, g, {}, 3.5f, 3.5f, 1.5f, 1.5f, Color{ 18, 14, 12, 215 }, 0, 0, {}, {} });   // scorch mark
}

float TimeScale() {
  if (!S.on) return 1;
  const Preset& p = S.p; float t = S.t;
  if (S.s.kind == SMASH) {   // deep slow motion while the ball flies, a freeze on the impact, then ramp back up
    if (S.impT < 0) return t < p.hitStop ? 0.03f : 0.23f;
    float ti = t - S.impT; if (ti < 0.14f) return 0.03f;
    return 0.3f + 0.7f * Sm((ti - 0.14f) / 0.9f);
  }
  if (t < p.hitStop) return 0.03f;
  if (p.slowDur <= 0) return 1;
  float k = (t - p.hitStop) / p.slowDur; if (k >= 1) return 1;
  return p.slowMin + (1 - p.slowMin) * Sm((k - 0.55f) / 0.45f);   // hold the slow motion, then ramp back to speed
}

void Update(float dt) {
  float gdt = dt * TimeScale();
  if (S.on) {
    S.t += dt;
    // the winner keeps throwing gold sparks for a while
    if (S.s.kind == WIN && S.t < 2.6f) { emitAcc += dt * 46; while (emitAcc >= 1) { emitAcc -= 1; float z = S.s.size; Vector3 d = { frand(-0.5f, 0.5f), 1, frand(-0.5f, 0.5f) }; Spark(Vector3Add(S.s.at, { frand(-0.6f, 0.6f) * z, -0.3f * z, frand(-0.6f, 0.6f) * z }), Vector3Scale(Vector3Normalize(d), z * frand(2.5f, 5)), z * frand(0.15f, 0.35f), frand(0.7f, 1.3f), frand(0, 1) < 0.5f ? Hex(0xffd43b) : WHITE, z * 2.2f); } }
    if (S.t >= S.p.dur) S.on = false;
  }
  // HUD steps aside during the shot, then comes back
  float want = 1;
  if (S.on) { float in = S.p.cut ? Sm(S.t / 0.15f) : Sm(S.t / S.p.tIn), out = 1 - Sm((S.t - (S.p.dur - S.p.tOut)) / S.p.tOut); want = 1 + (S.p.hud - 1) * fminf(in, out); }
  g_hudA = want;
  for (auto& f : fx) {
    f.life -= gdt; float k = expf(-f.drag * gdt);
    f.v = Vector3Scale(f.v, k); f.v.y -= f.grav * gdt; f.p = Vector3Add(f.p, Vector3Scale(f.v, gdt));
    if (f.type == F_DEBRIS) {
      f.rot = Vector3Add(f.rot, Vector3Scale(f.spin, gdt));
      float fy = S.s.floorY > -1e8f ? S.s.floorY : -1e9f;
      if (f.p.y < fy + f.s0 * 0.5f) { f.p.y = fy + f.s0 * 0.5f; f.v.y = fabsf(f.v.y) * 0.35f; f.v.x *= 0.6f; f.v.z *= 0.6f; f.spin = Vector3Scale(f.spin, 0.5f); }
    }
  }
  fx.erase(std::remove_if(fx.begin(), fx.end(), [](const Fx& f) { return f.life <= 0; }), fx.end());
}

Camera3D Apply(Camera3D c) {
  if (!S.on) return c;
  const Preset& p = S.p; float t = S.t, u = Clamp(t / p.dur, 0, 1);
  float w = p.cut ? 1 : Sm(t / p.tIn); w *= 1 - Sm((t - (p.dur - p.tOut)) / p.tOut);
  w = w * w * (3 - 2 * w);
  Vector3 d0 = Vector3Subtract(c.position, c.target); float L0 = fmaxf(Vector3Length(d0), 0.01f);
  float yaw0 = atan2f(d0.x, d0.z), pitch0 = asinf(Clamp(d0.y / L0, -1, 1));
  float yawB = Vector3LengthSqr(S.s.face) > 1e-6f ? atan2f(S.s.face.x, S.s.face.z) : yaw0;
  float yaw1, pitch1, dist, fov, shakeA, roll;
  if (S.s.kind == SMASH) {
    const float T = 3.4f;   // one full lap around the ball
    float tf = S.impT >= 0 ? S.impT : t, ti = S.impT >= 0 ? t - S.impT : -1;
    float ph = 2 * PI * tf / T + (ti > 0 ? ti * 0.3f : 0);
    yaw1 = yawB + S.side * (p.yawOff + ph);
    pitch1 = 0.2f - 0.42f * cosf(ph);                      // starts under the ball looking up, rises over the top, comes back down
    dist = 2.0f * S.s.size * (1 + 0.3f * cosf(2 * ph));    // an ellipse: wide at the sides, close at the front and back
    fov = 34 - 7 * expf(-t * 6);
    shakeA = 0.5f * expf(-t * 5);
    roll = 0.16f * sinf(ph) * S.side;
    if (ti >= 0) { float k = EaseOut(ti / 1.2f); dist *= 1 + 2.6f * k; pitch1 = Lerp(pitch1, 0.45f, k); fov = 36 + 4 * k - 9 * expf(-ti * 7); shakeA = 1.3f * expf(-ti * 3.5f); roll *= 1 - k; }
  } else {
    yaw1 = yawB + (p.yawOff + S.s.yaw) * S.side + p.orbit * S.side * (1 - powf(1 - u, 1.7f));
    pitch1 = S.s.elev > -9 ? S.s.elev : p.elev;
    fov = Lerp(p.fov0, p.fov1, EaseOut(u));
    dist = S.s.size * p.frame / tanf(fov * DEG2RAD / 2);
    shakeA = p.shake * expf(-t * 3.2f); roll = p.roll * S.side;
  }
  float yaw = yaw0 + angDiff(yaw0, yaw1) * w, pitch = Lerp(pitch0, pitch1, w), L = expf(Lerp(logf(L0), logf(dist), w));
  Vector3 tgt = Vector3Lerp(c.target, S.s.at, w);
  c.fovy = Lerp(c.fovy, fov, w);
  c.position = Vector3Add(tgt, { sinf(yaw) * cosf(pitch) * L, sinf(pitch) * L, cosf(yaw) * cosf(pitch) * L });
  if (S.s.floorY > -1e8f) c.position.y = fmaxf(c.position.y, S.s.floorY + 0.22f);   // never under the floor
  if (S.s.maxR > 0) { float r = hypotf(c.position.x, c.position.z); if (r > S.s.maxR) { c.position.x *= S.s.maxR / r; c.position.z *= S.s.maxR / r; } }   // never through the cage wall
  c.target = tgt;
  // handheld impact shake that settles, and a dutch tilt
  float sh = shakeA * w * L * 0.02f;
  Vector3 n = { sinf(t * 53) + 0.5f * sinf(t * 97 + 1), sinf(t * 61 + 2) + 0.5f * sinf(t * 89), sinf(t * 47 + 3) + 0.5f * sinf(t * 71) };
  c.position = Vector3Add(c.position, Vector3Scale(n, sh)); c.target = Vector3Add(c.target, Vector3Scale(n, sh * 0.35f));
  Vector3 f = Vector3Normalize(Vector3Subtract(c.target, c.position));
  c.up = Vector3RotateByAxisAngle({ 0, 1, 0 }, f, roll * w);
  return c;
}

void Light(Vector4& pos, Vector3& col) {
  pos = { 0, 0, 0, 1 }; col = { 0, 0, 0 };
  if (!S.on) return;
  float t = S.t, z = S.s.size, e = 0; Vector3 base = { 1, 0.6f, 0.25f };
  switch (S.s.kind) {
    case BOOM: e = 4.2f * expf(-t * 2.0f) * (1 + 0.25f * sinf(t * 43)); base = { 1, 0.55f, 0.2f }; break;
    case SNOW: e = 3.0f * expf(-t * 2.2f); base = { 0.75f, 0.88f, 1 }; break;
    case STRIKE: e = 2.6f * expf(-t * 2.6f); base = { 1, 0.8f, 0.4f }; break;
    case SMASH: e = 1.6f * expf(-t * 5) + (S.impT >= 0 ? 4.0f * expf(-(t - S.impT) * 2.2f) : 0.6f); base = { 1, 0.62f, 0.28f }; break;
    case GOAL: e = 3.0f * expf(-t * 2.0f); base = { S.s.c.r / 255.f, S.s.c.g / 255.f, S.s.c.b / 255.f }; break;
    case WIN: e = 1.1f * Sm(t / 0.6f) * (1 - Sm((t - S.p.dur + 0.9f) / 0.9f)); base = { 1, 0.82f, 0.45f }; break;
    case REVEAL: e = 0.9f * expf(-t * 2.2f); base = { S.s.c.r / 255.f, S.s.c.g / 255.f, S.s.c.b / 255.f }; break;
    case KO: e = 2.4f * expf(-t * 2.6f); base = { 1, 0.82f, 0.55f }; break;
  }
  e *= S.p.light;
  pos = { S.s.at.x, S.s.at.y + 0.3f * z, S.s.at.z, z * 5.5f }; col = Vector3Scale(base, e);
  if (S.s.kind == SMASH) pos = { S.s.at.x, S.s.at.y + 0.25f, S.s.at.z, 4.5f };
}

PostFx Post() {
  PostFx o{}; o.ctr = { 0.5f, 0.5f }; o.fc = WHITE;
  if (!S.on) return o;
  const Preset& p = S.p; float t = S.t, out = 1 - Sm((t - (p.dur - p.tOut)) / p.tOut);
  o.impact = t < 0.07f ? p.impact : t < 0.11f ? p.impact * 0.45f : 0;
  o.flash = p.flash * (t < 0.07f ? 0 : expf(-(t - 0.07f) * 6.5f));
  o.zoom = p.zoom * expf(-t * 2.4f) * out;
  o.aber = p.aber * (0.2f + 0.8f * expf(-t * 3.0f)) * out;
  float slowEnd = p.hitStop + p.slowDur;
  o.speed = p.speed * (t < slowEnd ? 1 : expf(-(t - slowEnd) * 5)) * out;
  o.bars = p.bars * fminf(p.cut ? Sm(t / 0.18f) : Sm(t / p.tIn), out);
  { float ok = Clamp((t - (p.dur - p.tOut)) / p.tOut, 0, 1); if (p.cut && ok > 0) o.zoom = fmaxf(o.zoom, 0.5f * sinf(ok * PI)); }   // whip back: blur the swing
  if (S.s.kind == SMASH) {
    float ti = S.impT >= 0 ? t - S.impT : 1e9f;
    o.impact = (t < 0.06f || ti < 0.07f) ? 1.f : ti < 0.11f ? 0.45f : 0.f;
    o.flash = (t > 0.06f ? 0.45f * expf(-(t - 0.06f) * 6) : 0) + (ti > 0.07f && ti < 1e8f ? 0.95f * expf(-(ti - 0.07f) * 5) : 0);
    o.zoom = (0.45f * expf(-t * 3) + (ti < 1e8f ? 0.95f * expf(-ti * 2.6f) : 0)) * out;
    o.aber = (0.35f + 0.65f * expf(-t * 3) + (ti < 1e8f ? 0.8f * expf(-ti * 3) : 0)) * out;
    o.speed = (S.impT < 0 ? 0.4f : expf(-ti * 2.2f)) * out;
  }
  o.fc = S.s.kind == BOOM ? Hex(0xffb056) : S.s.kind == SNOW ? Hex(0xdff1ff) : Mix(S.s.c, WHITE, 0.45f);
  Vector2 sp = GetWorldToScreenEx(S.s.at, g_finalCam, W, H);
  o.ctr = { Clamp(sp.x / W, 0.05f, 0.95f), Clamp(1 - sp.y / H, 0.05f, 0.95f) };
  return o;
}

void Draw3D() {
  bool ball = S.on && S.s.kind == SMASH && S.follow && S.impT < 0;
  if (fx.empty() && !ball) return;
  Build();
  rlDrawRenderBatchActive();
  // debris: real lit chunks
  for (auto& f : fx) if (f.type == F_DEBRIS) {
    float a = Clamp(f.life / 0.4f, 0, 1); Color c = f.c; c.a = (unsigned char)(255 * a);
    lit.Draw(prim.cube, TRS(f.p, f.rot, { f.s0, f.s0 * 0.7f, f.s0 * 0.85f }), c);
  }
  rlDrawRenderBatchActive(); rlDisableDepthMask();
  for (auto& f : fx) if (f.type == F_DECAL) {
    Color c = f.c; c.a = (unsigned char)(c.a * Clamp(f.life / 1.2f, 0, 1));
    DrawU(disc, MatrixMultiply(MatrixMultiply(MatrixScale(f.s0, f.s0, 1), MatrixRotateX(PI / 2)), MatrixTranslate(f.p.x, f.p.y, f.p.z)), c);
  }
  // smoke: soft dark (or snow-white) billows, blended normally
  for (auto& f : fx) if (f.type == F_SMOKE) {
    float k = 1 - f.life / f.max, s = Lerp(f.s0, f.s1, EaseOut(k)); Color c = f.c; c.a = (unsigned char)(c.a * Sm(k / 0.08f) * (1 - Sm((k - 0.45f) / 0.55f)));
    DrawU(disc, Billboard(f.p, s, s, f.s1 * 3 + k), c);
  }
  rlDrawRenderBatchActive();
  BeginBlendMode(BLEND_ADDITIVE);
  if (ball) { float fl = 1 + 0.15f * sinf(S.t * 40); DrawU(disc, Billboard(S.s.at, 0.55f * fl, 0.55f * fl), Color{ 255, 120, 30, 210 }); DrawU(disc, Billboard(S.s.at, 0.22f, 0.22f), Color{ 255, 240, 200, 230 }); }
  for (auto& f : fx) {
    float k = 1 - f.life / f.max;
    if (f.type == F_GLOW) {
      float s = Lerp(f.s0, f.s1, EaseOut(k)); Color c = Mix(Hex(0xfff2c8), f.c, Sm(k / 0.16f)); c.a = (unsigned char)(205 * (1 - Sm((k - 0.2f) / 0.8f)));
      DrawU(disc, Billboard(f.p, s, s), c); if (k < 0.3f) DrawU(disc, Billboard(f.p, s * 0.4f, s * 0.4f), Fade(WHITE, (1 - k / 0.3f) * 0.45f));
    } else if (f.type == F_SPARK) {
      float sp = Vector3Length(f.v); if (sp < 1e-3f) continue;
      Vector3 a = Vector3Scale(f.v, -1 / sp), toCam = Vector3Normalize(Vector3Subtract(g_finalCam.position, f.p));
      Vector3 side = Vector3CrossProduct(a, toCam); if (Vector3LengthSqr(side) < 1e-6f) continue; side = Vector3Normalize(side);
      float len = f.s0 * Clamp(sp / (f.s0 * 6), 0.3f, 2.5f), wd = f.s0 * 0.09f;
      Matrix m = MatrixIdentity(); m.m0 = side.x * wd; m.m1 = side.y * wd; m.m2 = side.z * wd; m.m4 = a.x * len; m.m5 = a.y * len; m.m6 = a.z * len; m.m8 = toCam.x; m.m9 = toCam.y; m.m10 = toCam.z; m.m12 = f.p.x; m.m13 = f.p.y; m.m14 = f.p.z;
      Color c = f.c; c.a = (unsigned char)(255 * (1 - Sm((k - 0.5f) / 0.5f)));
      DrawU(streak, m, c);
    } else if (f.type == F_RING || f.type == F_RINGV) {
      float r = Lerp(f.s0, f.s1, EaseOut(k)); Color c = f.c; c.a = (unsigned char)(255 * (1 - Sm((k - 0.2f) / 0.8f)));
      Matrix m = f.type == F_RINGV ? Billboard(f.p, r, r) : MatrixMultiply(MatrixMultiply(MatrixScale(r, r, 1), MatrixRotateX(PI / 2)), MatrixTranslate(f.p.x, f.p.y, f.p.z));
      DrawU(ring, m, c);
    }
  }
  rlDrawRenderBatchActive();
  EndBlendMode(); BeginBlendMode(BLEND_ALPHA);
  rlEnableDepthMask();
}

void Draw2D() {
  if (!S.on || S.s.caption.empty()) return;
  float t0 = S.s.kind == SMASH ? S.impT : S.p.hitStop * 0.5f; if (t0 < 0) return;
  float t = S.t - t0; if (t < 0) return;
  float save = g_hudA; g_hudA = 1;
  float k = Clamp(t / 0.26f, 0, 1), out = Sm((S.t - (S.p.dur - S.p.tOut - 0.15f)) / 0.4f);
  float sc = 1 + 1.9f * powf(1 - k, 3), a = fminf(1, k * 3) * (1 - out);
  float fs = 150 * sc * (1 + 0.04f * sinf(t * 3)), x = 800 + out * 90, y = 600 - (1 - k) * 30;
  float sp = (1 - k) * 46 + 7 * expf(-t * 3) + 3;
  Color gold = S.s.kind == SNOW ? Hex(0xe7f5ff) : Mix(S.s.c, WHITE, 0.25f);
  Text(x - sp, y + sp * 0.2f, fs, Fade(Color{ 255, 46, 99, 255 }, a * 0.6f), S.s.caption, 0, TS_DISPLAY | TS_PLAIN);
  Text(x + sp, y - sp * 0.2f, fs, Fade(Color{ 40, 220, 255, 255 }, a * 0.6f), S.s.caption, 0, TS_DISPLAY | TS_PLAIN);
  Text(x, y, fs, Fade(gold, a), S.s.caption, 0, TS_DISPLAY | TS_SHADOW | TS_HEAVY);
  if (!S.s.sub.empty()) { float k2 = Clamp((t - 0.22f) / 0.3f, 0, 1); Text(x + (1 - EaseOut(k2)) * 120, y + 104, 46, Fade(WHITE, a * k2), S.s.sub, 0, TS_DISPLAY | TS_SHADOW | TS_SPACED); }
  g_hudA = save;
}
}  // namespace cine
