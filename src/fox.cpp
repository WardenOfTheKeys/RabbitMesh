// RabbitMesh fox hunt (see fox.h).
#include "fox.h"
#include <vector>
#include "app.h"
#include "node.h"
#include "fieldtools.h"
#include "wardrive.h"
#include "audio_jingle.h"
#include "ui.h"
#include "fox_core.h"
#include "direction_draw.h"
#include "gps.h"
#include <WiFi.h>
#include <algorithm>
#include "netwifi.h"
#include "wifidrive.h"
#include "flock.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include "ble_up.h"

namespace fox {

using namespace foxcore;

constexpr uint32_t PROBE_MS = 8000;      // a ping every 8 s: zero-hop, but it's still airtime
constexpr uint32_t HARVEST_MS = 5000;    // answers come in within this
constexpr uint32_t STALE_ANY_MS = 20000, STALE_REP_MS = 25000, STALE_WIFI_MS = 5000, STALE_BT_MS = 6000;
constexpr uint32_t BT_LIST_S = 3, BT_FOLLOW_S = 1;   // a 3 s listen for the list, then 1 s listens
constexpr uint32_t WIFI_EVERY_MS = 700;  // one channel, one network: a reading about every second
constexpr uint8_t WIFI_MISSES_WIDEN = 3; // not on its channel this many times: look on every channel

struct Target { uint8_t key[4]; char name[24]; };
struct Ap { uint8_t bssid[6]; uint8_t ch; int8_t rssi; char name[28]; bool rnd; };   // a WiFi network or a BT device
enum Mode : uint8_t { ANY = 0, REP, WIFI, BT, MODES };

// The scan-done event: the core reports a scan that ran past 6 s as failed even once its results
// are in, so the event ends a scan and the results are counted directly (as wifidrive.cpp does).
static volatile bool s_wifiDone = false;
static volatile bool s_btDone = false;
static void btDone(BLEScanResults) { s_btDone = true; }

static ToneStep s_click[] = {{1200, 12}};
static const Jingle CLICK_J = {"fox-click", s_click, 1, WAVE_SQUARE, false};

class FoxView : public View {
public:
  FoxView() { if (g_node) _gen = g_node->pktGen; }
  ~FoxView() override { leaveMode(); }

  // Opened from the WiFi + BT wardrive's "follow one": straight to that network or device.
  void follow(bool ble, const uint8_t mac[6], uint8_t ch, bool rnd, const char* name) {
    leaveMode();
    _mode = ble ? BT : WIFI;
    enterMode();
    Ap a = {};
    memcpy(a.bssid, mac, 6);
    a.ch = ch; a.rnd = rnd;
    snprintf(a.name, sizeof a.name, "%s %02x%02x", name && name[0] ? name : "(no name)", mac[4], mac[5]);
    _aps.clear();
    _aps.push_back(a);
    _apPick = 0;
    _wAt = 0;
  }

  void tick() override {
    const uint32_t now = millis();
    if (_mode == REP) repeaterTick(now); else if (_mode == WIFI) wifiTick(now); else if (_mode == BT) btTick(now); else anyTick();
    if (_m.n != _fedN) {                         // a new reading: where it was taken, for the direction
      _fedN = _m.n;
      if (gps.hasFix() && !(gps.fix().lat == 0 && gps.fix().lon == 0)) _g.add(gps.fix().lat, gps.fix().lon, _m.last, now);
    }
    // the Geiger click
    const bool fresh = _m.fresh(now, staleMs());
    if (_sound && fresh) {
      const float s = level(_m.avg);
      if (now - _clickAt >= clickGapMs(s)) {
        _clickAt = now;
        s_click[0].freq = clickHz(s);
        jingle.play(&CLICK_J);
      }
    }
    if (now - _drawn > 200) { _drawn = now; dirty = true; }
  }

