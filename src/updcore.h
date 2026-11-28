// RabbitMesh updates over WiFi: the parts with no network or flash in them, so they're tested
// on a computer (test/updcore_test.cpp). updcheck.cpp does the rest.
//
// What a release carries (.github/workflows/release.yml):
//   rabbitmesh-<v>-tdeck-update.bin      the firmware (USB: flash it at 0x10000)
//   rabbitmesh-<v>-tdeck-update.bin.sig  64 bytes: an Ed25519 signature of the file's SHA-256,
//                                        made with RabbitMesh's release key (tools/sign_release.py)
// The T-Deck downloads both from RabbitMesh's own latest GitHub release, hashes the firmware as
// it writes it to the spare app slot, and switches to it only if the signature checks out
// against RELEASE_KEY below. Anything else (a broken download, a file from anywhere else, a
// release without a signature) is thrown away and nothing changes.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>     // strncasecmp

namespace updcore {

// The public half of RabbitMesh's release key. Its private half is a GitHub Actions secret
// (RABBITMESH_SIGNING_KEY) that only the release job uses.
static const uint8_t RELEASE_KEY[32] = {
  0x71, 0x45, 0x17, 0x44, 0xc9, 0x8b, 0x75, 0x7b, 0x4d, 0x43, 0xa0, 0xd6, 0xda, 0xd4, 0x4c, 0x50,
  0x4a, 0x89, 0xb3, 0xea, 0x49, 0x8e, 0x39, 0x73, 0x25, 0xd5, 0x4d, 0x49, 0x02, 0x57, 0xa5, 0x46,
};

constexpr const char* UPDATE_SUFFIX = "-tdeck-update.bin";
constexpr const char* SIG_SUFFIX = "-tdeck-update.bin.sig";
constexpr int SIG_LEN = 64;
constexpr uint8_t APP_MAGIC = 0xE9;                 // an ESP32 app image's first byte

// "v0.1.10" > "0.1.9": numbers compared part by part.
inline bool newer(const char* tag, const char* mine) {
  int a[3] = {0, 0, 0}, b[3] = {0, 0, 0};
  sscanf(tag[0] == 'v' || tag[0] == 'V' ? tag + 1 : tag, "%d.%d.%d", &a[0], &a[1], &a[2]);
  sscanf(mine[0] == 'v' || mine[0] == 'V' ? mine + 1 : mine, "%d.%d.%d", &b[0], &b[1], &b[2]);
  for (int i = 0; i < 3; i++) if (a[i] != b[i]) return a[i] > b[i];
  return false;
}

// A string field of GitHub's release answer: the first "<key>": "<value>" at or after from.
// Returns where the value ends (to search on from), or -1; the value goes to out.
inline int field(const char* body, const char* key, char* out, size_t outLen, int from = 0) {
  char pat[48];
  snprintf(pat, sizeof pat, "\"%s\"", key);
  const char* p = strstr(body + from, pat);
  if (!p) return -1;
  p = strchr(p + strlen(pat), ':');
  if (!p) return -1;
  p = strchr(p, '"');
  if (!p) return -1;
  const char* e = strchr(p + 1, '"');
  if (!e || (size_t)(e - p - 1) >= outLen) return -1;
  memcpy(out, p + 1, e - p - 1);
  out[e - p - 1] = 0;
  return (int)(e - body) + 1;
}

// The download address of the release's file whose name ends with suffix (and only that: the
// update file's suffix is also the start of the signature's). True if found.
inline bool asset(const char* body, const char* suffix, char* url, size_t urlLen) {
  int at = 0;
  char u[256];
  while ((at = field(body, "browser_download_url", u, sizeof u, at)) >= 0) {
    const size_t n = strlen(u), s = strlen(suffix);
    if (n >= s && !strcmp(u + n - s, suffix)) {
      if (strlen(u) >= urlLen) return false;
      strcpy(url, u);
      return true;
    }
  }
  return false;
}

// Only RabbitMesh's own release files: on GitHub, in RabbitMesh's repository.
inline bool trustedUrl(const char* url) {
  static const char* P = "https://github.com/WardenOfTheKeys/rabbitmesh/releases/download/";
  return !strncasecmp(url, P, strlen(P));
}

}  // namespace updcore
