// RabbitMesh holiday themes by date (see holiday.h).
#include "holiday.h"
#include <Preferences.h>
#include "app.h"
#include "regional.h"
#include "themes.h"
#include "holiday_core.h"

namespace holiday {

using namespace holidaycore;

static bool s_loaded = false, s_auto = true;
static uint32_t s_dismissed = 0;        // the season (seasonKey) the owner picked their own theme in
static int s_shown = -1;
static uint8_t s_today = H_NONE;
static uint32_t s_key = 0, s_checkAt = 0;

static void load() {
  if (s_loaded) return;
  s_loaded = true;
  Preferences p;
  if (!p.begin("rm-hol", true)) return;
  s_auto = p.getBool("auto", true);
  s_dismissed = p.getUInt("dis", 0);
  p.end();
}
static void save() {
  Preferences p;
  if (!p.begin("rm-hol", false)) return;
  p.putBool("auto", s_auto);
  p.putUInt("dis", s_dismissed);
  p.end();
}

static void decide(bool force) {
  int want = -1;
  if (s_auto && s_today != H_NONE && s_key != s_dismissed) want = HOLIDAY_THEME[s_today];
  if (want != s_shown || force) {
    s_shown = want;
    app::applyTheme();
  }
}

void tick() {
  load();
  if (s_checkAt && millis() - s_checkAt < 30000) return;
  s_checkAt = millis() | 1;
  uint8_t today = H_NONE;
  uint32_t key = 0;
  if (app::timeValid()) {
    const time_t t = (time_t)app::now() + regional::offsetMin(app::now()) * 60;   // the local date
    struct tm tm;
    gmtime_r(&t, &tm);
    today = holidayOn(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    key = seasonKey(tm.tm_year + 1900, tm.tm_mon + 1, today);
  }
  s_today = today;
  s_key = key;
  decide(false);
}

int shownId() { return s_shown; }
bool autoOn() { load(); return s_auto; }
void setAuto(bool on) { load(); s_auto = on; save(); decide(false); }
const char* current() { return holidayName(s_today); }

void picked(uint8_t themeId) {
  load();
  if (s_shown >= 0 && themeId != s_shown) {   // their choice wins until this holiday is over
    s_dismissed = s_key;
    save();
    s_shown = -1;
  }
}

}  // namespace holiday
