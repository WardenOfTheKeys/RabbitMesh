#include <cstring>
// Host test for wardrive_core.h: g++ -std=c++17 -I<src> wardrive_core_test.cpp && ./a.out
#include "wardrive_core.h"
#include <cstdio>
#include <cassert>
using namespace wdcore;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main() {
  // geometry: 0.001 deg of latitude ~ 111 m
  CHECK(fabs(metres(36.85, -76.0, 36.851, -76.0) - 111.2) < 1.0);

  // colour ramp ends and middle
  CHECK(rampRgb(0) == 0xFF3860);
  CHECK(rampRgb(1) == 0x00FFAA);
  CHECK(rampRgb(0.5f) == 0xFFE14D);
  CHECK(quality(-30) == 0 && quality(30) == 1);
  CHECK(fabs(quality(-2) - 0.5f) < 1e-6);

  // stops: link is the best of min(they, we) over the replies
  Stop s; stopBegin(s, 1, 2);
  stopAdd(s, 20, -8);   // they hear us at +5, we hear them at -2  -> link -2
  stopAdd(s, -4, 12);   // -1 / +3 -> link -1
  CHECK(s.n == 2 && s.they4 == 20 && s.we4 == 12 && s.link4 == -4);
  CHECK(stopDb(s, DIR_LINK) == -1.0f && stopDb(s, DIR_THEY_HEAR_US) == 5.0f && stopDb(s, DIR_WE_HEAR_THEM) == 3.0f);

  // probe gate
  Gate g; g.minGapS = 20; g.moveM = 50; g.idleS = 120;
  CHECK(!shouldProbe(g, true, 0, false, 0));          // no fix, not even the first
  CHECK(shouldProbe(g, true, 0, true, 0));            // first with a fix
  CHECK(!shouldProbe(g, false, 10000, true, 500));    // moved far but inside the floor
  CHECK(shouldProbe(g, false, 21000, true, 60));      // past the floor and moved
  CHECK(!shouldProbe(g, false, 60000, true, 10));     // parked, not idle long enough
  CHECK(shouldProbe(g, false, 121000, true, 0));      // parked, idle timer
  CHECK(harvestValid(5000, 40) && !harvestValid(9000, 0) && !harvestValid(5000, 400));

  // parsing: our format, the range-test format, headers, no-fix rows, dead zones
  Row r;
  CHECK(parseRow(CSV_HEADER, r) == P_SKIP);
  CHECK(parseRow("2026-11-25T01:02:03Z,36.850000,-76.280000,12,9,3,repeater,Hill Top,ab12cd34,5.25,-2.0,-101", r) == P_HIT);
  CHECK(r.they4 == 21 && r.we4 == -8 && fabs(r.lat - 36.85) < 1e-9);
  CHECK(parseRow("2026-11-25T01:02:03Z,36.850000,-76.280000,12,9,3,none,,,,,\r\n", r) == P_NONE);
  CHECK(parseRow("2026-11-25T01:02:03Z,,,0,repeater,x,1,1,-90", r) == P_SKIP);        // range test, no fix
  CHECK(parseRow("2026-11-25T01:02:03Z,36.9,-76.1,7,Ridge,deadbeef,-3.5,4.0,-110", r) == P_HIT);
  CHECK(r.they4 == -14 && r.we4 == 16);
  CHECK(parseRow("time_utc,lat,lon,sats,repeater,key,they_hear_us_db,we_hear_them_db,rssi_dbm", r) == P_SKIP);
  CHECK(parseRow("garbage", r) == P_SKIP);

  // the loader groups rows that share time+lat+lon into one stop
  Stop buf[3];
  Loader L(buf, 3);
  L.add(CSV_HEADER);
  L.add("T1,36.85,-76.28,1,9,0,repeater,A,k1,4,4,-90");
  L.add("T1,36.85,-76.28,1,9,0,room,B,k2,-6,2,-100");
  L.add("T2,36.86,-76.28,1,9,0,none,,,,,");
  L.add("T3,36.87,-76.28,1,9,0,repeater,A,k1,1,0,-95");
  L.add("T4,36.88,-76.28,1,9,0,repeater,A,k1,1,0,-95");   // past cap: dropped
  L.finish();
  CHECK(L.n == 3 && L.rows == 4 && L.hits == 3 && L.dead == 1);
  CHECK(buf[0].n == 2 && buf[0].link4 == 16 && buf[1].n == 0 && buf[2].n == 1);

  // export
  {
    Stop a; stopBegin(a, 36.85, -76.29); stopAdd(a, 32, 16);       // link 4 dB
    Stop z; stopBegin(z, 36.86, -76.30);                             // dead
    char b[512];
    const int k = kmlStop(b, sizeof b, a, DIR_LINK, 0);
    CHECK(k > 0 && (size_t)k < sizeof b);
    CHECK(strstr(b, "<coordinates>-76.290001,36.849998,0</coordinates>") || strstr(b, "<coordinates>-76.290000,36.850000,0</coordinates>"));
    CHECK(strstr(b, "4.0 dB, 1 answered"));
    kmlStop(b, sizeof b, z, DIR_LINK, 1);
    CHECK(strstr(b, "no answer") && strstr(b, "<color>ff606060</color>"));
    geojsonStop(b, sizeof b, a, true);
    CHECK(b[0] == '{' && strstr(b, "\"answered\":1") && strstr(b, "\"link_db\":4.00"));
    geojsonStop(b, sizeof b, z, false);
    CHECK(b[0] == ',' && strstr(b, "\"link_db\":null"));
    char tiny[16];
    CHECK(kmlStop(tiny, sizeof tiny, a, DIR_LINK, 0) > 15 && strlen(tiny) == 15);   // truncates, no overrun
  }
  // cues
  {
    Stop a; stopBegin(a, 0, 0);
    CHECK(cueHz(a, DIR_LINK) == 0);
    stopAdd(a, 40, 40);                                              // 10 dB: solid
    CHECK(cueHz(a, DIR_LINK) == 1760);
    Stop w; stopBegin(w, 0, 0); stopAdd(w, -60, -60);                // -15 dB: gone
    CHECK(cueHz(w, DIR_LINK) == 440);
  }
  // known repeaters
  {
    uint32_t mem[3];
    KnownSet ks(mem, 3);
    const uint8_t p1[4] = {0xde, 0xad, 0xbe, 0xef}, p2[4] = {0x01, 0x02, 0x03, 0x04};
    CHECK(shortKey(p1) == 0xdeadbeefu);
    CHECK(ks.add(shortKey(p1)) && !ks.add(shortKey(p1)));
    CHECK(ks.add(shortKey(p2)) && ks.has(0x01020304u) && ks.has(0xdeadbeefu) && !ks.has(5));
    CHECK(ks.add(7) && !ks.add(9) && ks.n == 3);                    // full: refuses, no overrun
    CHECK(mem[0] < mem[1] && mem[1] < mem[2]);
  }

  printf(fails ? "%d FAILED\n" : "all wardrive_core tests passed\n", fails);
  return fails != 0;
}
