// RabbitMesh Flock hunt: the camera map. Kept apart from the radio and the SD card so the host
// tests run it against the real data file (test/alpr_core_test.cpp).
//
// Where the cameras are comes from OpenStreetMap: ALPRs mapped by the DeFlock community
// (man_made=surveillance + surveillance:type=ALPR), published once a day as one static
// GeoJSON file by the deflock-data project (github.com/flockhopper3/deflock-data, MIT):
//   https://data.dontgetflocked.com/cameras.geojson.gz      (US; ...cameras-ca... for Canada)
// served gzip-compressed (~4 MB for the US, ~39 MB unpacked). No API, no key: a T-Deck fetches
// that one file at most once a day, and tools/alpr_extract.py does the same on a computer.
// Map data (c) OpenStreetMap contributors, ODbL.
//
// On the SD card it becomes one compact file, /rabbit/alpr/cameras.bin, the same from the
// T-Deck and from the script:
//   header   "RMALPR1\0", u32 count, u32 cells, u32 built (unix time), u32 reserved   (24 bytes)
//   cells    {i32 key, u32 first, u32 n} * cells, sorted by key                      (12 bytes each)
//   records  Record * count, grouped by cell in key order                            (32 bytes each)
// A cell is a 1 x 1 degree square; the T-Deck loads only the squares around it.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>
#include <algorithm>

#if defined(ESP_PLATFORM)
#include <esp32s3/rom/miniz.h>          // the ROM's inflate
#else
#include "miniz.h"                      // test/third_party/miniz
#endif

namespace alprcore {

// ---- records ----------------------------------------------------------------------------------
enum Brand : uint8_t { B_UNKNOWN = 0, B_FLOCK, B_MOTOROLA, B_GENETEC, B_AXIS, B_LEONARDO, B_REKOR, B_AXON, B_OTHER };
inline uint8_t brandCode(const char* s) {
  if (!s || !*s) return B_UNKNOWN;
  struct { const char* key; uint8_t code; } T[] = {{"flock", B_FLOCK}, {"motorola", B_MOTOROLA}, {"genetec", B_GENETEC},
      {"axis", B_AXIS}, {"leonardo", B_LEONARDO}, {"rekor", B_REKOR}, {"axon", B_AXON}};
  char low[24];
  size_t i = 0;
  for (; s[i] && i < sizeof low - 1; i++) low[i] = (char)((s[i] >= 'A' && s[i] <= 'Z') ? s[i] + 32 : s[i]);
  low[i] = 0;
  for (auto& t : T) if (strstr(low, t.key)) return t.code;
  return B_OTHER;
}
inline const char* brandName(uint8_t b) {
  static const char* N[] = {"unknown", "Flock Safety", "Motorola", "Genetec", "Axis", "Leonardo", "Rekor", "Axon", "other"};
  return b <= B_OTHER ? N[b] : "other";
}

#pragma pack(push, 1)
struct Record {
  float   lat, lon;
  int16_t dir;            // the way it faces, degrees 0-359; -1 not mapped
  uint8_t brand;          // Brand
  uint8_t flags;          // reserved
  char    op[20];         // operator, trimmed
};
struct CellEntry { int32_t key; uint32_t first, n; };
#pragma pack(pop)
static_assert(sizeof(Record) == 32, "Record is 32 bytes on the card");
static_assert(sizeof(CellEntry) == 12, "CellEntry is 12 bytes on the card");
constexpr char MAGIC[8] = {'R', 'M', 'A', 'L', 'P', 'R', '1', 0};
constexpr size_t HEADER_BYTES = 24;

inline int32_t cellKey(double lat, double lon) {
  return (int32_t)(floor(lat) + 90) * 360 + (int32_t)(floor(lon) + 180);
}

// ---- geometry ---------------------------------------------------------------------------------
inline double metres(double la1, double lo1, double la2, double lo2) {
  const double R = 6371000.0, k = 3.14159265358979 / 180.0;
  const double dla = (la2 - la1) * k, dlo = (lo2 - lo1) * k;
  const double a = sin(dla / 2) * sin(dla / 2) + cos(la1 * k) * cos(la2 * k) * sin(dlo / 2) * sin(dlo / 2);
  return 2 * R * atan2(sqrt(a), sqrt(1 - a));
}
inline double bearing(double la1, double lo1, double la2, double lo2) {
  const double k = 3.14159265358979 / 180.0;
  const double y = sin((lo2 - lo1) * k) * cos(la2 * k);
  const double x = cos(la1 * k) * sin(la2 * k) - sin(la1 * k) * cos(la2 * k) * cos((lo2 - lo1) * k);
  double b = atan2(y, x) / k;
  return b < 0 ? b + 360 : b;
}

// ---- the GeoJSON, one feature at a time --------------------------------------------------------
// Fed the file in whatever pieces it arrives in; for each feature it pulls out the point, the
// brand, the operator and the facing direction. A feature is at most ~530 characters in this
// file; anything longer than the buffer is skipped (counted), never half-read.
struct FeatureScanner {
  char buf[1024];
  size_t len = 0;
  int depth = 0;
  bool inStr = false, esc = false, capturing = false, overflow = false;
  uint32_t features = 0, skipped = 0;

