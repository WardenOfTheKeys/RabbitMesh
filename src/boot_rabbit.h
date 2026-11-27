// RabbitMesh's boot screen pieces (bootscreen.cpp uses them when RABBITMESH is set): the
// rabbit with his radio up and rings going out of the antenna where Squatch Mesh has its
// mesh mark, and "RABBIT / M E S H" for the wordmark. A header so the previews can draw it.
#pragma once
#include "rabbit.h"

namespace bootrabbit {

constexpr const char* WORD = "RABBIT";

// The rabbit in the mark's box (screen coordinates). animate: the rings pulse.
template <class G>
inline void mark(G& g, const Theme& t, int boxX, int boxY, int boxW, int boxH, uint32_t ms, bool animate) {
  using namespace rabbitsprite;
  rabbit::Shot s = rabbit::walkShot(0, 0);
  s.frame = NTALK > 0 ? NWALK + NTALK - 1 : 0;    // radio up at his mouth
  s.sub = 0; s.bob = 0; s.lean = 0;
  s.walking = NTALK == 0;
  s.mouthOpen = false;
  s.overlays = animate;                           // the still one has no rings (the boot task adds them)
  SquatchPose p;
  p.talking = true;
  const int h = boxH - 8;
  // x is the middle of his feet; the figure sits a little right of that (the radio's in front)
  const int x = boxX + boxW / 2 - (int)((W / 2 - FOOT_X) * (float)h / H) - 6;
  // ~2 rings a second while booting; a still frame (rings at rest) once done
  const float phase = animate ? ms / 500.0f : 0.6f;
  drawRabbitShot(g, t, x, boxY + boxH - 6, h, phase, t.green, p, s);
}

// RabbitMesh's boot mark in plain shapes (the same kind of drawing as the upstream mesh mark it
// replaces, no sprite): a rabbit's head, two tall ears, the visor, and signal arcs rippling out of
// an antenna while it boots. Drawn in the box (screen coordinates).
template <class G>
inline void geoMark(G& g, const Theme& t, int boxX, int boxY, int boxW, int boxH, uint32_t ms, bool animate) {
  (void)boxH;
  const int cx = boxX + boxW / 2, cy = boxY + 74;
  const uint16_t ear = t.greenDim, head = t.panel, edge = t.green;
  g.fillEllipse(cx - 17, cy - 44, 9, 30, ear);                   // ears
  g.fillEllipse(cx + 17, cy - 44, 9, 30, ear);
  g.fillEllipse(cx - 17, cy - 44, 4, 22, mascot::mix(ear, t.white, 0.35f));
  g.fillEllipse(cx + 17, cy - 44, 4, 22, mascot::mix(ear, t.white, 0.35f));
  g.fillCircle(cx, cy, 30, head);                                // head
  g.drawCircle(cx, cy, 30, edge);
  g.fillRoundRect(cx - 22, cy - 8, 44, 12, 6, edge);             // visor
  g.drawFastHLine(cx - 16, cy - 4, 32, t.white);
  g.drawFastVLine(cx + 26, cy - 40, 22, t.dim);                  // antenna
  g.fillCircle(cx + 26, cy - 41, 2, animate ? t.amber : t.dim);
  if (!animate) return;
  for (int k = 0; k < 3; k++) {                                  // arcs rippling out
    const float f = fmodf(ms / 900.0f + k / 3.0f, 1.0f);
    const int r = 5 + (int)(f * 16);
    const uint16_t c = mascot::mix(t.bg, edge, 1.0f - f);
    for (int a = -60; a <= 20; a += 8) {
      const float q = a * 0.01745f;
      g.drawPixel(cx + 26 + (int)(cosf(q) * r), cy - 41 + (int)(sinf(q) * r), c);
    }
  }
}

// Just the rings, centred on (cx, cy), for the boot animation: the rabbit is drawn once on the
// main task (drawLogo), and the animation task redraws only this little box over a copy of
// what was under it. It never touches his image data or frame buffer.
constexpr int RING_W = 18, RING_H = 18, RING_DX = 3, RING_DY = 14;   // the box: (cx - DX, cy - DY)
template <class G>
inline void rings(G& g, const Theme& t, int cx, int cy, uint32_t ms) {
  const float phase = ms / 500.0f;
  for (int k = 0; k < 2; k++) {
    const float f = fmodf(phase * 0.5f + k * 0.5f, 1.0f);
    const int r = 3 + (int)(f * 9);
    const uint16_t c = mascot::mix(t.bg, t.green, 0.8f * (1 - f));
    for (int a = -50; a <= 50; a += 10) {
      const float q = (a - 45) * 0.01745f;
      g.drawPixel(cx + (int)(cosf(q) * r), cy + (int)(sinf(q) * r), c);
    }
  }
}

}  // namespace bootrabbit
