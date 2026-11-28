// RabbitMesh "Burrow Breach": a brick-breaker in the rabbit's cyberpunk style. The rabbit's
// hoverboard (the paddle) bats a data packet (the ball) into a firewall (the bricks); ICE
// blocks take two hits. Broken blocks sometimes drop a carrot: a wider board, more packets, a
// slower packet, or a life. The game itself, with no screen or keys: bricks_draw.h draws it,
// bricks.cpp runs it on the T-Deck, test/bricks_core_test.cpp plays it on a computer.
#pragma once
#include <stdint.h>
#include <math.h>
#include <string.h>

namespace bricks {

// The field, in pixels: the screen under the header (320 x 198 on the T-Deck).
constexpr int FW = 320, FH = 198;
constexpr int HUD_H = 12;                          // score, level and lives across the top
constexpr int COLS = 10, ROWS = 7;
constexpr int BW = 30, BH = 10, GAP = 1;
constexpr int BX0 = (FW - COLS * (BW + GAP) + GAP) / 2, BY0 = HUD_H + 8;
constexpr int PADDLE_Y = FH - 14, PADDLE_H = 5, PADDLE_W = 46, PADDLE_WIDE = 70;
constexpr float BALL_R = 3.0f;
constexpr int MAX_BALLS = 3, MAX_DROPS = 4, TRAIL = 4;
constexpr float PADDLE_SPEED = 720.0f;             // px/s toward where the trackball put it
constexpr float BASE_SPEED = 150.0f, SPEED_STEP = 12.0f, SPEED_CAP = 270.0f;

// The levels: '.' nothing, '1' a firewall block, '2' an ICE block (two hits). Past the last
// they come round again, faster.
struct Level { const char* name; const char* rows[ROWS]; };
constexpr Level LEVELS[] = {
  {"PORT SCAN", {"..........", "1111111111", "1111111111", "1111111111", "1111111111", "..........", ".........."}},
  {"FIREWALL", {"2222222222", "1.1.1.1.1.", ".1.1.1.1.1", "1111111111", "1.1.1.1.1.", ".1.1.1.1.1", ".........."}},
  {"CARROT", {"....22....", "...2112...", "...1111...", "..111111..", "...1111...", "....11....", ".........."}},
  {"EARS", {".22....22.", ".11....11.", ".11....11.", ".11....11.", "1111111111", "1111111111", "1121111211"}},
  {"MAINFRAME", {"2222222222", "2111111112", "21.2112.12", "2111111112", "21.1221.12", "2111111112", "2222222222"}},
};
constexpr int NLEVELS = sizeof(LEVELS) / sizeof(LEVELS[0]);

enum State : uint8_t { SERVE, PLAY, PAUSED, CLEARED, OVER };
enum Drop : uint8_t { D_NONE, D_WIDE, D_MULTI, D_SLOW, D_LIFE };
// What happened in a step, for sounds.
enum Event : uint16_t { E_BRICK = 1, E_ICE = 2, E_PADDLE = 4, E_WALL = 8, E_LOST = 16, E_PICKUP = 32, E_CLEAR = 64, E_OVER = 128 };

struct Ball { float x, y, vx, vy; bool on; float tx[TRAIL], ty[TRAIL]; };
struct Falling { float x, y; uint8_t kind; };

struct Game {
  uint8_t hp[ROWS][COLS];          // 0 gone, 1 or 2 hits left
  uint8_t ice[ROWS][COLS];         // started as ICE (drawn cracked once hit)
  Ball ball[MAX_BALLS];
  Falling drop[MAX_DROPS];
  float px = FW / 2, target = FW / 2;
  float wideS = 0, slowS = 0;      // seconds of each power-up left
  uint32_t score = 0;
  uint8_t lives = 3, level = 0;    // level counts on past NLEVELS (the patterns come round again)
  uint16_t left = 0;               // blocks still standing
  State state = SERVE;
  float stateS = 0;                // seconds in this state
  uint16_t events = 0;
  uint32_t rng = 0x2545F491u;

  void seed(uint32_t s) { rng = s ? s : 0x2545F491u; }
  uint32_t rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

  const Level& lvl() const { return LEVELS[level % NLEVELS]; }
  float paddleW() const { return wideS > 0 ? (float)PADDLE_WIDE : (float)PADDLE_W; }
  float speed() const {
    float s = BASE_SPEED + SPEED_STEP * level;
    if (s > SPEED_CAP) s = SPEED_CAP;
    return slowS > 0 ? s * 0.7f : s;
  }
  static int brickX(int c) { return BX0 + c * (BW + GAP); }
  static int brickY(int r) { return BY0 + r * (BH + GAP); }

