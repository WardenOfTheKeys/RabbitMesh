// RabbitMesh "Burrow Breach": the picture (see bricks_core.h). In the theme's own colours, so the
// Neon Rabbit, Red Team and Blue Team themes each give it their look: a dark grid, neon firewall
// blocks, white ICE that cracks, the rabbit's hoverboard with its ears up, a glowing packet with
// a trail, and falling carrots.
#pragma once
#include <stdio.h>
#include "bricks_core.h"
#include "deck_draw.h"     // mix(), the screen size

namespace bricks {

using deckdraw::mix;

template <class G>
inline void centre(G& g, const char* s, int y, int x0 = 0, int w = FW) { g.drawString(s, x0 + (w - (int)g.textWidth(s)) / 2, y); }

// A carrot, 9 x 13, its tip at the bottom: what a broken block sometimes drops.
template <class G>
inline void carrot(G& g, const Theme& t, int cx, int cy, uint8_t kind) {
  const uint16_t orange = mix(t.amber, t.red, 0.25f);
  g.fillTriangle(cx - 4, cy - 3, cx + 4, cy - 3, cx, cy + 7, orange);
  g.drawFastHLine(cx - 2, cy, 3, mix(orange, t.bg, 0.4f));
  g.drawFastHLine(cx - 1, cy + 3, 2, mix(orange, t.bg, 0.4f));
  g.drawLine(cx, cy - 4, cx - 3, cy - 7, t.green);
  g.drawLine(cx, cy - 4, cx, cy - 8, t.green);
  g.drawLine(cx, cy - 4, cx + 3, cy - 7, t.green);
  static const char* L[] = {"", "W", "M", "S", "+"};
  g.setFont(&fonts::Font0);
  g.setTextColor(t.white);
  g.drawString(L[kind < 5 ? kind : 0], cx + 6, cy - 3);
}

// The rabbit's hoverboard: a neon bar with a glow under it and two ears standing up from it.
template <class G>
inline void board(G& g, const Theme& t, int x0, int y0, float px, float w, float phase) {
  const int x = x0 + (int)lroundf(px - w / 2), y = y0 + PADDLE_Y, iw = (int)lroundf(w);
  g.fillRoundRect(x - 2, y + 2, iw + 4, PADDLE_H + 2, 3, mix(t.bg, t.green, 0.25f));   // the glow under it
  g.fillRoundRect(x, y, iw, PADDLE_H, 2, t.green);
  g.drawFastHLine(x + 2, y + 1, iw - 4, mix(t.green, t.white, 0.6f));
  // the ears, leaning a little with the hover
  const int lean = (int)lroundf(sinf(phase * 3.0f));
  const int ex = x + iw / 2;
  const uint16_t pink = mix(t.red, t.white, 0.45f);
  g.fillEllipse(ex - 7 + lean, y - 7, 3, 7, t.green);
  g.fillEllipse(ex + 7 + lean, y - 7, 3, 7, t.green);
  g.fillEllipse(ex - 7 + lean, y - 7, 1, 5, pink);
  g.fillEllipse(ex + 7 + lean, y - 7, 1, 5, pink);
  // thrusters
  for (int k = 0; k < 3; k++) {
    const int fx = x + 4 + k * (iw - 8) / 2;
    const int fl = 2 + (int)((sinf(phase * 20.0f + k * 2.1f) * 0.5f + 0.5f) * 3);
    g.drawFastVLine(fx, y + PADDLE_H, fl, mix(t.bg, t.blue, 0.8f));
  }
}

// The whole field at (x0, y0): FW x FH. best: the saved best score; sound: on or off.
template <class G>
inline void draw(G& g, const Theme& t, const Game& gm, int x0, int y0, float phase, uint32_t best, bool sound) {
  char s[48];
  // the grid
  const uint16_t grid = mix(t.bg, t.line, 0.35f);
  for (int x = 16; x < FW; x += 32) for (int y = HUD_H + 2; y < FH; y += 4) g.drawPixel(x0 + x, y0 + y, grid);
  for (int y = HUD_H + 18; y < FH; y += 24) for (int x = 0; x < FW; x += 4) g.drawPixel(x0 + x, y0 + y, grid);

  // the top line: score, level, lives (as pairs of ears)
  g.setFont(&fonts::Font0);
  g.setTextColor(t.green);
  snprintf(s, sizeof s, "%06lu", (unsigned long)gm.score);
  g.drawString(s, x0 + 4, y0 + 2);
  g.setTextColor(t.dim);
  snprintf(s, sizeof s, "L%u %s", (unsigned)(gm.level + 1), gm.lvl().name);
  centre(g, s, y0 + 2, x0);
  for (int i = 0; i < gm.lives && i < 5; i++) {                 // a rabbit's head for each life
    const int lx = x0 + FW - 8 - i * 12;
    g.fillEllipse(lx - 2, y0 + 3, 1, 3, t.green);
    g.fillEllipse(lx + 2, y0 + 3, 1, 3, t.green);
    g.fillCircle(lx, y0 + 8, 3, t.green);
    g.drawPixel(lx - 1, y0 + 8, t.bg); g.drawPixel(lx + 1, y0 + 8, t.bg);
  }
  if (gm.lives > 5) { snprintf(s, sizeof s, "+%u", (unsigned)(gm.lives - 5)); g.setTextColor(t.green); g.drawString(s, x0 + FW - 80, y0 + 2); }
  g.drawFastHLine(x0, y0 + HUD_H, FW, mix(t.bg, t.line, 0.7f));

  // the firewall: a colour per row; ICE white, cracked once hit
  const uint16_t rowc[ROWS] = {t.red, t.amber, mix(t.amber, t.green, 0.5f), t.green, t.blue, mix(t.blue, t.red, 0.5f), t.red};
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++) {
      if (!gm.hp[r][c]) continue;
      const int bx = x0 + Game::brickX(c), by = y0 + Game::brickY(r);
      if (gm.ice[r][c]) {
        g.fillRect(bx, by, BW, BH, mix(t.bg, t.white, 0.28f));
        g.drawRect(bx, by, BW, BH, t.white);
        g.drawFastHLine(bx + 2, by + 2, BW - 4, mix(t.white, t.blue, 0.3f));
        if (gm.hp[r][c] == 1) {                      // cracked
          g.drawLine(bx + 8, by, bx + 12, by + 5, t.bg);
          g.drawLine(bx + 12, by + 5, bx + 10, by + BH - 1, t.bg);
          g.drawLine(bx + 12, by + 5, bx + 19, by + 3, t.bg);
        }
      } else {
        const uint16_t c0 = rowc[r];
        g.fillRect(bx, by, BW, BH, mix(t.bg, c0, 0.38f));
        g.drawRect(bx, by, BW, BH, c0);
        g.drawFastHLine(bx + 1, by + 1, BW - 2, mix(c0, t.white, 0.35f));
        g.drawFastVLine(bx + BW / 2, by + 3, BH - 6, mix(t.bg, c0, 0.7f));   // a circuit trace
      }
    }

