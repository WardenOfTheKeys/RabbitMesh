// Host tests for the Flock hunt's on-air signatures (src/flock_core.h), on frames built byte by byte.
#include <cstdio>
#include <vector>
#include <cstring>
#include "flock_core.h"
using namespace flockcore;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static void ie(std::vector<uint8_t>& v, uint8_t id, std::vector<uint8_t> body) {
  v.push_back(id); v.push_back((uint8_t)body.size()); v.insert(v.end(), body.begin(), body.end());
}
// A probe request (fc 0x40) from addr2, to broadcast, with the given body.
static std::vector<uint8_t> probe(const uint8_t a2[6], const std::vector<uint8_t>& body) {
  std::vector<uint8_t> f = {0x40, 0x00, 0, 0, 0xff,0xff,0xff,0xff,0xff,0xff};
  f.insert(f.end(), a2, a2 + 6);
  for (int i = 0; i < 6; i++) f.push_back(0xff);
  f.push_back(0); f.push_back(0);
  f.insert(f.end(), body.begin(), body.end());
  return f;
}
// The camera's IEs: wildcard SSID, then the fingerprinted set.
static std::vector<uint8_t> cameraBody(bool wildcard = true, bool primary = true) {
  std::vector<uint8_t> b;
  if (wildcard) ie(b, 0, {}); else ie(b, 0, {'h','o','m','e'});
  ie(b, 2, {0x02});
  ie(b, 12, {0x00});
  ie(b, 127, {0,0,0,0,0,0,0,0x40});
  ie(b, 221, {0x50,0x6f,0x9a,0x16,0x03,0x01,primary ? (uint8_t)0x03 : (uint8_t)0x09});
  ie(b, 45, std::vector<uint8_t>(26, 0));
  ie(b, 191, std::vector<uint8_t>(12, 0));
  ie(b, 221, {0x00,0x50,0xf2,0x08,0x00,0x00,0x00});
  return b;
}

