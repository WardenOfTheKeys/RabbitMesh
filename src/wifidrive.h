// RabbitMesh WiFi + Bluetooth wardrive: classic wardriving with the T-Deck's own ESP32 radio,
// alongside the mesh. While it runs it scans WiFi access points (every 8 s) and Bluetooth LE
// devices (a 3 s listen every 10 s) and logs each one it sees, with the GPS position, to a
// WiGLE CSV on the SD card (/rabbit/wigle/wigle-<date>-<time>.csv) that uploads to wigle.net
// as it is. It scans with or without a GPS fix (the live page shows what's in range), but
// nothing is logged without one.
//
// A transmitter is logged when first seen, and again only after a minute AND 50 m of travel
// (another vantage point for WiGLE to place it with; parked, it isn't logged over and over).
//
// RabbitMesh never talks to WiGLE: no API calls, no login, no automatic upload. It writes the
// file (WiGLE's published CSV 1.6 format); the owner uploads it at wigle.net if they choose. The live page counts unique networks, open
// ones and BLE devices, and shows the latest few.
//
// The WiFi radio is the firmware's own (netwifi.h, for updates and the clock): if its WiFi is
// off, the wardrive brings the radio up just to scan and turns it off again after; if it's on
// and joining a network, a scan can't start, so it tries again a couple of seconds later. BLE
// scans beside MeshCore's Bluetooth link to the phone app, passively (it only listens), waking
// the Bluetooth stack first if it was parked with Bluetooth off (ble_up.h).
#pragma once
#include <Arduino.h>

namespace wifidrive {
  void tick();                 // from deck::tick
  bool running();
  uint32_t wifiCount();        // unique logged this run, or in range now if more (no fix yet)
  uint32_t bleCount();
  void openMenu();
  // For Drive mode (drive.cpp).
  void setRunning(bool on);
  uint32_t openCount();        // open networks logged
  uint32_t rowsLogged();
  const char* logPath();       // "" if none
}
