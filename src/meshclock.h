// RabbitMesh: the clock from the mesh (meshclock_core.h). node.cpp hands it every advert heard;
// deck::tick checks once every 30 s whether at least three nodes agree on a time the clock is
// more than two minutes behind, and if so sets it (forward only). The GPS and the internet
// still win: nothing happens while the GPS has a fix (and sets the clock) or WiFi is connected
// (and syncs it). The phone app's time agrees with the mesh, so it's left alone too.
#pragma once
#include <Arduino.h>

namespace meshclock {
  void heard(const uint8_t* pubKey, uint32_t advertTime);   // from node.cpp, every advert
  void tick();                                             // from deck::tick
}
