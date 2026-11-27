// RabbitMesh wardrive: the parts with no hardware in them, so they can be tested on a
// computer (test/wardrive_core_test.cpp). Pure C++: no
// Arduino, no SD, no radio.
//
// The survey works the way WadaMesh's Discover/Wardrive does: it PROBES rather than
// listens. A probe is a zero-hop discover that every repeater in earshot answers, so a
// reply proves the link works from where you are, and each reply carries both directions
// of it: how well we heard them and how well they heard us. Those are rarely equal, and
// "I can hear the repeater but it can't hear me" is the thing a coverage map is for.
//
// A probe that nobody answers is logged too (a "none" row): a dead zone is coverage
// data, not a gap in it.

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

namespace wdcore {

// ---- geometry -----------------------------------------------------------------------------
inline double metres(double la1, double lo1, double la2, double lo2) {
  const double r = 6371000.0, d2r = M_PI / 180.0;
  const double dla = (la2 - la1) * d2r, dlo = (lo2 - lo1) * d2r;
  const double a = sin(dla / 2) * sin(dla / 2) + cos(la1 * d2r) * cos(la2 * d2r) * sin(dlo / 2) * sin(dlo / 2);
  return 2 * r * asin(sqrt(a < 1 ? a : 1));
}

// ---- which direction of the link a stop is coloured by --------------------------------------
enum Dir : uint8_t {
  DIR_LINK = 0,        // the weaker of the two: what works both ways (the honest one, and the default)
  DIR_THEY_HEAR_US,    // our transmit reach
  DIR_WE_HEAR_THEM,    // our receive reach
  DIR_COUNT
};
inline const char* dirName(uint8_t d) {
  static const char* const N[DIR_COUNT] = {"both ways", "they hear us", "we hear them"};
  return N[d % DIR_COUNT];
}

// One probe's worth: where we were and the best of what answered. SNR is kept x4 like
// MeshCore does (int8, quarter-dB). n == 0 is a dead zone.
struct Stop {
  float   lat, lon;
  int8_t  they4, we4, link4;   // best "they hear us", best "we hear them", best min(they, we)
  uint8_t n;                   // how many answered
};

inline void stopBegin(Stop& s, double lat, double lon) {
  s.lat = (float)lat; s.lon = (float)lon;
  s.they4 = s.we4 = s.link4 = -128;
  s.n = 0;
}
inline void stopAdd(Stop& s, int theirSnr4, int ourSnr4) {
  const int link = theirSnr4 < ourSnr4 ? theirSnr4 : ourSnr4;
  if (theirSnr4 > s.they4) s.they4 = (int8_t)theirSnr4;
  if (ourSnr4 > s.we4) s.we4 = (int8_t)ourSnr4;
  if (link > s.link4) s.link4 = (int8_t)link;
  if (s.n < 255) s.n++;
}
inline float stopDb(const Stop& s, uint8_t dir) {
  const int8_t v = dir == DIR_THEY_HEAR_US ? s.they4 : dir == DIR_WE_HEAR_THEM ? s.we4 : s.link4;
  return v / 4.0f;
}

// ---- colour ------------------------------------------------------------------------------
// SNR to a 0..1 quality. LoRa still decodes below 0 dB (SF7 to about -7.5 dB), so the ramp
// runs from -12 dB (gone) to +8 dB (solid).
constexpr float Q_LO_DB = -12.0f, Q_HI_DB = 8.0f;
inline float quality(float snrDb) {
  float q = (snrDb - Q_LO_DB) / (Q_HI_DB - Q_LO_DB);
  return q < 0 ? 0 : q > 1 ? 1 : q;
}
// Neon ramp, RGB888: hot red -> neon yellow -> cyan-green. Bright on the dark map style
// and on OSM tiles alike.
inline uint32_t rampRgb(float q) {
  struct C { float r, g, b; };
  const C lo = {255, 56, 96}, mid = {255, 225, 77}, hi = {0, 255, 170};
  const C a = q < 0.5f ? lo : mid, b = q < 0.5f ? mid : hi;
  const float f = q < 0.5f ? q * 2 : (q - 0.5f) * 2;
  const uint32_t r = (uint32_t)lroundf(a.r + (b.r - a.r) * f);
  const uint32_t g = (uint32_t)lroundf(a.g + (b.g - a.g) * f);
  const uint32_t bl = (uint32_t)lroundf(a.b + (b.b - a.b) * f);
  return (r << 16) | (g << 8) | bl;
}

// ---- when to probe -----------------------------------------------------------------------
// Probes spend everyone's airtime, so the survey sends one only when it has something new
// to learn: never more often than minGap, sooner once we have moved moveM, and every idle
// seconds when parked (a repeater coming and going is worth knowing too). No GPS fix, no
// probe: a reply we can't place is airtime for nothing.
struct Gate {
  uint16_t minGapS = 20;       // WadaMesh's floor is 15 s; Squatch's range test allows 15
  uint16_t moveM = 50;
  uint16_t idleS = 120;
};
inline bool shouldProbe(const Gate& g, bool first, uint32_t sinceLastMs, bool haveFix, double movedM) {
  if (!haveFix) return false;
  if (first) return true;
  if (sinceLastMs < (uint32_t)g.minGapS * 1000u) return false;
  if (movedM >= g.moveM) return true;
  return sinceLastMs >= (uint32_t)g.idleS * 1000u;
}

// Replies land over the few seconds after a probe. They are logged at the probe's
// position. If the harvest is late (the loop stalled) or we moved too far while
// listening, they were heard somewhere the probe's position no longer describes, and they
// are dropped instead of being put on the map in the wrong place.
constexpr uint32_t HARVEST_MS = 5000;
constexpr uint32_t STALE_SLACK_MS = 3000;
constexpr double MAX_DRIFT_M = 250;     // ~180 km/h for the whole window
inline bool harvestValid(uint32_t sinceProbeMs, double driftM) {
  return sinceProbeMs <= HARVEST_MS + STALE_SLACK_MS && driftM <= MAX_DRIFT_M;
}

// ---- the log --------------------------------------------------------------------------------
// One row per answer, and one "none" row for a probe nobody answered. Opens in a
// spreadsheet, QGIS, or anything that reads CSV with lat/lon columns.
constexpr const char* CSV_HEADER =
    "time_utc,lat,lon,alt_m,sats,drift_m,type,name,key,they_hear_us_db,we_hear_them_db,rssi_dbm";

// Reading a run back for review. Takes this firmware's own logs and Squatch's range-test
// logs (/inw/range: time_utc,lat,lon,sats,repeater,key,they_hear_us_db,we_hear_them_db,rssi_dbm),
// so a range test from before you flashed RabbitMesh shows on the map too.
struct Row {
  char   key[48];              // time,lat,lon as written: rows with the same one are one stop
  double lat, lon;
  bool   none;                 // nobody answered this probe
  int    they4, we4;
};
enum Parse : uint8_t { P_SKIP = 0, P_HIT, P_NONE };

// Splits on commas in place; returns the field count (at most cap).
inline int splitCsv(char* line, char** f, int cap) {
  int n = 0;
  char* p = line;
  while (n < cap) {
    f[n++] = p;
    char* c = strchr(p, ',');
    if (!c) break;
    *c = 0;
    p = c + 1;
  }
  // trim a trailing \r or \n off the last field
  if (n) { char* e = f[n - 1] + strlen(f[n - 1]); while (e > f[n - 1] && (e[-1] == '\r' || e[-1] == '\n')) *--e = 0; }
  return n;
}
inline bool numField(const char* s, double& out) {
  if (!s || !*s) return false;
  char* end = nullptr;
  out = strtod(s, &end);
  return end && end != s && *end == 0;
}
inline Parse parseRow(const char* lineIn, Row& r) {
  char buf[256];
  strncpy(buf, lineIn, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = 0;
  char* f[16];
  const int n = splitCsv(buf, f, 16);
  int iThey, iWe, iName;
  if (n == 12) { iName = 7; iThey = 9; iWe = 10; }       // RabbitMesh wardrive
  else if (n == 9) { iName = 4; iThey = 6; iWe = 7; }    // Squatch range test
  else return P_SKIP;
  double la, lo;
  if (!numField(f[1], la) || !numField(f[2], lo)) return P_SKIP;   // the header, or a row with no fix
  if (la == 0 && lo == 0) return P_SKIP;
  if (la < -90 || la > 90 || lo < -180 || lo > 180) return P_SKIP;
  r.lat = la; r.lon = lo;
  // the stop key: time + lat + lon exactly as written
  size_t k = 0;
  for (int i = 0; i < 3; i++) {
    const size_t len = strlen(f[i]);
    if (k + len + 1 >= sizeof(r.key)) break;
    memcpy(r.key + k, f[i], len); k += len;
    r.key[k++] = i < 2 ? ',' : 0;
  }
  r.key[k < sizeof(r.key) ? k : sizeof(r.key) - 1] = 0;
  double th, we;
  if (!strcmp(f[iName], "none") || !numField(f[iThey], th) || !numField(f[iWe], we)) {
    r.none = true; r.they4 = r.we4 = -128;
    return P_NONE;
  }
  r.none = false;
  r.they4 = (int)lround(th * 4); r.we4 = (int)lround(we * 4);
  auto clamp4 = [](int v) { return v < -127 ? -127 : v > 127 ? 127 : v; };
  r.they4 = clamp4(r.they4);
  r.we4 = clamp4(r.we4);
  return P_HIT;
}

// Feeds rows in file order and groups them into stops. Call add() for each line, then
// finish(); out/cap is the caller's buffer, and the count stops at cap.
struct Loader {
  Stop*  out;
  size_t cap, n = 0;
  size_t rows = 0, hits = 0, dead = 0;
  char   cur[48] = "";
  bool   open = false;
  Loader(Stop* o, size_t c) : out(o), cap(c) {}
  void add(const char* line) {
    Row r;
    const Parse p = parseRow(line, r);
    if (p == P_SKIP) return;
    if (!open || strcmp(cur, r.key) != 0) {
      if (n >= cap) { open = false; return; }   // full: the rest of the run is not shown
      stopBegin(out[n], r.lat, r.lon);
      strncpy(cur, r.key, sizeof(cur) - 1); cur[sizeof(cur) - 1] = 0;
      open = true;
      n++;
    }
    rows++;
    if (p == P_HIT) { stopAdd(out[n - 1], r.they4, r.we4); hits++; }
  }
  void finish() { for (size_t i = 0; i < n; i++) if (!out[i].n) dead++; }
};

// ---- export: Google Earth (KML) and GeoJSON -------------------------------------------------
// One point per stop, coloured and labelled by the chosen direction's best SNR; a stop nobody
// answered is a dead zone and is exported too (n = 0, no SNR). Each writer fills buf and
// returns the length written (snprintf's, so a too-small buf truncates rather than overruns).
constexpr const char* KML_HEAD =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<kml xmlns=\"http://www.opengis.net/kml/2.2\"><Document><name>RabbitMesh wardrive</name>\n";
constexpr const char* KML_TAIL = "</Document></kml>\n";
inline int kmlStop(char* buf, size_t cap, const Stop& s, uint8_t dir, size_t index) {
  // KML colours are aabbggrr.
  uint32_t rgb = s.n ? rampRgb(quality(stopDb(s, dir))) : 0x606060;
  const unsigned r = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255;
  char what[48];
  if (s.n) snprintf(what, sizeof what, "%.1f dB, %u answered", stopDb(s, dir), (unsigned)s.n);
  else snprintf(what, sizeof what, "no answer");
  return snprintf(buf, cap,
      "<Placemark><name>%u</name><description>%s</description>"
      "<Style><IconStyle><color>ff%02x%02x%02x</color><scale>0.6</scale></IconStyle></Style>"
      "<Point><coordinates>%.6f,%.6f,0</coordinates></Point></Placemark>\n",
      (unsigned)index + 1, what, b, g, r, s.lon, s.lat);
}
constexpr const char* GEOJSON_HEAD = "{\"type\":\"FeatureCollection\",\"features\":[\n";
constexpr const char* GEOJSON_TAIL = "\n]}\n";
inline int geojsonStop(char* buf, size_t cap, const Stop& s, bool first) {
  char snr[64];
  if (s.n) snprintf(snr, sizeof snr, "\"link_db\":%.2f,\"they_hear_us_db\":%.2f,\"we_hear_them_db\":%.2f",
                    s.link4 / 4.0, s.they4 / 4.0, s.we4 / 4.0);
  else snprintf(snr, sizeof snr, "\"link_db\":null");
  return snprintf(buf, cap,
      "%s{\"type\":\"Feature\",\"geometry\":{\"type\":\"Point\",\"coordinates\":[%.6f,%.6f]},"
      "\"properties\":{\"answered\":%u,%s}}",
      first ? "" : ",\n", s.lon, s.lat, (unsigned)s.n, snr);
}

// ---- cues while driving ----------------------------------------------------------------------
// The tone for a stop: higher for a better link, so the strength can be heard without looking.
// 0 for a stop nobody answered (the caller plays its own low "nothing" blip).
inline uint16_t cueHz(const Stop& s, uint8_t dir) {
  if (!s.n) return 0;
  return (uint16_t)(440 + quality(stopDb(s, dir)) * 1320);       // 440 Hz (weak) .. 1760 Hz (solid)
}

// ---- repeaters seen before, across runs --------------------------------------------------------
// A short key (the first 4 bytes of a node's public key, as the CSV logs it) as one number.
inline uint32_t shortKey(const uint8_t* pub) {
  return ((uint32_t)pub[0] << 24) | ((uint32_t)pub[1] << 16) | ((uint32_t)pub[2] << 8) | pub[3];
}
// A set of keys kept sorted, for "is this one new?" over a few hundred repeaters.
struct KnownSet {
  uint32_t* keys;
  size_t cap, n = 0;
  KnownSet(uint32_t* k, size_t c) : keys(k), cap(c) {}
  bool has(uint32_t k) const {
    size_t lo = 0, hi = n;
    while (lo < hi) { const size_t m = (lo + hi) / 2; if (keys[m] < k) lo = m + 1; else hi = m; }
    return lo < n && keys[lo] == k;
  }
  // true if it wasn't there (and now is); false if known, or the set is full
  bool add(uint32_t k) {
    size_t lo = 0, hi = n;
    while (lo < hi) { const size_t m = (lo + hi) / 2; if (keys[m] < k) lo = m + 1; else hi = m; }
    if (lo < n && keys[lo] == k) return false;
    if (n >= cap) return false;
    for (size_t i = n; i > lo; i--) keys[i] = keys[i - 1];
    keys[lo] = k;
    n++;
    return true;
  }
};

}  // namespace wdcore