  void draw(Canvas& g) override {
    const Theme& t = nav.theme();
    drawHeader(g, "Fox hunt", _mode == REP ? "repeater  m: wifi" : _mode == WIFI ? "wifi  m: bluetooth" : _mode == BT ? "bluetooth  m: any"
                                                                                                         : "any signal  m: repeater");
    const uint32_t now = millis();
    const bool fresh = _m.fresh(now, staleMs());
    int y = L::BODY_Y + 4;
    // what we're hunting
    g.setFont(&fonts::Font2);
    g.setTextColor(t.dim);
    char line[64];
    if (_mode == WIFI || _mode == BT) {
      if (_aps.empty()) snprintf(line, sizeof line, _wListing ? (_mode == BT ? "listening for bluetooth..." : "listing wifi networks...")
                                                              : (_mode == BT ? "press: list bluetooth devices" : "press: list wifi networks"));
      else if (_mode == BT) snprintf(line, sizeof line, "target  %.23s  %u/%u </>", _aps[_apPick].name, (unsigned)_apPick + 1, (unsigned)_aps.size());
      else snprintf(line, sizeof line, "target  %.23s ch%u  %u/%u </>", _aps[_apPick].name, (unsigned)_aps[_apPick].ch,
                    (unsigned)_apPick + 1, (unsigned)_aps.size());   // fits the screen: names are cut
    } else if (_mode == REP) {
      if (_targets.empty()) snprintf(line, sizeof line, _scanning ? "looking for repeaters in range..." : "press: look for repeaters");
      else snprintf(line, sizeof line, "target  %s  (%u/%u, </>)", _targets[_pick].name, (unsigned)_pick + 1, (unsigned)_targets.size());
    } else {
      snprintf(line, sizeof line, "every packet heard  (%lu so far)", (unsigned long)_m.n);
    }
    g.drawString(line, 8, y);
    y += 22;
    // the reading
    g.setFont(&fonts::Font4);
    g.setTextColor(fresh ? t.green : t.dim);
    if (_m.have) snprintf(line, sizeof line, "%d dBm", (int)lroundf(_m.avg));
    else snprintf(line, sizeof line, "-- dBm");
    g.drawString(line, 8, y);
    g.setFont(&fonts::Font2);
    const char* tr = !_m.have || !fresh ? "" : _m.trend > 0 ? "STRONGER" : _m.trend < 0 ? "weaker" : "steady";
    g.setTextColor(_m.trend > 0 ? t.green : _m.trend < 0 ? t.red : t.txt);
    g.drawString(tr, L::W - 8 - g.textWidth(tr), y + 6);
    y += 34;
    // the meter: red (faint) to amber to the accent (close), the peak marked
    const int mx = 8, mw = L::W - 16, mh = 18;
    g.drawRect(mx, y, mw, mh, t.line);
    const int segs = 32;
    const float s = _m.have ? level(_m.avg) : 0;
    for (int i = 0; i < segs; i++) {
      const float f = (i + 0.5f) / segs;
      if (f > s) break;
      const uint16_t c = f < 0.33f ? t.red : f < 0.66f ? t.amber : t.green;
      g.fillRect(mx + 2 + i * (mw - 4) / segs, y + 2, (mw - 4) / segs - 1, mh - 4, fresh ? c : t.dim);
    }
    if (_m.have && _m.peak > -200) {
      const int px = mx + 2 + (int)(level(_m.peak) * (mw - 4));
      g.drawFastVLine(px, y - 3, mh + 6, t.white);
    }
    y += mh + 8;
    g.setTextColor(t.txt);
    if (_m.have) {
      const unsigned ago = (now - _m.at) / 1000;
      snprintf(line, sizeof line, "last %d dBm  peak %d dBm  %us ago", (int)lroundf(_m.last), (int)lroundf(_m.peak), ago);
    } else {
      snprintf(line, sizeof line, _mode == REP ? "waiting for an answer" : (_mode == WIFI || _mode == BT) ? (_aps.empty() ? "" : "waiting for a reading")
                                                                                  : "waiting for a packet");
    }
    g.drawString(line, 8, y);
    y += 18;
    y = drawDirection(g, t, y, now);
    if (_mode == REP && _probeAt) { g.setTextColor(t.dim); g.drawString("ping out, listening...", 8, y); }
    else if (_mode != ANY && _note[0]) { g.setTextColor(t.amber); g.drawString(_note, 8, y); }
    // keys
    g.setTextColor(t.dim);
    g.drawString(_sound ? "s: sound off   p: reset peak" : "s: sound on   p: reset peak", 8, L::H - 18);
  }

