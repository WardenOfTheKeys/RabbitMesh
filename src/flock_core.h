// RabbitMesh Flock hunt: hearing a Flock Safety ALPR camera on the air. Kept apart from the
// radio so the host tests run it (test/flock_core_test.cpp).
//
// The signatures and the detection method are the community's, ported from flock-you by
// colonelpanichacks (github.com/colonelpanichacks/flock-you, MIT, commit 1e814bc0,
// licenses/flock-you-MIT.txt), which credits:
//   - @NitekryDPaul (nitekry/nite-oui-collection): the Flock camera OUI list and the addr1
//     (receiver-side) technique;
//   - DeFlockJoplin: the wildcard probe-request signature, the information-element fingerprint
//     and the 82:6b:f2 prefix;
//   - a Flock camera firmware dump: Flock's own IEEE prefix, the Qualcomm radio prefix and the
//     battery-pack BLE signatures.
//
// What a camera does, per that research: it sends wildcard probe requests (an empty SSID)
// about every 125 ms, channel-hopping, from one of these prefixes, with a recognisable set of
// information elements. Hearing that is the strongest sign. Confidence tiers, as flock-you's:
//   4  probe request from a camera prefix, wildcard SSID, and the IE fingerprint
//   3  probe request from a camera prefix, wildcard SSID, fingerprint didn't match
//   2  any other frame transmitted from a camera prefix, or a camera-battery BLE advert
//   1  a camera prefix as the receiver or BSSID (an access point answering it: an echo,
//      and the noisiest)
// Listening only: nothing is ever transmitted at a camera.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

