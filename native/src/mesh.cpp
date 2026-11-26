// Mesh toolkit + the real-world props (see mesh.h). Everything is built once, uploaded to the GPU, drawn in one call.
#include "mesh.h"
#include "rlgl.h"

RealProps props;

void MeshBuilder::Lathe(const std::vector<Vector2>& prof, int seg, std::function<Color(float, int)> col, Matrix xf) {
  int m = (int)prof.size();
  std::vector<Vector2> pn(m);  // 2D profile normals
  for (int i = 0; i < m; i++) {
    Vector2 a = prof[std::max(0, i - 1)], b = prof[std::min(m - 1, i + 1)];
    Vector2 d = { b.x - a.x, b.y - a.y }; float l = hypotf(d.x, d.y); if (l < 1e-6f) l = 1;
    pn[i] = { d.y / l, -d.x / l };
  }
  Matrix nx = MatrixTranspose(MatrixInvert(xf)); nx.m12 = nx.m13 = nx.m14 = 0;
  for (int s = 0; s < seg; s++) {
    float a0 = 2 * PI * s / seg, a1 = 2 * PI * (s + 1) / seg;
    for (int i = 0; i + 1 < m; i++) {
      auto P = [&](int k, float a) { return Vector3Transform({ prof[k].x * cosf(a), prof[k].y, prof[k].x * sinf(a) }, xf); };
      auto N = [&](int k, float a) { return Vector3Normalize(Vector3Transform({ pn[k].x * cosf(a), pn[k].y, pn[k].x * sinf(a) }, nx)); };
      Color c0 = col((prof[i].y + prof[i + 1].y) / 2, i);
      Tri(P(i, a0), P(i + 1, a0), P(i + 1, a1), N(i, a0), N(i + 1, a0), N(i + 1, a1), c0);
      Tri(P(i, a0), P(i + 1, a1), P(i, a1), N(i, a0), N(i + 1, a1), N(i, a1), c0);
    }
  }
}
void MeshBuilder::Tube(const std::vector<Vector3>& path, float r, int sides, Color col, bool closed) {
  int m = (int)path.size(); if (m < 2) return;
  std::vector<Vector3> T(m), Nn(m), Bn(m);
  for (int i = 0; i < m; i++) {
    Vector3 a = path[closed ? (i - 1 + m) % m : std::max(0, i - 1)], b = path[closed ? (i + 1) % m : std::min(m - 1, i + 1)];
    T[i] = Vector3Normalize(Vector3Subtract(b, a));
  }
  // parallel transport frames: no twisting along the tube
  Vector3 up = fabsf(T[0].y) < 0.9f ? Vector3{ 0, 1, 0 } : Vector3{ 1, 0, 0 };
  Nn[0] = Vector3Normalize(Vector3CrossProduct(T[0], up)); Bn[0] = Vector3CrossProduct(T[0], Nn[0]);
  for (int i = 1; i < m; i++) { Vector3 b = Vector3CrossProduct(T[i], Nn[i - 1]); b = Vector3Length(b) < 1e-5f ? Bn[i - 1] : Vector3Normalize(b); Bn[i] = b; Nn[i] = Vector3CrossProduct(b, T[i]); }
  int segs = closed ? m : m - 1;
  for (int i = 0; i < segs; i++) {
    int j = (i + 1) % m;
    for (int s = 0; s < sides; s++) {
      float a0 = 2 * PI * s / sides, a1 = 2 * PI * (s + 1) / sides;
      auto D = [&](int k, float a) { return Vector3Add(Vector3Scale(Nn[k], cosf(a)), Vector3Scale(Bn[k], sinf(a))); };
      Vector3 d00 = D(i, a0), d01 = D(i, a1), d10 = D(j, a0), d11 = D(j, a1);
      Vector3 p00 = Vector3Add(path[i], Vector3Scale(d00, r)), p01 = Vector3Add(path[i], Vector3Scale(d01, r)), p10 = Vector3Add(path[j], Vector3Scale(d10, r)), p11 = Vector3Add(path[j], Vector3Scale(d11, r));
      Tri(p00, p10, p11, d00, d10, d11, col); Tri(p00, p11, p01, d00, d11, d01, col);
    }
  }
}
void MeshBuilder::Sphere(Vector3 ce, float r, Color col, int rings, int segs, Vector3 sc) {
  for (int i = 0; i < rings; i++) for (int s = 0; s < segs; s++) {
    auto D = [&](int a, int b) { float th = PI * a / rings, ph = 2 * PI * b / segs; return Vector3{ sinf(th) * cosf(ph), cosf(th), sinf(th) * sinf(ph) }; };
    auto P = [&](Vector3 d) { return Vector3{ ce.x + d.x * r * sc.x, ce.y + d.y * r * sc.y, ce.z + d.z * r * sc.z }; };
    Vector3 a = D(i, s), b = D(i + 1, s), d = D(i + 1, s + 1), e = D(i, s + 1);
    Tri(P(a), P(b), P(d), a, b, d, col); Tri(P(a), P(d), P(e), a, d, e, col);
  }
}
void MeshBuilder::Box(Vector3 c, Vector3 sz, Color col) {
  Vector3 h = Vector3Scale(sz, 0.5f);
  const Vector3 N[6] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
  for (auto& nn : N) {
    Vector3 u = fabsf(nn.y) > 0.5f ? Vector3{ 1, 0, 0 } : Vector3{ 0, 1, 0 }, w = Vector3CrossProduct(nn, u);
    auto P = [&](float a, float b) { return Vector3{ c.x + (nn.x + u.x * a + w.x * b) * h.x, c.y + (nn.y + u.y * a + w.y * b) * h.y, c.z + (nn.z + u.z * a + w.z * b) * h.z }; };
    Tri(P(-1, -1), P(1, -1), P(1, 1), nn, nn, nn, col); Tri(P(-1, -1), P(1, 1), P(-1, 1), nn, nn, nn, col);
  }
}
Mesh MeshBuilder::Build() {
  Mesh m{}; m.vertexCount = (int)v.size() / 3; m.triangleCount = m.vertexCount / 3;
  m.vertices = (float*)MemAlloc(v.size() * sizeof(float)); memcpy(m.vertices, v.data(), v.size() * sizeof(float));
  m.normals = (float*)MemAlloc(n.size() * sizeof(float)); memcpy(m.normals, n.data(), n.size() * sizeof(float));
  m.texcoords = (float*)MemAlloc(t.size() * sizeof(float)); memcpy(m.texcoords, t.data(), t.size() * sizeof(float));
  m.colors = (unsigned char*)MemAlloc(c.size()); memcpy(m.colors, c.data(), c.size());
  UploadMesh(&m, false);
  return m;
}

