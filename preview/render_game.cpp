// Renders Burrow Breaker (Deck > GAME) through the real bricks_draw.h and the art in
// bricks_art.{h,cpp}, at moments of a game the real bricks_core.h plays: preview only.
#include "themes.h"        // LovyanGFX's stand-in (display_config.h), as the other previews
#include "bricks_draw.h"
#include <cstdio>
#include <string>
#include <vector>
using namespace bricks;

static void save(lgfx::LovyanGFX& d, const std::string& stem) {
  FILE* f = fopen((stem + ".ppm").c_str(), "wb");
  fprintf(f, "P6 %d %d 255\n", d.W, d.H);
  for (uint16_t c : d.px) { unsigned char rgb[3] = {(unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 63) << 2), (unsigned char)((c & 31) << 3)}; fwrite(rgb, 1, 3, f); }
  fclose(f);
  f = fopen((stem + ".txt").c_str(), "w");
  for (auto& o : textOps()) fprintf(f, "%d\t%d\t%d\t%u\t%s\n", o.x, o.y, o.font, o.color, o.text.c_str());
  fclose(f);
  textOps().clear();
}

// Plays with a simple autopilot for the given seconds, the sparks following.
static void play(Game& g, Sparks& sp, float secs) {
  for (int i = 0; i < (int)(secs / 0.008f); i++) {
    const Ball* t = nullptr;
    for (auto& b : g.ball) if (b.on && (!t || (b.vy > 0 && b.y > t->y))) t = &b;
    if (t) g.setTarget(t->x + 7);
    if (g.state == SERVE) g.press();
    g.step(0.008f);
    sp.add(g);
    sp.step(0.008f);
  }
}

int main() {
  LGFX d;
  std::vector<uint16_t> bg(A::W * A::H);
  unpackBackground(bg.data());
  auto shot = [&](const Game& g, const Sparks& sp, const char* name) {
    draw(d, g, sp, bg.data(), 4210, true);
    save(d, std::string("game_") + name);
  };
  Sparks none;
  // the concept's moment: level 1, a few blocks down, a carrot falling, sparks
  { Game g; g.seed(11); g.newGame(); g.press();
    Sparks sp;
    // the concept's own gap: two blocks out of column 6, the steel above them cracked
    g.hp[2][5] = 0; g.hp[3][5] = 0; g.left -= 2; g.hp[1][5] = 1; g.score = 4210;
    g.burst[0] = {Game::brickX(5) + BW / 2.0f, Game::brickY(2) + BH / 2.0f, K_AMBER}; g.nburst = 1; sp.add(g);
    for (int i = 0; i < 4; i++) sp.step(0.03f);
    g.ball[0].x = 220; g.ball[0].y = 80; g.ball[0].vx = 60; g.ball[0].vy = 140;
    for (int i = 0; i < TRAIL; i++) { g.ball[0].tx[i] = 220 - 3.0f * (i + 1) * 0.4f; g.ball[0].ty[i] = 80 + 3.0f * (i + 1) * 0.9f; }
    g.drop[0] = {112, 110, D_WIDE};
    g.px = g.target = 159.5f;
    shot(g, sp, "1_concept"); }
  { Game g; g.seed(7); g.newGame(); g.setTarget(130); for (int i = 0; i < 60; i++) g.step(0.008f);
    shot(g, none, "2_serve"); }
  // mid-game on level 2: the wide board, three packets, carrots falling, sparks
  { Game p; Sparks sp; p.seed(9); p.level = 1; p.loadLevel(); play(p, sp, 14);
    p.state = PLAY; p.take(D_WIDE); p.take(D_MULTI);
    p.drop[0] = {120, 120, D_SLOW}; p.drop[1] = {230, 100, D_LIFE};
    for (int i = 0; i < 6; i++) { p.step(0.008f); sp.add(p); sp.step(0.008f); }
    shot(p, sp, "3_play"); }
  { Game c; Sparks sp; c.seed(3); c.level = 2; c.loadLevel(); play(c, sp, 6); c.state = PLAY; c.pause();
    shot(c, sp, "4_paused"); }
  { Game m; Sparks sp; m.seed(5); m.level = 4; m.loadLevel(); play(m, sp, 25); m.state = PLAY;
    for (int i = 0; i < 4; i++) { m.step(0.008f); sp.add(m); sp.step(0.008f); }
    shot(m, sp, "5_mainframe"); }
  { Game e; e.seed(4); e.level = 3; e.loadLevel(); e.score = 5120; e.state = CLEARED; e.stateS = 0;
    for (int r = 0; r < ROWS; r++) for (int cc = 0; cc < COLS; cc++) e.hp[r][cc] = 0;
    shot(e, none, "6_cleared"); }
  { Game o; Sparks sp; o.seed(4); o.level = 2; o.loadLevel(); play(o, sp, 9); o.score = 3870; o.state = OVER; o.lives = 0;
    shot(o, none, "7_over"); }
  return 0;
}
