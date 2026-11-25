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
  Player& P(int i) { return (*ps)[i]; }
  int N() { return (int)ps->size(); }
  void InitStage() {
    cam.position = { 0, 4.1f, 13.6f }; cam.target = { 0, 3.5f, 0 }; cam.up = { 0, 1, 0 }; cam.fovy = 40; cam.projection = CAMERA_PERSPECTIVE;
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
  void Win() { auto o = Order(); conf.Spawn({ PodX(o[0]), 3, 2 }, 160, 4, 7); Sfx("win"); Banner(P(o[0]).name + " WINS!", 2600, kit::Hx(P(o[0]).color).c_str()); cheer[o[0]] = 4; endT = 4.2f; }
  // call from Update: true once the game is over and finished
  bool Ending(float dt) { if (endT < 0) return false; endT -= dt; if (endT <= 0 && !fin) { fin = true; auto o = Order(); std::string r; for (int i : o) r += kit::I(i) + ":" + kit::I(pts[i]) + " pts;"; Finish(r); } return true; }

  virtual void Props3D() {}  // extra 3D things on stage (drawn inside the scene)
  void Stage(std::function<bool(int)> hot = nullptr) {
    UI::Sky(Hex(0x070d26), Hex(0x1a1250));
    lit.Begin(cam, { 0.05f, 0.06f, 0.16f }, 0.006f);
    BeginMode3D(cam);
    lit.Pattern(PAT_TILE);
    lit.Draw(prim.plane, TRS({ 0, 0, 0 }, { 0, 0, 0 }, { 90, 1, 90 }), Hex(0x18224f));
    lit.Pattern(PAT_NONE);
    // round stage riser with a gold edge
    lit.Draw(prim.cyl, TRS({ 0, -0.3f, 1.2f }, { 0, 0, 0 }, { 10.5f, 0.42f, 5.2f }), Hex(0x223070));
    lit.Draw(prim.cyl, TRS({ 0, -0.31f, 1.2f }, { 0, 0, 0 }, { 10.62f, 0.4f, 5.3f }), Hex(0xc9a24f));
    // back wall: deep navy panels with gold seams
    float wz = -10;
    lit.Draw(prim.cube, TRS({ 0, 8, wz }, { 0, 0, 0 }, { 70, 20, 0.5f }), Hex(0x0e1640));
    for (int i = -9; i <= 9; i++) lit.Draw(prim.cube, TRS({ i * 3.6f, 8, wz + 0.3f }, { 0, 0, 0 }, { 0.08f, 20, 0.05f }), Hex(0x8a6d2f));
    // light-bulb arch
    for (int k = 0; k <= 34; k++) {
      float a = PI * k / 34.f; bool on = ((k + (int)(time * 6)) % 3) != 0;
      Vector3 p = { cosf(a) * 10.5f, 0.6f + sinf(a) * 9.0f, wz + 0.6f };
      lit.Draw(prim.sphere, TRS(p, { 0, 0, 0 }, { 0.32f, 0.32f, 0.32f }), on ? Hex(0xfff1c4) : Hex(0x9a7a3a));
    }
    // side light towers
    for (int s = -1; s <= 1; s += 2) {
      lit.Draw(prim.cube, TRS({ s * 13.5f, 4, -4 }, { 0, 0, 0 }, { 1.2f, 9, 1.2f }), Hex(0x141c46));
      for (int k = 0; k < 6; k++) lit.Draw(prim.sphere, TRS({ s * 13.5f, 1 + k * 1.4f, -3.3f }, { 0, 0, 0 }, { 0.38f, 0.38f, 0.2f }), (k + (int)(time * 3)) % 2 ? Shade(tint, 0.5f) : Hex(0xfff1c4));
    }
    // podiums + characters
    float r = PodR();
    for (int i = 0; i < N(); i++) {
      float x = PodX(i); bool h = hot ? hot(i) : false; Color c = P(i).color;
      float hop = cheer[i] > 0 ? fabsf(sinf(time * 9 + i)) * 0.35f * fminf(1, cheer[i]) : 0;
      DrawToon({ x, 0.15f, 1.55f }, 0, c, time + i * 0.7f, hop, fminf(1, cheer[i]), 0.86f * r / 0.78f);
      lit.Draw(prim.cyl, TRS({ x, 0, 2.35f }, { 0, 0, 0 }, { r, 1.15f, r }), Shade(c, -0.2f));
      lit.Draw(prim.cyl, TRS({ x, 0.32f, 2.35f }, { 0, 0, 0 }, { r * 1.03f, 0.1f, r * 1.03f }), Hex(0xd9b25a));
      lit.Draw(prim.cyl, TRS({ x, 1.15f, 2.35f }, { 0, 0, 0 }, { r * 1.1f, 0.12f, r * 1.1f }), Hex(0xf2c66d));
      // answer light on top of the podium
      lit.Draw(prim.sphere, TRS({ x, 1.33f, 2.35f + r * 0.55f }, { 0, 0, 0 }, { 0.16f, 0.12f, 0.16f }), h ? Shade(c, 0.6f) : Hex(0x2a3155));
    }
    Props3D();
    fx.Draw(); conf.Draw();
    // spotlight beams (no depth write, so they never cut into things behind them)
    rlDrawRenderBatchActive(); rlDisableDepthMask();
    BeginBlendMode(BLEND_ADDITIVE);
    for (int s = -1; s <= 1; s += 2) {
      float sw = sinf(time * 0.6f + s) * 2.5f;
      DrawCylinderEx({ s * 9.f, 12, -3 }, { s * 3.f + sw, 0, 2 }, 0.2f, 2.6f, 18, Fade(tint, 0.06f));
    }
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
      Vector2 a = GetWorldToScreen({ x, 0.78f, 2.35f + r }, cam), b = GetWorldToScreen({ x, 0.42f, 2.35f + r }, cam);
      float fs = fminf(22, fmaxf(13, 150.f / fmaxf(5, (float)P(i).name.size()) * r / 0.78f));
      Text(a.x, a.y, fs, WHITE, P(i).name, 0, TS_HEAVY | TS_SHADOW);
      Text(b.x, b.y, fminf(26, 30 * r / 0.78f), Hex(0xffe9a8), kit::I((int)roundf(shown[i].x)), 0, TS_DISPLAY | TS_SHADOW | TS_GOLD);
      if (tag) { std::string t = tag(i); if (!t.empty()) { Vector2 c = GetWorldToScreen({ x, 2.75f * r / 0.78f, 1.55f }, cam); Text(c.x, c.y, 28, WHITE, t, 0, TS_HEAVY | TS_SHADOW); } }
      if (pop[i] > 0) {
        float k = 1.8f - pop[i]; Vector2 c = GetWorldToScreen({ x, 2.9f + k * 0.9f, 1.55f }, cam);
        Text(c.x, c.y, 34 + 10 * EaseBack(fminf(1, k * 4)), Fade(popTxt[i][0] == '-' ? Hex(0xff6b81) : Hex(0xffe066), fminf(1, pop[i] * 1.5f)), popTxt[i], 0, TS_DISPLAY | TS_SHADOW);
      }
    }
  }
};
