// Real people: rigged, animated character models (Quaternius "Ultimate Modular" men & women, CC0) replace the
// hand-built toon bodies. The website fetches the .glb files and hands them over with cc_put_file.
// Skinning is done here (not with raylib's animation loader, which drops the armature's own transform):
// joint matrix = world(joint) * inverseBind(joint), with the whole node hierarchy animated from the glTF samplers.
#include "cc.h"
#include <functional>
#include "external/cgltf.h"
#include <emscripten/emscripten.h>
#include <map>
#include <cstring>
#include "rlgl.h"

static std::map<std::string, std::vector<unsigned char>>& Files() { static std::map<std::string, std::vector<unsigned char>> f; return f; }
static unsigned char* FromMemory(const char* name, int* size) {
  std::string k = name; size_t s = k.find_last_of("/\\"); if (s != std::string::npos) k = k.substr(s + 1);
  auto it = Files().find(k);
  if (it == Files().end()) { *size = 0; return nullptr; }
  *size = (int)it->second.size();
  unsigned char* d = (unsigned char*)MemAlloc((unsigned)*size); memcpy(d, it->second.data(), it->second.size()); return d;
}
extern "C" EMSCRIPTEN_KEEPALIVE void cc_put_file(const char* name, const unsigned char* data, int size) { Files()[name].assign(data, data + size); }
void InstallFileLoader() { SetLoadFileDataCallback(FromMemory); }
bool HaveFile(const std::string& n) { return Files().count(n) > 0; }

