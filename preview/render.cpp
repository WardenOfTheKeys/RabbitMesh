// Renders the RabbitMesh lock scene and rabbit poses through the real headers.
#include "themes.h"
#include "scenes.h"
#include <cstdio>
static void save(const lgfx::LovyanGFX& d, const char* path) {
  FILE* f = fopen(path, "wb");
  fprintf(f, "P6 %d %d 255\n", d.W, d.H);
  for (uint16_t c : d.px) { unsigned char rgb[3] = {(unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 63) << 2), (unsigned char)((c & 31) << 3)}; fwrite(rgb, 1, 3, f); }
  fclose(f);
}
int main() {
  LGFX d;
  Theme t;
  const ThemeSpec& spec = THEMES[THEME_COUNT - 1];
  t.apply(d, spec.palette, spec.style);
  t.scene = spec.scene;
  printf("theme %s scene %d count %d\n", spec.name, t.scene, THEME_COUNT);
  // lock frames across a hop cycle
  const float phases[] = {0.3f, 1.6f, 3.6f, 4.7f};
  for (int k = 0; k < 4; k++) {
    d.fillRect(0, 0, 320, 240, t.bg);
    d.fillRect(0, 0, 320, 18, t.panel);                 // status bar stand-in
    scenes::neonRabbit(d, t, phases[k] + 10, 37.0f * k + 80, k == 2);
    d.fillRect(0, scenes::GROUND + 2, 320, 240 - scenes::GROUND - 2, t.bg);
    char p[32]; snprintf(p, sizeof p, "lock_%d.ppm", k); save(d, p);
  }
  // poses at the size he's drawn (render_lock.cpp shows them on the lock screen too)
  struct { const char* n; SquatchPose p; } poses[6];
  poses[0].n = "idle";
  poses[1].n = "blink"; poses[1].p.blink = true;
  poses[2].n = "happy"; poses[2].p.happy = true;
  poses[3].n = "tired"; poses[3].p.slump = 1;
  poses[4].n = "dizzy"; poses[4].p.dizzy = 0.7f;
  poses[5].n = "on air"; poses[5].p.wave = 1;
  d.fillRect(0, 0, 320, 240, t.bg);
  for (int i = 0; i < 6; i++) {
    d.drawFastHLine(i * 53 + 4, 180, 46, t.line);
    drawRabbit(d, t, i * 53 + 27, 180, rabbitsprite::H, 1.0f + i, t.green, poses[i].p);
  }
  save(d, "poses.ppm");
  return 0;
}
