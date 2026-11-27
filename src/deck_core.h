// RabbitMesh deck tools: the parts with no hardware in them (host-tested in
// rabbitmesh/test/deck_core_test.cpp). RF Monitor, Airtime, Nearby radar and the Trip
// computer, after WadaMesh's RF Monitor / Airtime / Nearby / Trip Odometer / GPS Compass
// apps (GPL-3.0, github.com/ALLFATHER-BV/wadamesh), rebuilt natively for Squatch Mesh's UI.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

namespace deckcore {

// ---- LoRa ------------------------------------------------------------------------------------
// Time on air of one packet, ms (Semtech AN1200.13). len: the whole radio payload, bytes.
// cr: 5..8 as MeshCore stores it (4/5..4/8). Explicit header, CRC on, as MeshCore sends.
constexpr int PREAMBLE = 16;          // MeshCore's preamble length
inline float timeOnAirMs(int len, int sf, float bwKHz, int cr) {
  if (sf < 5 || sf > 12 || bwKHz <= 0) return 0;
  const float tsym = (float)(1 << sf) / (bwKHz * 1000.0f) * 1000.0f;      // ms
  const int de = tsym > 16.0f ? 1 : 0;                                     // low data-rate optimise
  const int crN = (cr >= 5 && cr <= 8) ? cr - 4 : 1;
  const float num = 8.0f * len - 4.0f * sf + 28 + 16 - 0;                  // CRC on, explicit header
  const float den = 4.0f * (sf - 2 * de);
  float extra = ceilf(num / den) * (crN + 4);
  if (extra < 0) extra = 0;
  const float payloadSym = 8 + extra;
  return (PREAMBLE + 4.25f) * tsym + payloadSym * tsym;
}

// The lowest SNR the demodulator still decodes at this spreading factor, dB.
inline float snrLimit(int sf) { return -7.5f - 2.5f * (sf - 7); }
// How far above that a packet was: the link's margin, dB.
inline float margin(float snrDb, int sf) { return snrDb - snrLimit(sf); }

// ---- airtime -----------------------------------------------------------------------------------
// The channel's load: time on the air per second, kept for ten minutes in 10-second bars,
// split into what we heard and what we sent.
struct Airtime {
  static constexpr int BAR_S = 10, BARS = 60;     // 10 minutes
  float rxMs[BARS] = {0}, txMs[BARS] = {0};
  uint32_t barStart = 0;                          // seconds (any clock) the newest bar began
  int head = 0;                                   // the newest bar
  bool started = false;
  float totalRxMs = 0, totalTxMs = 0;
  uint32_t firstS = 0;

