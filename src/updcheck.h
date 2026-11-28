// RabbitMesh: "is there a newer RabbitMesh?", and installing it over WiFi.
//
// Once a week, while the T-Deck is on WiFi, it asks RabbitMesh's own GitHub page for the newest
// release (one small request to api.github.com, nothing about you or the device in it but the
// firmware's name and version in the User-Agent) and, if it's newer than this one, says so once
// with a banner. Settings > System turns that off, and has "check for updates" to ask now.
//
// It installs only when asked (Settings > System > install), only RabbitMesh's own release from
// its own GitHub, and only if the download is signed with RabbitMesh's release key (updcore.h):
// written to the spare app slot, checked, then switched to on a restart. The new version is on
// trial for its first 15 seconds: if it crashes or restarts before then, the old one comes back
// by itself. Once it has proven itself, it copies itself into the first slot, so the USB update
// file (at 0x10000) keeps working however the T-Deck was last updated.
#pragma once
#include <Arduino.h>

namespace updcheck {
  void tick();                 // from deck::tick
  bool enabled();
  void setEnabled(bool on);
  void checkNow();             // Settings > System > check for updates
  String status();             // "up to date (v0.1.0)", "v0.1.1 is out", "not checked yet"
  bool installable();          // a newer release, signed, and somewhere to put it
  String latest();             // its tag, "v0.1.5"
  const char* install();       // downloads, checks and installs it, then restarts; or says why not
}
