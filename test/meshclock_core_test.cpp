// Host tests for the time from the mesh (src/meshclock_core.h).
#include <cstdio>
#include "meshclock_core.h"
using namespace meshclockcore;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main() {
  const uint32_t T = 1795000000UL;               // Nov 2026
  {
    Clock c;
    c.heard(1, T, 0);
    c.heard(2, T + 10, 0);
    CHECK(c.estimate(0) == 0);                   // two nodes: not enough
    c.heard(3, T - 20, 0);
    int k = 0;
    CHECK(c.estimate(0, &k) == T && k == 3);     // the median of three that agree
    CHECK(c.estimate(60000) == T + 60);          // carried forward by the time since
  }
  {
    Clock c;                                     // one wild clock can't move it
    c.heard(1, T, 0); c.heard(2, T + 5, 0); c.heard(3, T + 3, 0); c.heard(4, T + 86400 * 400, 0);
    int k = 0;
    const uint32_t e = c.estimate(0, &k);
    CHECK(e >= T && e <= T + 5 && k == 3);
  }
  {
    Clock c;                                     // three that disagree: no answer
    c.heard(1, T, 0); c.heard(2, T + 1000, 0); c.heard(3, T + 5000, 0);
    CHECK(c.estimate(0) == 0);
  }
  {
    Clock c;                                     // never-set clocks are ignored
    c.heard(1, 1000, 0); c.heard(2, 2000, 0); c.heard(3, 3000, 0);
    CHECK(c.estimate(0) == 0);
  }
  {
    Clock c;                                     // the same node again counts once, its latest time
    c.heard(1, T, 0); c.heard(1, T + 2, 0); c.heard(1, T + 4, 0);
    CHECK(c.estimate(0) == 0);
    c.heard(2, T, 0); c.heard(3, T, 0);
    CHECK(c.estimate(0) != 0);
  }
  {
    Clock c;                                     // sightings over 3 h old drop out
    c.heard(1, T, 0); c.heard(2, T, 0); c.heard(3, T, 0);
    CHECK(c.estimate(MAX_AGE_MS + 1) == 0);
  }
  {
    Clock c;                                     // full: the longest-ago sighting makes room
    for (uint32_t i = 0; i < (uint32_t)SLOTS; i++) c.heard(100 + i, T, i * 1000);
    c.heard(999, T + 16, 16000);
    bool first = false, newest = false;
    for (auto& s : c.s) { if (s.used && s.key == 100) first = true; if (s.used && s.key == 999) newest = true; }
    CHECK(!first && newest);
  }
  CHECK(shouldSet(T, T + 2400));                 // 40 min behind: set it
  CHECK(!shouldSet(T, T + 60));                  // within 2 min: leave it
  CHECK(!shouldSet(T, T - 2400));                // ahead of the mesh: never moved back
  CHECK(!shouldSet(T, 0));                       // no answer
  printf(fails ? "%d FAILED\n" : "all meshclock_core tests passed\n", fails);
  return fails != 0;
}
