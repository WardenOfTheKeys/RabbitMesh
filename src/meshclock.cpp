// RabbitMesh: the clock from the mesh (see meshclock.h).
#include "meshclock.h"
#include "app.h"
#include "gps.h"
#include "logstore.h"
#include "netwifi.h"
#include "settings.h"
#include "ui.h"
#include "meshclock_core.h"

extern LogStore logs;

namespace meshclock {

static meshclockcore::Clock s_clock;
static uint32_t s_checkAt = 0;

void heard(const uint8_t* pub, uint32_t ts) {
  if (!pub) return;
  const uint32_t key = (uint32_t)pub[0] | ((uint32_t)pub[1] << 8) | ((uint32_t)pub[2] << 16) | ((uint32_t)pub[3] << 24);
  s_clock.heard(key, ts, millis());
}

void tick() {
  const uint32_t ms = millis();
  if (s_checkAt && ms - s_checkAt < 30000) return;
  s_checkAt = ms | 1;
  if (gps.hasFix() && ui_settings.gpsSetsClock) return;      // the GPS sets it
  if (wifi::connected() && ui_settings.ntpSync) return;      // the internet does
  int agree = 0;
  const uint32_t mesh = s_clock.estimate(ms, &agree);
  const uint32_t now = app::now();
  if (!meshclockcore::shouldSet(now, mesh)) return;
  app::setTime(mesh);
  const uint32_t behind = mesh - now;
  logs.add(LOG_INFO, "clock set from the mesh: %d nodes agree, it was %lu s behind", agree, (unsigned long)behind);
  char msg[48];
  snprintf(msg, sizeof msg, "clock set from the mesh (%d nodes)", agree);
  nav.toast(msg, 3000);
}

}  // namespace meshclock
