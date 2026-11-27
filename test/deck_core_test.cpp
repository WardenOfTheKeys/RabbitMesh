// Host test for deck_core.h: g++ -std=c++17 -I<src> deck_core_test.cpp && ./a.out
#include "deck_core.h"
#include <cstdio>
using namespace deckcore;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
#define NEAR(a, b, e) (fabs((a) - (b)) <= (e))

int main() {
  // Time on air. SF7/125k/CR4-5/20 B is 56.58 ms with an 8-symbol preamble (Semtech
  // calculator); MeshCore's 16-symbol preamble adds 8 x 1.024 ms.
  CHECK(NEAR(timeOnAirMs(20, 7, 125, 5), 56.576f + 8 * 1.024f, 0.01f));
  // SF12/125k: symbols are 32.8 ms, so low-data-rate optimise is on; must be longer than SF11's
  CHECK(timeOnAirMs(20, 12, 125, 5) > timeOnAirMs(20, 11, 125, 5));
  CHECK(timeOnAirMs(20, 7, 62.5f, 5) > timeOnAirMs(20, 7, 125, 5) * 1.9f);   // half the bandwidth, about twice as long
  CHECK(timeOnAirMs(10, 7, 125, 5) < timeOnAirMs(100, 7, 125, 5));
  CHECK(timeOnAirMs(10, 3, 125, 5) == 0);

  CHECK(snrLimit(7) == -7.5f && snrLimit(12) == -20.0f);
  CHECK(margin(-2.5f, 7) == 5.0f);

  // Airtime: 1 s on the air in one 10 s bar = 10 %
  Airtime a;
  a.add(1000, 600, false);
  a.add(1003, 400, true);
  CHECK(NEAR(a.barPct(0, false), 6.0f, 1e-4) && NEAR(a.barPct(0, true), 4.0f, 1e-4));
  a.add(1010, 0, false);                       // next bar
  CHECK(a.barPct(0, false) == 0 && NEAR(a.barPct(1, false), 6.0f, 1e-4));
  CHECK(NEAR(a.loadPct(1010, 6), 10.0f, 1e-3));   // 1000 ms over the 10 s since we started
  CHECK(NEAR(a.peakPct(), 10.0f, 1e-4));
  a.roll(1010 + 600);                          // ten minutes later: all gone
  CHECK(a.peakPct() == 0);
  CHECK(dutyLimitPct(868.1f) == 1.0f && dutyLimitPct(869.525f) == 10.0f && dutyLimitPct(910.525f) == 0);

  Series<4> s;
  for (int i = 1; i <= 6; i++) s.push(i);
  int16_t lo, hi; s.range(lo, hi);
  CHECK(s.n == 4 && s.at(0) == 3 && s.at(3) == 6 && lo == 3 && hi == 6);

  // geo
  CHECK(NEAR(bearing(36.85, -76.0, 36.95, -76.0), 0.0, 0.01));
  CHECK(NEAR(bearing(36.85, -76.0, 36.85, -75.9), 90.0, 0.1));
  CHECK(!strcmp(compass(359), "N") && !strcmp(compass(135), "SE"));
  char b[24];
  distText(b, sizeof b, 50, true);  CHECK(!strcmp(b, "164 ft"));
  distText(b, sizeof b, 3218.7, true); CHECK(!strcmp(b, "2.00 mi"));
  distText(b, sizeof b, 750, false); CHECK(!strcmp(b, "750 m"));
  distText(b, sizeof b, 12345, false); CHECK(!strcmp(b, "12.3 km"));

  CHECK(radarRange(1800, false) == 2000 && radarRange(1e9, false) == 200000);
  CHECK(NEAR(radarRange(1700, true), 2 * 1609.344, 1e-6));
  CHECK(radarStep(2000, false, 1) == 5000 && radarStep(250, false, -1) == 250);

  // trip: three 100 m steps 20 s apart = 300 m at 5 m/s; a wobble and a glitch don't count
  Trip t; t.start(0);
  const double dLat = 100.0 / 111195.0;
  CHECK(t.feed(0, 36.0, -76.0, true, 10));
  CHECK(!t.feed(5, 36.0 + dLat * 0.02, -76.0, true, 12));       // 2 m wobble
  CHECK(t.feed(20, 36.0 + dLat, -76.0, true, 15));
  CHECK(!t.feed(21, 37.0, -76.0, true, 15));                     // 110 km in a second
  CHECK(t.feed(40, 36.0 + 2 * dLat, -76.0, true, 9));
  CHECK(t.feed(60, 36.0 + 3 * dLat, -76.0, true, 20));
  CHECK(NEAR(t.distM, 300, 1) && t.movingS == 60 && NEAR(t.avgMps(), 5, 0.05) && NEAR(t.maxMps, 5, 0.05));
  CHECK(t.altMin == 9 && t.altMax == 20 && NEAR(t.climbM, 16, 0.01));   // 10 -> 15 (+5), 9 -> 20 (+11)
  CHECK(NEAR(speedUnits(5, false), 18, 1e-9) && NEAR(speedUnits(5, true), 11.18, 0.01));

  // GPX: head + point both end in the tail, so the file is always closed
  char g[512];
  gpxHead(g, sizeof g, "trip");
  CHECK(strstr(g, "<trkseg>") && strstr(g, GPX_TAIL));
  const int n = gpxPoint(g, sizeof g, 36.85, -76.28, true, 12.5f, "2026-11-25T01:02:03Z");
  const char* want = "<trkpt lat=\"36.850000\" lon=\"-76.280000\"><ele>12.5</ele><time>2026-11-25T01:02:03Z</time></trkpt>\n";
  CHECK(n > 0 && !strncmp(g, want, strlen(want)));
  CHECK(!strcmp(g + n - strlen(GPX_TAIL), GPX_TAIL));

  printf(fails ? "%d FAILED\n" : "all deck_core tests passed\n", fails);
  return fails != 0;
}
