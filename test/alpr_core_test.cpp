// Host tests for the Flock hunt's camera map (src/alpr_core.h). With ALPR_GZ set to a copy of
// the published cameras.geojson.gz it also runs the whole download path on the real file:
// gunzip in odd-sized pieces -> feature scanner -> grid file -> read back.
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include "alpr_core.h"
using namespace alprcore;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main() {
  CHECK(brandCode("Flock Safety") == B_FLOCK && brandCode("FLOCK") == B_FLOCK && brandCode("Motorola Solutions") == B_MOTOROLA);
  CHECK(brandCode("") == B_UNKNOWN && brandCode(nullptr) == B_UNKNOWN && brandCode("Acme") == B_OTHER);
  CHECK(cellKey(36.85, -76.29) == (36 + 90) * 360 + (-77 + 180));

  // the scanner, fed one character at a time across features
  const char* js =
      "{\"type\":\"FeatureCollection\",\"features\":["
      "{\"type\":\"Feature\",\"geometry\":{\"type\":\"Point\",\"coordinates\":[-76.29,36.85]},"
      "\"properties\":{\"osmId\":1,\"brand\":\"Flock Safety\",\"operator\":\"Norfolk \\\"PD\\\", VA\",\"direction\":370,\"directionCardinal\":\"N\"}},"
      "{\"type\":\"Feature\",\"geometry\":{\"type\":\"Point\",\"coordinates\":[-76.30,36.86]},\"properties\":{\"osmId\":2}},"
      "{\"type\":\"Feature\",\"geometry\":{\"type\":\"Point\",\"coordinates\":[0,0]},\"properties\":{}}"
      "]}";
  std::vector<Record> got;
  FeatureScanner sc;
  for (const char* p = js; *p; p++) sc.feed(p, 1, [&](const Record& r) { got.push_back(r); });
  CHECK(sc.features == 3 && sc.skipped == 1 && got.size() == 2);
  CHECK(got.size() == 2 && fabs(got[0].lat - 36.85) < 1e-5 && fabs(got[0].lon + 76.29) < 1e-5);
  CHECK(got.size() == 2 && got[0].brand == B_FLOCK && got[0].dir == 10 && !strcmp(got[0].op, "Norfolk \"PD\", VA"));
  CHECK(got.size() == 2 && got[1].brand == B_UNKNOWN && got[1].dir == -1 && got[1].op[0] == 0);

  // the file
  std::vector<uint8_t> file;
  buildFile(got, 1234, file);
  FileInfo fi;
  CHECK(readHeader(file.data(), file.size(), fi) && fi.count == 2 && fi.cells == 1 && fi.built == 1234);
  CHECK(file.size() == HEADER_BYTES + 12 + 2 * 32);
  file[0] = 'X';
  CHECK(!readHeader(file.data(), file.size(), fi));

  int32_t keys[9];
  CHECK(neighbourKeys(36.85, -76.29, keys) == 9);
  Near nr = nearest(got.data(), got.size(), 36.851, -76.29);
  CHECK(nr.index == 0 && nr.m > 100 && nr.m < 120 && (nr.bearing > 170 && nr.bearing < 190));
  CHECK(within(got[0], 36.85, -76.29, 1) && !within(got[0], 37.85, -76.29, 100));

  // the real file, if given
  if (const char* path = getenv("ALPR_GZ")) {
    FILE* f = fopen(path, "rb");
    CHECK(f != nullptr);
    if (f) {
      std::vector<uint8_t> win(TINFL_LZ_DICT_SIZE);
      Gunzip gz(win.data());
      FeatureScanner fs;
      std::vector<Record> all;
      std::vector<uint8_t> in(2048);
      size_t chunk = 1, n;
      while ((n = fread(in.data(), 1, chunk, f)) > 0) {
        gz.feed(in.data(), n, [&](const char* p, size_t k) { fs.feed(p, k, [&](const Record& r) { all.push_back(r); }); });
        chunk = chunk % 1499 + 97;                      // odd sizes, so pieces split everything
      }
      fclose(f);
      size_t flock = 0, dir = 0, near = 0;
      for (auto& r : all) { flock += r.brand == B_FLOCK; dir += r.dir >= 0; near += within(r, 36.85, -76.29, 100); }
      printf("real file: %llu bytes unpacked, %u features, %zu records (%u skipped), %zu Flock, %zu with direction, %zu within 100 km of Norfolk\n",
             (unsigned long long)gz.outTotal, fs.features, all.size(), fs.skipped, flock, dir, near);
      const char* want = getenv("ALPR_EXPECT");
      CHECK(gz.done() && !gz.bad());
      if (want) CHECK(all.size() == (size_t)atoi(want));
      std::vector<uint8_t> out;
      buildFile(all, 1, out);
      CHECK(readHeader(out.data(), out.size(), fi) && fi.count == all.size());
      printf("grid file %zu bytes, %u cells\n", out.size(), fi.cells);
      // the T-Deck's loader: only the 3x3 cells round a point, read through the index; the
      // nearest camera from those must be the nearest of all (for points well inside the US)
      const double pts[][2] = {{36.85, -76.29}, {34.05, -118.24}, {41.88, -87.63}, {29.76, -95.37}, {47.61, -122.33}};
      for (auto& pt : pts) {
        int32_t keys[9];
        const int nk = neighbourKeys(pt[0], pt[1], keys);
        const CellEntry* cells = (const CellEntry*)(out.data() + HEADER_BYTES);
        const Record* recs = (const Record*)(out.data() + HEADER_BYTES + fi.cells * sizeof(CellEntry));
        std::vector<Record> loaded;
        for (uint32_t c = 0; c < fi.cells; c++)
          for (int k = 0; k < nk; k++)
            if (cells[c].key == keys[k]) loaded.insert(loaded.end(), recs + cells[c].first, recs + cells[c].first + cells[c].n);
        const Near a = nearest(loaded.data(), loaded.size(), pt[0], pt[1]);
        const Near b = nearest(recs, fi.count, pt[0], pt[1]);
        CHECK(a.index >= 0 && fabs(a.m - b.m) < 1e-6);
        const Near c = nearestFast(loaded.data(), loaded.size(), pt[0], pt[1]);
        CHECK(c.index >= 0 && fabs(c.m - a.m) < 1.0);      // the fast search finds the same camera (or one as near)
        printf("  near %.2f,%.2f: %zu loaded, nearest %.0f m\n", pt[0], pt[1], loaded.size(), a.m);
      }
      int worse = 0;
      for (int k = 0; k < 2000; k++) {                       // 2000 random spots near real cameras
        const Record& base = all[(k * 7919) % all.size()];
        const double la = base.lat + ((k * 37) % 200 - 100) * 0.0005, lo = base.lon + ((k * 53) % 200 - 100) * 0.0005;
        const Near e = nearest(all.data(), all.size(), la, lo), f = nearestFast(all.data(), all.size(), la, lo);
        if (f.m > e.m + 1.0) worse++;
      }
      printf("  fast vs exact over 2000 spots: %d where the fast pick was over 1 m farther\n", worse);
      CHECK(worse == 0);
      if (const char* dump = getenv("ALPR_OUT")) {        // for comparing with tools/alpr_extract.py
        FILE* o = fopen(dump, "wb");
        if (o) { fwrite(out.data(), 1, out.size(), o); fclose(o); }
      }
    }
  }

  printf(fails ? "%d FAILED\n" : "all alpr_core tests passed\n", fails);
  return fails != 0;
}
