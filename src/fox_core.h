// RabbitMesh fox hunt: the signal-finding logic, kept apart from the screen so the host tests
// can run it (test/fox_core_test.cpp).
//
// Walk around with the T-Deck and the meter says whether you're getting warmer: it smooths the
// signal strength of what it hears, keeps the peak, says whether the signal is rising or
// falling, and turns the strength into a Geiger-counter click, faster and higher the closer
// you get.
#pragma once
#include <stdint.h>
#include <math.h>

namespace foxcore {

// The RSSI range the meter spans: about the SX1262's floor to right next to a transmitter.
constexpr float RSSI_LO = -125.0f, RSSI_HI = -45.0f;
// WiFi: about the ESP32's floor to standing beside the access point.
constexpr float WIFI_LO = -95.0f, WIFI_HI = -30.0f;
inline float strength(float rssiDbm, float lo = RSSI_LO, float hi = RSSI_HI) {
  const float s = (rssiDbm - lo) / (hi - lo);
  return s < 0 ? 0 : s > 1 ? 1 : s;
}

constexpr float TREND_DB = 1.5f;       // smaller moves than this are "steady"

struct Meter {
  bool have = false;
  float avg = 0;                       // smoothed RSSI, dBm
  float last = 0;                      // the latest reading
  float peak = -200;
  int trend = 0;                       // +1 stronger, -1 weaker, 0 steady
  uint32_t at = 0;                     // when the latest reading came, ms
  uint32_t n = 0;

  void feed(float rssi, uint32_t now) {
    if (!have) { avg = rssi; have = true; trend = 0; }
    else {
      const float before = avg;
      avg = avg * 0.6f + rssi * 0.4f;
      trend = avg > before + TREND_DB ? 1 : avg < before - TREND_DB ? -1 : 0;
    }
    last = rssi;
    if (rssi > peak) peak = rssi;
    at = now;
    n++;
  }
  bool fresh(uint32_t now, uint32_t staleMs) const { return have && now - at < staleMs; }
  void resetPeak() { peak = have ? avg : -200; }
  void clear() { *this = Meter(); }
};

// The Geiger click: 1.2 s apart when faint, 70 ms apart right on top of it, pitch rising too.
inline uint32_t clickGapMs(float s) { return (uint32_t)(1200.0f - s * 1130.0f); }
inline uint16_t clickHz(float s) { return (uint16_t)(600.0f + s * 1800.0f); }

// ---- which way is stronger --------------------------------------------------------------------
// With no compass, the T-Deck can't tell which way you're facing; but walking, the GPS says where
// each reading was taken. A plane fitted through recent (east, north, signal) readings slopes up
// toward the transmitter; its uphill bearing is the hint. It answers only once you've covered
// some ground (SPREAD_M) with enough readings, and the slope is clear of the noise (MIN_SLOPE).
// Fox hunt and flock hunt both use it (test/fox_core_test.cpp).
struct Gradient {
  static constexpr int N = 40;                         // readings kept
  static constexpr uint32_t WINDOW_MS = 90000;         // only the last minute and a half
  static constexpr int MIN_SAMPLES = 6;
  static constexpr float SPREAD_M = 15.0f;             // ground covered before it answers
  static constexpr float MIN_SLOPE = 0.05f;            // dB per metre: flatter is no direction
  struct S { float e, n, rssi; uint32_t at; };
  S s[N];
  int count = 0, head = 0;
  double lat0 = 0, lon0 = 0;
  bool origin = false;

