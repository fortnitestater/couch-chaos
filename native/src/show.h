// THE SHOW — a TV game-show studio shared by the quiz & party games: a stage with a light-bulb arch,
// spotlights, and one podium per player with their character standing behind it.
#pragma once
#include "kit.h"
#include "rlgl.h"

struct Show : Game {
  Camera3D cam{}; Particles3D fx; Confetti conf; float time = 0, flash = 0;
  std::vector<int> pts; std::vector<Spring> shown; std::vector<float> cheer, pop, lit_; std::vector<std::string> popTxt;
  Color tint = Hex(0x3d8bff);  // stage light colour
  bool fin = false; float endT = -1;
  CamRig rig; Camera3D master{}; std::vector<char> wasHot; int glance = -1; float glanceT = 0;
  virtual float JibK() { return 1; }   // how far the studio camera roams (Bomb Squad keeps its wire board steady)
  Player& P(int i) { return (*ps)[i]; }
  int N() { return (int)ps->size(); }
  void InitStage() {
    cam.position = { 0, 4.0f, 14.6f }; cam.target = { 0, 3.75f, 0 }; cam.up = { 0, 1, 0 }; cam.fovy = 39; cam.projection = CAMERA_PERSPECTIVE; master = cam;
    pts.assign(N(), 0); shown.assign(N(), Spring{}); cheer.assign(N(), 0); pop.assign(N(), 0); lit_.assign(N(), 0); popTxt.assign(N(), "");
  }
  float PodX(int i) { float sp = fminf(2.0f, 13.2f / fmaxf(1, N())); return (i - (N() - 1) / 2.f) * sp; }
  float PodR() { return fminf(0.78f, fminf(2.0f, 13.2f / fmaxf(1, N())) * 0.4f); }
  void Gain(int i, int g) { if (g == 0) return; pts[i] += g; pop[i] = 1.8f; popTxt[i] = (g > 0 ? "+" : "") + kit::I(g); if (g > 0) cheer[i] = 1.4f; }
  void Tick(float dt) {
    time += dt; fx.Update(dt); conf.Update(dt); flash = fmaxf(0, flash - dt * 2.5f);
    for (int i = 0; i < N(); i++) { shown[i].To((float)pts[i], dt, 60, 15.5f); pop[i] = fmaxf(0, pop[i] - dt); cheer[i] = fmaxf(0, cheer[i] - dt * 0.7f); }
  }
  std::vector<int> Order() { std::vector<int> o; for (int i = 0; i < N(); i++) o.push_back(i); std::stable_sort(o.begin(), o.end(), [&](int a, int b) { return pts[a] > pts[b]; }); return o; }
  void Win() { auto o = Order(); conf.Spawn({ PodX(o[0]), 3, 2 }, 160, 4, 7); Sfx("win"); Banner(P(o[0]).name + " WINS!", 2600, kit::Hx(P(o[0]).color).c_str()); cheer[o[0]] = 4; endT = 4.2f; WinShot(o[0]); }
  // the winner gets a slow orbit at their podium; a big answer pushes in on whoever scored most
  void WinShot(int i) { cine::Shot sh; sh.kind = cine::WIN; sh.at = { PodX(i), 2.0f, 1.6f }; sh.size = 1.2f; sh.c = P(i).color; sh.floorY = 0.1f; sh.hero = true; cine::Play(sh); }
  void RevealShot(int i) { cine::Shot s; s.kind = cine::REVEAL; s.at = { PodX(i), 2.2f, 1.6f }; s.size = 1.1f; s.c = P(i).color; s.floorY = 0.1f; cine::Play(s); }
  void DebugShot(int k) override { if (k == cine::WIN) WinShot(0); else if (k == cine::REVEAL) RevealShot(0); }
  // call from Update: true once the game is over and finished
  bool Ending(float dt) { if (endT < 0) return false; endT -= dt; if (endT <= 0 && !fin) { fin = true; auto o = Order(); std::string r; for (int i : o) r += kit::I(i) + ":" + kit::I(pts[i]) + " pts;"; Finish(r); } return true; }