int main() {
  CHECK(OUI_COUNT == 34);
  const uint8_t flock[6] = {0xb4,0x1e,0x52,0x11,0x22,0x33};
  const uint8_t joplin[6] = {0x82,0x6b,0xf2,0x14,0x07,0x3a};
  const uint8_t phone[6] = {0x3a,0x11,0x22,0x33,0x44,0x55};
  CHECK(matchOui(flock) && matchOui(joplin) && !matchOui(phone));

  char sig[128];
  const auto body = cameraBody();
  CHECK(sigFromProbeBody(body.data(), (int)body.size(), sig, sizeof sig));
  CHECK(!strcmp(sig, SIG_PRIMARY));

  auto f = probe(flock, cameraBody());
  Hit h = classify(f.data(), (int)f.size(), true);
  CHECK(h.tier == T_FINGERPRINT && !memcmp(h.mac, flock, 6));

  auto fcs = f; fcs.insert(fcs.end(), {0xde,0xad,0xbe,0xef});           // the FCS left on the end
  CHECK(classify(fcs.data(), (int)fcs.size(), true).tier == T_FINGERPRINT);

  f = probe(joplin, cameraBody(true, false));                           // vendor IE differs
  CHECK(classify(f.data(), (int)f.size(), true).tier == T_PROBE);
  f = probe(flock, cameraBody(false));                                  // a directed probe: not wildcard
  CHECK(classify(f.data(), (int)f.size(), true).tier == T_OUI);
  f = probe(phone, cameraBody());                                       // not a camera prefix
  CHECK(classify(f.data(), (int)f.size(), true).tier == T_NONE);
  CHECK(classify(f.data(), (int)f.size(), false).tier == T_NONE);

  // a probe response from an access point to the camera: addr1 = camera (an echo)
  std::vector<uint8_t> resp = {0x50, 0x00, 0, 0};
  resp.insert(resp.end(), flock, flock + 6);
  resp.insert(resp.end(), phone, phone + 6);
  resp.insert(resp.end(), phone, phone + 6);
  resp.push_back(0); resp.push_back(0);
  h = classify(resp.data(), (int)resp.size(), true);
  CHECK(h.tier == T_ECHO && !memcmp(h.mac, flock, 6));
  CHECK(classify(resp.data(), 10, true).tier == T_NONE);                // too short: nothing, no overrun

  // a garbled body: no crash, no false fingerprint
  std::vector<uint8_t> junk(64, 0xff);
  f = probe(flock, junk);
  CHECK(classify(f.data(), (int)f.size(), true).tier == T_OUI);

  CHECK(bleNameMatches("Penguin-1234567890") && bleNameMatches("penguin-0000000000"));
  CHECK(!bleNameMatches("Penguin-123456789") && !bleNameMatches("Penguin-12345678901"));
  CHECK(bleNameMatches("1234567890") && !bleNameMatches("123456789") && !bleNameMatches("12345678901"));
  CHECK(bleNameMatches("FS Ext Battery") && bleNameMatches("DfuTarg") && !bleNameMatches("Pixel 8") && !bleNameMatches(nullptr));
  CHECK(bleServiceMatches("3101") && bleServiceMatches("00003500-0000-1000-8000-00805f9b34fb") && !bleServiceMatches("3501"));
  CHECK(bleServiceMatches("E8CCBB38-9532-46A8-9FE5-1814DF172E6F") && !bleServiceMatches("180f"));
  CHECK(!strcmp(tierWords(T_FINGERPRINT), "camera signature"));

  {
    // Shared-vendor relabel: every listed maker's prefix is one the hunt listens for
    for (const auto& v : SHARED) CHECK(matchOui(v.oui));
    const uint8_t liteon[6] = {0xd0, 0x39, 0x57, 0xe8, 0xe6, 0x59};   // a Liteon laptop, heard as an echo
    const uint8_t flock[6] = {0xb4, 0x1e, 0x52, 0x01, 0x02, 0x03};    // Flock's own prefix
    const uint8_t atheros[6] = {0x00, 0x03, 0x7f, 0x01, 0x02, 0x03};  // the camera radio's default
    CHECK(sharedVendor(liteon) && !strcmp(sharedVendor(liteon)->vendor, "Liteon"));
    CHECK(vendorOnly(T_ECHO, liteon, false, false));
    CHECK(vendorOnly(T_OUI, liteon, false, false));
    CHECK(vendorOnly(T_PROBE, liteon, false, false));        // a bare wildcard probe: laptops send those
    CHECK(!vendorOnly(T_FINGERPRINT, liteon, false, false)); // the camera's own fingerprint still counts
    CHECK(!vendorOnly(T_ECHO, liteon, false, true));         // so does one by a mapped camera
    CHECK(!vendorOnly(T_OUI, liteon, true, false));          // BLE hits are matched by name, not prefix
    CHECK(!vendorOnly(T_ECHO, flock, false, false) && !vendorOnly(T_OUI, atheros, false, false));
    // the meter: any camera-grade hit before any maker-only one
    CHECK(rank(T_ECHO, flock, false, false) > rank(T_PROBE, liteon, false, false));
    CHECK(rank(T_FINGERPRINT, liteon, false, false) > rank(T_PROBE, flock, false, false));
    char w[48];
    hitWords(w, sizeof w, T_ECHO, liteon, false, false);
    CHECK(!strcmp(w, "Liteon radio - laptops use these too"));
    hitWords(w, sizeof w, T_PROBE, liteon, false, false);
    CHECK(!strcmp(w, "Liteon radio probing - laptops do too"));
    hitWords(w, sizeof w, T_ECHO, liteon, false, true);
    CHECK(!strcmp(w, "camera echo (weak)"));
    hitWords(w, sizeof w, T_PROBE, flock, false, false);
    CHECK(!strcmp(w, "likely camera"));
  }
  printf(fails ? "%d FAILED\n" : "all flock_core tests passed\n", fails);
  return fails != 0;
}