  template <class F>
  void feed(const char* p, size_t n, F&& onRecord) {
    for (size_t i = 0; i < n; i++) {
      const char c = p[i];
      if (capturing) {
        if (len < sizeof buf - 1) buf[len++] = c; else overflow = true;
      }
      if (inStr) {
        if (esc) esc = false;
        else if (c == '\\') esc = true;
        else if (c == '"') inStr = false;
        continue;
      }
      if (c == '"') { inStr = true; continue; }
      if (c == '{' || c == '[') {
        depth++;
        if (c == '{' && depth == 3 && !capturing) { capturing = true; len = 0; overflow = false; buf[len++] = c; }
      } else if (c == '}' || c == ']') {
        if (capturing && depth == 3 && c == '}') {
          capturing = false;
          buf[len] = 0;
          features++;
          Record r;
          if (!overflow && parse(buf, r)) onRecord(r); else skipped++;
        }
        depth--;
      }
    }
  }

  // A JSON string value after "key": copied (escapes reduced to their character), trimmed.
  static bool str(const char* s, const char* key, char* out, size_t cap) {
    const char* k = strstr(s, key);
    if (!k) return false;
    k += strlen(key);
    while (*k == ' ') k++;
    if (*k != '"') return false;
    k++;
    size_t w = 0;
    for (; *k && *k != '"'; k++) {
      char c = *k;
      if (c == '\\' && k[1]) { k++; c = *k == 'n' || *k == 't' ? ' ' : *k; if (*k == 'u') { c = '?'; k += 4; } }
      if ((unsigned char)c < 32) c = ' ';
      if (w + 1 < cap) out[w++] = c;
    }
    out[w] = 0;
    return true;
  }
  static bool parse(const char* s, Record& r) {
    const char* c = strstr(s, "\"coordinates\":");
    if (!c) return false;
    c = strchr(c, '[');
    if (!c) return false;
    char* end;
    const double lon = strtod(c + 1, &end);
    if (end == c + 1 || *end != ',') return false;
    const double lat = strtod(end + 1, &end);
    if (lat < -90 || lat > 90 || lon < -180 || lon > 180 || (lat == 0 && lon == 0)) return false;
    memset(&r, 0, sizeof r);
    r.lat = (float)lat; r.lon = (float)lon;
    char tmp[64];
    r.brand = str(s, "\"brand\":", tmp, sizeof tmp) ? brandCode(tmp) : (uint8_t)B_UNKNOWN;
    if (str(s, "\"operator\":", tmp, sizeof tmp)) {
      const size_t k = strlen(tmp) < sizeof r.op - 1 ? strlen(tmp) : sizeof r.op - 1;
      memcpy(r.op, tmp, k);
      r.op[k] = 0;
    }
    r.dir = -1;
    const char* d = strstr(s, "\"direction\":");
    if (d) {
      const double v = strtod(d + 12, &end);
      if (end != d + 12) r.dir = (int16_t)(((int)lround(v) % 360 + 360) % 360);
    }
    return true;
  }
};

// ---- gzip, unpacked as it streams in ------------------------------------------------------------
// The gzip wrapper (RFC 1952: a 10-byte header and its optional fields) by hand, the deflate
// stream by tinfl, out through a 32 KB window. Feed it whatever arrives; it calls out(p, n)
// with what that unpacked to.
struct Gunzip {
  tinfl_decompressor inf;
  uint8_t* dict = nullptr;              // TINFL_LZ_DICT_SIZE, the caller's (PSRAM on the T-Deck)
  size_t dictPos = 0;
  uint8_t hdr[10];
  size_t hdrN = 0;
  uint8_t flg = 0;
  int stage = 0;                        // 0 header, 1 FEXTRA len, 2 FEXTRA, 3 FNAME, 4 FCOMMENT, 5 FHCRC, 6 deflate, 7 done, -1 bad
  uint16_t extraLeft = 0;
  uint8_t extraLen[2];
  size_t extraLenN = 0, crcLeft = 2;
  uint64_t outTotal = 0;

