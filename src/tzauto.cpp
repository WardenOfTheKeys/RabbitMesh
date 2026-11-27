// RabbitMesh: time zone from location (see tzauto.h).
#include "tzauto.h"
#include <Preferences.h>
#include "app.h"
#include "gps.h"
#include "logstore.h"
#include "regional.h"
#include "settings.h"
#include "ui.h"
#include "tz_core.h"
#include "node.h"

extern LogStore logs;

namespace tzauto {

static bool s_loaded = false, s_auto = true;
static uint32_t s_at = 0;

static void load() {
  if (s_loaded) return;
  s_loaded = true;
  Preferences p;
  if (!p.begin("rm-tz", true)) return;
  s_auto = p.getBool("auto", true);
  p.end();
}
static void save() {
  Preferences p;
  if (!p.begin("rm-tz", false)) return;
  p.putBool("auto", s_auto);
  p.end();
}

bool autoOn() { load(); return s_auto; }
void setAuto(bool on) { load(); s_auto = on; save(); s_at = 0; }
void manualPick() { if (autoOn()) setAuto(false); }

// regional::ZONES index + 1 (the setting's numbering) for a tz_core zone, 0 if not in the list.
static uint8_t settingFor(uint8_t z) {
  const char* pre = tzcore::zonePrefix(z);
  if (!pre) return 0;
  for (uint8_t i = 0; i < regional::ZONE_COUNT; i++)
    if (!strncmp(regional::ZONES[i].name, pre, strlen(pre))) return i + 1;
  return 0;
}

void tick() {
  load();
  if (!s_auto || (s_at && millis() - s_at < 60000)) return;
  s_at = millis() | 1;
  // The GPS's fix, or with none (indoors, or before the first one) the node's saved position:
  // the last fix (main.cpp keeps it) or one set by hand or from the phone app.
  double la = 0, lo = 0;
  const char* from = "gps";
  if (gps.hasFix()) { la = gps.fix().lat; lo = gps.fix().lon; }
  else if (g_node) { la = g_node->prefs().node_lat; lo = g_node->prefs().node_lon; from = "saved position"; }
  if (la == 0 && lo == 0) return;
  const uint8_t want = settingFor(tzcore::zoneFor((float)la, (float)lo));
  if (!want || want == ui_settings.tzZone) return;
  ui_settings.tzZone = want;
  ui_settings.save();
  nav.statusChanged();
  logs.add(LOG_INFO, "time zone from %s: %s", from, regional::ZONES[want - 1].name);
  char msg[64];
  snprintf(msg, sizeof msg, "time zone: %s", regional::ZONES[want - 1].name);
  nav.toast(msg, 3500);
}

}  // namespace tzauto
