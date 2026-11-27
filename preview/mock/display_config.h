// Host stand-in for LovyanGFX: just enough of its drawing API to render the lock
// scenes to an image. Not part of the firmware.
#pragma once
#include <stdint.h>
#include <math.h>
#include <algorithm>
#include <vector>
#include <string>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
using std::min; using std::max;
#define SCREEN_W 320
#define SCREEN_H 240
namespace fonts { struct Font { int s; }; static const Font Font0{0}, Font2{2}, Font4{4}, Font6{6}; }
// Text isn't rasterised here: each drawString is recorded, and render.sh draws them with a
// real font over the image (preview only).
struct TextOp { int x, y, font; uint16_t color; std::string text; };
inline std::vector<TextOp>& textOps() { static std::vector<TextOp> v; return v; }
enum textdatum_t { top_left = 0, middle_center };
namespace lgfx {
inline uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)); }
// As LovyanGFX's: RGB565 with its two bytes swapped (how sprite memory holds it).
struct swap565_t { uint16_t raw; constexpr swap565_t(uint16_t r = 0) : raw(r) {} };
struct DrawStats { long calls = 0, pixels = 0; int depth = 0; };
inline DrawStats& stats() { static DrawStats s; return s; }
struct CallGuard { CallGuard() { if (stats().depth++ == 0) stats().calls++; } ~CallGuard() { stats().depth--; } };
class LovyanGFX {
public:
  int W = SCREEN_W, H = SCREEN_H;
  std::vector<uint16_t> px = std::vector<uint16_t>(SCREEN_W * SCREEN_H, 0);
  uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return lgfx::color565(r, g, b); }
  void drawPixel(int x, int y, uint16_t c) { CallGuard g_; stats().pixels++; if (x >= 0 && y >= 0 && x < W && y < H) px[y * W + x] = c; }
  void fillRect(int x, int y, int w, int h, uint16_t c) { CallGuard g_; for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) drawPixel(x + i, y + j, c); }
  void drawFastHLine(int x, int y, int w, uint16_t c) { fillRect(x, y, w, 1, c); }
  void drawFastVLine(int x, int y, int h, uint16_t c) { fillRect(x, y, 1, h, c); }
  void drawRect(int x, int y, int w, int h, uint16_t c) { drawFastHLine(x, y, w, c); drawFastHLine(x, y + h - 1, w, c); drawFastVLine(x, y, h, c); drawFastVLine(x + w - 1, y, h, c); }
  void drawLine(int x0, int y0, int x1, int y1, uint16_t c) { CallGuard g_;
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (;;) { drawPixel(x0, y0, c); if (x0 == x1 && y0 == y1) break; int e2 = 2 * e; if (e2 >= dy) { e += dy; x0 += sx; } if (e2 <= dx) { e += dx; y0 += sy; } }
  }
  void drawWedgeLine(float x0, float y0, float x1, float y1, float r0, float r1, uint16_t c) { CallGuard g_;
    const int bx0 = (int)floorf(min(x0, x1) - max(r0, r1) - 1), bx1 = (int)ceilf(max(x0, x1) + max(r0, r1) + 1);
    const int by0 = (int)floorf(min(y0, y1) - max(r0, r1) - 1), by1 = (int)ceilf(max(y0, y1) + max(r0, r1) + 1);
    const float vx = x1 - x0, vy = y1 - y0, L2 = vx * vx + vy * vy;
    for (int y = by0; y <= by1; y++) for (int x = bx0; x <= bx1; x++) {
      float t = L2 > 0 ? ((x - x0) * vx + (y - y0) * vy) / L2 : 0; t = t < 0 ? 0 : t > 1 ? 1 : t;
      const float qx = x0 + vx * t - x, qy = y0 + vy * t - y, r = r0 + (r1 - r0) * t;
      if (qx * qx + qy * qy <= r * r) drawPixel(x, y, c);
    }
  }
  void drawWideLine(float x0, float y0, float x1, float y1, float r, uint16_t c) { drawWedgeLine(x0, y0, x1, y1, r, r, c); }
  void fillEllipse(int x, int y, int rx, int ry, uint16_t c) { CallGuard g_;
    if (rx < 0) rx = 0; if (ry < 0) ry = 0;
    for (int j = -ry; j <= ry; j++) for (int i = -rx; i <= rx; i++) if ((rx ? (float)i * i / (rx * rx) : 0) + (ry ? (float)j * j / (ry * ry) : 0) <= 1.0f) drawPixel(x + i, y + j, c);
  }
  void drawEllipse(int x, int y, int rx, int ry, uint16_t c) { CallGuard g_;
    for (int a = 0; a < 360; a++) drawPixel(x + (int)lroundf(rx * cosf(a * 0.01745f)), y + (int)lroundf(ry * sinf(a * 0.01745f)), c);
  }
  void fillCircle(int x, int y, int r, uint16_t c) { fillEllipse(x, y, r, r, c); }
  // LovyanGFX: radii r0/r1 (either order), angles in degrees clockwise from 3 o'clock.
  void drawArc(int x, int y, int r0, int r1, float a0, float a1, uint16_t c) { CallGuard g_;
    const int ro = max(r0, r1), ri = min(r0, r1);
    for (int j = -ro; j <= ro; j++) for (int i = -ro; i <= ro; i++) {
      const int d2 = i * i + j * j;
      if (d2 > ro * ro + ro || d2 < ri * ri - ri) continue;
      float a = atan2f((float)j, (float)i) * 57.29578f; if (a < 0) a += 360;
      if ((a0 <= a1) ? (a >= a0 && a <= a1) : (a >= a0 || a <= a1)) drawPixel(x + i, y + j, c);
    } }
  void fillArc(int x, int y, int r0, int r1, float a0, float a1, uint16_t c) { drawArc(x, y, r0, r1, a0, a1, c); }
  void drawCircle(int x, int y, int r, uint16_t c) { drawEllipse(x, y, r, r, c); }
  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) { CallGuard g_;
    const int bx0 = min(x0, min(x1, x2)), bx1 = max(x0, max(x1, x2)), by0 = min(y0, min(y1, y2)), by1 = max(y0, max(y1, y2));
    auto e = [](int ax, int ay, int bx, int by, int px, int py) { return (bx - ax) * (py - ay) - (by - ay) * (px - ax); };
    for (int y = by0; y <= by1; y++) for (int x = bx0; x <= bx1; x++) {
      const int w0 = e(x1, y1, x2, y2, x, y), w1 = e(x2, y2, x0, y0, x, y), w2 = e(x0, y0, x1, y1, x, y);
      if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0)) drawPixel(x, y, c);
    }
  }
  void drawTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) { drawLine(x0, y0, x1, y1, c); drawLine(x1, y1, x2, y2, c); drawLine(x2, y2, x0, y0, c); }
  void fillRoundRect(int x, int y, int w, int h, int r, uint16_t c) { CallGuard g_;
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {
      const int cx = i < r ? r - i : i >= w - r ? i - (w - r - 1) : 0, cy = j < r ? r - j : j >= h - r ? j - (h - r - 1) : 0;
      if (cx * cx + cy * cy <= r * r + r) drawPixel(x + i, y + j, c);
    }
  }
  void drawRoundRect(int x, int y, int w, int h, int r, uint16_t c) {
    drawFastHLine(x + r, y, w - 2 * r, c); drawFastHLine(x + r, y + h - 1, w - 2 * r, c);
    drawFastVLine(x, y + r, h - 2 * r, c); drawFastVLine(x + w - 1, y + r, h - 2 * r, c);
    for (int a = 0; a <= 90; a += 5) { const float q = a * 0.01745f; const int dx = (int)lroundf(r * cosf(q)), dy = (int)lroundf(r * sinf(q));
      drawPixel(x + r - dx, y + r - dy, c); drawPixel(x + w - 1 - r + dx, y + r - dy, c);
      drawPixel(x + r - dx, y + h - 1 - r + dy, c); drawPixel(x + w - 1 - r + dx, y + h - 1 - r + dy, c); } }
  int font = 2; uint16_t fg = 0xFFFF;
  // pushImage(x, y, w, h, data, transparent) for swapped-565 data, as LovyanGFX does it.
  void pushImage(int x, int y, int w, int h, const swap565_t* data, const swap565_t& tr) {
    CallGuard g_;
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {
      const uint16_t r = data[j * w + i].raw;
      if (r != tr.raw) drawPixel(x + i, y + j, (uint16_t)((r >> 8) | (r << 8)));
    } }
  void setFont(const fonts::Font* f) { font = f->s; }
  void setTextColor(uint16_t c, uint16_t = 0) { fg = c; }
  void setTextDatum(textdatum_t) {}
  static int advance(int font) { return font == 0 ? 6 : font == 2 ? 8 : font == 4 ? 14 : 27; }
  int textWidth(const char* s) { return advance(font) * (int)strlen(s); }
  void drawString(const char* s, int x, int y) { textOps().push_back({x, y, font, fg, s}); }
};
}
class LGFX : public lgfx::LovyanGFX {};
#include <string.h>