namespace {
Matrix FromGL(const float* a) { Matrix m; m.m0 = a[0]; m.m1 = a[1]; m.m2 = a[2]; m.m3 = a[3]; m.m4 = a[4]; m.m5 = a[5]; m.m6 = a[6]; m.m7 = a[7]; m.m8 = a[8]; m.m9 = a[9]; m.m10 = a[10]; m.m11 = a[11]; m.m12 = a[12]; m.m13 = a[13]; m.m14 = a[14]; m.m15 = a[15]; return m; }
unsigned char ToSRGB(float v) { return (unsigned char)Clamp(255.f * powf(Clamp(v, 0, 1), 1 / 2.2f), 0, 255); }

struct Prim { Mesh mesh{}; std::vector<float> pos, nrm, outP, outN; std::vector<unsigned short> jnt; std::vector<float> wgt; int mat = -1; };
struct Sampler { std::vector<float> in, out; int comps = 3; bool step = false; };
struct Channel { int node, path, sampler; };   // path 0 T, 1 R, 2 S
struct Clip { std::string name; float len = 0; std::vector<Sampler> s; std::vector<Channel> c; };
struct Person {
  bool tried = false, ok = false;
  std::vector<int> parent; std::vector<Vector3> T0, S0; std::vector<Quaternion> R0; std::vector<bool> hasMat; std::vector<Matrix> M0;
  std::vector<int> joints; std::vector<Matrix> invBind; std::vector<Prim> prims; std::vector<Clip> clips; std::vector<Color> mats;
  int idle = -1, walk = -1, run = -1, wave = -1, interact = -1, idle2 = -1;
  int shirt = -1, skin = -1, skin2 = -1, hair = -1; int cKey = -1, cTq = -1;
  float unit = 1, footY = 0;
  std::vector<Vector3> T; std::vector<Quaternion> R; std::vector<Vector3> S; std::vector<Matrix> W, J;
  std::vector<std::string> names; std::vector<char> upper; int head = -1, neck = -1, wristR = -1; std::map<std::string, int> clipCache;
};
Person P[64];   // 0..3: the shared people; 4..63: instances with their own skinned meshes (a crowd that animates at a lower rate)
const int CFG[4][4] = { { 2, 3, 4, 7 }, { 1, 2, -1, 6 }, { 0, 1, -1, 3 }, { 3, 0, -1, -1 } };   // shirt / skin / skin2 / hair glTF material
const char* FILE_[4] = { "p0.glb", "p1.glb", "p2.glb", "p3.glb" };

int ClipIdx(const Person& p, const char* tail) {
  for (size_t i = 0; i < p.clips.size(); i++) { const std::string& n = p.clips[i].name; size_t t = strlen(tail); if (n.size() >= t && n.compare(n.size() - t, t, tail) == 0 && (n.size() == t || n[n.size() - t - 1] == '|')) return (int)i; }
  return -1;
}
void SampleInto(Person& p, int clip, float t, std::vector<Vector3>& T, std::vector<Quaternion>& R, std::vector<Vector3>& S) {
  if (clip < 0) return;
  Clip& c = p.clips[clip]; t = c.len > 0 ? fmodf(fmaxf(t, 0), c.len) : 0;
  for (auto& ch : c.c) {
    Sampler& s = c.s[ch.sampler]; int n = (int)s.in.size(); if (!n) continue;
    int k = 0; while (k + 1 < n && s.in[k + 1] <= t) k++;
    int k2 = std::min(k + 1, n - 1); float f = (k2 == k || s.step) ? 0 : Clamp((t - s.in[k]) / fmaxf(1e-6f, s.in[k2] - s.in[k]), 0, 1);
    const float* a = &s.out[k * s.comps]; const float* b = &s.out[k2 * s.comps];
    if (ch.path == 1) R[ch.node] = QuaternionSlerp({ a[0], a[1], a[2], a[3] }, { b[0], b[1], b[2], b[3] }, f);
    else { Vector3 v = Vector3Lerp({ a[0], a[1], a[2] }, { b[0], b[1], b[2] }, f); if (ch.path == 0) T[ch.node] = v; else S[ch.node] = v; }
  }
}
void Finish(Person& p);
void Pose(Person& p, int clip, float t) { p.T = p.T0; p.R = p.R0; p.S = p.S0; SampleInto(p, clip, t, p.T, p.R, p.S); Finish(p); }
// legs from one clip, upper body blended toward another (a swing while running), and an extra head turn
void PoseEx(Person& p, int a, float ta, int b, float tb, float mix, float hy, float hp) {
  p.T = p.T0; p.R = p.R0; p.S = p.S0; SampleInto(p, a, ta, p.T, p.R, p.S);
  if (b >= 0 && mix > 0.001f) {
    static std::vector<Vector3> T2, S2; static std::vector<Quaternion> R2; T2 = p.T0; R2 = p.R0; S2 = p.S0; SampleInto(p, b, tb, T2, R2, S2);
    for (size_t i = 0; i < p.R.size() && i < p.upper.size(); i++) if (p.upper[i]) p.R[i] = QuaternionSlerp(p.R[i], R2[i], Clamp(mix, 0, 1));
  }
  if (p.head >= 0 && (hy != 0 || hp != 0)) {
    p.R[p.head] = QuaternionMultiply(p.R[p.head], QuaternionMultiply(QuaternionFromAxisAngle({ 0, 1, 0 }, hy * 0.6f), QuaternionFromAxisAngle({ 1, 0, 0 }, hp * 0.6f)));
    if (p.neck >= 0) p.R[p.neck] = QuaternionMultiply(p.R[p.neck], QuaternionMultiply(QuaternionFromAxisAngle({ 0, 1, 0 }, hy * 0.4f), QuaternionFromAxisAngle({ 1, 0, 0 }, hp * 0.4f)));
  }
  Finish(p);
}
void Finish(Person& p) {
  size_t n = p.parent.size(); p.W.resize(n);
  // nodes are processed parents-first (the load step ordered them so)
  for (size_t i = 0; i < n; i++) {
    Matrix L = p.hasMat[i] ? p.M0[i] : MatrixMultiply(MatrixMultiply(MatrixScale(p.S[i].x, p.S[i].y, p.S[i].z), QuaternionToMatrix(p.R[i])), MatrixTranslate(p.T[i].x, p.T[i].y, p.T[i].z));
    p.W[i] = p.parent[i] >= 0 ? MatrixMultiply(L, p.W[p.parent[i]]) : L;
  }
  p.J.resize(p.joints.size());
  for (size_t j = 0; j < p.joints.size(); j++) p.J[j] = MatrixMultiply(p.invBind[j], p.W[p.joints[j]]);
}
void Skin(Person& p, bool upload) {
  for (auto& pr : p.prims) {
    size_t vc = pr.pos.size() / 3;
    for (size_t v = 0; v < vc; v++) {
      Vector3 a = { pr.pos[v * 3], pr.pos[v * 3 + 1], pr.pos[v * 3 + 2] }, nn = { pr.nrm[v * 3], pr.nrm[v * 3 + 1], pr.nrm[v * 3 + 2] }, o = { 0, 0, 0 }, on = { 0, 0, 0 };
      for (int k = 0; k < 4; k++) {
        float w = pr.wgt[v * 4 + k]; if (w <= 0) continue; const Matrix& m = p.J[pr.jnt[v * 4 + k]];
        o.x += w * (m.m0 * a.x + m.m4 * a.y + m.m8 * a.z + m.m12); o.y += w * (m.m1 * a.x + m.m5 * a.y + m.m9 * a.z + m.m13); o.z += w * (m.m2 * a.x + m.m6 * a.y + m.m10 * a.z + m.m14);
        on.x += w * (m.m0 * nn.x + m.m4 * nn.y + m.m8 * nn.z); on.y += w * (m.m1 * nn.x + m.m5 * nn.y + m.m9 * nn.z); on.z += w * (m.m2 * nn.x + m.m6 * nn.y + m.m10 * nn.z);
      }
      float l = sqrtf(on.x * on.x + on.y * on.y + on.z * on.z); if (l > 1e-6f) { on.x /= l; on.y /= l; on.z /= l; }
      pr.outP[v * 3] = o.x; pr.outP[v * 3 + 1] = o.y; pr.outP[v * 3 + 2] = o.z; pr.outN[v * 3] = on.x; pr.outN[v * 3 + 1] = on.y; pr.outN[v * 3 + 2] = on.z;
    }
    if (upload) { rlUpdateVertexBuffer(pr.mesh.vboId[0], pr.outP.data(), (int)(vc * 3 * sizeof(float)), 0); rlUpdateVertexBuffer(pr.mesh.vboId[2], pr.outN.data(), (int)(vc * 3 * sizeof(float)), 0); }
  }
}
bool Load(int v, int var = -1) {
  if (var < 0) var = v % 4;
  Person& p = P[v];
  if (p.tried) return p.ok;
  auto it = Files().find(FILE_[var]); if (it == Files().end()) return false;
  p.tried = true;
  cgltf_options opt{}; cgltf_data* d = nullptr;
  if (cgltf_parse(&opt, it->second.data(), it->second.size(), &d) != cgltf_result_success) return false;
  if (cgltf_load_buffers(&opt, d, nullptr) != cgltf_result_success || d->skins_count == 0) { cgltf_free(d); return false; }
  // order nodes parents-first
  std::vector<cgltf_node*> order; std::map<cgltf_node*, int> idx;
  std::function<void(cgltf_node*)> add = [&](cgltf_node* n) { idx[n] = (int)order.size(); order.push_back(n); for (size_t c = 0; c < n->children_count; c++) add(n->children[c]); };
  for (size_t i = 0; i < d->nodes_count; i++) if (!d->nodes[i].parent) add(&d->nodes[i]);
  size_t N = order.size(); p.parent.assign(N, -1); p.T0.resize(N); p.R0.resize(N); p.S0.resize(N); p.hasMat.assign(N, false); p.M0.resize(N);
  for (size_t i = 0; i < N; i++) {
    cgltf_node* n = order[i]; p.parent[i] = n->parent ? idx[n->parent] : -1;
    p.T0[i] = n->has_translation ? Vector3{ n->translation[0], n->translation[1], n->translation[2] } : Vector3{ 0, 0, 0 };
    p.R0[i] = n->has_rotation ? Quaternion{ n->rotation[0], n->rotation[1], n->rotation[2], n->rotation[3] } : Quaternion{ 0, 0, 0, 1 };
    p.S0[i] = n->has_scale ? Vector3{ n->scale[0], n->scale[1], n->scale[2] } : Vector3{ 1, 1, 1 };
    if (n->has_matrix) { p.hasMat[i] = true; p.M0[i] = FromGL(n->matrix); }
  }
  p.names.resize(N); for (size_t i = 0; i < N; i++) p.names[i] = order[i]->name ? order[i]->name : "";
  {  // which nodes are the upper body (a second clip can drive them), and the head / neck / right wrist
    static const char* UP[] = { "Abdomen", "Torso", "Chest", "Neck", "Head", "Shoulder", "UpperArm", "LowerArm", "Wrist", "Index", "Middle", "Ring", "Pinky", "Thumb" };
    p.upper.assign(N, 0);
    for (size_t i = 0; i < N; i++) {
      for (auto u : UP) if (p.names[i].rfind(u, 0) == 0) p.upper[i] = 1;
      if (p.names[i] == "Head") p.head = (int)i; else if (p.names[i] == "Neck") p.neck = (int)i; else if (p.names[i] == "Wrist.R") p.wristR = (int)i;
    }
  }
  cgltf_skin* sk = &d->skins[0];
  for (size_t j = 0; j < sk->joints_count; j++) { p.joints.push_back(idx[sk->joints[j]]); float m[16]; cgltf_accessor_read_float(sk->inverse_bind_matrices, j, m, 16); p.invBind.push_back(FromGL(m)); }
  for (size_t i = 0; i < d->materials_count; i++) { const float* c = d->materials[i].pbr_metallic_roughness.base_color_factor; p.mats.push_back({ ToSRGB(c[0]), ToSRGB(c[1]), ToSRGB(c[2]), 255 }); }
  for (size_t mi = 0; mi < d->meshes_count; mi++) for (size_t pi = 0; pi < d->meshes[mi].primitives_count; pi++) {
    cgltf_primitive& cp = d->meshes[mi].primitives[pi]; Prim pr; pr.mat = cp.material ? (int)(cp.material - d->materials) : -1;
    cgltf_accessor *ap = nullptr, *an = nullptr, *aj = nullptr, *aw = nullptr;
    for (size_t a = 0; a < cp.attributes_count; a++) { auto& at = cp.attributes[a];
      if (at.type == cgltf_attribute_type_position) ap = at.data; else if (at.type == cgltf_attribute_type_normal) an = at.data;
      else if (at.type == cgltf_attribute_type_joints && at.index == 0) aj = at.data; else if (at.type == cgltf_attribute_type_weights && at.index == 0) aw = at.data; }
    if (!ap || !aj || !aw) continue;
    size_t vc = ap->count; pr.pos.resize(vc * 3); pr.nrm.assign(vc * 3, 0); pr.jnt.resize(vc * 4); pr.wgt.resize(vc * 4);
    for (size_t k = 0; k < vc; k++) {
      cgltf_accessor_read_float(ap, k, &pr.pos[k * 3], 3); if (an) cgltf_accessor_read_float(an, k, &pr.nrm[k * 3], 3);
      cgltf_uint jj[4] = { 0, 0, 0, 0 }; cgltf_accessor_read_uint(aj, k, jj, 4); for (int q = 0; q < 4; q++) pr.jnt[k * 4 + q] = (unsigned short)std::min<cgltf_uint>(jj[q], (cgltf_uint)p.joints.size() - 1);
      float ww[4] = { 0, 0, 0, 0 }; cgltf_accessor_read_float(aw, k, ww, 4); float s = ww[0] + ww[1] + ww[2] + ww[3]; for (int q = 0; q < 4; q++) pr.wgt[k * 4 + q] = s > 0 ? ww[q] / s : (q == 0 ? 1.f : 0.f);
    }
    pr.outP = pr.pos; pr.outN = pr.nrm;
    Mesh& m = pr.mesh; m.vertexCount = (int)vc;
    m.vertices = (float*)MemAlloc((unsigned)(vc * 3 * sizeof(float))); memcpy(m.vertices, pr.pos.data(), vc * 3 * sizeof(float));
    m.normals = (float*)MemAlloc((unsigned)(vc * 3 * sizeof(float))); memcpy(m.normals, pr.nrm.data(), vc * 3 * sizeof(float));
    m.texcoords = (float*)MemAlloc((unsigned)(vc * 2 * sizeof(float)));
    if (cp.indices) {
      size_t ic = cp.indices->count; m.triangleCount = (int)(ic / 3); m.indices = (unsigned short*)MemAlloc((unsigned)(ic * sizeof(unsigned short)));
      for (size_t k = 0; k < ic; k++) m.indices[k] = (unsigned short)cgltf_accessor_read_index(cp.indices, k);
    } else m.triangleCount = (int)(vc / 3);
    UploadMesh(&m, true);
    p.prims.push_back(std::move(pr));
  }
  for (size_t ai = 0; ai < d->animations_count; ai++) {
    cgltf_animation& A = d->animations[ai]; Clip c; c.name = A.name ? A.name : "";
    for (size_t si = 0; si < A.samplers_count; si++) {
      cgltf_animation_sampler& s = A.samplers[si]; Sampler o; o.step = s.interpolation == cgltf_interpolation_type_step;
      o.comps = (int)cgltf_num_components(s.output->type); o.in.resize(s.input->count); o.out.resize(s.output->count * o.comps);
      for (size_t k = 0; k < s.input->count; k++) { cgltf_accessor_read_float(s.input, k, &o.in[k], 1); c.len = fmaxf(c.len, o.in[k]); }
      for (size_t k = 0; k < s.output->count; k++) cgltf_accessor_read_float(s.output, k, &o.out[k * o.comps], o.comps);
      if (s.interpolation == cgltf_interpolation_type_cubic_spline) {   // keep the value of each key (in-tangent, value, out-tangent triples)
        std::vector<float> v; for (size_t k = 0; k + 2 < s.output->count; k += 3) v.insert(v.end(), &o.out[(k + 1) * o.comps], &o.out[(k + 1) * o.comps] + o.comps); o.out = v;
      }
      c.s.push_back(std::move(o));
    }
    for (size_t ci = 0; ci < A.channels_count; ci++) {
      cgltf_animation_channel& ch = A.channels[ci]; if (!ch.target_node || !idx.count(ch.target_node)) continue;
      int path = ch.target_path == cgltf_animation_path_type_translation ? 0 : ch.target_path == cgltf_animation_path_type_rotation ? 1 : ch.target_path == cgltf_animation_path_type_scale ? 2 : -1;
      if (path < 0) continue; c.c.push_back({ idx[ch.target_node], path, (int)(ch.sampler - A.samplers) });
    }
    p.clips.push_back(std::move(c));
  }
  cgltf_free(d);
  if (p.prims.empty()) return false;
  p.idle = ClipIdx(p, "Idle"); p.walk = ClipIdx(p, "Walk"); p.run = ClipIdx(p, "Run"); p.wave = ClipIdx(p, "Wave"); p.interact = ClipIdx(p, "Interact"); p.idle2 = ClipIdx(p, "Idle_Neutral");
  p.shirt = CFG[var][0]; p.skin = CFG[var][1]; p.skin2 = CFG[var][2]; p.hair = CFG[var][3];
  // measure the standing height once
  Pose(p, p.idle, 0); Skin(p, false);
  float y0 = 1e9f, y1 = -1e9f; for (auto& pr : p.prims) for (size_t k = 1; k < pr.outP.size(); k += 3) { y0 = fminf(y0, pr.outP[k]); y1 = fmaxf(y1, pr.outP[k]); }
  if (y1 - y0 < 1e-6f) return false;
  p.unit = 1.0f / (y1 - y0); p.footY = y0;
  p.ok = true; return true;
}
}  // namespace

