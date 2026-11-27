// RabbitMesh WiFi + Bluetooth wardrive (see wifidrive.h).
#include "wifidrive.h"
#include <WiFi.h>
#include <vector>
#include <algorithm>
#include <BLEDevice.h>
#include <BLEScan.h>
#include "ble_up.h"
#include <SD.h>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include "app.h"
#include "dataio.h"
#include "gps.h"
#include "logstore.h"
#include "regional.h"
#include "netwifi.h"
#include "ui.h"
#include "wigle_core.h"
#include "flock.h"
#include "needs.h"
#include "fox.h"
#include <esp_idf_version.h>
#include <esp_system.h>

#ifndef RABBITMESH_VERSION
#define RABBITMESH_VERSION "dev"
#endif

extern LogStore logs;

namespace wifidrive {

using namespace wiglecore;

constexpr uint32_t WIFI_EVERY_MS = 8000, BLE_EVERY_MS = 10000, BLE_LISTEN_S = 3;
constexpr uint32_t RESEEN_S = 60;            // a repeat row needs this long AND ...
constexpr double RESEEN_MOVE_M = 50;          // ... this far from where its last row was
constexpr size_t SEEN_SLOTS = 4096;          // per kind: 64 KB each, in PSRAM

static bool s_wifiOn = true, s_bleOn = true, s_loaded = false;
static bool s_on = false, s_weRaisedWifi = false;
static char s_path[64] = "";
static Seen::Slot *s_wifiMem = nullptr, *s_bleMem = nullptr;
static Seen* s_wifiSeen = nullptr;
static Seen* s_bleSeen = nullptr;
static uint32_t s_wifiN = 0, s_bleN = 0, s_openN = 0, s_rows = 0, s_full = 0, s_startedAt = 0;
static uint32_t s_wifiAt = 0, s_bleAt = 0, s_flushAt = 0;
static bool s_wifiScanning = false, s_bleScanning = false;
static volatile bool s_bleDone = false;
// The core gives up on an async WiFi scan after 6 s and reports it failed even once the results
// are in (Bluetooth sharing the radio makes scans that slow), so the scan-done event says when
// it's finished and the results are counted directly, as netwifi.cpp does.
static volatile bool s_wifiDoneEvt = false;
static wifi_event_id_t s_wifiEvtId = 0;
static uint32_t s_wifiScanStart = 0;
static uint16_t s_wifiNow = 0, s_bleNow = 0;   // in range in the last scan, fix or not
static String s_buf;
static String s_recent[6];
// Lately seen, one per address, for "follow one" (the fox hunt); see recent().
static constexpr size_t CANDS = 24;
static Recent s_cands[CANDS];
static bool s_paused = false;
static void noteCand(bool ble, const uint8_t mac[6], int ch, int rssi, bool rnd, const char* name) {
  size_t slot = 0;
  for (size_t i = 0; i < CANDS; i++) {
    if (s_cands[i].at && !memcmp(s_cands[i].mac, mac, 6) && s_cands[i].ble == ble) { slot = i; break; }
    if (!s_cands[i].at) { slot = i; break; }
    if (s_cands[i].at < s_cands[slot].at) slot = i;          // full: the longest-ago one goes
  }
  Recent& c = s_cands[slot];
  memcpy(c.mac, mac, 6);
  c.ch = (uint8_t)ch; c.ble = ble; c.randomAddr = rnd; c.rssi = (int8_t)rssi; c.at = millis() | 1;
  snprintf(c.name, sizeof c.name, "%s", name && name[0] ? name : "(no name)");
  for (char* p = c.name; *p; p++) if ((uint8_t)*p < 32 || (uint8_t)*p > 126) *p = '?';
}
static uint8_t s_recentNext = 0;
static const char* s_wifiNote = "";
static const char* s_bleNote = "";

static void loadPrefs() {
  if (s_loaded) return;
  s_loaded = true;
  Preferences p;
  if (!p.begin("rm-wg", true)) return;
  s_wifiOn = p.getBool("wifi", s_wifiOn);
  s_bleOn = p.getBool("ble", s_bleOn);
  p.end();
}
static void savePrefs() {
  Preferences p;
  if (!p.begin("rm-wg", false)) return;
  p.putBool("wifi", s_wifiOn);
  p.putBool("ble", s_bleOn);
  p.end();
}

bool running() { return s_on; }
uint32_t wifiCount() { return s_wifiN > s_wifiNow ? s_wifiN : s_wifiNow; }
uint32_t bleCount() { return s_bleN > s_bleNow ? s_bleN : s_bleNow; }

static bool fix(double& la, double& lo) {
  if (!gps.hasFix()) return false;
  la = gps.fix().lat; lo = gps.fix().lon;
  return !(la == 0 && lo == 0);
}

// "YYYY-MM-DD HH:MM:SS" UTC: the mesh clock if set, else the GPS's own date and time.
static bool utc(char* out, size_t cap) {
  if (app::timeValid()) {
    const time_t t = (time_t)app::now();
    struct tm tm;
    gmtime_r(&t, &tm);
    strftime(out, cap, "%Y-%m-%d %H:%M:%S", &tm);
    return true;
  }
  const GpsFix& f = gps.fix();
  if (!f.year) return false;
  snprintf(out, cap, "%04u-%02u-%02u %02u:%02u:%02u", f.year, f.month, f.day, f.hour, f.minute, f.second);
  return true;
}

static void flush() {
  s_flushAt = millis();
  if (!s_buf.length()) return;
  if (s_path[0] && sdMounted()) {
    File f = SD.open(s_path, FILE_APPEND);
    if (f) { f.print(s_buf); f.close(); }
    else sdWriteFailed("wigle log");
  }
  s_buf = "";
}

// One of the latest seen, for the live page: the name and signal, and under it (dimmed) its
// address and channel, as the flock hunt shows a camera's. A Bluetooth device on a random
// address (most phones, for privacy) says so: that address changes, it isn't the device's own.
static void remember(bool ble, const uint8_t mac[6], int ch, int rssi, bool open, bool randomAddr, const char* name) {
  noteCand(ble, mac, ch, rssi, randomAddr, name);
  char top[48], m[18], sub[48];
  snprintf(top, sizeof top, "%s %4d  %.30s", ble ? "BT  " : (open ? "OPEN" : "WiFi"), rssi, name && name[0] ? name : "(no name)");
  macString(m, mac);
  if (ble) snprintf(sub, sizeof sub, "#      %s%s", m, randomAddr ? "  (random)" : "");
  else snprintf(sub, sizeof sub, "#      %s  ch %d", m, ch);
  s_recent[s_recentNext] = String(top) + "\n" + sub;
  s_recentNext = (s_recentNext + 1) % 6;
}

// One sighting: counted, and logged if it's new or due again.
static void sighting(bool ble, const uint8_t mac[6], const char* name, const char* caps, int ch, int freq, int mfgr,
                     int rssi, bool open, double la, double lo, const char* when, bool randomAddr = false) {
  Seen* seen = ble ? s_bleSeen : s_wifiSeen;
  const int r = seen->note(macKey(mac), millis() / 1000, RESEEN_S, la, lo, RESEEN_MOVE_M);
  if (r == 0) return;
  if (r < 0) { s_full++; return; }
  if (r == 1) {
    if (ble) s_bleN++; else { s_wifiN++; if (open) s_openN++; }
    remember(ble, mac, ch, rssi, open, randomAddr, name);
  }
  char m[18], line[256];
  macString(m, mac);
  const int n = row(line, sizeof line, m, name, caps, when, ch, freq, rssi, la, lo, gps.fix().altitudeM,
                    accuracyM(gps.fix().hdop, gps.fix().satellites), mfgr, ble);
  if (n > 0) { s_buf += line; s_rows++; }
}

// Seen without a GPS fix: shown on the live page, not counted or logged (WiGLE needs a position).
static void glimpse(bool ble, const uint8_t mac[6], int ch, int rssi, bool open, bool randomAddr, const char* name) {
  remember(ble, mac, ch, rssi, open, randomAddr, name);
}

static void bleDone(BLEScanResults) { s_bleDone = true; }

static bool openLog() {
  if (!sdMount()) return false;
  if (!SD.exists("/rabbit")) SD.mkdir("/rabbit");
  if (!SD.exists("/rabbit/wigle")) SD.mkdir("/rabbit/wigle");
  if (app::timeValid()) {
    const time_t t = (time_t)app::now() + regional::offsetMin(app::now()) * 60;
    struct tm tm;
    gmtime_r(&t, &tm);
    snprintf(s_path, sizeof s_path, "/rabbit/wigle/wigle-%04d%02d%02d-%02d%02d%02d.csv",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  } else {
    snprintf(s_path, sizeof s_path, "/rabbit/wigle/wigle-up%lu.csv", (unsigned long)(millis() / 1000));
  }
  File f = SD.open(s_path, FILE_WRITE);
  if (!f) { s_path[0] = 0; return false; }
  char pre[256];
  preHeader(pre, sizeof pre, RABBITMESH_VERSION, esp_get_idf_version());
  f.println(pre);
  f.println(HEADER);
  f.close();
  return true;
}

static Seen::Slot* psAlloc(size_t n) {
  void* p = heap_caps_malloc(n * sizeof(Seen::Slot), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return (Seen::Slot*)p;                       // PSRAM only: 64 KB is too much for the internal heap BLE needs
}

static void start() {
  loadPrefs();
  if (!s_wifiOn && !s_bleOn) { nav.toast("turn on wifi or bluetooth scanning first", 3000); return; }
  if (flockhunt::listening()) { nav.toast("stop the flock hunt's listening first: they share the radios", 3000); return; }
  if (!s_wifiMem) s_wifiMem = psAlloc(SEEN_SLOTS);
  if (!s_bleMem) s_bleMem = psAlloc(SEEN_SLOTS);
  if (!s_wifiMem || !s_bleMem) { nav.toast("out of memory"); return; }
  if (!s_wifiSeen) s_wifiSeen = new Seen(s_wifiMem, SEEN_SLOTS); else s_wifiSeen->clear();
  if (!s_bleSeen) s_bleSeen = new Seen(s_bleMem, SEEN_SLOTS); else s_bleSeen->clear();
  if (!openLog()) { s_path[0] = 0; nav.toast("no sd card: scanning without a log", 3000); }
  s_weRaisedWifi = false;
  if (s_wifiOn && !wifi::enabled()) {           // Squatch's wifi is off: bring the radio up just to scan
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false);
    s_weRaisedWifi = true;
  }
  s_bleNote = "";
  if (s_bleOn && !rabbit_ble::up()) s_bleNote = "bluetooth wouldn't start";
  if (s_bleOn) {
    BLEScan* sc = BLEDevice::getScan();
    sc->setActiveScan(false);                    // listen only
    sc->setInterval(100);
    sc->setWindow(60);
  }
  s_on = true;
  s_wifiN = s_bleN = s_openN = s_rows = s_full = 0;
  s_wifiAt = s_bleAt = 0;
  s_wifiScanning = s_bleScanning = false;
  s_bleDone = false;
  s_wifiDoneEvt = false;
  s_paused = false;
  s_wifiNow = s_bleNow = 0;
  s_wifiNote = "";
  if (!s_wifiEvtId) s_wifiEvtId = WiFi.onEvent([](arduino_event_id_t, arduino_event_info_t) { s_wifiDoneEvt = true; },
                                               ARDUINO_EVENT_WIFI_SCAN_DONE);
  s_buf = "";
  for (auto& r : s_recent) r = "";
  s_startedAt = s_flushAt = millis();
  logs.add(LOG_INFO, "wifi/bt wardrive started: %s", s_path);
}

static void stop() {
  if (!s_on) return;
  s_on = false;
  if (s_bleScanning) { BLEDevice::getScan()->stop(); BLEDevice::getScan()->clearResults(); s_bleScanning = false; }
  if (s_wifiScanning) { WiFi.scanDelete(); s_wifiScanning = false; }
  if (s_wifiEvtId) { WiFi.removeEvent(s_wifiEvtId); s_wifiEvtId = 0; }
  if (s_weRaisedWifi && !wifi::enabled()) WiFi.mode(WIFI_OFF);
  s_weRaisedWifi = false;
  flush();
  logs.add(LOG_INFO, "wifi/bt wardrive stopped: %lu wifi, %lu bt, %lu rows", (unsigned long)s_wifiN,
           (unsigned long)s_bleN, (unsigned long)s_rows);
}

static void wifiTick(uint32_t now, bool have, double la, double lo, const char* when) {
  if (!s_wifiOn) { s_wifiNote = "off"; return; }
  if (s_wifiScanning) {
    if (!s_wifiDoneEvt && now - s_wifiScanStart < 20000) return;   // still going (20 s: it never came back)
    s_wifiScanning = false;
    s_wifiDoneEvt = false;
    WiFi.scanComplete();                       // lets the core clear its "scanning" flag
    int16_t n = 0;
    while (n < 100 && WiFi.getScanInfoByIndex(n)) n++;
    s_wifiNow = (uint16_t)n;
    s_wifiNote = "";
    for (int16_t i = 0; i < n; i++) {
      const uint8_t* b = WiFi.BSSID(i);
      if (!b) continue;
      const uint8_t a = (uint8_t)WiFi.encryptionType(i);
      const int ch = WiFi.channel(i);
      noteCand(false, b, ch, WiFi.RSSI(i), false, WiFi.SSID(i).c_str());
      if (have) sighting(false, b, WiFi.SSID(i).c_str(), authString(a), ch, wifiFreqMHz(ch), -1, WiFi.RSSI(i), a == AUTH_OPEN, la, lo, when);
      else glimpse(false, b, ch, WiFi.RSSI(i), a == AUTH_OPEN, false, WiFi.SSID(i).c_str());
    }
    WiFi.scanDelete();
    return;
  }
  if (now - s_wifiAt < WIFI_EVERY_MS) return;
  s_wifiAt = now;
  s_wifiDoneEvt = false;
  if (WiFi.scanNetworks(true, true, false, 120) == WIFI_SCAN_RUNNING) {
    s_wifiScanning = true;
    s_wifiScanStart = now;
  } else {
    // A scan can't start while the T-Deck's own wifi is joining a network: try again shortly.
    s_wifiNote = wifi::enabled() && !wifi::connected() ? "radio busy joining a network, retrying" : "couldn't start a scan, retrying";
    s_wifiAt = now - WIFI_EVERY_MS + 2000;
  }
}

static void bleTick(uint32_t now, bool have, double la, double lo, const char* when) {
  if (!s_bleOn) return;
  BLEScan* sc = BLEDevice::getScan();
  if (s_bleScanning) {
    if (!s_bleDone) return;
    s_bleScanning = false;
    s_bleDone = false;
    {
      BLEScanResults res = sc->getResults();
      const int n = res.getCount();
      s_bleNow = (uint16_t)n;
      for (int i = 0; i < n; i++) {
        BLEAdvertisedDevice d = res.getDevice(i);
        const uint8_t* mac = (const uint8_t*)d.getAddress().getNative();
        const std::string nm = d.haveName() ? d.getName() : std::string();
        int mfgr = -1;                           // the Bluetooth company id: the manufacturer data's first two bytes, little-endian
        if (d.haveManufacturerData()) {
          const std::string md = d.getManufacturerData();
          if (md.size() >= 2) mfgr = (uint8_t)md[0] | ((uint8_t)md[1] << 8);
        }
        const bool rnd = d.getAddressType() == BLE_ADDR_TYPE_RANDOM;
        noteCand(true, mac, 0, d.getRSSI(), rnd, nm.c_str());
        if (have) sighting(true, mac, nm.c_str(), BLE_CAPS, 0, 0, mfgr, d.getRSSI(), false, la, lo, when, rnd);
        else glimpse(true, mac, 0, d.getRSSI(), false, rnd, nm.c_str());
      }
    }
    sc->clearResults();
    return;
  }
  if (now - s_bleAt < BLE_EVERY_MS) return;
  s_bleAt = now;
  s_bleDone = false;
  if (sc->start(BLE_LISTEN_S, bleDone, false)) { s_bleScanning = true; s_bleNote = ""; }
  else s_bleNote = rabbit_ble::up() ? "couldn't start a scan, retrying" : "bluetooth wouldn't start";
}

void setPaused(bool on) {
  if (on == s_paused) return;
  s_paused = on;
  if (on) {                                     // hand the radios over clean
    if (s_wifiScanning) { WiFi.scanDelete(); s_wifiScanning = false; }
    if (s_bleScanning) { BLEDevice::getScan()->stop(); BLEDevice::getScan()->clearResults(); s_bleScanning = false; }
    s_bleDone = false;
  } else {
    s_wifiAt = s_bleAt = 0;                     // scan again straight away
  }
}
bool paused() { return s_paused; }

size_t recent(Recent* out, size_t max) {
  const uint32_t now = millis();
  std::vector<Recent> v;
  for (const Recent& c : s_cands) if (c.at && now - c.at < 120000) v.push_back(c);
  std::sort(v.begin(), v.end(), [](const Recent& a, const Recent& b) { return a.rssi > b.rssi; });
  size_t n = 0;
  for (const Recent& c : v) { if (n >= max) break; out[n++] = c; }
  return n;
}

void tick() {
  if (!s_on) return;
  if (s_paused) { if (millis() - s_flushAt > 5000) flush(); return; }   // the fox hunt has the radios
  const uint32_t now = millis();
  double la = 0, lo = 0;
  char when[24] = "";
  const bool have = fix(la, lo) && utc(when, sizeof when);
  wifiTick(now, have, la, lo, when);
  bleTick(now, have, la, lo, when);
  if (now - s_flushAt > 5000 || s_buf.length() > 8192) flush();
}

void setRunning(bool on) { loadPrefs(); if (on && !s_on) start(); else if (!on && s_on) stop(); }
uint32_t openCount() { return s_openN; }
uint32_t rowsLogged() { return s_rows; }
const char* logPath() { return s_path; }

// ---- screens ---------------------------------------------------------------------------------
static String elapsed(uint32_t ms) {
  const uint32_t s = ms / 1000;
  char b[16];
  if (s < 3600) snprintf(b, sizeof b, "%lum%02lus", (unsigned long)(s / 60), (unsigned long)(s % 60));
  else snprintf(b, sizeof b, "%luh%02lum", (unsigned long)(s / 3600), (unsigned long)(s / 60 % 60));
  return String(b);
}

static void livePage() {
  auto* v = new TextPageView("WiFi + BT wardrive", [](std::vector<String>& out) {
    out.push_back(s_on ? String(">> SCANNING  ") + elapsed(millis() - s_startedAt) : String("stopped"));
    if (!needs::met(needs::GPS | needs::SD_CARD)) out.push_back(String("#") + needs::line(needs::GPS | needs::SD_CARD) + " (to log)");
    double la, lo;
    out.push_back(fix(la, lo) ? String("gps   ") + String(la, 5) + ", " + String(lo, 5) + "  " + gps.fix().satellites + " sats"
                              : String("gps   no fix - scanning, logging starts with a fix"));
    out.push_back(String("wifi  ") + s_wifiNow + " in range, " + s_wifiN + " logged, " + s_openN + " open" +
                  (s_wifiOn ? (s_wifiNote[0] ? String("  (") + s_wifiNote + ")" : String("")) : String("  (scanning off)")));
    out.push_back(String("bt    ") + s_bleNow + " in range, " + s_bleN + " logged" +
                  (s_bleOn ? (s_bleNote[0] ? String("  (") + s_bleNote + ")" : String("")) : String("  (scanning off)")));
    out.push_back(String("rows  ") + s_rows + (s_full ? String("  (table full: ") + s_full + " not logged)" : String("")));
    const char* file = strrchr(s_path, '/');        // the name only: the folder is on the menu
    out.push_back(s_path[0] ? String("log   ") + (file ? file + 1 : s_path) : String("log   none (no sd card)"));
    out.push_back("");
    bool any = false;
    for (int i = 0; i < 6; i++) {
      const String& r = s_recent[(s_recentNext + 5 - i) % 6];
      if (!r.length()) continue;
      const int nl = r.indexOf('\n');                // the name line, then its address line
      out.push_back(nl < 0 ? r : r.substring(0, nl));
      if (nl >= 0) out.push_back(r.substring(nl + 1));
      any = true;
    }
    if (!any) out.push_back(s_on ? "nothing seen yet" : "");
    out.push_back("");
    out.push_back(s_on ? "press: stop" : "press: start");
    out.push_back("#to walk one down with the arrow: menu > follow one");
  }, 1000);
  v->onPress = [] { if (s_on) stop(); else start(); };
  nav.push(v);
}

// Pick one of what it has seen lately and follow it in the fox hunt: the signal meter, the
// Geiger click and, walking with a GPS fix, the "which way is stronger" arrow.
static void followMenu() {
  auto* m = new MenuView("Follow one");
  m->rebuild = [](MenuView& v) {
    Recent list[16];
    const size_t n = recent(list, 16);
    if (!n) { v.info("nothing", []() -> String { return String(s_on ? "seen in the last 2 minutes yet" : "start the wardrive first"); }); return; }
    v.info("how", []() -> String { return String("pick one: the fox hunt follows it, with the arrow"); });
    for (size_t i = 0; i < n; i++) {
      const Recent c = list[i];
      char label[48];
      snprintf(label, sizeof label, "%s %4d  %.20s%s", c.ble ? "BT  " : "WiFi", c.rssi, c.name, c.randomAddr ? " (random)" : "");
      v.action(label, [c] {
        if (c.ble && c.randomAddr) nav.toast("a random address: it may change and vanish", 3000);
        fox::follow(c.ble, c.mac, c.ch, c.randomAddr, c.name);
      });
    }
  };
  m->rebuild(*m);
  nav.push(m);
}

void openMenu() {
  loadPrefs();
  auto* m = new MenuView("WiFi + BT wardrive");
  m->rebuild = [](MenuView& v) {
    v.info("needs", []() -> String { return needs::line(needs::GPS | needs::SD_CARD) + "  (to log)"; });
    v.info("status", []() -> String {
      if (!s_on) return String("stopped");
      return String("running  ") + s_wifiN + " wifi  " + s_bleN + " bt";
    });
    v.action("start / stop", [] {
      if (s_on) { stop(); nav.toast("wardrive stopped, log saved"); }
      else { start(); if (s_on && s_path[0]) nav.toast("scanning - logs go to the sd card", 3000); }
    });
    v.action("live view", [] { livePage(); });
    v.action("follow one (with the direction arrow)", [] { followMenu(); });
    v.header("scan");
    v.toggle("wifi access points", [] { return s_wifiOn; }, [] { s_wifiOn = !s_wifiOn; savePrefs(); });
    v.toggle("bluetooth le devices", [] { return s_bleOn; }, [] { s_bleOn = !s_bleOn; savePrefs(); });
    v.info("format", []() -> String { return String("WiGLE CSV: upload at wigle.net"); });
    v.info("folder", []() -> String { return String("/rabbit/wigle on the sd card"); });
    v.info("gps", []() -> String { return s_on ? String("kept on while scanning") : String("as set in settings"); });
  };
  m->rebuild(*m);
  nav.push(m);
}

}  // namespace wifidrive
