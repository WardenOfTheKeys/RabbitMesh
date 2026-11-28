// RabbitMesh updates: the weekly notice, and installing over WiFi (see updcheck.h).
#include "updcheck.h"
#include <HTTPClient.h>
#include <Preferences.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_image_format.h>
#include <mbedtls/sha256.h>
#include <ed_25519.h>
#include "app.h"
#include "backlight.h"     // the dimmer: the screen stays on while it installs
#include "updcore.h"
#include "wifidrive.h"
#include "logstore.h"
#include "netwifi.h"
#include "tls_client.h"
#include "ui.h"

#ifndef RABBITMESH_VERSION
#define RABBITMESH_VERSION "0.0.0"
#endif

extern LogStore logs;
void inwProgress(const char* what, uint32_t done, uint32_t total);   // main.cpp: the full-screen progress bar

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
// The newest release's update file and signature (from the check; kept, so "install" works
// after a restart without checking again).
static char s_updUrl[200] = "", s_sigUrl[200] = "";
static char s_foundUpd[200] = "", s_foundSig[200] = "";

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
  p.getString("upd", s_updUrl, sizeof s_updUrl);
  p.getString("sig", s_sigUrl, sizeof s_sigUrl);
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
  p.putString("upd", s_updUrl);
  p.putString("sig", s_sigUrl);
  p.end();
}

using updcore::newer;

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
        if (updcore::field(body.c_str(), "tag_name", s_found, sizeof s_found) < 0) { strlcpy(s_err, "an answer it didn't understand", sizeof s_err); s_state = FAILED; }
        else {
          // its update file and signature, if it has them (a release before WiFi updates has no signature)
          if (!updcore::asset(body.c_str(), updcore::UPDATE_SUFFIX, s_foundUpd, sizeof s_foundUpd)) s_foundUpd[0] = 0;
          if (!updcore::asset(body.c_str(), updcore::SIG_SUFFIX, s_foundSig, sizeof s_foundSig)) s_foundSig[0] = 0;
          s_state = DONE;
        }
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

// ---- installing over WiFi ----------------------------------------------------------------------
bool installable() {
  load();
  if (!s_latest[0] || !newer(s_latest, RABBITMESH_VERSION)) return false;
  if (!updcore::trustedUrl(s_updUrl) || !updcore::trustedUrl(s_sigUrl)) return false;
  const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
  return next && next != esp_ota_get_running_partition();
}
String latest() { load(); return String(s_latest); }

// A small file (the signature) into buf: exactly n bytes, or false.
static bool fetchSmall(const char* url, uint8_t* buf, int n) {
  TlsClient tls;
  tls.setInsecure();                 // the signature is what's trusted, not the connection
  HTTPClient http;
  http.setTimeout(15000);
  http.setUserAgent(USER_AGENT);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);   // github sends release files on to its file store
  if (!http.begin(tls, url)) return false;
  bool ok = false;
  if (http.GET() == 200 && http.getSize() == n) {
    WiFiClient* st = http.getStreamPtr();
    int got = 0;
    const uint32_t t0 = millis();
    while (got < n && millis() - t0 < 15000) {
      const int r = st->readBytes(buf + got, n - got);
      if (r > 0) got += r; else delay(5);
    }
    ok = got == n;
  }
  http.end();
  return ok;
}

// Keys the "trial" state in NVS: after a WiFi update, which version it came from and what was in
// the first app slot then (its SHA-256), so the next start can tell how it went.
static void noteInstalled(const char* from) {
  Preferences p;
  if (!p.begin("rm-upd", false)) return;
  p.putString("from", from);
  p.putBool("moveback", true);
  uint8_t sha[32];
  const esp_partition_t* a0 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
  if (a0 && esp_partition_get_sha256(a0, sha) == ESP_OK) p.putBytes("slot0", sha, 32);
  p.end();
}