  explicit Gunzip(uint8_t* window) : dict(window) { tinfl_init(&inf); }
  bool done() const { return stage == 7; }
  bool bad() const { return stage < 0; }

  template <class F>
  bool feed(const uint8_t* p, size_t n, F&& out) {
    size_t i = 0;
    while (i < n && stage >= 0 && stage < 6) {
      const uint8_t b = p[i++];
      switch (stage) {
        case 0:
          hdr[hdrN++] = b;
          if (hdrN == 10) {
            if (hdr[0] != 0x1f || hdr[1] != 0x8b || hdr[2] != 8) { stage = -1; return false; }
            flg = hdr[3];
            stage = (flg & 4) ? 1 : (flg & 8) ? 3 : (flg & 16) ? 4 : (flg & 2) ? 5 : 6;
          }
          break;
        case 1:
          extraLen[extraLenN++] = b;
          if (extraLenN == 2) { extraLeft = extraLen[0] | (extraLen[1] << 8); stage = extraLeft ? 2 : ((flg & 8) ? 3 : (flg & 16) ? 4 : (flg & 2) ? 5 : 6); }
          break;
        case 2:
          if (--extraLeft == 0) stage = (flg & 8) ? 3 : (flg & 16) ? 4 : (flg & 2) ? 5 : 6;
          break;
        case 3: if (!b) stage = (flg & 16) ? 4 : (flg & 2) ? 5 : 6; break;
        case 4: if (!b) stage = (flg & 2) ? 5 : 6; break;
        case 5: if (--crcLeft == 0) stage = 6; break;
      }
    }
    while (i < n && stage == 6) {
      size_t inN = n - i, outN = TINFL_LZ_DICT_SIZE - dictPos;
      const tinfl_status st = tinfl_decompress(&inf, p + i, &inN, dict, dict + dictPos, &outN, TINFL_FLAG_HAS_MORE_INPUT);
      i += inN;
      if (outN) { out((const char*)dict + dictPos, outN); outTotal += outN; }
      dictPos = (dictPos + outN) & (TINFL_LZ_DICT_SIZE - 1);
      if (st == TINFL_STATUS_DONE) { stage = 7; break; }
      if (st < 0) { stage = -1; return false; }
      if (!inN && !outN) break;
    }
    return stage >= 0;
  }
};

// ---- the file on the card ----------------------------------------------------------------------
// Sorts the records by cell, then hands header + cell index + records to write(p, n) in pieces
// (straight to the card on the T-Deck: no second copy in memory). False if write() fails.
template <class W>
inline bool writeFile(std::vector<Record>& recs, uint32_t builtUnix, W&& write) {
  std::sort(recs.begin(), recs.end(), [](const Record& a, const Record& b) {
    const int32_t ka = cellKey(a.lat, a.lon), kb = cellKey(b.lat, b.lon);
    if (ka != kb) return ka < kb;
    if (a.lat != b.lat) return a.lat < b.lat;
    if (a.lon != b.lon) return a.lon < b.lon;
    if (a.dir != b.dir) return a.dir < b.dir;              // cameras on one pole: a full order, so the
    if (a.brand != b.brand) return a.brand < b.brand;      // T-Deck and the script write the same bytes
    return memcmp(a.op, b.op, sizeof a.op) < 0;
  });
  std::vector<CellEntry> cells;
  for (uint32_t i = 0; i < recs.size(); i++) {
    const int32_t k = cellKey(recs[i].lat, recs[i].lon);
    if (cells.empty() || cells.back().key != k) cells.push_back({k, i, 0});
    cells.back().n++;
  }
  uint8_t head[HEADER_BYTES];
  memcpy(head, MAGIC, 8);
  const uint32_t h[4] = {(uint32_t)recs.size(), (uint32_t)cells.size(), builtUnix, 0};
  memcpy(head + 8, h, 16);
  if (!write(head, HEADER_BYTES)) return false;
  if (!cells.empty() && !write((const uint8_t*)cells.data(), cells.size() * sizeof(CellEntry))) return false;
  const size_t CHUNK = 256;                                  // 8 KB at a time
  for (size_t i = 0; i < recs.size(); i += CHUNK) {
    const size_t n = recs.size() - i < CHUNK ? recs.size() - i : CHUNK;
    if (!write((const uint8_t*)(recs.data() + i), n * sizeof(Record))) return false;
  }
  return true;
}
inline void buildFile(std::vector<Record>& recs, uint32_t builtUnix, std::vector<uint8_t>& out) {
  out.clear();
  writeFile(recs, builtUnix, [&](const uint8_t* p, size_t n) { out.insert(out.end(), p, p + n); return true; });
}

struct FileInfo { uint32_t count = 0, cells = 0, built = 0; };
inline bool readHeader(const uint8_t* p, size_t n, FileInfo& fi) {
  if (n < HEADER_BYTES || memcmp(p, MAGIC, 8)) return false;
  uint32_t h[4];
  memcpy(h, p + 8, 16);
  fi.count = h[0]; fi.cells = h[1]; fi.built = h[2];
  return fi.cells <= fi.count + 1;
}
// The cells to load around a point: its own and the eight round it.
inline int neighbourKeys(double lat, double lon, int32_t keys[9]) {
  int n = 0;
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++) {
      const double la = lat + dy, lo = lon + dx;
      if (la < -90 || la >= 90) continue;
      keys[n++] = cellKey(la, lo < -180 ? lo + 360 : lo >= 180 ? lo - 360 : lo);
    }
  return n;
}

