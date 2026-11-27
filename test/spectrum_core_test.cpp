// Host test for spectrum_core.h
#include "spectrum_core.h"
#include <cstdio>
#include <cstdlib>
using namespace speccore;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
int main() {
  Span s = makeSpan(910.525f, 24);
  CHECK(fabsf(s.start - 898.525f) < 1e-3f && fabsf(s.stop() - 922.525f) < 1e-3f);
  CHECK(s.bin(910.525f) == 80 || s.bin(910.525f) == 79);
  CHECK(s.bin(0) == 0 && s.bin(2000) == BINS - 1);
  Span e = makeSpan(955, 24);                        // slid inside the driver's range
  CHECK(fabsf(e.stop() - 960) < 1e-3f);
  Span lo = makeSpan(152, 8);
  CHECK(fabsf(lo.start - 150) < 1e-3f);

  static Sweep w; w.reset();
  CHECK(!w.put(3, 0) && !w.put(3, -190) && w.rssi[3] == -140);   // failed reads ignored
  for (int i = 0; i < BINS; i++) { CHECK(w.put(w.pos, i == 50 ? -70 : -118 - (i % 3))); w.next(); }
  CHECK(w.sweeps == 1 && w.pos == 0 && w.floorDbm == -120 && w.wfRows == 1);
  CHECK(w.strongestBin() == 50 && w.peak[50] == -70 && w.wfAt(0, 50) == -70 && w.wfAt(1, 50) == -200);
  // the next sweep: the carrier is gone, the peak sinks 1 dB a sweep, the floor smooths
  for (int i = 0; i < BINS; i++) { w.put(w.pos, -110); w.next(); }
  CHECK(w.peak[50] == -71 && w.rssi[50] == -110 && w.floorDbm == (-120 * 3 - 110) / 4);
  CHECK(w.wfAt(0, 50) == -110 && w.wfAt(1, 50) == -70);
  for (int k = 0; k < WF_ROWS + 5; k++) for (int i = 0; i < BINS; i++) { w.put(w.pos, -115); w.next(); }
  CHECK(w.wfRows == WF_ROWS && w.wfAt(WF_ROWS - 1, 0) == -115);

  CHECK(heat(-130, -120) == 0 && heat(-118, -120) == 0 && heat(-50, -120) == 1 && heat(-110, -120) > 0.25f && heat(-110, -120) < 0.3f);
  int16_t a[BINS]; for (int i = 0; i < BINS; i++) a[i] = -120; a[101] = -60;
  CHECK(colMax(a, 50, 80) == -60);                   // 2 bins a column: the peak survives
  CHECK(colMax(a, 0, 320) == -120);
  printf(fails ? "%d FAILED\n" : "all spectrum_core tests passed\n", fails);
  return fails != 0;
}
