// RabbitMesh: the time from the mesh, kept apart from the radio so the host tests can run it
// (test/meshclock_core_test.cpp).
//
// The T-Deck has no clock chip. With no GPS fix, no internet and no phone app, MeshCore starts
// the clock at the newest contact change it has saved, which can be minutes or days behind.
// Every advert carries its sender's clock, though, so the nodes around agree on the time even
// when this one doesn't. This keeps the latest advert time from each node heard (when it was
// heard, by the local millisecond counter, so each can be carried forward to "now") and
// answers with the median once at least MIN_NODES distinct nodes agree within AGREE_S of it.
// One node with a wrong clock can't move it; times before 2025 are ignored outright.
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace meshclockcore {

constexpr uint32_t MIN_VALID = 1735689600UL;   // 2025-01-01: older is a clock that was never set
constexpr int MIN_NODES = 3;
constexpr uint32_t AGREE_S = 90;               // within this of the median counts as agreeing
constexpr uint32_t MAX_AGE_MS = 3UL * 3600UL * 1000UL;   // older sightings are dropped
constexpr int SLOTS = 16;

struct Clock {
  struct Seen { uint32_t key, ts, atMs; bool used; };
  Seen s[SLOTS] = {};

  // A node's advert: key (any id for the node, e.g. its public key's first 4 bytes), the time in
  // its advert, and the local millis() when it was heard.
  void heard(uint32_t key, uint32_t ts, uint32_t nowMs) {
    if (ts < MIN_VALID) return;
    int slot = -1, oldest = 0;
    for (int i = 0; i < SLOTS; i++) {
      if (s[i].used && s[i].key == key) { slot = i; break; }
      if (!s[i].used) { if (slot < 0) slot = i; continue; }
      if (s[oldest].used && nowMs - s[i].atMs > nowMs - s[oldest].atMs) oldest = i;
    }
    if (slot < 0) slot = oldest;                 // full: the longest-ago one goes
    s[slot] = {key, ts, nowMs, true};
  }

  // The mesh's time now (seconds since 1970), or 0 if not enough nodes agree. agreeing: how many.
  uint32_t estimate(uint32_t nowMs, int* agreeing = nullptr) const {
    uint32_t v[SLOTS];
    int n = 0;
    for (int i = 0; i < SLOTS; i++) {
      if (!s[i].used || nowMs - s[i].atMs > MAX_AGE_MS) continue;
      v[n++] = s[i].ts + (nowMs - s[i].atMs) / 1000;   // carried forward to now
    }
    if (agreeing) *agreeing = 0;
    if (n < MIN_NODES) return 0;
    for (int i = 1; i < n; i++)                    // insertion sort: at most 16
      for (int j = i; j > 0 && v[j] < v[j - 1]; j--) { const uint32_t t = v[j]; v[j] = v[j - 1]; v[j - 1] = t; }
    const uint32_t med = v[n / 2];
    int k = 0;
    for (int i = 0; i < n; i++) if ((v[i] > med ? v[i] - med : med - v[i]) <= AGREE_S) k++;
    if (agreeing) *agreeing = k;
    return k >= MIN_NODES ? med : 0;
  }
  void clear() { for (auto& x : s) x.used = false; }
};

// Whether to move the clock to the mesh's time: only forward (a clock moved back can make new
// messages look older than ones already sent), and only when it's off by more than slackS.
inline bool shouldSet(uint32_t now, uint32_t mesh, uint32_t slackS = 120) {
  return mesh && mesh > now + slackS;
}

}  // namespace meshclockcore