  void newGame() { score = 0; lives = 3; level = 0; loadLevel(); }
  void loadLevel() {
    left = 0;
    for (int r = 0; r < ROWS; r++)
      for (int c = 0; c < COLS; c++) {
        const char k = lvl().rows[r][c];
        hp[r][c] = k == '2' ? 2 : k == '1' ? 1 : 0;
        ice[r][c] = k == '2';
        if (hp[r][c]) left++;
      }
    for (auto& d : drop) d.kind = D_NONE;
    wideS = slowS = 0;
    serve();
  }
  // A packet sits on the board until the trackball is pressed.
  void serve() {
    for (auto& b : ball) b.on = false;
    ball[0].on = true;
    placeOnPaddle(ball[0]);
    state = SERVE; stateS = 0;
  }
  void placeOnPaddle(Ball& b) {
    b.x = px; b.y = PADDLE_Y - BALL_R - 1; b.vx = b.vy = 0;
    for (int i = 0; i < TRAIL; i++) { b.tx[i] = b.x; b.ty[i] = b.y; }
  }

  // The one button: launch, pause off, next game.
  void press() {
    if (state == SERVE) {
      const float a = ((int)(rnd() % 41) - 20) * (float)M_PI / 180.0f;   // up, up to 20 degrees off
      ball[0].vx = sinf(a) * speed(); ball[0].vy = -cosf(a) * speed();
      state = PLAY; stateS = 0;
    } else if (state == PAUSED) { state = PLAY; }
    else if (state == OVER) { newGame(); }
  }
  void pause() { if (state == PLAY) state = PAUSED; else if (state == PAUSED) state = PLAY; }
  void nudge(float dx) { setTarget(target + dx); }
  void setTarget(float x) {
    const float h = paddleW() / 2;
    target = x < h ? h : x > FW - h ? FW - h : x;
  }

  void step(float dt) {
    events = 0;
    stateS += dt;
    if (state == PAUSED || state == OVER) return;
    if (state == CLEARED) {
      if (stateS > 1.6f) { level++; loadLevel(); }
      return;
    }
    // the board glides to where it was sent
    setTarget(target);
    const float d = target - px, mv = PADDLE_SPEED * dt;
    px += fabsf(d) <= mv ? d : (d > 0 ? mv : -mv);
    if (wideS > 0) wideS -= dt;
    if (slowS > 0) { slowS -= dt; if (slowS <= 0) rescale(); }
    if (state == SERVE) { placeOnPaddle(ball[0]); return; }

    for (auto& b : ball) if (b.on) moveBall(b, dt);
    for (auto& f : drop) if (f.kind) moveDrop(f, dt);

    bool any = false;
    for (auto& b : ball) any |= b.on;
    if (!any) {
      events |= E_LOST;
      if (lives) lives--;
      if (!lives) { state = OVER; stateS = 0; events |= E_OVER; }
      else { wideS = slowS = 0; for (auto& f : drop) f.kind = D_NONE; serve(); }
    }
    if (!left && state == PLAY) { state = CLEARED; stateS = 0; events |= E_CLEAR; score += 100u * (level + 1); }
  }

  // Every packet at the speed of the moment, keeping its direction.
  void rescale() {
    for (auto& b : ball) {
      if (!b.on) continue;
      const float v = sqrtf(b.vx * b.vx + b.vy * b.vy);
      if (v > 0) { b.vx *= speed() / v; b.vy *= speed() / v; }
    }
  }

  bool hitBricks(float x, float y) {
    const int c0 = (int)floorf((x - BALL_R - BX0) / (BW + GAP)), c1 = (int)floorf((x + BALL_R - BX0) / (BW + GAP));
    const int r0 = (int)floorf((y - BALL_R - BY0) / (BH + GAP)), r1 = (int)floorf((y + BALL_R - BY0) / (BH + GAP));
    bool hit = false;
    for (int r = r0; r <= r1; r++)
      for (int c = c0; c <= c1; c++) {
        if (r < 0 || r >= ROWS || c < 0 || c >= COLS || !hp[r][c]) continue;
        const float bx = brickX(c), by = brickY(r);
        if (x + BALL_R <= bx || x - BALL_R >= bx + BW || y + BALL_R <= by || y - BALL_R >= by + BH) continue;
        hit = true;
        if (--hp[r][c]) { events |= E_ICE; continue; }
        left--;
        events |= E_BRICK;
        score += ice[r][c] ? 25 : 10;
        if (rnd() % 7 == 0) spawnDrop(bx + BW / 2.0f, by + BH / 2.0f);
      }
    return hit;
  }

