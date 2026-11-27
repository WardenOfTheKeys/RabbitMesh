// RabbitMesh Flock hunt (see flock.h).
#include "flock.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <HTTPClient.h>
#include <BLEDevice.h>
#include "ble_up.h"
#include "node.h"
#include "needs.h"
#include <BLEScan.h>
#include <SD.h>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "app.h"
#include "dataio.h"
#include "gps.h"
#include "logstore.h"
#include "regional.h"
#include "netwifi.h"
#include "tls_client.h"
#include "wifidrive.h"
#include "audio_jingle.h"
#include "ui.h"
#include "alpr_core.h"
#include "flock_core.h"
#include "fox_core.h"
#include "direction_draw.h"

extern LogStore logs;

#ifndef RABBITMESH_VERSION
#define RABBITMESH_VERSION "dev"
#endif

namespace flockhunt {

using namespace alprcore;
using namespace flockcore;

static const char* MAP_PATH = "/rabbit/alpr/cameras.bin";
static const char* MAP_TMP = "/rabbit/alpr/cameras.tmp";
static const char* URL_US = "https://data.dontgetflocked.com/cameras.geojson.gz";
static const char* URL_CA = "https://data.dontgetflocked.com/cameras-ca.geojson.gz";
static const char* USER_AGENT = "RabbitMesh/" RABBITMESH_VERSION " (LilyGo T-Deck)";
constexpr uint32_t DOWNLOAD_EVERY_S = 20 * 3600;     // the file changes daily: at most once a day
constexpr uint32_t RETRY_AFTER_FAIL_S = 3600;
constexpr size_t DEVICE_MAX = 30000;                 // cameras kept from a T-Deck download (~1 MB of PSRAM while building)
constexpr size_t LOADED_MAX = 20000;                 // cameras held around you (the 3x3 degrees)
constexpr uint32_t MIN_FEATURES = 200;               // fewer than this in a whole file: something's wrong, keep the old map

// ---- settings ------------------------------------------------------------------------------------
static uint16_t s_radiusKm = 150;
static uint32_t s_alertsGiven = 0;                   // approach warnings since boot (Drive mode's summary)
static bool s_canada = false, s_alerts = true, s_sound = true, s_loaded = false;
static uint32_t s_lastTry = 0, s_lastOk = 0;          // unix time (or 0)

static void loadPrefs() {
  if (s_loaded) return;
  s_loaded = true;
  Preferences p;
  if (!p.begin("rm-fl", true)) return;
  s_radiusKm = p.getUShort("rad", s_radiusKm);
  s_canada = p.getBool("ca", s_canada);
  s_alerts = p.getBool("alert", s_alerts);
  s_sound = p.getBool("snd", s_sound);
  s_lastTry = p.getUInt("try", 0);
  s_lastOk = p.getUInt("ok", 0);
  p.end();
}
static void savePrefs() {
  Preferences p;
  if (!p.begin("rm-fl", false)) return;
  p.putUShort("rad", s_radiusKm);
  p.putBool("ca", s_canada);
  p.putBool("alert", s_alerts);
  p.putBool("snd", s_sound);
  p.putUInt("try", s_lastTry);
  p.putUInt("ok", s_lastOk);
  p.end();
}

static bool fix(double& la, double& lo) {
  if (!gps.hasFix()) return false;
  la = gps.fix().lat; lo = gps.fix().lon;
  return !(la == 0 && lo == 0);
}
static uint32_t nowUnix() { return app::timeValid() ? (uint32_t)app::now() : 0; }
// Where to keep the cameras round, for a download: the GPS's fix, or with none (at home on WiFi,
// indoors) the node's saved position: the last fix, or one set by hand or from the phone app.
static bool wherePos(double& la, double& lo) {
  if (fix(la, lo)) return true;
  if (!g_node) return false;
  la = g_node->prefs().node_lat; lo = g_node->prefs().node_lon;
  return !(la == 0 && lo == 0);
}

// ---- the map in memory: the cameras in the 3x3 degrees round where it was loaded ------------------
static Record* s_cams = nullptr;
static size_t s_camN = 0;
static FileInfo s_info;
static bool s_infoRead = false;
static int32_t s_loadedKey = INT32_MIN;
static uint32_t s_loadAt = 0;

static void readInfo() {
  s_infoRead = true;
  s_info = FileInfo();
  if (!sdMount()) return;
  File f = SD.open(MAP_PATH, FILE_READ);
  if (!f) return;
  uint8_t h[HEADER_BYTES];
  if (f.read(h, sizeof h) == sizeof h) readHeader(h, sizeof h, s_info);
  f.close();
}
uint32_t mappedCount() { if (!s_infoRead) readInfo(); return s_info.count; }

static void loadAround(double la, double lo) {
  s_loadedKey = cellKey(la, lo);
  s_loadAt = millis();
  s_camN = 0;
  if (!s_infoRead) readInfo();
  if (!s_info.count || !sdMounted()) return;
  if (!s_cams) s_cams = (Record*)heap_caps_malloc(LOADED_MAX * sizeof(Record), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!s_cams) return;
  File f = SD.open(MAP_PATH, FILE_READ);
  if (!f) return;
  int32_t keys[9];
  const int nk = neighbourKeys(la, lo, keys);
  // the cell index is small (12 bytes a cell): walk it
  if (!f.seek(HEADER_BYTES)) { f.close(); return; }
  const size_t recBase = HEADER_BYTES + (size_t)s_info.cells * sizeof(CellEntry);
  std::vector<CellEntry> want;
  CellEntry ce;
  for (uint32_t c = 0; c < s_info.cells; c++) {
    if (f.read((uint8_t*)&ce, sizeof ce) != sizeof ce) break;
    for (int k = 0; k < nk; k++) if (ce.key == keys[k]) { want.push_back(ce); break; }
  }
  for (const auto& w : want) {
    if (s_camN >= LOADED_MAX) break;
    const uint32_t n = w.n < LOADED_MAX - s_camN ? w.n : (uint32_t)(LOADED_MAX - s_camN);
    if (!f.seek(recBase + (size_t)w.first * sizeof(Record))) break;
    s_camN += f.read((uint8_t*)(s_cams + s_camN), n * sizeof(Record)) / sizeof(Record);
  }
  f.close();
}

// ---- the download (its own task: ~4 MB of gzip, ~39 MB of JSON to scan) -------------------------
// The task only downloads and filters into PSRAM (DL_WRITE); the SD card shares its SPI bus with
// the screen and the radio and only the main loop may use it, and the log isn't thread-safe, so
// tick() writes the file and logs.
// DL_WAITWIFI: the T-Deck's WiFi was off (or not yet joined): turned on just for the download,
// waiting for it to join a saved network; turned off again after, if this turned it on.
enum DlState : uint8_t { DL_IDLE, DL_WAITWIFI, DL_RUNNING, DL_WRITE, DL_DONE, DL_FAILED };
static bool s_dlRaisedWifi = false, s_dlQuiet = false;
static uint32_t s_dlWaitAt = 0;
constexpr uint32_t WIFI_JOIN_MS = 45000;
static bool listeningNow();
static Record* s_dlKeep = nullptr;            // the task's result, handed to tick() at DL_WRITE
static uint32_t s_dlKeepN = 0, s_dlSeen = 0;
static volatile uint8_t s_dl = DL_IDLE;
static volatile uint32_t s_dlBytes = 0, s_dlFeatures = 0, s_dlKept = 0;
static char s_dlMsg[64] = "";
static double s_dlLat = 0, s_dlLon = 0;

// HTTPClient hands the body (chunked or not) to a Stream: this one unpacks and scans it.
class MapSink : public Stream {
public:
  MapSink(uint8_t* window, Record* keep) : _gz(window), _keep(keep) {}
  size_t write(uint8_t b) override { return write(&b, 1); }
  size_t write(const uint8_t* p, size_t n) override {
    s_dlBytes += n;
    if (!_sniffed && n) { _sniffed = true; _gzip = n >= 2 ? (p[0] == 0x1f && p[1] == 0x8b) : p[0] == 0x1f; }
    auto scan = [&](const char* q, size_t k) {
      _scan.feed(q, k, [&](const Record& r) {
        if (_kept < DEVICE_MAX && within(r, s_dlLat, s_dlLon, s_radiusKm)) _keep[_kept++] = r;
      });
    };
    if (_gzip) { if (!_gz.feed(p, n, scan)) return 0; }
    else scan((const char*)p, n);
    s_dlFeatures = _scan.features;
    s_dlKept = _kept;
    return n;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
  size_t kept() const { return _kept; }
  uint32_t features() const { return _scan.features; }
  bool ok() const { return !_gzip || _gz.done(); }
private:
  Gunzip _gz;
  FeatureScanner _scan;
  Record* _keep;
  size_t _kept = 0;
  bool _sniffed = false, _gzip = false;
};

static void fail(const char* why) {            // from the task: tick() logs it
  strlcpy(s_dlMsg, why, sizeof s_dlMsg);
  s_dl = DL_FAILED;
}

static void downloadTask(void*) {
  uint8_t* window = (uint8_t*)heap_caps_malloc(TINFL_LZ_DICT_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  Record* keep = (Record*)heap_caps_malloc(DEVICE_MAX * sizeof(Record), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!window || !keep) { fail("out of memory"); }
  else {
    MapSink* sink = new MapSink(window, keep);
    TlsClient tls;
    tls.setInsecure();                         // public map data, checked for shape below (as Squatch's map tiles)
    HTTPClient http;
    http.setTimeout(20000);
    http.setUserAgent(USER_AGENT);
    if (!http.begin(tls, s_canada ? URL_CA : URL_US)) fail("couldn't reach the map site");
    else {
      http.addHeader("Accept-Encoding", "gzip");   // ~4 MB instead of ~39
      const int code = http.GET();
      if (code == 429 || code == 503) fail("the map site is busy: try again later");
      else if (code != 200) { char m[48]; snprintf(m, sizeof m, "the map site answered %d", code); fail(m); }
      else {
        http.writeToStream(sink);
        if (!sink->ok()) fail("the download was cut short");
        else if (sink->features() < MIN_FEATURES) fail("the file didn't look right: map kept as it was");
        else {
          s_dlKeepN = (uint32_t)sink->kept();      // tick() writes it to the card
          s_dlSeen = sink->features();
          s_dlKeep = keep;
          keep = nullptr;
          s_dl = DL_WRITE;
        }
      }
      http.end();
    }
    delete sink;
  }
  free(window);
  free(keep);
  vTaskDelete(nullptr);
}

// Why a download can't start now, or nullptr.
static const char* downloadBlocked() {
  if (s_dl != DL_IDLE) return "already downloading";
  if (!wifi::connected() && !wifi::savedCount()) return "save a wifi network first (settings > wifi)";
  if (listeningNow()) return "stop listening first: they share the wifi radio";
  if (wifidrive::running()) return "stop the wifi + bt wardrive first: they share the radio";
  if (!sdMount()) return "needs an sd card";
  double la, lo;
  if (!wherePos(la, lo)) return "needs a gps fix or a saved position (it keeps the cameras round you)";
  const uint32_t now = nowUnix();
  if (now && s_lastTry && now - s_lastTry < (s_lastOk >= s_lastTry ? DOWNLOAD_EVERY_S : RETRY_AFTER_FAIL_S))
    return s_lastOk >= s_lastTry ? "the map was updated today: once a day" : "the last try failed: wait an hour";
  return nullptr;
}
static void beginDownload();
static void startDownload(bool quiet = false) {
  const char* why = downloadBlocked();
  if (why) { if (!quiet) nav.toast(why, 3000); return; }
  s_dlQuiet = quiet;
  if (!wifi::connected()) {                    // bring the WiFi up just for this
    if (!wifi::enabled()) { wifi::setEnabled(true, true); s_dlRaisedWifi = true; }
    s_dl = DL_WAITWIFI;
    s_dlWaitAt = millis();
    if (!quiet) nav.toast("joining wifi for the camera map...", 3000);
    return;
  }
  beginDownload();
}
static void beginDownload() {
  wherePos(s_dlLat, s_dlLon);
  s_lastTry = nowUnix();
  savePrefs();
  s_dlBytes = s_dlFeatures = s_dlKept = 0;
  s_dlMsg[0] = 0;
  s_dl = DL_RUNNING;
  if (xTaskCreatePinnedToCore(downloadTask, "alprdl", 16384, nullptr, 1, nullptr, 1) != pdPASS) fail("couldn't start the download");
}

// ---- the radio ----------------------------------------------------------------------------------
struct Heard {
  uint8_t mac[6];
  uint8_t tier;
  bool ble;
  int8_t rssi;
  uint8_t ch;
  uint32_t first, last;          // millis
  uint32_t loggedAt;             // millis of its last log row
  uint8_t loggedTier;
  float loggedLat, loggedLon;
  bool confirmed;
  int mappedM;                   // distance to the nearest mapped camera when heard, -1 none
};
constexpr size_t HEARD_MAX = 32;
static Heard s_heard[HEARD_MAX];
static size_t s_heardN = 0;
static uint32_t s_heardTotal = 0;

// frames come in on the WiFi task: a small ring, drained in tick()
struct Event { uint8_t mac[6]; uint8_t tier; int8_t rssi; uint8_t ch; bool ble; };
constexpr size_t RING = 64;
static Event s_ring[RING];
static volatile size_t s_head = 0, s_tail = 0;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static void push(const Event& e) {
  portENTER_CRITICAL_ISR(&s_mux);
  const size_t next = (s_head + 1) % RING;
  if (next != s_tail) { s_ring[s_head] = e; s_head = next; }
  portEXIT_CRITICAL_ISR(&s_mux);
}
static bool pop(Event& e) {
  bool got = false;
  portENTER_CRITICAL(&s_mux);
  if (s_tail != s_head) { e = s_ring[s_tail]; s_tail = (s_tail + 1) % RING; got = true; }
  portEXIT_CRITICAL(&s_mux);
  return got;
}

static void sniff(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT && type != WIFI_PKT_DATA) return;
  const wifi_promiscuous_pkt_t* pkt = (const wifi_promiscuous_pkt_t*)buf;
  if (pkt->rx_ctrl.rssi < -95) return;
  const Hit h = classify(pkt->payload, (int)pkt->rx_ctrl.sig_len, type == WIFI_PKT_MGMT);
  if (h.tier == T_NONE) return;
  Event e;
  memcpy(e.mac, h.mac, 6);
  e.tier = h.tier; e.rssi = (int8_t)pkt->rx_ctrl.rssi; e.ch = (uint8_t)pkt->rx_ctrl.channel; e.ble = false;
  push(e);
}

static bool s_listen = false, s_weRaisedWifi = false;
static uint8_t s_chIdx = 0;
static uint32_t s_hopAt = 0, s_bleAt = 0;
static bool s_bleScanning = false;
static volatile bool s_bleDone = false;
static const char* s_radioNote = "";
static const uint8_t HOP[] = {11, 6, 1};

bool listening() { return s_listen; }
uint32_t heardCount() { return s_heardTotal; }

static void bleDone(BLEScanResults) { s_bleDone = true; }

// ---- the log ------------------------------------------------------------------------------------------
static char s_logPath[64] = "";
static String s_logBuf;
static uint32_t s_logFlushAt = 0;
static bool openLog() {
  if (!sdMount()) return false;
  if (!SD.exists("/rabbit")) SD.mkdir("/rabbit");
  if (!SD.exists("/rabbit/flock")) SD.mkdir("/rabbit/flock");
  if (app::timeValid()) {
    const time_t t = (time_t)app::now() + regional::offsetMin(app::now()) * 60;
    struct tm tm;
    gmtime_r(&t, &tm);
    snprintf(s_logPath, sizeof s_logPath, "/rabbit/flock/flock-%04d%02d%02d-%02d%02d%02d.csv",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  } else {
    snprintf(s_logPath, sizeof s_logPath, "/rabbit/flock/flock-up%lu.csv", (unsigned long)(millis() / 1000));
  }
  File f = SD.open(s_logPath, FILE_WRITE);
  if (!f) { s_logPath[0] = 0; return false; }
  f.println("time_utc,lat,lon,mac,tier,how,radio,rssi_dbm,channel,mapped_m,confirmed,maker_only");
  f.close();
  return true;
}
static void logFlush() {
  s_logFlushAt = millis();
  if (!s_logBuf.length()) return;
  if (s_logPath[0] && sdMounted()) {
    File f = SD.open(s_logPath, FILE_APPEND);
    if (f) { f.print(s_logBuf); f.close(); }
  }
  s_logBuf = "";
}
static void logRow(const Heard& h, double la, double lo, bool have) {
  char iso[24] = "";
  if (app::timeValid()) {
    const time_t t = (time_t)app::now();
    struct tm tm;
    gmtime_r(&t, &tm);
    strftime(iso, sizeof iso, "%Y-%m-%dT%H:%M:%SZ", &tm);
  }
  char where[32] = ",";                           // no fix: empty lat,lon rather than 0,0
  if (have) snprintf(where, sizeof where, "%.6f,%.6f", la, lo);
  char mapped[12] = "";
  if (h.mappedM >= 0) snprintf(mapped, sizeof mapped, "%d", h.mappedM);
  // maker_only: the radio's maker also makes laptops' (flock_core.h vendorOnly), e.g. "Liteon"
  const SharedVendor* v = vendorOnly(h.tier, h.mac, h.ble, h.confirmed) ? sharedVendor(h.mac) : nullptr;
  char row[220];
  snprintf(row, sizeof row, "%s,%s,%02x:%02x:%02x:%02x:%02x:%02x,%u,%s,%s,%d,%u,%s,%s,%s\n", iso, where, h.mac[0], h.mac[1],
           h.mac[2], h.mac[3], h.mac[4], h.mac[5], h.tier, tierName(h.tier), h.ble ? "ble" : "wifi", h.rssi, h.ch, mapped,
           h.confirmed ? "yes" : "no", v ? v->vendor : "");
  s_logBuf += row;
}

// ---- sounds -------------------------------------------------------------------------------------------
static ToneStep s_click[] = {{1200, 12}};
static const Jingle CLICK_J = {"fl-click", s_click, 1, WAVE_SQUARE, false};
static const ToneStep NEW_STEPS[] = {{1400, 60}, {0, 30}, {1800, 60}, {0, 30}, {2600, 140}};
static const Jingle NEW_J = {"fl-new", NEW_STEPS, 5, WAVE_SQUARE, false};
static const ToneStep NEAR_STEPS[] = {{880, 120}, {0, 60}, {880, 120}};
static const Jingle NEAR_J = {"fl-near", NEAR_STEPS, 3, WAVE_TRIANGLE, false};

// ---- detections ---------------------------------------------------------------------------------------
static foxcore::Meter s_meter;
static foxcore::Gradient s_grad;             // which way the followed camera is stronger
static foxcore::SmoothBearing s_gradSm;       // its bearing, steadied
static int s_lock = -1;                           // the heard camera the meter follows
static uint32_t s_clickAt = 0;

static void heard(const Event& e) {
  const uint32_t now = millis();
  double la = 0, lo = 0;
  const bool have = fix(la, lo);
  Heard* h = nullptr;
  for (size_t i = 0; i < s_heardN; i++) if (!memcmp(s_heard[i].mac, e.mac, 6)) { h = &s_heard[i]; break; }
  bool isNew = false;
  if (!h) {
    size_t slot = s_heardN < HEARD_MAX ? s_heardN++ : 0;
    if (slot == 0 && s_heardN == HEARD_MAX) {     // full: reuse the one heard longest ago
      for (size_t i = 1; i < HEARD_MAX; i++) if (s_heard[i].last < s_heard[slot].last) slot = i;
    }
    h = &s_heard[slot];
    memset(h, 0, sizeof *h);
    memcpy(h->mac, e.mac, 6);
    h->first = now;
    h->mappedM = -1;
    isNew = true;
    s_heardTotal++;
  }
  const bool upgraded = e.tier > h->tier;
  if (upgraded) h->tier = e.tier;
  h->ble = e.ble; h->rssi = e.rssi; h->ch = e.ch; h->last = now;
  if (have && s_camN) {
    const Near n = nearestFast(s_cams, s_camN, la, lo);
    h->mappedM = n.index >= 0 ? (int)lround(n.m) : -1;
    if (n.index >= 0 && n.m <= CONFIRM_M && h->tier >= T_OUI) h->confirmed = true;
  }
  // the meter follows the best camera heard lately (highest tier, then strongest)
  const int idx = (int)(h - s_heard);
  const Heard& cur = s_heard[s_lock < 0 ? 0 : s_lock];
  if (s_lock < 0 || s_lock == idx || now - cur.last > 15000 ||
      rank(h->tier, h->mac, h->ble, h->confirmed) > rank(cur.tier, cur.mac, cur.ble, cur.confirmed)) {
    if (s_lock != idx) { s_meter.clear(); s_grad.clear(); s_gradSm.clear(); }
    s_lock = idx;
    s_meter.feed(e.rssi, now);
    if (have) s_grad.add(la, lo, e.rssi, now);   // where it was heard from: which way is stronger
  }
  // log: first heard, a better tier, or a minute and 50 m on
  const bool moved = have && metres(h->loggedLat, h->loggedLon, la, lo) >= 50;
  if (isNew || upgraded || (now - h->loggedAt > 60000 && moved)) {
    logRow(*h, la, lo, have);
    h->loggedAt = now; h->loggedTier = h->tier; h->loggedLat = (float)la; h->loggedLon = (float)lo;
  }
  // the alarm: camera-grade only (a Liteon laptop's probe for WiFi isn't one: flock_core.h)
  if ((isNew || upgraded) && h->tier >= T_PROBE && !vendorOnly(h->tier, h->mac, h->ble, h->confirmed)) {
    if (s_sound) jingle.play(&NEW_J);
    char msg[64];
    snprintf(msg, sizeof msg, "%s%s", tierWords(h->tier), h->confirmed ? " - CONFIRMED on the map" : (h->mappedM < 0 || h->mappedM > CONFIRM_M) ? " - not on the map" : "");
    nav.toast(msg, 3000);
  }
}

static void startListening() {
  if (s_listen) return;
  if (wifi::enabled()) { nav.toast("turn off wifi (settings) first: listening hops channels", 3500); return; }
  if (wifidrive::running()) { nav.toast("stop the wifi + bt wardrive first", 3000); return; }
  if (!openLog()) nav.toast("no sd card: listening without a log", 2500);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false);
  s_weRaisedWifi = true;
  wifi_promiscuous_filter_t filt = {};
  filt.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
  esp_wifi_set_promiscuous_filter(&filt);
  esp_wifi_set_promiscuous_rx_cb(&sniff);
  if (esp_wifi_set_promiscuous(true) != ESP_OK) { s_radioNote = "the wifi radio wouldn't listen"; WiFi.mode(WIFI_OFF); return; }
  s_chIdx = 0;
  esp_wifi_set_channel(HOP[0], WIFI_SECOND_CHAN_NONE);
  s_hopAt = millis();
  const bool bleOk = rabbit_ble::up();
  BLEScan* sc = BLEDevice::getScan();
  sc->setActiveScan(false);
  sc->setInterval(300);
  sc->setWindow(30);                              // ~10%: the WiFi listener keeps most of the radio
  s_bleAt = 0;
  s_bleScanning = false;
  s_bleDone = false;
  s_listen = true;
  s_radioNote = bleOk ? "" : "bluetooth wouldn't start: wifi only";
  logs.add(LOG_INFO, "flock hunt: listening%s%s", s_logPath[0] ? ", log " : "", s_logPath);
}
static void stopListening() {
  if (!s_listen) return;
  s_listen = false;
  esp_wifi_set_promiscuous(false);
  esp_wifi_set_promiscuous_rx_cb(nullptr);
  if (s_bleScanning) { BLEDevice::getScan()->stop(); BLEDevice::getScan()->clearResults(); s_bleScanning = false; }
  if (s_weRaisedWifi && !wifi::enabled()) WiFi.mode(WIFI_OFF);
  s_weRaisedWifi = false;
  logFlush();
  logs.add(LOG_INFO, "flock hunt: stopped, %lu heard", (unsigned long)s_heardTotal);
}

static void bleTick(uint32_t now) {
  BLEScan* sc = BLEDevice::getScan();
  if (s_bleScanning) {
    if (!s_bleDone) return;
    s_bleScanning = false;
    s_bleDone = false;
    BLEScanResults res = sc->getResults();
    for (int i = 0; i < res.getCount(); i++) {
      BLEAdvertisedDevice d = res.getDevice(i);
      bool match = d.haveName() && bleNameMatches(d.getName().c_str());
      if (!match && d.haveManufacturerData()) {
        const std::string md = d.getManufacturerData();
        match = md.size() >= 2 && (((uint8_t)md[0] | ((uint8_t)md[1] << 8)) == BLE_MFG_XUNTONG);
      }
      for (int k = 0; !match && d.haveServiceUUID() && k < d.getServiceUUIDCount(); k++)
        match = bleServiceMatches(d.getServiceUUID(k).toString().c_str());
      if (!match) continue;
      Event e;
      memcpy(e.mac, d.getAddress().getNative(), 6);
      e.tier = T_OUI; e.rssi = (int8_t)d.getRSSI(); e.ch = 0; e.ble = true;
      heard(e);
    }
    sc->clearResults();
    return;
  }
  if (now - s_bleAt < 8000) return;
  s_bleAt = now;
  s_bleDone = false;
  if (sc->start(3, bleDone, false)) s_bleScanning = true;
}

// ---- approach alerts (from the map, with or without listening) ------------------------------------------
struct Alerted { float lat, lon; uint32_t at; };
static Alerted s_alerted[8];
static uint32_t s_alertAt = 0;
static int s_nearIdx = -1;
static double s_nearM = 0, s_nearBearing = 0;

static void mapTick(uint32_t now) {
  if (now - s_alertAt < 2000) return;
  s_alertAt = now;
  double la, lo;
  if (!fix(la, lo)) { s_nearIdx = -1; return; }
  if (cellKey(la, lo) != s_loadedKey || (!s_camN && now - s_loadAt > 60000)) loadAround(la, lo);
  if (!s_camN) { s_nearIdx = -1; return; }
  const Near n = nearestFast(s_cams, s_camN, la, lo);
  s_nearIdx = n.index; s_nearM = n.m; s_nearBearing = n.bearing;
  if (!s_alerts || n.index < 0 || n.m > ALERT_M) return;
  const Record& r = s_cams[n.index];
  for (auto& a : s_alerted)
    if (a.at && metres(a.lat, a.lon, r.lat, r.lon) < 30 && now - a.at < REALERT_S * 1000) return;
  size_t oldest = 0;
  for (size_t i = 1; i < 8; i++) if (s_alerted[i].at < s_alerted[oldest].at) oldest = i;
  s_alerted[oldest] = {r.lat, r.lon, now | 1};
  s_alertsGiven++;
  if (s_sound) jingle.play(&NEAR_J);
  char msg[72];
  static const char* C[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  snprintf(msg, sizeof msg, "%s camera %d m %s%s%s", brandName(r.brand), (int)lround(n.m), C[(int)((n.bearing + 22.5) / 45) % 8],
           r.dir >= 0 ? ", faces " : "", r.dir >= 0 ? C[((r.dir + 22) / 45) % 8] : "");
  nav.toast(msg, 4000);
}

bool wantsGps() { return s_listen || (s_alerts && mappedCount() > 0); }
static bool listeningNow() { return s_listen; }
bool alertsOn() { loadPrefs(); return s_alerts; }
void setAlerts(bool on) { loadPrefs(); s_alerts = on; savePrefs(); }
uint32_t alertsGiven() { return s_alertsGiven; }

// The download's result to the card, on the main loop (see DlState).
static void writeDownloaded() {
  std::vector<Record> recs(s_dlKeep, s_dlKeep + s_dlKeepN);
  free(s_dlKeep);
  s_dlKeep = nullptr;
  bool wrote = false;
  if (sdMount()) {
    if (!SD.exists("/rabbit")) SD.mkdir("/rabbit");
    if (!SD.exists("/rabbit/alpr")) SD.mkdir("/rabbit/alpr");
    SD.remove(MAP_TMP);
    File f = SD.open(MAP_TMP, FILE_WRITE);
    if (f) {
      wrote = writeFile(recs, nowUnix(), [&](const uint8_t* p, size_t n) { return f.write(p, n) == n; });
      f.close();
    }
    if (wrote) { SD.remove(MAP_PATH); wrote = SD.rename(MAP_TMP, MAP_PATH); }
  }
  if (!wrote) { fail("couldn't write the map to the sd card"); return; }
  snprintf(s_dlMsg, sizeof s_dlMsg, "%u cameras within %u km", (unsigned)recs.size(), (unsigned)s_radiusKm);
  logs.add(LOG_INFO, "camera map: %u of %lu cameras kept (%u km)", (unsigned)recs.size(), (unsigned long)s_dlSeen,
           (unsigned)s_radiusKm);
  s_lastOk = nowUnix();
  s_dl = DL_DONE;
}

// The map's own upkeep: once a day while the T-Deck is on WiFi, for anyone who has downloaded
// it before (nobody gets 4 MB they didn't ask for), quietly - a toast only when it's done.
static void autoRefresh(uint32_t now) {
  static uint32_t checkAt = 0;
  if (checkAt && now - checkAt < 10UL * 60UL * 1000UL) return;
  checkAt = now | 1;
  if (s_dl != DL_IDLE || !wifi::connected() || s_listen || !s_lastOk) return;
  const uint32_t t = nowUnix();
  if (!t || t - s_lastOk < DOWNLOAD_EVERY_S) return;
  startDownload(true);
}

void tick() {
  const uint32_t now = millis();
  if (s_dl == DL_WAITWIFI) {
    if (wifi::connected()) beginDownload();
    else if (now - s_dlWaitAt > WIFI_JOIN_MS) fail("couldn't join a saved wifi network");
  }
  if ((s_dl == DL_DONE || s_dl == DL_FAILED) && s_dlRaisedWifi) {   // this turned it on: off again
    s_dlRaisedWifi = false;
    if (wifi::enabled()) wifi::setEnabled(false);
  }
  if (s_dl == DL_IDLE) autoRefresh(now);
  if (s_dl == DL_WRITE) writeDownloaded();
  if (s_dl == DL_DONE) { s_dl = DL_IDLE; s_infoRead = false; readInfo(); s_loadedKey = INT32_MIN; nav.toast(s_dlMsg, 3500); savePrefs(); }
  if (s_dl == DL_FAILED) { s_dl = DL_IDLE; logs.add(LOG_WARN, "camera map: %s", s_dlMsg); if (!s_dlQuiet) nav.toast(s_dlMsg, 3500); savePrefs(); }
  loadPrefs();
  if (s_alerts || s_listen) mapTick(now);
  if (!s_listen) return;
  if (wifi::enabled()) { stopListening(); nav.toast("wifi came on: flock listening stopped", 3000); return; }
  if (now - s_hopAt >= 250) {
    s_hopAt = now;
    s_chIdx = (s_chIdx + 1) % sizeof HOP;
    esp_wifi_set_channel(HOP[s_chIdx], WIFI_SECOND_CHAN_NONE);
  }
  Event e;
  for (int k = 0; k < 32 && pop(e); k++) heard(e);
  bleTick(now);
  if (s_sound && s_lock >= 0 && s_meter.fresh(now, 15000) && now - s_clickAt >= foxcore::clickGapMs(foxcore::strength(s_meter.avg))) {
    s_clickAt = now;
    s_click[0].freq = foxcore::clickHz(foxcore::strength(s_meter.avg));
    jingle.play(&CLICK_J);
  }
  if (now - s_logFlushAt > 5000 || s_logBuf.length() > 4096) logFlush();
}

// ---- screens ---------------------------------------------------------------------------------------------
static const char* compass(double deg) {
  static const char* C[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  return C[(int)((deg + 22.5) / 45) % 8];
}

class FlockView : public View {
public:
  void tick() override { if (millis() - _at > 300) { _at = millis(); dirty = true; } }
  void draw(Canvas& g) override {
    const Theme& t = nav.theme();
    drawHeader(g, "Flock hunt", s_listen ? "press: stop listening" : "press: listen");
    const uint32_t now = millis();
    int y = L::BODY_Y + 2;
    char line[80];
    g.setFont(&fonts::Font2);
    // the radio
    g.setTextColor(s_listen ? t.green : t.dim);
    if (s_listen) {
      unsigned cams = 0;                          // of those in the table now, the camera-grade ones
      for (size_t i = 0; i < s_heardN; i++)
        if (!vendorOnly(s_heard[i].tier, s_heard[i].mac, s_heard[i].ble, s_heard[i].confirmed)) cams++;
      snprintf(line, sizeof line, "LISTENING  ch %u  %lu heard, %u camera-like", HOP[s_chIdx], (unsigned long)s_heardTotal, cams);
    }
    else snprintf(line, sizeof line, "%s", s_radioNote[0] ? s_radioNote : "radio idle (wifi must be off to listen)");
    g.drawString(line, 6, y); y += 18;
    // the camera the meter follows
    if (s_lock >= 0 && now - s_heard[s_lock].last < 60000) {
      const Heard& h = s_heard[s_lock];
      const bool maker = vendorOnly(h.tier, h.mac, h.ble, h.confirmed);
      g.setTextColor(maker ? t.dim : h.tier >= T_PROBE ? t.red : t.amber);
      char words[48];
      hitWords(words, sizeof words, h.tier, h.mac, h.ble, h.confirmed);
      snprintf(line, sizeof line, "%s  %s", words, h.ble ? "(bluetooth)" : "");
      g.drawString(line, 6, y); y += 16;
      g.setTextColor(t.txt);
      snprintf(line, sizeof line, "%02x:%02x:%02x:%02x:%02x:%02x  %d dBm  %lus ago", h.mac[0], h.mac[1], h.mac[2], h.mac[3],
               h.mac[4], h.mac[5], (int)lroundf(s_meter.avg), (unsigned long)((now - h.last) / 1000));
      g.drawString(line, 6, y); y += 16;
      // the meter
      const int mx = 6, mw = L::W - 12, mh = 12;
      g.drawRect(mx, y, mw, mh, t.line);
      const float s = foxcore::strength(s_meter.avg);
      g.fillRect(mx + 2, y + 2, (int)((mw - 4) * s), mh - 4, s > 0.66f ? t.red : s > 0.33f ? t.amber : t.green);
      y += mh + 4;
      // which way is stronger: "GO AHEAD-RIGHT" with an arrow up your way (direction_draw.h)
      y = dirdraw::draw(g, t, 6, y, L::W, s_grad, s_gradSm, gps.hasFix(), s_meter.have, now, gps.heard(), gps.bestSignal());
      g.setTextColor(h.confirmed ? t.green : t.amber);
      if (h.confirmed) snprintf(line, sizeof line, "CONFIRMED: mapped camera %d m away", h.mappedM);
      else if (!s_camN) snprintf(line, sizeof line, "no camera map loaded to check against");
      else snprintf(line, sizeof line, "not on the map: worth adding to OpenStreetMap");
      g.drawString(line, 6, y); y += 18;
    } else {
      g.setTextColor(t.dim);
      g.drawString(s_listen ? "nothing heard yet" : "", 6, y); y += 50;
    }
    // the map
    g.setTextColor(t.txt);
    if (!mappedCount()) snprintf(line, sizeof line, "no camera map: menu > update camera map");
    else if (s_nearIdx >= 0) {
      const Record& r = s_cams[s_nearIdx];
      snprintf(line, sizeof line, "nearest mapped: %.1f km %s  %s", s_nearM / 1000.0, compass(s_nearBearing), brandName(r.brand));
    } else snprintf(line, sizeof line, "%lu cameras mapped; waiting for a gps fix", (unsigned long)mappedCount());
    g.drawString(line, 6, y); y += 16;
    if (s_nearIdx >= 0) {
      const Record& r = s_cams[s_nearIdx];
      snprintf(line, sizeof line, "%s%s%s%.20s", r.dir >= 0 ? "faces " : "", r.dir >= 0 ? compass(r.dir) : "", r.op[0] ? "  " : "", r.op);
      g.setTextColor(t.dim);
      g.drawString(line, 6, y);
    }
    g.setTextColor(t.dim);
    g.drawString("map (c) OpenStreetMap contributors  sigs: flock-you", 6, L::H - 16);
  }
  void press() override { if (s_listen) stopListening(); else startListening(); dirty = true; }
  bool coasts() override { return false; }
private:
  uint32_t _at = 0;
};

static String builtText() {
  if (!s_info.built) return String("");
  const time_t t = (time_t)s_info.built;
  struct tm tm;
  gmtime_r(&t, &tm);
  char b[24];
  strftime(b, sizeof b, "%Y-%m-%d", &tm);
  return String(b);
}

void openMenu() {
  loadPrefs();
  readInfo();
  auto* m = new MenuView("Flock hunt");
  m->rebuild = [](MenuView& v) {
    v.info("to listen", []() -> String { return needs::line(needs::WIFI_OFF); });
    v.info("for the map", []() -> String { return needs::line(needs::SD_CARD) + (wifi::savedCount() ? ", wifi saved ok" : ", wifi saved NO"); });
    v.info("radio", []() -> String { return s_listen ? String("listening, ") + s_heardTotal + " heard" : String("idle"); });
    v.action("live view", [] { nav.push(new FlockView()); });
    v.action("listen on / off", [] { if (s_listen) stopListening(); else startListening(); });
    v.header("camera map");
    v.info("on the card", []() -> String {
      if (s_dl == DL_RUNNING) return String("downloading ") + (s_dlBytes / 1024) + " KB, " + s_dlKept + " kept";
      if (s_dl == DL_WRITE) return String("saving to the sd card...");
      if (s_dl == DL_WAITWIFI) return String("joining wifi...");
      const uint32_t n = mappedCount();
      return n ? String(n) + " cameras " + builtText() : String("none yet");
    });
    v.action("update camera map", [] { startDownload(); });
    v.adjust("keep cameras within", []() -> String { return String(s_radiusKm) + " km"; }, [](int d) {
      static const uint16_t R[] = {50, 100, 150, 250, 500};
      int i = 0;
      for (int k = 0; k < 5; k++) if (R[k] <= s_radiusKm) i = k;
      s_radiusKm = R[constrain(i + d, 0, 4)];
      savePrefs();
    });
    v.toggle("canada (else the us)", [] { return s_canada; }, [] { s_canada = !s_canada; savePrefs(); });
    v.toggle("warn approaching a camera", [] { return s_alerts; }, [] { s_alerts = !s_alerts; savePrefs(); });
    v.toggle("sounds", [] { return s_sound; }, [] { s_sound = !s_sound; savePrefs(); });
    v.header("about");
    v.info("map", []() -> String { return String("(c) OpenStreetMap contributors, via deflock-data"); });
    v.info("signatures", []() -> String { return String("flock-you: NitekryDPaul, DeFlockJoplin"); });
    v.info("bigger map", []() -> String { return String("tools/alpr_extract.py on a computer"); });
  };
  m->rebuild(*m);
  nav.push(m);
}

}  // namespace flockhunt
