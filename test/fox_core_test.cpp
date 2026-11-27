// Host tests for the fox hunt's meter (src/fox_core.h).
#include <cstdio>
#include <cmath>
#include <cstring>
#include "fox_core.h"
using namespace foxcore;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main() {
  CHECK(strength(-200) == 0 && strength(0) == 1);
  CHECK(std::fabs(strength(-85) - 0.5f) < 1e-6);
  // WiFi's own range: -95 (floor) .. -30 (beside it). -70 dBm is strong for LoRa, middling for WiFi
  CHECK(strength(-95, WIFI_LO, WIFI_HI) == 0 && strength(-30, WIFI_LO, WIFI_HI) == 1);
  CHECK(std::fabs(strength(-62.5f, WIFI_LO, WIFI_HI) - 0.5f) < 1e-6);
  CHECK(strength(-70, WIFI_LO, WIFI_HI) < strength(-70));

  Meter m;
  CHECK(!m.fresh(0, 1000));
  m.feed(-100, 1000);
  CHECK(m.have && m.avg == -100 && m.peak == -100 && m.trend == 0 && m.n == 1);
  CHECK(m.fresh(1500, 1000) && !m.fresh(2500, 1000));
  m.feed(-80, 2000);                                   // 20 dB stronger: avg moves 8 dB up
  CHECK(std::fabs(m.avg - (-92)) < 1e-4 && m.trend == 1 && m.peak == -80 && m.last == -80);
  m.feed(-92, 3000);                                   // right on the average: steady
  CHECK(m.trend == 0);
  m.feed(-120, 4000);                                  // falling away
  CHECK(m.trend == -1 && m.peak == -80);
  m.resetPeak();
  CHECK(m.peak == m.avg);
  m.clear();
  CHECK(!m.have && m.n == 0 && m.peak == -200);

  CHECK(clickGapMs(0) == 1200 && clickGapMs(1) == 70);
  CHECK(clickGapMs(0.9f) < clickGapMs(0.1f));
  CHECK(clickHz(0) == 600 && clickHz(1) == 2400);

  {
    // Direction: a transmitter at (200 m east, 200 m north) = bearing 45; signal falls 0.3 dB/m
    const double LA = 36.85, LO = -76.29, k = 111320.0;
    auto at = [&](double e, double n, double& la, double& lo) { la = LA + n / k; lo = LO + e / (k * cos(LA * M_PI / 180.0)); };
    auto rssiAt = [](double e, double n) { return (float)(-60.0 - 0.3 * sqrt((e - 200) * (e - 200) + (n - 200) * (n - 200))); };
    Gradient g;
    float b = 0, s10 = 0;
    double la, lo;
    for (int i = 0; i < 5; i++) { at(i * 2.0, 0, la, lo); g.add(la, lo, rssiAt(i * 2.0, 0), i * 1000); }
    CHECK(!g.estimate(5000, b, s10));                  // too few readings, too little ground
    Gradient w;                                        // walk an L: 30 m east, then 30 m north
    for (int i = 0; i <= 10; i++) { at(i * 3.0, 0, la, lo); w.add(la, lo, rssiAt(i * 3.0, 0), i * 1000); }
    for (int i = 1; i <= 10; i++) { at(30, i * 3.0, la, lo); w.add(la, lo, rssiAt(30, i * 3.0), (10 + i) * 1000); }
    CHECK(w.estimate(21000, b, s10));
    CHECK(fabsf(b - 45.0f) < 12.0f);
    CHECK(s10 > 1.5f && s10 < 4.5f);
    float crs = 0;
    CHECK(w.course(21000, crs) && fabsf(crs - 45.0f) < 10.0f);              // the last 20 s: east, then north
    CHECK(w.course(21000, crs, 8000) && (crs < 5.0f || crs > 355.0f));      // the last 8 s: north
    CHECK(!strcmp(relative(45, 0), "ahead-right"));
    CHECK(!strcmp(relative(180, 0), "behind"));
    CHECK(!strcmp(relative(10, 350), "ahead"));
    CHECK(!strcmp(relative(270, 0), "left"));
    CHECK(!strcmp(compass8(44), "NE") && !strcmp(compass8(359), "N"));
    Gradient line;                                     // a straight line can't tell left from right
    for (int i = 0; i <= 20; i++) { at(i * 3.0, 0, la, lo); line.add(la, lo, rssiAt(i * 3.0, 0), i * 1000); }
    CHECK(!line.estimate(20000, b, s10));
    Gradient flat;                                     // no slope: no direction
    for (int i = 0; i <= 10; i++) { at(i * 3.0, 0, la, lo); flat.add(la, lo, -80, i * 1000); }
    for (int i = 1; i <= 10; i++) { at(30, i * 3.0, la, lo); flat.add(la, lo, -80, (10 + i) * 1000); }
    CHECK(!flat.estimate(21000, b, s10));
    CHECK(!w.estimate(21000 + Gradient::WINDOW_MS + 30000, b, s10));   // old readings drop out
  }
  {
    // Why: each reason, and a nearly straight walk (1 m off a line) isn't taken as a direction
    const double LA = 36.85, LO = -76.29, k = 111320.0;
    auto at = [&](double e, double n, double& la, double& lo) { la = LA + n / k; lo = LO + e / (k * cos(LA * M_PI / 180.0)); };
    auto rssiAt = [](double e, double n) { return (float)(-60.0 - 0.3 * sqrt((e - 200) * (e - 200) + (n - 200) * (n - 200))); };
    double la, lo;
    float b = 0, s10 = 0, cov = 0;
    Gradient::Why why;
    Gradient few;
    for (int i = 0; i < 3; i++) { at(i * 5.0, 0, la, lo); few.add(la, lo, rssiAt(i * 5.0, 0), i * 1000); }
    CHECK(!few.estimate(3000, b, s10, &why) && why == Gradient::FEW);
    Gradient shortw;
    for (int i = 0; i < 8; i++) { at(i * 1.0, 0, la, lo); shortw.add(la, lo, rssiAt(i * 1.0, 0), i * 1000); }
    CHECK(!shortw.estimate(8000, b, s10, &why, &cov) && why == Gradient::SHORT && cov > 5 && cov < 9);
    Gradient wobble;                                   // 60 m along, never more than 1 m off the line
    for (int i = 0; i <= 20; i++) { at(i * 3.0, (i % 2) ? 1.0 : 0.0, la, lo); wobble.add(la, lo, rssiAt(i * 3.0, (i % 2) ? 1.0 : 0.0), i * 1000); }
    CHECK(!wobble.estimate(20000, b, s10, &why) && why == Gradient::LINE);
    Gradient flat2;
    for (int i = 0; i <= 10; i++) { at(i * 3.0, 0, la, lo); flat2.add(la, lo, -80, i * 1000); }
    for (int i = 1; i <= 10; i++) { at(30, i * 3.0, la, lo); flat2.add(la, lo, -80, (10 + i) * 1000); }
    CHECK(!flat2.estimate(21000, b, s10, &why) && why == Gradient::FLAT);
    Gradient ell;
    for (int i = 0; i <= 10; i++) { at(i * 3.0, 0, la, lo); ell.add(la, lo, rssiAt(i * 3.0, 0), i * 1000); }
    for (int i = 1; i <= 10; i++) { at(30, i * 3.0, la, lo); ell.add(la, lo, rssiAt(30, i * 3.0), (10 + i) * 1000); }
    CHECK(ell.estimate(21000, b, s10, &why) && why == Gradient::OK && fabsf(b - 45.0f) < 12.0f);
    // smoothing: across north, and toward a steady value
    SmoothBearing sb;
    sb.add(350); sb.add(10);
    CHECK(sb.deg() > 345.0f || sb.deg() < 5.0f);
    SmoothBearing sb2;
    for (int i = 0; i < 20; i++) sb2.add(90);
    CHECK(fabsf(sb2.deg() - 90.0f) < 0.5f);
  }
  printf(fails ? "%d FAILED\n" : "all fox_core tests passed\n", fails);
  return fails != 0;
}