  void press() override {
    if (_mode == REP) startScan();
    else if (_mode == WIFI) listNetworks();
    else if (_mode == BT) listBt();
    dirty = true;
  }
  void rotate(int d) override { pickTarget(d); }
  bool roll(int dx, int dy) override { if (dx) pickTarget(dx); return dx != 0; }
  void key(char c) override {
    if (c == 'm' || c == 'M') {
      leaveMode();
      _mode = (Mode)((_mode + 1) % MODES);
      clearReadings(); _note[0] = 0; _probeAt = 0;
      _aps.clear(); _apPick = 0;
      if (_mode == ANY && g_node) _gen = g_node->pktGen;
      enterMode();
    }
    if (c == 's' || c == 'S') _sound = !_sound;
    if (c == 'p' || c == 'P') _m.resetPeak();
    if (c == '<' || c == ',') pickTarget(-1);
    if (c == '>' || c == '.') pickTarget(1);
    dirty = true;
  }
  bool coasts() override { return false; }

private:
  Meter _m;
  Gradient _g;                                   // which way is stronger (fox_core.h)
  SmoothBearing _sm;                             // its bearing, steadied
  uint32_t _fedN = 0;
  Mode _mode = ANY;
  bool _sound = true, _scanning = false;
  // wifi mode
  std::vector<Ap> _aps;
  size_t _apPick = 0;
  bool _wScanning = false, _wListing = false, _wRaised = false;
  uint32_t _wAt = 0, _wStart = 0;
  uint8_t _wMiss = 0;
  wifi_event_id_t _wEvt = 0;

  // "stronger: NE  ahead-right" and a small north-up arrow, or what it needs to say that.
  // "GO AHEAD-RIGHT" with an arrow up your way, or why it can't tell yet (direction_draw.h).
  int drawDirection(Canvas& g, const Theme& t, int y, uint32_t now) {
    return dirdraw::draw(g, t, 8, y, L::W, _g, _sm, gps.hasFix(), _m.have, now, gps.heard(), gps.bestSignal());
  }
  void clearReadings() { _m.clear(); _g.clear(); _sm.clear(); _fedN = 0; }
  uint32_t staleMs() const { return _mode == REP ? STALE_REP_MS : _mode == WIFI ? STALE_WIFI_MS : _mode == BT ? STALE_BT_MS : STALE_ANY_MS; }
  float level(float rssi) const { return (_mode == WIFI || _mode == BT) ? strength(rssi, WIFI_LO, WIFI_HI) : strength(rssi); }
  uint32_t _gen = 0, _clickAt = 0, _drawn = 0, _probeAt = 0, _lastProbe = 0;
  std::vector<Target> _targets;
  size_t _pick = 0;
  char _note[48] = "";

  // Any signal: every packet received since the last tick.
  void anyTick() {
    if (!g_node) return;
    uint32_t fresh = g_node->pktGen - _gen;
    if (fresh > InwNode::PKT_LOG_MAX) fresh = InwNode::PKT_LOG_MAX;
    for (uint32_t i = 0; i < fresh; i++) {
      const PacketLogEntry& e = g_node->pktLog[(g_node->pktHead + InwNode::PKT_LOG_MAX - fresh + i) % InwNode::PKT_LOG_MAX];
      if (!e.tx && e.rssi) _m.feed(e.rssi, millis());
    }
    _gen = g_node->pktGen;
  }

  bool radioFree() {
    if (wardrive::running()) { strlcpy(_note, "the wardrive is using the pings", sizeof _note); return false; }
    if (field::rangeOn()) { strlcpy(_note, "the range test is using the pings", sizeof _note); return false; }
    return true;
  }

  void startScan() {
    if (!g_node || !radioFree() || _probeAt) return;
    if (!g_node->discover()) { strlcpy(_note, "couldn't send a ping", sizeof _note); return; }
    _note[0] = 0;
    _scanning = true;
    _probeAt = millis() | 1;
    _lastProbe = millis();
  }

