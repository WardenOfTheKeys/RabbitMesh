// RabbitMesh spectrum analyzer: the parts with no radio in them (host-tested in
// rabbitmesh/test/spectrum_core_test.cpp). After WadaMesh's Spectrum app (GPL-3.0,
// github.com/ALLFATHER-BV/wadamesh): a swept-tuned analyzer on the mesh radio, BINS
// frequencies across the span, each read as the peak of several instantaneous-RSSI
// samples through a 62.5 kHz filter.
#pragma once
#include <stdint.h>
#include <string.h>
#include <math.h>

namespace speccore {

constexpr int BINS = 160;
constexpr int WF_ROWS = 36;              // waterfall history, one row per sweep
constexpr float FMIN = 150.0f, FMAX = 960.0f;   // what the SX1262 driver accepts
constexpr float RBW_KHZ = 62.5f;         // the narrowest LoRa filter: adjacent bins resolve
constexpr int DBM_LO = -180, DBM_HI = -5;   // a read outside this is a failed read (0 dBm), not a signal

// The spans offered, MHz, centred on the mesh channel.
constexpr float SPANS[] = {2.0f, 8.0f, 24.0f};
constexpr int NSPANS = 3;

struct Span {
  float start = 0, step = 0;             // MHz: bin i is start + i * step
  float stop() const { return start + step * (BINS - 1); }
  float freq(int bin) const { return start + step * bin; }
  int bin(float mhz) const { int b = (int)lroundf((mhz - start) / step); return b < 0 ? 0 : b >= BINS ? BINS - 1 : b; }
};
// Centred on centre where possible; slid inside the tunable range near its ends.
inline Span makeSpan(float centreMHz, float spanMHz) {
  Span s;
  if (spanMHz > FMAX - FMIN) spanMHz = FMAX - FMIN;
  float a = centreMHz - spanMHz / 2;
  if (a < FMIN) a = FMIN;
  if (a + spanMHz > FMAX) a = FMAX - spanMHz;
  s.start = a;
  s.step = spanMHz / (BINS - 1);
  return s;
}

struct Sweep {
  int16_t rssi[BINS];                   // the freshest read of each bin
  int16_t peak[BINS];                   // peak hold, sinking 1 dB a sweep back toward the live trace
  uint8_t wf[WF_ROWS][BINS];            // waterfall: dBm + 200, newest row at wfHead
  int wfHead = 0, wfRows = 0;
  int pos = 0;                          // the next bin to read
  int floorDbm = -120;                  // the noise floor, smoothed over sweeps
  uint32_t sweeps = 0;

  void reset() {
    for (int i = 0; i < BINS; i++) { rssi[i] = -140; peak[i] = -140; }
    memset(wf, 0, sizeof wf);
    wfHead = wfRows = pos = 0;
    floorDbm = -120;
    sweeps = 0;
  }
  // A bin's reading; false if it was a failed read and the bin keeps its old value.
  bool put(int bin, int dbm) {
    if (bin < 0 || bin >= BINS || dbm < DBM_LO || dbm > DBM_HI) return false;
    rssi[bin] = (int16_t)dbm;
    if (dbm > peak[bin]) peak[bin] = (int16_t)dbm;
    return true;
  }
  // The caller read bin `pos`; move on. True when that finished a sweep.
  bool next() {
    if (++pos < BINS) return false;
    pos = 0;
    finishSweep();
    return true;
  }
  void finishSweep() {
    sweeps++;
    // The floor: the quietest bin is a fair cheap estimate once every bin is really in RX.
    int mn = 9999;
    for (int i = 0; i < BINS; i++) if (rssi[i] < mn) mn = rssi[i];
    if (mn > -170 && mn < -20) floorDbm = sweeps == 1 ? mn : (floorDbm * 3 + mn) / 4;
    for (int i = 0; i < BINS; i++) if (peak[i] > rssi[i]) peak[i]--;
    wfHead = (wfHead + 1) % WF_ROWS;
    for (int i = 0; i < BINS; i++) {
      int v = rssi[i] + 200;
      wf[wfHead][i] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
    }
    if (wfRows < WF_ROWS) wfRows++;
  }
  // Waterfall row r back from the newest (0 = newest), bin i, as dBm; -200 when empty.
  int wfAt(int r, int i) const {
    if (r >= wfRows) return -200;
    return wf[((wfHead - r) % WF_ROWS + WF_ROWS) % WF_ROWS][i] - 200;
  }
  int strongestBin() const {
    int b = 0;
    for (int i = 1; i < BINS; i++) if (rssi[i] > rssi[b]) b = i;
    return b;
  }
};

// Heat for the waterfall: 0 up to a little over the noise floor (so plain noise stays
// dark and only real energy lights up), 1 at floor + headroom.
inline float heat(int dbm, int floorDbm, int headroom = 30) {
  const int lo = floorDbm + 3, hi = floorDbm + headroom;
  if (dbm <= lo) return 0;
  if (dbm >= hi) return 1;
  return (float)(dbm - lo) / (hi - lo);
}

// The strongest bin under screen column x of a w-column view: a narrow view shows
// several bins per column and must not drop a peak between them.
inline int colMax(const int16_t* a, int x, int w) {
  const int b0 = x * BINS / w;
  int b1 = (x + 1) * BINS / w;
  if (b1 <= b0) b1 = b0 + 1;
  int m = a[b0];
  for (int i = b0 + 1; i < b1 && i < BINS; i++) if (a[i] > m) m = a[i];
  return m;
}

}  // namespace speccore