  virtual void Props3D() {}  // extra 3D things on stage (drawn inside the scene)
  void Stage(std::function<bool(int)> hot = nullptr) {
    UI::Sky(Hex(0x070d26), Hex(0x1a1250));
    { // studio crane: a slow sweep across the stage that never stops; it glances at whoever just locked in an answer
      if (wasHot.size() != (size_t)N()) wasHot.assign(N(), 0);
      for (int i = 0; i < N(); i++) { bool h = hot && hot(i); if (h && !wasHot[i]) { glance = i; glanceT = 1.6f; } wasHot[i] = h; }
      glanceT -= g_frameDt;
      float jk = JibK(), tt = time;
      float yaw = jk * (0.11f * sinf(tt * 0.105f) + 0.035f * sinf(tt * 0.31f + 1)), lift = jk * 0.45f * sinf(tt * 0.083f + 2), dolly = 1 + jk * 0.045f * sinf(tt * 0.13f);
      Vector3 tg = master.target;
      if (glance >= 0 && glance < N() && glanceT > 0) { float gx = PodX(glance); tg.x += gx * 0.3f * jk; yaw -= Clamp(gx / 7, -1, 1) * 0.07f * jk; dolly -= 0.06f * jk; }
      Camera3D c = rig.Go(Vector3Add(Jib(master.position, master.target, yaw, lift, dolly), Vector3Subtract(tg, master.target)), tg, master.fovy, 1.9f);
      cam.position = c.position; cam.target = c.target; cam.fovy = c.fovy;
    }
    if (g_dbg >= 8 && g_dbg <= 12) { int q = g_dbg >= 9 ? std::min(N() - 1, g_dbg - 9) : 0; float z = g_dbg >= 9 ? 3.1f : 4.6f; cam.position = { PodX(q) + 0.3f, 2.3f, z }; cam.target = { PodX(q), g_dbg >= 9 ? 2.5f : 2.1f, 1.55f }; }
    lit.Begin(cam, { 0.05f, 0.06f, 0.16f }, 0.006f);
    BeginMode3D(cam);
    lit.Pattern(PAT_TILE);
    lit.Draw(prim.plane, TRS({ 0, 0, 0 }, { 0, 0, 0 }, { 90, 1, 90 }), Hex(0x141c40));
    lit.Pattern(PAT_NONE);
    static Material glowM = LoadMaterialDefault();
    auto Glow = [&](const Mesh& m, Matrix t, Color c) { glowM.maps[MATERIAL_MAP_DIFFUSE].color = c; DrawMesh(m, glowM, t); };
    Color tintD = Shade(tint, -0.55f);
    // stage riser: glossy dark deck with a glowing neon edge
    Glow(prim.cyl, TRS({ 0, -0.3f, 1.2f }, { 0, 0, 0 }, { 10.75f, 0.36f, 5.4f }), tint);
    lit.Draw(prim.cyl, TRS({ 0, -0.3f, 1.2f }, { 0, 0, 0 }, { 10.5f, 0.42f, 5.2f }), Hex(0x1a2350));
    lit.Draw(prim.cyl, TRS({ 0, 0.11f, 1.2f }, { 0, 0, 0 }, { 9.4f, 0.02f, 4.5f }), Hex(0x222d63));
    // back wall + the LED video wall in a bezel, with a moving light sweep
    float wz = -10;
    lit.Draw(prim.cube, TRS({ 0, 8, wz - 0.4f }, { 0, 0, 0 }, { 70, 20, 0.5f }), Hex(0x0c1230));
    lit.Draw(prim.cube, TRS({ 0, 6.3f, wz }, { 0, 0, 0 }, { 23.6f, 10.6f, 0.4f }), Hex(0x0a0e22));
    // three different sets, picked by the show's colour: LED video wall, velvet curtain with marquee bulbs, neon arches
    int var = ((tint.r >> 4) + (tint.g >> 5) * 3 + (tint.b >> 4) * 5) % 3;
    if (var == 1) {
      for (int q = 0; q < 24; q++) lit.Draw(prim.cyl, TRS({ -11.5f + q, 1.0f, wz + 0.3f }, { 0, 0, 0 }, { 1.08f, 10.6f, 0.55f }), q % 2 ? Shade(tint, -0.5f) : Shade(tint, -0.66f));
      lit.Draw(prim.cube, TRS({ 0, 11.2f, wz + 0.5f }, { 0, 0, 0 }, { 23.6f, 1.1f, 0.5f }), Shade(tint, -0.35f));
      for (int q = 0; q < 24; q++) lit.Draw(prim.sphere, TRS({ -11.5f + q, 10.6f, wz + 0.6f }, { 0, 0, 0 }, { 1.0f, 0.5f, 0.3f }), Hex(0xd9a441));
      for (int q = 0; q < 66; q++) {   // chasing marquee bulbs round the frame
        float u = q / 66.f * (2 * 23.6f + 2 * 10.6f), bx, by;
        if (u < 23.6f) { bx = -11.8f + u; by = 11.75f; } else if (u < 34.2f) { bx = 11.8f; by = 11.75f - (u - 23.6f); } else if (u < 57.8f) { bx = 11.8f - (u - 34.2f); by = 1.15f; } else { bx = -11.8f; by = 1.15f + (u - 57.8f); }
        bool on = ((q + (int)(time * 7)) % 3) == 0;
        Glow(prim.sphere, TRS({ bx, by, wz + 0.75f }, { 0, 0, 0 }, { 0.3f, 0.3f, 0.3f }), on ? Hex(0xfff3c4) : Hex(0x8a6a2a));
      }
      for (int q = 0; q < 2; q++) Glow(prim.torus, TRS({ 0, 6.3f, wz + 0.9f }, { 0, 0, time * (q ? -0.25f : 0.2f) }, { 2.6f + q * 1.3f, 2.6f + q * 1.3f, 0.6f }), q ? Hex(0xd9a441) : Hex(0xfff0c2));
    } else if (var == 2) {
      for (int rg = 0; rg < 6; rg++) {
        float rr = 2.6f + rg * 1.6f; int seg = 14 + rg * 4;
        for (int q = 0; q < seg; q++) {
          float an = PI * (q + 0.5f) / seg, pulse = 0.5f + 0.5f * sinf(time * 2.2f - rg * 0.8f + q * 0.15f);
          Color cc = rg % 2 ? ColorLerp(Shade(tint, -0.5f), Hex(0xf2f6ff), pulse * 0.8f) : ColorLerp(Shade(tint, -0.55f), Shade(tint, 0.25f), pulse);
          Glow(prim.cube, TRS({ cosf(an) * rr * 1.08f, 1.3f + sinf(an) * rr * 0.92f, wz + 0.24f }, { 0, 0, an + PI / 2 }, { PI * rr / seg * 0.82f, 0.2f, 0.06f }), cc);
        }
      }
      for (int q = -5; q <= 5; q++) Glow(prim.cube, TRS({ q * 2.1f, 0.2f + 0.1f * sinf(time * 3 + q), wz + 0.3f }, { 0, 0, 0 }, { 0.14f, 2.2f + 1.2f * (0.5f + 0.5f * sinf(time * 1.7f + q * 1.3f)), 0.06f }), Shade(tint, 0.3f));
    } else
    for (int y = 0; y < 10; y++) for (int x = 0; x < 22; x++) {
      float px = -10.5f + x, py = 1.75f + y, w = sinf(px * 0.35f + py * 0.25f - time * 1.6f) * 0.5f + 0.5f;
      float sweep = expf(-powf(fmodf(px * 0.6f - py * 0.4f - time * 4.5f, 30.f) + 6, 2) * 0.05f);
      Color c = ColorLerp(Hex(0x0e1640), tintD, 0.35f + 0.35f * w); c = ColorLerp(c, Shade(tint, -0.25f), sweep * 0.45f);
      Glow(prim.cube, TRS({ px, py, wz + 0.22f }, { 0, 0, 0 }, { 0.96f, 0.96f, 0.05f }), c);
    }
    // neon bezel around the screen
    Glow(prim.cube, TRS({ 0, 11.65f, wz + 0.25f }, { 0, 0, 0 }, { 23.8f, 0.12f, 0.1f }), Shade(tint, 0.3f));
    Glow(prim.cube, TRS({ 0, 0.95f, wz + 0.25f }, { 0, 0, 0 }, { 23.8f, 0.12f, 0.1f }), Shade(tint, 0.3f));
    for (int q = -1; q <= 1; q += 2) Glow(prim.cube, TRS({ q * 11.85f, 6.3f, wz + 0.25f }, { 0, 0, 0 }, { 0.12f, 10.8f, 0.1f }), Shade(tint, 0.3f));
    // angled side walls with chevron neon stripes
    for (int sd = -1; sd <= 1; sd += 2) {
      lit.Draw(prim.cube, TRS({ sd * 16.5f, 7, wz + 3.2f }, { 0, -sd * 0.6f, 0 }, { 9, 14, 0.4f }), Hex(0x10183c));
      for (int k = 0; k < 4; k++) {
        Matrix m = MatrixMultiply(MatrixMultiply(MatrixScale(0.14f, 7.5f, 0.08f), MatrixRotateZ(sd * 0.55f)), MatrixTranslate(-1.5f * sd + k * 1.1f * sd, 6.5f, 0.25f));
        m = MatrixMultiply(m, MatrixMultiply(MatrixRotateY(-sd * 0.6f), MatrixTranslate(sd * 16.5f, 0, wz + 3.2f)));
        Glow(prim.cube, m, k % 2 ? Hex(0xe9f4ff) : Shade(tint, 0.2f));
      }
    }
    // out-of-focus studio lights (warm + stage colour), slowly drifting
    rlDrawRenderBatchActive(); rlDisableDepthMask(); BeginBlendMode(BLEND_ADDITIVE);
    for (int k = 0; k < 46; k++) {
      float hx = fmodf(k * 7.31f, 1.f), hy = fmodf(k * 3.77f, 1.f), hz = fmodf(k * 5.13f, 1.f);
      float x = (hx - 0.5f) * 62 + sinf(time * 0.2f + k) * 0.6f, y = 1.5f + hy * 13, z = wz + 0.6f + hz * 3.0f;
      if (fabsf(x) < 12.4f && y < 11.9f && y > 0.8f) x += x < 0 ? -13 : 13;   // keep the video wall clean
      if (fabsf(x) > 27) continue;
      float r = 0.22f + hz * 0.5f, tw = 0.55f + 0.45f * sinf(time * (0.8f + hx) + k * 2.1f);
      Color c = k % 3 == 0 ? Shade(tint, 0.35f) : k % 3 == 1 ? Hex(0xffc98a) : Hex(0xfff1d6);
      DrawSphere({ x, y, z }, r, Fade(c, 0.10f * tw)); DrawSphere({ x, y, z }, r * 0.55f, Fade(c, 0.12f * tw));
    }
    BeginBlendMode(BLEND_ALPHA); rlDrawRenderBatchActive(); rlEnableDepthMask();
    // lighting truss with spot cans
    lit.Draw(prim.cube, TRS({ 0, 13.2f, 0 }, { 0, 0, 0 }, { 30, 0.35f, 0.35f }), Hex(0x6c7385));
    lit.Draw(prim.cube, TRS({ 0, 13.8f, 0 }, { 0, 0, 0 }, { 30, 0.35f, 0.35f }), Hex(0x6c7385));
    for (int k = 0; k < 9; k++) {
      float x = -12 + k * 3;
      lit.Draw(prim.cyl, TRS({ x, 12.4f, 0.2f }, { 0.5f, 0, 0 }, { 0.6f, 0.8f, 0.6f }), Hex(0x22252e));
      Glow(prim.sphere, TRS({ x, 12.3f, 0.75f }, { 0, 0, 0 }, { 0.42f, 0.42f, 0.2f }), k % 2 ? Hex(0xfff4dc) : Shade(tint, 0.5f));
    }
    // host desks: dark glossy body, white top, glowing front panel in the player's colour
    float r = PodR();
    for (int i = 0; i < N(); i++) {
      float x = PodX(i); bool h = hot ? hot(i) : false; Color c = P(i).color;
      float hop = cheer[i] > 0 ? fabsf(sinf(time * 9 + i)) * 0.35f * fminf(1, cheer[i]) : 0;
      DrawToon({ x, 0.15f, 1.5f }, 0, c, time + i * 0.7f, hop, fminf(1, cheer[i]), 0.98f * r / 0.78f);
      float dw = r * 2.1f, dd = r * 1.25f, z = 2.35f;
      lit.Draw(prim.cube, TRS({ x, 0.58f, z }, { 0, 0, 0 }, { dw, 1.14f, dd }), Hex(0x161d3c));
      lit.Draw(prim.cube, TRS({ x, 1.17f, z }, { 0, 0, 0 }, { dw + 0.14f, 0.07f, dd + 0.14f }), Hex(0x34407a));
      Color panel = h ? Shade(c, 0.3f) : ColorLerp(Hex(0x101633), c, 0.72f + 0.08f * sinf(time * 2 + i));
      Glow(prim.cube, TRS({ x, 0.58f, z + dd / 2 + 0.005f }, { 0, 0, 0 }, { dw - 0.14f, 0.98f, 0.02f }), panel);
      Glow(prim.cube, TRS({ x, 0.36f, z + dd / 2 + 0.012f }, { 0, 0, 0 }, { dw - 0.3f, 0.46f, 0.02f }), Hex(0x0b1030));   // score window
      Glow(prim.cube, TRS({ x, 1.11f, z + dd / 2 + 0.02f }, { 0, 0, 0 }, { dw + 0.12f, 0.06f, 0.03f }), Shade(c, 0.5f));
      Glow(prim.cube, TRS({ x, 0.03f, z + dd / 2 + 0.02f }, { 0, 0, 0 }, { dw + 0.12f, 0.05f, 0.03f }), Shade(c, 0.5f));
      // answer light on the desk
      lit.Draw(prim.cyl, TRS({ x + dw * 0.32f, 1.2f, z + 0.1f }, { 0, 0, 0 }, { 0.3f, 0.06f, 0.3f }), Hex(0x2a3155));
      if (h) Glow(prim.sphere, TRS({ x + dw * 0.32f, 1.28f, z + 0.1f }, { 0, 0, 0 }, { 0.26f, 0.16f, 0.26f }), Shade(c, 0.6f));
      else lit.Draw(prim.sphere, TRS({ x + dw * 0.32f, 1.28f, z + 0.1f }, { 0, 0, 0 }, { 0.26f, 0.16f, 0.26f }), Hex(0xd9364f));
    }
    Props3D();
    fx.Draw(); conf.Draw();
    // spotlight beams (no depth write, so they never cut into things behind them)
    rlDrawRenderBatchActive(); rlDisableDepthMask();
    BeginBlendMode(BLEND_ADDITIVE);
    for (int s = -1; s <= 1; s += 2) {
      float sw = sinf(time * 0.6f + s) * 2.5f;
      for (int q = 0; q < 3; q++) DrawCylinderEx({ s * 9.f, 12.4f, -3 }, { s * 3.f + sw, 0, 2 }, 0.15f, 3.2f - q * 0.9f, 20, Fade(tint, 0.035f));
      float sw2 = cosf(time * 0.45f + s * 2) * 4;
      for (int q = 0; q < 2; q++) DrawCylinderEx({ s * 12.f, 12.4f, 0 }, { s * 9.f + sw2, 0, -5 }, 0.15f, 2.6f - q * 0.9f, 20, Fade(Shade(tint, 0.4f), 0.03f));
    }
    for (int i = 0; i < N(); i++) {
      float x = PodX(i), rr = PodR();
      for (int q = 0; q < 3; q++) DrawCylinderEx({ x * 0.8f, 12.4f, 0.6f }, { x, 0.12f, 1.75f }, 0.12f, rr * (1.9f - q * 0.45f), 20, Fade(Hex(0xffe3b0), 0.028f));
    }
    for (int i = 0; i < N(); i++) { float x = PodX(i); for (int q = 0; q < 3; q++) DrawCylinder({ x, 0.13f + q * 0.001f, 1.7f }, PodR() * (1.9f - q * 0.45f), PodR() * (1.9f - q * 0.45f), 0.01f, 32, Fade(Hex(0xffd89a), 0.05f)); }
    for (int i = 0; i < N(); i++) { float x = PodX(i), dw = PodR() * 2.1f, z0 = 2.35f + PodR() * 0.625f; for (int k = 0; k < 5; k++) DrawCube({ x, 0.125f + k * 0.001f, z0 + 0.25f + k * 0.18f }, dw - 0.3f - k * 0.1f, 0.002f, 0.5f + k * 0.36f, Fade(P(i).color, 0.035f)); }
    for (int k = 0; k < 6; k++) DrawCube({ 0, 0.005f, wz + 1.2f + k * 0.5f }, 22 - k * 1.5f, 0.002f, 1 + k, Fade(tint, 0.025f));
    BeginBlendMode(BLEND_ALPHA);
    rlDrawRenderBatchActive(); rlEnableDepthMask();
    EndMode3D();
    if (flash > 0) DrawRectangle(0, 0, W, H, Fade(Hex(0xfff3d6), flash * 0.6f));
  }
  // names + scores on the podium fronts, floating "+500"s
  void PodiumLabels(std::function<std::string(int)> tag = nullptr) {
    float r = PodR();
    for (int i = 0; i < N(); i++) {
      float x = PodX(i);
      float zf = 2.35f + r * 0.625f + 0.03f;
      Vector2 a = GetWorldToScreen({ x, 0.86f, zf }, cam), b = GetWorldToScreen({ x, 0.36f, zf }, cam);
      Vector2 e0 = GetWorldToScreen({ x - r * 0.95f, 0.86f, zf }, cam), e1 = GetWorldToScreen({ x + r * 0.95f, 0.86f, zf }, cam);
      float fs = fminf(32, fmaxf(14, (e1.x - e0.x) * 1.75f / fmaxf(4.5f, (float)P(i).name.size())));
      Text(a.x, a.y, fs, WHITE, P(i).name, 0, TS_HEAVY | TS_SHADOW);
      Text(b.x, b.y + 1, fminf(50, (e1.x - e0.x) * 0.36f), Hex(0xffe9a8), kit::I((int)roundf(shown[i].x)), 0, TS_DISPLAY | TS_SHADOW | TS_GOLD);
      if (tag) { std::string t = tag(i); if (!t.empty()) { Vector2 c = GetWorldToScreen({ x, 2.75f * r / 0.78f, 1.55f }, cam); Text(c.x, c.y, 28, WHITE, t, 0, TS_HEAVY | TS_SHADOW); } }
      if (pop[i] > 0) {
        float k = 1.8f - pop[i]; Vector2 c = GetWorldToScreen({ x, 2.9f + k * 0.9f, 1.55f }, cam);
        Text(c.x, c.y, 34 + 10 * EaseBack(fminf(1, k * 4)), Fade(popTxt[i][0] == '-' ? Hex(0xff6b81) : Hex(0xffe066), fminf(1, pop[i] * 1.5f)), popTxt[i], 0, TS_DISPLAY | TS_SHADOW);
      }
    }
  }
};
