// RabbitMesh deck: a tile hub for the radio and field tools, and four tools ported from
// WadaMesh's apps (GPL-3.0, github.com/ALLFATHER-BV/wadamesh) as native screens:
//
//   RF Monitor  live RSSI chart over the noise floor, link margin above what the spreading
//               factor still decodes, packets a minute, the latest packets
//   Airtime     channel load (1 min, 10 min, peak), our own share, 10 minutes of bars,
//               the region's duty-cycle limit where there is one
//   Nearby      contacts with a position on a radar scope round you, nearest first,
//               distance and bearing; open one for its details
//   Trip        speed gauge, distance, moving time, average and top speed, altitude and
//               climb, the way back to the start, and a GPX track on the SD card
//
// Batch 2 added the Spectrum analyzer (spectrum.h) and MECP (mecp.h). The hub also opens
// the wardrive, discover, packet sniffer, field tools, GPS and the full Tools list.
// With RABBITMESH the home screen's "t" opens the deck.
//
// deck::tick() runs from field::tick(), so the airtime history and the trip keep going
// with the screen off; a running trip keeps the GPS on through field::wantsGps().
#pragma once
#include <Arduino.h>

namespace deck {
  void tick();
  bool wantsGps();             // a trip is running

  void open();                 // the hub
  void openMonitor();
  void openAirtime();
  void openRadar();
  void openTrip();

  // The packet log: every packet heard or sent, to a CSV on the SD card ("l" on the RF Monitor).
  bool packetLogOn();
  uint32_t packetLogRows();
  void packetLogToggle();

  // The trip, for Drive mode (drive.cpp).
  bool tripOn();
  void tripSet(bool on);       // start a fresh trip / stop it
  double tripDistM();
  uint32_t tripMovingS();
  double tripMaxMps();
  const char* tripGpx();       // the GPX track's path, "" if none
}

// tools.cpp: its own screens, for the hub's tiles.
namespace deckhooks {
  void discover();
  void sniffer();
  void gps();
}
