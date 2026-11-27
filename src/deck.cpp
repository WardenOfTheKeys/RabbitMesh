// RabbitMesh deck (see deck.h). Data and keys here; the look is deck_draw.h.
#include "deck.h"
#include <vector>
#include <algorithm>
#include <SD.h>
#include <Preferences.h>
#include "app.h"
#include "node.h"
#include "dataio.h"
#include "gps.h"
#include "logstore.h"
#include "regional.h"
#include "settings.h"
#include "fieldtools.h"
#include "wardrive.h"
#include "spectrum.h"
#include "mecp.h"
#include "fox.h"
#include "wifidrive.h"
#include "flock.h"
#include "holiday.h"
#include "tzauto.h"
#include "meshclock.h"
#include "drive.h"
#include "sdview.h"
#include "ui.h"
#include "deck_draw.h"

extern LogStore logs;

namespace deck {

using namespace deckcore;
using namespace deckdraw;

static uint32_t nowS() { return millis() / 1000; }
static bool miles() { return ui_settings.miles; }

// ---- background sampling: every packet's time on air, RSSI and arrival -------------------
static Airtime s_air;
static Series<96> s_rssi, s_noise;
static uint32_t s_lastGen = 0;
static bool s_genInit = false;
static uint32_t s_rxTimes[64];               // arrival (millis) of the last 64 packets heard
static uint8_t s_rxHead = 0;
static uint32_t s_noiseAt = 0;

static void radioParams(int& sf, float& bw, int& cr, float& mhz) {
  NodePrefs& p = g_node->prefs();
  sf = p.sf; bw = p.bw; cr = p.cr; mhz = p.freq;
}

static void sample() {
  if (!g_node) return;
  const uint32_t now = millis();
  if (!s_genInit) { s_genInit = true; s_lastGen = g_node->pktGen; }
  uint32_t fresh = g_node->pktGen - s_lastGen;
  s_lastGen = g_node->pktGen;
  if (fresh > g_node->pktCount) fresh = g_node->pktCount;
  if (fresh) {
    int sf, cr; float bw, mhz;
    radioParams(sf, bw, cr, mhz);
    for (uint32_t k = fresh; k > 0; k--) {      // oldest new one first
      const PacketLogEntry& e = g_node->pktLog[(g_node->pktHead + InwNode::PKT_LOG_MAX - k) % InwNode::PKT_LOG_MAX];
      s_air.add(nowS(), timeOnAirMs(e.len, sf, bw, cr), e.tx);
      if (!e.tx) {
        s_rssi.push(e.rssi);
        s_rxTimes[s_rxHead] = e.at;
        s_rxHead = (s_rxHead + 1) % 64;
      }
    }
  }
  s_air.roll(nowS());
  if (now - s_noiseAt >= 2000) {
    s_noiseAt = now;
    s_noise.push((int16_t)g_node->noiseFloor());
  }
}

static int perMinute() {
  const uint32_t now = millis();
  int n = 0;
  for (uint32_t t : s_rxTimes) if (t && now - t <= 60000) n++;
  return n;
}

// ---- trip ------------------------------------------------------------------------------------
static Trip s_trip;
static bool s_gpx = true;                     // record a GPX track with each trip
static char s_gpxPath[64] = "";
static uint32_t s_tripFeedAt = 0;

bool wantsGps() { return s_trip.on || wifidrive::running() || flockhunt::wantsGps(); }

static void isoNow(char* out, size_t cap) {
  out[0] = 0;
  if (!app::timeValid()) return;
  const time_t t = (time_t)app::now();
  struct tm tm;
  gmtime_r(&t, &tm);
  strftime(out, cap, "%Y-%m-%dT%H:%M:%SZ", &tm);
}

static bool gpxOpen() {
  s_gpxPath[0] = 0;
  if (!sdMount()) return false;
  if (!SD.exists("/rabbit")) SD.mkdir("/rabbit");
  if (!SD.exists("/rabbit/tracks")) SD.mkdir("/rabbit/tracks");
  char name[32];
  if (app::timeValid()) {
    const time_t t = (time_t)app::now() + regional::offsetMin(app::now()) * 60;
    struct tm tm;
    gmtime_r(&t, &tm);
    snprintf(name, sizeof name, "trip-%04d%02d%02d-%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
  } else {
    snprintf(name, sizeof name, "trip-up%lu", (unsigned long)nowS());
  }
  snprintf(s_gpxPath, sizeof s_gpxPath, "/rabbit/tracks/%s.gpx", name);
  File f = SD.open(s_gpxPath, FILE_WRITE);
  if (!f) { s_gpxPath[0] = 0; return false; }
  char head[256];
  gpxHead(head, sizeof head, name);
  f.print(head);
  f.close();
  return true;
}

// Overwrites the closing tags with the new point and writes them again behind it.
static void gpxAppend(double la, double lo, bool altOk, float alt) {
  if (!s_gpxPath[0] || !sdMounted()) return;
  File f = SD.open(s_gpxPath, "r+");
  if (!f) { sdWriteFailed("gpx track"); return; }
  const size_t tail = strlen(GPX_TAIL), sz = f.size();
  if (sz < tail || !f.seek(sz - tail)) { f.close(); return; }
  char iso[24], pt[200];
  isoNow(iso, sizeof iso);
  const int n = gpxPoint(pt, sizeof pt, la, lo, altOk, alt, iso);
  if (n > 0) f.write((const uint8_t*)pt, (size_t)n);
  f.close();
}

// ---- the trip across a restart --------------------------------------------------------------------
// Saved to flash (its own Preferences space) every minute while it runs and when it starts or
// stops, and read back at boot: a running trip carries on, distance, moving time and GPX track
// and all. Its seconds count from boot (millis), so the last-point time is re-based on the way in.
static uint32_t s_tripSavedAt = 0;
static void tripSave() {
  s_tripSavedAt = millis();
  Preferences p;
  if (!p.begin("rm-trip", false)) return;
  p.putBytes("t", &s_trip, sizeof s_trip);
  p.putString("gpx", s_gpxPath);
  p.putBool("gpxOn", s_gpx);
  p.end();
}
static void tripRestore() {
  Preferences p;
  if (!p.begin("rm-trip", true)) return;
  Trip t;
  const bool ok = p.getBytesLength("t") == sizeof t && p.getBytes("t", &t, sizeof t) == sizeof t;
  const String gpx = p.getString("gpx", "");
  s_gpx = p.getBool("gpxOn", s_gpx);
  p.end();
  if (!ok || (!t.on && !t.have)) return;
  t.startS = t.lastS = nowS();                    // seconds since this boot from here on
  s_trip = t;
  strlcpy(s_gpxPath, gpx.c_str(), sizeof s_gpxPath);
  if (s_gpxPath[0] && (!sdMount() || !SD.exists(s_gpxPath))) s_gpxPath[0] = 0;   // the card changed: no track
  if (s_trip.on) logs.add(LOG_INFO, "trip resumed after a restart: %.0f m so far", s_trip.distM);
}

static void tripStart() {
  s_trip.start(nowS());
  if (s_gpx && !gpxOpen()) nav.toast("no sd card: trip without a gpx track", 3000);
  logs.add(LOG_INFO, "trip started%s%s", s_gpxPath[0] ? ": " : "", s_gpxPath);
  tripSave();
}
static void tripStop() {
  if (!s_trip.on) return;
  s_trip.stop();
  logs.add(LOG_INFO, "trip stopped: %.0f m", s_trip.distM);
  tripSave();
}

static void tripTick() {
  static bool restored = false;
  if (!restored) { restored = true; tripRestore(); }
  if (s_trip.on && millis() - s_tripSavedAt > 60000) tripSave();
  if (!s_trip.on || millis() - s_tripFeedAt < 1000) return;
  s_tripFeedAt = millis();
  if (!gps.hasFix()) return;
  const GpsFix& f = gps.fix();
  if (f.lat == 0 && f.lon == 0) return;
  const bool altOk = f.satellites >= 4;          // altitude wants a 3D fix
  if (s_trip.feed(nowS(), f.lat, f.lon, altOk, f.altitudeM) && s_gpx) gpxAppend(f.lat, f.lon, altOk, f.altitudeM);
}

// ---- packet log to SD ------------------------------------------------------------------------------
// Every packet the radio hears or sends, one CSV row each, in /rabbit/packets/pk-<date>-<time>.csv:
//   time_utc,uptime_ms,dir,type,route,hops,len,rssi_dbm,snr_db,lat,lon
// Rows are gathered from the node's packet ring (64 deep) every tick and written to the card
// every few seconds, so a busy channel costs one SD write, not one per packet.
static bool s_pkOn = false;
static char s_pkPath[64] = "";
static uint32_t s_pkGen = 0, s_pkRows = 0, s_pkLost = 0, s_pkFlushAt = 0;
static String s_pkBuf;
static const char* payloadName(uint8_t t);

static bool pkOpen() {
  s_pkPath[0] = 0;
  if (!sdMount()) return false;
  if (!SD.exists("/rabbit")) SD.mkdir("/rabbit");
  if (!SD.exists("/rabbit/packets")) SD.mkdir("/rabbit/packets");
  if (app::timeValid()) {
    const time_t t = (time_t)app::now() + regional::offsetMin(app::now()) * 60;
    struct tm tm;
    gmtime_r(&t, &tm);
    snprintf(s_pkPath, sizeof s_pkPath, "/rabbit/packets/pk-%04d%02d%02d-%02d%02d%02d.csv",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  } else {
    snprintf(s_pkPath, sizeof s_pkPath, "/rabbit/packets/pk-up%lu.csv", (unsigned long)nowS());
  }
  File f = SD.open(s_pkPath, FILE_WRITE);
  if (!f) { s_pkPath[0] = 0; return false; }
  f.println("time_utc,uptime_ms,dir,type,route,hops,len,rssi_dbm,snr_db,lat,lon");
  f.close();
  return true;
}
static void pkFlush() {
  s_pkFlushAt = millis();
  if (!s_pkBuf.length()) return;
  if (s_pkPath[0] && sdMounted()) {
    File f = SD.open(s_pkPath, FILE_APPEND);
    if (f) { f.print(s_pkBuf); f.close(); }
    else sdWriteFailed("packet log");
  }
  s_pkBuf = "";
}
bool packetLogOn() { return s_pkOn; }
uint32_t packetLogRows() { return s_pkRows; }
void packetLogToggle() {
  if (s_pkOn) {
    pkFlush();
    s_pkOn = false;
    logs.add(LOG_INFO, "packet log stopped: %lu packets%s", (unsigned long)s_pkRows, s_pkLost ? " (some missed)" : "");
    char msg[48];
    snprintf(msg, sizeof msg, "packet log saved: %lu packets", (unsigned long)s_pkRows);
    nav.toast(msg, 2500);
    return;
  }
  if (!g_node) { nav.toast("radio not running"); return; }
  if (!pkOpen()) { nav.toast("needs an sd card for the log", 3000); return; }
  s_pkOn = true;
  s_pkGen = g_node->pktGen;                         // from now on, not what's already in the ring
  s_pkRows = s_pkLost = 0;
  s_pkBuf = "";
  s_pkFlushAt = millis();
  logs.add(LOG_INFO, "packet log started: %s", s_pkPath);
  nav.toast("logging packets to the sd card", 2000);
}
static void pkTick() {
  if (!s_pkOn || !g_node) return;
  uint32_t fresh = g_node->pktGen - s_pkGen;
  if (fresh > InwNode::PKT_LOG_MAX) { s_pkLost += fresh - InwNode::PKT_LOG_MAX; fresh = InwNode::PKT_LOG_MAX; }
  if (fresh) {
    char iso[24] = "";
    if (app::timeValid()) isoNow(iso, sizeof iso);
    char where[40] = ",";
    if (gps.hasFix() && !(gps.fix().lat == 0 && gps.fix().lon == 0))
      snprintf(where, sizeof where, "%.6f,%.6f", gps.fix().lat, gps.fix().lon);
    static const char* R[] = {"tflood", "flood", "direct", "tdirect"};
    for (uint32_t i = 0; i < fresh; i++) {
      const PacketLogEntry& e = g_node->pktLog[(g_node->pktHead + InwNode::PKT_LOG_MAX - fresh + i) % InwNode::PKT_LOG_MAX];
      char row[160];
      if (e.tx) snprintf(row, sizeof row, "%s,%lu,tx,%s,%s,%u,%u,,,%s\n", iso, (unsigned long)e.at, payloadName(e.payloadType),
                         R[e.routeType & 3], e.hops, e.len, where);
      else snprintf(row, sizeof row, "%s,%lu,rx,%s,%s,%u,%u,%d,%.2f,%s\n", iso, (unsigned long)e.at, payloadName(e.payloadType),
                    R[e.routeType & 3], e.hops, e.len, e.rssi, e.snr4 / 4.0, where);
      s_pkBuf += row;
      s_pkRows++;
    }
    s_pkGen = g_node->pktGen;
  }
  if (millis() - s_pkFlushAt > 5000 || s_pkBuf.length() > 4096) pkFlush();
}

void tick() {
  sample();
  tripTick();
  pkTick();
  wifidrive::tick();
  flockhunt::tick();
  holiday::tick();
  tzauto::tick();
  meshclock::tick();
  spectrum::watch();
  mecp::tick();
}

// ---- RF Monitor ----------------------------------------------------------------------------------
static const char* payloadName(uint8_t t) {
  static const char* N[] = {"REQ", "RESP", "TXT_MSG", "ACK", "ADVERT", "GRP_TXT", "GRP_DAT", "ANON",
                            "PATH", "TRACE", "MULTI", "CTRL", "?12", "?13", "?14", "RAW"};
  return N[t & 15];
}

// ---- one packet, explained ---------------------------------------------------------------------
// Contacts whose public key starts with these bytes: a name, or up to three for a 1-byte hash
// (one byte matches many nodes on a big mesh: "maybe X, Y").
static String namesFor(const uint8_t* pre, uint8_t n) {
  if (!g_node || !n) return String("");
  String out;
  int found = 0;
  ContactsIterator it = g_node->startContactsIterator();
  ContactInfo c;
  while (it.hasNext(g_node, c) && found < 4) {
    if (memcmp(c.id.pub_key, pre, n)) continue;
    char nm[24];
    sanitize(c.name, nm, sizeof nm);
    if (found < 3) out += (found ? ", " : "") + String(nm);
    found++;
  }
  if (found > 3) out += ", ...";
  return found ? (n < 3 && found > 1 ? String("maybe ") + out : out) : String("");
}
static String hex(const uint8_t* b, uint8_t n) {
  String s;
  char h[3];
  for (uint8_t i = 0; i < n; i++) { snprintf(h, sizeof h, "%02x", b[i]); s += h; }
  return s;
}
static const char* typeWords(uint8_t t) {
  switch (t & 15) {
    case 0:  return "a request (login, status, telemetry)";
    case 1:  return "an answer to a request";
    case 2:  return "a direct message";
    case 3:  return "an ack: a message arrived";
    case 4:  return "an advert: a node announcing itself";
    case 5:  return "a channel message";
    case 6:  return "channel data";
    case 7:  return "an anonymous request (a login)";
    case 8:  return "a path being returned";
    case 9:  return "a trace";
    case 11: return "control (discover)";
    default: return "other";
  }
}
static void packetDetail(const PacketLogEntry& pe) {
  static PacketLogEntry e;                        // a copy: the ring moves on while it's open
  e = pe;
  nav.push(new TextPageView("Packet", [](std::vector<String>& out) {
    static const char* R[] = {"flood with transport codes", "flood: every repeater passes it on",
                              "direct: it follows a set path", "direct with transport codes"};
    out.push_back(String(e.tx ? "sent " : "heard ") + (millis() - e.at) / 1000 + " s ago");
    out.push_back(String(payloadName(e.payloadType)) + ": " + typeWords(e.payloadType));
    out.push_back(String("route  ") + R[e.routeType & 3]);
    if (!e.tx) out.push_back(String("hops   ") + e.hops + (e.hops == 0 ? " (straight from the sender)" : e.hops == 1 ? " repeat on the way" : " repeats on the way"));
    if (e.srcLen) {
      const String nm = namesFor(e.src, e.srcLen);
      out.push_back(String("from   ") + (nm.length() ? nm : String("not in your contacts")) + "  (" + hex(e.src, e.srcLen) + ")");
    }
    if (e.viaLen) {
      const String nm = namesFor(e.via, e.viaLen);
      out.push_back(String("via    ") + (nm.length() ? nm : String("a repeater not in your contacts")) + "  (" + hex(e.via, e.viaLen) + ")");
    }
    if (!e.tx && g_node) {
      int sf, cr; float bw, mhz;
      radioParams(sf, bw, cr, mhz);
      const float snr = e.snr4 / 4.0f, mg = margin(snr, sf);
      out.push_back(String("signal ") + e.rssi + " dBm, SNR " + String(snr, 1) + " dB");
      out.push_back(String("margin ") + (mg >= 0 ? "+" : "") + String(mg, 1) + " dB over what SF" + sf + " still decodes" +
                    (mg < 0 ? " (lucky)" : mg < 5 ? " (weak)" : ""));
    }
    if (g_node) {
      int sf, cr; float bw, mhz;
      radioParams(sf, bw, cr, mhz);
      out.push_back(String("size   ") + e.len + " bytes, " + String(timeOnAirMs(e.len, sf, bw, cr), 0) + " ms on the air");
    }
  }, 1000));
}

class MonitorView : public View {
public:
  void tick() override { if (millis() - _at > 500) { _at = millis(); dirty = true; } }
  void draw(Canvas& g) override {
    char hint[40];
    if (s_pkOn) snprintf(hint, sizeof hint, "LOG %lu  l: stop", (unsigned long)s_pkRows);
    drawHeader(g, "RF Monitor", s_pkOn ? hint : _sel >= 0 ? "press: detail  s: all" : "up/down: pick  l: log");
    const Theme& t = nav.theme();
    MonitorModel m;
    if (g_node) {
      int sf, cr; float bw, mhz;
      radioParams(sf, bw, cr, mhz);
      m.noise = g_node->noiseFloor();
      m.heard = g_node->heardAt != 0;
      m.rssi = (int)lroundf(g_node->lastRssi());
      m.snr = g_node->lastSnr();
      m.marginDb = margin(m.snr, sf);
      m.perMin = perMinute();
      snprintf(m.radio, sizeof m.radio, "%.3f MHz  SF%d  BW%g", mhz, sf, bw);
      m.rssiS = &s_rssi;
      m.noiseS = &s_noise;
      static const char* R[] = {"tflood", "flood", "direct", "tdirect"};
      const uint8_t n = g_node->pktCount;
      for (uint8_t i = 0; i < n && m.nRows < 4; i++) {        // newest first
        const PacketLogEntry& e = g_node->pktLog[(g_node->pktHead + InwNode::PKT_LOG_MAX - 1 - i) % InwNode::PKT_LOG_MAX];
        PktRow& r = m.rows[m.nRows++];
        const unsigned long ago = (millis() - e.at) / 1000;
        if (e.tx) snprintf(r.text, sizeof r.text, "%lus %-7s %-6s %uB", ago, payloadName(e.payloadType), R[e.routeType & 3], e.len);
        else snprintf(r.text, sizeof r.text, "%lus %-7s %-6s %uhop %ddBm %.1fdB", ago, payloadName(e.payloadType), R[e.routeType & 3],
                      e.hops, e.rssi, e.snr4 / 4.0);
        r.tx = e.tx; r.snr4 = e.snr4;
      }
    }
    m.sel = _sel < m.nRows ? _sel : -1;
    deckdraw::monitor(g, t, m);
  }
  // Up/down pick one of the latest packets, press explains it; with none picked, press opens
  // the full packet list (as before). 's': that list any time.
  void press() override {
    if (_sel >= 0 && g_node && _sel < (int)g_node->pktCount)
      packetDetail(g_node->pktLog[(g_node->pktHead + InwNode::PKT_LOG_MAX - 1 - _sel) % InwNode::PKT_LOG_MAX]);
    else deckhooks::sniffer();
  }
  void rotate(int d) override { pick(d); }
  bool roll(int dx, int dy) override { if (dy) pick(dy); return dy != 0; }
  bool backspace() override { if (_sel < 0) return false; _sel = -1; dirty = true; return true; }
  void key(char c) override {
    if (c == 'l' || c == 'L') { packetLogToggle(); dirty = true; }
    if (c == 's' || c == 'S') deckhooks::sniffer();
  }
private:
  uint32_t _at = 0;
  int _sel = -1;
  void pick(int d) {
    const int n = g_node ? min<int>(g_node->pktCount, 4) : 0;
    if (!n) return;
    _sel = _sel < 0 ? 0 : constrain(_sel + d, 0, n - 1);
    dirty = true;
  }
};

// ---- Airtime --------------------------------------------------------------------------------------
class AirtimeView : public View {
public:
  void tick() override { if (millis() - _at > 1000) { _at = millis(); dirty = true; } }
  void draw(Canvas& g) override {
    drawHeader(g, "Airtime");
    AirModel m;
    m.a = &s_air;
    m.nowS = nowS();
    m.load1 = s_air.loadPct(m.nowS, 6);
    m.load10 = s_air.loadPct(m.nowS, Airtime::BARS - 1);
    m.peak = s_air.peakPct();
    float tx = 0;
    for (int b = 0; b < Airtime::BARS; b++) tx += s_air.barPct(b, true);
    m.txPct10 = tx / Airtime::BARS;
    if (g_node) {
      int sf, cr; float bw, mhz;
      radioParams(sf, bw, cr, mhz);
      m.duty = dutyLimitPct(mhz);
      snprintf(m.radio, sizeof m.radio, "20 B = %.0f ms on air", timeOnAirMs(20, sf, bw, cr));
    }
    deckdraw::airtime(g, nav.theme(), m);
  }
private:
  uint32_t _at = 0;
};

// ---- Nearby radar ----------------------------------------------------------------------------------
class RadarView : public View {
public:
  RadarView() { collect(); _range = _blips.empty() ? radarRange(2000, miles()) : radarRange(_blips[min<size_t>(_blips.size(), 5) - 1].dist, miles()); }
  void tick() override {
    if (millis() - _frame > 50) { _frame = millis(); _sweep = fmodf(_sweep + 0.10f, 2 * (float)M_PI); dirty = true; }
    if (g_node && g_node->contactsGen() != _gen && millis() - _collected > 5000) collect();
  }
  void draw(Canvas& g) override {
    drawHeader(g, "Nearby", "z/x: range");
    RadarModel m;
    m.b = _blips.data(); m.n = (int)_blips.size(); m.sel = _sel; m.first = _first;
    m.rangeM = _range; m.miles = miles(); m.haveMe = _haveMe; m.sweep = _sweep;
    deckdraw::radar(g, nav.theme(), m);
  }
  void rotate(int d) override { select(_sel + d); }
  bool roll(int dx, int dy) override {
    if (dy) select(_sel + dy);
    if (dx) { _range = radarStep(_range, miles(), dx > 0 ? 1 : -1); dirty = true; }
    return true;
  }
  void key(char c) override {
    if (c == 'z' || c == '+') { _range = radarStep(_range, miles(), -1); dirty = true; }
    if (c == 'x' || c == '-') { _range = radarStep(_range, miles(), 1); dirty = true; }
  }
  void press() override { if (_sel >= 0 && _sel < (int)_keys.size()) app::openContactDetail(_keys[_sel].k); }
  bool touch(const TouchEvent& e) override {
    if (e.type != TouchEvent::Tap) return false;
    const int R = (H - BODY_Y) / 2 - 5, lx = 2 * R + 14;
    if (e.x >= lx && e.y >= BODY_Y + 16) {
      const int i = _first + (e.y - BODY_Y - 16) / 22;
      if (i >= 0 && i < (int)_blips.size()) { if (i == _sel) press(); else select(i); }
      return true;
    }
    if (e.x < lx) { _range = radarStep(_range, miles(), e.y < BODY_Y + R ? -1 : 1); dirty = true; return true; }
    return false;
  }
  bool coasts() override { return false; }
private:
  struct Key { uint8_t k[32]; };
  std::vector<Blip> _blips;
  std::vector<Key> _keys;
  int _sel = 0, _first = 0;
  double _range = 2000;
  bool _haveMe = false;
  float _sweep = 0;
  uint32_t _frame = 0, _gen = 0, _collected = 0;

  void select(int i) {
    if (_blips.empty()) return;
    _sel = constrain(i, 0, (int)_blips.size() - 1);
    const int rows = (H - BODY_Y - 24) / 22;
    if (_sel < _first) _first = _sel;
    if (_sel >= _first + rows) _first = _sel - rows + 1;
    dirty = true;
  }
  void collect() {
    _collected = millis();
    _blips.clear(); _keys.clear();
    if (!g_node) return;
    _gen = g_node->contactsGen();
    double la, lo;
    _haveMe = app::myPosition(la, lo);
    if (!_haveMe) return;
    std::vector<std::pair<Blip, Key>> all;
    ContactsIterator it = g_node->startContactsIterator();
    ContactInfo c;
    while (it.hasNext(g_node, c)) {
      if (!c.type || (!c.gps_lat && !c.gps_lon)) continue;
      const double cla = c.gps_lat / 1e6, clo = c.gps_lon / 1e6;
      Blip b = {};
      sanitize(c.name, b.name, sizeof b.name);
      b.type = c.type;
      b.dist = metres(la, lo, cla, clo);
      b.brg = bearing(la, lo, cla, clo);
      b.eastM = b.dist * sin(b.brg * M_PI / 180.0);
      b.northM = b.dist * cos(b.brg * M_PI / 180.0);
      memcpy(b.pub, c.id.pub_key, sizeof b.pub);
      Key k;
      memcpy(k.k, c.id.pub_key, 32);
      all.push_back({b, k});
    }
    std::sort(all.begin(), all.end(), [](const std::pair<Blip, Key>& a, const std::pair<Blip, Key>& b) { return a.first.dist < b.first.dist; });
    if (all.size() > 60) all.resize(60);
    for (auto& p : all) { _blips.push_back(p.first); _keys.push_back(p.second); }
    if (_sel >= (int)_blips.size()) _sel = 0;
    dirty = true;
  }
};

static int s_radarCount = -1;                 // for the hub's tile: counted when the hub opens
static void countNearby() {
  s_radarCount = 0;
  if (!g_node) return;
  ContactsIterator it = g_node->startContactsIterator();
  ContactInfo c;
  while (it.hasNext(g_node, c)) if (c.type && (c.gps_lat || c.gps_lon)) s_radarCount++;
}

// ---- Trip ----------------------------------------------------------------------------------------
class TripView : public View {
public:
  void tick() override { if (millis() - _at > 250) { _at = millis(); dirty = true; } }
  void draw(Canvas& g) override {
    drawHeader(g, "Trip", s_trip.on ? "press: stop" : "press: start  g: gpx");
    TripModel m;
    m.t = &s_trip; m.miles = miles(); m.fix = gps.hasFix(); m.sats = gps.fix().satellites;
    m.gpx = s_gpx; m.nowS = nowS(); m.phase = millis() / 1000.0f;
    deckdraw::trip(g, nav.theme(), m);
  }
  void press() override {
    if (s_trip.on) { tripStop(); nav.toast(s_gpxPath[0] ? "trip stopped - gpx saved" : "trip stopped"); }
    else if (s_trip.have) {
      nav.push(new ConfirmView("Start a new trip?", "this one is cleared", [] { tripStart(); }));
    } else tripStart();
    dirty = true;
  }
  void key(char c) override {
    if (c == 'g') {
      if (s_trip.on) { nav.toast("stop the trip to change gpx recording"); return; }
      s_gpx = !s_gpx;
      nav.toast(s_gpx ? "gpx track: on" : "gpx track: off");
      dirty = true;
    }
  }
  bool touch(const TouchEvent& e) override {
    if (e.type == TouchEvent::Tap && e.x < 150 && e.y > BODY_Y + 120) { press(); return true; }   // the status pill
    if (e.type == TouchEvent::LongPress && e.x < 150) { key('g'); return true; }
    return false;
  }
private:
  uint32_t _at = 0;
};

bool tripOn() { return s_trip.on; }
void tripSet(bool on) { if (on && !s_trip.on) tripStart(); else if (!on && s_trip.on) tripStop(); }
double tripDistM() { return s_trip.distM; }
uint32_t tripMovingS() { return s_trip.movingS; }
double tripMaxMps() { return s_trip.maxMps; }
const char* tripGpx() { return s_gpxPath; }

void openMonitor() { nav.push(new MonitorView()); }
void openAirtime() { nav.push(new AirtimeView()); }
void openRadar() { nav.push(new RadarView()); }
void openTrip() { nav.push(new TripView()); }

// ---- the hub -------------------------------------------------------------------------------------
// The tiles, in hub order, with their shortcut letters (shown on each tile). Numbers 1-9 and 0
// open the first ten as before.
enum TileId : uint8_t { T_DRIVE, T_WARDRV, T_WIFI, T_FLOCK, T_FOX, T_TRIP, T_RF, T_SPECTRUM, T_AIR,
                        T_RADAR, T_MECP, T_FIELD, T_SD, T_MORE, T_COUNT };
static const char TILE_KEY[T_COUNT] = {'d', 'w', 'b', 'f', 'x', 't', 'r', 's', 'a', 'n', 'e', 'i', 'c', 'o'};

class HubView : public View {
public:
  HubView() { refresh(); }
  void tick() override { if (millis() - _at > 80) { _at = millis(); dirty = true; } }
  void resume() override { refresh(); dirty = true; }
  void draw(Canvas& g) override {
    drawHeader(g, "Deck", "letters: open a tool");
    Tile tl[N];
    build(tl);
    deckdraw::hub(g, nav.theme(), tl, N, _focus, millis() / 1000.0f);
  }
  void rotate(int d) override { _focus = (_focus + d + N) % N; dirty = true; }
  bool roll(int dx, int dy) override {
    _focus = constrain(_focus + dx + dy * 3, 0, N - 1);
    dirty = true;
    return true;
  }
  void press() override { launch(_focus); }
  void key(char c) override {
    if (c >= '1' && c <= '9') { launch(c - '1'); return; }
    if (c == '0') { launch(9); return; }
    const char k = (char)tolower((unsigned char)c);
    for (int i = 0; i < N; i++) if (TILE_KEY[i] == k) { _focus = i; launch(i); return; }
  }
  bool wantsAllKeys() override { return true; }   // letters are this screen's shortcuts
  bool touch(const TouchEvent& e) override {
    if (e.type != TouchEvent::Tap || e.y < BODY_Y) return false;
    const int rows = (N + 2) / 3, gap = 4, tw = (W - gap * 4) / 3, th = (H - BODY_Y - gap * (rows + 1)) / rows;
    const int c = (e.x - gap) / (tw + gap), r = (e.y - BODY_Y - gap) / (th + gap);
    if (c < 0 || c > 2 || r < 0 || r >= rows || r * 3 + c >= N) return false;
    _focus = r * 3 + c;
    launch(_focus);
    return true;
  }
private:
  static constexpr int N = T_COUNT;
  int _focus = 0;
  uint32_t _at = 0;
  String _sdText;                                // worked out on open: counting a card's free space is slow

  void refresh() { countNearby(); _sdText = sdview::tileText(); }

  // Five rows of three: driving, hunting, the radio, comms and the field, the card and the rest.
  void build(Tile* t) {
    char d[20];
    setTile(t[T_DRIVE], "DRIVE", drive::tileText().c_str(), IC_DRIVE, drive::on(), TILE_KEY[T_DRIVE]);
    setTile(t[T_RF], "RF MON", "", IC_RF, false, TILE_KEY[T_RF]);
    if (g_node) snprintf(t[T_RF].stat, sizeof t[T_RF].stat, "noise %d", g_node->noiseFloor());
    setTile(t[T_SPECTRUM], "SPECTRUM", "", IC_SPECTRUM, spectrum::ownsRadio(), TILE_KEY[T_SPECTRUM]);
    snprintf(t[T_SPECTRUM].stat, sizeof t[T_SPECTRUM].stat, "%s", spectrum::ownsRadio() ? "mesh paused" : "sweep band");
    setTile(t[T_AIR], "AIRTIME", "", IC_AIR, false, TILE_KEY[T_AIR]);
    snprintf(t[T_AIR].stat, sizeof t[T_AIR].stat, "load %.1f%%", s_air.loadPct(nowS(), 6));
    setTile(t[T_RADAR], "RADAR", "", IC_RADAR, false, TILE_KEY[T_RADAR]);
    snprintf(t[T_RADAR].stat, sizeof t[T_RADAR].stat, "%d placed", s_radarCount < 0 ? 0 : s_radarCount);
    setTile(t[T_TRIP], "TRIP", "", IC_TRIP, s_trip.on, TILE_KEY[T_TRIP]);
    distText(d, sizeof d, s_trip.distM, miles());
    snprintf(t[T_TRIP].stat, sizeof t[T_TRIP].stat, s_trip.on ? "%s%s" : (s_trip.have ? "%s  stopped" : "ready"), d, s_trip.on && s_gpx ? "  REC" : "");
    setTile(t[T_WARDRV], "WARDRV", "", IC_WARDRIVE, wardrive::running(), TILE_KEY[T_WARDRV]);
    size_t wn = 0;
    wardrive::overlay(wn);
    if (wardrive::running()) snprintf(t[T_WARDRV].stat, sizeof t[T_WARDRV].stat, "%u stops", (unsigned)wn);
    else snprintf(t[T_WARDRV].stat, sizeof t[T_WARDRV].stat, "mesh coverage");
    const uint8_t urgent = mecp::urgentWaiting();
    setTile(t[T_MECP], "MECP", "", IC_MECP, urgent > 0, TILE_KEY[T_MECP]);
    if (urgent) snprintf(t[T_MECP].stat, sizeof t[T_MECP].stat, "%u urgent", (unsigned)urgent);
    else snprintf(t[T_MECP].stat, sizeof t[T_MECP].stat, "emergency codes");
    setTile(t[T_FOX], "FOX", "find a signal", IC_FOX, false, TILE_KEY[T_FOX]);
    setTile(t[T_WIFI], "WIFI/BT", "", IC_WIFI, wifidrive::running(), TILE_KEY[T_WIFI]);
    if (wifidrive::running()) snprintf(t[T_WIFI].stat, sizeof t[T_WIFI].stat, "%lu wifi %lubt", (unsigned long)wifidrive::wifiCount(), (unsigned long)wifidrive::bleCount());
    else snprintf(t[T_WIFI].stat, sizeof t[T_WIFI].stat, "wigle wardrive");
    setTile(t[T_FIELD], "FIELD", "", IC_FIELD, field::sosOn() || field::rangeOn() || field::trailOn(), TILE_KEY[T_FIELD]);
    snprintf(t[T_FIELD].stat, sizeof t[T_FIELD].stat, "%s", field::sosOn() ? "SOS ON" : field::rangeOn() ? "range test on" : field::trailOn() ? "trail on" : "sos range trl");
    setTile(t[T_FLOCK], "FLOCK", "", IC_FLOCK, flockhunt::listening(), TILE_KEY[T_FLOCK]);
    if (flockhunt::listening()) snprintf(t[T_FLOCK].stat, sizeof t[T_FLOCK].stat, "%lu heard", (unsigned long)flockhunt::heardCount());
    else if (flockhunt::mappedCount()) snprintf(t[T_FLOCK].stat, sizeof t[T_FLOCK].stat, "%lu mapped", (unsigned long)flockhunt::mappedCount());
    else snprintf(t[T_FLOCK].stat, sizeof t[T_FLOCK].stat, "alpr cameras");
    setTile(t[T_SD], "SD CARD", _sdText.c_str(), IC_SD, false, TILE_KEY[T_SD]);
    setTile(t[T_MORE], "MORE", "all tools", IC_MORE, false, TILE_KEY[T_MORE]);
  }
  void launch(int i) {
    switch (i) {
      case T_DRIVE: drive::open(); break;
      case T_RF: openMonitor(); break;
      case T_SPECTRUM: spectrum::open(); break;
      case T_AIR: openAirtime(); break;
      case T_RADAR: openRadar(); break;
      case T_TRIP: openTrip(); break;
      case T_WARDRV: wardrive::openMenu(); break;
      case T_MECP: mecp::urgentWaiting() ? mecp::openInbox() : mecp::open(); break;
      case T_FOX: fox::open(); break;
      case T_WIFI: wifidrive::openMenu(); break;
      case T_FIELD: field::openMenu(); break;
      case T_FLOCK: flockhunt::openMenu(); break;
      case T_SD: sdview::open(); break;
      default: app::openTools(); break;
    }
  }
};

void open() { nav.push(new HubView()); }

}  // namespace deck