  for (const Falling& f : gm.drop) if (f.kind) carrot(g, t, x0 + (int)f.x, y0 + (int)f.y, f.kind);
  board(g, t, x0, y0, gm.px, gm.paddleW(), phase);

  // the packets, each with a fading trail
  for (const Ball& b : gm.ball) {
    if (!b.on) continue;
    if (gm.state == PLAY || gm.state == PAUSED)
      for (int i = TRAIL - 1; i >= 0; i--) g.fillCircle(x0 + (int)b.tx[i], y0 + (int)b.ty[i], 2, mix(t.bg, t.blue, 0.6f - i * 0.12f));
    g.fillCircle(x0 + (int)b.x, y0 + (int)b.y, (int)BALL_R + 2, mix(t.bg, t.blue, 0.55f));
    g.fillCircle(x0 + (int)b.x, y0 + (int)b.y, (int)BALL_R, t.white);
  }

  // power-ups running, beside the score (W: wide board, S: slow packet), seconds left
  const bool live = gm.state == PLAY || gm.state == PAUSED || gm.state == SERVE;
  int hx = x0 + 52;
  g.setFont(&fonts::Font0);
  if (live && gm.wideS > 0) { snprintf(s, sizeof s, "W%d", (int)ceilf(gm.wideS)); g.setTextColor(t.green); g.drawString(s, hx, y0 + 2); hx += 24; }
  if (live && gm.slowS > 0) { snprintf(s, sizeof s, "S%d", (int)ceilf(gm.slowS)); g.setTextColor(t.blue); g.drawString(s, hx, y0 + 2); }

  // what's going on, across the middle under the blocks
  const int my = y0 + Game::brickY(ROWS - 1) + BH + 22;
  auto panel = [&](int h) {
    g.fillRoundRect(x0 + 40, my - 6, FW - 80, h, 4, mix(t.bg, t.panel, 0.85f));
    g.drawRoundRect(x0 + 40, my - 6, FW - 80, h, 4, mix(t.bg, t.green, 0.6f));
  };
  if (gm.state == SERVE) {
    panel(42);
    g.setFont(&fonts::Font2); g.setTextColor(t.green);
    snprintf(s, sizeof s, "LEVEL %u: %s", (unsigned)(gm.level + 1), gm.lvl().name);
    centre(g, s, my, x0);
    g.setFont(&fonts::Font0); g.setTextColor(t.dim);
    centre(g, "roll: move   press: launch   p: pause", my + 20, x0);
  } else if (gm.state == PAUSED) {
    panel(42);
    g.setFont(&fonts::Font2); g.setTextColor(t.amber);
    centre(g, "PAUSED", my, x0);
    g.setFont(&fonts::Font0); g.setTextColor(t.dim);
    snprintf(s, sizeof s, "press: go on  s: sound %s  back: quit", sound ? "on" : "off");
    centre(g, s, my + 20, x0);
  } else if (gm.state == CLEARED) {
    panel(42);
    g.setFont(&fonts::Font2); g.setTextColor(t.green);
    centre(g, "FIREWALL BREACHED", my, x0);
    g.setFont(&fonts::Font0); g.setTextColor(t.dim);
    snprintf(s, sizeof s, "+%u   next: %s", 100u * (gm.level + 1), LEVELS[(gm.level + 1) % NLEVELS].name);
    centre(g, s, my + 20, x0);
  } else if (gm.state == OVER) {
    panel(54);
    g.setFont(&fonts::Font2); g.setTextColor(t.red);
    centre(g, "CONNECTION LOST", my, x0);
    g.setFont(&fonts::Font0); g.setTextColor(gm.score >= best && gm.score ? t.green : t.dim);
    snprintf(s, sizeof s, gm.score >= best && gm.score ? "score %lu   NEW BEST" : "score %lu   best %lu", (unsigned long)gm.score, (unsigned long)best);
    centre(g, s, my + 20, x0);
    g.setTextColor(t.dim);
    centre(g, "press: play again   back: quit", my + 33, x0);
  }
}

}  // namespace bricks
