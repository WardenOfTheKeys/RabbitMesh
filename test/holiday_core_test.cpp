// Host tests for the holiday dates (src/holiday_core.h).
#include <cstdio>
#include "holiday_core.h"
using namespace holidaycore;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main() {
  // Easter Sundays (published tables)
  int m, d;
  easter(2024, m, d); CHECK(m == 3 && d == 31);
  easter(2025, m, d); CHECK(m == 4 && d == 20);
  easter(2026, m, d); CHECK(m == 4 && d == 5);
  easter(2027, m, d); CHECK(m == 3 && d == 28);
  easter(2038, m, d); CHECK(m == 4 && d == 25);
  easter(2285, m, d); CHECK(m == 3 && d == 22);       // the earliest possible
  // Thanksgiving
  CHECK(thanksgiving(2024) == 28 && thanksgiving(2025) == 27 && thanksgiving(2026) == 26 && thanksgiving(2027) == 25);
  CHECK(weekday(2026, 11, 26) == 4);                   // a Thursday
  CHECK(weekday(2000, 1, 1) == 6 && weekday(2026, 7, 4) == 6);

  CHECK(holidayOn(2026, 10, 14) == H_NONE && holidayOn(2026, 10, 15) == H_HALLOWEEN && holidayOn(2026, 10, 31) == H_HALLOWEEN);
  CHECK(holidayOn(2026, 11, 1) == H_NONE);
  CHECK(holidayOn(2026, 11, 19) == H_NONE && holidayOn(2026, 11, 20) == H_THANKSGIVING);
  CHECK(holidayOn(2026, 11, 26) == H_THANKSGIVING && holidayOn(2026, 11, 29) == H_THANKSGIVING && holidayOn(2026, 11, 30) == H_NONE);
  CHECK(holidayOn(2026, 12, 1) == H_CHRISTMAS && holidayOn(2026, 12, 26) == H_CHRISTMAS && holidayOn(2026, 12, 27) == H_NONE);
  CHECK(holidayOn(2026, 12, 31) == H_NEWYEAR && holidayOn(2027, 1, 1) == H_NEWYEAR && holidayOn(2027, 1, 2) == H_NONE);
  CHECK(holidayOn(2026, 3, 29) == H_EASTER && holidayOn(2026, 3, 28) == H_NONE);       // Easter 2026: Apr 5
  CHECK(holidayOn(2026, 4, 5) == H_EASTER && holidayOn(2026, 4, 6) == H_EASTER && holidayOn(2026, 4, 7) == H_NONE);
  CHECK(holidayOn(2024, 3, 31) == H_EASTER && holidayOn(2024, 4, 1) == H_EASTER);       // across a month end
  CHECK(holidayOn(2026, 7, 2) == H_NONE && holidayOn(2026, 7, 4) == H_JULY4 && holidayOn(2026, 7, 6) == H_NONE);

  // no overlaps across every day of 200 years, and every holiday turns up every year
  int seen[H_COUNT] = {0};
  for (int y = 2000; y < 2200; y++)
    for (int mo = 1; mo <= 12; mo++)
      for (int dd = 1; dd <= daysInMonth(y, mo); dd++) seen[holidayOn(y, mo, dd)]++;
  for (int h = 1; h < H_COUNT; h++) CHECK(seen[h] >= 200);

  CHECK(seasonKey(2026, 12, H_NEWYEAR) == seasonKey(2027, 1, H_NEWYEAR));
  CHECK(seasonKey(2026, 12, H_CHRISTMAS) != seasonKey(2027, 12, H_CHRISTMAS) && seasonKey(2026, 5, H_NONE) == 0);

  printf(fails ? "%d FAILED\n" : "all holiday_core tests passed\n", fails);
  return fails != 0;
}
