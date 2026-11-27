// RabbitMesh: the "which way is stronger" line of the fox hunt and the flock hunt (fox_core.h
// Gradient), drawn the same in both. The T-Deck has no compass, so the arrow points relative to
// the way you're walking (up = straight ahead), with it in words: "GO AHEAD-RIGHT". Standing
// still it can only name a compass direction, and says to walk a few steps; with no direction
// yet it says exactly why, and what to do about it.
#pragma once
#include <ctype.h>
#include "fox_core.h"

namespace dirdraw {

// Two lines (36 px) at y, across the screen; returns the y under them.
template <class G, class T>
inline int draw(G& g, const T& t, int x, int y, int screenW, const foxcore::Gradient& gr, foxcore::SmoothBearing& sm,
                bool gpsFix, bool haveReadings, uint32_t now, uint8_t satsHeard = 0, uint8_t bestSignal = 0) {
  using namespace foxcore;
  char l1[48], l2[64];
  float b = 0, s10 = 0, crs = 0, cov = 0;
  Gradient::Why why = Gradient::FEW;
  const bool ok = gpsFix && haveReadings && gr.estimate(now, b, s10, &why, &cov);
  if (ok) sm.add(b);
  const bool moving = gr.course(now, crs, 8000, 6.0f);   // the way you're going now: the last ~8 s
  if (ok && moving) {
    const float rel = fmodf(sm.deg() - crs + 360.0f, 360.0f);
    char word[16];
    snprintf(word, sizeof word, "%s", relative(sm.deg(), crs));
    for (char* p = word; *p; p++) *p = (char)toupper((unsigned char)*p);
    snprintf(l1, sizeof l1, "GO %s", word);
    snprintf(l2, sizeof l2, "toward %s, +%.1f dB/10 m, up = ahead", compass8(sm.deg()), s10);
    // the arrow: up is the way you're walking
    const int cx = x + 15, cy = y + 17, r = 14;
    const float a = rel * (float)M_PI / 180.0f;
    const float sx = sinf(a), cyv = -cosf(a);
    g.drawCircle(cx, cy, r + 1, t.line);
    // a bold arrow: a shaft and a wide head, tip on the rim
    const int tx = cx + (int)lroundf(sx * r), ty = cy + (int)lroundf(cyv * r);
    const int bx = cx - (int)lroundf(sx * (r - 3)), by = cy - (int)lroundf(cyv * (r - 3));
    const float px = -cyv, py = sx;                    // across the shaft
    for (int k = -1; k <= 1; k++) g.drawLine(bx + (int)lroundf(px * k), by + (int)lroundf(py * k), tx + (int)lroundf(px * k), ty + (int)lroundf(py * k), t.green);
    const int hx = cx + (int)lroundf(sx * (r - 9)), hy = cy + (int)lroundf(cyv * (r - 9));   // the head's base
    g.fillTriangle(tx, ty, hx + (int)lroundf(px * 7), hy + (int)lroundf(py * 7), hx - (int)lroundf(px * 7), hy - (int)lroundf(py * 7), t.green);
    g.setTextColor(t.green);
    g.drawString(l1, x + 36, y);
    g.setTextColor(t.dim);
    g.drawString(l2, x + 36, y + 17);
    return y + 38;
  }
  if (ok) {
    snprintf(l1, sizeof l1, "stronger toward %s (+%.1f dB / 10 m)", compass8(sm.deg()), s10);
    snprintf(l2, sizeof l2, "walk a few steps: then it points your way");
    g.setTextColor(t.green);
  } else {
    if (!gpsFix) {
      snprintf(l1, sizeof l1, "direction: waiting for a gps fix");
      if (satsHeard) snprintf(l2, sizeof l2, "hearing %u sats, best %u: %s", (unsigned)satsHeard, (unsigned)bestSignal,
                              bestSignal >= 35 ? "should fix soon" : "too weak: open sky helps");
      else snprintf(l2, sizeof l2, "hearing no satellites: go outside");
    }
    else if (!haveReadings || why == Gradient::FEW) { snprintf(l1, sizeof l1, "direction: gathering readings"); snprintf(l2, sizeof l2, "keep moving while it hears the target"); }
    else if (why == Gradient::SHORT) { snprintf(l1, sizeof l1, "direction: walk on, %d of %d m", (int)cov, (int)Gradient::SPREAD_M); snprintf(l2, sizeof l2, "then turn a corner or loop"); }
    else if (why == Gradient::LINE) { snprintf(l1, sizeof l1, "direction: turn a corner"); snprintf(l2, sizeof l2, "a straight walk can't tell left from right"); }
    else { snprintf(l1, sizeof l1, "direction: signal too even here"); snprintf(l2, sizeof l2, "keep moving, or get closer"); }
    g.setTextColor(t.dim);
  }
  g.drawString(l1, x, y);
  g.setTextColor(t.dim);
  g.drawString(l2, x, y + 17);
  (void)screenW;
  return y + 38;
}

}  // namespace dirdraw
