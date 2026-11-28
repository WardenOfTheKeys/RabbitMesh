// Host tests for the WiFi update's parts without network or flash (src/updcore.h), and, when the
// MeshCore library has been fetched (after a build), that the T-Deck's own Ed25519 code accepts
// a signature tools/sign_release.py makes (cryptography's Ed25519) and refuses a changed one.
#include <cstdio>
#include <cstring>
#include "updcore.h"
#ifdef WITH_ED25519
#include "ed_25519.h"
#endif
using namespace updcore;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

// sha256("RabbitMesh release signing test"), signed with the release key by tools/sign_release.py's method
static const uint8_t T_DIGEST[32] = {0x8f, 0xa4, 0x7d, 0x65, 0x5b, 0x90, 0x33, 0xb5, 0x08, 0x91, 0xe7, 0x2f, 0x61, 0xa9, 0x7b, 0xe6, 0x77, 0x4d, 0x19, 0xc8, 0x2a, 0xb0, 0x26, 0x9e, 0x57, 0x50, 0xaf, 0x42, 0x29, 0xa6, 0x48, 0x80};
static const uint8_t T_SIG[64] = {0x01, 0xd3, 0xeb, 0xe0, 0xb1, 0x27, 0xb4, 0x0b, 0x88, 0x8d, 0xea, 0xcd, 0x01, 0x42, 0xe1, 0x6f, 0x65, 0x28, 0x6f, 0xe2, 0xe6, 0x82, 0x33, 0x98, 0x15, 0xa7, 0x66, 0x13, 0x47, 0x5d, 0xf2, 0x1e, 0xe2, 0xbb, 0xfc, 0x19, 0xd0, 0x22, 0x3b, 0xc4, 0xab, 0x05, 0x5c, 0x97, 0x05, 0xfb, 0xd2, 0xe2, 0x93, 0xa6, 0x8f, 0xe1, 0x7c, 0x9d, 0x13, 0xf8, 0x19, 0xe4, 0xce, 0xcd, 0x70, 0x7f, 0x64, 0x07};

int main() {
  CHECK(newer("v0.1.5", "0.1.4") && newer("v0.2.0", "0.1.9") && newer("v0.1.10", "0.1.9"));
  CHECK(!newer("v0.1.4", "0.1.4") && !newer("v0.1.3", "0.1.4") && !newer("garbage", "0.1.4"));

  // a GitHub release answer, cut down: the signature's name starts with the update file's
  const char* body =
    "{\"tag_name\": \"v0.1.5\", \"assets\": ["
    "{\"name\": \"SHA256SUMS.txt\", \"browser_download_url\": \"https://github.com/WardenOfTheKeys/RabbitMesh/releases/download/v0.1.5/SHA256SUMS.txt\"},"
    "{\"name\": \"x.sig\", \"browser_download_url\": \"https://github.com/WardenOfTheKeys/RabbitMesh/releases/download/v0.1.5/rabbitmesh-0.1.5-tdeck-update.bin.sig\"},"
    "{\"name\": \"x.bin\", \"browser_download_url\": \"https://github.com/WardenOfTheKeys/RabbitMesh/releases/download/v0.1.5/rabbitmesh-0.1.5-tdeck-update.bin\"},"
    "{\"name\": \"full\", \"browser_download_url\": \"https://github.com/WardenOfTheKeys/RabbitMesh/releases/download/v0.1.5/rabbitmesh-0.1.5-tdeck-full.bin\"}]}";
  char tag[24], url[256];
  CHECK(field(body, "tag_name", tag, sizeof tag) > 0 && !strcmp(tag, "v0.1.5"));
  CHECK(asset(body, UPDATE_SUFFIX, url, sizeof url) && strstr(url, "update.bin") && !strstr(url, ".sig"));
  CHECK(trustedUrl(url));
  CHECK(asset(body, SIG_SUFFIX, url, sizeof url) && strstr(url, "update.bin.sig"));
  CHECK(!asset(body, "-tdeck-nothing.bin", url, sizeof url));
  char tiny[16];
  CHECK(!asset(body, UPDATE_SUFFIX, tiny, sizeof tiny));     // too long for the buffer: not found, not cut short
  CHECK(field("{\"tag_name\": 5}", "tag_name", tag, sizeof tag) < 0);
  // only RabbitMesh's own releases
  CHECK(!trustedUrl("https://github.com/someone/rabbitmesh/releases/download/v9/x-tdeck-update.bin"));
  CHECK(!trustedUrl("http://github.com/WardenOfTheKeys/rabbitmesh/releases/download/v9/x"));
  CHECK(!trustedUrl("https://github.com.evil.example/WardenOfTheKeys/rabbitmesh/releases/download/"));

#ifdef WITH_ED25519
  CHECK(ed25519_verify(T_SIG, T_DIGEST, 32, RELEASE_KEY) == 1);
  uint8_t bad[64];
  memcpy(bad, T_SIG, 64); bad[10] ^= 1;
  CHECK(ed25519_verify(bad, T_DIGEST, 32, RELEASE_KEY) == 0);
  uint8_t d2[32];
  memcpy(d2, T_DIGEST, 32); d2[0] ^= 0x80;
  CHECK(ed25519_verify(T_SIG, d2, 32, RELEASE_KEY) == 0);
  printf("ed25519: the T-Deck's verifier accepts the release signature and refuses changed ones\n");
#endif
  if (fails) { printf("%d updcore test(s) failed\n", fails); return 1; }
  printf("all updcore tests passed\n");
  return 0;
}
