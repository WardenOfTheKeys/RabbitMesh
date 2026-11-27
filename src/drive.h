// RabbitMesh Drive mode: one press for a drive. It starts the trip (with its GPX track), the mesh
// coverage survey (wardrive), the WiFi + Bluetooth wardrive (WiGLE) and the camera-map warnings,
// and stops what it started when you're done, with a summary of the drive: distance and time,
// stops and new repeaters, networks and devices logged, cameras passed, and the files written.
// Anything already running is left as it was; one that can't run alongside another (the flock
// hunt's listening and the WiFi wardrive share the WiFi radio; the range test and the survey share
// the mesh discover) is skipped and the page says so.
#pragma once
#include <Arduino.h>

namespace drive {
  bool on();
  void open();                 // the Drive page: start, live figures, stop
  String tileText();           // for the Deck's tile
}
