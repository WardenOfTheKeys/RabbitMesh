// RabbitMesh wardrive: a LoRa coverage survey, on the device, no phone needed.
//
// Ported in spirit from WadaMesh's Discover wardriving and its Wardrive app (GPL-3.0,
// github.com/ALLFATHER-BV/wadamesh), rebuilt on Squatch Mesh's own discover and field
// tools. While it runs it probes the repeaters in direct range on a cadence (sooner when
// you move, slower when parked), logs every answer with your GPS position and BOTH link
// directions to /rabbit/wardrive/wd-<date>-<time>.csv on the SD card, logs a "none" row
// for a probe nobody answered (a dead zone), and paints each stop on the map in a neon
// ramp: cyan-green strong, yellow marginal, red weak, a hollow red ring for nothing.
//
// Review: any earlier run, or a Squatch range-test log from /inw/range, can be loaded
// back and shown on the map, coloured by the direction you ask about. The survey pauses
// while you review: nobody chose to spend airtime while reading a map.
//
// It runs from field::tick() and keeps the GPS and the loop awake through
// field::wantsGps(), like the range test, so it carries on with the screen off.
#pragma once
#include <Arduino.h>
#include "wardrive_core.h"

namespace wardrive {
  void tick();                 // from field::tick()
  bool running();              // a survey is on
  bool wantsGps();             // running: keep the GPS on

  void openMenu();             // Tools > wardrive
  // For Drive mode (drive.cpp): start/stop, and what the run found.
  void setRunning(bool on);
  uint32_t stops();            // probes banked this run
  uint32_t answeredStops();    // of those, ones something answered
  uint32_t newRepeaters();     // never seen in any run before
  const char* logPath();       // "" if none

  // What the map draws (mapview.cpp): the live run's stops, or the run under review.
  bool overlayShown();
  const wdcore::Stop* overlay(size_t& n);
  uint8_t overlayDir();        // wdcore::Dir
}