  void repeaterTick(uint32_t now) {
    if (!g_node) return;
    if (_probeAt && now - _probeAt >= HARVEST_MS) {
      _probeAt = 0;
      bool answered = false;
      for (uint8_t i = 0; i < g_node->discoveredCount; i++) {
        const DiscoverHit& h = g_node->discovered[i];
        if (_scanning) addTarget(h);
        else if (!_targets.empty() && !memcmp(h.pub, _targets[_pick].key, 4)) { _m.feed(h.rssi, now); answered = true; }
      }
      if (!_scanning && !_targets.empty())
        strlcpy(_note, answered ? "" : "no answer this time - out of range?", sizeof _note);
      if (_scanning) {
        _scanning = false;
        if (_targets.empty()) strlcpy(_note, "nothing answered - try somewhere else", sizeof _note);
      }
    }
    // keep pinging the target
    if (!_targets.empty() && !_probeAt && now - _lastProbe >= PROBE_MS && radioFree()) {
      if (g_node->discover()) { _probeAt = now | 1; _note[0] = 0; }
      _lastProbe = now;
    }
  }

  // ---- wifi: one access point, by its address, on its channel -------------------------------
  // The radio is the firmware's own (netwifi.h): with its WiFi off it's brought up just for this
  // and turned off again after; the wardrive and the flock hunt have it first.
  void wifiEnter() {
    pauseWardrive();
    _wScanning = _wListing = false;
    _wMiss = 0;
    _wAt = 0;
    s_wifiDone = false;
    if (!_wEvt) _wEvt = WiFi.onEvent([](arduino_event_id_t, arduino_event_info_t) { s_wifiDone = true; },
                                     ARDUINO_EVENT_WIFI_SCAN_DONE);
    _wRaised = false;
    if (!wifi::enabled() && !flockhunt::listening() && WiFi.getMode() == WIFI_OFF) {
      WiFi.mode(WIFI_STA);
      WiFi.disconnect(false);
      _wRaised = true;
    }
  }
  void wifiLeave() {
    if (_wScanning) { WiFi.scanDelete(); _wScanning = _wListing = false; }
    if (_wEvt) { WiFi.removeEvent(_wEvt); _wEvt = 0; }
    if (_wRaised && !wifi::enabled() && !wifidrive::running() && !flockhunt::listening()) WiFi.mode(WIFI_OFF);
    _wRaised = false;
    resumeWardrive();
  }
  // The WiFi + BT wardrive pauses while this has the radios (it carries on after); the flock
  // hunt's listening can't be paused like that, so it has to be stopped first.
  const char* wifiBusy() const {
    if (flockhunt::listening()) return "the flock hunt is listening: stop it first";
    return nullptr;
  }
  bool _pausedWd = false;
  void pauseWardrive() { if (wifidrive::running() && !wifidrive::paused()) { wifidrive::setPaused(true); _pausedWd = true; } }
  void resumeWardrive() { if (_pausedWd) { wifidrive::setPaused(false); _pausedWd = false; } }
  void enterMode() { if (_mode == WIFI) wifiEnter(); else if (_mode == BT) btEnter(); }
  void leaveMode() { if (_mode == WIFI) wifiLeave(); else if (_mode == BT) btLeave(); }