  void roll(uint32_t nowS) {
    if (!started) { started = true; barStart = nowS - nowS % BAR_S; firstS = nowS; return; }
    while (nowS >= barStart + BAR_S) {
      barStart += BAR_S;
      head = (head + 1) % BARS;
      rxMs[head] = txMs[head] = 0;
    }
  }
  void add(uint32_t nowS, float ms, bool tx) {
    roll(nowS);
    (tx ? txMs : rxMs)[head] += ms;
    (tx ? totalTxMs : totalRxMs) += ms;
  }
  // Bar i back from the newest (0 = the one filling now), percent of its 10 s.
  float barPct(int back, bool tx) const {
    const int i = ((head - back) % BARS + BARS) % BARS;
    return (tx ? txMs[i] : rxMs[i]) / (BAR_S * 10.0f);
  }
  // Load over the last `bars` whole bars plus the one filling now, percent.
  float loadPct(uint32_t nowS, int bars, bool includeTx = true) const {
    if (!started) return 0;
    float ms = 0;
    for (int b = 0; b <= bars && b < BARS; b++) {
      const int i = ((head - b) % BARS + BARS) % BARS;
      ms += rxMs[i] + (includeTx ? txMs[i] : 0);
    }
    uint32_t span = bars * BAR_S + (nowS >= barStart ? nowS - barStart : 0);
    if (nowS - firstS < span) span = nowS - firstS;   // don't dilute over time we weren't listening
    if (span == 0) span = 1;
    return ms / (span * 10.0f);
  }
  float peakPct() const {
    float p = 0;
    for (int i = 0; i < BARS; i++) { const float v = (rxMs[i] + txMs[i]) / (BAR_S * 10.0f); if (v > p) p = v; }
    return p;
  }
};

// Regions with a duty-cycle rule: EU 863-870 MHz, 1% in most of it, 10% in 869.4-869.65.
inline float dutyLimitPct(float mhz) {
  if (mhz >= 869.4f && mhz <= 869.65f) return 10.0f;
  if (mhz >= 863.0f && mhz <= 870.0f) return 1.0f;
  return 0;                                   // none (US 902-928 has no duty cycle)
}

// ---- a rolling series ----------------------------------------------------------------------------
template <int N>
struct Series {
  int16_t v[N] = {0};
  int n = 0, head = 0;                        // head: the next slot
  void push(int16_t x) { v[head] = x; head = (head + 1) % N; if (n < N) n++; }
  int16_t at(int i) const { return v[((head - n + i) % N + N) % N]; }   // 0 = oldest kept
  void range(int16_t& lo, int16_t& hi) const {
    lo = 32767; hi = -32768;
    for (int i = 0; i < n; i++) { const int16_t x = at(i); if (x < lo) lo = x; if (x > hi) hi = x; }
  }
};

// ---- geo -----------------------------------------------------------------------------------------
inline double metres(double la1, double lo1, double la2, double lo2) {
  const double r = 6371000.0, d2r = M_PI / 180.0;
  const double dla = (la2 - la1) * d2r, dlo = (lo2 - lo1) * d2r;
  const double a = sin(dla / 2) * sin(dla / 2) + cos(la1 * d2r) * cos(la2 * d2r) * sin(dlo / 2) * sin(dlo / 2);
  return 2 * r * asin(sqrt(a < 1 ? a : 1));
}
inline double bearing(double la1, double lo1, double la2, double lo2) {
  const double d2r = M_PI / 180.0;
  const double y = sin((lo2 - lo1) * d2r) * cos(la2 * d2r);
  const double x = cos(la1 * d2r) * sin(la2 * d2r) - sin(la1 * d2r) * cos(la2 * d2r) * cos((lo2 - lo1) * d2r);
  return fmod(atan2(y, x) / d2r + 360.0, 360.0);
}
inline const char* compass(double deg) {
  static const char* P[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  return P[(int)((deg + 22.5) / 45.0) % 8];
}
// Distance as text, in miles or km.
inline void distText(char* out, size_t cap, double m, bool miles) {
  if (miles) {
    const double ft = m * 3.28084, mi = m / 1609.344;
    if (mi < 0.1) snprintf(out, cap, "%d ft", (int)lround(ft));
    else snprintf(out, cap, mi < 10 ? "%.2f mi" : "%.1f mi", mi);
  } else {
    if (m < 1000) snprintf(out, cap, "%d m", (int)lround(m));
    else snprintf(out, cap, m < 10000 ? "%.2f km" : "%.1f km", m / 1000.0);
  }
}

// The radar's range: the smallest step that holds the target distance.
inline double radarRange(double needM, bool miles) {
  static const double KM[] = {250, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000};
  static const double MI[] = {0.25, 0.5, 1, 2, 5, 10, 20, 50, 100};
  if (miles) { for (double r : MI) if (r * 1609.344 >= needM) return r * 1609.344; return MI[8] * 1609.344; }
  for (double r : KM) if (r >= needM) return r;
  return KM[9];
}
inline double radarStep(double rangeM, bool miles, int d) {
  static const double KM[] = {250, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000};
  static const double MI[] = {0.25, 0.5, 1, 2, 5, 10, 20, 50, 100};
  const double* T = miles ? MI : KM;
  const int n = miles ? 9 : 10;
  const double f = miles ? 1609.344 : 1;
  int i = 0;
  for (int k = 0; k < n; k++) if (T[k] * f <= rangeM + 1) i = k;
  i += d; if (i < 0) i = 0; if (i >= n) i = n - 1;
  return T[i] * f;
}

// ---- trip --------------------------------------------------------------------------------------
// Distance only counts a step of at least STEP_M (GPS wander parked in one spot is not a
// walk), and a jump of more than JUMP_M in one fix is a glitch, not a drive. Moving time
// counts the seconds spent above MOVING_MPS.
struct Trip {
  static constexpr double STEP_M = 8, JUMP_M = 1500, MOVING_MPS = 0.7;
  bool on = false, have = false;
  double lat0 = 0, lon0 = 0;                  // the start
  double lat = 0, lon = 0;                    // the last counted point
  double distM = 0, maxMps = 0, curMps = 0;
  uint32_t startS = 0, movingS = 0, lastS = 0;
  float alt = 0, altMin = 0, altMax = 0, climbM = 0, altRef = 0;
  bool haveAlt = false;

  void start(uint32_t nowS) { *this = Trip(); on = true; startS = lastS = nowS; }
  void stop() { on = false; }
  // A fix. Returns true when the point was counted (a new trackpoint for the GPX log).
  bool feed(uint32_t nowS, double la, double lo, bool altOk, float altM) {
    if (!on) return false;
    if (altOk) {
      alt = altM;
      if (!haveAlt) { haveAlt = true; altMin = altMax = altRef = altM; }
      if (altM < altMin) altMin = altM;
      if (altM > altMax) altMax = altM;
      if (altM - altRef >= 3) { climbM += altM - altRef; altRef = altM; }   // climb past a 3 m step
      else if (altM < altRef) altRef = altM;
    }
    if (!have) { have = true; lat0 = lat = la; lon0 = lon = lo; lastS = nowS; return true; }
    const double d = metres(lat, lon, la, lo);
    const uint32_t dt = nowS > lastS ? nowS - lastS : 0;
    if (d > JUMP_M && dt < 30) return false;
    if (d < STEP_M) { if (dt > 5) curMps = 0; return false; }
    distM += d;
    const double mps = dt ? d / dt : 0;
    curMps = mps;
    if (mps > maxMps && mps < 90) maxMps = mps;          // 90 m/s: a GPS glitch, not a car
    if (mps >= MOVING_MPS) movingS += dt;
    lat = la; lon = lo; lastS = nowS;
    return true;
  }
  double avgMps() const { return movingS ? distM / movingS : 0; }
};

inline double speedUnits(double mps, bool miles) { return miles ? mps * 2.236936 : mps * 3.6; }
inline const char* speedName(bool miles) { return miles ? "mph" : "km/h"; }

// ---- GPX ---------------------------------------------------------------------------------------
// A track file that is complete after every point: the closing tags are written after
// each point and the next point overwrites them, so a flat battery still leaves a file
// any mapping program opens. (WadaMesh's track recorder does the same.)
constexpr const char* GPX_TAIL = "</trkseg></trk></gpx>\n";
inline int gpxHead(char* out, size_t cap, const char* name) {
  return snprintf(out, cap,
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<gpx version=\"1.1\" creator=\"RabbitMesh\" xmlns=\"http://www.topografix.com/GPX/1/1\">\n"
      "<trk><name>%s</name><trkseg>\n%s", name, GPX_TAIL);
}
// One point plus the tail; iso is "" when the clock isn't set.
inline int gpxPoint(char* out, size_t cap, double la, double lo, bool altOk, float altM, const char* iso) {
  char ele[24] = "", tim[40] = "";
  if (altOk) snprintf(ele, sizeof ele, "<ele>%.1f</ele>", altM);
  if (iso && *iso) snprintf(tim, sizeof tim, "<time>%s</time>", iso);
  return snprintf(out, cap, "<trkpt lat=\"%.6f\" lon=\"%.6f\">%s%s</trkpt>\n%s", la, lo, ele, tim, GPX_TAIL);
}

}  // namespace deckcore
