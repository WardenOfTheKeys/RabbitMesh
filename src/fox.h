// RabbitMesh fox hunt: find a transmitter by walking toward it.
//
//   Any signal  passive: the strength of every packet the radio hears. The mesh carries on
//               as normal. Good for finding whatever is busy on the channel.
//   Repeater    pick one repeater (or room or sensor) and the T-Deck pings it zero-hop every
//               8 s; its answer's strength drives the meter. Finds a particular node.
//   Bluetooth   the same for a Bluetooth device, by its address (passive: it only listens).
//               Devices on a fixed address are listed first; a phone's random address changes
//               every few minutes, and then it can't be followed by it.
//   WiFi        press to list the WiFi networks in range (strongest first), pick one with < >;
//               the T-Deck then scans just that access point (by its address, on its channel)
//               about once a second and its strength drives the meter, on a WiFi scale
//               (-95 to -30 dBm). Finds a router, a hotspot, a camera's WiFi.
//
// Any signal and Repeater hear LoRa only (the mesh radio); WiFi uses the ESP32's own radio,
// borrowing it like the WiFi + BT wardrive does. A running wardrive pauses while you follow
// something and carries on after; the flock hunt's listening has to be stopped first.
//
// A big meter of the smoothed signal with the peak marked, "stronger / weaker / steady", and a
// Geiger-counter click that speeds up and rises in pitch as you close in (`s` turns it off).
// The logic is in fox_core.h.
#pragma once

#include <stdint.h>

namespace fox {
  void open();
  // Straight to one WiFi network (ble false: its BSSID and channel) or Bluetooth device (its
  // address), from the WiFi + BT wardrive's "follow one".
  void follow(bool ble, const uint8_t mac[6], uint8_t ch, bool randomAddr, const char* name);
}
