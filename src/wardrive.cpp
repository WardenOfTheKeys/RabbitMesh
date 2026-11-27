// RabbitMesh wardrive (see wardrive.h for what it does and where it came from).
#include "wardrive.h"
#include <vector>
#include <algorithm>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <SD.h>
#include "app.h"
#include "node.h"
#include "dataio.h"
#include "gps.h"
#include "logstore.h"
#include "regional.h"
#include "fieldtools.h"
#include "ui.h"
#include "audio_jingle.h"
#include "needs.h"

extern LogStore logs;

namespace wardrive {

using namespace wdcore;

// ---- settings (their own Preferences namespace: the UI settings blob's layout stays put) --
static Gate s_gate;
static uint8_t s_dir = DIR_LINK;
static bool s_mapOn = true;
static bool s_cue = true;                     // a tone for each probe's result
static bool s_newAlert = true;                // a fanfare and a toast for a repeater never seen before
static bool s_loaded = false;

static void loadPrefs() {
  if (s_loaded) return;
  s_loaded = true;
  Preferences p;
  if (!p.begin("rm-wd", true)) return;
  s_gate.minGapS = p.getUShort("gap", s_gate.minGapS);
  s_gate.moveM = p.getUShort("move", s_gate.moveM);
  s_gate.idleS = p.getUShort("idle", s_gate.idleS);
  s_dir = p.getUChar("dir", s_dir) % DIR_COUNT;
  s_mapOn = p.getBool("map", s_mapOn);
  s_cue = p.getBool("cue", s_cue);
  s_newAlert = p.getBool("new", s_newAlert);
  p.end();
}
static void savePrefs() {
  Preferences p;
  if (!p.begin("rm-wd", false)) return;
  p.putUShort("gap", s_gate.minGapS);
  p.putUShort("move", s_gate.moveM);
  p.putUShort("idle", s_gate.idleS);
  p.putUChar("dir", s_dir);
  p.putBool("map", s_mapOn);
  p.putBool("cue", s_cue);
  p.putBool("new", s_newAlert);
  p.end();
}

// ---- buffers: in PSRAM, made the first time they are needed ------------------------------
static constexpr size_t LIVE_MAX = 2000;     // 24 KB: about 11 hours at one stop every 20 s
static constexpr size_t REVIEW_MAX = 4000;   // 48 KB
static Stop* s_live = nullptr;
static size_t s_liveN = 0;                    // total stops this run (ring slot = n % LIVE_MAX)
static Stop* s_review = nullptr;
static size_t s_reviewN = 0;
static bool s_reviewing = false;              // the map shows the review, not the live run
static char s_reviewName[40] = "";

static Stop* psAlloc(size_t n) {
  void* p = heap_caps_malloc(n * sizeof(Stop), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) p = malloc(n * sizeof(Stop));
  return (Stop*)p;
}

// ---- per-node summary for the live page --------------------------------------------------
struct NodeSeen {
  uint8_t key[4];
  char    name[24];
  int8_t  bestLink4, worstLink4, bestThey4, bestWe4;
  uint16_t seen;
};
static std::vector<NodeSeen> s_nodes;
static constexpr size_t NODES_MAX = 64;

static void noteNode(const DiscoverHit& h, const char* name) {
  const int link = h.theirSnr4 < h.ourSnr4 ? h.theirSnr4 : h.ourSnr4;
  for (auto& n : s_nodes) {
    if (memcmp(n.key, h.pub, 4)) continue;
    if (link > n.bestLink4) n.bestLink4 = (int8_t)link;
    if (link < n.worstLink4) n.worstLink4 = (int8_t)link;
    if (h.theirSnr4 > n.bestThey4) n.bestThey4 = h.theirSnr4;
    if (h.ourSnr4 > n.bestWe4) n.bestWe4 = h.ourSnr4;
    if (n.seen < 65535) n.seen++;
    return;
  }
  if (s_nodes.size() >= NODES_MAX) return;
  NodeSeen n;
  memcpy(n.key, h.pub, 4);
  strlcpy(n.name, name, sizeof(n.name));
  n.bestLink4 = n.worstLink4 = (int8_t)link;
  n.bestThey4 = h.theirSnr4;
  n.bestWe4 = h.ourSnr4;
  n.seen = 1;
  s_nodes.push_back(n);
}

// ---- repeaters seen before, in any run: /rabbit/wardrive/known.txt, one short key a line ------
static constexpr size_t KNOWN_MAX = 1024;      // 4 KB
static uint32_t s_knownMem[KNOWN_MAX];
static KnownSet s_known(s_knownMem, KNOWN_MAX);
static bool s_knownLoaded = false;
static const char* KNOWN_PATH = "/rabbit/wardrive/known.txt";

static void loadKnown() {
  if (s_knownLoaded || !sdMounted()) return;
  s_knownLoaded = true;
  File f = SD.open(KNOWN_PATH, FILE_READ);
  if (!f) return;
  char line[16];
  while (f.available()) {
    const size_t n = f.readBytesUntil('\n', line, sizeof(line) - 1);
    line[n] = 0;
    if (n >= 8) s_known.add((uint32_t)strtoul(line, nullptr, 16));
  }
  f.close();
}
static void rememberKnown(uint32_t k) {
  File f = SD.open(KNOWN_PATH, FILE_APPEND);
  if (!f) return;
  f.printf("%08lx\n", (unsigned long)k);
  f.close();
}

// ---- cues -------------------------------------------------------------------------------------
static ToneStep s_cueSteps[] = {{880, 90}};
static const Jingle s_cueJ = {"wd-cue", s_cueSteps, 1, WAVE_SINE, true};
static const ToneStep NONE_STEPS[] = {{220, 70}, {0, 40}, {196, 90}};
static const Jingle NONE_J = {"wd-none", NONE_STEPS, 3, WAVE_TRIANGLE, false};
static const ToneStep NEW_STEPS[] = {{784, 70}, {988, 70}, {1319, 70}, {0, 40}, {1568, 200}};
static const Jingle NEW_J = {"wd-new", NEW_STEPS, 5, WAVE_SQUARE, false};

// ---- the run --------------------------------------------------------------------------------
static bool s_on = false;
static char s_path[64] = "";
static uint32_t s_probeAt = 0;                // millis of the probe in flight, 0 = none
static double s_probeLat = 0, s_probeLon = 0;
static uint32_t s_lastProbe = 0;
static double s_lastLat = 0, s_lastLon = 0;
static uint32_t s_probes = 0, s_answered = 0, s_answers = 0, s_dropped = 0;
static uint8_t s_lastHits = 0;
static uint32_t s_newFound = 0;               // repeaters this run that no run had seen
static char s_newName[24] = "";
static uint32_t s_startedAt = 0;

bool running() { return s_on; }
bool wantsGps() { return s_on; }

static bool fixNow(double& la, double& lo) {
  if (!gps.hasFix()) return false;
  la = gps.fix().lat; lo = gps.fix().lon;
  return !(la == 0 && lo == 0);
}

static String utcStamp() {
  if (!app::timeValid()) return String("");
  const time_t t = (time_t)app::now();
  struct tm tm;
  gmtime_r(&t, &tm);
  char b[24];
  snprintf(b, sizeof(b), "%04d-%02d-%02dT%02d:%02d:%02dZ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  return String(b);
}

static bool openRunLog() {
  if (!sdMount()) return false;
  if (!SD.exists("/rabbit")) SD.mkdir("/rabbit");
  if (!SD.exists("/rabbit/wardrive")) SD.mkdir("/rabbit/wardrive");
  if (app::timeValid()) {
    const time_t t = (time_t)app::now() + regional::offsetMin(app::now()) * 60;
    struct tm tm;
    gmtime_r(&t, &tm);
    snprintf(s_path, sizeof(s_path), "/rabbit/wardrive/wd-%04d%02d%02d-%02d%02d%02d.csv",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  } else {
    snprintf(s_path, sizeof(s_path), "/rabbit/wardrive/wd-up%lu.csv", (unsigned long)(millis() / 1000));
  }
  File f = SD.open(s_path, FILE_WRITE);
  if (!f) { s_path[0] = 0; return false; }
  f.println(CSV_HEADER);
  f.close();
  return true;
}

static void appendBlock(const String& lines) {
  if (!s_path[0] || !sdMounted()) return;
  File f = SD.open(s_path, FILE_APPEND);
  if (!f) { sdWriteFailed("wardrive log"); return; }
  f.print(lines);
  f.close();
}

static const char* typeName(uint8_t t) {
  switch (t) {
    case ADV_TYPE_REPEATER: return "repeater";
    case ADV_TYPE_ROOM:     return "room";
    case ADV_TYPE_SENSOR:   return "sensor";
    case ADV_TYPE_CHAT:     return "chat";
    default:                return "other";
  }
}

static void start() {
  loadPrefs();
  if (!g_node) { nav.toast("radio not running"); return; }
  if (field::rangeOn()) { nav.toast("stop the range test first - they share the discover", 3000); return; }
  if (!s_live) s_live = psAlloc(LIVE_MAX);
  if (!s_live) { nav.toast("out of memory"); return; }
  if (!openRunLog()) { s_path[0] = 0; nav.toast("no sd card: surveying without a log (map only)", 3000); }
  s_on = true;
  s_reviewing = false;                         // the map follows the live run again
  s_liveN = 0;
  s_nodes.clear();
  s_probeAt = s_lastProbe = 0;
  s_probes = s_answered = s_answers = s_dropped = 0;
  s_lastHits = 0;
  s_newFound = 0;
  loadKnown();
  s_startedAt = millis();
  logs.add(LOG_INFO, "wardrive started: %s", s_path);
}

static void stop() {
  if (!s_on) return;
  s_on = false;
  s_probeAt = 0;
  logs.add(LOG_INFO, "wardrive stopped: %lu probes, %lu answered", (unsigned long)s_probes, (unsigned long)s_answered);
}

// The probe's replies are in: one stop for the map, one CSV row per answer (or one "none").
static void bank(double drift) {
  const String where = utcStamp() + "," + String(s_probeLat, 6) + "," + String(s_probeLon, 6) + "," +
                       String((int)lroundf(gps.fix().altitudeM)) + "," + String(gps.fix().satellites) + "," +
                       String((int)lround(drift));
  Stop st;
  stopBegin(st, s_probeLat, s_probeLon);
  String out;
  const uint8_t n = g_node->discoveredCount;
  int newNow = 0;
  for (uint8_t i = 0; i < n; i++) {
    const DiscoverHit& h = g_node->discovered[i];
    if ((int32_t)(h.at - s_probeAt) < 0) continue;      // an answer to somebody else's discover
    char nm[36], key[10];
    ContactInfo* c = g_node->contact(h.pub);
    if (c) sanitize(c->name, nm, sizeof(nm)); else strlcpy(nm, "(unknown)", sizeof(nm));
    for (char* p = nm; *p; p++) if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') *p = ' ';
    snprintf(key, sizeof(key), "%02x%02x%02x%02x", h.pub[0], h.pub[1], h.pub[2], h.pub[3]);
    out += where + "," + typeName(h.type) + "," + nm + "," + key + "," + String(h.theirSnr4 / 4.0, 2) + "," +
           String(h.ourSnr4 / 4.0, 2) + "," + String(h.rssi) + "\n";
    stopAdd(st, h.theirSnr4, h.ourSnr4);
    noteNode(h, nm);
    if (h.type == ADV_TYPE_REPEATER && s_known.add(shortKey(h.pub))) {
      rememberKnown(shortKey(h.pub));
      s_newFound++;
      strlcpy(s_newName, nm, sizeof s_newName);
      newNow++;
    }
  }
  if (!st.n) out = where + ",none,,,,,\n";
  appendBlock(out);
  s_live[s_liveN % LIVE_MAX] = st;
  s_liveN++;
  s_lastHits = st.n;
  s_answers += st.n;
  if (st.n) s_answered++;
  // Heard, not looked at: a fanfare for a new repeater, otherwise a tone that climbs with the link.
  if (newNow && s_newAlert) {
    jingle.play(&NEW_J);
    char msg[56];
    if (newNow == 1) snprintf(msg, sizeof msg, "new repeater: %s", s_newName);
    else snprintf(msg, sizeof msg, "%d new repeaters", newNow);
    nav.toast(msg, 3500);
  } else if (s_cue) {
    if (st.n) { s_cueSteps[0].freq = cueHz(st, s_dir); jingle.play(&s_cueJ); }
    else jingle.play(&NONE_J);
  }
}

void tick() {
  if (!s_on || !g_node) return;
  const uint32_t now = millis();
  double la = 0, lo = 0;
  const bool have = fixNow(la, lo);

  if (s_probeAt && now - s_probeAt >= HARVEST_MS) {
    const double drift = have ? metres(s_probeLat, s_probeLon, la, lo) : 0;
    if (harvestValid(now - s_probeAt, drift)) bank(drift);
    else s_dropped++;
    s_probeAt = 0;
  }
  if (s_probeAt || s_reviewing) return;        // listening, or paused while a run is reviewed

  const double moved = (have && s_lastProbe) ? metres(s_lastLat, s_lastLon, la, lo) : 0;
  if (!shouldProbe(s_gate, s_lastProbe == 0, now - s_lastProbe, have, moved)) return;
  if (field::rangeOn()) return;
  s_lastProbe = now;                           // a refused probe waits a whole gap too
  s_lastLat = la; s_lastLon = lo;
  if (!g_node->discover()) return;
  s_probeAt = now | 1;
  s_probeLat = la; s_probeLon = lo;
  s_probes++;
}

void setRunning(bool on) { if (on && !s_on) start(); else if (!on && s_on) stop(); }
uint32_t stops() { return (uint32_t)s_liveN; }
uint32_t answeredStops() { return s_answered; }
uint32_t newRepeaters() { return s_newFound; }
const char* logPath() { return s_path; }

// ---- the map overlay ---------------------------------------------------------------------
bool overlayShown() { return s_mapOn && (s_reviewing ? s_reviewN : s_liveN) > 0; }
uint8_t overlayDir() { return s_dir; }
const Stop* overlay(size_t& n) {
  if (s_reviewing) { n = s_reviewN; return s_review; }
  n = s_liveN < LIVE_MAX ? s_liveN : LIVE_MAX;
  return s_live;
}

// ---- screens ---------------------------------------------------------------------------------
static String db(int8_t v4) { return v4 == -128 ? String("--") : String(v4 / 4.0, 1); }
static String elapsed(uint32_t ms) {
  const uint32_t s = ms / 1000;
  char b[16];
  if (s < 3600) snprintf(b, sizeof(b), "%lum%02lus", (unsigned long)(s / 60), (unsigned long)(s % 60));
  else snprintf(b, sizeof(b), "%luh%02lum", (unsigned long)(s / 3600), (unsigned long)(s / 60 % 60));
  return String(b);
}

static void livePage() {
  auto* v = new TextPageView("Wardrive", [](std::vector<String>& out) {
    out.push_back(s_on ? String(">> SURVEY RUNNING  ") + elapsed(millis() - s_startedAt) : String("survey stopped"));
    if (!needs::met(needs::GPS | needs::SD_CARD)) out.push_back(String("#") + needs::line(needs::GPS | needs::SD_CARD));
    double la, lo;
    out.push_back(fixNow(la, lo) ? String("gps   ") + String(la, 5) + ", " + String(lo, 5) + "  " + gps.fix().satellites + " sats"
                                 : String("gps   no fix - not probing until there is one"));
    if (s_probeAt) out.push_back(String("probe out, listening ") + (HARVEST_MS - min<uint32_t>(HARVEST_MS, millis() - s_probeAt)) / 1000 + " s");
    else if (s_probes) out.push_back(String("last stop: ") + s_lastHits + (s_lastHits == 1 ? " answer" : " answers"));
    out.push_back(String("probes ") + s_probes + "  answered " + s_answered + "  dead " + (s_liveN - s_answered) +
                  (s_dropped ? String("  dropped ") + s_dropped : String("")));
    out.push_back(String("colour  ") + dirName(s_dir));
    if (s_newFound) out.push_back(String("new repeaters this run: ") + s_newFound + "  (last: " + s_newName + ")");
    const char* file = strrchr(s_path, '/');      // the name only: it's in /rabbit/wardrive
    out.push_back(s_path[0] ? String("log   ") + (file ? file + 1 : s_path) : String("log   none (no sd card)"));
    out.push_back("");
    if (s_nodes.empty()) out.push_back("nothing has answered a probe yet");
    else {
      out.push_back("node         best/worst  us>  >us  n");   // fits 320 px unwrapped
      std::vector<NodeSeen> sorted(s_nodes);
      std::sort(sorted.begin(), sorted.end(), [](const NodeSeen& a, const NodeSeen& b) { return a.bestLink4 > b.bestLink4; });
      for (const auto& n : sorted) {
        char row[96];
        snprintf(row, sizeof(row), "%-12.12s %5s/%-5s %4s %4s %u", n.name, db(n.bestLink4).c_str(), db(n.worstLink4).c_str(),
                 db(n.bestThey4).c_str(), db(n.bestWe4).c_str(), (unsigned)n.seen);
        out.push_back(row);
      }
      out.push_back("us> = they hear us   >us = we hear them (dB SNR)");
    }
    out.push_back("");
    out.push_back(s_on ? "press: stop the survey" : "press: start a survey");
  }, 1000);
  v->onPress = [] { if (s_on) stop(); else start(); };
  nav.push(v);
}

// Every run on the card, newest first: ours, and Squatch range tests.
static void listRuns(std::vector<String>& out) {
  if (!sdMount()) return;
  for (const char* dir : {"/rabbit/wardrive", "/inw/range"}) {
    File d = SD.open(dir);
    if (!d || !d.isDirectory()) continue;
    for (File f = d.openNextFile(); f; f = d.openNextFile()) {
      if (!f.isDirectory()) {
        String nm = f.name();
        const int slash = nm.lastIndexOf('/');
        if (slash >= 0) nm = nm.substring(slash + 1);
        if (nm.endsWith(".csv")) out.push_back(String(dir) + "/" + nm);
      }
      f.close();
    }
    d.close();
  }
  std::sort(out.begin(), out.end(), [](const String& a, const String& b) {
    return a.substring(a.lastIndexOf('/') + 1) > b.substring(b.lastIndexOf('/') + 1);
  });
  if (out.size() > 40) out.resize(40);
}

static void showReview() {
  // The middle of the run is where the map opens.
  double sla = 0, slo = 0;
  for (size_t i = 0; i < s_reviewN; i++) { sla += s_review[i].lat; slo += s_review[i].lon; }
  if (s_reviewN) { sla /= s_reviewN; slo /= s_reviewN; }
  app::openMap(sla, slo);
}

static void loadRun(const String& path) {
  if (!s_review) s_review = psAlloc(REVIEW_MAX);
  if (!s_review) { nav.toast("out of memory"); return; }
  File f = SD.open(path.c_str(), FILE_READ);
  if (!f) { nav.toast("can't open that file"); return; }
  Loader L(s_review, REVIEW_MAX);
  char line[256];
  while (f.available()) {
    const size_t n = f.readBytesUntil('\n', line, sizeof(line) - 1);
    line[n] = 0;
    L.add(line);
  }
  f.close();
  L.finish();
  if (!L.n) { nav.toast("no positioned rows in that file", 3000); return; }
  s_reviewN = L.n;
  s_reviewing = true;
  const String base = path.substring(path.lastIndexOf('/') + 1);
  strlcpy(s_reviewName, base.c_str(), sizeof(s_reviewName));
  static size_t rows, hits, dead;
  rows = L.rows; hits = L.hits; dead = L.dead;
  auto* v = new TextPageView("Review", [](std::vector<String>& out) {
    out.push_back(String("run   ") + s_reviewName);
    out.push_back(String("stops ") + s_reviewN + "   answered " + (s_reviewN - dead) + "   dead " + dead);
    out.push_back(String("rows  ") + rows + "   answers " + hits);
    if (s_reviewN >= REVIEW_MAX) out.push_back(String("showing the first ") + REVIEW_MAX + " stops");
    int8_t best = -128;
    for (size_t i = 0; i < s_reviewN; i++) {
      const int8_t v4 = s_dir == DIR_THEY_HEAR_US ? s_review[i].they4 : s_dir == DIR_WE_HEAR_THEM ? s_review[i].we4 : s_review[i].link4;
      if (v4 > best) best = v4;
    }
    out.push_back(String("best  ") + db(best) + " dB  (" + dirName(s_dir) + ")");
    out.push_back(s_on ? "survey paused while you review" : "");
    out.push_back("");
    out.push_back("press: show it on the map");
    out.push_back("wardrive > show coverage: back to the live run");
  }, 2000);
  v->onPress = [] { showReview(); };
  nav.push(v);
}

// The reviewed run, for Google Earth (KML) or any GIS tool (GeoJSON), in /rabbit/wardrive/export.
static void exportRun(bool kml) {
  if (!s_reviewing || !s_reviewN) { nav.toast("load a run first"); return; }
  if (!sdMount()) { nav.toast("no sd card"); return; }
  if (!SD.exists("/rabbit/wardrive/export")) SD.mkdir("/rabbit/wardrive/export");
  String base = s_reviewName;
  if (base.endsWith(".csv")) base = base.substring(0, base.length() - 4);
  const String path = String("/rabbit/wardrive/export/") + base + (kml ? ".kml" : ".geojson");
  File f = SD.open(path.c_str(), FILE_WRITE);
  if (!f) { nav.toast("can't write to the sd card"); return; }
  f.print(kml ? KML_HEAD : GEOJSON_HEAD);
  char b[512];
  for (size_t i = 0; i < s_reviewN; i++) {
    const int k = kml ? kmlStop(b, sizeof b, s_review[i], s_dir, i) : geojsonStop(b, sizeof b, s_review[i], i == 0);
    if (k > 0) f.write((const uint8_t*)b, min((size_t)k, sizeof b - 1));
  }
  f.print(kml ? KML_TAIL : GEOJSON_TAIL);
  f.close();
  logs.add(LOG_INFO, "wardrive exported: %s", path.c_str());
  nav.toast((String("saved ") + path.substring(path.lastIndexOf('/') + 1)).c_str(), 3500);
}

static void reviewMenu() {
  auto* m = new MenuView("Review a run");
  m->rebuild = [](MenuView& v) {
    std::vector<String> runs;
    listRuns(runs);
    if (s_reviewing) {
      v.action("back to the live run", [] { s_reviewing = false; nav.pop(); nav.toast("map shows the live run"); });
      v.action("export it: Google Earth (KML)", [] { exportRun(true); });
      v.action("export it: GeoJSON", [] { exportRun(false); });
    }
    if (runs.empty()) v.info("sd", []() -> String { return sdMounted() ? String("no runs yet") : String("no card"); });
    for (const String& r : runs) {
      const String label = r.startsWith("/inw/") ? String("range ") + r.substring(r.lastIndexOf('/') + 1)
                                                 : r.substring(r.lastIndexOf('/') + 1);
      v.action(label, [r] { loadRun(r); });
    }
  };
  m->rebuild(*m);
  nav.push(m);
}

void openMenu() {
  loadPrefs();
  auto* m = new MenuView("Wardrive");
  m->rebuild = [](MenuView& v) {
    v.info("needs", []() -> String { return needs::line(needs::GPS | needs::SD_CARD); });
    v.info("status", []() -> String {
      if (!s_on) return String("stopped");
      return String("running  ") + s_liveN + (s_liveN == 1 ? " stop" : " stops") + (s_reviewing ? "  (paused)" : "");
    });
    v.action("start / stop survey", [] {
      if (s_on) { stop(); nav.toast("survey stopped"); }
      else { start(); if (s_on && s_path[0]) nav.toast("survey on - the screen can go off", 3000); }
    });
    v.action("live view + nodes", [] { livePage(); });
    v.action("show coverage on map", [] {
      s_reviewing = false;
      double la, lo;
      if (fixNow(la, lo)) app::openMap(la, lo); else app::openMap();
    });
    v.action("review a run", [] { reviewMenu(); });
    v.header("survey");
    v.adjust("probe at most every", []() -> String { return String(s_gate.minGapS) + " s"; }, [](int d) {
      static const uint16_t S[] = {15, 20, 30, 60, 120};
      int i = 0;
      for (int k = 0; k < 5; k++) if (S[k] <= s_gate.minGapS) i = k;
      s_gate.minGapS = S[constrain(i + d, 0, 4)];
      savePrefs();
    });
    v.adjust("sooner once moved", []() -> String { return String(s_gate.moveM) + " m"; }, [](int d) {
      static const uint16_t S[] = {25, 50, 100, 250, 500};
      int i = 0;
      for (int k = 0; k < 5; k++) if (S[k] <= s_gate.moveM) i = k;
      s_gate.moveM = S[constrain(i + d, 0, 4)];
      savePrefs();
    });
    v.adjust("parked: every", []() -> String { return String(s_gate.idleS) + " s"; }, [](int d) {
      static const uint16_t S[] = {60, 120, 300, 600};
      int i = 0;
      for (int k = 0; k < 4; k++) if (S[k] <= s_gate.idleS) i = k;
      s_gate.idleS = S[constrain(i + d, 0, 3)];
      savePrefs();
    });
    v.header("while driving");
    v.toggle("tone for each probe", [] { return s_cue; }, [] { s_cue = !s_cue; savePrefs(); });
    v.toggle("alert on a new repeater", [] { return s_newAlert; }, [] { s_newAlert = !s_newAlert; savePrefs(); });
    v.info("repeaters known", []() -> String { loadKnown(); return String(s_known.n) + " from all runs"; });
    v.header("map");
    v.adjust("colour by", []() -> String { return String(dirName(s_dir)); }, [](int d) {
      s_dir = (uint8_t)((s_dir + DIR_COUNT + (d > 0 ? 1 : -1)) % DIR_COUNT);
      savePrefs();
    });
    v.toggle("coverage on the map", [] { return s_mapOn; }, [] { s_mapOn = !s_mapOn; savePrefs(); });
    v.info("legend", []() -> String { return String("cyan strong / yellow ok / red weak / ring: none"); });
    v.info("gps", []() -> String { return s_on ? String("kept on while surveying") : String("as set in settings"); });
  };
  m->rebuild(*m);
  nav.push(m);
}

}  // namespace wardrive