  void moveBall(Ball& b, float dt) {
    for (int i = TRAIL - 1; i > 0; i--) { b.tx[i] = b.tx[i - 1]; b.ty[i] = b.ty[i - 1]; }
    b.tx[0] = b.x; b.ty[0] = b.y;
    // in steps of at most 2 px, each axis on its own: a fast packet can't pass through a block
    const float dx = b.vx * dt, dy = b.vy * dt;
    const int n = (int)ceilf(fmaxf(fabsf(dx), fabsf(dy)) / 2.0f) + 1;
    for (int i = 0; i < n && b.on; i++) {
      b.x += dx / n;
      if (b.x < BALL_R) { b.x = BALL_R; b.vx = fabsf(b.vx); events |= E_WALL; }
      if (b.x > FW - BALL_R) { b.x = FW - BALL_R; b.vx = -fabsf(b.vx); events |= E_WALL; }
      if (hitBricks(b.x, b.y)) { b.x -= dx / n; b.vx = -b.vx; break; }
      b.y += dy / n;
      if (b.y < HUD_H + BALL_R) { b.y = HUD_H + BALL_R; b.vy = fabsf(b.vy); events |= E_WALL; }
      if (hitBricks(b.x, b.y)) { b.y -= dy / n; b.vy = -b.vy; break; }
      // the board: where it's hit sets the angle, up to 60 degrees off straight up
      const float h = paddleW() / 2;
      if (b.vy > 0 && b.y + BALL_R >= PADDLE_Y && b.y - BALL_R <= PADDLE_Y + PADDLE_H && b.x >= px - h - BALL_R && b.x <= px + h + BALL_R) {
        float off = (b.x - px) / (h + BALL_R);
        off = off < -1 ? -1 : off > 1 ? 1 : off;
        // plus up to 3 degrees either way: no packet settles into a loop that never reaches the last blocks
        const float a = (off * 60.0f + ((int)(rnd() % 61) - 30) / 10.0f) * (float)M_PI / 180.0f;
        b.vx = sinf(a) * speed(); b.vy = -cosf(a) * speed();
        b.y = PADDLE_Y - BALL_R;
        events |= E_PADDLE;
        break;
      }
      if (b.y - BALL_R > FH) { b.on = false; }
    }
  }

  void spawnDrop(float x, float y) {
    for (auto& f : drop) {
      if (f.kind) continue;
      const uint32_t k = rnd() % 10;
      f.kind = k < 3 ? D_WIDE : k < 6 ? D_MULTI : k < 9 ? D_SLOW : D_LIFE;
      f.x = x; f.y = y;
      return;
    }
  }
  void moveDrop(Falling& f, float dt) {
    f.y += 55.0f * dt;
    const float h = paddleW() / 2;
    if (f.y + 6 >= PADDLE_Y && f.y - 6 <= PADDLE_Y + PADDLE_H && f.x >= px - h - 4 && f.x <= px + h + 4) {
      events |= E_PICKUP;
      take(f.kind);
      f.kind = D_NONE;
    } else if (f.y > FH + 8) f.kind = D_NONE;
  }
  void take(uint8_t k) {
    switch (k) {
      case D_WIDE: wideS = 15; break;
      case D_SLOW: slowS = 10; rescale(); break;
      case D_LIFE: if (lives < 9) lives++; break;
      case D_MULTI: {
        const Ball* from = nullptr;
        for (auto& b : ball) if (b.on) { from = &b; break; }
        if (!from) break;
        const Ball src = *from;
        int made = 0;
        for (auto& b : ball) {
          if (b.on || made == 2) continue;
          b = src;
          const float a = (made ? 35.0f : -35.0f) * (float)M_PI / 180.0f;   // the same packet, split left and right
          const float ca = cosf(a), sa = sinf(a);
          b.vx = src.vx * ca - src.vy * sa; b.vy = src.vx * sa + src.vy * ca;
          if (b.vy > -40.0f) b.vy = -40.0f;                                   // never one flat across
          b.on = true; made++;
        }
        rescale();
        break;
      }
    }
  }
};

}  // namespace bricks
