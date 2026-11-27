// RabbitMesh WiFi + Bluetooth wardrive: the WiGLE CSV format and the "seen" table, kept apart
// from the radios so the host tests can run them (test/wigle_core_test.cpp).
//
// The log follows WiGLE's published CSV format, version 1.6 (api.wigle.net/csvFormat.html):
//   - a pre-header naming the producing app and device, and the planet;
//   - the fixed column header;
//   - one row per sighting. WiFi rows have the access point's capabilities, channel and
//     centre frequency. Bluetooth LE rows have "Misc [LE]" for capabilities, channel 0, an
//     empty frequency (no device class from an LE advert) and the manufacturer id when the
//     advert carries one.
// FirstSeen is UTC "YYYY-MM-DD hh:mm:ss"; AccuracyMeters is decimal metres.
//
// This only writes a file. Nothing here (or anywhere in RabbitMesh) talks to WiGLE's API:
// the owner uploads the file at wigle.net themselves.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

namespace wiglecore {

constexpr const char* HEADER =
    "MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type";

// The pre-header: appRelease is RabbitMesh's version, release the ESP-IDF version the firmware
// runs on (the device's "OS"). Observations are on Earth: Sol, body 3, sub-body 0.
inline int preHeader(char* out, size_t cap, const char* appRelease, const char* osRelease) {
  return snprintf(out, cap,
      "WigleWifi-1.6,appRelease=%s,model=T-Deck,release=%s,device=t-deck,display=ST7789 320x240,"
      "board=ESP32-S3,brand=LilyGo,star=Sol,body=3,subBody=0",
      appRelease, osRelease);
}

// The ESP32's wifi_auth_mode_t values, in its order, as Android-style capabilities.
enum Auth : uint8_t { AUTH_OPEN = 0, AUTH_WEP, AUTH_WPA_PSK, AUTH_WPA2_PSK, AUTH_WPA_WPA2_PSK, AUTH_WPA2_ENTERPRISE,
                      AUTH_WPA3_PSK, AUTH_WPA2_WPA3_PSK, AUTH_WAPI_PSK };
inline const char* authString(uint8_t a) {
  switch (a) {
    case AUTH_OPEN:            return "[ESS]";
    case AUTH_WEP:             return "[WEP][ESS]";
    case AUTH_WPA_PSK:         return "[WPA-PSK-CCMP+TKIP][ESS]";
    case AUTH_WPA2_PSK:        return "[WPA2-PSK-CCMP][ESS]";
    case AUTH_WPA_WPA2_PSK:    return "[WPA-PSK-CCMP+TKIP][WPA2-PSK-CCMP+TKIP][ESS]";
    case AUTH_WPA2_ENTERPRISE: return "[WPA2-EAP-CCMP][ESS]";
    case AUTH_WPA3_PSK:        return "[WPA3-SAE-CCMP][ESS]";
    case AUTH_WPA2_WPA3_PSK:   return "[WPA2-PSK-CCMP][WPA3-SAE-CCMP][ESS]";
    case AUTH_WAPI_PSK:        return "[WAPI-PSK][ESS]";
    default:                   return "[ESS]";
  }
}
constexpr const char* BLE_CAPS = "Misc [LE]";

// A 2.4 GHz channel's centre frequency in MHz (the ESP32-S3 has no 5 GHz); 0 if it isn't one.
inline int wifiFreqMHz(int ch) {
  if (ch >= 1 && ch <= 13) return 2407 + 5 * ch;
  if (ch == 14) return 2484;
  return 0;
}

// Position accuracy, metres: HDOP times ~5 m (a consumer receiver's typical range error).
// Without an HDOP, a cautious guess from the satellite count.
inline float accuracyM(float hdop, int sats) {
  if (hdop > 0) return hdop * 5.0f;
  return sats >= 8 ? 15.0f : sats >= 5 ? 30.0f : 60.0f;
}

inline void macString(char* out, const uint8_t mac[6]) {
  snprintf(out, 18, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
inline uint64_t macKey(const uint8_t mac[6]) {
  uint64_t k = 0;
  for (int i = 0; i < 6; i++) k = (k << 8) | mac[i];
  return k;
}

// A CSV field (rfc4180): control characters dropped, quoted (quotes doubled) when it holds a
// comma or quote. Always NUL-terminated within cap.
inline void csvField(char* out, size_t cap, const char* in) {
  if (!cap) return;
  bool quote = false;
  for (const char* p = in; *p; p++) if (*p == ',' || *p == '"') { quote = true; break; }
  size_t w = 0;
  auto put = [&](char c) { if (w + 1 < cap) out[w++] = c; };
  if (quote) put('"');
  for (const char* p = in; *p; p++) {
    const unsigned char c = (unsigned char)*p;
    if (c < 32 || c == 127) continue;
    if (c == '"') put('"');
    put((char)c);
  }
  if (quote) put('"');
  out[w] = 0;
}

// One row. freqMHz 0 and mfgrId < 0 leave those columns empty. Returns snprintf's length.
inline int row(char* out, size_t cap, const char* mac, const char* name, const char* caps, const char* timeUtc,
               int channel, int freqMHz, int rssi, double lat, double lon, float altM, float accM, int mfgrId, bool ble) {
  char nm[72], freq[12] = "", mfgr[12] = "";
  csvField(nm, sizeof nm, name ? name : "");
  if (freqMHz > 0) snprintf(freq, sizeof freq, "%d", freqMHz);
  if (mfgrId >= 0) snprintf(mfgr, sizeof mfgr, "%d", mfgrId);
  return snprintf(out, cap, "%s,%s,%s,%s,%d,%s,%d,%.6f,%.6f,%.0f,%.1f,,%s,%s\n", mac, nm, caps, timeUtc, channel, freq,
                  rssi, lat, lon, (double)altM, (double)accM, mfgr, ble ? "BLE" : "WIFI");
}

inline double metres(double la1, double lo1, double la2, double lo2) {
  const double R = 6371000.0, k = 3.14159265358979 / 180.0;
  const double dla = (la2 - la1) * k, dlo = (lo2 - lo1) * k;
  const double a = sin(dla / 2) * sin(dla / 2) + cos(la1 * k) * cos(la2 * k) * sin(dlo / 2) * sin(dlo / 2);
  return 2 * R * atan2(sqrt(a), sqrt(1 - a));
}

// What's been seen, so the log stays lean: a transmitter gets a row when first seen, and
// another only once BOTH reSeenS have passed AND you've moved minMoveM from where its last
// row was (a new vantage point helps WiGLE place it; parked, the same row again is just
// noise). Open-addressed table: MAC -> when and where its last row was.
struct Seen {
  struct Slot { uint64_t mac; uint32_t at; float lat, lon; };   // mac 0 = empty
  Slot* slots;
  size_t cap, n = 0;
  Seen(Slot* s, size_t c) : slots(s), cap(c) { clear(); }
  void clear() { for (size_t i = 0; i < cap; i++) slots[i].mac = 0; n = 0; }
  // 1: new (log it), 2: seen but due again (log it), 0: skip, -1: table full.
  int note(uint64_t mac, uint32_t nowS, uint32_t reSeenS, double lat, double lon, double minMoveM) {
    if (!mac) mac = 1ull << 63;                    // 00:00:00:00:00:00 still counts (no real 48-bit MAC is this)
    size_t i = (size_t)((mac * 0x9E3779B97F4A7C15ull) >> 40) % cap;
    for (size_t probe = 0; probe < cap; probe++, i = (i + 1) % cap) {
      Slot& s = slots[i];
      if (s.mac == mac) {
        if (nowS - s.at < reSeenS || metres(s.lat, s.lon, lat, lon) < minMoveM) return 0;
        s.at = nowS; s.lat = (float)lat; s.lon = (float)lon;
        return 2;
      }
      if (!s.mac) {
        if (n * 10 >= cap * 9) return -1;          // keep a tenth empty so probes end quickly
        s.mac = mac; s.at = nowS; s.lat = (float)lat; s.lon = (float)lon;
        n++;
        return 1;
      }
    }
    return -1;
  }
};

}  // namespace wiglecore
