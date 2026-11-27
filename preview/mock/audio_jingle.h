#pragma once
#include <stdint.h>
enum : uint8_t { WAVE_SINE = 0, WAVE_SQUARE, WAVE_TRIANGLE, WAVE_CHIME, WAVE_JINGLE };
struct ToneStep {
  uint16_t freq, ms, slideTo;
  constexpr ToneStep(uint16_t f, uint16_t m, uint16_t s = 0) : freq(f), ms(m), slideTo(s) {}
};
struct Jingle {
  const char*     name;
  const ToneStep* steps;
  uint8_t         count;
  uint8_t         wave;
  bool            bell;     // piano/bell envelope: sharp attack, exponential decay
};
