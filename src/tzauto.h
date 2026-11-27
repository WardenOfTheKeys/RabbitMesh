// RabbitMesh: the time zone follows the GPS (tz_core.h), in the US, Canada and Mexico. Once a
// minute it works out the zone from the GPS's fix, or with none (indoors, before the first fix)
// from the node's saved position (the last fix, or one set by hand or from the phone app), and, if it's changed, switches the clock's zone
// (daylight saving comes with it: each zone carries its rule). On by default; picking a zone
// by hand in Settings turns it off, so a choice sticks. Outside North America it does nothing.
#pragma once
#include <Arduino.h>

namespace tzauto {
  void tick();                 // from deck::tick
  bool autoOn();
  void setAuto(bool on);
  void manualPick();           // the owner chose a zone in the menu
}
