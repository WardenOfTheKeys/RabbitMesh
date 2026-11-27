// Host tests for the WiFi/BLE wardrive's WiGLE CSV 1.6 output and seen table (src/wigle_core.h).
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include "wigle_core.h"
using namespace wiglecore;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static int commas(const char* s) { int n = 0; for (; *s; s++) if (*s == ',') n++; return n; }

int main() {
  char ph[256];
  preHeader(ph, sizeof ph, "0.1.0", "v4.4.7");
  const char* want = "WigleWifi-1.6,appRelease=0.1.0,model=T-Deck,release=v4.4.7,";
  CHECK(!strncmp(ph, want, strlen(want)));
  CHECK(strstr(ph, ",star=Sol,body=3,subBody=0"));
  CHECK(commas(HEADER) == 13);                          // 14 columns

  CHECK(!strcmp(authString(AUTH_OPEN), "[ESS]"));
  CHECK(!strcmp(authString(AUTH_WPA2_PSK), "[WPA2-PSK-CCMP][ESS]"));
  CHECK(!strcmp(authString(200), "[ESS]"));
  CHECK(!strcmp(BLE_CAPS, "Misc [LE]"));

  CHECK(wifiFreqMHz(1) == 2412 && wifiFreqMHz(6) == 2437 && wifiFreqMHz(13) == 2472 && wifiFreqMHz(14) == 2484);
  CHECK(wifiFreqMHz(0) == 0 && wifiFreqMHz(36) == 0);

  CHECK(accuracyM(1.2f, 9) == 6.0f);
  CHECK(accuracyM(0, 9) == 15 && accuracyM(0, 6) == 30 && accuracyM(0, 3) == 60);

  const uint8_t m[6] = {0xde, 0xad, 0xbe, 0xef, 0x00, 0x01};
  char ms[18];
  macString(ms, m);
  CHECK(!strcmp(ms, "de:ad:be:ef:00:01"));
  CHECK(macKey(m) == 0xdeadbeef0001ull);

  char f[32];
  csvField(f, sizeof f, "plain");            CHECK(!strcmp(f, "plain"));
  csvField(f, sizeof f, "a,b");              CHECK(!strcmp(f, "\"a,b\""));
  csvField(f, sizeof f, "say \"hi\"");       CHECK(!strcmp(f, "\"say \"\"hi\"\"\""));
  csvField(f, sizeof f, "tab\there\n");      CHECK(!strcmp(f, "tabhere"));
  char tiny[4];
  csvField(tiny, sizeof tiny, "abcdef");     CHECK(!strcmp(tiny, "abc"));

  // the rows match the page's examples column for column
  char r[256];
  row(r, sizeof r, ms, "Cafe, Free", authString(AUTH_OPEN), "2026-11-26 14:00:00", 6, wifiFreqMHz(6), -71,
      36.85, -76.29, 12.4f, 6.0f, -1, false);
  CHECK(!strcmp(r, "de:ad:be:ef:00:01,\"Cafe, Free\",[ESS],2026-11-26 14:00:00,6,2437,-71,36.850000,-76.290000,12,6.0,,,WIFI\n"));
  row(r, sizeof r, "63:56:ac:c4:d4:30", "", BLE_CAPS, "2018-08-03 18:14:12", 0, 0, -67, 37.76090571, -122.44877987,
      104, 49.3f, 72, true);
  CHECK(!strcmp(r, "63:56:ac:c4:d4:30,,Misc [LE],2018-08-03 18:14:12,0,,-67,37.760906,-122.448780,104,49.3,,72,BLE\n"));
  CHECK(commas(r) == 13);

  // seen: once, then again only after the time AND a move
  std::vector<Seen::Slot> mem(10);
  Seen s(mem.data(), mem.size());
  const double la = 36.85, lo = -76.29, la2 = 36.8510;  // la2 is ~111 m north
  CHECK(s.note(1, 100, 60, la, lo, 50) == 1);
  CHECK(s.note(1, 130, 60, la2, lo, 50) == 0);           // moved, but too soon
  CHECK(s.note(1, 200, 60, la, lo, 50) == 0);            // long enough, but parked: no repeat
  CHECK(s.note(1, 200, 60, la2, lo, 50) == 2);           // both: log again
  CHECK(s.note(1, 300, 60, la2, lo, 50) == 0);           // parked at the new spot
  CHECK(s.note(0, 100, 60, la, lo, 50) == 1 && s.n == 2);
  int added = 0;
  for (uint64_t k = 100; k < 200; k++) if (s.note(k, 100, 60, la, lo, 50) == 1) added++;
  CHECK(s.n == 9 && added == 7);                         // stops at 90% full
  CHECK(s.note(5000, 100, 60, la, lo, 50) == -1);

  printf(fails ? "%d FAILED\n" : "all wigle_core tests passed\n", fails);
  return fails != 0;
}