// ---- what's near ---------------------------------------------------------------------------------
struct Near { int index = -1; double m = 1e18, bearing = 0; };
inline Near nearest(const Record* r, size_t n, double lat, double lon) {
  Near best;
  for (size_t i = 0; i < n; i++) {
    const double m = metres(lat, lon, r[i].lat, r[i].lon);
    if (m < best.m) { best.m = m; best.index = (int)i; }
  }
  if (best.index >= 0) best.bearing = bearing(lat, lon, r[best.index].lat, r[best.index].lon);
  return best;
}
// The same answer, fast enough for the T-Deck to run every couple of seconds over thousands of
// cameras: the ESP32-S3 does single-precision maths in hardware but doubles in software, so the
// search ranks by a flat (equirectangular) distance in floats - exact enough to order points a
// degree or so apart - and only the winner gets the real distance.
inline Near nearestFast(const Record* r, size_t n, double lat, double lon) {
  Near best;
  const float la = (float)lat, lo = (float)lon;
  const float kx = cosf(la * 0.017453293f);
  float bestD = 3.4e38f;
  for (size_t i = 0; i < n; i++) {
    const float dy = r[i].lat - la;
    float dx = r[i].lon - lo;
    if (dx > 180) dx -= 360; else if (dx < -180) dx += 360;
    dx *= kx;
    const float d = dx * dx + dy * dy;
    if (d < bestD) { bestD = d; best.index = (int)i; }
  }
  if (best.index >= 0) {
    best.m = metres(lat, lon, r[best.index].lat, r[best.index].lon);
    best.bearing = bearing(lat, lon, r[best.index].lat, r[best.index].lon);
  }
  return best;
}

// "Confirmed": the radio heard a camera signature while a mapped one was this close.
constexpr double CONFIRM_M = 150;
// Approach alert distance, and how long before the same camera alerts again.
constexpr double ALERT_M = 300;
constexpr uint32_t REALERT_S = 600;

// Radius filter for the T-Deck's download (the script can keep everything).
inline bool within(const Record& r, double lat, double lon, double radiusKm) {
  // quick reject on latitude, then the real distance
  if (fabs(r.lat - lat) * 111.0 > radiusKm) return false;
  return metres(lat, lon, r.lat, r.lon) <= radiusKm * 1000.0;
}

}  // namespace alprcore