namespace flockcore {

enum Tier : uint8_t { T_NONE = 0, T_ECHO = 1, T_OUI = 2, T_PROBE = 3, T_FINGERPRINT = 4 };
inline const char* tierName(uint8_t t) {
  switch (t) {
    case T_FINGERPRINT: return "fingerprint";
    case T_PROBE:       return "probe";
    case T_OUI:         return "prefix";
    case T_ECHO:        return "echo";
    default:            return "";
  }
}
// How sure, in words, for the screen.
inline const char* tierWords(uint8_t t) {
  switch (t) {
    case T_FINGERPRINT: return "camera signature";
    case T_PROBE:       return "likely camera";
    case T_OUI:         return "possible camera";
    case T_ECHO:        return "camera echo (weak)";
    default:            return "";
  }
}

// The 34 prefixes (flock-you's union list).
static const uint8_t OUIS[][3] = {
  {0x70,0xc9,0x4e}, {0x3c,0x91,0x80}, {0xd8,0xf3,0xbc}, {0x80,0x30,0x49}, {0xb8,0x35,0x32},
  {0x14,0x5a,0xfc}, {0x74,0x4c,0xa1}, {0x08,0x3a,0x88}, {0x9c,0x2f,0x9d}, {0xc0,0x35,0x32},
  {0x94,0x08,0x53}, {0xe4,0xaa,0xea}, {0xf4,0x6a,0xdd}, {0xe0,0x0a,0xf6}, {0x24,0xb2,0xb9},
  {0x00,0xf4,0x8d}, {0xd0,0x39,0x57}, {0xe8,0xd0,0xfc}, {0xe0,0x4f,0x43}, {0xb8,0x1e,0xa4},
  {0x70,0x08,0x94}, {0x58,0x8e,0x81}, {0xec,0x1b,0xbd}, {0x3c,0x71,0xbf}, {0x58,0x00,0xe3},
  {0x90,0x35,0xea}, {0x5c,0x93,0xa2}, {0x64,0x6e,0x69}, {0x48,0x27,0xea}, {0xa4,0xcf,0x12},
  {0x14,0xb5,0xcd},
  {0x82,0x6b,0xf2},   // DeFlockJoplin; locally-administered bit set, so no LA-MAC filter
  {0xb4,0x1e,0x52},   // Flock Safety's IEEE MA-L
  {0x00,0x03,0x7f},   // Qualcomm Atheros QCA9377 (the camera radio's default MACs)
};
constexpr size_t OUI_COUNT = sizeof(OUIS) / sizeof(OUIS[0]);

inline bool matchOui(const uint8_t* mac) {
  for (size_t i = 0; i < OUI_COUNT; i++)
    if (mac[0] == OUIS[i][0] && mac[1] == OUIS[i][1] && mac[2] == OUIS[i][2]) return true;
  return false;
}

// Prefixes the IEEE registry gives to companies whose WiFi modules go in everyday gear: the
// cameras use Liteon radios, but so do a great many laptops, printers and TVs (22 of the 34
// prefixes are Liteon's). A hit from one of these that is only the prefix, an echo, or a bare
// wildcard probe (every laptop looking for WiFi sends those) says no more than "a radio from
// that maker is near", so it's shown as that, muted, with no alarm. The camera's own probe
// fingerprint, and any hit near a mapped camera, still count as a camera. The other prefixes
// (Flock's own, the radio's default Atheros address, the unregistered ones) aren't in here.
// Registrants checked against the IEEE MA-L registry, 2026-11-27.
struct SharedVendor { uint8_t oui[3]; const char* vendor; const char* also; };
static const SharedVendor SHARED[] = {
  {{0x70,0xc9,0x4e}, "Liteon", "laptops"}, {{0x3c,0x91,0x80}, "Liteon", "laptops"}, {{0xd8,0xf3,0xbc}, "Liteon", "laptops"},
  {{0x80,0x30,0x49}, "Liteon", "laptops"}, {{0x14,0x5a,0xfc}, "Liteon", "laptops"}, {{0x74,0x4c,0xa1}, "Liteon", "laptops"},
  {{0x9c,0x2f,0x9d}, "Liteon", "laptops"}, {{0xc0,0x35,0x32}, "Liteon", "laptops"}, {{0x94,0x08,0x53}, "Liteon", "laptops"},
  {{0xe4,0xaa,0xea}, "Liteon", "laptops"}, {{0xf4,0x6a,0xdd}, "Liteon", "laptops"}, {{0xe0,0x0a,0xf6}, "Liteon", "laptops"},
  {{0x00,0xf4,0x8d}, "Liteon", "laptops"}, {{0xd0,0x39,0x57}, "Liteon", "laptops"}, {{0xe8,0xd0,0xfc}, "Liteon", "laptops"},
  {{0xb8,0x1e,0xa4}, "Liteon", "laptops"}, {{0x58,0x00,0xe3}, "Liteon", "laptops"}, {{0x5c,0x93,0xa2}, "Liteon", "laptops"},
  {{0x64,0x6e,0x69}, "Liteon", "laptops"},
  {{0x08,0x3a,0x88}, "USI", "laptops"}, {{0xe0,0x4f,0x43}, "USI", "laptops"},
  {{0x58,0x8e,0x81}, "Silicon Labs", "smart gadgets"}, {{0xec,0x1b,0xbd}, "Silicon Labs", "smart gadgets"},
  {{0x90,0x35,0xea}, "Silicon Labs", "smart gadgets"},
  {{0x3c,0x71,0xbf}, "Espressif", "smart gadgets"}, {{0xa4,0xcf,0x12}, "Espressif", "smart gadgets"},
  {{0x48,0x27,0xea}, "Samsung", "phones"},
};
inline const SharedVendor* sharedVendor(const uint8_t* mac) {
  for (const auto& v : SHARED)
    if (mac[0] == v.oui[0] && mac[1] == v.oui[1] && mac[2] == v.oui[2]) return &v;
  return nullptr;
}
// Only "a radio from that maker": see above. Bluetooth hits are matched by the battery pack's
// name or company id, not the prefix, so they never are.
inline bool vendorOnly(uint8_t tier, const uint8_t* mac, bool ble, bool confirmed) {
  return !ble && !confirmed && tier <= T_PROBE && sharedVendor(mac) != nullptr;
}
// Which hit the meter follows: any camera-grade hit before any vendor-only one, then by tier.
inline uint8_t rank(uint8_t tier, const uint8_t* mac, bool ble, bool confirmed) {
  return (uint8_t)(tier + (vendorOnly(tier, mac, ble, confirmed) ? 0 : 4));
}
// The words for the screen.
inline void hitWords(char* out, size_t cap, uint8_t tier, const uint8_t* mac, bool ble, bool confirmed) {
  if (vendorOnly(tier, mac, ble, confirmed)) {
    const SharedVendor* v = sharedVendor(mac);
    if (tier == T_PROBE) snprintf(out, cap, "%s radio probing - %s do too", v->vendor, v->also);
    else snprintf(out, cap, "%s radio - %s use these too", v->vendor, v->also);
  } else {
    snprintf(out, cap, "%s", tierWords(tier));
  }
}
inline bool isMulticast(const uint8_t* mac) { return mac[0] & 0x01; }

// 1: the SSID IE is there and empty (a wildcard probe); 0: there with a name; -1: not found.
inline int wildcardProbe(const uint8_t* body, int len) {
  if (!body || len < 2) return -1;
  while (len >= 2) {
    const uint8_t id = body[0], elen = body[1];
    if ((int)elen + 2 > len) break;
    if (id == 0) return elen == 0 ? 1 : 0;
    body += elen + 2;
    len -= elen + 2;
  }
  return -1;
}

// ---- the information-element fingerprint (flock-you's builder, kept to its behaviour) ------------
// The IEs after the SSID, as "tag,tag,221:<first 8 vendor bytes hex>,...". The drive-tested
// camera fingerprint is PRIMARY. The builder copes with what the ESP32 driver hands over: a
// length that overruns (a phantom tag-64/len-128 header, or the 4-byte FCS left on), by
// skipping or resyncing to the next plausible IE.
constexpr const char* SIG_PRIMARY = "2,12,127,221:506f9a16030103,45,191,221:0050f208000000";
constexpr const char* LITEON_PREFIX = "221:506f9a16030103";
constexpr int PHANTOM_SKIP_CAP = 16, TLV_RESYNC_MAX = 64;

inline bool liteonVendorAt(const uint8_t* ies, int len, int pos) {
  return pos + 9 <= len && ies[pos] == 221 && ies[pos + 1] == 7 && ies[pos + 2] == 0x50 && ies[pos + 3] == 0x6f &&
         ies[pos + 4] == 0x9a;
}
inline bool phantomLiteonAhead(const uint8_t* ies, int len, int pos) {
  int end = pos + 2 + 32;
  if (end > len - 1) end = len - 1;
  for (int j = pos + 2; j < end; j++) if (liteonVendorAt(ies, len, j)) return true;
  return false;
}
inline bool isPhantomOverflow(const uint8_t* ies, int len, uint8_t id, int elen, int i) {
  if (i + 2 + elen <= len) return false;
  if (elen > 200) return true;
  return id == 64 && elen == 128 && phantomLiteonAhead(ies, len, i);
}
inline int tlvResync(const uint8_t* ies, int len, int start) {
  int end = start + TLV_RESYNC_MAX;
  if (end > len - 1) end = len - 1;
  for (int j = start; j < end; j++) {
    const int elen = ies[j + 1];
    if (elen <= 200 && j + 2 + elen <= len) return j;
  }
  return -1;
}
inline bool sigAppend(char* out, size_t cap, size_t* pos, const char* part) {
  const size_t plen = strlen(part);
  if (*pos != 0) {
    if (*pos + 1 >= cap) return false;
    out[(*pos)++] = ',';
  }
  if (*pos + plen >= cap) return false;
  memcpy(out + *pos, part, plen);
  *pos += plen;
  out[*pos] = 0;
  return true;
}
inline bool buildSigFromIes(const uint8_t* ies, int len, char* out, size_t cap, bool* complete) {
  if (!ies || len < 2 || !out || cap < 2) return false;
  size_t pos = 0;
  out[0] = 0;
  int i = 0;
  uint8_t phantomSkips = 0;
  while (i + 2 <= len) {
    const uint8_t id = ies[i];
    const int elen = ies[i + 1];
    if (i + 2 + elen > len) {
      if (phantomSkips < PHANTOM_SKIP_CAP && isPhantomOverflow(ies, len, id, elen, i)) { phantomSkips++; i += 2; continue; }
      const int j = tlvResync(ies, len, i);
      if (j > i) { i = j; continue; }
      return false;
    }
    i += 2;
    if (id == 0) {                                     // the SSID: skipped (and any empty pairs after it)
      if (elen == 0) { while (i + 2 <= len && ies[i] == 0 && ies[i + 1] == 0) i += 2; }
      else i += elen;
      continue;
    }
    char part[24];
    if (id == 221 && elen >= 4) {
      static const char hd[] = "0123456789abcdef";
      const int take = elen < 8 ? elen : 8;
      memcpy(part, "221:", 4);
      for (int k = 0; k < take; k++) { part[4 + k * 2] = hd[ies[i + k] >> 4]; part[5 + k * 2] = hd[ies[i + k] & 15]; }
      part[4 + take * 2] = 0;
    } else {
      snprintf(part, sizeof part, "%u", (unsigned)id);
    }
    if (!sigAppend(out, cap, &pos, part)) return false;
    i += elen;
  }
  if (complete) *complete = (i == len);
  return pos > 0;
}
inline void canonicalize(char* sig, size_t cap) {
  if (!sig || cap < 8) return;
  if (!strncmp(sig, "2,12,127,", 9) && strstr(sig, LITEON_PREFIX)) return;
  const char* anchor = strstr(sig, LITEON_PREFIX);
  if (!anchor) return;
  char tmp[128];
  const int n = snprintf(tmp, sizeof tmp, "2,12,127,%s", anchor);
  if (n > 0 && (size_t)n < cap) memcpy(sig, tmp, (size_t)n + 1);
}
inline bool sigFromProbeBody(const uint8_t* body, int bodyLen, char* out, size_t cap) {
  if (!body || bodyLen < 2 || !out || cap < 16) return false;
  char a[128] = {0}, b[128] = {0};
  bool ca = false, cb = false;
  const bool okA = buildSigFromIes(body, bodyLen, a, sizeof a, &ca);
  bool okB = false;
  if (bodyLen >= 2 && body[0] == 0 && body[1] == 0) okB = buildSigFromIes(body + 2, bodyLen - 2, b, sizeof b, &cb);
  const char* sa = okA ? a : "";
  const char* sb = okB ? b : "";
  if (!sa[0] && !sb[0]) return false;
  const char* pick = !sb[0] ? sa : !sa[0] ? sb : (ca && !cb) ? sa : (!ca && cb) ? sb : (strlen(sb) > strlen(sa) ? sb : sa);
  char merged[128];
  strncpy(merged, pick, sizeof merged - 1);
  merged[sizeof merged - 1] = 0;
  canonicalize(merged, sizeof merged);
  strncpy(out, merged, cap - 1);
  out[cap - 1] = 0;
  return out[0] != 0;
}
inline bool probeHasPrimarySig(const uint8_t* body, int bodyLen) {
  char sig[128];
  if (sigFromProbeBody(body, bodyLen, sig, sizeof sig) && !strcmp(sig, SIG_PRIMARY)) return true;
  if (bodyLen > 4 && sigFromProbeBody(body, bodyLen - 4, sig, sizeof sig) && !strcmp(sig, SIG_PRIMARY)) return true;
  return false;
}

// ---- one 802.11 frame --------------------------------------------------------------------------------
// frame: the 802.11 header (24 bytes: fc[2], duration[2], addr1, addr2, addr3, seq[2]) then the
// body; mgmt: the driver called it a management frame. Returns the best tier and which MAC it
// was about (the camera's).
struct Hit { uint8_t tier = T_NONE; uint8_t mac[6]; };
constexpr int HDR = 24;
inline Hit classify(const uint8_t* frame, int len, bool mgmt) {
  Hit h;
  if (!frame || len < HDR) return h;
  const uint8_t* a1 = frame + 4;
  const uint8_t* a2 = frame + 10;
  const uint8_t* a3 = frame + 16;
  if (matchOui(a2)) {
    h.tier = T_OUI;
    if (mgmt) {
      const uint8_t fc0 = frame[0];
      const uint8_t type = (fc0 >> 2) & 3, sub = (fc0 >> 4) & 15;
      if (type == 0 && sub == 4) {                           // a probe request
        const uint8_t* body = frame + HDR;
        const int bodyLen = len - HDR;
        int r = bodyLen > 0 ? wildcardProbe(body, bodyLen) : -1;
        if (r == -1 && bodyLen > 4) r = wildcardProbe(body, bodyLen - 4);   // the FCS left on
        if (r == 1) h.tier = probeHasPrimarySig(body, bodyLen) ? T_FINGERPRINT : T_PROBE;
      }
    }
    memcpy(h.mac, a2, 6);
    return h;
  }
  if (!isMulticast(a1) && matchOui(a1)) { h.tier = T_ECHO; memcpy(h.mac, a1, 6); return h; }
  if (mgmt && !isMulticast(a3) && matchOui(a3)) { h.tier = T_ECHO; memcpy(h.mac, a3, 6); return h; }
  return h;
}

// ---- BLE: the camera's battery pack ------------------------------------------------------------------
// "Penguin-" and exactly 10 digits, a bare 10-digit serial, "FS Ext Battery" or "DfuTarg"
// (a pack taking a firmware update); or manufacturer data with company id 0x09C8 (XUNTONG);
// or Flock's accessory service, the Nordic legacy DFU service, or a 16-bit service in
// 0x3100-0x3500 (Raven). The research notes BLE detection stopped working in the spring, so
// these count only as tier 2.
inline bool bleNameMatches(const char* n) {
  if (!n) return false;
  if (!strncasecmp(n, "penguin-", 8)) {
    for (int i = 8; i < 18; i++) if (!isdigit((unsigned char)n[i])) return false;
    return n[18] == 0;
  }
  bool ten = true;
  for (int i = 0; i < 10; i++) if (!isdigit((unsigned char)n[i])) { ten = false; break; }
  if (ten && n[10] == 0) return true;
  return !strcasecmp(n, "fs ext battery") || !strcasecmp(n, "dfutarg");
}
constexpr uint16_t BLE_MFG_XUNTONG = 0x09C8;
constexpr uint16_t RAVEN_SVC_MIN = 0x3100, RAVEN_SVC_MAX = 0x3500;
constexpr const char* FLOCK_GATT_UUID = "e8ccbb38-9532-46a8-9fe5-1814df172e6f";
constexpr const char* NORDIC_DFU_UUID = "00001530-1212-efde-1523-785feabcd123";
inline bool bleServiceMatches(const char* uuid) {
  if (!uuid) return false;
  if (!strcasecmp(uuid, FLOCK_GATT_UUID) || !strcasecmp(uuid, NORDIC_DFU_UUID)) return true;
  // a 16-bit UUID, written short ("3101") or in the Bluetooth base form ("00003101-0000-1000-8000-00805f9b34fb")
  unsigned v = 0;
  if (strlen(uuid) == 4 && sscanf(uuid, "%4x", &v) == 1) return v >= RAVEN_SVC_MIN && v <= RAVEN_SVC_MAX;
  if (strlen(uuid) == 36 && !strncasecmp(uuid, "0000", 4) && !strcasecmp(uuid + 8, "-0000-1000-8000-00805f9b34fb") &&
      sscanf(uuid + 4, "%4x", &v) == 1)
    return v >= RAVEN_SVC_MIN && v <= RAVEN_SVC_MAX;
  return false;
}

}  // namespace flockcore