  // ---- bluetooth: one device, by its address ----------------------------------------------------
  // Listens for it (passively: nothing is sent) a second at a time. A device on a random address
  // (most phones) changes it every few minutes, and then can't be followed by it.
  void btEnter() {
    pauseWardrive();
    _wScanning = _wListing = false;
    _wMiss = 0; _wAt = 0;
    s_btDone = false;
    if (!rabbit_ble::up()) { strlcpy(_note, "bluetooth wouldn't start", sizeof _note); return; }
    BLEScan* sc = BLEDevice::getScan();
    sc->setActiveScan(false);
    sc->setInterval(100);
    sc->setWindow(90);                           // listening nearly all the time
  }
  void btLeave() {
    if (_wScanning) { BLEDevice::getScan()->stop(); BLEDevice::getScan()->clearResults(); _wScanning = _wListing = false; }
    resumeWardrive();
  }
  bool btScan(uint32_t secs, uint32_t now) {
    s_btDone = false;
    if (!BLEDevice::getScan()->start(secs, btDone, false)) { strlcpy(_note, "couldn't start a bluetooth listen, retrying", sizeof _note); return false; }
    _wScanning = true;
    _wStart = now;
    return true;
  }
  void listBt() {
    if (const char* b = wifiBusy()) { strlcpy(_note, b, sizeof _note); return; }
    if (_wScanning) return;
    if (btScan(BT_LIST_S, millis())) { _wListing = true; _note[0] = 0; }
  }
  void btTick(uint32_t now) {
    if (const char* b = wifiBusy()) { strlcpy(_note, b, sizeof _note); return; }
    if (_wScanning) {
      if (!s_btDone && now - _wStart < (_wListing ? BT_LIST_S : BT_FOLLOW_S) * 1000 + 3000) return;
      _wScanning = false;
      s_btDone = false;
      BLEScan* sc = BLEDevice::getScan();
      BLEScanResults res = sc->getResults();
      const int n = res.getCount();
      if (_wListing) {
        _wListing = false;
        _aps.clear();
        for (int i = 0; i < n; i++) {
          BLEAdvertisedDevice d = res.getDevice(i);
          const uint8_t* mac = (const uint8_t*)d.getAddress().getNative();
          bool dup = false;
          for (auto& a : _aps) if (!memcmp(a.bssid, mac, 6)) dup = true;
          if (dup) continue;
          Ap a = {};
          memcpy(a.bssid, mac, 6);
          a.rssi = (int8_t)d.getRSSI();
          a.rnd = d.getAddressType() == BLE_ADDR_TYPE_RANDOM;
          const std::string nm = d.haveName() ? d.getName() : std::string();
          snprintf(a.name, sizeof a.name, "%.18s %02x%02x", nm.empty() ? "(no name)" : nm.c_str(), mac[4], mac[5]);
          for (char* p = a.name; *p; p++) if ((uint8_t)*p < 32 || (uint8_t)*p > 126) *p = '?';
          _aps.push_back(a);
        }
        // fixed addresses first (they can be followed), each lot strongest first
        std::sort(_aps.begin(), _aps.end(), [](const Ap& x, const Ap& y) { return x.rnd != y.rnd ? !x.rnd : x.rssi > y.rssi; });
        if (_aps.size() > 30) _aps.resize(30);
        _apPick = 0;
        clearReadings();
        strlcpy(_note, _aps.empty() ? "no bluetooth devices heard - press to listen again" : "", sizeof _note);
        _wMiss = 0; _wAt = 0;
      } else if (!_aps.empty()) {
        const Ap& t = _aps[_apPick];
        bool got = false;
        for (int i = 0; i < n && !got; i++) {
          BLEAdvertisedDevice d = res.getDevice(i);
          if (memcmp(d.getAddress().getNative(), t.bssid, 6)) continue;
          _m.feed(d.getRSSI(), now);
          got = true;
        }
        if (got) { _wMiss = 0; _note[0] = 0; }
        else if (_wMiss < 250 && ++_wMiss >= 3)
          strlcpy(_note, t.rnd ? "not heard - a random address may have changed" : "not heard lately - out of range?", sizeof _note);
      }
      sc->clearResults();
      return;
    }
    if (_aps.empty()) return;
    btScan(BT_FOLLOW_S, now);
  }
  bool wifiScan(uint8_t ch, const uint8_t* bssid, uint32_t now) {
    if (WiFi.getMode() == WIFI_OFF) { WiFi.mode(WIFI_STA); WiFi.disconnect(false); _wRaised = !wifi::enabled(); }
    s_wifiDone = false;
    if (WiFi.scanNetworks(true, true, false, bssid ? 110 : 120, ch, nullptr, bssid) != WIFI_SCAN_RUNNING) {
      strlcpy(_note, wifi::enabled() && !wifi::connected() ? "radio busy joining a network, retrying" : "couldn't start a scan, retrying",
              sizeof _note);
      return false;
    }
    _wScanning = true;
    _wStart = now;
    return true;
  }
  void listNetworks() {
    if (const char* b = wifiBusy()) { strlcpy(_note, b, sizeof _note); return; }
    if (_wScanning) return;                     // one at a time: the list comes after this one
    if (wifiScan(0, nullptr, millis())) { _wListing = true; _note[0] = 0; }
  }
  void wifiTick(uint32_t now) {
    if (const char* b = wifiBusy()) { strlcpy(_note, b, sizeof _note); return; }
    if (_wScanning) {
      if (!s_wifiDone && now - _wStart < (_wListing ? 20000u : 4000u)) return;
      _wScanning = false;
      s_wifiDone = false;
      WiFi.scanComplete();                     // lets the core clear its "scanning" flag
      int16_t n = 0;
      while (n < 100 && WiFi.getScanInfoByIndex(n)) n++;
      if (_wListing) {
        _wListing = false;
        uint8_t keep[6] = {0};
        const bool had = !_aps.empty();
        if (had) memcpy(keep, _aps[_apPick].bssid, 6);
        _aps.clear();
        for (int16_t i = 0; i < n; i++) {
          const uint8_t* b = WiFi.BSSID(i);
          if (!b) continue;
          bool dup = false;
          for (auto& a : _aps) if (!memcmp(a.bssid, b, 6)) dup = true;
          if (dup) continue;
          Ap a;
          memcpy(a.bssid, b, 6);
          a.ch = (uint8_t)WiFi.channel(i);
          a.rssi = (int8_t)WiFi.RSSI(i);
          const String ss = WiFi.SSID(i);
          snprintf(a.name, sizeof a.name, "%.18s %02x%02x", ss.length() ? ss.c_str() : "(hidden)", b[4], b[5]);
          for (char* p = a.name; *p; p++) if ((uint8_t)*p < 32 || (uint8_t)*p > 126) *p = '?';
          _aps.push_back(a);
        }
        std::sort(_aps.begin(), _aps.end(), [](const Ap& x, const Ap& y) { return x.rssi > y.rssi; });
        if (_aps.size() > 30) _aps.resize(30);
        _apPick = 0;
        for (size_t i = 0; had && i < _aps.size(); i++) if (!memcmp(_aps[i].bssid, keep, 6)) _apPick = i;
        if (!had || memcmp(_aps.empty() ? keep : _aps[_apPick].bssid, keep, 6)) clearReadings();
        strlcpy(_note, _aps.empty() ? "no wifi networks heard - press to look again" : "", sizeof _note);
        _wMiss = 0;
        _wAt = 0;
      } else if (!_aps.empty()) {
        Ap& t = _aps[_apPick];
        bool got = false;
        for (int16_t i = 0; i < n; i++) {
          const uint8_t* b = WiFi.BSSID(i);
          if (!b || memcmp(b, t.bssid, 6)) continue;
          _m.feed(WiFi.RSSI(i), now);
          t.ch = (uint8_t)WiFi.channel(i);       // it may have moved channel
          got = true;
          break;
        }
        if (got) { _wMiss = 0; _note[0] = 0; }
        else if (_wMiss < 250) { _wMiss++; strlcpy(_note, "not heard this time - out of range?", sizeof _note); }
      }
      WiFi.scanDelete();
      return;
    }
    if (_aps.empty() || now - _wAt < WIFI_EVERY_MS) return;
    _wAt = now;
    const Ap& t = _aps[_apPick];
    wifiScan(_wMiss >= WIFI_MISSES_WIDEN ? 0 : t.ch, t.bssid, now);   // lost on its channel: every channel
  }

