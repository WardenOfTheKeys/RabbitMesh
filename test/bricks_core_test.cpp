// Host tests for Burrow Breaker (src/bricks_core.h): the rules, and an autopilot that plays.
#include <cstdio>
#include <cmath>
#include <cstring>
#include "bricks_core.h"
using namespace bricks;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static int standing(const Game& g) {
  int n = 0;
  for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) n += g.hp[r][c] > 0;
  return n;
}
// The board under the lowest packet that's coming down (or the first one), hitting it a little
// off centre, by a new amount each time a packet starts down: some angle, varied.
static uint32_t aim = 7;
static int off = 6;
static bool wasDown = false;
static void autopilot(Game& g) {
  const Ball* t = nullptr;
  for (auto& b : g.ball) if (b.on && (!t || (b.vy > 0 && b.y > t->y))) t = &b;
  if (!t) return;
  const bool down = t->vy > 0;
  if (down && !wasDown) { aim = aim * 1103515245u + 12345u; off = (int)((aim >> 16) % 31) - 15; }
  wasDown = down;
  g.setTarget(t->x + off);
}

int main() {
  // every level is ROWS x COLS and has blocks
  for (int i = 0; i < NLEVELS; i++) {
    int n = 0;
    for (int r = 0; r < ROWS; r++) { CHECK(strlen(LEVELS[i].rows[r]) == (size_t)COLS); for (int c = 0; c < COLS; c++) n += LEVELS[i].rows[r][c] != '.'; }
    CHECK(n > 0);
  }
  // the bricks fit between the walls, in order, without overlapping
  CHECK(Game::brickX(0) >= L_WALL && Game::brickX(COLS - 1) + BW <= R_WALL);
  for (int c = 1; c < COLS; c++) CHECK(Game::brickX(c) >= Game::brickX(c - 1) + BW);
  // every letter in a level is one the game knows
  for (int i = 0; i < NLEVELS; i++) for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) CHECK(strchr(".cpaS", LEVELS[i].rows[r][c]));
  CHECK(Game::brickY(ROWS - 1) + BH < PADDLE_Y - 40);

  Game g;
  g.seed(1);
  g.newGame();
  CHECK(g.state == SERVE && g.lives == 3 && g.left == 32 && standing(g) == 32);
  CHECK(g.kind[0][0] == K_CYAN && g.kind[0][3] == K_STEEL && g.hp[0][3] == 2);
  // on the board until launched, and it goes along with it
  g.setTarget(60);
  for (int i = 0; i < 100; i++) g.step(0.01f);
  CHECK(std::fabs(g.px - 60) < 0.01f && std::fabs(g.ball[0].x - 60) < 0.01f);
  // the board stays on the field
  g.nudge(-1000); for (int i = 0; i < 100; i++) g.step(0.01f);
  CHECK(g.px - g.paddleW() / 2 >= L_WALL);
  g.nudge(5000); for (int i = 0; i < 100; i++) g.step(0.01f);
  CHECK(g.px + g.paddleW() / 2 <= R_WALL);
  // launched: up, at the level's speed
  g.press();
  CHECK(g.state == PLAY && g.ball[0].vy < 0);
  CHECK(std::fabs(std::hypot(g.ball[0].vx, g.ball[0].vy) - BASE_SPEED) < 0.5f);

  // missed: a life goes, and it's back on the board
  g.setTarget(0);
  int guard = 0;
  while (g.lives == 3 && guard++ < 20000) g.step(0.008f);
  CHECK(g.lives == 2 && g.state == SERVE);

  // the board's edge sends it off at an angle; its middle, straight up
  {
    Game h; h.newGame(); h.px = h.target = 160; h.press();
    h.ball[0].x = 160 + PADDLE_W / 2.0f; h.ball[0].y = PADDLE_Y - 6; h.ball[0].vx = 0; h.ball[0].vy = 100;
    for (int i = 0; i < 10; i++) h.step(0.008f);
    CHECK(h.ball[0].vy < 0 && h.ball[0].vx > 0.7f * h.speed());
    Game m; m.newGame(); m.px = m.target = 160; m.press();
    m.ball[0].x = 160; m.ball[0].y = PADDLE_Y - 6; m.ball[0].vx = 0; m.ball[0].vy = 100;
    for (int i = 0; i < 10; i++) m.step(0.008f);
    CHECK(m.ball[0].vy < 0 && std::fabs(m.ball[0].vx) < 0.06f * m.speed());   // up to 3 degrees off
  }
  // steel takes two hits
  {
    Game h; h.level = 1; h.loadLevel();
    CHECK(h.hp[0][0] == 2);
    h.press();
    h.ball[0].x = Game::brickX(0) + BW / 2.0f; h.ball[0].y = Game::brickY(0) + BH + BALL_R + 1; h.ball[0].vx = 0; h.ball[0].vy = -150;
    // clear what's between, so the packet reaches the top row
    for (int r = 1; r < ROWS; r++) if (h.hp[r][0]) { h.hp[r][0] = 0; h.left--; }
    h.step(0.02f);
    CHECK(h.hp[0][0] == 1 && (h.events & E_ICE) && h.ball[0].vy > 0 && h.nburst == 0);
    // the second hit breaks it, and it's reported where it was, for the sparks
    h.ball[0].x = Game::brickX(0) + BW / 2.0f; h.ball[0].y = Game::brickY(0) + BH + BALL_R + 1; h.ball[0].vx = 0; h.ball[0].vy = -150;
    h.step(0.02f);
    CHECK(h.hp[0][0] == 0 && (h.events & E_BRICK) && h.nburst == 1 && h.burst[0].kind == K_STEEL);
    CHECK(std::fabs(h.burst[0].x - (Game::brickX(0) + BW / 2.0f)) < 0.01f);
  }
  // power-ups
  {
    Game h; h.newGame(); h.press();
    const float w = h.paddleW();
    h.take(D_WIDE); CHECK(h.paddleW() > w);
    const float s = h.speed();
    h.take(D_SLOW); CHECK(h.speed() < s);
    CHECK(std::fabs(std::hypot(h.ball[0].vx, h.ball[0].vy) - h.speed()) < 0.5f);
    h.take(D_MULTI);
    int on = 0; for (auto& b : h.ball) on += b.on;
    CHECK(on == 3);
    for (auto& b : h.ball) if (b.on) CHECK(b.vy < 0);
    const uint8_t l = h.lives; h.take(D_LIFE); CHECK(h.lives == l + 1);
    for (int i = 0; i < 2000; i++) h.step(0.01f);   // 20 s: both wear off
    CHECK(h.paddleW() == PADDLE_W);
  }

  // The autopilot plays from level 1. The packets stay on the field, never inside a block, and
  // it clears levels (and the patterns come round again past the last).
  {
    Game a; a.seed(12345); a.newGame();
    int cleared = 0, steps = 0;
    bool inside = false, out = false;
    while (cleared < NLEVELS + 1 && a.state != OVER && steps < 2000000) {
      autopilot(a);
      if (a.state == SERVE) a.press();
      a.step(0.008f);
      steps++;
      if (a.events & E_CLEAR) cleared++;
      for (auto& b : a.ball) {
        if (!b.on) continue;
        if (b.x < L_WALL + BALL_R - 0.01f || b.x > R_WALL - BALL_R + 0.01f || b.y < TOP_WALL + BALL_R - 0.01f) out = true;
        for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) {
          if (!a.hp[r][c]) continue;
          const float bx = Game::brickX(c), by = Game::brickY(r);
          if (b.x > bx + BALL_R && b.x < bx + BW - BALL_R && b.y > by + BALL_R && b.y < by + BH - BALL_R) inside = true;
        }
      }
    }
    printf("autopilot: %d levels cleared, score %u, lives %u, %.0f s of play\n", cleared, (unsigned)a.score, (unsigned)a.lives, steps * 0.008);
    CHECK(!out && !inside);
    CHECK(cleared >= NLEVELS + 1);
    CHECK(a.level == NLEVELS && a.speed() > BASE_SPEED);   // the next level loads 1.6 s after the clear
  }
  // pause holds everything still
  {
    Game h; h.newGame(); h.press();
    const float y = h.ball[0].y;
    h.pause(); for (int i = 0; i < 50; i++) h.step(0.01f);
    CHECK(h.state == PAUSED && h.ball[0].y == y);
    h.press(); CHECK(h.state == PLAY);
  }
  // game over, and press starts again
  {
    Game h; h.newGame(); h.lives = 1; h.press(); h.setTarget(0);
    for (int i = 0; i < 20000 && h.state != OVER; i++) h.step(0.008f);
    CHECK(h.state == OVER && (h.events & E_OVER || h.lives == 0));
    h.press(); CHECK(h.state == SERVE && h.lives == 3 && h.score == 0 && h.level == 0);
  }

  if (fails) { printf("%d bricks_core test(s) failed\n", fails); return 1; }
  printf("all bricks_core tests passed\n");
  return 0;
}
