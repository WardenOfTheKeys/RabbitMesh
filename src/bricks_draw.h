// RabbitMesh "Burrow Breaker": the picture (see bricks_core.h), made from the concept art
// (art/game/burrow_breaker_concept.png, turned into bricks_art.h by tools/make_game_art.py):
// the framed screen with the hooded rabbit and the city, server-rack blocks, steel plates that
// crack, the bunny-eared hoverboard, carrots with their trail, the packet with its trail, and
// magenta sparks where a block breaks. The whole screen, title bar included.
#pragma once
#include <stdio.h>
#include "bricks_core.h"
#include "bricks_art.h"

namespace bricks {

namespace A = bricksart;

inline uint16_t mix565(uint16_t a, uint16_t b, float f) {
  if (f <= 0) return a;
  if (f >= 1) return b;
  const int r = ((a >> 11) & 31) + (int)((((b >> 11) & 31) - ((a >> 11) & 31)) * f);
  const int g = ((a >> 5) & 63) + (int)((((b >> 5) & 63) - ((a >> 5) & 63)) * f);
  const int bl = (a & 31) + (int)(((b & 31) - (a & 31)) * f);
  return (uint16_t)((r << 11) | (g << 5) | bl);
}
constexpr uint16_t C_WHITE = 0xFFFF, C_LAVENDER = 0xAC9A, C_PANEL = 0x1045, C_PURPLE = 0x6150;

// The sparks: a handful of magenta squares thrown from each block that breaks. Kept by the
// screen (it's only looks), fed from Game::burst.
struct Sparks {
  struct P { float x, y, vx, vy, life; };
  P p[48];
  uint32_t seed = 0x9E3779B9u;
  Sparks() { for (auto& q : p) q.life = 0; }
  float rnd() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return (seed % 1000) / 1000.0f; }
  void add(const Game& g) {
    for (int i = 0; i < g.nburst; i++) {
      int made = 0;
      for (auto& q : p) {
        if (q.life > 0) continue;
        q.x = g.burst[i].x + (rnd() - 0.5f) * BW * 0.8f; q.y = g.burst[i].y + (rnd() - 0.5f) * BH;
        q.vx = (rnd() - 0.5f) * 90; q.vy = (rnd() - 0.7f) * 70; q.life = 0.35f + rnd() * 0.35f;
        if (++made == 8) break;
      }
    }
  }
  void step(float dt) {
    for (auto& q : p) if (q.life > 0) { q.x += q.vx * dt; q.y += q.vy * dt; q.vy += 120 * dt; q.life -= dt; }
  }
};

// The background, unpacked (W x H, byte-swapped): once, into memory the caller gives.
inline void unpackBackground(uint16_t* out) {
  int n = 0;
  for (int i = 0; i < A::BG_NRUNS; i++)
    for (int k = 0; k < A::BG_RUNS[i * 2]; k++) if (n < A::W * A::H) out[n++] = A::BG_RUNS[i * 2 + 1];
}

