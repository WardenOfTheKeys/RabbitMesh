// RabbitMesh holiday themes, shown by themselves on their dates (holiday_core.h): Halloween,
// Thanksgiving, Christmas, New Year's, Easter and the 4th of July. The owner's chosen theme is
// never changed: the holiday's is shown over it while the holiday lasts, and theirs comes back
// after. Picking a theme in the menu during a holiday keeps theirs until that holiday is over.
// Off in the theme menu ("holiday themes by date"). Needs the clock set (GPS, phone or mesh).
#pragma once
#include <Arduino.h>

namespace holiday {
  void tick();                 // from deck::tick: checks the date every 30 s
  int shownId();               // the holiday theme being shown over the owner's, or -1
  bool autoOn();
  void setAuto(bool on);
  void picked(uint8_t themeId);   // the owner chose a theme in the menu
  const char* current();       // today's holiday's name, or ""
}