void DrawProp(const Mesh& m, Matrix xf, Color tint) { if (m.vertexCount) lit.Draw(m, xf, tint); }

// grip at 'hand', handle running along 'dir', string face pointing along 'face'
Matrix RacketXf(Vector3 hand, Vector3 dir, Vector3 face, float sc) {
  Vector3 Y = Vector3Normalize(dir), Z = Vector3Normalize(Vector3Subtract(face, Vector3Scale(Y, Vector3DotProduct(face, Y)))), X = Vector3CrossProduct(Y, Z);
  Matrix r = { X.x * sc, Y.x * sc, Z.x * sc, hand.x, X.y * sc, Y.y * sc, Z.y * sc, hand.y, X.z * sc, Y.z * sc, Z.z * sc, hand.z, 0, 0, 0, 1 };
  return r;
}

static std::vector<Vector2> Resample(const std::vector<Vector2>& p, int n) {  // even points along a profile (crisp colour bands)
  std::vector<float> L(1, 0); for (size_t i = 1; i < p.size(); i++) L.push_back(L.back() + hypotf(p[i].x - p[i - 1].x, p[i].y - p[i - 1].y));
  std::vector<Vector2> o; size_t k = 1;
  for (int i = 0; i <= n; i++) { float d = L.back() * i / n; while (k < p.size() - 1 && L[k] < d) k++; float u = (d - L[k - 1]) / fmaxf(1e-6f, L[k] - L[k - 1]); o.push_back({ p[k - 1].x + (p[k].x - p[k - 1].x) * u, p[k - 1].y + (p[k].y - p[k - 1].y) * u }); }
  return o;
}

