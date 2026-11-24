// Tiny JSON reader (enough for the data the website sends us).
#pragma once
#include <string>
#include <vector>
#include <utility>
#include <cstdlib>

struct J {
  enum T { NUL, BOOL, NUM, STR, ARR, OBJ } t = NUL;
  double n = 0; bool b = false; std::string s; std::vector<J> a; std::vector<std::pair<std::string, J>> o;
  static const J& none() { static J z; return z; }
  const J& operator[](const char* k) const { if (t == OBJ) for (auto& kv : o) if (kv.first == k) return kv.second; return none(); }
  const J& operator[](size_t i) const { return t == ARR && i < a.size() ? a[i] : none(); }
  const J& operator[](int i) const { return (*this)[(size_t)(i < 0 ? 999999 : i)]; }
  size_t size() const { return t == ARR ? a.size() : t == OBJ ? o.size() : 0; }
  float num(float d = 0) const { return t == NUM ? (float)n : t == BOOL ? (b ? 1.f : 0.f) : d; }
  int i(int d = 0) const { return t == NUM ? (int)n : d; }
  bool yes() const { return t == BOOL ? b : t == NUM ? n != 0 : t == STR ? !s.empty() : false; }
  const std::string& str() const { static std::string e; return t == STR ? s : e; }
  bool ok() const { return t != NUL; }

  static J Parse(const char* p) { const char* q = p; J j = Val(q); return j; }
 private:
  static void Ws(const char*& p) { while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++; }
  static void Utf8(std::string& out, unsigned c) {
    if (c < 0x80) out += (char)c;
    else if (c < 0x800) { out += (char)(0xC0 | (c >> 6)); out += (char)(0x80 | (c & 63)); }
    else if (c < 0x10000) { out += (char)(0xE0 | (c >> 12)); out += (char)(0x80 | ((c >> 6) & 63)); out += (char)(0x80 | (c & 63)); }
    else { out += (char)(0xF0 | (c >> 18)); out += (char)(0x80 | ((c >> 12) & 63)); out += (char)(0x80 | ((c >> 6) & 63)); out += (char)(0x80 | (c & 63)); }
  }
  static std::string Str(const char*& p) {
    std::string r; p++;
    while (*p && *p != '"') {
      if (*p == '\\') {
        p++;
        switch (*p) {
          case 'n': r += '\n'; break; case 't': r += '\t'; break; case 'r': break; case 'b': case 'f': break;
          case 'u': {
            unsigned c = (unsigned)strtoul(std::string(p + 1, 4).c_str(), nullptr, 16); p += 4;
            if (c >= 0xD800 && c < 0xDC00 && p[1] == '\\' && p[2] == 'u') { unsigned lo = (unsigned)strtoul(std::string(p + 3, 4).c_str(), nullptr, 16); c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00); p += 6; }
            Utf8(r, c); break;
          }
          default: r += *p;
        }
        p++;
      } else r += *p++;
    }
    if (*p == '"') p++;
    return r;
  }
  static J Val(const char*& p) {
    Ws(p); J j;
    if (*p == '{') {
      j.t = OBJ; p++; Ws(p);
      while (*p && *p != '}') { Ws(p); std::string k = Str(p); Ws(p); if (*p == ':') p++; j.o.emplace_back(k, Val(p)); Ws(p); if (*p == ',') p++; Ws(p); }
      if (*p) p++;
    } else if (*p == '[') {
      j.t = ARR; p++; Ws(p);
      while (*p && *p != ']') { j.a.push_back(Val(p)); Ws(p); if (*p == ',') p++; Ws(p); }
      if (*p) p++;
    } else if (*p == '"') { j.t = STR; j.s = Str(p); }
    else if (*p == 't') { j.t = BOOL; j.b = true; p += 4; }
    else if (*p == 'f') { j.t = BOOL; j.b = false; p += 5; }
    else if (*p == 'n') { p += 4; }
    else { char* e; j.n = strtod(p, &e); if (e != p) j.t = NUM; p = e == p ? p + 1 : e; }
    return j;
  }
};