const char* install() {
  static char msg[64];
  load();
  if (!installable()) return "no update to install over wifi";
  if (!wifi::connected()) return "needs wifi connected to a network";
  if (app::batteryPct() < 30 && !app::pluggedIn()) return "charge it to 30% first, or plug it in";
  if (wifidrive::running()) return "stop the wifi + bt wardrive first";

  // 1. the signature: 64 bytes, from the same release
  uint8_t sig[updcore::SIG_LEN];
  nav.busy("getting the signature...");
  if (!fetchSmall(s_sigUrl, sig, updcore::SIG_LEN)) return "couldn't download the signature";

  // 2. the firmware, written to the spare slot as it comes, hashed as it goes
  TlsClient tls;
  tls.setInsecure();                 // as above: the signature decides, not the connection
  HTTPClient http;
  http.setTimeout(15000);
  http.setUserAgent(USER_AGENT);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(tls, s_updUrl)) return "couldn't reach github";
  const int code = http.GET();
  if (code != 200) { http.end(); snprintf(msg, sizeof msg, "download failed (%d)", code); return msg; }
  const int len = http.getSize();
  const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
  if (len < 100000 || !next || len > (int)next->size) { http.end(); return "the download's size is wrong"; }
  if (!Update.begin(len, U_FLASH)) { http.end(); return "couldn't start writing the update"; }

  char title[40];
  snprintf(title, sizeof title, "updating to %s", s_latest);
  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);
  WiFiClient* st = http.getStreamPtr();
  uint8_t* buf = (uint8_t*)malloc(4096);
  if (!buf) { http.end(); Update.abort(); return "not enough memory to download"; }
  int got = 0;
  uint32_t lastData = millis();
  bool ok = true;
  while (got < len) {
    const int avail = st->available();
    if (!avail) {
      if (!http.connected() || millis() - lastData > 15000) { ok = false; break; }
      delay(2);
      continue;
    }
    const int n = st->readBytes(buf, min(avail, 4096));
    if (n <= 0) continue;
    lastData = millis();
    mbedtls_sha256_update(&sha, buf, n);
    if (Update.write(buf, n) != (size_t)n) { ok = false; break; }
    got += n;
    dimmer.wake();
    inwProgress(title, got, len);
  }
  http.end();
  free(buf);
  uint8_t digest[32];
  mbedtls_sha256_finish(&sha, digest);
  mbedtls_sha256_free(&sha);
  inwProgress(nullptr, 0, 0);
  if (!ok || got != len) { Update.abort(); return "download interrupted: nothing changed"; }

  // 3. only RabbitMesh's own signed firmware is switched to
  if (ed25519_verify(sig, digest, 32, updcore::RELEASE_KEY) != 1) {
    Update.abort();
    logs.add(LOG_WARN, "update %s: signature doesn't match, discarded", s_latest);
    return "not signed by rabbitmesh: nothing changed";
  }
  if (!Update.end(true)) { snprintf(msg, sizeof msg, "couldn't finish: %s", Update.errorString()); return msg; }
  noteInstalled("v" RABBITMESH_VERSION);
  logs.add(LOG_INFO, "update: installed %s over wifi, restarting", s_latest);
  nav.busy("installed - restarting");
  delay(1200);
  app::reboot();
  return "restarting";
}

// ---- after a WiFi update ---------------------------------------------------------------------------
// A new version starts on trial: the bootloader goes back to the old one if it restarts before
// the new one says it's fine. It says so once it has run TRIAL_MS, so a version that crashes at
// start, or soon after, undoes itself. (The Arduino core would otherwise say so at once.)
extern "C" bool verifyRollbackLater() { return true; }
constexpr uint32_t TRIAL_MS = 15000;

// Then it copies itself into the first app slot and starts from there next time, so the USB
// update file (flashed at 0x10000, the first slot) always replaces what runs, however it was
// last updated. Unless that slot no longer holds what was there before the update: someone has
// flashed something there by USB since, and that's what should run.
static void moveBackTask(void* arg) {
  const esp_partition_t* run = esp_ota_get_running_partition();
  const esp_partition_t* a0 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
  const char* result = "move failed";
  esp_partition_pos_t pos = {run->address, run->size};
  esp_image_metadata_t md;
  uint8_t* buf = (uint8_t*)malloc(4096);
  if (buf && a0 && esp_image_get_metadata(&pos, &md) == ESP_OK && md.image_len <= a0->size) {
    const size_t len = (md.image_len + 4095) & ~(size_t)4095;
    bool ok = esp_partition_erase_range(a0, 0, len) == ESP_OK;
    for (size_t off = 0; ok && off < len; off += 4096) {
      ok = esp_partition_read(run, off, buf, 4096) == ESP_OK && esp_partition_write(a0, off, buf, 4096) == ESP_OK;
      vTaskDelay(1);
    }
    if (ok && esp_ota_set_boot_partition(a0) == ESP_OK) result = "moved to the first slot";
  }
  free(buf);
  Preferences p;
  if (!strcmp(result, "moved to the first slot") && p.begin("rm-upd", false)) { p.putBool("moveback", false); p.end(); }
  *(const char**)arg = result;
  vTaskDelete(nullptr);
}

