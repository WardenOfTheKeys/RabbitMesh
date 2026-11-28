// RabbitMesh deck: how its screens look. Templates over the canvas, like the talk
// bubble in squatch_talk.h, so the T-Deck's SmoothCanvas keeps its soft edges and the
// same code renders on a computer for previews (rabbitmesh/preview). Everything is in
// the theme's colours: in Neon Rabbit that is cyan and magenta on violet-black, and the
// other themes get a deck in their own palette.
//
// Each screen draws the body under the usual header (y >= BODY_Y); deck.cpp feeds them
// a model and handles the keys.
#pragma once
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "theme.h"
#include "deck_core.h"
#include "spectrum_core.h"
#include "mecp_core.h"

namespace deckdraw {

constexpr int W = SCREEN_W, H = SCREEN_H, BODY_Y = 42;

inline uint16_t mix(uint16_t a, uint16_t b, float f) {
  if (f < 0) f = 0; if (f > 1) f = 1;
  const int ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31;
  const int br = b >> 11, bg = (b >> 5) & 63, bb = b & 31;
  return (uint16_t)(((int)(ar + (br - ar) * f) << 11) | ((int)(ag + (bg - ag) * f) << 5) | (int)(ab + (bb - ab) * f));
}
inline uint16_t rgb(uint32_t hex) { return (uint16_t)(((hex >> 8) & 0xF800) | ((hex >> 5) & 0x07E0) | ((hex & 0xFF) >> 3)); }

// ---- shared pieces -------------------------------------------------------------------------
// A HUD panel: dark fill, hairline border, bright corner ticks, and a small caps label.
template <class G>
inline void panel(G& g, const Theme& t, int x, int y, int w, int h, const char* label = nullptr, bool hot = false) {
  g.fillRoundRect(x, y, w, h, 3, t.panel);
  g.drawRoundRect(x, y, w, h, 3, hot ? t.green : t.line);
  const uint16_t c = hot ? t.green : mix(t.line, t.green, 0.5f);
  const int k = 5;
  g.drawFastHLine(x, y, k, c); g.drawFastVLine(x, y, k, c);
  g.drawFastHLine(x + w - k, y + h - 1, k, c); g.drawFastVLine(x + w - 1, y + h - k, k, c);
  if (label) {
    g.setFont(&fonts::Font0);
    g.setTextColor(t.dim, t.panel);
    g.drawString(label, x + 5, y + 4);
  }
}

// A stat cell: label on top, value under it (Font2), optional unit after the value.
template <class G>
inline void stat(G& g, const Theme& t, int x, int y, int w, int h, const char* label, const char* value,
                 uint16_t valColor, const char* unit = nullptr) {
  panel(g, t, x, y, w, h, label);
  g.setFont(&fonts::Font2);
  g.setTextColor(valColor, t.panel);
  g.drawString(value, x + 5, y + h - 19);
  if (unit && *unit) {
    const int vw = g.textWidth(value);
    g.setFont(&fonts::Font0);
    g.setTextColor(t.dim, t.panel);
    g.drawString(unit, x + 7 + vw, y + h - 13);
  }
}

template <class G>
inline void dashedH(G& g, int x, int y, int w, uint16_t c, int on = 3, int off = 3) {
  for (int i = 0; i < w; i += on + off) g.drawFastHLine(x + i, y, (i + on <= w) ? on : w - i, c);
}

// ---- the hub ---------------------------------------------------------------------------------
enum Icon : uint8_t { IC_RF = 0, IC_AIR, IC_RADAR, IC_TRIP, IC_WARDRIVE, IC_DISCOVER, IC_SNIFF, IC_FIELD, IC_MORE, IC_SPECTRUM, IC_MECP, IC_GPS,
                      IC_FOX, IC_WIFI, IC_FLOCK, IC_DRIVE, IC_SD, IC_GAME };

struct Tile {
  const char* title;
  char stat[28];
  uint8_t icon;
  bool live;           // running in the background: a pulsing dot
  char key;            // its shortcut letter on the hub, 0: none
};
// Fills a tile (a helper, not brace assignment: the T-Deck's compiler won't assign a braced
// list to a struct holding a char array).
inline void setTile(Tile& t, const char* title, const char* stat, uint8_t icon, bool live, char key = 0) {
  t.key = key;
  t.title = title;
  strncpy(t.stat, stat, sizeof t.stat - 1);
  t.stat[sizeof t.stat - 1] = 0;
  t.icon = icon;
  t.live = live;
}

// A 20 x 20 icon at (x, y), drawn from primitives.
template <class G>
inline void icon(G& g, const Theme& t, uint8_t ic, int x, int y, uint16_t c, uint16_t c2, float phase) {
  switch (ic) {
    case IC_RF:                                            // a waveform
      for (int i = 0; i < 19; i++) {
        const int y0 = y + 10 + (int)(sinf((i + phase * 6) * 0.7f) * 6 * sinf(i * 0.165f));
        const int y1 = y + 10 + (int)(sinf((i + 1 + phase * 6) * 0.7f) * 6 * sinf((i + 1) * 0.165f));
        g.drawLine(x + i, y0, x + i + 1, y1, c);
      }
      break;
    case IC_AIR:                                           // bars
      for (int i = 0; i < 5; i++) {
        const int h = 4 + (int)((sinf(phase * 2 + i * 1.3f) * 0.5f + 0.5f) * 14);
        g.fillRect(x + i * 4, y + 20 - h, 3, h, i == 2 ? c2 : c);
      }
      break;
    case IC_RADAR: {                                       // a scope with a sweep
      g.drawCircle(x + 10, y + 10, 9, c);
      g.drawCircle(x + 10, y + 10, 4, mix(t.panel, c, 0.5f));
      const float a = phase * 2.5f;
      g.drawLine(x + 10, y + 10, x + 10 + (int)(cosf(a) * 9), y + 10 + (int)(sinf(a) * 9), c2);
      g.fillCircle(x + 15, y + 6, 1, c2);
      break;
    }
    case IC_TRIP: {                                        // a speedometer
      for (int i = 0; i <= 8; i++) {
        const float a = 2.618f + i * 0.524f;               // lower left, over the top, lower right
        g.drawPixel(x + 10 + (int)(cosf(a) * 9), y + 13 + (int)(sinf(a) * 9), c);
      }
      const float a = 2.618f + (sinf(phase) * 0.5f + 0.5f) * 4.19f;
      g.drawLine(x + 10, y + 13, x + 10 + (int)(cosf(a) * 8), y + 13 + (int)(sinf(a) * 8), c2);
      g.fillCircle(x + 10, y + 13, 2, c);
      break;
    }
    case IC_WARDRIVE:                                      // a pin sending out arcs
      g.drawCircle(x + 10, y + 8, 8, mix(t.panel, c2, 0.45f));
      g.drawCircle(x + 10, y + 8, 10, mix(t.panel, c2, 0.25f));
      g.fillCircle(x + 10, y + 8, 4, c);
      g.fillTriangle(x + 7, y + 10, x + 13, y + 10, x + 10, y + 18, c);
      g.fillCircle(x + 10, y + 8, 1, t.panel);
      break;
    case IC_DISCOVER:                                      // arcs out of a mast
      g.drawFastVLine(x + 10, y + 8, 12, c);
      g.fillCircle(x + 10, y + 7, 2, c2);
      for (int r = 5; r <= 9; r += 4) {
        g.drawLine(x + 10 - r, y + 7 - r / 2, x + 10 - r / 2 - 1, y + 7 - r, c);
        g.drawLine(x + 10 + r, y + 7 - r / 2, x + 10 + r / 2 + 1, y + 7 - r, c);
      }
      break;
    case IC_SNIFF:                                         // packets on a wire
      g.drawFastHLine(x, y + 10, 20, mix(t.panel, c, 0.5f));
      for (int i = 0; i < 3; i++) {
        const int px = x + ((int)(phase * 10) + i * 7) % 20;
        g.fillRect(px - 2, y + 7, 5, 7, i == 1 ? c2 : c);
      }
      break;
    case IC_FOX: {                                         // a direction-finding antenna, the signal pulsing
      g.drawLine(x + 3, y + 17, x + 12, y + 4, c);          // the boom
      for (int k = 0; k < 3; k++) g.drawLine(x + 2 + k * 3, y + 10 - k * 3, x + 9 + k * 3, y + 13 - k * 3, c);   // elements
      const int r = 3 + (int)(fmodf(phase * 2, 1.0f) * 6);
      g.drawCircle(x + 15, y + 4, r, c2);
      break;
    }
    case IC_WIFI:                                          // wifi arcs and a bluetooth rune
      g.fillCircle(x + 8, y + 16, 2, c);
      for (int r = 5; r <= 13; r += 4) {
        g.drawLine(x + 8 - r, y + 16 - r, x + 8 - r / 2, y + 16 - r - r / 3, c);
        g.drawLine(x + 8 + r / 2, y + 16 - r - r / 3, x + 8 + r, y + 16 - r, c);
        g.drawLine(x + 8 - r / 2, y + 16 - r - r / 3, x + 8 + r / 2, y + 16 - r - r / 3, c);
      }
      g.drawLine(x + 16, y + 3, x + 19, y + 6, c2); g.drawLine(x + 19, y + 6, x + 16, y + 9, c2);
      g.drawFastVLine(x + 16, y + 2, 8, c2);
      break;
    case IC_FLOCK: {                                       // a camera on a pole, its beam sweeping
      g.drawFastVLine(x + 4, y + 6, 14, c);
      g.fillRect(x + 4, y + 3, 11, 6, c);
      g.fillRect(x + 15, y + 4, 3, 4, c2);
      const int r = 2 + (int)(fmodf(phase * 1.5f, 1.0f) * 3);
      g.drawLine(x + 18, y + 6, x + 18 + r, y + 6 + r, c2);
      break;
    }
    case IC_FIELD:                                         // a flag
      g.drawFastVLine(x + 4, y + 2, 17, c);
      g.fillTriangle(x + 5, y + 2, x + 5, y + 11, x + 17, y + 6, c2);
      break;
    case IC_SPECTRUM:                                      // a trace with a peak, over waterfall rows
      for (int i = 0; i < 20; i++) {
        const int h = 2 + (int)(fabsf(sinf(i * 1.7f + phase)) * 3) + (i == 12 ? 9 : 0);
        g.drawFastVLine(x + i, y + 13 - h, h, i == 12 ? c2 : c);
      }
      for (int r = 0; r < 3; r++) for (int i = 0; i < 20; i += 2)
        g.fillRect(x + i, y + 15 + r * 2, 2, 1, (i >= 10 && i <= 14) ? c2 : mix(t.panel, c, 0.3f + 0.2f * ((i * 7 + r) % 3)));
      break;
    case IC_MECP:                                          // a warning triangle
      g.fillTriangle(x + 10, y + 1, x + 1, y + 18, x + 19, y + 18, (fmodf(phase, 1.2f) < 0.8f) ? c2 : c);
      g.fillTriangle(x + 10, y + 5, x + 4, y + 16, x + 16, y + 16, t.panel);
      g.fillRect(x + 9, y + 8, 2, 5, c);
      g.fillRect(x + 9, y + 14, 2, 2, c);
      break;
    case IC_DRIVE: {                                       // a steering wheel, turning a little
      g.drawCircle(x + 10, y + 10, 9, c);
      g.drawCircle(x + 10, y + 10, 8, c);
      g.fillCircle(x + 10, y + 10, 2, c2);
      const float a = sinf(phase * 2) * 0.35f;
      for (int k = 0; k < 3; k++) {
        const float q = a + (k == 0 ? (float)M_PI / 2 : k == 1 ? (float)M_PI * 7 / 6 : -(float)M_PI / 6);
        g.drawLine(x + 10, y + 10, x + 10 + (int)(cosf(q) * 8), y + 10 + (int)(sinf(q) * 8), c);
      }
      break;
    }
    case IC_SD:                                            // a memory card, its contacts lit
      g.fillRect(x + 4, y + 1, 12, 18, c);
      g.fillTriangle(x + 4, y + 1, x + 4, y + 6, x + 9, y + 1, t.panel);   // the clipped corner
      for (int i = 0; i < 3; i++) g.fillRect(x + 9 + i * 2, y + 2, 1, 4, c2);
      g.fillRect(x + 6, y + 12, 8, 5, mix(t.panel, c, 0.4f));
      break;
    case IC_GAME: {                                        // a wall of blocks, a packet bouncing under it, the board
      for (int r = 0; r < 2; r++) for (int i = 0; i < 4; i++) g.fillRect(x + i * 5, y + 1 + r * 4, 4, 3, r ? c2 : c);
      const float b = fabsf(sinf(phase * 2.2f));
      g.fillCircle(x + 6 + (int)(b * 8), y + 10 + (int)(b * 5), 1, t.white);
      g.fillRect(x + 4, y + 18, 12, 2, c);
      break;
    }
    case IC_GPS:                                           // a satellite
      g.fillRect(x + 7, y + 7, 6, 6, c);
      g.fillRect(x, y + 8, 6, 4, c2); g.fillRect(x + 14, y + 8, 6, 4, c2);
      g.drawLine(x + 10, y + 13, x + 10, y + 17, c);
      g.drawCircle(x + 10, y + 19, 2, mix(t.panel, c, 0.6f));
      break;
    default:                                               // a 3 x 3 grid
      for (int i = 0; i < 9; i++) g.fillRect(x + (i % 3) * 7 + 1, y + (i / 3) * 7 + 1, 5, 5, i == 4 ? c2 : c);
      break;
  }
}

template <class G>
inline void hub(G& g, const Theme& t, const Tile* tiles, int n, int focus, float phase) {
  const int cols = 3, rows = (n + cols - 1) / cols, gap = 4;
  const int tw = (W - gap * (cols + 1)) / cols, th = (H - BODY_Y - gap * (rows + 1)) / rows;
  for (int i = 0; i < n; i++) {
    const int x = gap + (i % cols) * (tw + gap), y = BODY_Y + gap + (i / cols) * (th + gap);
    const bool f = i == focus;
    if (f) g.fillRoundRect(x - 1, y - 1, tw + 2, th + 2, 4, mix(t.bg, t.green, 0.35f));    // glow
    panel(g, t, x, y, tw, th, nullptr, f);
    icon(g, t, tiles[i].icon, x + 6, y + (th < 52 ? 4 : 6), f ? t.green : mix(t.dim, t.green, 0.5f), t.greenDim, phase);
    g.setFont(&fonts::Font2);
    g.setTextColor(f ? t.white : t.txt, t.panel);
    g.drawString(tiles[i].title, x + 30, y + (th < 52 ? 5 : 7));
    g.setFont(&fonts::Font0);
    g.setTextColor(tiles[i].live ? t.green : t.dim, t.panel);
    char stat[28];                                       // cut to fit, clear of the key letter
    const int maxc = (tw - 6 - (tiles[i].key ? 12 : 4)) / 6;
    snprintf(stat, sizeof stat, "%.*s", maxc < 0 ? 0 : maxc, tiles[i].stat);
    g.drawString(stat, x + 6, y + th - 11);
    if (tiles[i].key) {                                  // its shortcut, bottom right
      const char k[2] = {tiles[i].key, 0};
      g.setTextColor(f ? t.green : mix(t.panel, t.dim, 0.8f), t.panel);
      g.drawString(k, x + tw - 9, y + th - 11);
    }
    if (tiles[i].live) {
      const float p = sinf(phase * 4) * 0.5f + 0.5f;
      g.fillCircle(x + tw - 8, y + 8, 3, mix(t.panel, t.greenDim, 0.4f + 0.6f * p));
    }
  }
}

// ---- RF Monitor ------------------------------------------------------------------------------
struct PktRow { char text[48]; bool tx; int8_t snr4; };
struct MonitorModel {
  int rssi = 0, noise = 0;              // dBm; rssi of the last packet heard
  float snr = 0, marginDb = 0;
  int perMin = 0;
  bool heard = false;                   // anything heard yet
  const deckcore::Series<96>* rssiS = nullptr;    // per packet heard
  const deckcore::Series<96>* noiseS = nullptr;   // sampled every 2 s
  PktRow rows[5];
  int nRows = 0;
  int sel = -1;                         // the row picked for its detail, -1: none
  char radio[40] = "";
};

template <class G>
inline void monitor(G& g, const Theme& t, const MonitorModel& m) {
  char b[24];
  const int cw = (W - 5 * 4) / 4, y0 = BODY_Y + 3, ch = 34;
  snprintf(b, sizeof b, m.heard ? "%d" : "--", m.rssi);
  stat(g, t, 4, y0, cw, ch, "RSSI", b, t.green, "dBm");
  snprintf(b, sizeof b, "%d", m.noise);
  stat(g, t, 8 + cw, y0, cw, ch, "NOISE", b, t.greenDim, "dBm");
  snprintf(b, sizeof b, m.heard ? "%+.1f" : "--", m.marginDb);
  stat(g, t, 12 + 2 * cw, y0, cw, ch, "MARGIN", b, !m.heard ? t.dim : m.marginDb >= 5 ? t.green : m.marginDb >= 0 ? t.amber : t.red, "dB");
  snprintf(b, sizeof b, "%d", m.perMin);
  stat(g, t, 16 + 3 * cw, y0, cw, ch, "PKT/MIN", b, t.txt);

  // The chart: RSSI per packet as a filled trace, the noise floor dashed under it.
  const int cx = 4, cy = y0 + ch + 4, cwid = W - 8, chh = H - cy - 66;
  panel(g, t, cx, cy, cwid, chh, "SIGNAL");
  g.setFont(&fonts::Font0);
  g.setTextColor(t.dim, t.panel);
  g.drawString(m.radio, cx + cwid - 6 - g.textWidth(m.radio), cy + 4);
  int16_t lo = -125, hi = -60;
  if (m.rssiS && m.rssiS->n) { int16_t a, z; m.rssiS->range(a, z); if (a < lo) lo = a; if (z > hi) hi = z; }
  if (m.noiseS && m.noiseS->n) { int16_t a, z; m.noiseS->range(a, z); if (a < lo) lo = a; if (z > hi) hi = z; }
  lo -= 5; hi += 5;
  const int px0 = cx + 26, pw = cwid - 32, py0 = cy + 18, ph = chh - 22;
  auto Y = [&](int v) { return py0 + ph - (int)((long)(v - lo) * ph / (hi - lo)); };
  for (int k = 0; k <= 2; k++) {                                     // grid + labels
    const int v = lo + (hi - lo) * k / 2;
    dashedH(g, px0, Y(v), pw, mix(t.panel, t.line, 0.9f), 1, 3);
    snprintf(b, sizeof b, "%d", v);
    g.drawString(b, cx + 4, Y(v) - 4);
  }
  if (m.rssiS && m.rssiS->n) {
    const int n = m.rssiS->n;
    const uint16_t fill = mix(t.panel, t.green, 0.22f);
    int lx = -1, ly = 0;
    for (int i = 0; i < n; i++) {
      const int x = n > 1 ? px0 + i * (pw - 1) / (n - 1) : px0 + pw - 1, y = Y(m.rssiS->at(i));
      if (lx >= 0) {
        for (int xx = lx; xx <= x; xx++) {
          const int yy = ly + (x > lx ? (y - ly) * (xx - lx) / (x - lx) : 0);
          g.drawFastVLine(xx, yy, py0 + ph - yy, fill);
        }
        g.drawLine(lx, ly, x, y, t.green);
      }
      lx = x; ly = y;
    }
    g.fillCircle(lx, ly, 2, t.white);
  } else {
    g.setTextColor(t.dim, t.panel);
    g.drawString("waiting for a packet...", px0 + 8, py0 + ph / 2 - 4);
  }

  if (m.noiseS && m.noiseS->n > 1) {
    const int n = m.noiseS->n;
    for (int i = 0; i < n; i++) {
      const int x = px0 + i * (pw - 1) / (n - 1);
      if ((i / 2) % 2 == 0) g.drawPixel(x, Y(m.noiseS->at(i)), t.greenDim);
      if ((i / 2) % 2 == 0) g.drawPixel(x, Y(m.noiseS->at(i)) + 1, t.greenDim);
    }
  }
  // The latest packets.
  const int ly0 = H - 62;
  panel(g, t, 4, ly0, W - 8, 60, "LATEST");
  g.setFont(&fonts::Font0);
  for (int i = 0; i < m.nRows && i < 4; i++) {
    const PktRow& r = m.rows[i];
    const uint16_t bg = i == m.sel ? mix(t.panel, t.green, 0.28f) : t.panel;
    if (i == m.sel) g.fillRect(6, ly0 + 14 + i * 11, W - 12, 11, bg);
    g.setTextColor(r.tx ? t.greenDim : (r.snr4 >= 0 ? t.green : r.snr4 >= -20 ? t.amber : t.red), bg);
    g.drawString(r.tx ? ">>" : "<<", 9, ly0 + 15 + i * 11);
    g.setTextColor(i == m.sel ? t.white : t.txt, bg);
    g.drawString(r.text, 25, ly0 + 15 + i * 11);
  }
}

// ---- Airtime ---------------------------------------------------------------------------------
struct AirModel {
  const deckcore::Airtime* a = nullptr;
  uint32_t nowS = 0;
  float load1 = 0, load10 = 0, peak = 0, ownTxShare = 0;   // percent; ownTxShare of our time on the air
  float duty = 0;                                           // the region's duty-cycle limit, % (0 none)
  float txPct10 = 0;                                        // our own transmit, % of the last 10 min
  char radio[48] = "";
};

template <class G>
inline void airtime(G& g, const Theme& t, const AirModel& m) {
  char b[32];
  // The headline: load over the last minute, big.
  panel(g, t, 4, BODY_Y + 3, 130, 58, "CHANNEL LOAD  1 MIN");
  g.setFont(&fonts::Font4);
  const uint16_t lc = m.load1 >= 30 ? t.red : m.load1 >= 10 ? t.amber : t.green;
  g.setTextColor(lc, t.panel);
  snprintf(b, sizeof b, m.load1 < 10 ? "%.1f" : "%.0f", m.load1);
  g.drawString(b, 10, BODY_Y + 22);
  const int bw = g.textWidth(b);
  g.setFont(&fonts::Font2);
  g.drawString("%", 12 + bw, BODY_Y + 30);
  const int sx = 138, sw = (W - sx - 4 - 8) / 3;
  snprintf(b, sizeof b, "%.1f", m.load10);
  stat(g, t, sx, BODY_Y + 3, sw, 58, "AVG 10M", b, t.txt, "%");
  snprintf(b, sizeof b, "%.1f", m.peak);
  stat(g, t, sx + sw + 4, BODY_Y + 3, sw, 58, "PEAK", b, t.amber, "%");
  snprintf(b, sizeof b, "%.2f", m.txPct10);
  stat(g, t, sx + 2 * (sw + 4), BODY_Y + 3, sw, 58, "OUR TX", b, t.greenDim, "%");

  // Ten minutes of 10 s bars, heard (accent) under sent (second colour).
  const int cy = BODY_Y + 65, ch = H - cy - 34;
  panel(g, t, 4, cy, W - 8, ch, "LAST 10 MIN");
  float top = m.peak * 1.25f;
  if (m.duty > 0 && m.duty * 1.2f > top) top = m.duty * 1.2f;
  if (top < 2) top = 2;
  const int px0 = 10, pw = W - 20, py0 = cy + 14, ph = ch - 18;
  const int bars = deckcore::Airtime::BARS, bwid = pw / bars;
  if (m.a) {
    for (int k = 0; k < bars; k++) {
      const int back = bars - 1 - k;
      const float rx = m.a->barPct(back, false), tx = m.a->barPct(back, true);
      const int hr = (int)(rx / top * ph), ht = (int)(tx / top * ph);
      const int x = px0 + k * bwid;
      if (hr) g.fillRect(x, py0 + ph - hr, bwid - 1, hr, back == 0 ? t.white : t.green);
      if (ht) g.fillRect(x, py0 + ph - hr - ht, bwid - 1, ht, t.greenDim);
    }
  }
  const int ya = py0 + ph - (int)(m.load10 / top * ph);
  dashedH(g, px0, ya, pw, t.txt, 4, 3);
  g.setFont(&fonts::Font0);
  g.setTextColor(t.dim, t.panel);
  snprintf(b, sizeof b, "scale %.0f%%", top);
  g.drawString(b, W - 12 - g.textWidth(b), cy + 4);
  if (m.duty > 0) {
    const int yd = py0 + ph - (int)(m.duty / top * ph);
    g.drawFastHLine(px0, yd, pw, t.red);
    snprintf(b, sizeof b, "%.0f%% duty limit", m.duty);
    g.setTextColor(t.red, t.panel);
    g.drawString(b, px0 + 2, yd - 10);
  }

  // Who's using it: heard vs sent, and what one packet costs.
  const int by = H - 30;
  const float tot = m.a ? m.a->totalRxMs + m.a->totalTxMs : 0;
  const float rxShare = tot > 0 ? m.a->totalRxMs / tot : 1;
  g.fillRect(4, by, W - 8, 6, t.greenDim);
  g.fillRect(4, by, (int)((W - 8) * rxShare), 6, t.green);
  g.setTextColor(t.txt, t.bg);
  snprintf(b, sizeof b, "heard %.0f%%  sent %.0f%%", rxShare * 100, (1 - rxShare) * 100);
  g.drawString(b, 4, by + 10);
  g.setTextColor(t.dim, t.bg);
  g.drawString(m.radio, W - 4 - g.textWidth(m.radio), by + 10);
}

// ---- Nearby radar ------------------------------------------------------------------------------
struct Blip {
  uint8_t pub[6];
  char name[24];
  uint8_t type;         // ADV_TYPE_*: 1 chat, 2 repeater, 3 room, 4 sensor
  double eastM, northM, dist, brg;
};
struct RadarModel {
  const Blip* b = nullptr;
  int n = 0, sel = 0, first = 0;   // first: the top row of the list
  double rangeM = 2000;
  bool miles = true, haveMe = false;
  float sweep = 0;                  // radians, clockwise from north
};

inline uint16_t blipColor(const Theme& t, uint8_t type) {
  switch (type) { case 2: return t.green; case 3: return t.amber; case 4: return t.greenDim; default: return t.blue; }
}

template <class G>
inline void radar(G& g, const Theme& t, const RadarModel& m) {
  const int R = (H - BODY_Y) / 2 - 5, cx = R + 6, cy = BODY_Y + 3 + R;
  char b[32];
  // The scope.
  g.fillCircle(cx, cy, R, mix(t.bg, t.panel, 0.8f));
  for (int k = 1; k <= 3; k++) g.drawCircle(cx, cy, R * k / 3, k == 3 ? t.line : mix(t.bg, t.line, 0.7f));
  g.drawFastHLine(cx - R, cy, 2 * R, mix(t.bg, t.line, 0.6f));
  g.drawFastVLine(cx, cy - R, 2 * R, mix(t.bg, t.line, 0.6f));
  // The sweep, with a fading wedge behind it.
  for (int k = 0; k < 14; k++) {
    const float a = m.sweep - k * 0.05f;
    const uint16_t c = mix(mix(t.bg, t.panel, 0.8f), t.green, 0.55f * (1 - k / 14.0f));
    g.drawLine(cx, cy, cx + (int)(sinf(a) * R), cy - (int)(cosf(a) * R), c);
  }
  g.setFont(&fonts::Font0);
  g.setTextColor(t.green, t.bg);
  g.drawString("N", cx - 2, cy - R - 1);
  g.setTextColor(t.dim, t.bg);
  for (int k = 1; k <= 3; k++) {
    deckcore::distText(b, sizeof b, m.rangeM * k / 3, m.miles);
    const int d = (int)(R * k / 3 * 0.7071f);
    g.drawString(b, cx + d + 2, cy + d - 2);
  }
  if (!m.haveMe) {
    g.setTextColor(t.amber, t.bg);
    g.drawString("no position yet", cx - 44, cy + 14);
    g.drawString("gps, or settings", cx - 46, cy + 26);
  }
  // Blips: bright as the sweep passes, fading after. Past the edge: a hollow one on the rim.
  for (int i = 0; i < m.n && m.haveMe; i++) {
    const Blip& p = m.b[i];
    double fx = p.eastM / m.rangeM, fy = p.northM / m.rangeM;
    const double r = sqrt(fx * fx + fy * fy);
    const bool out = r > 1;
    if (out) { fx /= r; fy /= r; }
    const int x = cx + (int)(fx * (R - 2)), y = cy - (int)(fy * (R - 2));
    float since = m.sweep - (float)(p.brg * M_PI / 180.0);
    since = fmodf(since, 2 * (float)M_PI); if (since < 0) since += 2 * (float)M_PI;
    const float glow = since < 2.5f ? 1 - since / 2.5f : 0;
    const uint16_t c = mix(mix(t.bg, blipColor(t, p.type), 0.45f), blipColor(t, p.type), glow);
    if (out) g.drawCircle(x, y, 2, c);
    else if (p.type == 2) g.fillTriangle(x, y - 4, x - 3, y + 2, x + 3, y + 2, c);
    else g.fillCircle(x, y, 2, c);
    if (i == m.sel) {
      g.drawCircle(x, y, 6, t.white);
      for (int s = 0; s < 10; s++) {                    // a dotted line from us to it
        const int dx = cx + (x - cx) * s / 10, dy = cy + (y - cy) * s / 10;
        g.drawPixel(dx, dy, t.white);
      }
    }
  }
  g.fillCircle(cx, cy, 3, t.white);

  // The list: nearest first.
  const int lx = cx + R + 8, lw = W - lx - 4;
  deckcore::distText(b, sizeof b, m.rangeM, m.miles);
  char head[40];
  snprintf(head, sizeof head, "NEAREST  %d", m.n);
  panel(g, t, lx, BODY_Y + 3, lw, H - BODY_Y - 7, head);
  g.setFont(&fonts::Font0);
  g.setTextColor(t.dim, t.panel);
  g.drawString(b, lx + lw - 5 - g.textWidth(b), BODY_Y + 7);
  const int rowH = 22, rows = (H - BODY_Y - 24) / rowH;
  for (int k = 0; k < rows && m.first + k < m.n; k++) {
    const int i = m.first + k;
    const Blip& p = m.b[i];
    const int y = BODY_Y + 18 + k * rowH;
    const bool s = i == m.sel;
    if (s) g.fillRect(lx + 2, y - 2, lw - 4, rowH - 1, mix(t.panel, t.green, 0.18f));
    const uint16_t bg = s ? mix(t.panel, t.green, 0.18f) : t.panel;
    g.fillRect(lx + 5, y + 1, 3, 7, blipColor(t, p.type));
    char nm[24];
    const int maxc = (lw - 16) / 6;
    snprintf(nm, sizeof nm, "%.*s", maxc < 23 ? maxc : 23, p.name);
    g.setTextColor(s ? t.white : t.txt, bg);
    g.drawString(nm, lx + 11, y);
    deckcore::distText(b, sizeof b, p.dist, m.miles);
    char line[40];
    snprintf(line, sizeof line, "%s  %3d %s", b, (int)lround(p.brg), deckcore::compass(p.brg));
    g.setTextColor(t.dim, bg);
    g.drawString(line, lx + 11, y + 10);
  }
  if (!m.n) {
    g.setTextColor(t.dim, t.panel);
    g.drawString("no contacts with", lx + 6, BODY_Y + 24);
    g.drawString("a position yet", lx + 6, BODY_Y + 34);
  }
}

// ---- Trip computer --------------------------------------------------------------------------------
struct TripModel {
  const deckcore::Trip* t = nullptr;
  bool miles = true, fix = false, gpx = false;
  uint8_t sats = 0;
  uint32_t nowS = 0;
  char gpxName[40] = "";
  float phase = 0;
};

template <class G>
inline void trip(G& g, const Theme& t, const TripModel& m) {
  const deckcore::Trip& tr = *m.t;
  char b[32], u[12];
  // The speed gauge: a neon arc from 210 deg to -30 deg, full at the trip's own top speed
  // (at least 30 km/h or 20 mph, so a walk doesn't peg it).
  const int gx = 74, gy = BODY_Y + 84, gr = 64;
  const double cur = deckcore::speedUnits(tr.curMps, m.miles), mx = deckcore::speedUnits(tr.maxMps, m.miles);
  double full = mx > (m.miles ? 20 : 30) ? mx * 1.1 : (m.miles ? 20 : 30);
  const float a0 = 2.618f, span = 4.189f;            // from 150 deg (lower left, y down) 240 deg clockwise over the top
  for (int i = 0; i <= 48; i++) {
    const float a = a0 + span * i / 48;
    const float f = (float)i / 48;
    const bool lit = f <= cur / full;
    const uint16_t c = lit ? mix(t.green, t.greenDim, f) : mix(t.bg, t.line, 0.8f);
    g.drawWideLine(gx + cosf(a) * (gr - 6), gy + sinf(a) * (gr - 6), gx + cosf(a) * gr, gy + sinf(a) * gr, 1.4f, c);
  }
  if (mx > 0) {                                        // the max-speed tick
    const float a = a0 + span * (float)(mx / full);
    g.drawLine(gx + (int)(cosf(a) * (gr - 12)), gy + (int)(sinf(a) * (gr - 12)), gx + (int)(cosf(a) * (gr + 2)), gy + (int)(sinf(a) * (gr + 2)), t.amber);
  }
  g.setFont(&fonts::Font6);
  snprintf(b, sizeof b, "%d", (int)lround(cur));
  g.setTextColor(t.white, t.bg);
  g.drawString(b, gx - g.textWidth(b) / 2, gy - 26);
  g.setFont(&fonts::Font0);
  g.setTextColor(t.dim, t.bg);
  const char* sn = deckcore::speedName(m.miles);
  g.drawString(sn, gx - g.textWidth(sn) / 2, gy + 26);
  // Running / stopped, and the GPX log.
  const char* st = tr.on ? "RUNNING" : (tr.have ? "STOPPED" : "READY");
  const uint16_t sc = tr.on ? t.green : t.dim;
  g.fillRoundRect(gx - 34, gy + 42, 68, 14, 7, mix(t.bg, sc, 0.25f));
  g.setTextColor(sc, mix(t.bg, sc, 0.25f));
  g.drawString(st, gx - g.textWidth(st) / 2, gy + 45);
  if (m.gpx) {
    const bool on = fmodf(m.phase, 1.0f) < 0.6f || !tr.on;
    g.fillCircle(gx - 42, gy + 66, 3, on ? t.red : t.bg);
    g.setTextColor(t.red, t.bg);
    g.drawString("GPX", gx - 35, gy + 63);
  }
  g.setTextColor(m.fix ? t.dim : t.amber, t.bg);
  snprintf(b, sizeof b, m.fix ? "%u sats" : "no fix", m.sats);
  g.drawString(b, gx + 46 - g.textWidth(b), gy + 63);

  // The figures, two columns of three.
  const int sx = 152, cw = (W - sx - 4 - 4) / 2, chh = 40;
  auto cell = [&](int i, const char* label, const char* v, uint16_t c, const char* unit) {
    stat(g, t, sx + (i % 2) * (cw + 4), BODY_Y + 3 + (i / 2) * (chh + 4), cw, chh, label, v, c, unit);
  };
  deckcore::distText(b, sizeof b, tr.distM, m.miles);
  { char* sp = strchr(b, ' '); u[0] = 0; if (sp) { strncpy(u, sp + 1, sizeof u - 1); u[sizeof u - 1] = 0; *sp = 0; } }
  cell(0, "DISTANCE", b, t.green, u);
  const uint32_t mv = tr.movingS;
  snprintf(b, sizeof b, mv >= 3600 ? "%lu:%02lu" : "%lu:%02lu", (unsigned long)(mv >= 3600 ? mv / 3600 : mv / 60),
           (unsigned long)(mv >= 3600 ? mv / 60 % 60 : mv % 60));
  cell(1, "MOVING", b, t.txt, mv >= 3600 ? "h:m" : "m:s");
  snprintf(b, sizeof b, "%.1f", deckcore::speedUnits(tr.avgMps(), m.miles));
  cell(2, "AVG", b, t.txt, sn);
  snprintf(b, sizeof b, "%.1f", mx);
  cell(3, "MAX", b, t.amber, sn);
  const double af = m.miles ? 3.28084 : 1.0;
  const char* au = m.miles ? "ft" : "m";
  if (tr.haveAlt) snprintf(b, sizeof b, "%d", (int)lround(tr.alt * af)); else snprintf(b, sizeof b, "--");
  cell(4, "ALTITUDE", b, t.txt, au);
  snprintf(b, sizeof b, "%d", (int)lround(tr.climbM * af));
  cell(5, "CLIMB", b, t.greenDim, au);

  // Back to the start: distance, and an arrow pointing the way (north up).
  const int by = BODY_Y + 3 + 3 * (chh + 4);
  panel(g, t, sx, by, W - sx - 4, H - by - 4, "BACK TO START");
  if (tr.have) {
    const double d = deckcore::metres(tr.lat, tr.lon, tr.lat0, tr.lon0), br = deckcore::bearing(tr.lat, tr.lon, tr.lat0, tr.lon0);
    deckcore::distText(b, sizeof b, d, m.miles);
    char line[40];
    snprintf(line, sizeof line, "%s  %s", b, deckcore::compass(br));
    g.setFont(&fonts::Font2);
    g.setTextColor(t.txt, t.panel);
    g.drawString(line, sx + 30, by + 14);
    const float a = (float)(br * M_PI / 180.0);
    const int ax = sx + 15, ay = by + 22;
    const float dx = sinf(a), dy = -cosf(a);
    g.fillTriangle(ax + (int)(dx * 9), ay + (int)(dy * 9), ax + (int)(-dy * 5 - dx * 6), ay + (int)(dx * 5 - dy * 6),
                   ax + (int)(dy * 5 - dx * 6), ay + (int)(-dx * 5 - dy * 6), t.green);
  } else {
    g.setFont(&fonts::Font0);
    g.setTextColor(t.dim, t.panel);
    g.drawString(tr.on ? "waiting for a fix" : "press to start", sx + 8, by + 18);
  }
}


// ---- Spectrum ----------------------------------------------------------------------------------
struct SpecModel {
  const speccore::Sweep* s = nullptr;
  speccore::Span span;
  float spanMHz = 24, meshMHz = 0;
  int cursor = 80;                  // bin
  bool frozen = false, owns = false;
  float phase = 0;
};

// The waterfall's heat ramp in the theme's colours: background, a deep wash of the
// theme's blue, the second colour, the accent, white.
inline uint16_t heatColor(const Theme& t, float h) {
  const uint16_t st[5] = {mix(t.bg, t.panel, 0.5f), mix(t.bg, t.blue, 0.55f), t.greenDim, t.green, t.white};
  if (h <= 0) return st[0];
  if (h >= 1) return st[4];
  const float x = h * 4;
  const int k = (int)x;
  return mix(st[k], st[k + 1], x - k);
}

template <class G>
inline void spectrum(G& g, const Theme& t, const SpecModel& m) {
  using namespace speccore;
  char b[48];
  const Sweep& s = *m.s;
  // Readout row: centre and span left, floor and the strongest bin right.
  g.setFont(&fonts::Font0);
  g.setTextColor(t.dim, t.bg);
  snprintf(b, sizeof b, "span %.0f MHz  rbw %.1fk", m.spanMHz, RBW_KHZ);
  g.drawString(b, 4, BODY_Y + 3);
  const int pk = s.strongestBin();
  snprintf(b, sizeof b, "floor %d  peak %.3f %d", s.floorDbm, m.span.freq(pk), s.rssi[pk]);
  g.setTextColor(t.txt, t.bg);
  g.drawString(b, W - 4 - g.textWidth(b), BODY_Y + 3);

  // The trace.
  const int px = 26, pw = W - px - 4, ty = BODY_Y + 14, th = 84;
  panel(g, t, 2, ty - 2, W - 4, th + 4, nullptr);
  int lo = s.floorDbm - 6, hi = s.floorDbm + 40;
  for (int i = 0; i < BINS; i++) { if (s.peak[i] + 4 > hi) hi = s.peak[i] + 4; }
  if (hi > -5) hi = -5;
  auto Y = [&](int v) { int y = ty + th - (int)((long)(v - lo) * th / (hi - lo)); return y < ty ? ty : y > ty + th ? ty + th : y; };
  for (int k = 0; k <= 2; k++) {
    const int v = lo + (hi - lo) * k / 2;
    dashedH(g, px, Y(v), pw, mix(t.panel, t.line, 0.9f), 1, 3);
    snprintf(b, sizeof b, "%d", v);
    g.setTextColor(t.dim, t.panel);
    g.drawString(b, 5, Y(v) - (k == 2 ? 0 : 7));
  }
  // the mesh channel
  if (m.meshMHz >= m.span.start && m.meshMHz <= m.span.stop()) {
    const int mx = px + (int)((m.meshMHz - m.span.start) / (m.span.stop() - m.span.start) * (pw - 1));
    for (int y = ty; y < ty + th; y += 3) g.drawPixel(mx, y, t.amber);
    g.setTextColor(t.amber, t.panel);
    g.drawString("MESH", mx + 2, ty + 1);
  }
  int lx = -1, ly = 0;
  for (int x = 0; x < pw; x++) {
    const int pkv = colMax(s.peak, x, pw), v = colMax(s.rssi, x, pw);
    g.drawPixel(px + x, Y(pkv), t.greenDim);
    const int y = Y(v);
    g.drawFastVLine(px + x, y, ty + th - y, mix(t.panel, t.green, 0.18f));
    if (lx >= 0) g.drawLine(lx, ly, px + x, y, t.green);
    lx = px + x; ly = y;
  }
  // the sweep head, while sweeping
  if (!m.frozen && m.owns) g.drawFastVLine(px + s.pos * pw / BINS, ty, th, mix(t.panel, t.white, 0.35f));
  // the cursor
  const int cx = px + m.cursor * pw / BINS + pw / BINS / 2;
  g.drawFastVLine(cx, ty, th, t.white);
  snprintf(b, sizeof b, "%.3f MHz  %d dBm", m.span.freq(m.cursor), s.rssi[m.cursor]);
  const int bw = g.textWidth(b) + 8, bx = cx + 4 + bw > W - 4 ? cx - 4 - bw : cx + 4;
  g.fillRect(bx, ty + 12, bw, 11, t.bg);
  g.drawRect(bx, ty + 12, bw, 11, t.white);
  g.setTextColor(t.white, t.bg);
  g.drawString(b, bx + 4, ty + 14);

  // The waterfall: newest sweep on top, 2 px a row.
  const int wy = ty + th + 5, rows = WF_ROWS, rh = 2;
  for (int r = 0; r < rows; r++) {
    for (int x = 0; x < pw; x += 2) {
      const int b0 = x * BINS / pw, b1 = (x + 2) * BINS / pw;
      int v = -200;
      for (int i = b0; i < b1 && i < BINS; i++) { const int a = s.wfAt(r, i); if (a > v) v = a; }
      g.fillRect(px + x, wy + r * rh, 2, rh, v <= -200 ? t.bg : heatColor(t, heat(v, s.floorDbm)));
    }
  }
  g.drawFastVLine(cx, wy, rows * rh, mix(t.bg, t.white, 0.4f));
  // Frequency labels under it.
  const int fy = wy + rows * rh + 2;
  g.setTextColor(t.dim, t.bg);
  snprintf(b, sizeof b, "%.2f", m.span.start);
  g.drawString(b, px, fy);
  snprintf(b, sizeof b, "%.2f", (m.span.start + m.span.stop()) / 2);
  g.drawString(b, px + pw / 2 - g.textWidth(b) / 2, fy);
  snprintf(b, sizeof b, "%.2f", m.span.stop());
  g.drawString(b, px + pw - g.textWidth(b), fy);
  // Whether the mesh is paused: a light in the left margin (the header says it in words).
  const uint16_t sc = !m.owns ? t.green : m.frozen ? t.amber : (fmodf(m.phase, 1.0f) < 0.6f ? t.red : mix(t.bg, t.red, 0.4f));
  g.fillCircle(12, wy + 8, 4, sc);
  g.setTextColor(t.dim, t.bg);
  g.drawString(!m.owns ? "ON" : m.frozen ? "HLD" : "TX", 4, wy + 16);
  g.drawString(!m.owns ? "" : "OFF", 4, wy + 25);
}

// ---- MECP inbox ----------------------------------------------------------------------------------
struct MecpItem {
  int8_t sev;
  char from[40];          // "Bob in #emergency"
  char words[240];
  char age[12];
  bool hasPos, mine;
};
struct MecpModel {
  const MecpItem* items = nullptr;
  int n = 0, sel = 0, first = 0;
};

inline uint16_t sevColor(const Theme& t, int sev) {
  switch (sev) { case 0: return t.red; case 1: return t.amber; case 2: return t.green; default: return t.dim; }
}

// Word-wraps text into lines of at most cols characters; returns how many it drew.
template <class G>
inline int wrapText(G& g, const char* text, int x, int y, int cols, int maxLines, int lineH) {
  int lines = 0;
  const char* p = text;
  char buf[64];
  while (*p && lines < maxLines) {
    int n = (int)strlen(p);
    if (n > cols) {
      n = cols;
      while (n > 0 && p[n] != ' ') n--;
      if (n == 0) n = cols;
    }
    if (lines == maxLines - 1 && (int)strlen(p) > n) {           // the last line: an ellipsis
      n = cols - 3 < n ? cols - 3 : n;
      snprintf(buf, sizeof buf, "%.*s...", n, p);
    } else snprintf(buf, sizeof buf, "%.*s", n, p);
    g.drawString(buf, x, y + lines * lineH);
    lines++;
    p += n;
    while (*p == ' ') p++;
  }
  return lines;
}

template <class G>
inline void mecpInbox(G& g, const Theme& t, const MecpModel& m) {
  const int cardH = 48, gap = 4, rows = (H - BODY_Y - 4) / (cardH + gap);
  if (!m.n) {
    g.setFont(&fonts::Font2);
    g.setTextColor(t.dim, t.bg);
    g.drawString("no MECP messages in your chats", 12, BODY_Y + 30);
    g.setFont(&fonts::Font0);
    g.drawString("they show here, decoded, as they arrive", 12, BODY_Y + 54);
    g.drawString("press: write one", 12, BODY_Y + 68);
    return;
  }
  for (int k = 0; k < rows && m.first + k < m.n; k++) {
    const int i = m.first + k;
    const MecpItem& it = m.items[i];
    const int y = BODY_Y + 3 + k * (cardH + gap);
    const bool s = i == m.sel;
    const uint16_t sc = sevColor(t, it.sev);
    g.fillRoundRect(4, y, W - 8, cardH, 3, s ? mix(t.panel, sc, 0.14f) : t.panel);
    g.drawRoundRect(4, y, W - 8, cardH, 3, s ? sc : t.line);
    g.fillRect(4, y + 3, 3, cardH - 6, sc);
    // severity pill, who and when
    const char* sv = it.sev >= 0 && it.sev < mecpcore::NSEV ? mecpcore::SEV[it.sev] : "MECP";
    g.setFont(&fonts::Font0);
    const int pw = g.textWidth(sv) + 8;
    g.fillRoundRect(11, y + 4, pw, 11, 5, sc);
    g.setTextColor(t.bg, sc);
    g.drawString(sv, 15, y + 6);
    g.setTextColor(t.txt, s ? mix(t.panel, sc, 0.14f) : t.panel);
    g.drawString(it.from, 15 + pw, y + 6);
    g.setTextColor(t.dim, s ? mix(t.panel, sc, 0.14f) : t.panel);
    char right[24];
    snprintf(right, sizeof right, "%s%s", it.hasPos ? "MAP  " : "", it.age);
    g.drawString(right, W - 10 - g.textWidth(right), y + 6);
    // the words, after the "SEVERITY: " the pill already says
    const char* w = strchr(it.words, ':');
    w = w ? w + 1 : it.words;
    while (*w == ' ') w++;
    g.setTextColor(it.sev <= 1 ? t.white : t.txt, s ? mix(t.panel, sc, 0.14f) : t.panel);
    wrapText(g, w, 12, y + 19, (W - 24) / 6, 3, 9);
  }
}

}  // namespace deckdraw