void RealProps::Init() {
  if (ready) return; ready = true;
  // a real conifer (1 unit tall): tapered bark trunk + six drooping, jagged branch tiers; the snowy one has white tips
  for (int sn = 0; sn < 2; sn++) {
    MeshBuilder b; const int NS = 14;
    Color bark = Hex(0x5b3a26), barkD = Hex(0x3f271a);
    for (int i = 0; i < 8; i++) {
      float a0 = i * PI / 4, a1 = (i + 1) * PI / 4;
      Vector3 b0 = { cosf(a0) * 0.085f, 0, sinf(a0) * 0.085f }, b1 = { cosf(a1) * 0.085f, 0, sinf(a1) * 0.085f }, t0 = { cosf(a0) * 0.035f, 0.66f, sinf(a0) * 0.035f }, t1 = { cosf(a1) * 0.035f, 0.66f, sinf(a1) * 0.035f };
      Vector3 nn = Vector3Normalize({ cosf((a0 + a1) / 2), 0.1f, sinf((a0 + a1) / 2) });
      b.Tri(b0, t0, b1, nn, nn, nn, i % 2 ? bark : barkD); b.Tri(b1, t0, t1, nn, nn, nn, i % 2 ? bark : barkD);
    }
    for (int tier = 0; tier < 6; tier++) {
      float f = tier / 5.f, yb = 0.15f + f * 0.6f, yt = yb + 0.31f - f * 0.05f, r = 0.47f * (1 - f * 0.78f), twist = tier * 0.37f;
      Color in = Hex(tier % 2 ? 0x1f6b3a : 0x175c33), tip = sn ? Hex(0xf2f7ff) : Hex(tier % 2 ? 0x3aa457 : 0x2f9449), under = Hex(0x0f3d24);
      Vector3 top = { 0, yt, 0 }, mid = { 0, yb + 0.06f, 0 };
      for (int i = 0; i < NS; i++) {
        auto pt = [&](int k) { float a = twist + k * 2 * PI / NS, rr = r * (k % 2 ? 0.68f : 1.0f); return Vector3{ cosf(a) * rr, yb - (k % 2 ? 0 : 0.05f), sinf(a) * rr }; };
        Vector3 p0 = pt(i), p1 = pt(i + 1);
        Vector3 nn = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(p1, top), Vector3Subtract(p0, top)));
        if (nn.y < 0) nn = Vector3Negate(nn);
        b.Vert(top, nn, in); b.Vert(p1, nn, tip); b.Vert(p0, nn, tip);
        b.Tri(mid, p0, p1, { 0, -1, 0 }, { 0, -1, 0 }, { 0, -1, 0 }, under);
      }
    }
    (sn ? pineSnow : pine) = b.Build();
  }
  const Color WHT = { 245, 245, 240, 255 };
  // ---- tennis net (ITF size): 0.914 m at the centre, 1.07 m at the posts, posts 0.914 m outside the doubles lines ----
  {
    MeshBuilder b; const float PX = 6.4f, cell = 0.1f, cord = 0.0045f;
    auto top = [&](float x) { float u = fabsf(x) / PX; return 0.914f + 0.156f * u * u; };
    for (float x = -PX + cell; x < PX - 0.01f; x += cell) b.Tube({ { x, 0.04f, 0 }, { x, top(x) - 0.03f, 0 } }, cord, 3, { 28, 30, 34, 255 });
    for (float y = 0.04f; y < 0.9f; y += cell) b.Tube({ { -PX, y, 0 }, { PX, y, 0 } }, cord, 3, { 28, 30, 34, 255 });
    std::vector<Vector3> cable; for (int i = 0; i <= 64; i++) { float x = -PX + 2 * PX * i / 64; cable.push_back({ x, top(x), 0 }); }
    b.Tube(cable, 0.018f, 8, WHT);  // the white headband over the top cable
    for (int i = 0; i < 64; i++) { float x = -PX + 2 * PX * (i + 0.5f) / 64; b.Box({ x, top(x) - 0.03f, 0 }, { 2 * PX / 64 + 0.002f, 0.06f, 0.016f }, WHT); }
    b.Box({ 0, 0.46f, 0 }, { 0.05f, 0.9f, 0.012f }, WHT);  // centre strap
    b.Tube({ { -PX, 0.04f, 0 }, { PX, 0.04f, 0 } }, 0.008f, 6, { 28, 30, 34, 255 });
    for (int s = -1; s <= 1; s += 2) {  // posts with the winding crank
      b.Tube({ { s * (PX + 0.05f), 0, 0 }, { s * (PX + 0.05f), 1.09f, 0 } }, 0.04f, 12, { 34, 74, 52, 255 });
      b.Sphere({ s * (PX + 0.05f), 1.09f, 0 }, 0.04f, { 34, 74, 52, 255 }, 6, 10);
      b.Box({ s * (PX + 0.05f), 0.6f, 0.06f }, { 0.04f, 0.08f, 0.06f }, { 120, 124, 130, 255 });
      b.Tube({ { s * (PX + 0.05f), 0.6f, 0.09f }, { s * (PX + 0.05f), 0.6f, 0.16f }, { s * (PX + 0.05f), 0.5f, 0.16f } }, 0.008f, 6, { 120, 124, 130, 255 });
    }
    tennisNet = b.Build();
  }
  // ---- tennis racket: 68.5 cm long, 33 x 25 cm head, 16x19 string bed, wrapped grip ----
  {
    MeshBuilder f, r; const float cy = 0.525f, a = 0.125f, bb = 0.165f;
    std::vector<Vector3> hoop; for (int i = 0; i < 72; i++) { float t = 2 * PI * i / 72; hoop.push_back({ a * cosf(t), cy + bb * sinf(t), 0 }); }
    f.Tube(hoop, 0.011f, 10, WHITE, true);
    float ta = 4.3f; Vector3 jl = { a * cosf(ta), cy + bb * sinf(ta), 0 }, jr = { -jl.x, jl.y, 0 };
    f.Tube({ { -0.014f, 0.19f, 0 }, { -0.02f, 0.26f, 0 }, jl }, 0.01f, 8, WHITE);
    f.Tube({ { 0.014f, 0.19f, 0 }, { 0.02f, 0.26f, 0 }, jr }, 0.01f, 8, WHITE);
    f.Tube({ jl, { 0, jl.y - 0.012f, 0 }, jr }, 0.009f, 8, WHITE);  // throat bridge
    for (int i = 0; i < 16; i++) { float x = -0.105f + 0.21f * i / 15, h = bb * 0.95f * sqrtf(fmaxf(0, 1 - (x / (a * 0.95f)) * (x / (a * 0.95f)))); r.Tube({ { x, cy - h, 0 }, { x, cy + h, 0 } }, 0.0017f, 4, { 240, 236, 220, 255 }); }
    for (int i = 0; i < 19; i++) { float y = -0.14f + 0.28f * i / 18, w = a * 0.95f * sqrtf(fmaxf(0, 1 - (y / (bb * 0.95f)) * (y / (bb * 0.95f)))); r.Tube({ { -w, cy + y, 0 }, { w, cy + y, 0 } }, 0.0017f, 4, { 240, 236, 220, 255 }); }
    r.Lathe({ { 0, 0 }, { 0.019f, 0.0f }, { 0.02f, 0.012f }, { 0.017f, 0.02f }, { 0.017f, 0.185f }, { 0.014f, 0.2f }, { 0, 0.2f } }, 8, [](float y, int) { return y < 0.016f ? Color{ 200, 30, 40, 255 } : Color{ 30, 30, 34, 255 }; });
    racketFrame = f.Build(); racketRest = r.Build();
  }
  // ---- tennis ball: optic-yellow felt with the white curved seam ----
  {
    MeshBuilder b; b.Sphere({ 0, 0, 0 }, 1, { 214, 240, 60, 255 }, 14, 22);
    std::vector<Vector3> seam; const float A = 1, B = 0.45f, C = 2 * sqrtf(A * B);
    for (int i = 0; i < 96; i++) { float t = 2 * PI * i / 96; Vector3 p = Vector3Normalize({ A * cosf(t) + B * cosf(3 * t), A * sinf(t) - B * sinf(3 * t), C * sinf(2 * t) }); seam.push_back(Vector3Scale(p, 1.005f)); }
    b.Tube(seam, 0.05f, 5, { 250, 250, 245, 255 }, true);
    tennisBall = b.Build();
  }
  // ---- ten-pin: regulation profile (38.1 cm tall), two red neck stripes ----
  {
    std::vector<Vector2> p = { { 0, 0 }, { 0.026f, 0 }, { 0.04f, 0.02f }, { 0.058f, 0.09f }, { 0.0605f, 0.13f }, { 0.052f, 0.19f }, { 0.032f, 0.25f }, { 0.0235f, 0.28f }, { 0.029f, 0.32f }, { 0.032f, 0.35f }, { 0.024f, 0.375f }, { 0, 0.383f } };
    MeshBuilder b; b.Lathe(Resample(p, 90), 28, [](float y, int) { return (y > 0.253f && y < 0.266f) || (y > 0.274f && y < 0.287f) ? Color{ 214, 40, 50, 255 } : Color{ 250, 250, 246, 255 }; });
    pin = b.Build();
  }
  // ---- curling stone: polished granite, striking band, handle ----
  {
    std::vector<Vector2> p = { { 0, 0 }, { 0.06f, 0 }, { 0.12f, 0.006f }, { 0.138f, 0.018f }, { 0.145f, 0.04f }, { 0.145f, 0.075f }, { 0.138f, 0.097f }, { 0.12f, 0.109f }, { 0.07f, 0.114f }, { 0, 0.114f } };
    MeshBuilder b; b.Lathe(Resample(p, 60), 36, [](float y, int i) { unsigned char g = (unsigned char)(120 + ((i * 37) % 11)); return y > 0.045f && y < 0.07f ? Color{ (unsigned char)(g - 25), (unsigned char)(g - 22), (unsigned char)(g - 18), 255 } : Color{ g, (unsigned char)(g + 2), (unsigned char)(g + 6), 255 }; });
    stone = b.Build();
    MeshBuilder h; h.Lathe({ { 0, 0.114f }, { 0.05f, 0.114f }, { 0.05f, 0.13f }, { 0.03f, 0.14f }, { 0, 0.14f } }, 20, [](float, int) { return WHITE; });
    h.Tube({ { 0, 0.135f, 0 }, { 0, 0.165f, 0 }, { 0.03f, 0.19f, 0 }, { 0.13f, 0.19f, 0 } }, 0.014f, 10, WHITE);
    stoneHandle = h.Build();
  }
  // ---- baseball bat: 84 cm maple, taped handle; baseball with red stitches ----
  {
    std::vector<Vector2> p = { { 0, 0 }, { 0.024f, 0 }, { 0.025f, 0.008f }, { 0.014f, 0.022f }, { 0.0135f, 0.26f }, { 0.019f, 0.42f }, { 0.028f, 0.58f }, { 0.033f, 0.74f }, { 0.033f, 0.82f }, { 0.028f, 0.838f }, { 0, 0.84f } };
    MeshBuilder b; b.Lathe(Resample(p, 70), 20, [](float y, int) { return y > 0.03f && y < 0.25f ? Color{ 36, 36, 40, 255 } : Color{ 214, 168, 110, 255 }; });
    bat = b.Build();
    MeshBuilder s; s.Sphere({ 0, 0, 0 }, 1, { 250, 248, 240, 255 }, 12, 18);
    std::vector<Vector3> seam; for (int i = 0; i < 80; i++) { float t = 2 * PI * i / 80; seam.push_back(Vector3Scale(Vector3Normalize({ cosf(t) + 0.45f * cosf(3 * t), sinf(t) - 0.45f * sinf(3 * t), 1.34f * sinf(2 * t) }), 1.01f)); }
    s.Tube(seam, 0.045f, 4, { 210, 40, 40, 255 }, true);
    baseball = s.Build();
  }
  // ---- hockey goal (unit size, mouth faces +X): red pipe frame, white mesh netting, back bar ----
  {
    MeshBuilder b; const Color GRED = { 210, 30, 40, 255 }, GNET = { 235, 238, 242, 255 };
    b.Tube({ { 0, 0, -0.5f }, { 0, 1, -0.5f }, { 0, 1, 0.5f }, { 0, 0, 0.5f } }, 0.035f, 10, GRED);
    std::vector<Vector3> back; for (int i = 0; i <= 16; i++) { float a = PI * i / 16; back.push_back({ -0.25f - sinf(a) * 0.45f, 0.02f, -0.5f * cosf(a) }); }
    b.Tube(back, 0.025f, 8, GRED);
    // netting: top + back curtain from the crossbar to the base hoop
    for (int i = 0; i <= 16; i++) { float u = (float)i / 16, z = -0.5f + u; float a = acosf(Clamp(-z / 0.5f, -1, 1)); Vector3 base = { -0.25f - sinf(a) * 0.45f, 0.02f, z };
      b.Tube({ { 0, 1, z }, { -0.35f, 0.92f, z }, base }, 0.006f, 3, GNET); }
    for (int k = 1; k <= 8; k++) { float u = k / 9.f; std::vector<Vector3> row;
      for (int i = 0; i <= 16; i++) { float z = -0.5f + i / 16.f, a = acosf(Clamp(-z / 0.5f, -1, 1)); Vector3 base = { -0.25f - sinf(a) * 0.45f, 0.02f, z }, mid = { -0.35f, 0.92f, z };
        Vector3 p = u < 0.4f ? Vector3Lerp({ 0, 1, z }, mid, u / 0.4f) : Vector3Lerp(mid, base, (u - 0.4f) / 0.6f); row.push_back(p); }
      b.Tube(row, 0.006f, 3, GNET); }
    for (int s = -1; s <= 1; s += 2) for (int k = 1; k < 6; k++) { float y = k / 6.f; b.Tube({ { 0, y, s * 0.5f }, { -0.25f - 0.45f * 0.0f, y * 0.9f, s * 0.5f } }, 0.006f, 3, GNET); }
    hockeyGoal = b.Build();
  }
  // ---- human body: one smooth sculpted trunk (oval cross-section) for shirt + shorts, and a head shape ----
  {
    Matrix oval = MatrixScale(1, 1, 0.66f);
    MeshBuilder a; a.Lathe(Resample({ { 0, 0.97f }, { 0.158f, 0.975f }, { 0.152f, 1.05f }, { 0.143f, 1.12f }, { 0.152f, 1.2f }, { 0.172f, 1.28f }, { 0.19f, 1.35f }, { 0.197f, 1.4f }, { 0.185f, 1.445f }, { 0.13f, 1.475f }, { 0.07f, 1.495f }, { 0, 1.5f } }, 40), 28, [](float, int) { return WHITE; }, oval);
    torsoShirt = a.Build();
    MeshBuilder b; b.Lathe(Resample({ { 0, 0.84f }, { 0.12f, 0.84f }, { 0.158f, 0.88f }, { 0.168f, 0.94f }, { 0.162f, 1.0f }, { 0, 1.0f } }, 20), 28, [](float, int) { return WHITE; }, MatrixScale(1, 1, 0.7f));
    torsoShorts = b.Build();
    // head: rounder cranium, narrower chin, real proportions (23 cm tall)
    MeshBuilder h; h.Lathe(Resample({ { 0, -0.12f }, { 0.04f, -0.118f }, { 0.07f, -0.1f }, { 0.088f, -0.06f }, { 0.096f, -0.01f }, { 0.1f, 0.04f }, { 0.094f, 0.08f }, { 0.075f, 0.105f }, { 0.04f, 0.118f }, { 0, 0.12f } }, 30), 28, [](float, int) { return WHITE; }, MatrixScale(1, 1, 1.08f));
    head = h.Build();
    // tapered limb: radius 1 -> 0.75 over length 1, slight muscle swell
    MeshBuilder l; l.Lathe({ { 1.0f, 0 }, { 1.04f, 0.18f }, { 1.02f, 0.4f }, { 0.92f, 0.65f }, { 0.8f, 0.88f }, { 0.75f, 1.0f } }, 18, [](float, int) { return WHITE; });
    for (size_t i = 0; i < l.n.size(); i += 3) { float x = l.n[i], z = l.n[i + 2], q = sqrtf(x * x + z * z); if (q > 1e-5f) { l.n[i] = x / q; l.n[i + 1] = 0; l.n[i + 2] = z / q; } }  // radial normals survive non-uniform scaling
    limb = l.Build();
    // dome (radius 1, rim at y = 0): hair and caps
    { std::vector<Vector2> d; for (int i = 0; i <= 16; i++) { float a = i / 16.f * PI / 2; d.push_back({ cosf(a), sinf(a) }); } d.back().x = 0; MeshBuilder m; m.Lathe(d, 32, [](float, int) { return WHITE; });
      for (size_t t = 0; t + 2 < m.v.size() / 3; t += 3) for (int k = 0; k < 3; k++) std::swap(m.v[(t + 1) * 3 + k], m.v[(t + 2) * 3 + k]), std::swap(m.n[(t + 1) * 3 + k], m.n[(t + 2) * 3 + k]);  // face outward
      dome = m.Build(); }
  }
  // ---- ice hockey stick: 1.5 m composite shaft, taped blade (grip end at the origin, shaft along +Y) ----
  {
    MeshBuilder b;
    b.Box({ 0, 0.7f, 0 }, { 0.03f, 1.4f, 0.022f }, { 30, 32, 38, 255 });
    b.Box({ 0, 0.08f, 0 }, { 0.034f, 0.16f, 0.026f }, { 235, 235, 235, 255 });  // knob tape
    b.Box({ 0, 1.44f, 0.13f }, { 0.012f, 0.07f, 0.3f }, { 30, 32, 38, 255 });   // blade
    b.Box({ 0.007f, 1.44f, 0.15f }, { 0.004f, 0.05f, 0.22f }, { 245, 245, 245, 255 });  // white tape on the blade
    hockeyStick = b.Build();
  }
}

