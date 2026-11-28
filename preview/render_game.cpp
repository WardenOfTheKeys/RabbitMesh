// Renders Burrow Breach (Deck > GAME) through the real bricks_draw.h, in RabbitMesh's three
// themes, at moments of a game the real bricks_core.h plays: preview only.
#define RABBITMESH 1     // its own themes (Neon Rabbit, Red Team, Blue Team), as the firmware has them
#include "themes.h"
#include "bricks_draw.h"
#include <cstdio>
#include <string>
using namespace bricks;

static void save(lgfx::LovyanGFX& d, const std::string& stem, const Theme& t, const char* right) {
  // the status bar and header the real UI draws around a view
  d.fillRect(0, 0, deckdraw::W, 18, t.panel);
  d.setFont(&fonts::Font2); d.setTextColor(t.green);
  d.drawString("<", 6, 22); d.setTextColor(t.txt); d.drawString("Burrow Breach", 18, 22);
  d.setTextColor(t.dim); d.drawString(right, deckdraw::W - 8 * (int)strlen(right) - 6, 22);
  d.drawFastHLine(0, deckdraw::BODY_Y - 1, deckdraw::W, t.line);
  d.setFont(&fonts::Font0); d.setTextColor(t.dim); d.drawString("9:41", deckdraw::W - 30, 5); d.drawString("RabbitMesh", 6, 5);
  FILE* f = fopen((stem + ".ppm").c_str(), "wb");
  fprintf(f, "P6 %d %d 255\n", d.W, d.H);
  for (uint16_t c : d.px) { unsigned char rgb[3] = {(unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 63) << 2), (unsigned char)((c & 31) << 3)}; fwrite(rgb, 1, 3, f); }
  fclose(f);
  f = fopen((stem + ".txt").c_str(), "w");
  for (auto& o : textOps()) fprintf(f, "%d\t%d\t%d\t%u\t%s\n", o.x, o.y, o.font, o.color, o.text.c_str());
  fclose(f);
  textOps().clear();
}

// Plays with a simple autopilot for the given seconds.
static void play(Game& g, float secs) {
  for (int i = 0; i < (int)(secs / 0.008f); i++) {
    const Ball* t = nullptr;
    for (auto& b : g.ball) if (b.on && (!t || (b.vy > 0 && b.y > t->y))) t = &b;
    if (t) g.setTarget(t->x + 7);
    if (g.state == SERVE) g.press();
    g.step(0.008f);
  }
}

int main() {
  LGFX d;
  const uint8_t themes[] = {6, 7, 8};
  static_assert(THEME_COUNT == 15, "RabbitMesh's own themes are ids 6, 7, 8");
  const char* tn[] = {"neon", "red", "blue"};
  for (int k = 0; k < 3; k++) {
    Theme t;
    const ThemeSpec& spec = THEMES[themes[k]];
    printf("%s\n", spec.name);
    t.apply(d, spec.palette, spec.style); t.scene = spec.scene;
    auto shot = [&](const Game& g, const char* name, uint32_t best) {
      d.fillRect(0, 0, deckdraw::W, deckdraw::H, t.bg);
      draw(d, t, g, 0, deckdraw::BODY_Y, 1.3f, best, true);
      char r[24]; snprintf(r, sizeof r, "best %lu", (unsigned long)(g.score > best ? g.score : best));
      save(d, std::string("game_") + tn[k] + "_" + name, t, r);
    };
    Game g; g.seed(7); g.newGame(); g.setTarget(130); for (int i = 0; i < 60; i++) g.step(0.008f);
    shot(g, "1_serve", 4210);
    // mid-game: a few blocks gone, a carrot falling, the wide board, three packets
    Game p; p.seed(9); p.level = 1; p.loadLevel(); play(p, 14);
    p.state = PLAY; p.take(D_WIDE); p.take(D_MULTI);
    p.drop[0] = {120, 120, D_SLOW}; p.drop[1] = {230, 90, D_LIFE};
    for (int i = 0; i < 6; i++) p.step(0.008f);
    shot(p, "2_play", 4210);
    Game c; c.seed(3); c.level = 2; c.loadLevel(); play(c, 6); c.state = PLAY; c.pause();
    shot(c, "3_paused", 4210);
    Game m; m.seed(5); m.level = 4; m.loadLevel(); play(m, 25); m.state = PLAY; for (int i = 0; i < 4; i++) m.step(0.008f);
    shot(m, "4_mainframe", 4210);
    Game e; e.seed(4); e.level = 3; e.loadLevel(); e.score = 5120; e.state = CLEARED; e.stateS = 0;
    for (int r = 0; r < ROWS; r++) for (int cc = 0; cc < COLS; cc++) e.hp[r][cc] = 0;
    shot(e, "5_cleared", 4210);
    Game o; o.seed(4); o.level = 2; o.loadLevel(); play(o, 9); o.score = 3870; o.state = OVER; o.lives = 0;
    shot(o, "6_over", 4210);
  }
  return 0;
}