// draw one real person; false = models not loaded yet (caller falls back to the toon body)
bool DrawPerson(Vector3 pos, float yaw, Color c, float t, float hop, float cheer, float sc, float k, unsigned hsh) {
  int v = (int)(hsh % 4);
  if (!Load(v)) { bool any = false; for (int q = 0; q < 4 && !any; q++) if (Load(q)) { v = q; any = true; } if (!any) return false; }
  Person& p = P[v];
  bool flash = c.r == 255 && c.g == 255 && c.b == 255, sport = k <= 1.01f, moving = hop > 0.02f;
  int epid = PidOfColor(c); if (epid >= 0 && epid < 8 && g_emoteT[epid] > 0) cheer = fmaxf(cheer, 1.f);   // an emote from their phone: they wave
  int a = p.idle; float tt = t + (float)(hsh % 97) * 0.13f;
  if (cheer > 0.3f && !sport && p.wave >= 0) a = p.wave;
  else if (sport && cheer > 0.04f && p.interact >= 0) { a = p.interact; tt = Clamp(cheer, 0, 1) * p.clips[a].len * 0.999f; }
  else if (moving) { a = hop > 0.12f && p.run >= 0 ? p.run : (p.walk >= 0 ? p.walk : p.idle); tt *= 1.1f; }
  else if (p.idle2 >= 0 && ((hsh >> 3) & 1)) a = p.idle2;
  Pose(p, a, tt); Skin(p, true);
  float H = (sport ? 1.78f : 2.1f) * sc * k, s = H * p.unit;
  if (epid >= 0 && epid < 8) { g_emotePos[epid] = { pos.x, pos.y + H * 1.06f, pos.z }; g_emoteVis[epid] = true; }
  float lift = cheer > 0.3f && !sport ? hop * 0.35f : 0;
  Matrix xf = MatrixMultiply(MatrixMultiply(MatrixTranslate(0, -p.footY, 0), MatrixScale(s, s, s)), MatrixMultiply(MatrixRotateY(yaw), MatrixTranslate(pos.x, pos.y + lift, pos.z)));
  static const Color SK[5] = { Hex(0xf1c7a3), Hex(0xd9a47a), Hex(0xb57a52), Hex(0x7d4e33), Hex(0xf5d5bd) };
  static const Color HR[6] = { Hex(0x3a2516), Hex(0x6b4423), Hex(0xd9a441), Hex(0x221a16), Hex(0x9c4a22), Hex(0x2e2e34) };
  Color skin = SK[(hsh / 5) % 5], hair = HR[(hsh / 11) % 6];
  lit.Caster(pos, 0.45f * H / 1.95f);
  for (auto& pr : p.prims) {
    int mi = pr.mat; Color col = mi >= 0 && mi < (int)p.mats.size() ? p.mats[mi] : WHITE;
    if (mi == p.shirt) col = c; else if (mi == p.skin) col = skin; else if (mi == p.skin2) col = Shade(skin, -0.15f); else if (mi == p.hair) col = hair;
    if (flash) col = WHITE;
    lit.Draw(pr.mesh, xf, col);
  }
  return true;
}

