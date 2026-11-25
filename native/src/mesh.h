// Real props, modelled at real-world size (metres) from their actual shapes:
// a woven tennis net, a strung racket, a seamed tennis ball, lathe-turned bowling pins / curling stones / bats,
// hockey goals with mesh netting, and a crowd of spectators merged into one mesh (one draw call).
#pragma once
#include "cc.h"
#include <functional>

struct MeshBuilder {
  std::vector<float> v, n, t; std::vector<unsigned char> c;
  void Vert(Vector3 p, Vector3 nn, Color col) { v.insert(v.end(), { p.x, p.y, p.z }); n.insert(n.end(), { nn.x, nn.y, nn.z }); t.insert(t.end(), { 0, 0 }); c.insert(c.end(), { col.r, col.g, col.b, 255 }); }
  void Tri(Vector3 a, Vector3 b, Vector3 d, Vector3 na, Vector3 nb, Vector3 nd, Color col) { Vert(a, na, col); Vert(b, nb, col); Vert(d, nd, col); }
  // revolve a profile (x = radius, y = height) around the Y axis; colour picked per profile point
  void Lathe(const std::vector<Vector2>& prof, int seg, std::function<Color(float y, int i)> col, Matrix xf = MatrixIdentity());
  // a round tube along a path (racket frames, cords, goal posts, wires)
  void Tube(const std::vector<Vector3>& path, float r, int sides, Color col, bool closed = false);
  void Sphere(Vector3 c, float r, Color col, int rings = 8, int segs = 12, Vector3 scale = { 1, 1, 1 });
  void Box(Vector3 c, Vector3 size, Color col);
  Mesh Build();
};
struct RealProps {
  bool ready = false;
  Mesh tennisNet{}, racketFrame{}, racketRest{}, tennisBall{}, pin{}, stone{}, stoneHandle{}, bat{}, hockeyGoal{}, baseball{}, hockeyStick{};
  void Init();
};
extern RealProps props;
// draw a prop mesh with a transform (vertex colours are kept; tint multiplies them)
void DrawProp(const Mesh& m, Matrix xf, Color tint = WHITE);
// a racket placed from grip to head direction: grip at 'hand', pointing along 'dir', strings facing 'face'
Matrix RacketXf(Vector3 hand, Vector3 dir, Vector3 face, float scale = 1);
// spectators sitting on the given seats (x, y, z, facing yaw) — returns one merged mesh
Mesh BuildCrowd(const std::vector<Vector4>& seats, unsigned seed);
