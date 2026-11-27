// RabbitMesh Flock hunt: find Flock Safety ALPR cameras two ways and check one against the other.
//
//   The map     where the community has mapped ALPRs (OpenStreetMap, via the deflock-data
//               project's daily file), on the SD card. "Update camera map" downloads it over
//               Squatch's WiFi at most once a day and keeps the cameras within a radius of you;
//               tools/alpr_extract.py builds the same file on a computer for a whole state or
//               country. With a map, the T-Deck warns as you come within 300 m of a camera.
//   The radio   listening (promiscuous, receive-only) for what a camera sends: its WiFi probe
//               requests and, as a weak extra, its battery pack's Bluetooth. Signatures and
//               confidence tiers from the flock-you project (flock_core.h).
//   Both        a camera heard within 150 m of a mapped one is CONFIRMED; one heard with
//               nothing mapped near is "not on the map": worth adding to OpenStreetMap.
//
// While listening, the WiFi radio hops channels 11/6/1, so Squatch's WiFi must be off and the
// WiFi + BT wardrive stopped. The strongest camera heard drives a Geiger click (fox_core.h) to
// walk up to it. Detections go to /rabbit/flock/flock-<date>-<time>.csv.
//
// Nothing here transmits at a camera; the only thing ever sent is the one map download.
#pragma once
#include <Arduino.h>

namespace flockhunt {
  void tick();                 // from deck::tick
  bool wantsGps();             // listening, or approach alerts on with a map loaded
  bool listening();
  uint32_t heardCount();       // cameras heard this session
  uint32_t mappedCount();      // cameras in the file on the card
  void openMenu();
  // For Drive mode (drive.cpp): the approach warnings on, and how many it gave.
  bool alertsOn();
  void setAlerts(bool on);
  uint32_t alertsGiven();
}
