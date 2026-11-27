// RabbitMesh: "is there a newer RabbitMesh?" Once a week, while the T-Deck is on WiFi, it asks
// RabbitMesh's own GitHub page for the newest release's version number (one small request to
// api.github.com, nothing about you or the device in it but the firmware's name and version in
// the User-Agent) and, if it's newer than this one, says so once with a banner. It never
// downloads or installs anything: updating is still flashing the update file. Settings >
// System turns it off, and has "check for updates" to ask now.
#pragma once
#include <Arduino.h>

namespace updcheck {
  void tick();                 // from deck::tick
  bool enabled();
  void setEnabled(bool on);
  void checkNow();             // Settings > System > check for updates
  String status();             // "up to date (v0.1.0)", "v0.1.1 is out", "not checked yet"
}
