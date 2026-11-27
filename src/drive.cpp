// RabbitMesh Drive mode (see drive.h).
#include "drive.h"
#include "deck.h"
#include "deck_core.h"
#include "wardrive.h"
#include "wifidrive.h"
#include "flock.h"
#include "fieldtools.h"
#include "logstore.h"
#include "settings.h"
#include "needs.h"
#include "ui.h"

extern LogStore logs;

namespace drive {

static bool s_on = false;
static uint32_t s_startedAt = 0, s_alerts0 = 0;
static bool s_startedTrip = false, s_startedWd = false, s_startedWifi = false, s_raisedAlerts = false;
static char s_skipped[80] = "";              // what couldn't start, and why

// The figures at the end, kept for the summary page.
struct Summary {
  uint32_t secs = 0, movingS = 0, stops = 0, answered = 0, newRep = 0, wifi = 0, open = 0, bt = 0, rows = 0, cams = 0;
  double distM = 0, maxMps = 0;
  char trip[64] = "", wd[64] = "", wigle[64] = "";
};
static Summary s_last;

bool on() { return s_on; }

static String fileOf(const char* path) {
  const char* f = strrchr(path, '/');
  return String(f ? f + 1 : path);
}
static String dist(double m) {
  char b[24];
  deckcore::distText(b, sizeof b, m, ui_settings.miles);
  return String(b);
}
static String hms(uint32_t s) {
  char b[16];
  if (s >= 3600) snprintf(b, sizeof b, "%luh%02lum", (unsigned long)(s / 3600), (unsigned long)(s / 60 % 60));
  else snprintf(b, sizeof b, "%lum%02lus", (unsigned long)(s / 60), (unsigned long)(s % 60));
  return String(b);
}

static void start() {
  s_skipped[0] = 0;
  s_startedTrip = !deck::tripOn();
  if (s_startedTrip) deck::tripSet(true);
  s_startedWd = false;
  if (field::rangeOn()) strlcat(s_skipped, "mesh survey (range test on) ", sizeof s_skipped);
  else if (!wardrive::running()) { wardrive::setRunning(true); s_startedWd = wardrive::running(); }
  s_startedWifi = false;
  if (flockhunt::listening()) strlcat(s_skipped, "wifi/bt (flock listening) ", sizeof s_skipped);
  else if (!wifidrive::running()) { wifidrive::setRunning(true); s_startedWifi = wifidrive::running(); }
  s_raisedAlerts = !flockhunt::alertsOn();
  if (s_raisedAlerts) flockhunt::setAlerts(true);
  s_alerts0 = flockhunt::alertsGiven();
  s_startedAt = millis();
  s_on = true;
  logs.add(LOG_INFO, "drive mode on");
}

static void stop() {
  if (!s_on) return;
  Summary& m = s_last;
  m = Summary();
  m.secs = (millis() - s_startedAt) / 1000;
  m.distM = deck::tripDistM(); m.movingS = deck::tripMovingS(); m.maxMps = deck::tripMaxMps();
  m.stops = wardrive::stops(); m.answered = wardrive::answeredStops(); m.newRep = wardrive::newRepeaters();
  m.wifi = wifidrive::wifiCount(); m.open = wifidrive::openCount(); m.bt = wifidrive::bleCount(); m.rows = wifidrive::rowsLogged();
  m.cams = flockhunt::alertsGiven() - s_alerts0;
  strlcpy(m.trip, deck::tripGpx(), sizeof m.trip);
  strlcpy(m.wd, wardrive::logPath(), sizeof m.wd);
  strlcpy(m.wigle, wifidrive::logPath(), sizeof m.wigle);
  if (s_startedTrip) deck::tripSet(false);
  if (s_startedWd) wardrive::setRunning(false);
  if (s_startedWifi) wifidrive::setRunning(false);
  if (s_raisedAlerts) flockhunt::setAlerts(false);
  s_on = false;
  logs.add(LOG_INFO, "drive mode off: %.0f m, %lu stops, %lu wifi, %lu bt", m.distM, (unsigned long)m.stops,
           (unsigned long)m.wifi, (unsigned long)m.bt);
}

static void summaryLines(std::vector<String>& out, const Summary& m) {
  const bool mi = ui_settings.miles;
  out.push_back(String("time   ") + hms(m.secs) + "   moving " + hms(m.movingS));
  out.push_back(String("drove  ") + dist(m.distM) + "   top " + String(deckcore::speedUnits(m.maxMps, mi), 0) + " " + deckcore::speedName(mi));
  out.push_back(String("mesh   ") + m.stops + " stops, " + m.answered + " answered" + (m.newRep ? String(", ") + m.newRep + " NEW repeaters" : String("")));
  out.push_back(String("wifi   ") + m.wifi + " networks (" + m.open + " open)   bt " + m.bt);
  out.push_back(String("alpr   ") + m.cams + (m.cams == 1 ? " camera passed" : " cameras passed"));
  out.push_back("");
  out.push_back("#saved on the sd card:");
  if (m.trip[0]) out.push_back(String("#  track  ") + fileOf(m.trip));
  if (m.wd[0]) out.push_back(String("#  mesh   ") + fileOf(m.wd));
  if (m.wigle[0]) out.push_back(String("#  wigle  ") + fileOf(m.wigle) + " (upload at wigle.net)");
  if (!m.trip[0] && !m.wd[0] && !m.wigle[0]) out.push_back("#  nothing (no sd card)");
}

static void showSummary() {
  nav.push(new TextPageView("Drive summary", [](std::vector<String>& out) { summaryLines(out, s_last); }, 60000));
}

String tileText() {
  if (!s_on) return s_last.secs ? String("last ") + dist(s_last.distM) : String("one-press survey");
  return dist(deck::tripDistM()) + "  " + hms((millis() - s_startedAt) / 1000);
}

void open() {
  auto* v = new TextPageView("Drive mode", [](std::vector<String>& out) {
    const uint8_t want = needs::GPS | needs::SD_CARD;
    if (!s_on) {
      out.push_back("one press starts, together:");
      out.push_back("  trip + gpx track, mesh coverage survey,");
      out.push_back("  wifi + bt wardrive (wigle), camera warnings");
      out.push_back(needs::met(want) ? needs::line(want) : String("#") + needs::line(want) + " (it starts anyway)");
      if (s_last.secs) { out.push_back(""); out.push_back("#last drive:"); summaryLines(out, s_last); }
      out.push_back("");
      out.push_back("press: start driving");
      return;
    }
    const bool mi = ui_settings.miles;
    out.push_back(String(">> DRIVING  ") + hms((millis() - s_startedAt) / 1000));
    if (!needs::met(want)) out.push_back(String("#") + needs::line(want));
    out.push_back(String("trip   ") + dist(deck::tripDistM()) + "   moving " + hms(deck::tripMovingS()) + "   top " +
                  String(deckcore::speedUnits(deck::tripMaxMps(), mi), 0) + " " + deckcore::speedName(mi));
    out.push_back(String("mesh   ") + (wardrive::running() ? String(wardrive::stops()) + " stops, " + wardrive::answeredStops() + " answered" +
                                     (wardrive::newRepeaters() ? String(", ") + wardrive::newRepeaters() + " NEW" : String(""))
                                   : String("off")));
    out.push_back(String("wifi   ") + (wifidrive::running() ? String(wifidrive::wifiCount()) + " networks, " + wifidrive::openCount() +
                                      " open   bt " + wifidrive::bleCount() : String("off")));
    out.push_back(String("alpr   ") + (flockhunt::alertsGiven() - s_alerts0) + " cameras passed" +
                  (flockhunt::mappedCount() ? String("") : String("  (no camera map: flock > update)")));
    if (s_skipped[0]) out.push_back(String("#skipped: ") + s_skipped);
    out.push_back("");
    out.push_back("the screen can go off - it keeps going");
    out.push_back("press: stop and see the summary");
  }, 1000);
  v->onPress = [] {
    if (s_on) { stop(); showSummary(); }         // not nav.pop(): this page is running this very call
    else { start(); nav.toast("drive mode on", 2000); }
  };
  nav.push(v);
}

}  // namespace drive
