// RabbitMesh SD card viewer: how full the card is, what each tool has written there (files and
// size per folder), the files newest first, a look at the start of one, and deleting a file or a
// whole folder's logs (each asks first). Everything runs on the main loop: the card shares its
// bus with the screen and the radio.
#pragma once
#include <Arduino.h>

namespace sdview {
  void open();
  String tileText();           // for the Deck's tile: "1.2 of 29 GB"
}