  void addTarget(const DiscoverHit& h) {
    for (auto& t : _targets) if (!memcmp(t.key, h.pub, 4)) return;
    Target t;
    memcpy(t.key, h.pub, 4);
    ContactInfo* c = g_node->contact(h.pub);
    if (c) strlcpy(t.name, c->name, sizeof t.name);
    else snprintf(t.name, sizeof t.name, "%02x%02x%02x%02x", h.pub[0], h.pub[1], h.pub[2], h.pub[3]);
    for (char* p = t.name; *p; p++) if ((uint8_t)*p < 32 || (uint8_t)*p > 126) *p = '?';
    _targets.push_back(t);
  }

  void pickTarget(int d) {
    if (_mode == WIFI || _mode == BT) {
      if (_aps.empty()) return;
      _apPick = (_apPick + _aps.size() + (d > 0 ? 1 : -1)) % _aps.size();
      clearReadings();
      _wMiss = 0;
      _wAt = 0;                            // read the new one straight away
      _note[0] = 0;
      dirty = true;
      return;
    }
    if (_mode != REP || _targets.empty()) return;
    _pick = (_pick + _targets.size() + (d > 0 ? 1 : -1)) % _targets.size();
    clearReadings();
    _lastProbe = 0;                       // ping the new one straight away
    dirty = true;
  }
};

void open() { nav.push(new FoxView()); }

void follow(bool ble, const uint8_t mac[6], uint8_t ch, bool randomAddr, const char* name) {
  auto* v = new FoxView();
  v->follow(ble, mac, ch, randomAddr, name);
  nav.push(v);
}

}  // namespace fox