Mesh BuildCrowd(const std::vector<Vector4>& seats, unsigned seed) {
  // every spot gets a moulded plastic stadium seat; most are taken by a seated spectator (legs, arms, head, hair, real clothes)
  MeshBuilder b; srand(seed);
  const Color SKIN[] = { { 247, 214, 186, 255 }, { 232, 186, 150, 255 }, { 205, 150, 110, 255 }, { 150, 100, 68, 255 }, { 110, 72, 48, 255 }, { 240, 200, 168, 255 } };
  const Color TOP[] = { { 238, 240, 242, 255 }, { 35, 47, 82, 255 }, { 150, 155, 162, 255 }, { 30, 30, 34, 255 }, { 200, 48, 52, 255 }, { 110, 170, 220, 255 },
                        { 96, 110, 70, 255 }, { 120, 36, 50, 255 }, { 245, 200, 60, 255 }, { 236, 140, 170, 255 }, { 40, 120, 90, 255 }, { 250, 250, 250, 255 }, { 70, 100, 160, 255 }, { 225, 120, 50, 255 } };
  const Color LEGS[] = { { 52, 70, 110, 255 }, { 40, 52, 80, 255 }, { 34, 34, 38, 255 }, { 200, 185, 150, 255 }, { 120, 120, 125, 255 }, { 225, 225, 220, 255 } };
  const Color HAIR[] = { { 28, 22, 18, 255 }, { 70, 45, 25, 255 }, { 110, 75, 40, 255 }, { 205, 165, 90, 255 }, { 150, 150, 150, 255 }, { 140, 60, 30, 255 } };
  const Color CAP[] = { { 245, 245, 245, 255 }, { 200, 40, 50, 255 }, { 30, 45, 90, 255 }, { 230, 200, 60, 255 } };
  const Color SEAT_A = { 26, 110, 70, 255 }, SEAT_B = { 20, 88, 56, 255 };
  int idx = 0;
  for (auto& s : seats) {
    idx++;
    float yaw = s.w, cx = cosf(yaw), sx = sinf(yaw);
    auto R = [&](float lx, float ly, float lz) { return Vector3{ s.x + lx * cx + lz * sx, s.y + ly, s.z - lx * sx + lz * cx }; };
    auto Rn = [&](Vector3 n) { return Vector3{ n.x * cx + n.z * sx, n.y, -n.x * sx + n.z * cx }; };
    auto OBox = [&](Vector3 c, Vector3 sz, Color col) {  // a box in seat-local space
      Vector3 h = Vector3Scale(sz, 0.5f);
      const Vector3 N[6] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
      for (auto& nn : N) {
        Vector3 u = fabsf(nn.y) > 0.5f ? Vector3{ 1, 0, 0 } : Vector3{ 0, 1, 0 }, w = Vector3CrossProduct(nn, u);
        auto P = [&](float a, float bb) { return R(c.x + (nn.x + u.x * a + w.x * bb) * h.x, c.y + (nn.y + u.y * a + w.y * bb) * h.y, c.z + (nn.z + u.z * a + w.z * bb) * h.z); };
        Vector3 wn = Rn(nn);
        b.Tri(P(-1, -1), P(1, -1), P(1, 1), wn, wn, wn, col); b.Tri(P(-1, -1), P(1, 1), P(-1, 1), wn, wn, wn, col);
      }
    };
    // the seat: pan + back on a short stem
    Color sc = (idx / 34 + idx) % 7 == 0 ? SEAT_B : SEAT_A;
    OBox({ 0, 0.42f, 0.05f }, { 0.46f, 0.06f, 0.42f }, sc);
    OBox({ 0, 0.68f, -0.17f }, { 0.46f, 0.5f, 0.06f }, sc);
    OBox({ 0, 0.2f, 0.0f }, { 0.06f, 0.4f, 0.06f }, { 60, 64, 70, 255 });
    if (rand() % 100 < 12) continue;  // empty seat
    float k = rand() % 9 == 0 ? 0.78f : 0.92f + (rand() % 100) / 700.f;  // kids are smaller
    Color top = TOP[rand() % 14], legs = LEGS[rand() % 6], skin = SKIN[rand() % 6], hair = HAIR[rand() % 6];
    bool shorts = rand() % 3 == 0;
    float lean = ((rand() % 100) / 100.f - 0.3f) * 0.08f;
    float hipY = 0.47f, hipZ = -0.02f;
    // legs: thighs forward along the seat, shins down to the step, shoes
    for (int sd = -1; sd <= 1; sd += 2) {
      float lx = sd * 0.1f * k + (rand() % 10 - 5) * 0.004f;
      Vector3 hip = R(lx, hipY, hipZ), knee = R(lx * 1.1f, hipY + 0.02f, hipZ + 0.44f * k), ank = R(lx * 1.1f, 0.06f, hipZ + 0.5f * k);
      b.Tube({ hip, knee }, 0.075f * k, 6, legs);
      b.Tube({ knee, ank }, 0.058f * k, 6, shorts ? skin : legs);
      b.Sphere(R(lx * 1.1f, 0.05f, hipZ + 0.58f * k), 0.07f * k, rand() % 2 ? Color{ 245, 245, 245, 255 } : Color{ 40, 40, 44, 255 }, 3, 6, { 1, 0.6f, 1.6f });
    }
    // torso: a shaped lathe (narrow waist, broad shoulders), flattened front-to-back
    float th = 0.58f * k;
    Matrix xf = MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixScale(1, 1, 0.66f), MatrixRotateX(lean)), MatrixRotateY(yaw)), MatrixTranslate(R(0, hipY - 0.02f, hipZ - 0.02f).x, s.y + hipY - 0.02f, R(0, hipY, hipZ - 0.02f).z));
    b.Lathe({ { 0.0f, 0 }, { 0.17f * k, 0.01f }, { 0.18f * k, 0.12f * th }, { 0.165f * k, 0.38f * th }, { 0.2f * k, 0.72f * th }, { 0.2f * k, 0.86f * th }, { 0.12f * k, th }, { 0, th } }, 8,
            [&](float, int) { return top; }, xf);
    float shY = hipY + th * 0.88f, headY = hipY + th + 0.2f * k;
    // arms: on the lap, waving, cheering with both hands, or holding a drink / phone
    int pose = rand() % 10;
    for (int sd = -1; sd <= 1; sd += 2) {
      Vector3 sh = R(sd * 0.2f * k, shY, hipZ), el, hand;
      bool up = (pose == 0) || (pose == 1 && sd == 1);
      if (up) { el = R(sd * 0.3f * k, shY + 0.25f * k, hipZ + 0.06f); hand = R(sd * 0.27f * k, shY + 0.52f * k, hipZ + 0.12f); }
      else if (pose == 2 && sd == 1) { el = R(sd * 0.24f * k, shY - 0.26f * k, hipZ + 0.1f); hand = R(sd * 0.1f * k, shY + 0.02f, hipZ + 0.26f * k); }
      else { el = R(sd * 0.23f * k, shY - 0.27f * k, hipZ + 0.05f); hand = R(sd * 0.11f * k, hipY + 0.06f, hipZ + 0.3f * k); }
      b.Tube({ sh, el }, 0.05f * k, 5, (rand() % 3) ? top : skin);
      b.Tube({ el, hand }, 0.04f * k, 5, skin);
      b.Sphere(hand, 0.045f * k, skin, 3, 5);
      if (pose == 2 && sd == 1) b.Tube({ Vector3Add(hand, { 0, -0.04f, 0 }), Vector3Add(hand, { 0, 0.1f, 0 }) }, 0.035f, 6, rand() % 2 ? Color{ 230, 60, 50, 255 } : Color{ 240, 240, 240, 255 });  // a drink
    }
    // neck, head, hair (short, long, bald, cap, sun hat)
    b.Tube({ R(0, hipY + th - 0.02f, hipZ), R(0, headY - 0.08f * k, hipZ + 0.01f) }, 0.045f * k, 5, skin);
    Vector3 hc = R(0, headY, hipZ + 0.01f);
    b.Sphere(hc, 0.115f * k, skin, 5, 8, { 0.9f, 1.05f, 0.95f });
    int hs = rand() % 10;
    if (hs < 4) b.Sphere(R(0, headY + 0.03f * k, hipZ - 0.01f), 0.122f * k, hair, 3, 8, { 0.92f, 0.85f, 0.98f });
    else if (hs < 6) { b.Sphere(R(0, headY + 0.02f * k, hipZ - 0.02f), 0.124f * k, hair, 4, 8, { 0.95f, 0.9f, 1 }); b.Sphere(R(0, headY - 0.1f * k, hipZ - 0.07f), 0.1f * k, hair, 3, 6, { 1.1f, 1.4f, 0.6f }); }
    else if (hs < 8) { Color cc = CAP[rand() % 4]; b.Sphere(R(0, headY + 0.04f * k, hipZ), 0.122f * k, cc, 3, 8, { 0.93f, 0.8f, 0.98f }); OBox({ 0, headY + 0.045f * k, hipZ + 0.14f * k }, { 0.16f * k, 0.018f, 0.12f * k }, cc); }
    else if (hs == 8) { b.Sphere(R(0, headY + 0.05f * k, hipZ), 0.12f * k, { 235, 220, 180, 255 }, 3, 8, { 0.95f, 0.9f, 0.95f }); b.Lathe({ { 0.0f, 0 }, { 0.24f * k, 0 }, { 0.24f * k, 0.01f }, { 0, 0.01f } }, 10, [&](float, int) { return Color{ 235, 220, 180, 255 }; }, MatrixTranslate(hc.x, hc.y + 0.05f * k, hc.z)); }
    else b.Sphere(R(0, headY + 0.02f * k, hipZ - 0.01f), 0.118f * k, Color{ 120, 120, 120, 255 }, 2, 8, { 0.95f, 0.8f, 0.98f });
  }
  return b.Build();
}