static void afterUpdate() {
  static uint8_t step = 0;                 // 0: trial; 1: confirmed, deciding; 2: done
  static const char* moved = nullptr;
  if (step == 2) return;
  if (step == 0) {
    if (millis() < TRIAL_MS) return;
    step = 1;
    const esp_partition_t* run = esp_ota_get_running_partition();
    esp_ota_img_states_t st;
    if (esp_ota_get_state_partition(run, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY) {
      esp_ota_mark_app_valid_cancel_rollback();
      logs.add(LOG_INFO, "update: v%s runs fine, kept", RABBITMESH_VERSION);
      // said once: the start after the move to the first slot is on trial too, quietly
      Preferences p;
      char from[24] = "";
      if (p.begin("rm-upd", false)) { p.getString("from", from, sizeof from); p.remove("from"); p.end(); }
      if (from[0]) {
        char t[48];
        snprintf(t, sizeof t, "updated to RabbitMesh v%s", RABBITMESH_VERSION);
        nav.banner(t, (String("from ") + from + ", over wifi").c_str(), 6000, nullptr);
      }
    }
    Preferences p;
    bool moveback = false, haveWas0 = false;
    uint8_t was0[32] = {0};
    if (p.begin("rm-upd", true)) { moveback = p.getBool("moveback", false); haveWas0 = p.getBytes("slot0", was0, 32) == 32; p.end(); }
    if (!moveback) { step = 2; return; }
    const esp_partition_t* a1 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
    if (run->subtype != ESP_PARTITION_SUBTYPE_APP_OTA_1) {
      // Running from the first slot after a WiFi update: the update didn't take, and the
      // bootloader went back to this one.
      if (a1 && esp_ota_get_state_partition(a1, &st) == ESP_OK && (st == ESP_OTA_IMG_ABORTED || st == ESP_OTA_IMG_INVALID)) {
        logs.add(LOG_WARN, "update: the new version didn't start; back on v%s", RABBITMESH_VERSION);
        nav.banner("the update didn't start", (String("back on v") + RABBITMESH_VERSION + ": nothing lost").c_str(), 8000, nullptr);
      }
      if (p.begin("rm-upd", false)) { p.putBool("moveback", false); p.end(); }
      step = 2;
      return;
    }
    // Running from the second slot: has the first been flashed by USB since the update?
    const esp_partition_t* a0 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
    uint8_t now0[32];
    // (not knowing what was there counts as unchanged: never back to an old version on a guess)
    const bool same = !haveWas0 || (a0 && esp_partition_get_sha256(a0, now0) == ESP_OK && !memcmp(now0, was0, 32));
    if (a0 && !same && esp_ota_set_boot_partition(a0) == ESP_OK) {
      // a working firmware someone put there by USB: that's what they meant to run
      logs.add(LOG_INFO, "update: the first slot was flashed by usb since; starting that");
      if (p.begin("rm-upd", false)) { p.putBool("moveback", false); p.end(); }
      nav.busy("starting the firmware flashed by usb...");
      delay(1500);
      app::reboot();
      return;
    }
    if (xTaskCreatePinnedToCore(moveBackTask, "updmove", 6144, &moved, 1, nullptr, 0) != pdPASS) step = 2;
    return;
  }
  if (step == 1 && moved) {                // the copy has finished, one way or the other
    logs.add(LOG_INFO, "update: %s", moved);
    step = 2;
  }
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
  afterUpdate();
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
    strlcpy(s_updUrl, s_foundUpd, sizeof s_updUrl);
    strlcpy(s_sigUrl, s_foundSig, sizeof s_sigUrl);
    s_lastOk = s_lastTry;
    const bool isNew = newer(s_latest, RABBITMESH_VERSION);
    logs.add(LOG_INFO, "update check: newest is %s, this is v%s", s_latest, RABBITMESH_VERSION);
    if (isNew && (s_manual || strcmp(s_told, s_latest))) {
      strlcpy(s_told, s_latest, sizeof s_told);
      char title[48];
      snprintf(title, sizeof title, "RabbitMesh %s is out", s_latest);
      nav.banner(title, installable() ? "settings > system: install it over wifi" : (String("flash its update file from ") + WHERE).c_str(), 15000, nullptr);
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
