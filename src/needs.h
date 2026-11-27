// RabbitMesh: what a tool needs, and whether it has it right now, as one line for its menu or
// live page: "needs: gps ok, sd card ok, wifi off NO". So nobody wonders why a tool is waiting.
#pragma once
#include <Arduino.h>
#include "gps.h"
#include "app.h"      // the gps object
#include "dataio.h"
#include "netwifi.h"

namespace needs {

enum : uint8_t { GPS = 1, SD_CARD = 2, WIFI_OFF = 4, WIFI_ON = 8 };

// The card, checked at most every 5 s: a missing card isn't re-mounted every frame.
inline bool sdOk() {
  static uint32_t at = 0;
  static bool ok = false;
  if (!at || millis() - at > 5000) { at = millis() | 1; ok = sdMount(); }
  return ok;
}
inline bool gpsOk() { return gps.hasFix() && !(gps.fix().lat == 0 && gps.fix().lon == 0); }
// While there's no fix: what the GPS hears, so "no fix" doesn't look like "no GPS". Satellites
// counted on the GPS page are the ones it hears; a fix needs a few of them strong enough.
inline String gpsWait() {
  const uint8_t n = gps.heard(), sig = gps.bestSignal();
  if (!n) return String("hearing no satellites: go outside");
  return String("hearing ") + n + " sats, best " + sig + (sig >= 30 ? " - nearly there" : " - weak: open sky helps");
}

inline String line(uint8_t what) {
  String s = "needs:";
  bool first = true;
  auto add = [&](const char* name, bool ok) {
    s += first ? " " : ", ";
    first = false;
    s += name;
    s += ok ? " ok" : " NO";
  };
  if (what & GPS) { add("gps fix", gpsOk()); if (!gpsOk()) s += String(" (") + gpsWait() + ")"; }
  if (what & SD_CARD) add("sd card", sdOk());
  if (what & WIFI_OFF) add("wifi off", !wifi::enabled());
  if (what & WIFI_ON) add("wifi", wifi::connected());
  return s;
}
// True when everything listed is there.
inline bool met(uint8_t what) {
  return (!(what & GPS) || gpsOk()) && (!(what & SD_CARD) || sdOk()) && (!(what & WIFI_OFF) || !wifi::enabled()) &&
         (!(what & WIFI_ON) || wifi::connected());
}

}  // namespace needs
