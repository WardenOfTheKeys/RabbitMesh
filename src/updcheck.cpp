// RabbitMesh update notice (see updcheck.h).
#include "updcheck.h"
#include <HTTPClient.h>
#include <Preferences.h>
#include "app.h"
#include "logstore.h"
#include "netwifi.h"
#include "tls_client.h"
#include "ui.h"

#ifndef RABBITMESH_VERSION
#define RABBITMESH_VERSION "0.0.0"
#endif

extern LogStore logs;

namespace updcheck {

static const char* URL = "https://api.github.com/repos/WardenOfTheKeys/rabbitmesh/releases/latest";
static const char* WHERE = "github.com/WardenOfTheKeys/rabbitmesh";
static const char* USER_AGENT = "RabbitMesh/" RABBITMESH_VERSION " (LilyGo T-Deck)";
constexpr uint32_t EVERY_S = 7UL * 24 * 3600;      // once a week
constexpr uint32_t RETRY_S = 6UL * 3600;           // after a failed try

static bool s_loaded = false, s_on = true, s_manual = false;
static uint32_t s_lastTry = 0, s_lastOk = 0;       // unix time
static char s_latest[24] = "";                     // the newest release's tag, "v0.1.1"
static char s_told[24] = "";                       // the one already announced
enum State : uint8_t { IDLE, RUNNING, DONE, FAILED };
static volatile uint8_t s_state = IDLE;
static char s_found[24] = "", s_err[48] = "";

static void load() {
  if (s_loaded) return;
  s_loaded = true;
  Preferences p;
  if (!p.begin("rm-upd", true)) return;
  s_on = p.getBool("on", true);
  s_lastTry = p.getUInt("try", 0);
  s_lastOk = p.getUInt("ok", 0);
  p.getString("latest", s_latest, sizeof s_latest);
  p.getString("told", s_told, sizeof s_told);
  p.end();
}
static void save() {
  Preferences p;
  if (!p.begin("rm-upd", false)) return;
  p.putBool("on", s_on);
  p.putUInt("try", s_lastTry);
  p.putUInt("ok", s_lastOk);
  p.putString("latest", s_latest);
  p.putString("told", s_told);
  p.end();
}

// "v0.1.10" > "0.1.9": numbers compared part by part.
static bool newer(const char* tag, const char* mine) {
  int a[3] = {0, 0, 0}, b[3] = {0, 0, 0};
  sscanf(tag[0] == 'v' || tag[0] == 'V' ? tag + 1 : tag, "%d.%d.%d", &a[0], &a[1], &a[2]);
  sscanf(mine[0] == 'v' || mine[0] == 'V' ? mine + 1 : mine, "%d.%d.%d", &b[0], &b[1], &b[2]);
  for (int i = 0; i < 3; i++) if (a[i] != b[i]) return a[i] > b[i];
  return false;
}

// Its own task (TLS wants a deep stack): only the network here, no log, no screen.
static void task(void*) {
  {
    TlsClient tls;
    tls.setInsecure();                           // a public version number, checked for shape below
    HTTPClient http;
    http.setTimeout(15000);
    http.setUserAgent(USER_AGENT);
    if (!http.begin(tls, URL)) { strlcpy(s_err, "couldn't reach github", sizeof s_err); s_state = FAILED; }
    else {
      http.addHeader("Accept", "application/vnd.github+json");
      const int code = http.GET();
      if (code == 404) { strlcpy(s_err, "no release published yet", sizeof s_err); s_state = FAILED; }
      else if (code != 200) { snprintf(s_err, sizeof s_err, "github answered %d", code); s_state = FAILED; }
      else {
        const String body = http.getString();
        const int k = body.indexOf("\"tag_name\"");
        const int q1 = k < 0 ? -1 : body.indexOf('"', body.indexOf(':', k) + 1);
        const int q2 = q1 < 0 ? -1 : body.indexOf('"', q1 + 1);
        if (q1 < 0 || q2 <= q1 || q2 - q1 > (int)sizeof s_found) { strlcpy(s_err, "an answer it didn't understand", sizeof s_err); s_state = FAILED; }
        else { strlcpy(s_found, body.substring(q1 + 1, q2).c_str(), sizeof s_found); s_state = DONE; }
      }
      http.end();
    }
  }
  vTaskDelete(nullptr);
}

static void start(bool manual) {
  if (s_state == RUNNING) return;
  s_manual = manual;
  s_err[0] = s_found[0] = 0;
  s_lastTry = app::timeValid() ? (uint32_t)app::now() : 0;
  s_state = RUNNING;
  if (xTaskCreatePinnedToCore(task, "updchk", 12288, nullptr, 1, nullptr, 1) != pdPASS) {
    strlcpy(s_err, "couldn't start the check", sizeof s_err);
    s_state = FAILED;
  }
}

bool enabled() { load(); return s_on; }
void setEnabled(bool on) { load(); s_on = on; save(); }

void checkNow() {
  load();
  if (!wifi::connected()) { nav.toast("needs wifi connected to a network (settings > wifi)", 3000); return; }
  if (s_state == RUNNING) return;
  nav.toast("checking...", 1500);
  start(true);
}

String status() {
  load();
  if (s_state == RUNNING) return String("checking...");
  if (!s_latest[0]) return String("not checked yet");
  if (newer(s_latest, RABBITMESH_VERSION)) return String(s_latest) + " is out";
  return String("up to date (v" RABBITMESH_VERSION ")");
}

void tick() {
  load();
  if (s_state == DONE || s_state == FAILED) {
    const bool ok = s_state == DONE;
    s_state = IDLE;
    if (!ok) {
      logs.add(LOG_INFO, "update check: %s", s_err);
      if (s_manual) nav.toast(s_err, 3000);
      save();
      return;
    }
    strlcpy(s_latest, s_found, sizeof s_latest);
    s_lastOk = s_lastTry;
    const bool isNew = newer(s_latest, RABBITMESH_VERSION);
    logs.add(LOG_INFO, "update check: newest is %s, this is v%s", s_latest, RABBITMESH_VERSION);
    if (isNew && (s_manual || strcmp(s_told, s_latest))) {
      strlcpy(s_told, s_latest, sizeof s_told);
      char title[48];
      snprintf(title, sizeof title, "RabbitMesh %s is out", s_latest);
      nav.banner(title, (String("flash its update file from ") + WHERE).c_str(), 15000, nullptr);
    } else if (s_manual) {
      nav.toast((String("up to date (v") + RABBITMESH_VERSION + ")").c_str(), 3000);
    }
    save();
    return;
  }
  // The weekly look: on WiFi, with the clock set (to count the week by), not while one runs.
  static uint32_t lookAt = 0;
  if (lookAt && millis() - lookAt < 60000) return;
  lookAt = millis() | 1;
  if (!s_on || s_state != IDLE || !wifi::connected() || !app::timeValid()) return;
  const uint32_t now = (uint32_t)app::now();
  const uint32_t wait = s_lastOk >= s_lastTry ? EVERY_S : RETRY_S;
  if (s_lastTry && now - s_lastTry < wait) return;
  start(false);
}

}  // namespace updcheck