  void clear() { count = head = 0; origin = false; }
  // local metres east/north of the first reading (flat-earth: fine over a few km)
  void toLocal(double lat, double lon, float& e, float& n) const {
    const double k = 111320.0;
    e = (float)((lon - lon0) * k * cos(lat0 * M_PI / 180.0));
    n = (float)((lat - lat0) * k);
  }
  void add(double lat, double lon, float rssi, uint32_t nowMs) {
    if (!origin) { origin = true; lat0 = lat; lon0 = lon; }
    float e, n;
    toLocal(lat, lon, e, n);
    s[head] = {e, n, rssi, nowMs};
    head = (head + 1) % N;
    if (count < N) count++;
  }
  // Why there's no direction yet, for the screen to say what to do about it.
  enum Why : uint8_t { OK = 0, FEW, SHORT, LINE, FLAT };
  static constexpr float WIDTH_M = 2.0f;               // you have to turn: this wide across your path
  // The bearing (degrees from north) toward stronger signal, and how steep (dB per 10 m). why:
  // FEW readings, SHORT of ground (coveredM says how much), all in a LINE (a straight walk can't
  // tell left from right), or the signal too FLAT to say.
  bool estimate(uint32_t nowMs, float& bearingDeg, float& dbPer10m, Why* why = nullptr, float* coveredM = nullptr) const {
    Why w = OK;
    bool ok = fit(nowMs, bearingDeg, dbPer10m, w, coveredM);
    if (why) *why = w;
    return ok;
  }
  bool fit(uint32_t nowMs, float& bearingDeg, float& dbPer10m, Why& why, float* coveredM) const {
    double me = 0, mn = 0, mr = 0;
    int k = 0;
    if (coveredM) *coveredM = 0;
    for (int i = 0; i < count; i++) if (nowMs - s[i].at <= WINDOW_MS) { me += s[i].e; mn += s[i].n; mr += s[i].rssi; k++; }
    if (k < MIN_SAMPLES) { why = FEW; return false; }
    me /= k; mn /= k; mr /= k;
    double see = 0, snn = 0, sen = 0, ser = 0, snr = 0, spread = 0;
    for (int i = 0; i < count; i++) {
      if (nowMs - s[i].at > WINDOW_MS) continue;
      const double de = s[i].e - me, dn = s[i].n - mn, dr = s[i].rssi - mr;
      see += de * de; snn += dn * dn; sen += de * dn; ser += de * dr; snr += dn * dr;
      const double d = sqrt(de * de + dn * dn);
      if (d > spread) spread = d;
    }
    if (coveredM) *coveredM = (float)(spread * 2);
    if (spread * 2 < SPREAD_M) { why = SHORT; return false; }   // spread: furthest from the middle
    // How wide the readings are across their main line (the smaller axis, as a standard deviation).
    const double tr = see + snn, dif = see - snn;
    const double minor = (tr - sqrt(dif * dif + 4 * sen * sen)) / 2 / k;
    if (minor < (WIDTH_M / 2) * (WIDTH_M / 2)) { why = LINE; return false; }
    const double det = see * snn - sen * sen;
    if (det <= 0) { why = LINE; return false; }
    const double b = (ser * snn - snr * sen) / det;    // dB per metre east
    const double c = (snr * see - ser * sen) / det;    // dB per metre north
    const double slope = sqrt(b * b + c * c);
    if (slope < MIN_SLOPE) { why = FLAT; return false; }
    bearingDeg = (float)fmod(atan2(b, c) * 180.0 / M_PI + 360.0, 360.0);
    dbPer10m = (float)(slope * 10.0);
    why = OK;
    return true;
  }
  // Which way you're going: from the oldest recent reading to the newest, if far enough apart.
  bool course(uint32_t nowMs, float& bearingDeg, uint32_t withinMs = 20000, float minM = 8.0f) const {
    if (count < 2) return false;
    const S& last = s[(head + N - 1) % N];
    const S* first = nullptr;
    for (int i = count; i >= 1; i--) {                 // oldest first, within the time
      const S& x = s[(head + N - i) % N];
      if (nowMs - x.at <= withinMs) { first = &x; break; }
    }
    if (!first) return false;
    const float de = last.e - first->e, dn = last.n - first->n;
    if (sqrtf(de * de + dn * dn) < minM) return false;
    bearingDeg = (float)fmod(atan2(de, dn) * 180.0 / M_PI + 360.0, 360.0);
    return true;
  }
};

// The target's bearing relative to the way you're going: "ahead", "ahead-right", ... "behind".
inline const char* relative(float targetDeg, float courseDeg) {
  static const char* R[] = {"ahead", "ahead-right", "right", "behind-right", "behind", "behind-left", "left", "ahead-left"};
  float d = fmodf(targetDeg - courseDeg + 360.0f + 22.5f, 360.0f);
  return R[(int)(d / 45.0f) % 8];
}
// A bearing that doesn't jump about: each new estimate pulls it a part of the way (as a unit
// vector, so 350 and 10 average to 0, not 180).
struct SmoothBearing {
  float x = 0, y = 0;
  bool have = false;
  void add(float deg, float k = 0.3f) {
    const float a = deg * (float)M_PI / 180.0f;
    if (!have) { x = sinf(a); y = cosf(a); have = true; return; }
    x += (sinf(a) - x) * k; y += (cosf(a) - y) * k;
  }
  float deg() const { return fmodf(atan2f(x, y) * 180.0f / (float)M_PI + 360.0f, 360.0f); }
  void clear() { have = false; x = y = 0; }
};

inline const char* compass8(float deg) {
  static const char* P[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  return P[(int)(fmodf(deg + 22.5f + 360.0f, 360.0f) / 45.0f) % 8];
}

}  // namespace foxcore
