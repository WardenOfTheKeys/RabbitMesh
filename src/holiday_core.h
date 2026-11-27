// RabbitMesh holiday themes: which holiday it is, by the local date. Kept apart from the
// clock and the screen so the host tests run it (test/holiday_core_test.cpp).
//
//   Halloween     Oct 15-31
//   Thanksgiving  US Thanksgiving week: the Friday before the 4th Thursday of November to the
//                 Sunday after it
//   Christmas     Dec 1-26
//   New Year's    Dec 31 - Jan 1
//   Easter        the week before Easter Sunday (Western, computed each year) to Easter Monday
//   4th of July   Jul 3-5
// Windows never overlap, so a date is in one holiday at most.
#pragma once
#include <stdint.h>

namespace holidaycore {

enum Holiday : uint8_t { H_NONE = 0, H_HALLOWEEN, H_THANKSGIVING, H_CHRISTMAS, H_NEWYEAR, H_EASTER, H_JULY4, H_COUNT };

inline const char* holidayName(uint8_t h) {
  static const char* N[] = {"", "Halloween", "Thanksgiving", "Christmas", "New Year's", "Easter", "4th of July"};
  return h < H_COUNT ? N[h] : "";
}

inline bool leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
inline int daysInMonth(int y, int m) {
  static const int D[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return m == 2 && leap(y) ? 29 : D[m - 1];
}
// Day of the year, 1-366.
inline int yday(int y, int m, int d) {
  int n = d;
  for (int k = 1; k < m; k++) n += daysInMonth(y, k);
  return n;
}
// 0 = Sunday (Sakamoto's method).
inline int weekday(int y, int m, int d) {
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}
// Western Easter Sunday (the anonymous Gregorian algorithm): month and day.
inline void easter(int y, int& m, int& d) {
  const int a = y % 19, b = y / 100, c = y % 100, dd = b / 4, e = b % 4, f = (b + 8) / 25, g = (b - f + 1) / 3;
  const int h = (19 * a + b - dd - g + 15) % 30, i = c / 4, k = c % 4, l = (32 + 2 * e + 2 * i - h - k) % 7;
  const int mm = (a + 11 * h + 22 * l) / 451;
  m = (h + l - 7 * mm + 114) / 31;
  d = ((h + l - 7 * mm + 114) % 31) + 1;
}
// US Thanksgiving: the 4th Thursday of November.
inline int thanksgiving(int y) {
  const int first = weekday(y, 11, 1);                 // 0 Sunday .. 6 Saturday
  const int firstThu = 1 + (4 - first + 7) % 7;
  return firstThu + 21;
}

// Which holiday a local date falls in.
inline uint8_t holidayOn(int y, int m, int d) {
  if (m == 10 && d >= 15) return H_HALLOWEEN;
  if (m == 11) {
    const int t = thanksgiving(y);
    if (d >= t - 6 && d <= t + 3) return H_THANKSGIVING;
  }
  if (m == 12 && d <= 26) return H_CHRISTMAS;
  if ((m == 12 && d == 31) || (m == 1 && d == 1)) return H_NEWYEAR;
  if (m == 7 && d >= 3 && d <= 5) return H_JULY4;
  int em, ed;
  easter(y, em, ed);
  const int e = yday(y, em, ed), today = yday(y, m, d);
  if (today >= e - 7 && today <= e + 1) return H_EASTER;
  return H_NONE;
}

// One number per holiday per year, so "the owner picked a theme during this one" can be kept
// until it's over (New Year's belongs to the year it starts in).
inline uint32_t seasonKey(int y, int m, uint8_t h) {
  if (h == H_NONE) return 0;
  if (h == H_NEWYEAR && m == 1) y -= 1;
  return (uint32_t)y * 10 + h;
}

}  // namespace holidaycore