// The score's digits, 5 x 7, in the art's cyan.
static const uint8_t DIGITS[10][7] = {
  {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
  {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E},
  {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
  {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
  {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},
};
template <class G>
inline void digit(G& g, int n, int x, int y) {
  for (int r = 0; r < 7; r++)
    for (int c = 0; c < 5; c++)
      if (DIGITS[n][r] & (0x10 >> c)) g.drawPixel(x + c, y + r, A::C_CYAN);
}
template <class G>
inline void score(G& g, uint32_t v, int x, int y) {
  char s[8];
  snprintf(s, sizeof s, "%06lu", (unsigned long)(v % 1000000));
  for (int i = 0; i < 6; i++) digit(g, s[i] - '0', x + (int)lroundf(i * 6.4f), y);
}

// The board, its middle stretched for the wide one (the ears stay where they are on it).
template <class G>
inline void board(G& g, float px, float w) {
  const int extra = (int)lroundf(w) - PADDLE_W;
  const int x = (int)lroundf(px - w / 2) - A::BOARD_BAR_X, y = PADDLE_Y - A::BOARD_BAR_Y;
  if (extra <= 0) { g.pushImage(x, y, A::BOARD_W, A::BOARD_H, (const lgfx::swap565_t*)A::BOARD, lgfx::swap565_t(A::KEY)); return; }
  constexpr int MAXW = A::BOARD_W + (PADDLE_WIDE - PADDLE_W);
  static uint16_t wide[A::BOARD_H * MAXW];
  const int ex = extra > PADDLE_WIDE - PADDLE_W ? PADDLE_WIDE - PADDLE_W : extra;
  const int ww = A::BOARD_W + ex, mid = A::BOARD_W / 2 - 1;     // the column repeated: bar only
  for (int r = 0; r < A::BOARD_H; r++)
    for (int c = 0; c < ww; c++) {
      const int sc = c <= mid ? c : c <= mid + ex ? mid : c - ex;
      wide[r * ww + c] = A::BOARD[r * A::BOARD_W + sc];
    }
  g.pushImage(x, y, ww, A::BOARD_H, (const lgfx::swap565_t*)wide, lgfx::swap565_t(A::KEY));
}

template <class G>
inline void panel(G& g, int x, int y, int w, int h) {
  g.fillRect(x, y, w, h, C_PANEL);
  g.drawRect(x, y, w, h, A::C_MAGENTA);
  g.drawRect(x + 2, y + 2, w - 4, h - 4, C_PURPLE);
}
template <class G>
inline void centre(G& g, const char* s, int y) { g.drawString(s, (FW - (int)g.textWidth(s)) / 2, y); }

// The whole screen. bg: the unpacked background (unpackBackground), or null to draw on what's there.
template <class G>
inline void draw(G& g, const Game& gm, const Sparks& sp, const uint16_t* bg, uint32_t best, bool sound) {
  char s[48];
  if (bg) g.pushImage(0, 0, A::W, A::H, (const lgfx::swap565_t*)bg, lgfx::swap565_t(A::KEY));

  // the title bar: the score, and the lives: a pair of ears each, where the art has them, or
  // past two, one pair and how many
  score(g, gm.score, 219, 7);
  auto ears = [&](int x) { g.pushImage(x, 4, A::EARS_W, A::EARS_H, (const lgfx::swap565_t*)A::EARS, lgfx::swap565_t(A::KEY)); };
  if (gm.lives <= 2) { for (int i = 0; i < gm.lives; i++) ears(275 + i * 19); }
  else {
    ears(275);
    for (int r = 0; r < 5; r++) for (int c = 0; c < 5; c++) if ((r == c || r == 4 - c)) g.drawPixel(296 + c, 9 + r, A::C_CYAN);   // x
    digit(g, gm.lives > 9 ? 9 : gm.lives, 303, 7);
  }

  // the wall
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++) {
      if (!gm.hp[r][c]) continue;
      const int k = gm.kind[r][c] == K_STEEL ? (gm.hp[r][c] == 1 ? 4 : 3) : gm.kind[r][c] - 1;
      g.pushImage(Game::brickX(c), Game::brickY(r), A::BRICK_W, A::BRICK_H, (const lgfx::swap565_t*)A::BRICK[k], lgfx::swap565_t(A::KEY));
    }

  // carrots, each with the dotted trail it falls along, and what it gives
  for (const Falling& f : gm.drop) {
    if (!f.kind) continue;
    const int cx = (int)f.x, cy = (int)f.y;
    for (int k = 1; k <= 3; k++) g.fillRect(cx - 1, cy - A::CARROT_H / 2 - k * 4, 2, 2, mix565(A::C_BG, A::C_CARROT_DOT, 1.0f - k * 0.25f));
    g.pushImage(cx - A::CARROT_W / 2, cy - A::CARROT_H / 2, A::CARROT_W, A::CARROT_H, (const lgfx::swap565_t*)A::CARROT, lgfx::swap565_t(A::KEY));
    static const char* L[] = {"", "W", "M", "S", "+"};
    g.setFont(&fonts::Font0); g.setTextColor(C_WHITE);
    g.drawString(L[f.kind < 5 ? f.kind : 0], cx + A::CARROT_W / 2 + 1, cy - 3);
  }

  board(g, gm.px, gm.paddleW());

  // sparks
  for (const auto& q : sp.p) {
    if (q.life <= 0) continue;
    const uint16_t c = mix565(A::C_BG, A::C_MAGENTA, q.life > 0.3f ? 1.0f : q.life / 0.3f);
    const int sz = q.life > 0.25f ? 2 : 1;
    g.fillRect((int)q.x, (int)q.y, sz, sz, c);
  }

  // the packets: a fading teal streak behind, a white core with a soft edge
  for (const Ball& b : gm.ball) {
    if (!b.on) continue;
    if (gm.state == PLAY || gm.state == PAUSED)
      for (int i = TRAIL - 1; i >= 1; i--) {
        const float f = 1.0f - (float)i / TRAIL;
        const int sz = i < TRAIL / 3 ? 2 : 1;
        g.fillRect((int)b.tx[i] - sz / 2, (int)b.ty[i] - sz / 2, sz, sz, mix565(A::C_BG, mix565(A::C_TRAIL, A::C_CYAN, f), 0.4f + 0.6f * f));
      }
    g.fillCircle((int)b.x, (int)b.y, 3, mix565(A::C_BG, C_WHITE, 0.4f));
    g.fillCircle((int)b.x, (int)b.y, 2, C_WHITE);
  }

  // power-ups running: above the board, between the cities
  g.setFont(&fonts::Font0);
  const bool live = gm.state == PLAY || gm.state == PAUSED || gm.state == SERVE;
  if (live && gm.wideS > 0) { snprintf(s, sizeof s, "WIDE %d", (int)ceilf(gm.wideS)); g.setTextColor(A::C_CYAN); g.drawString(s, 64, 186); }
  if (live && gm.slowS > 0) { snprintf(s, sizeof s, "SLOW %d", (int)ceilf(gm.slowS)); g.setTextColor(A::C_MAGENTA); g.drawString(s, 256 - (int)g.textWidth(s), 186); }

  // what's going on, in a framed panel across the middle
  const int py = 128;
  if (gm.state == SERVE) {
    panel(g, 46, py, FW - 92, 50);
    g.setFont(&fonts::Font2); g.setTextColor(A::C_CYAN);
    snprintf(s, sizeof s, "LEVEL %u: %s", (unsigned)(gm.level + 1), gm.lvl().name);
    centre(g, s, py + 8);
    g.setFont(&fonts::Font0); g.setTextColor(C_LAVENDER);
    centre(g, "roll: move  press: launch  p: pause", py + 32);
  } else if (gm.state == PAUSED) {
    panel(g, 46, py, FW - 92, 58);
    g.setFont(&fonts::Font2); g.setTextColor(A::C_MAGENTA);
    centre(g, "PAUSED", py + 8);
    g.setFont(&fonts::Font0); g.setTextColor(C_LAVENDER);
    snprintf(s, sizeof s, "press: go on  s: sound %s", sound ? "on" : "off");
    centre(g, s, py + 32);
    centre(g, "back: quit", py + 41);
  } else if (gm.state == CLEARED) {
    panel(g, 46, py, FW - 92, 50);
    g.setFont(&fonts::Font2); g.setTextColor(A::C_CYAN);
    centre(g, "FIREWALL BREACHED", py + 8);
    g.setFont(&fonts::Font0); g.setTextColor(C_LAVENDER);
    snprintf(s, sizeof s, "+%u   next: %s", 100u * (gm.level + 1), LEVELS[(gm.level + 1) % NLEVELS].name);
    centre(g, s, py + 32);
  } else if (gm.state == OVER) {
    panel(g, 46, py - 6, FW - 92, 62);
    g.setFont(&fonts::Font2); g.setTextColor(A::C_MAGENTA);
    centre(g, "CONNECTION LOST", py + 2);
    g.setFont(&fonts::Font0);
    const bool nb = gm.score && gm.score >= best;
    g.setTextColor(nb ? A::C_CYAN : C_LAVENDER);
    snprintf(s, sizeof s, nb ? "score %lu   NEW BEST" : "score %lu   best %lu", (unsigned long)gm.score, (unsigned long)best);
    centre(g, s, py + 26);
    g.setTextColor(C_LAVENDER);
    centre(g, "press: play again   back: quit", py + 40);
  }
}

}  // namespace bricks