// a posed person for sports and officials: any clip by name (looping or scrubbed), the upper body blended toward a
// second clip, a head turn, and the right wrist's world matrix out (so a racket sits in the hand)
static int ClipByName(Person& p, const char* name) {
  if (!name) return -1; auto it = p.clipCache.find(name); if (it != p.clipCache.end()) return it->second;
  int i = ClipIdx(p, name); p.clipCache[name] = i; return i;
}
bool DrawPersonPose(const PersonPose& q) {
  int v = q.slot >= 4 && q.slot < 64 ? q.slot : (int)(q.hsh % 4), var = (int)(q.hsh % 4);
  if (!Load(v, var)) { bool any = false; for (int k = 0; k < 4 && !any; k++) if (Load(k)) { v = k; any = true; } if (!any) return false; }
  Person& p = P[v];
  int a = ClipByName(p, q.clip); if (a < 0) a = p.idle;
  float ta = a >= 0 && q.scrub ? Clamp(q.t, 0, 1) * p.clips[a].len * 0.999f : q.t;
  int b = ClipByName(p, q.clip2); float tb = b >= 0 && q.scrub2 ? Clamp(q.t2, 0, 1) * p.clips[b].len * 0.999f : q.t2;
  // a crowd member with its own mesh only re-poses ~14 times a second (and only when its pose really changed)
  int key = a * 131 + b * 17 + (int)(q.mix * 8) + (q.scrub ? 1000 : 0) + (q.scrub2 ? 2000 : 0) + (int)((q.headYaw + 3) * 5) * 7 + (int)((q.headPitch + 3) * 5) * 3, tq = (int)((ta + tb * 3.7f) * 14);
  bool reuse = q.lowRate && v >= 4 && p.cKey == key && p.cTq == tq;
  if (!reuse) { PoseEx(p, a, ta, b, tb, q.mix, q.headYaw, q.headPitch); Skin(p, true); p.cKey = key; p.cTq = tq; }
  float s = q.height * p.unit;
  Matrix xf = MatrixMultiply(MatrixMultiply(MatrixTranslate(0, -p.footY, 0), MatrixScale(s, s, s)), MatrixMultiply(MatrixRotateY(q.yaw), MatrixTranslate(q.pos.x, q.pos.y, q.pos.z)));
  static const Color SK[5] = { Hex(0xf1c7a3), Hex(0xd9a47a), Hex(0xb57a52), Hex(0x7d4e33), Hex(0xf5d5bd) };
  static const Color HR[6] = { Hex(0x3a2516), Hex(0x6b4423), Hex(0xd9a441), Hex(0x221a16), Hex(0x9c4a22), Hex(0x2e2e34) };
  Color skin = SK[(q.hsh / 5) % 5], hair = HR[(q.hsh / 11) % 6];
  lit.Caster(q.pos, 0.45f * q.height / 1.95f);
  for (auto& pr : p.prims) {
    int mi = pr.mat; Color col = mi >= 0 && mi < (int)p.mats.size() ? p.mats[mi] : WHITE;
    if (mi == p.shirt) col = q.c; else if (mi == p.skin) col = skin; else if (mi == p.skin2) col = Shade(skin, -0.15f); else if (mi == p.hair) col = hair;
    lit.Draw(pr.mesh, xf, col);
  }
  if (q.wrist && p.wristR >= 0) *q.wrist = MatrixMultiply(p.W[p.wristR], xf);
  if (q.headM && p.head >= 0) *q.headM = MatrixMultiply(p.W[p.head], xf);
  return true;
}
// ---------- static props from .glb files (furniture, plants, lamps) ----------
namespace {
struct PropPart { Mesh m{}; Matrix node; Color c; };
struct PropModel { bool tried = false, ok = false; std::vector<PropPart> parts; Vector3 mn{ 1e9f, 1e9f, 1e9f }, mx{ -1e9f, -1e9f, -1e9f }; };
std::map<std::string, PropModel>& Props() { static std::map<std::string, PropModel> m; return m; }
bool LoadPropModel(const std::string& name, PropModel& pm) {
  if (pm.tried) return pm.ok;
  auto it = Files().find(name); if (it == Files().end()) return false;
  pm.tried = true;
  cgltf_options opt{}; cgltf_data* d = nullptr;
  if (cgltf_parse(&opt, it->second.data(), it->second.size(), &d) != cgltf_result_success) return false;
  if (cgltf_load_buffers(&opt, d, nullptr) != cgltf_result_success) { cgltf_free(d); return false; }
  for (size_t ni = 0; ni < d->nodes_count; ni++) {
    cgltf_node* n = &d->nodes[ni]; if (!n->mesh) continue;
    float wm[16]; cgltf_node_transform_world(n, wm); Matrix W = FromGL(wm);
    for (size_t pi = 0; pi < n->mesh->primitives_count; pi++) {
      cgltf_primitive& cp = n->mesh->primitives[pi]; cgltf_accessor *ap = nullptr, *an = nullptr;
      for (size_t a = 0; a < cp.attributes_count; a++) { if (cp.attributes[a].type == cgltf_attribute_type_position) ap = cp.attributes[a].data; else if (cp.attributes[a].type == cgltf_attribute_type_normal) an = cp.attributes[a].data; }
      if (!ap) continue;
      PropPart pp; pp.node = W; Mesh& m = pp.m; size_t vc = ap->count; m.vertexCount = (int)vc;
      m.vertices = (float*)MemAlloc((unsigned)(vc * 3 * sizeof(float))); m.normals = (float*)MemAlloc((unsigned)(vc * 3 * sizeof(float))); m.texcoords = (float*)MemAlloc((unsigned)(vc * 2 * sizeof(float)));
      for (size_t k = 0; k < vc; k++) {
        cgltf_accessor_read_float(ap, k, &m.vertices[k * 3], 3); if (an) cgltf_accessor_read_float(an, k, &m.normals[k * 3], 3); else { m.normals[k * 3 + 1] = 1; }
        Vector3 w = Vector3Transform({ m.vertices[k * 3], m.vertices[k * 3 + 1], m.vertices[k * 3 + 2] }, W); pm.mn = Vector3Min(pm.mn, w); pm.mx = Vector3Max(pm.mx, w);
      }
      if (cp.indices) { size_t ic = cp.indices->count; m.triangleCount = (int)(ic / 3); m.indices = (unsigned short*)MemAlloc((unsigned)(ic * sizeof(unsigned short))); for (size_t k = 0; k < ic; k++) m.indices[k] = (unsigned short)cgltf_accessor_read_index(cp.indices, k); }
      else m.triangleCount = (int)(vc / 3);
      {  // a textured material: sample the picture at every vertex and keep it as the vertex colour
        cgltf_accessor* at = nullptr; for (size_t a = 0; a < cp.attributes_count; a++) if (cp.attributes[a].type == cgltf_attribute_type_texcoord && cp.attributes[a].index == 0) at = cp.attributes[a].data;
        cgltf_texture* tx = cp.material ? cp.material->pbr_metallic_roughness.base_color_texture.texture : nullptr;
        if (at && tx && tx->image && tx->image->buffer_view) {
          cgltf_buffer_view* bv = tx->image->buffer_view; const unsigned char* data = (const unsigned char*)bv->buffer->data + bv->offset;
          const char* ext = tx->image->mime_type && strstr(tx->image->mime_type, "jpeg") ? ".jpg" : ".png";
          Image img = LoadImageFromMemory(ext, data, (int)bv->size);
          if (img.data && img.width > 0) {
            m.colors = (unsigned char*)MemAlloc((unsigned)(vc * 4));
            for (size_t k = 0; k < vc; k++) { float uv[2] = { 0, 0 }; cgltf_accessor_read_float(at, k, uv, 2); float u = uv[0] - floorf(uv[0]), v = uv[1] - floorf(uv[1]);
              Color c = GetImageColor(img, Clamp((int)(u * img.width), 0, img.width - 1), Clamp((int)(v * img.height), 0, img.height - 1)); m.colors[k * 4] = c.r; m.colors[k * 4 + 1] = c.g; m.colors[k * 4 + 2] = c.b; m.colors[k * 4 + 3] = 255; }
          }
          if (img.data) UnloadImage(img);
        }
      }
      UploadMesh(&m, false);
      const float* c = cp.material ? cp.material->pbr_metallic_roughness.base_color_factor : nullptr;
      pp.c = c ? Color{ ToSRGB(c[0]), ToSRGB(c[1]), ToSRGB(c[2]), 255 } : WHITE;
      pm.parts.push_back(pp);
    }
  }
  cgltf_free(d);
  pm.ok = !pm.parts.empty() && pm.mx.y > pm.mn.y; return pm.ok;
}
}  // namespace
// draw a model standing on 'pos' (its base on the ground), turned by yaw, scaled to 'height'.
// tint multiplies the model's own colours; force (if its alpha > 0) replaces them. False if not loaded yet.
bool DrawModel(const char* file, Vector3 pos, float yaw, float height, Color tint, Color force) {
  PropModel& pm = Props()[file];
  if (!LoadPropModel(file, pm)) return false;
  float s = height / (pm.mx.y - pm.mn.y);
  Matrix xf = MatrixMultiply(MatrixMultiply(MatrixTranslate(-(pm.mn.x + pm.mx.x) / 2, -pm.mn.y, -(pm.mn.z + pm.mx.z) / 2), MatrixScale(s, s, s)), MatrixMultiply(MatrixRotateY(yaw), MatrixTranslate(pos.x, pos.y, pos.z)));
  for (auto& p : pm.parts) {
    Color c = force.a ? force : Color{ (unsigned char)(p.c.r * tint.r / 255), (unsigned char)(p.c.g * tint.g / 255), (unsigned char)(p.c.b * tint.b / 255), 255 };
    lit.Draw(p.m, MatrixMultiply(p.node, xf), c);
  }
  return true;
}
